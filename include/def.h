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

/**
 * @brief Creates a 32-bit RGBA pixel value with correct byte ordering for graphics APIs
 * 
 * This macro packs RGBA components into a 32-bit integer with the byte layout:
 * [Blue][Green][Red][Alpha] (little-endian memory layout)
 * 
 * This specific ordering ensures compatibility with:
 * - Windows GDI: Expects BGRA format in DIB sections
 * - macOS Cocoa: Uses kCGImageAlphaPremultipliedFirst | kCGImageByteOrder32Little
 * 
 * The macro places:
 * - Blue in bits 0-7 (least significant byte)
 * - Green in bits 8-15
 * - Red in bits 16-23  
 * - Alpha in bits 24-31 (most significant byte)
 * 
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param a Alpha component (0-255)
 * @return 32-bit pixel value ready for direct use in framebuffers
 */
#define RGBA(r, g, b, a) ((((b)&0xFF)<<(8*0)) | (((g)&0xFF)<<(8*1)) | (((r)&0xFF)<<(8*2)) | (((a)&0xFF)<<(8*3)))

void debug_print(const char* format, ...);

#define TODO(msg) do { \
    debug_print("%s:%d:%d [TODO][%s()] %s\n", __FILE__, __LINE__, 1, __func__, msg); \
    abort(); \
} while(0)
