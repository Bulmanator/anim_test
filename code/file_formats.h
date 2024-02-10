#if !defined(FILE_FORMATS_H_)
#define FILE_FORMATS_H_

// Skeleton (AMTS) file format
//
// [ Header       ]
// [ String Table ] // header.string_table_count in length
// [ Bone Info    ] // header.num_bones count
// [ Track Info   ] // header.num_tracks count
// [ Samples      ] // header.total_samples count
//
// Header {
//     U32 magic;   // == AMTS
//     U32 version; // == 1
//
//     U32 num_bones;
//     U32 num_tracks;
//
//     U32 total_samples;
//
//     U32 framerate;
//     U32 string_table_count;
//
//     U32 pad[9]; // to 64 bytes
// }
//
// StringTable {
//     U8 data[header.string_table_count]
// }
//
// BoneInfo {
//     F32 inv_bind_pose[16];
//     F32 bind_orientation[4];
//
//     U8  parent_index;
//     U8  name_count;
//     U16 name_offset; // from beginning of the string table
// }
//
// TrackInfo {
//     U8  flags;
//     U8  name_count;
//     U16 name_offset; // from beginning of the string table
//
//     U32 num_frames;
// }
//
// Sample {
//     F32 position[3];
//     F32 orientation[4];
//     F32 scale[3];
// }
//
#define AMTS_MAGIC   FourCC('A', 'M', 'T', 'S')
#define AMTS_VERSION 1

#pragma pack(push, 1)

typedef struct AMTS_Header AMTS_Header;
struct AMTS_Header {
    U32 magic;
    U32 version;

    U32 num_bones;
    U32 num_tracks;

    U32 total_samples;

    U32 framerate;
    U32 string_table_count;

    U32 pad[9];
};

StaticAssert(sizeof(AMTS_Header) == 64);

// All frames for all tracks are stored sequentially for easy loading, they are stored interleaved one per bone
// for each frame in order of the tracks present in the file.
//
// samples = all_samples;
//
// for tracks {
//     track->samples = samples;
//     samples += (header.num_bones * track->num_frames);
// }
//
typedef struct AMTS_Sample AMTS_Sample;
struct AMTS_Sample {
    F32 position[3];
    F32 orientation[4]; // :note quaternion orientation written in wxyz order
    F32 scale[3];
};

typedef struct AMTS_BoneInfo AMTS_BoneInfo;
struct AMTS_BoneInfo {
    AMTS_Sample bind_pose;
    AMTS_Sample inv_bind_pose;

    U8  parent_index;
    U8  name_count;
    U16 name_offset;
};

typedef struct AMTS_TrackInfo AMTS_TrackInfo;
struct AMTS_TrackInfo {
    U8  flags;
    U8  name_count;
    U16 name_offset;

    // Each frame has one sample for each bone in the skeleton to calculate the total samples a track
    // has simply perform: (track.num_frames * header.num_bones)
    //
    U32 num_frames;
};

#pragma pack(pop)

typedef struct AMTS_Skeleton AMTS_Skeleton;
struct AMTS_Skeleton {
    U32 version; // actual version loaded from file

    U32 framerate; // per second

    Str8 string_table;

    U32 num_bones;
    U32 num_tracks;

    AMTS_BoneInfo  *bones;
    AMTS_TrackInfo *tracks;

    U32 total_samples;
    AMTS_Sample *samples; // flat array of header.total_samples
};


// @todo: AMTS_SkeletonFromFile() function!! Need to think about file system api
//

Function void AMTS_SkeletonFromData(AMTS_Skeleton *skeleton, Str8 data);
Function void AMTS_SkeletonCopyFromData(Arena *arena, AMTS_Skeleton *skeleton, Str8 data);

// Mesh (AMTM) file format
//
// [ Header       ]
// [ String Table ]
// [ Materials    ]
// [ Texture Info ]
// [ Mesh Info    ]
//   - [ Vertex Data ] // per mesh
//   - [ Index Data  ] // per mesh
//
// Header {
//     U32 magic;   // == AMTM
//     U32 version; // == 1
//
//     U32 num_meshes;
//
//     U32 num_materials;
//     U32 num_textures;
//
//     U32 string_table_count;
//
//     U32 pad[10]; // to 64 bytes
// }
//
// String Table {
//     U8 data[header.string_table_count];
// }
//
// Material {
//     U16 name_offset;
//     U8  name_count;
//     U8  flags;
//
//     U32 colour; // RGBA little-endian
//
//     F32 properties[10];
//     U32 textures[8];   // -1 if unused, [8 bit channel_mask] [24 bit index]
// }
//
// TextureInfo {
//     U16 name_offset;
//     U16 name_count;
//
//     U16 flags;
//     U16 num_channels;
// }
//
// Texture data is not stored directly in the file, the name corresponds to the basename of the texture
// file. This excludes the extension and the user is free to decide how these names are used to link material
// textures with its corresponding data.
//
// MeshInfo {
//     U32 num_vertices;
//     U32 num_indices;
//
//     U8  flags;
//     U8  name_count;
//     U16 name_offset; // from beginning of the string table
// }
//
// Skinned vertices are used if the prior mesh info has the IS_SKINNED flag set, otherwise
// only normal vertices are used
//
// Vertex {
//     F32 position[3];
//     F32 uv[2];
//     F32 normal[3];
//
//     U32 material_index;
// }
//
// SkinnedVertex {
//     F32 position[3];
//     F32 uv[2];
//     F32 normal[3];
//
//     U32 material_index;
//
//     U8  bone_indices[4];
//     F32 bone_indices[4];
// }
//
// Index {
//     U16 value;
// }
//
// Properties ordering:
//     METALLIC
//     ROUGHNESS
//     IOR
//     ANISOTROPIC
//     ANISOTROPIC_ROTATION
//     COAT_WEIGHT
//     COAT_ROUGHNESS
//     SHEEN_WEIGHT
//     SHEEN_ROUGHNESS
//     UNUSED
//
// Texture ordering:
//     BASE_COLOUR
//     NORMAL
//     METALLIC
//     ROUGHNESS
//     OCCLUSION
//     DISPLACEMENT
//     UNUSED
//     UNUSED
//
// @todo: implement loading!
//

#endif  // FILE_FORMATS_H_
