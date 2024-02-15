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

import os
import shutil

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

R_MESH_FLAG_IS_SKINNED = 0x1

AXES = [
    ("X", "X", "", 1), ("-X", "-X", "", 2),
    ("Y", "Y", "", 3), ("-Y", "-Y", "", 4),
    ("Z", "Z", "", 5), ("-Z", "-Z", "", 6)
]

TEXTURE_NAMES = { "Base Color" : 0, "Normal" : 1, "Metallic" : 2, "Roughness" : 3, "Occlusion" : 4, "Displacement" : 5 }

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
    def __init__(self, position, uv, normal, material_index, bone_info):
        self.position       = position.freeze()
        self.uv             = uv.freeze()
        self.normal         = normal.freeze()
        self.material_index = material_index
        self.bone_info      = bone_info

    def __eq__(self, other):
        if type(other) is type(self):
            return self.__dict__ == other.__dict__

        return False

    def __hash__(self):
        # @todo: re-incorporate bone_info into this hash
        #
        return hash((self.position, self.uv, self.normal, self.material_index))

class R_Texture:
    def __init__(self, path, index, num_channels):
        self.path         = path
        self.name         = bpy.path.basename(path).split('.')[0]

        self.name_offset  = 0
        self.name_count   = 0

        self.index        = index
        self.flags        = 0 # flags are reserved for now
        self.num_channels = num_channels

class R_Material:
    def __init__(self, name, index, material):
        self.name        = name
        self.name_offset = 0 # filled out later
        self.name_count  = 0

        self.index    = index
        self.material = material

        # Materials have a list of properties and a list of texture indices, we pull the local list of
        # textures and then repatch the indices later
        self.colour     = (1, 1, 1, 1)
        self.properties = [0, 0.5, 1.45, 0, 0, 0, 0.05, 0, 0.5, 0]

        self.textures = [-1, -1, -1, -1, -1, -1, -1, -1]

        if material.use_nodes and "Principled BSDF" in material.node_tree.nodes:
            bsdf = material.node_tree.nodes["Principled BSDF"]

            # Save the RGBA default colour, may be have a texture override
            self.colour = bsdf.inputs["Base Color"].default_value[:]

            self.properties[0] = bsdf.inputs["Metallic"            ].default_value
            self.properties[1] = bsdf.inputs["Roughness"           ].default_value
            self.properties[2] = bsdf.inputs["IOR"                 ].default_value
            self.properties[3] = bsdf.inputs["Anisotropic"         ].default_value
            self.properties[4] = bsdf.inputs["Anisotropic Rotation"].default_value
            self.properties[5] = bsdf.inputs["Coat Weight"         ].default_value
            self.properties[6] = bsdf.inputs["Coat Roughness"      ].default_value
            self.properties[7] = bsdf.inputs["Sheen Weight"        ].default_value
            self.properties[8] = bsdf.inputs["Sheen Roughness"     ].default_value

class R_Mesh:
    def __init__(self, name, armature, vertices, indices):
        self.name        = name
        self.name_offset = 0
        self.name_count  = 0

        self.vertices = vertices
        self.indices  = indices
        self.flags    = R_MESH_FLAG_IS_SKINNED if armature else 0


# File output functions

def U8Write(file, value):
    file.write(struct.pack("<B", value))

def U16Write(file, value):
    file.write(struct.pack("<H", value))

def U32Write(file, value):
    file.write(struct.pack("<I", value))

def S32Write(file, value):
    file.write(struct.pack("<i", value))

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

def R_MeshBoneWeightsNormalise(weights):
    result = [0, 0, 0, 0]

    total = sum(weights)
    if total != 0:
        for i, w in enumerate(weights):
            result[i] = w / total

    return result

def R_MeshBoneIndicesGet(bone_info):
    result = [0, 0, 0, 0]
    for i in range(0, min(len(bone_info), 4)):
        result[i] = bone_info[i][1]

    return result

