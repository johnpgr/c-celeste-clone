#pragma once

/**
 * TODO:
 * - Save game locations
 * - Getting a handle to our executable file
 * - Asset loading path
 * - Threading (launch a thread)
 * - Raw input (Support for multiple keyboard)
 * - Sleep/timeBeginPeriod
 * - ClipCursor() (For multi monitor support)
 * - Fullscreen support
 * - WM_SETCURSOR (control cursor visibility)
 * - QueryCancelAutoplay
 * - WM_ACTIVATEAPP (for when we are not the main application)
*/

#include "def.h"
#include "arena.h"
#include "audio-source.h"

#define TITLE "The game"
#define FPS 60
#define DELTA_TIME (1.0f/FPS)
#define DISPLAY_WIDTH 800
#define DISPLAY_HEIGHT 600
#define DISPLAY_CAPACITY (DISPLAY_WIDTH * DISPLAY_HEIGHT)
#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_CHANNELS 2
#define AUDIO_CAPACITY ((AUDIO_SAMPLE_RATE / FPS) * AUDIO_CHANNELS)

static_assert(AUDIO_SAMPLE_RATE % FPS == 0, "Audio sample rate must be divisible by fps");

typedef struct {
    char* title;
    usize fps;

    bool frame_skip;
    uint64 frame_count;
    uint64 fps_timer_start;
    real64 current_fps;

    Arena permanent_arena;
    Arena transient_arena;

    uint32* display;
    usize display_capacity;
    usize display_width;
    usize display_height;

    int16* audio;
    usize audio_capacity;
    usize audio_sample_rate;
    usize audio_channels;
    real32 audio_volume;
    
    AudioSource* audio_sources;
    usize audio_sources_capacity;
    usize audio_sources_size;

    uint32 blue_offset;
    uint32 green_offset;
} Game;

Game game_init(void);
void game_cleanup(Game* game);
void game_update_and_render(Game* game);
void game_key_up(int key);
void game_key_down(int key);

// Audio loading functions
void game_play_audio_source(AudioSource* source);
void game_stop_audio_source(AudioSource* source);
void game_set_audio_volume(AudioSource* source, real32 volume);
void game_free_audio_source(AudioSource* source);
void game_generate_audio(Game* game);
