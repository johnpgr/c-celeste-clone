#pragma once

#include "arena.h"
#include "def.h"
#include "audio-source.h"

#define TITLE "The game"
#define FPS 60
#define DELTA_TIME (1.0f/FPS)
#define WIDTH 800
#define HEIGHT 600
#define DISPLAY_CAPACITY (WIDTH * HEIGHT)
#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_CHANNELS 2
#define AUDIO_CAPACITY ((AUDIO_SAMPLE_RATE / FPS) * AUDIO_CHANNELS)

static_assert(AUDIO_SAMPLE_RATE % FPS == 0);

typedef struct {
    char* title;
    usize fps;

    Arena permanent_arena;
    Arena transient_arena;

    u32* display;
    usize display_capacity;
    usize display_width;
    usize display_height;

    i16* audio;
    usize audio_capacity;
    usize audio_sample_rate;
    usize audio_channels;
    f32 audio_volume;
    
    // Audio system
    AudioSource* loaded_audio;
    usize max_audio_sources;
    usize active_audio_count;
} Game;

Game game_init(void);
void game_cleanup(Game* game);
void game_update_and_render(Game* game);
void game_key_up(int key);
void game_key_down(int key);

// Audio loading functions
void game_play_audio_source(AudioSource* source);
void game_stop_audio_source(AudioSource* source);
void game_set_audio_volume(AudioSource* source, float volume);
void game_free_audio_source(AudioSource* source);
void game_generate_audio(Game* game);
