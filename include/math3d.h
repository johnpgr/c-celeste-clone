#pragma once
#include "def.h"

typedef struct {
    real32 x, y;
} Vec2;

typedef struct {
    int32 x, y;
} IVec2;

typedef struct {
    real32 x, y, z, w;
} Vec3;

typedef struct {
    union {
        real32 v[4];
        struct {
            real32 x;
            real32 y;
            real32 z;
            real32 w;
        };
        struct {
            real32 r;
            real32 g;
            real32 b;
            real32 a;
        };
    };
} Vec4;

typedef struct {
    union { 
        Vec4 v[4];
        struct {
            real32 ax;
            real32 bx;
            real32 cx;
            real32 dx;

            real32 ay;
            real32 by;
            real32 cy;
            real32 dy;

            real32 az;
            real32 bz;
            real32 cz;
            real32 dz;

            real32 aw;
            real32 bw;
            real32 cw;
            real32 dw;
        };
    };
} Mat4x4;

Vec2 vec2(real32 x, real32 y) {
    return (Vec2) {
        .x = x,
        .y = y,
    };
}

Vec2 vec2iv2(IVec2 vec){
    return vec2((real32)vec.x, (real32)vec.y);
}

Vec2 vec2_minus(Vec2 vec, Vec2 other) {
    return vec2(vec.x - other.x, vec.y - other.y);
}

Vec2 vec2_plus(Vec2 vec, Vec2 other) {
    return vec2(vec.x + other.x, vec.y + other.y);
}

Vec2 vec2_mult(Vec2 vec, real32 scalar) {
    return vec2(vec.x * scalar, vec.y * scalar);
}

Vec2 vec2_div(Vec2 vec, real32 scalar) {
    return vec2(vec.x / scalar, vec.y / scalar);
}

real32 vec2_dot(Vec2 vec, Vec2 other) {
    return vec.x * other.x + vec.y * other.y;
}

real32 vec2_length_squared(Vec2 vec) {
    return vec2_dot(vec, vec);
}

real32 vec2_length(Vec2 vec) {
    return sqrtf(vec2_length_squared(vec));
}

Vec2 vec2_normalize(Vec2 vec) {
    real32 len = vec2_length(vec);
    if (len < EPSILON) return vec2(0, 0);
    return vec2_div(vec, len);
}

Vec2 vec2_lerp(Vec2 a, Vec2 b, real32 t) {
    return vec2_plus(vec2_mult(a, 1.0f - t), vec2_mult(b, t));
}

IVec2 ivec2(int32 x, int32 y) {
    return (IVec2) {
        .x = x,
        .y = y,
    };
}

IVec2 ivec2_minus(IVec2 vec, IVec2 other) {
    return ivec2(vec.x - other.x, vec.y - other.y);
}

IVec2 ivec2_plus(IVec2 vec, IVec2 other) {
    return ivec2(vec.x + other.x, vec.y + other.y);
}

IVec2 ivec2_mult(IVec2 vec, int32 scalar) {
    return ivec2(vec.x * scalar, vec.y * scalar);
}

IVec2 ivec2_div(IVec2 vec, int32 scalar) {
    return ivec2(vec.x / scalar, vec.y / scalar);
}

int32 ivec2_dot(IVec2 vec, IVec2 other) {
    return vec.x * other.x + vec.y * other.y;
}Vec3 vec3(real32 x, real32 y, real32 z) {
    return (Vec3) {
        .x = x,
        .y = y,
        .z = z,
    };
}

Vec4 vec4(real32 x, real32 y, real32 z, real32 w) {
    return (Vec4) {
        .x = x,
        .y = y,
        .z = z,
        .w = w,
    };
}

Mat4x4 create_orthographic(
    real32 left,
    real32 right,
    real32 top,
    real32 bottom
) {
    Mat4x4 result = {};

    result.aw = -(right + left) / (right - left);
    result.bw = (top + bottom) / (top - bottom);
    result.cw = 0.0; // Near plane
    result.v[0].v[0] = 2.0 / (right - left);
    result.v[1].v[1] = 2.0 / (top - bottom);
    result.v[2].v[2] = 1.0 / (1.0 - 0.0);
    result.v[3].v[3] = 1.0;

    return result;
}