def R_MeshBoneWeightsGet(bone_info):
    result = [0, 0, 0, 0]
    for i in range(0, min(len(bone_info), 4)):
        result[i] = bone_info[i][0]

    result = R_MeshBoneWeightsNormalise(result)
    return result

def R_MaterialsWrite(file, materials):
    for material in materials.values():
        U16Write(file, material.name_offset)
        U8Write(file, material.name_count)
        U8Write(file, 0) # reserved flags

        # @todo: I don't think this is in the right order, I think its in ABGR order
        U8Write(file, int(material.colour[0] * 255))
        U8Write(file, int(material.colour[1] * 255))
        U8Write(file, int(material.colour[2] * 255))
        U8Write(file, int(material.colour[3] * 255))

        for p in material.properties: F32Write(file, p)
        for t in material.textures:   S32Write(file, t)

def R_TextureInfoWrite(file, textures):
    for t in textures.values():
        U16Write(file, t.name_offset)
        U16Write(file, t.name_count)
        U16Write(file, t.flags)
        U16Write(file, t.num_channels)

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
            # these will both be four elements long, weights will be normalised
            indices = R_MeshBoneIndicesGet(v.bone_info)
            weights = R_MeshBoneWeightsGet(v.bone_info)

            for i in indices: U8Write(file_handle, i)
            for w in weights: F32Write(file_handle, w)

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

def A_SkeletonExport(output_dir):
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
    filename = bpy.path.basename(bpy.data.filepath).split('.')[0]
    if not filename: filename = "[unnamed]"

    file_handle = open(bpy.path.abspath(output_dir + os.sep + filename + ".amts"), "wb")

    ### Header begin

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

    ### Header end

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

def R_LinkedTextureNodesFind(linked_nodes, node, channel_mask):
    for it, output in enumerate(node.outputs):
        if not output.is_linked: continue

        new_mask = (1 << it) if node.type == "SEPARATE_COLOR" else channel_mask

        # @todo: this assumes one link per node, should probably iterate over the links if there's more
        # than one!
        #
        link = output.links[0]
        if link.to_node.type == "BSDF_PRINCIPLED":
            # We have found a link to the actual material output so we append a tuple containing the
            # name of the socket it is connected to and the channel mask for the texture we started at
            # as it may have been modified in between by SEPARATE_COLOR nodes
            #
            linked_nodes.append((link.to_socket.name, new_mask))
        else:
            R_LinkedTextureNodesFind(linked_nodes, link.to_node, new_mask)

