#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <windows.h>
#endif

#define i8 int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t
#define usize size_t

#define PI 3.14159265358979323846

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
#define RGBA(r, g, b, a) ((((r)&0xFF)<<(8*0)) | (((g)&0xFF)<<(8*1)) | (((b)&0xFF)<<(8*2)) | (((a)&0xFF)<<(8*3)))

#define TODO(msg) do { \
    LOG("%s:%d:%d [TODO][%s()] %s\n", __FILE__, __LINE__, 1, __func__, msg); \
    abort(); \
} while(0)

extern void LOG(const char* format, ...);
