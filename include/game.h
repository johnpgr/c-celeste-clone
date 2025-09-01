#pragma once

#include "def.h"

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

typedef struct stb_vorbis stb_vorbis;

typedef enum {
    AUDIO_SOURCE_NONE      = 0,  // Uninitialized/empty slot
    AUDIO_SOURCE_STATIC    = 1,  // Fully loaded in memory
    AUDIO_SOURCE_STREAMING = 2,  // Streamed from file
} AudioSourceType;

typedef struct {
    // Common fields
    AudioSourceType type;
    int channels;
    int sample_rate;
    bool is_playing;
    bool loop;
    float volume;           // Per-source volume control
    
    // Static audio (fully loaded)
    struct {
        i16* samples;
        usize sample_count;
        usize frame_count;
        usize current_position;
    } static_data;
    
    // Streaming audio
    struct {
        stb_vorbis* vorbis;
        char* filename;         // Keep filename for reopening when looping
        i16* stream_buffer;     // Small buffer for streaming chunks
        usize buffer_frames;    // Size of stream buffer in frames
        usize buffer_position;  // Current position in stream buffer
        usize buffer_valid;     // How many frames in buffer are valid
        bool end_of_file;       // Have we reached EOF?
    } stream_data;
} AudioSource;


typedef struct {
    char* title;
    usize fps;

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
void game_update_and_render(Game* game);
void game_key_up(int key);
void game_key_down(int key);

// Audio loading functions
AudioSource* game_load_ogg_static(Game* game, const char* filename, bool loop);
AudioSource* game_load_ogg_static_from_memory(Game* game, const u8* data, usize data_size, bool loop);
AudioSource* game_load_ogg_streaming(Game* game, const char* filename, bool loop);
void game_play_audio_source(AudioSource* source);
void game_stop_audio_source(AudioSource* source);
void game_set_audio_volume(AudioSource* source, float volume);
void game_free_audio_source(AudioSource* source);
void game_generate_audio(Game* game);
void game_reset_transient_arena(void);
void game_reset_permanent_arena(void);
usize game_get_permanent_arena_usage(void);
usize game_get_transient_arena_usage(void);
void game_print_arena_stats(void);

void free_all_audio_sources();
