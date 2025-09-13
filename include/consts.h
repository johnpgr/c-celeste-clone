#pragma once
#include "math3d.h"

constexpr int FPS = 60;
constexpr real32 DELTA_TIME = 1.0f / FPS;

constexpr int AUDIO_SAMPLE_RATE = 48000;
constexpr int AUDIO_CHANNELS = 2;
constexpr int AUDIO_CAPACITY = (AUDIO_SAMPLE_RATE / FPS) * AUDIO_CHANNELS;

constexpr int MAX_AUDIO_SOURCES = 16;
constexpr int STREAM_BUFFER_FRAMES = 4096;

constexpr int WORLD_WIDTH = 320;
constexpr int WORLD_HEIGHT = 180;
constexpr int TILESIZE = 8;
constexpr IVec2 WORLD_GRID = (IVec2){WORLD_WIDTH / TILESIZE, WORLD_HEIGHT / TILESIZE};
