#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#endif

#define internal static

#define i8 int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t
#define usize size_t

#define f32 float
#define f64 double

#define PI 3.14159265358979323846

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
#define RGBA(r, g, b, a) ((((r)&0xFF)<<(8*0)) | (((g)&0xFF)<<(8*1)) | (((b)&0xFF)<<(8*2)) | (((a)&0xFF)<<(8*3)))
#define CLAMP(val, lo, hi) ((val) < (lo) ? (lo) : ((val) > (hi) ? (hi) : (val)))

extern void debug_print(const char* format, ...);

#define TODO(msg) do { \
    debug_print("%s:%d:%d [TODO][%s()] %s\n", __FILE__, __LINE__, 1, __func__, msg); \
    abort(); \
} while(0)

static inline void defer_cleanup(void (^*b)(void)) { (*b)(); }
#define defer_merge(a,b) a##b
#define defer_varname(a) defer_merge(defer_scopevar_, a)
#define defer __attribute__((cleanup(defer_cleanup))) void (^defer_varname(__COUNTER__))(void) =^
