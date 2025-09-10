/**
 * @file audio.h
 * @brief Audio system interface and definitions.
 */
#pragma once
#include "arena.h"
#include "consts.h"
#include "def.h"
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

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
    real32 volume;           // Per-source volume control
    
    // Static audio (fully loaded)
    struct {
        int16* samples;
        usize sample_count;
        usize frame_count;
        usize current_position;
    } static_data;
    
    // Streaming audio
    struct {
        stb_vorbis* vorbis;
        char* filename;         // Keep filename for reopening when looping
        int16* stream_buffer;     // Small buffer for streaming chunks
        usize buffer_frames;    // Size of stream buffer in frames
        usize buffer_position;  // Current position in stream buffer
        usize buffer_valid;     // How many frames in buffer are valid
        bool end_of_file;       // Have we reached EOF?
    } stream_data;
} AudioSource;

typedef struct {
    int16 audio[AUDIO_CAPACITY]; // Interleaved audio samples
    usize audio_size;     // Current size in samples
    
    AudioSource audio_sources[MAX_AUDIO_SOURCES]; // Array of audio sources
    usize audio_sources_size; // Current number of active sources

    real32 volume;         // Master volume control (0.0 to 1.0)
} AudioState;

static AudioState* audio_state;
static AudioState* create_audio_state(Arena* arena) {
    AudioState* state = (AudioState*)arena_alloc(arena, sizeof(AudioState));
    memset(state, 0, sizeof(AudioState));
    state->volume = 1.0f;
    return state;
}

AudioSource* create_audio_source_streaming(Arena* permanent_storage, AudioState* audio_state, const char* filename, int stream_buffer_frames, bool loop);
AudioSource* create_audio_source_static(Arena* permanent_storage, AudioState* audio_state, const char* filename, bool loop);
AudioSource* create_audio_source_static_memory(Arena* permanent_storage, AudioState* audio_state, const uint8* data, usize data_size, bool loop);

void audio_source_play(AudioSource* source);
void audio_source_stop(AudioSource* source);
void audio_source_set_volume(AudioSource* source, float volume);
void audio_source_cleanup(AudioSource* source);