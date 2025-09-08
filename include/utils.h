#pragma once

#include "def.h"

#ifdef _WIN32
#include <windows.h>
#include <stdint.h>
#else
#define __USE_POSIX199309
#include <time.h>
#include <unistd.h>
#endif

#define NANOS_PER_SEC (1000*1000*1000)

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
