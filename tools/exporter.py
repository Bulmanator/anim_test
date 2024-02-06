bl_info = {
    "name":        "AMTS/AMTM Exporter",
    "author":      "James Bulman",
    "version":     (2024, 1, 22),
    "blender":     (4, 0, 0),
    "location":    "Properties > Object > AMT Export",
    "description": "AMT animation and mesh exporter.",
    "category":    "Export"
}

# Imports

import bpy
import bmesh
import struct
import mathutils

from bpy.utils import (register_class, unregister_class)
from bpy.types import (Panel, PropertyGroup)
from bpy.props import (StringProperty, EnumProperty)

from bpy_extras.io_utils import (axis_conversion)

# Constants

AMTS_MAGIC   = 0x53544D41 # 'AMTS'
AMTS_VERSION = 1

AMTM_MAGIC   = 0x4D544D41 # 'AMTM'
AMTM_VERSION = 1

R_MESH_FLAG_HAS_SKELETON = 0x1

R_TEXTURE_TYPE_BASE_COLOUR = 1
R_TEXTURE_TYPE_ROUGHNESS   = 2
R_TEXTURE_TYPE_METALLIC    = 3
R_TEXTURE_TYPE_NORMAL      = 4

AXES = [
    ("X", "X", "", 1), ("-X", "-X", "", 2),
    ("Y", "Y", "", 3), ("-Y", "-Y", "", 4),
    ("Z", "Z", "", 5), ("-Z", "-Z", "", 6)
]

MATERIAL_PROPERTIES = [
        "Base Color", "Metallic", "Roughness", "IOR", "Anisotropic", "Anisotropic Rotation",
        "Coat Weight", "Coat Roughness", "Sheen Weight", "Sheen Roughness"
]

# Skeleton storage classes

class A_Bone:
    def __init__(self, bind_pose, inv_bind_pose, parent_index, name_count, name_offset):
        self.bind_pose     = bind_pose
        self.inv_bind_pose = inv_bind_pose
        self.parent_index  = parent_index
        self.name_count    = name_count
        self.name_offset   = name_offset

    def Write(self, file):
        # Write bind pose sample
        for p in self.bind_pose.to_translation(): F32Write(file, p)
        for o in self.bind_pose.to_quaternion():  F32Write(file, o)
        for s in self.bind_pose.to_scale():       F32Write(file, s)

        # Write inverse bind pose sample
        for p in self.inv_bind_pose.to_translation(): F32Write(file, p)
        for o in self.inv_bind_pose.to_quaternion():  F32Write(file, o)
        for s in self.inv_bind_pose.to_scale():       F32Write(file, s)

        # Write parent index
        U8Write(file, self.parent_index)

        # Write name information
        U8Write(file,  self.name_count)
        U16Write(file, self.name_offset)

class A_Track:
    def __init__(self, flags, name_count, name_offset, num_frames, samples):
        self.flags       = flags
        self.name_count  = name_count
        self.name_offset = name_offset
        self.num_frames  = num_frames
        self.samples     = samples

    def WriteInfo(self, file):
        U8Write(file,  self.flags)
        U8Write(file,  self.name_count)
        U16Write(file, self.name_offset)
        U32Write(file, self.num_frames)

    def WriteSamples(self, file):
        for sample in self.samples:
            for p in sample.to_translation(): F32Write(file, p)
            for o in sample.to_quaternion():  F32Write(file, o)
            for s in sample.to_scale():       F32Write(file, s)

# Mesh storage classes

class R_Vertex:
    def __init__(self, position, uv, normal, material_index, joint_indices, joint_weights):
        self.position       = position.freeze()
        self.uv             = uv.freeze()
        self.normal         = normal.freeze()
        self.material_index = material_index
        self.joint_indices  = joint_indices
        self.joint_weights  = joint_weights

    def __eq__(self, other):
        if type(other) is type(self):
            return self.__dict__ == other.__dict__

        return False

    def __hash__(self):
        return hash((self.position, self.uv, self.normal, self.material_index, \
                sum(self.joint_indices), sum(self.joint_weights)))

