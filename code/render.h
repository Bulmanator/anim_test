#if !defined(RENDER_H_)
#define RENDER_H_

typedef struct R_Setup R_Setup;
struct R_Setup {
    Mat4x4F view_proj;

    Vec3F view_p;
    F32 time;

    F32 dt;
    U32 window_width;
    U32 window_height;

    F32 unused[9]; // pad to 128 bytes, for push constants this is the guaranteed minimum
};

#endif  // RENDER_H_
