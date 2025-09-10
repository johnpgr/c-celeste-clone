#pragma once

#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#endif

#define internal static

#define int8 int8_t
#define int16 int16_t
#define int32 int32_t
#define int64 int64_t

#define uint8 uint8_t
#define uint16 uint16_t
#define uint32 uint32_t
#define uint64 uint64_t
#define usize size_t

#define real32 float
#define real64 double

#define PI 3.14159265358979323846
#define EPSILON 1e-6

#define KB(number) ((number) * 1024ull)
#define MB(number) (KB(number) * 1024ull)
#define GB(number) (MB(number) * 1024ull)
#define TB(number) (GB(number) * 1024ull)
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
#define CLAMP(val, lo, hi) ((val) < (lo) ? (lo) : ((val) > (hi) ? (hi) : (val)))
#define RGBA(r, g, b, a) (r / 255.0f), (g / 255.0f), (b / 255.0f), (a / 255.0f)

#ifdef _WIN32
#define export __declspec(dllexport)
#elif __linux__
#define export
#elif __APPLE__
#define export
#endif

void debug_print(const char* format, ...);

#define TODO(msg) do { \
    debug_print("%s:%d:%d [TODO][%s()] %s\n", __FILE__, __LINE__, 1, __func__, msg); \
    abort(); \
} while(0)

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

Vec2 vec2(real32 x, real32 y);
Vec2 vec2_minus(Vec2 vec, Vec2 other);
Vec2 vec2_div(Vec2 vec, real32 scalar);
Vec2 vec2iv2(IVec2 vec);

IVec2 ivec2(int32 x, int32 y);
IVec2 ivec2_minus(IVec2 vec, IVec2 other);

Vec3 vec3(real32 x, real32 y, real32 z);

Vec4 vec4(real32 x, real32 y, real32 z, real32 w);

Mat4x4 create_orthographic(real32 left, real32 right, real32 top, real32 bottom);
