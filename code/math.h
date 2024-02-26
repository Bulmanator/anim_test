#if !defined(ANIM_MATH_H_)
#define ANIM_MATH_H_

union Vec3F {
    struct {
        F32 x, y, z;
    };

    struct {
        F32 r, g, b;
    };

    struct {
        F32 w, h, d;
    };

    F32 e[3];
};

union Vec4F {
    struct {
        F32 x, y, z;
        F32 w;
    };

    struct {
        F32 r, g, b;
        F32 a;
    };

    struct {
        Vec3F xyz;
        F32   __w;
    };

    F32 e[4];
};

union Mat4x4F {
    F32   m[4][4];
    F32   e[16];
    Vec4F r[4];
};

struct Mat4x4FInv {
    Mat4x4F fwd;
    Mat4x4F inv;
};

union Quat4F {
    struct {
        F32 w, x, y, z;
    };

    struct {
        F32   __w;
        Vec3F xyz;
    };

    struct {
        F32   r; // real
        Vec3F i; // imaginary
    };

    F32 e[4];
};

// Construction
//
Func Vec3F V3F(F32 x, F32 y, F32 z);
Func Vec4F V4F(F32 x, F32 y, F32 z, F32 w);

Func Quat4F  Q4FIdentity();
Func Mat4x4F M4x4FIdentity();

Func Mat4x4F M4x4FRotationX(F32 turns);
Func Mat4x4F M4x4FRotationY(F32 turns);
Func Mat4x4F M4x4FRotationZ(F32 turns);

Func Mat4x4F M4x4FRows(Vec3F x, Vec3F y, Vec3F z);
Func Mat4x4F M4x4FColumns(Vec3F x, Vec3F y, Vec3F z);

Func Mat4x4FInv M4x4FPerspectiveProjection(F32 focal_length, F32 aspect, F32 near_plane, F32 far_plane);
Func Mat4x4FInv M4x4FCameraViewProjection(Vec3F x, Vec3F y, Vec3F z, Vec3F p);


// Conversion
//
Func Mat4x4F Q4FToM4x4F(Quat4F q);

// Operators
//
Func Vec3F V3FAdd(Vec3F a, Vec3F b);
Func Vec4F V4FAdd(Vec4F a, Vec4F b);

Func Vec3F  V3FNeg(Vec3F  a);
Func Vec4F  V4FNeg(Vec4F  a);
Func Quat4F Q4FNeg(Quat4F a);

Func Vec3F V3FHadamard(Vec3F a, Vec3F b);
Func Vec4F V4FHadamard(Vec4F a, Vec4F b);

Func Vec3F V3FScale(Vec3F a, F32 b);
Func Vec4F V4FScale(Vec4F a, F32 b);

Func Mat4x4F M4x4FMul(Mat4x4F a, Mat4x4F b);

Func Vec3F M4x4FMulV3F(Mat4x4F a, Vec3F b);
Func Vec4F M4x4FMulV4F(Mat4x4F a, Vec4F b);

// Others
//
Func Vec3F V3FLerp(Vec3F a, Vec3F b, F32 t);
Func Vec4F V4FLerp(Vec4F a, Vec4F b, F32 t);

Func Quat4F Q4FNormalizedLerp(Quat4F a, Quat4F b, F32 t);

Func F32 V3FDot(Vec3F  a, Vec3F  b);
Func F32 V4FDot(Vec4F  a, Vec4F  b);
Func F32 Q4FDot(Quat4F a, Quat4F b);

Func Vec3F  V3FNormalize(Vec3F  a);
Func Vec4F  V4FNormalize(Vec4F  a);
Func Quat4F Q4FNormalize(Quat4F a);

Func Vec4F M4x4FRowExtract(Mat4x4F m, U32 r);
Func Vec4F M4x4FColumnExtract(Mat4x4F m, U32 c);

Func Mat4x4F M4x4FTranslateV3F(Mat4x4F m, Vec3F v);

#endif  // ANIM_MATH_H_
