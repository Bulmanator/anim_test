#include <math.h>

struct Vec3F {
    F32 x, y, z;
};

struct Vec4F {
    F32 x, y, z, w;
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

struct Quat4F {
    F32 x, y, z, w;
};

static Vec3F V3FLerp(Vec3F a, Vec3F b, F32 t) {
    Vec3F result;
    result.x = ((1.0f - t) * a.x) + (t * b.x);
    result.y = ((1.0f - t) * a.y) + (t * b.y);
    result.z = ((1.0f - t) * a.z) + (t * b.z);

    return result;
}

static F32 V3FDot(Vec3F a, Vec3F b) {
    F32 result = (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
    return result;
}

static F32 V4FDot(Vec4F a, Vec4F b) {
    F32 result = (a.x * b.x) + (a.y * b.y) + (a.z * b.z) + (a.w * b.w);
    return result;
}

static Vec3F V3FAdd(Vec3F a, Vec3F b) {
    Vec3F result;
    result.x = (a.x + b.x);
    result.y = (a.y + b.y);
    result.z = (a.z + b.z);

    return result;
}

static Vec3F V3FMulF32(Vec3F v, F32 s) {
    Vec3F result;
    result.x = (v.x * s);
    result.y = (v.y * s);
    result.z = (v.z * s);

    return result;
}

static Vec3F V3FNeg(Vec3F v) {
    Vec3F result;
    result.x = -v.x;
    result.y = -v.y;
    result.z = -v.z;

    return result;
}

static Vec4F M4x4FMulV4F(Mat4x4F m, Vec4F v) {
    Vec4F result;
    result.x = V4FDot(m.r[0], v);
    result.y = V4FDot(m.r[1], v);
    result.z = V4FDot(m.r[2], v);
    result.w = V4FDot(m.r[3], v);

    return result;
}

static Mat4x4F M4x4FRotationX(F32 angle) {
    F32 s = sinf(angle);
    F32 c = cosf(angle);

    Mat4x4F result = {
        1, 0,  0, 0,
        0, c, -s, 0,
        0, s,  c, 0,
        0, 0,  0, 1
    };

    return result;
}

static Mat4x4F M4x4FRotationY(F32 angle) {
    F32 s = sinf(angle);
    F32 c = cosf(angle);

    Mat4x4F result = {
         c, 0, s, 0,
         0, 1, 0, 0,
        -s, 0, c, 0,
         0, 0, 0, 1
    };

    return result;
}

static Mat4x4F M4x4FRotationZ(F32 angle) {
    F32 s = sinf(angle);
    F32 c = cosf(angle);

    Mat4x4F result = {
        c, -s, 0, 0,
        s,  c, 0, 0,
        0,  0, 1, 0,
        0,  0, 0, 1
    };

    return result;
}

static Vec3F M4x4FRowExtract(Mat4x4F m, U32 r) {
    Vec4F row = m.r[r];

    Vec3F result = { row.x, row.y, row.z };
    return result;
}

static Vec3F M4x4FColumnExtract(Mat4x4F m, U32 c) {
    Vec3F result = { m.m[0][c], m.m[1][c], m.m[2][c] };
    return result;
}

static Mat4x4F M4x4FMul(Mat4x4F a, Mat4x4F b) {
    Mat4x4F result;

    for (U32 r = 0; r < 4; ++r) {
        for (U32 c = 0; c < 4; ++c) {
            result.m[r][c] =
                (a.m[r][0] * b.m[0][c]) + (a.m[r][1] * b.m[1][c]) +
                (a.m[r][2] * b.m[2][c]) + (a.m[r][3] * b.m[3][c]);
        }
    }

    return result;
}

static Vec3F M4x4FMulV3F(Mat4x4F m, Vec3F v) {
    Vec4F vv;
    vv.x = v.x;
    vv.y = v.y;
    vv.z = v.z;
    vv.w = 1.0f;

    Vec4F p = M4x4FMulV4F(m, vv);

    Vec3F result;
    result.x = p.x;
    result.y = p.y;
    result.z = p.z;
    return result;
}

static Mat4x4F M4x4FRows(Vec3F x, Vec3F y, Vec3F z) {
    Mat4x4F result = {
        x.x, x.y, x.z, 0,
        y.x, y.y, y.z, 0,
        z.x, z.y, z.z, 0,
        0,   0,   0,   1
    };

    return result;
}

static Mat4x4F M4x4FColumns(Vec3F x, Vec3F y, Vec3F z) {
    Mat4x4F result = {
        x.x, y.x, z.x, 0,
        x.y, y.y, z.y, 0,
        x.z, y.z, z.z, 0,
        0,   0,   0,   1
    };

    return result;
}

static Mat4x4F M4x4FTranslateV3F(Mat4x4F m, Vec3F v) {
    Mat4x4F result = m;

    result.r[0].w += v.x;
    result.r[1].w += v.y;
    result.r[2].w += v.z;

    return result;
}

static Mat4x4FInv M4x4FPerspectiveProjection(F32 focal_length, F32 aspect, F32 near_plane, F32 far_plane) {
    F32 a = focal_length / aspect;
    F32 b = focal_length;

    F32 c = (near_plane + far_plane) / (near_plane - far_plane);
    F32 d = (2.0f * near_plane * far_plane) / (near_plane - far_plane);

    Mat4x4FInv result = {
        // fwd
        //
        {
            a, 0 , 0, 0,
            0, b,  0, 0,
            0, 0,  c, d,
            0, 0, -1, 0
        },

        // inv
        {
            (1 / a), 0,      0,       0,
            0,      (1 / b), 0,       0,
            0,       0,      0,      -1,
            0,       0,     (1 / d), (c / d)
        }
    };

    return result;
}

static Mat4x4FInv M4x4FCameraViewProjection(Vec3F x, Vec3F y, Vec3F z, Vec3F p) {
    Mat4x4FInv result;

    result.fwd = M4x4FRows(x, y, z);

    Vec3F txp  = V3FNeg(M4x4FMulV3F(result.fwd, p));
    result.fwd = M4x4FTranslateV3F(result.fwd, txp);

    Vec3F ix = V3FMulF32(x, 1.0f / V3FDot(x, x));
    Vec3F iy = V3FMulF32(y, 1.0f / V3FDot(y, y));
    Vec3F iz = V3FMulF32(z, 1.0f / V3FDot(z, z));

    Vec3F ip;
    ip.x = (txp.x * ix.x) + (txp.y * iy.x) + (txp.z * iz.x);
    ip.y = (txp.x * ix.y) + (txp.y * iy.y) + (txp.z * iz.y);
    ip.z = (txp.x * ix.z) + (txp.y * iy.z) + (txp.z * iz.z);

    result.inv = M4x4FColumns(ix, iy, iz);
    result.inv = M4x4FTranslateV3F(result.inv, V3FNeg(ip));

    return result;
}

static Mat4x4F M4x4FIdentity() {
    Mat4x4F result = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    return result;
}

static Quat4F Q4FNormalise(Quat4F q) {
    Quat4F result = { 0, 0, 0, 1 };

    F32 sq = sqrtf((q.w * q.w) + (q.x * q.x) + (q.y * q.y) + (q.z * q.z));
    if (sq != 0) {
        F32 inv = 1.0f / sq;

        result.w = q.w * inv;
        result.x = q.x * inv;
        result.y = q.y * inv;
        result.z = q.z * inv;
    }

    return result;
}

static Quat4F Q4FNeg(Quat4F q) {
    Quat4F result;
    result.w = -q.w;
    result.x = -q.x;
    result.y = -q.y;
    result.z = -q.z;

    return result;
}

static F32 Q4FDot(Quat4F a, Quat4F b) {
    F32 result = (a.w * b.w) + (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
    return result;
}

static Quat4F Q4FNLerp(Quat4F a, Quat4F b, F32 t) {
    Quat4F result;
    result.w = ((1.0f - t) * a.w) + (t * b.w);
    result.x = ((1.0f - t) * a.x) + (t * b.x);
    result.y = ((1.0f - t) * a.y) + (t * b.y);
    result.z = ((1.0f - t) * a.z) + (t * b.z);

    result = Q4FNormalise(result);

    return result;
}
static Mat4x4F Q4FToM4x4F(Quat4F q) {
    Mat4x4F result;

    F32 xx = (q.x * q.x);
    F32 yy = (q.y * q.y);
    F32 zz = (q.z * q.z);

    F32 xy = (q.x * q.y);
    F32 xz = (q.x * q.z);
    F32 xw = (q.x * q.w);

    F32 yz = (q.y * q.z);
    F32 yw = (q.y * q.w);

    F32 zw = (q.z * q.w);

    // row 0
    result.m[0][0] = 1 - 2 * yy - 2 * zz;
    result.m[0][1] =     2 * xy - 2 * zw;
    result.m[0][2] =     2 * xz + 2 * yw;
    result.m[0][3] = 0;

    // row 1
    result.m[1][0] =     2 * xy + 2 * zw;
    result.m[1][1] = 1 - 2 * xx - 2 * zz;
    result.m[1][2] =     2 * yz - 2 * xw;
    result.m[1][3] = 0;

    // row 2
    result.m[2][0] =     2 * xz - 2 * yw;
    result.m[2][1] =     2 * yz + 2 * xw;
    result.m[2][2] = 1 - 2 * xx - 2 * yy;
    result.m[2][3] = 0;

    // row 3
    result.m[3][0] = 0;
    result.m[3][1] = 0;
    result.m[3][2] = 0;
    result.m[3][3] = 1;

    return result;
}