class R_Material:
    def __init__(self, name, index, material):
        self.name       = name
        self.index      = index

        # filled out later
        self.name_offset = 0
        self.name_count  = 0
        # Material properties map from a name in MATERIAL_PROPERTIES to a tuple which contains three values,
        # (default_value, image, channel_mask), if there is not an image associated with the property then
        # image and channel mask will both be 0
        #
        self.properties = {}

        bsdf = None

        if material.use_nodes:
            for n in material.node_tree.nodes:
                if n.type == "BSDF_PRINCIPLED":
                    bsdf = n
                    break

        if not bsdf:
            # Non-node based/unsupported material, we can only pull basic things from this
            self.properties["Base Color"] = (material.diffuse_color, 0, 0)
            self.properties["Metallic"]   = (material.metallic, 0, 0)
            self.properties["Roughness"]  = (material.roughness, 0, 0)

            # We can't get any of these values so just use blender defaults
            self.properties["IOR"] = (1.45, 0, 0)

            self.properties["Anisotropic"]          = (0, 0, 0)
            self.properties["Anisotropic Rotation"] = (0, 0, 0)

            self.properties["Coat Weight"]    = (0, 0, 0)
            self.properties["Coat Roughness"] = (0.03, 0, 0)

            self.properties["Sheen Weight"]    = (0, 0, 0)
            self.properties["Sheen Roughness"] = (0.5, 0, 0)
        else:
            for name in MATERIAL_PROPERTIES:
                node = bsdf.inputs[name]
                if node.is_linked:
                    link = node.links[0]
                    if link.from_node.type == "TEX_IMAGE":
                        # Direct from texture with all channels present in the image
                        image = link.from_node.image
                        self.properties[name] = (node.default_value, image, (1 << image.channels) - 1)
                    elif link.from_node.type == "SEPARATE_COLOR":
                        # Separate out specific channels
                        #
                        # Calculate channel mask we for the channel that is being separated out
                        #
                        channel_index = 0x0
                        for output in link.from_node.outputs:
                            if not output.is_linked: continue

                            if output.links[0].to_socket.name == name:
                                # This is linked to the input on the BSDF node
                                if   output.name == "Red":   channel_index = 0x0
                                elif output.name == "Green": channel_index = 0x1
                                elif output.name == "Blue":  channel_index = 0x2

                                break

                        input_node = link.from_node.inputs["Color"]
                        if input_node.is_linked:
                            colour_node = input_node.links[0].from_node
                            if colour_node.type == "TEX_IMAGE":
                                self.properties[name] = (input_node.default_value[channel_index], colour_node.image, 1 << channel_index)
                            elif colour_node.type == "RGB":
                                self.properties[name] = (colour_node.outputs["Color"].default_value[channel_index], 0, 0)
                            elif colour_node.type == "VALUE":
                                self.properties[name] = (colour_node.outputs["Value"].default_value, 0, 0)
                            else:
                                print("[WRN] :: Unsupported link type " + colour_node.type)
                                self.properties[name] = (input_node.default_value[channel_index], 0, 0)
                        else:
                            self.properties[name] = (input_node.default_value[channel_index], 0, 0)
                    elif link.from_node.type == "RGB":
                        # Static RGB value
                        self.properties[name] = (link.from_node.outputs["Color"].default_value, 0, 0)
                    elif link.from_node.type == "VALUE":
                        # Single greyscale value across all channels
                        self.properties[name] = (link.from_node.outputs["Value"].default_value, 0, 0)
                    else:
                        # unsupported type
                        print("[WRN] :: Unsupported link type " + link.from_node.type)
                        self.properties[name] = (node.default_value, 0, 0)
                else:
                    self.properties[name] = (node.default_value, 0, 0)

            # @todo: need to get the normal map, it is a texture only thing
            #
            # bsdf.inputs["Normal"]

# File output functions

def U8Write(file, value):
    file.write(struct.pack("<B", value))

def U16Write(file, value):
    file.write(struct.pack("<H", value))

def U32Write(file, value):
    file.write(struct.pack("<I", value))

def F32Write(file, value):
    file.write(struct.pack("<f", value))

# Other utilities

def ArmatureListGet():
    result = []
    for o in bpy.data.objects:
        if o.type == "ARMATURE":
            result.append(o)

    return result

def ArmaturePoseSet(armature, pose):
    result = armature.data.pose_position

    armature.data.pose_position = pose
    armature.data.update_tag()
    bpy.context.scene.frame_set(bpy.context.scene.frame_current)

    return result

def MeshListGet():
    result = []
    for o in bpy.data.objects:
        if o.type == "MESH":
            result.append(o)

    return result

def MeshAttachedArmatureGet(mesh):
    for m in mesh.modifiers:
        if m.type == "ARMATURE":
            return m.object

    return 0