def R_MeshExport(output_dir):
    mesh_list = MeshListGet()
    if len(mesh_list) == 0:
        return { 'CANCELLED' }

    axis_mapping_matrix = AxisMappingMatrixGet()
    modified_scene      = bpy.context.evaluated_depsgraph_get()

    # Collect all material information, it would be nice if we could just do bpy.data.materials but
    # that is inconsistent with the number of materials that are actually used... because blender I guess
    #
    # For example, a test model I have uses a single material slot, there is only one visible in the UI,
    # yet bpy.data.materials has 59 materials in it??
    #
    materials = {}
    textures  = {}
    for o in mesh_list:
        for slot in o.material_slots:
            if not slot.name in materials:
                index = len(materials)
                materials[slot.name] = R_Material(slot.name, index, slot.material)

    # Look through all of the materials and pull out textures linked to them
    for name, m in materials.items():
        if not m.material.use_nodes: continue

        for node in m.material.node_tree.nodes:
            if node.type == "TEX_IMAGE":
                linked_nodes = []
                R_LinkedTextureNodesFind(linked_nodes, node, 0xF)
                if len(linked_nodes) > 0:
                    full_path    = bpy.path.abspath(node.image.filepath)
                    texture_name = bpy.path.basename(full_path).split('.')[0]

                    index = 0
                    if not texture_name in textures:
                        index = len(textures)
                        textures[texture_name] = R_Texture(full_path, index, node.image.channels)
                    else:
                        index = textures[texture_name].index

                    # @todo: maybe we shouldn't add the texture to the dict until we confirm
                    # its actually a supported link
                    for link in linked_nodes:
                        if not link[0] in TEXTURE_NAMES: continue

                        type_index = TEXTURE_NAMES[link[0]]
                        value = (link[1] << 24) | index

                        m.textures[type_index] = value

    # This expects each mesh that is animated to only have a single armature
    # attached, this may or may not be the common case. if it is not the common
    # case we can always change it in the future
    meshes = []
    for o in mesh_list:
        vertices  = {}
        indices   = []

        base_pose = "REST"
        armature  = MeshAttachedArmatureGet(o)
        if armature: base_pose = ArmaturePoseSet(armature, "REST")

        # @todo: do we even need to call evaluated_depsgraph_get() again?
        mesh = o.evaluated_get(modified_scene).to_mesh(preserve_all_data_layers = True, depsgraph = bpy.context.evaluated_depsgraph_get())
        mesh.transform(axis_mapping_matrix @ o.matrix_world)

        MeshTriangulate(mesh)

        mesh.calc_normals_split()

        for p in mesh.polygons:
            # Safety check, this shouldn't happen becasue we triangulated the mesh before
            if len(p.loop_indices) != 3:
                # @todo: should this be a 'break' to continue and just ignore this specific mesh?
                return { 'CANCELLED' }

            # Material names are unique so we can store them in a dictionary becasue the
            # material slots for polygons are local to the mesh
            material_name  = o.material_slots[p.material_index].name
            material_index = materials[material_name].index

            for it in p.loop_indices:
                loop = mesh.loops[it]

                position = mesh.vertices[loop.vertex_index].undeformed_co.copy()
                normal   = loop.normal.copy()
                uv       = mesh.uv_layers.active.data[loop.index].uv.copy()

                if bpy.context.scene.export_properties.flip_uv:
                    uv.y = 1 - uv.y

                # There may be more than four weights/bone indices in here, they are however sorted in
                # descending order and only the four highest weight values are written out to the file
                #
                bone_data = []

                # This checks that the vertex group which the vertex is apart of actually exists within
                # the armature. This may not always be the case due to blender deciding that it would be
                # a good idea to use vertex groups for armature deform modification as well as use defined
                # selection groups
                #
                if armature:
                    bone_count = 0
                    for group in mesh.vertices[loop.vertex_index].groups:
                        group_index = group.group
                        bone_name   = o.vertex_groups[group_index].name
                        bone_index  = armature.data.bones.find(bone_name)

                        if bone_index >= 0:
                            bone_data.append((group.weight, bone_index))

                # Sort the bone weights in descending order
                bone_data.sort(reverse = True)

                # :note bone weights are normalised at a later time
                vertex = R_Vertex(position, uv, normal, material_index, bone_data)
                if not vertex in vertices:
                   vertices[vertex] = len(vertices)

                indices.append(vertices[vertex])

        # Store the mesh information for later
        r_mesh = R_Mesh(o.name, armature, vertices.keys(), indices)
        meshes.append(r_mesh)

        if armature: ArmaturePoseSet(armature, base_pose)

    filename = bpy.path.basename(bpy.data.filepath).split('.')[0]
    if not filename: filename = "[unnamed]"

    file_handle = open(bpy.path.abspath(output_dir + os.sep + filename + ".amtm"), "wb")

    ### Header begin

    U32Write(file_handle, AMTM_MAGIC)
    U32Write(file_handle, AMTM_VERSION)

    U32Write(file_handle, len(meshes))
    U32Write(file_handle, len(materials))
    U32Write(file_handle, len(textures))

    # Gather string table
    #
    # @incomplete: once we start writing textures we will have to look in the material and get all of
    # the texture names to write out as well, probably need somewhere to store the offset/length info as well
    #
    string_table = []
    string_table_offset = 0

    for name, material in materials.items():
        encoded = name.encode('utf-8')
        string_table.append(encoded)

        material.name_offset = string_table_offset
        material.name_count  = len(encoded)

        string_table_offset += material.name_count

    for name, texture in textures.items():
        encoded = name.encode('utf-8')
        string_table.append(encoded)

        texture.name_offset = string_table_offset
        texture.name_count  = len(encoded)

        string_table_offset += texture.name_count

    for mesh in meshes:
        encoded = mesh.name.encode('utf-8')
        string_table.append(encoded)

        mesh.name_offset = string_table_offset
        mesh.name_count  = len(encoded)

        string_table_offset += mesh.name_count

    # string_table_offset will hold the full size of the string table at the end
    U32Write(file_handle, string_table_offset)

    for it in range(0, 10): U32Write(file_handle, 0)

    ### Header end

    # Write string table
    for s in string_table: file_handle.write(s)

    # Write material info
    R_MaterialsWrite(file_handle, materials)

    # Write texture info
    R_TextureInfoWrite(file_handle, textures)

    # Write mesh info
    for mesh in meshes:
        U32Write(file_handle, len(mesh.vertices))
        U32Write(file_handle, len(mesh.indices))

        U8Write(file_handle, mesh.flags)
        U8Write(file_handle, mesh.name_count)
        U16Write(file_handle, mesh.name_offset)

        R_MeshVerticesWrite(file_handle, mesh.vertices, (mesh.flags & R_MESH_FLAG_IS_SKINNED) != 0)

        for i in mesh.indices: U16Write(file_handle, i)

    # Copy all linked textures to output_dir/textures if there are any
    #
    if len(textures) > 0:
        textures_dir = output_dir + os.sep + "textures" + os.sep
        if not os.path.exists(textures_dir): os.mkdir(textures_dir)

        for name, texture in textures.items():
            to_path   = textures_dir + bpy.path.basename(texture.path)
            from_path = texture.path

            shutil.copyfile(from_path, to_path)

    return { 'FINISHED' }

