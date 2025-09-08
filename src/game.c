#include <stdio.h>
#include <string.h>
#include "game.h"
#include "ogg.h"
#include "utils.h"
#include "vector.c"

#define MAX_AUDIO_SOURCES 16
#define STREAM_BUFFER_FRAMES 4096

#define PERMANENT_ARENA_SIZE (64 * 1024 * 1024)
#define TRANSIENT_ARENA_SIZE (128 * 1024 * 1024)

static struct {
    uint32 display[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    int16 audio[AUDIO_CAPACITY];
    AudioSource audio_sources[MAX_AUDIO_SOURCES];
    uint8 permanent_memory[PERMANENT_ARENA_SIZE];
    uint8 transient_memory[TRANSIENT_ARENA_SIZE];
} global;

typedef enum {
    FACE_V1,
    FACE_V2,
    FACE_V3,
    FACE_VT1,
    FACE_VT2,
    FACE_VT3,
    FACE_VN1,
    FACE_VN2,
    FACE_VN3,
} Face_Index;

Game game_init(void) {
    debug_print("Initializing game...\n");
    debug_print("  Display: %dx%d pixels\n", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    debug_print("  Audio: %d Hz, %zu channels, %zu capacity\n", AUDIO_SAMPLE_RATE, AUDIO_CHANNELS, AUDIO_CAPACITY);
    debug_print("  FPS: %d\n", FPS);
    debug_print("  Max audio sources: %d\n", MAX_AUDIO_SOURCES);

    // Permanent arena (for static audio, textures, etc.)
    Arena permanent_arena = {
        .memory = global.permanent_memory,
        .size = PERMANENT_ARENA_SIZE,
        .offset = 0,
        .name = "Permanent",
    };
    debug_print("  Permanent arena: %.1f KB initialized\n", permanent_arena.size / 1024.0f);
    
    // Transient arena (for temporary data during processing)
    Arena transient_arena = {
        .memory = global.transient_memory,
        .size = TRANSIENT_ARENA_SIZE,
        .offset = 0,
        .name = "Transient",
    };
    debug_print("  Transient arena: %.1f KB initialized\n", transient_arena.size / 1024.0f);
    
    Game game = {
        .title                   = (char*)TITLE,
        .fps                     = FPS,
        .frame_skip              = false,
        .frame_count             = 0,
        .fps_timer_start         = current_time_nanos(),
        .current_fps             = 0.0,

        .permanent_arena         = permanent_arena,
        .transient_arena         = transient_arena,

        .display                 = global.display,
        .display_width           = DISPLAY_WIDTH,
        .display_height          = DISPLAY_HEIGHT,

        .audio                   = global.audio,
        .audio_capacity          = AUDIO_CAPACITY,
        .audio_sample_rate       = AUDIO_SAMPLE_RATE,
        .audio_channels          = AUDIO_CHANNELS,
        .audio_sources           = global.audio_sources,
        .audio_sources_capacity  = MAX_AUDIO_SOURCES,
        .audio_sources_size      = 0,
    };
    
    debug_print("Game initialized successfully\n");
    return game;
};

internal void print_arena_stats(Game* game) {
    debug_print("Arena statistics:\n");
    debug_print("  Permanent: %.1f/%.1f KB used (%.1f%%, %.1f KB remaining)\n",
               arena_get_used(&game->permanent_arena) / 1024.0f, 
               game->permanent_arena.size / 1024.0f,
               (real32)arena_get_used(&game->permanent_arena) / game->permanent_arena.size * 100.0f,
               arena_get_remaining(&game->permanent_arena) / 1024.0f);
    debug_print("  Transient: %.1f/%.1f KB used (%.1f%%, %.1f KB remaining)\n",
               arena_get_used(&game->transient_arena) / 1024.0f,
               game->transient_arena.size / 1024.0f,
               (real32)arena_get_used(&game->transient_arena) / game->transient_arena.size * 100.0f,
               arena_get_remaining(&game->transient_arena) / 1024.0f);
}

internal void render_weird_gradient(Game* game) {
    uint8* row = (uint8*)game->display;
    for (usize y = 0; y < game->display_height; y++) {
        uint32* pixel = (uint32*)row;
        for (usize x = 0; x < game->display_width; x++) {
            uint8 blue = (x + game->blue_offset);
            uint8 green = (y + game->green_offset);
            *pixel++ = RGBA(0, green, blue, 255);
        }
        row += game->display_width * sizeof(uint32);
    }
}

void game_update_and_render(Game* game) {
    game->blue_offset += 1;
    game->green_offset += 2;

    static uint32 COLOR_BLACK = RGBA(0, 0, 0, 255);

    for (size_t i = 0; i < game->display_width*game->display_height; ++i) {
        game->display[i] = COLOR_BLACK;
    }

    render_weird_gradient(game);

    arena_reset(&game->transient_arena);
};

void game_key_up([[maybe_unused]] int key) {
    TODO("Implement this");
};

void game_key_down([[maybe_unused]] int key) {
    TODO("Implement this");
};

void game_generate_audio(Game* game) {
    // Clear audio buffer
    memset(game->audio, 0, AUDIO_CAPACITY * sizeof(int16));
    
    usize frames_needed = AUDIO_CAPACITY / game->audio_channels;
    
    // Mix all active audio sources
    for (usize source_idx = 0; source_idx < game->audio_sources_capacity; source_idx++) {
        AudioSource* source = &game->audio_sources[source_idx];
        
        if (!source->is_playing) continue;
        
        if (source->type == AUDIO_SOURCE_STATIC) {
            // Handle static audio (same as before)
            for (usize frame = 0; frame < frames_needed; frame++) {
                if (source->static_data.current_position >= source->static_data.frame_count) {
                    if (source->loop) {
                        source->static_data.current_position = 0;
                    } else {
                        source->is_playing = false;
                        break;
                    }
                }
                
                for (usize ch = 0; ch < game->audio_channels; ch++) {
                    usize src_idx = source->static_data.current_position * source->channels + ch;
                    usize dst_idx = frame * game->audio_channels + ch;
                    
                    int mixed = (int)game->audio[dst_idx] + 
                               (int)(source->static_data.samples[src_idx] * source->volume);
                    
                    game->audio[dst_idx] = (int16)CLAMP(mixed, -32768, 32767);
                }
                
                source->static_data.current_position++;
            }
            
        } else if (source->type == AUDIO_SOURCE_STREAMING) {
            // Handle streaming audio
            usize frames_processed = 0;
            
            while (frames_processed < frames_needed && source->is_playing) {
                // Check if we need to refill the stream buffer
                if (source->stream_data.buffer_position >= source->stream_data.buffer_valid) {
                    if (!refill_stream_buffer(source)) {
                        // End of stream and no loop, stop playing
                        source->is_playing = false;
                        break;
                    }
                }
                
                // Process available frames from stream buffer
                usize frames_available = source->stream_data.buffer_valid - source->stream_data.buffer_position;
                usize frames_to_process = (frames_needed - frames_processed < frames_available) 
                                        ? frames_needed - frames_processed 
                                        : frames_available;
                
                for (usize frame = 0; frame < frames_to_process; frame++) {
                    usize stream_frame_idx = source->stream_data.buffer_position + frame;
                    usize output_frame_idx = frames_processed + frame;
                    
                    // Handle channel and sample rate conversion if needed
                    if (source->sample_rate == (int)game->audio_sample_rate && 
                        source->channels == (int)game->audio_channels) {
                        // Direct copy (most common case)
                        for (usize ch = 0; ch < game->audio_channels; ch++) {
                            usize src_idx = stream_frame_idx * source->channels + ch;
                            usize dst_idx = output_frame_idx * game->audio_channels + ch;
                            
                            int mixed = (int)game->audio[dst_idx] + 
                                       (int)(source->stream_data.stream_buffer[src_idx] * source->volume);
                            
                            game->audio[dst_idx] = (int16)CLAMP(mixed, -32768, 32767);
                        }
                    } else {
                        // Need format conversion (implement as needed)
                        // For now, just handle simple cases
                        if (source->channels == 1 && game->audio_channels == 2) {
                            // Mono to stereo
                            int16 mono_sample = (int16)(source->stream_data.stream_buffer[stream_frame_idx] * source->volume);
                            usize dst_idx = output_frame_idx * 2;
                            
                            int mixed_l = (int)game->audio[dst_idx + 0] + mono_sample;
                            int mixed_r = (int)game->audio[dst_idx + 1] + mono_sample;
                            
                            game->audio[dst_idx + 0] = (int16)CLAMP(mixed_l, -32768, 32767);
                            game->audio[dst_idx + 1] = (int16)CLAMP(mixed_r, -32768, 32767);
                        }
                        // Add other conversions as needed
                    }
                }
                
                source->stream_data.buffer_position += frames_to_process;
                frames_processed += frames_to_process;
            }
        }
    }
}

void game_play_audio_source(AudioSource* source) {
    if (!source) return;
    
    source->is_playing = true;
    
    if (source->type == AUDIO_SOURCE_STATIC) {
        source->static_data.current_position = 0;
    } else if (source->type == AUDIO_SOURCE_STREAMING) {
        start_stream_source(source);
        source->stream_data.buffer_position = 0;
        source->stream_data.buffer_valid = 0;
        source->stream_data.end_of_file = false;
    }
}

void game_stop_audio_source(AudioSource* source) {
    if (source) {
        source->is_playing = false;
    }
}

void game_set_audio_volume(AudioSource* source, float volume) {
    if (source) {
        source->volume = CLAMP(volume, 0.0f, 1.0f);
    }
}

void game_free_audio_source(AudioSource* source) {
    if (!source) return;
    
    source->is_playing = false;
    
    if (source->type == AUDIO_SOURCE_STATIC) {
        // Note: Arena-allocated memory doesn't need explicit freeing
        source->static_data.samples = NULL;
    } else if (source->type == AUDIO_SOURCE_STREAMING) {
        if (source->stream_data.vorbis) {
            close_stream_source(source);
        }
        // Note: Arena-allocated memory doesn't need explicit freeing
        source->stream_data.filename = NULL;
        source->stream_data.stream_buffer = NULL;
    }
    
    memset(source, 0, sizeof(AudioSource));
}

void game_cleanup(Game* game) {
    debug_print("Cleaning up game resources...\n");
    
    for (usize i = 0; i < game->audio_sources_capacity; i++) {
        game_free_audio_source(&game->audio_sources[i]);
    }
    
    arena_reset(&game->permanent_arena);
    arena_reset(&game->transient_arena);
    
    game->audio_sources_size = 0;
    
    debug_print("Game cleanup completed\n");
    print_arena_stats(game);
}