def MeshTriangulate(mesh):
    bm = bmesh.new()

    bm.from_mesh(mesh)
    bmesh.ops.triangulate(bm, faces = bm.faces)
    bm.to_mesh(mesh)

    bm.free()

def R_MeshJointWeightsNormalise(weights):
    result = [0, 0, 0, 0]

    total = sum(weights)
    if total != 0:
        for i, w in enumerate(weights):
            result[i] = w / total

    return result

def R_MaterialsWrite(file_handle, materials):
    for material in materials.values():
        colour = material.properties["Base Color"][0] # default_value
        F32Write(file_handle, colour[0])
        F32Write(file_handle, colour[1])
        F32Write(file_handle, colour[2])
        F32Write(file_handle, colour[3])

        F32Write(file_handle, material.properties["Roughness"][0])
        F32Write(file_handle, material.properties["Metallic"][0])
        F32Write(file_handle, material.properties["IOR"][0])

        F32Write(file_handle, material.properties["Anisotropic"][0])
        F32Write(file_handle, material.properties["Anisotropic Rotation"][0])

        F32Write(file_handle, material.properties["Coat Weight"][0])
        F32Write(file_handle, material.properties["Coat Roughness"][0])

        F32Write(file_handle, material.properties["Sheen Weight"][0])
        F32Write(file_handle, material.properties["Sheen Roughness"][0])

        U16Write(file_handle, material.name_count)
        U16Write(file_handle, material.name_offset)

def R_MeshVerticesWrite(file_handle, vertices, has_skeleton):
    for v in vertices:
        F32Write(file_handle, v.position.x)
        F32Write(file_handle, v.position.y)
        F32Write(file_handle, v.position.z)

        F32Write(file_handle, v.uv.x)
        F32Write(file_handle, v.uv.y)

        F32Write(file_handle, v.normal.x)
        F32Write(file_handle, v.normal.y)
        F32Write(file_handle, v.normal.z)

        U32Write(file_handle, v.material_index)

        if has_skeleton:
            weights = R_MeshJointWeightsNormalise(v.joint_weights)

            for i in v.joint_indices:  U8Write(file_handle, i)
            for w in weights:         F32Write(file_handle, w)


def DumpMaterials(materials):
    for k, v in materials.items():
        print("[" + str(v.index) + "]: " + k + ":")
        for name, prop in v.properties.items():
            print("   [" + name + "]: " + str(prop))

def AxisMappingMatrixGet():
    return axis_conversion("-Y", "Z", bpy.context.scene.export_properties.forward_axis, bpy.context.scene.export_properties.up_axis).to_4x4()

# Skeleton gather and export functions

def A_BonesGet(bones, string_table, armature, axis_mapping_matrix):
    string_table_offset = 0 # We know the incoming string table list is empty when this is called
    for b in armature.data.bones:
        parent_index = armature.data.bones.find(b.parent.name) if b.parent else 0xFF

        bind_pose_matrix     = axis_mapping_matrix @ b.matrix_local
        inv_bind_pose_matrix = bind_pose_matrix.inverted()

        name = b.name.encode('utf-8')

        name_count  = len(name)
        name_offset = string_table_offset

        string_table_offset += name_count
        string_table.append(name)

        bone = A_Bone(bind_pose_matrix, inv_bind_pose_matrix, parent_index, name_count, name_offset)
        bones.append(bone)

def A_TracksGet(tracks, string_table, armature, axis_mapping_matrix):
    # Count how much data is already in the string table from the bone names and use it as the starting offset
    string_table_offset = sum(map(len, string_table))
    for action in bpy.data.actions:
        start_frame = int(action.frame_range.x)
        end_frame   = int(action.frame_range.y)

        name = action.name.encode('utf-8')

        name_count  = len(name)
        name_offset = string_table_offset

        string_table_offset += name_count
        string_table.append(name)

        # Apply this action to the armature
        armature.animation_data.action = action

        samples = []

        for f in range(start_frame, end_frame + 1):
            bpy.context.scene.frame_set(f)

            for b in armature.pose.bones:
                # Put bone pose into parent space
                pose_matrix = b.matrix

                if b.parent:
                    pose_matrix = b.parent.matrix.inverted() @ b.matrix
                else:
                    pose_matrix = axis_mapping_matrix @ b.matrix

                samples.append(pose_matrix)

        # @incomplete: no flags yet
        track = A_Track(0, name_count, name_offset, (end_frame - start_frame) + 1, samples)
        tracks.append(track)

