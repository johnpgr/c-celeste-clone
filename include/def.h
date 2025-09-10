#pragma once

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#endif

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
#define EXPORT_FN __declspec(dllexport)
#elif __linux__
#define EXPORT_FN
#elif __APPLE__
#define EXPORT_FN
#endif

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
void debug_print(const char* format, ...) {
#ifndef DEBUG_MODE
    return;
#endif

    const int buffer_size = 1024;
    char buffer[buffer_size];

    va_list args;
    va_start(args, format);

    vsnprintf(buffer, buffer_size, format, args);

    va_end(args);
#ifdef _WIN32
    if (IsDebuggerPresent()) {
        OutputDebugStringA(buffer);
    } else {
        fprintf(stderr, "%s", buffer);
    }
#else
    fprintf(stderr, "%s", buffer);
#endif
}

#define TODO(msg) do { \
    debug_print("%s:%d:%d [TODO][%s()] %s\n", __FILE__, __LINE__, 1, __func__, msg); \
    abort(); \
} while(0)
