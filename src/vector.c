#include "def.h"
#include <math.h>

typedef struct {
    real32 x, y;
} Vec2;

static Vec2 vec2(real32 x, real32 y)
{
    return (Vec2) {
        .x = x,
        .y = y,
    };
}

typedef struct {
    real32 x, y, z;
} Vec3;

static Vec3 vec3(real32 x, real32 y, real32 z)
{
    return (Vec3) {
        .x = x,
        .y = y,
        .z = z,
    };
}

static Vec2 project_3d_2d(Vec3 v3)
{
    if (v3.z < 0) v3.z = -v3.z;
    if (v3.z < EPSILON) v3.z += EPSILON;
    return vec2(v3.x/v3.z, v3.y/v3.z);
}

static Vec2 project_2d_scr(Vec2 v2, int32 w, int32 h)
{
    return vec2((v2.x + 1)/2*w, (1 - (v2.y + 1)/2)*h);
}

static Vec3 vec3_rotate_y(Vec3 p, real32 delta_angle)
{
    real32 angle = atan2f(p.z, p.x) + delta_angle;
    real32 mag = sqrtf(p.x*p.x + p.z*p.z);
    return vec3(cosf(angle)*mag, p.y, sinf(angle)*mag);
}

static real32 vec3_dot(Vec3 a, Vec3 b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}