def A_SkeletonExport():
    armatures = ArmatureListGet()
    if len(armatures) == 0:
        return { 'CANCELLED' }

    axis_mapping_matrix = AxisMappingMatrixGet()

    # @incomplete: This only takes the first armature, we should probably export
    # to multiple files if theres more that one armature in the project
    armature = armatures[0]

    # So we can restore the original state after exporting
    base_pose   = ArmaturePoseSet(armature, "POSE")
    base_action = armature.animation_data.action
    base_frame  = bpy.context.scene.frame_current

    bones  = []
    tracks = []

    string_table = []

    A_BonesGet(bones, string_table, armature, axis_mapping_matrix)
    A_TracksGet(tracks, string_table, armature, axis_mapping_matrix)

    # Pull the filename from the open .blend project so we can export under the same name
    filename    = bpy.path.basename(bpy.data.filepath).split('.')[0] + ".amts"
    output_path = bpy.context.scene.export_properties.output_dir + filename
    file_handle = open(bpy.path.abspath(output_path), "wb")

    # Write header
    U32Write(file_handle, AMTS_MAGIC)
    U32Write(file_handle, AMTS_VERSION)

    num_bones  = len(armature.data.bones)
    num_tracks = len(bpy.data.actions)

    total_samples = 0
    for t in tracks:
        total_samples += t.num_frames

    total_samples *= num_bones

    U32Write(file_handle, num_bones)
    U32Write(file_handle, num_tracks)
    U32Write(file_handle, total_samples)

    U32Write(file_handle, bpy.context.scene.render.fps)

    U32Write(file_handle, sum(map(len, string_table)))

    # Pad the header to 64 bytes
    for i in range(0, 9): U32Write(file_handle, 0)

    # Write string table
    for s in string_table:
        file_handle.write(s)

    # Write bone information
    for b in bones:
        b.Write(file_handle)

    # Write animation track info
    for t in tracks:
        t.WriteInfo(file_handle)

    # Write animation track samples
    for t in tracks:
        t.WriteSamples(file_handle)

    bpy.context.scene.frame_set(base_frame)
    armature.animation_data.action = base_action

    ArmaturePoseSet(armature, base_pose)

    file_handle.close()

    return { 'FINISHED' }

# Mesh gather and export functions

