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

#endif  // FILE_FORMATS_H_
