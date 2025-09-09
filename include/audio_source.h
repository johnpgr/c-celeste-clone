#pragma once
#include "def.h"

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
