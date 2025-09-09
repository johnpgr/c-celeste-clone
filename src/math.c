#include "def.h"

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

Vec2 vec2_div(Vec2 vec, real32 scalar) {
    return vec2(vec.x / scalar, vec.y / scalar);
}

IVec2 ivec2(int32 x, int32 y) {
    return (IVec2) {
        .x = x,
        .y = y,
    };
}

Vec3 vec3(real32 x, real32 y, real32 z) {
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
