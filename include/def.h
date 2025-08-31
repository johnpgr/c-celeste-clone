#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
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

/**
 * @brief A simple cross-platform logging function.
 *
 * This function logs a formatted message to the appropriate system logger.
 * On Windows, it uses OutputDebugStringA, which is useful for debugging.
 * On Linux and macOS, it uses the standard printf to stdout for demonstration.
 *
 * @param format The format string, similar to printf.
 * @param ... The variable arguments to be formatted.
 */
inline void LOG(const char* format, ...) {
#ifdef _WIN32
    const int buffer_size = 1024;
    char buffer[buffer_size];
    
    va_list args;
    va_start(args, format);

    vsnprintf(buffer, buffer_size, format, args);

    va_end(args);
    OutputDebugStringA(buffer);
#else
    va_list args;
    va_start(args, format);
    fprintf(stderr, format, args);
    va_end(args);
#endif
}