# Blender specific export classes

class ExportProperties(bpy.types.PropertyGroup):
    output_dir:   bpy.props.StringProperty(name = "Output", subtype = 'DIR_PATH')
    forward_axis: bpy.props.EnumProperty(name = "Forward", items = AXES, default = "-Y")
    up_axis:      bpy.props.EnumProperty(name = "Up", items = AXES, default = "Z")
    flip_uv:      bpy.props.BoolProperty(name = "Flip Textures", default = True)

class AmtExporter(bpy.types.Operator):
    bl_idname = "scene.amt_exporter"
    bl_label  = "Export"

    def execute(self, context):
        # We create a directory in the output location with the same name of the project that
        # is currently open so we can collect all of the exported info in a single location
        #
        subdir_name = bpy.path.basename(bpy.data.filepath).split('.')[0]
        if not subdir_name: subdir_name = "[unnamed]"

        output_dir = bpy.path.abspath(context.scene.export_properties.output_dir) + os.sep + subdir_name

        # Nothing was set for the output dir and the project has no name!
        if output_dir == os.sep: return { 'CANCELLED' }

        if not os.path.exists(output_dir): os.mkdir(output_dir)

        R_MeshExport(output_dir)
        A_SkeletonExport(output_dir)

        return { 'FINISHED' }


class ExportPanel(bpy.types.Panel):
    bl_label       = "AMT Exporter"
    bl_idname      = "OBJECT_PT_layout"
    bl_space_type  = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context     = "output"

    def draw(self, context):
        self.layout.prop(context.scene.export_properties, "output_dir")
        self.layout.prop(context.scene.export_properties, "forward_axis")
        self.layout.prop(context.scene.export_properties, "up_axis")
        self.layout.prop(context.scene.export_properties, "flip_uv")
        self.layout.operator("scene.amt_exporter")

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