def R_MeshExport():
    meshes = MeshListGet()
    if len(meshes) == 0:
        return { 'CANCELLED' }

    axis_mapping_matrix = AxisMappingMatrixGet()
    modified_scene      = bpy.context.evaluated_depsgraph_get()

    total_vertices = 0
    total_indices  = 0
    total_meshes   = len(meshes)

    materials = {}
    vertices  = {}
    indices   = []

    flags = 0x0

    # This expects each mesh that is animated to only have a single armature
    # attached, this may or may not be the common case. if it is not the common
    # case we can always change it in the future
    for base_object in meshes:
        base_pose = "REST"
        armature  = MeshAttachedArmatureGet(base_object)
        if armature:
            base_pose = ArmaturePoseSet(armature, "REST")
            flags    |= R_MESH_FLAG_HAS_SKELETON

        # @todo: do we even need to call evaluated_depsgraph_get() again?
        mesh = base_object.evaluated_get(modified_scene).to_mesh(preserve_all_data_layers = True, depsgraph = bpy.context.evaluated_depsgraph_get())
        mesh.transform(axis_mapping_matrix @ base_object.matrix_world)

        MeshTriangulate(mesh)

        mesh.calc_normals_split()

        for p in mesh.polygons:
            # Safety check, this shouldn't happen becasue we triangulated the mesh before
            if len(p.loop_indices) != 3:
                # @todo: should this be a 'break' to continue and just ignore this specific mesh?
                return { 'CANCELLED' }

            # Material names are unique so we can store them in a dictionary becasue the
            # material slots for polygons are local to the mesh
            material_index = 0
            material_name  = base_object.material_slots[p.material_index].name
            if not material_name in materials:
                material_index = len(materials)
                material       = base_object.material_slots[p.material_index].material

                materials[material_name] = R_Material(material_name, material_index, material)
            else:
                material_index = materials[material_name].index

            for it in p.loop_indices:
                loop = mesh.loops[it]

                position = mesh.vertices[loop.vertex_index].undeformed_co.copy()
                normal   = loop.normal.copy()
                uv = mesh.uv_layers.active.data[loop.index].uv.copy()

                if bpy.context.scene.export_properties.flip_uv_y:
                    uv.y = 1 - uv.y

                joint_indices = [0, 0, 0, 0]
                joint_weights = [0, 0, 0, 0]

                if armature:
                    for it, group in enumerate(mesh.vertices[loop.vertex_index].groups):
                        if it < 4:
                            group_index = group.group
                            bone_name   = base_object.vertex_groups[group_index].name

                            joint_indices[it] = armature.data.bones.find(bone_name)
                            joint_weights[it] = group.weight

                # :note joint weights are normalised at a later time
                vertex = R_Vertex(position, uv, normal, material_index, joint_indices, joint_weights)
                if not vertex in vertices:
                   vertices[vertex] = len(vertices)

                indices.append(vertices[vertex])

        if armature: ArmaturePoseSet(armature, base_pose)

    DumpMaterials(materials)
    print("Vertex count: " + str(len(vertices)))
    print("Index  count: " + str(len(indices)) + " Num Triangles: " + str(len(indices) / 3.0))

    filename    = bpy.path.basename(bpy.data.filepath).split('.')[0] + ".amtm"
    output_path = bpy.context.scene.export_properties.output_dir + filename
    file_handle = open(bpy.path.abspath(output_path), "wb")

    U32Write(file_handle, AMTM_MAGIC)
    U32Write(file_handle, AMTM_VERSION)

    U32Write(file_handle, flags)

    U32Write(file_handle, 1) # num_meshes @incomplete: this needs to split up large meshes
    U32Write(file_handle, len(vertices))
    U32Write(file_handle, len(indices))

    U32Write(file_handle, len(materials))
    U32Write(file_handle, 0) # @incomplete: no textures being written out

    # Gather string table
    # @incomplete: once we start writing textures we will have to look in the material and get all of
    # the texture names to write out as well, probably need somewhere to store the offset/length info as well
    string_table = []
    string_table_offset = 0

    for name, material in materials.items():
        encoded = name.encode('utf-8')
        string_table.append(encoded)

        material.name_offset = string_table_offset
        material.name_count  = len(encoded)

        string_table_offset += material.name_count

    # string_table_offset will hold the full size of the string table at the end
    U32Write(file_handle, string_table_offset)

    for it in range(0, 7): U32Write(file_handle, 0)

    # Write string table
    for s in string_table: file_handle.write(s)

    # Write default mesh, when we split up large meshes this will change
    U32Write(file_handle, 0) # base vertex
    U32Write(file_handle, 0) # base index
    U32Write(file_handle, len(indices)) # num indices

    R_MaterialsWrite(file_handle, materials)
    R_MeshVerticesWrite(file_handle, vertices.keys(), (flags & R_MESH_FLAG_HAS_SKELETON) != 0)

    for i in indices:
        U16Write(file_handle, i)

    return { 'FINISHED' }

# Blender specific export classes

class ExportProperties(bpy.types.PropertyGroup):
    output_dir:   bpy.props.StringProperty(name = "Output Directory", subtype = 'DIR_PATH')
    forward_axis: bpy.props.EnumProperty(name = "Forward Axis", items = AXES, default = "-Y")
    up_axis:      bpy.props.EnumProperty(name = "Up Axis",      items = AXES, default =  "Z")
    flip_uv_y:    bpy.props.BoolProperty(name = "Flip UV", default = True)

class AmtExporter(bpy.types.Operator):
    bl_idname = "object.amt_exporter"
    bl_label  = "Export"

    def execute(self, context):
        return R_MeshExport()


class ExportPanel(bpy.types.Panel):
    bl_label       = "AMT Exporter"
    bl_idname      = "OBJECT_PT_layout"
    bl_space_type  = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context     = "object"

    def draw(self, context):
        self.layout.prop(context.scene.export_properties, "output_dir")
        self.layout.prop(context.scene.export_properties, "forward_axis")
        self.layout.prop(context.scene.export_properties, "up_axis")
        self.layout.prop(context.scene.export_properties, "flip_uv_y")
        self.layout.operator("object.amt_exporter")

# Init and shutdown

def register():
    bpy.utils.register_class(ExportProperties)
    bpy.utils.register_class(AmtExporter)
    bpy.utils.register_class(ExportPanel)
    bpy.types.Scene.export_properties = bpy.props.PointerProperty(type = ExportProperties)

def unregister():
    bpy.utils.unregister_class(ExportProperties)
    bpy.utils.unregister_class(AmtExporter)
    bpy.utils.unregister_class(ExportPanel)
    del bpy.types.Scene.export_properties

if __name__ == "__main__":
    register()
