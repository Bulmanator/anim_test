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

typedef struct R_Material R_Material;
struct R_Material {
    U32 colour; // RGBA

    F32 metallic;
    F32 roughness;
    F32 ior;

    // @todo: more can be added in the future!
    //
};

typedef struct R_Vertex3 R_Vertex3;
struct R_Vertex3 {
    Vec3F position;
    U16   uv[2];
    U8    normal[4];

    // this doesn't really need to be a U32 but the struct will likely be
    // padded anyway because 22 bytes is a weird size (i.e. for a U16)
    //
    U32 material_index;
};

// 32 bytes
//
// @todo: are U8 bone weights too compressed?
//
typedef struct R_SkinnedVertex3 R_SkinnedVertex3;
struct R_SkinnedVertex3 {
    union {
        struct {
            Vec3F position;
            U16   uv[2];
            U8    normal[4];

            U32 material_index;
        };

        R_Vertex3 vertex;
    };

    U8 bone_indices[4];
    U8 bone_weights[4];
};



#endif  // RENDER_H_
