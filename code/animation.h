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

    Mat4x4F inv_bind_pose;
    Quat4F  bind_pose_orientation;
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

Function Mat4x4F A_SampleToM4x4F(A_Sample *sample);

Function A_Sample A_SampleLerp(A_Sample *a, A_Sample *b, F32 t);

Function A_Sample *A_AnimationSamplesForFrame(A_Animation *animation, U32 num_bones, U32 frame_index);

Function void A_AnimationEvaluate(A_Sample *output_samples, A_Skeleton *skeleton, U32 animation_index, F32 dt);
Function void A_AnimationBoneMatricesGet(Mat4x4F *output_matrices, A_Skeleton *skeleton, A_Sample *samples);

#endif  // ANIMATION_H_
