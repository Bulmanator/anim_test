#if !defined(ANIMATION_H_)
#define ANIMATION_H_

typedef struct A_Sample A_Sample;
struct A_Sample {
    Vec3F  position;
    Quat4F orientation;
    Vec3F  scale;
};

typedef struct A_Bone A_Bone;
struct A_Bone {
    Str8 name;

    U32 parent_index;

    Mat4x4F  inv_bind_pose;
    A_Sample bind_pose;
};

typedef struct A_Animation A_Animation;
struct A_Animation {
    Str8 name;

    U32 num_frames;
    F32 time;
    F32 time_scale;

    A_Sample *samples; // base sample for first frame, indexed via (num_bones * frame_index)
};

typedef struct A_Skeleton A_Skeleton;
struct A_Skeleton {
    U32 framerate;

    Str8 string_table;

    U32 num_bones;
    U32 num_animations;

    A_Bone      *bones;
    A_Animation *animations;
};

Func Mat4x4F A_SampleToM4x4F(A_Sample *sample);

Func A_Sample A_SampleLerp(A_Sample *a, A_Sample *b, F32 t);

Func A_Sample *A_AnimationSamplesForFrame(A_Animation *animation, U32 num_bones, U32 frame_index);

Func void A_AnimationEvaluate(A_Sample *output_samples, A_Skeleton *skeleton, U32 animation_index, F32 dt);
Func void A_AnimationBoneMatricesGet(Mat4x4F *output_matrices, A_Skeleton *skeleton, A_Sample *samples);

// Mesh file
//
struct A_Material {
    Str8 name;

    U32 colour;

    F32 roughness;
    F32 metallic;
    F32 ior;

    // Other properties not currently used, just here to make sure we are loading correctly
    //
    F32 anisotropic;
    F32 anisotropic_rotation;

    F32 clear_coat;
    F32 clear_coat_roughness;

    F32 sheen;
    F32 sheen_roughness;

    // This is the only texture we use at the moment
    //
    U32 albedo_index;
};

struct A_Submesh {
    Str8 name;

    U32 flags; // skinned or not?

    // used for drawing
    //
    U32 base_vertex;
    U32 base_index;
    U32 num_indices;

    U32 num_vertices; // we only really need to know this for the length of the array below

    void *vertices;
    void *indices;
};

struct A_Texture {
    Str8 name;

    U32 width;
    U32 height;

    void *pixels;
};

struct A_Mesh {
    Str8 string_table;

    U32 num_submeshes;
    U32 num_materials;
    U32 num_textures;

    A_Submesh  *submeshes;
    A_Material *materials;
    A_Texture  *textures;
};

#endif  // ANIMATION_H_
