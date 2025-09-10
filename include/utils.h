#pragma once

#include "def.h"
#include "renderer.h"

#ifdef _WIN32
#include <windows.h>
#include <stdint.h>
#else
#define __USE_POSIX199309
#include <time.h>
#include <unistd.h>
#endif

#define NANOS_PER_SEC (1000*1000*1000)
#define NANOS_PER_MILLI (1000*1000)

static inline uint64 current_time_nanos(void) {
#ifdef _WIN32
    LARGE_INTEGER Time;
    QueryPerformanceCounter(&Time);

    static LARGE_INTEGER Frequency = {};
    if (Frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&Frequency);
    }

    uint64 Secs  = Time.QuadPart / Frequency.QuadPart;
    uint64 Nanos = Time.QuadPart % Frequency.QuadPart * NANOS_PER_SEC / Frequency.QuadPart;
    return NANOS_PER_SEC * Secs + Nanos;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return NANOS_PER_SEC * ts.tv_sec + ts.tv_nsec;
#endif // _WIN32
}

static inline void sleep_nanos(uint64 nanos) {
#ifdef _WIN32
    Sleep((nanos + 999999) / 1000000);
#else
    struct timespec ts;
    ts.tv_sec = nanos / 1000000000;
    ts.tv_nsec = nanos % 1000000000;
    nanosleep(&ts, nullptr);
#endif
}

static IVec2 screen_to_world(InputState* input_state, RendererState* renderer_state, IVec2 screen_pos) {
    OrthographicCamera2D camera = renderer_state->game_camera;

    int x = (real32)screen_pos.x / 
                (real32)input_state->screen_size.x * 
                camera.dimensions.x; // [0; dimensions.x]

    // Offset using dimensions and position
    x += -camera.dimensions.x / 2.0f + camera.position.x;

    int y = (real32)screen_pos.y / 
                (real32)input_state->screen_size.y * 
                camera.dimensions.y; // [0; dimensions.y]

    // Offset using dimensions and position
    y += camera.dimensions.y / 2.0f + camera.position.y;

    return ivec2(x, y);
}
