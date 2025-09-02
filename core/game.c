#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "ogg.h"
#include "utah-teapot.h"
#include "utils.h"
#include "vector.c"

#define OLIVEC_IMPLEMENTATION
#include "olive.c"

#define MAX_AUDIO_SOURCES 16
#define STREAM_BUFFER_FRAMES 4096

#define PERMANENT_ARENA_SIZE (64 * 1024 * 1024)
#define TRANSIENT_ARENA_SIZE (128 * 1024 * 1024)

static struct {
    u32 display[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    f32 zbuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    i16 audio[AUDIO_CAPACITY];
    AudioSource audio_sources[MAX_AUDIO_SOURCES];
    u8 permanent_memory[PERMANENT_ARENA_SIZE];
    u8 transient_memory[TRANSIENT_ARENA_SIZE];
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
        .display_zbuffer         = global.zbuffer,
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
               (float)arena_get_used(&game->permanent_arena) / game->permanent_arena.size * 100.0f,
               arena_get_remaining(&game->permanent_arena) / 1024.0f);
    debug_print("  Transient: %.1f/%.1f KB used (%.1f%%, %.1f KB remaining)\n",
               arena_get_used(&game->transient_arena) / 1024.0f,
               game->transient_arena.size / 1024.0f,
               (float)arena_get_used(&game->transient_arena) / game->transient_arena.size * 100.0f,
               arena_get_remaining(&game->transient_arena) / 1024.0f);
}

internal void draw_weird_triangle(Game* game) {
    static float angle = 0.0f;
    static float cx = DISPLAY_WIDTH / 2.0f;
    static float cy = DISPLAY_HEIGHT / 2.0f;
    static float dx = 800.0;
    static float dy = 800.0;
    usize vert_count = 3;
    float dangle = 2 * PI / vert_count;
    float mag = 100;

    Olivec_Canvas oc = {
        .pixels = game->display,
        .width  = game->display_width,
        .height = game->display_height,
        .stride = game->display_width,
    };

    olivec_triangle3c(
        oc,
        cx + cosf(dangle * 0 + angle) * mag, cy + sinf(dangle * 0 + angle) * mag,
        cx + cosf(dangle * 1 + angle) * mag, cy + sinf(dangle * 1 + angle) * mag,
        cx + cosf(dangle * 2 + angle) * mag, cy + sinf(dangle * 2 + angle) * mag,
        RGBA(255, 0, 0, 255),
        RGBA(0, 255, 0, 255),
        RGBA(0, 0, 255, 255)
    );
    angle += 2 * PI * DELTA_TIME;
    float nx = cx + dx * DELTA_TIME;
    if(nx - mag <= 0 || nx + mag >= game->display_width) {
        dx *= -1.0;
        game_play_audio_source(&game->audio_sources[1]);
    } else {
        cx = nx;
    }
    float ny = cy + dy * DELTA_TIME;
    if (ny - mag <= 0 || ny + mag >= game->display_height) {
        dy *= -1.0;
        game_play_audio_source(&game->audio_sources[1]);
    } else {
        cy = ny;
    }
}

float angle = 0;

void game_update_and_render(Game* game) {
    angle += 0.25 * PI * DELTA_TIME;

    Olivec_Canvas oc = olivec_canvas(
        game->display,
        game->display_width,
        game->display_height,
        game->display_width
    );
    olivec_fill(oc, 0xFFAAFFFF);
    for (size_t i = 0; i < game->display_width*game->display_height; ++i) {
        game->display_zbuffer[i] = 0;
    }

    Vec3 camera = vec3(0, 0, 1);
    for (size_t i = 0; i < faces_count; ++i) {
        int a, b, c;

        a = faces[i][FACE_V1];
        b = faces[i][FACE_V2];
        c = faces[i][FACE_V3];
        Vec3 v1 = vec3_rotate_y(vec3(vertices[a][0], vertices[a][1], vertices[a][2]), angle);
        Vec3 v2 = vec3_rotate_y(vec3(vertices[b][0], vertices[b][1], vertices[b][2]), angle);
        Vec3 v3 = vec3_rotate_y(vec3(vertices[c][0], vertices[c][1], vertices[c][2]), angle);
        v1.z += 2.5; v2.z += 2.5; v3.z += 2.5;

        a = faces[i][FACE_VN1];
        b = faces[i][FACE_VN2];
        c = faces[i][FACE_VN3];
        Vec3 vn1 = vec3_rotate_y(vec3(normals[a][0], normals[a][1], normals[a][2]), angle);
        Vec3 vn2 = vec3_rotate_y(vec3(normals[b][0], normals[b][1], normals[b][2]), angle);
        Vec3 vn3 = vec3_rotate_y(vec3(normals[c][0], normals[c][1], normals[c][2]), angle);
        if (vec3_dot(camera, vn1) > 0.0 &&
            vec3_dot(camera, vn2) > 0.0 &&
            vec3_dot(camera, vn3) > 0.0) continue;

        Vec2 p1 = project_2d_scr(project_3d_2d(v1), game->display_width, game->display_height);
        Vec2 p2 = project_2d_scr(project_3d_2d(v2), game->display_width, game->display_height);
        Vec2 p3 = project_2d_scr(project_3d_2d(v3), game->display_width, game->display_height);

        int x1 = p1.x;
        int x2 = p2.x;
        int x3 = p3.x;
        int y1 = p1.y;
        int y2 = p2.y;
        int y3 = p3.y;
        int lx, hx, ly, hy;
        if (olivec_normalize_triangle(oc.width, oc.height, x1, y1, x2, y2, x3, y3, &lx, &hx, &ly, &hy)) {
            for (int y = ly; y <= hy; ++y) {
                for (int x = lx; x <= hx; ++x) {
                    int u1, u2, det;
                    if (olivec_barycentric(x1, y1, x2, y2, x3, y3, x, y, &u1, &u2, &det)) {
                        int u3 = det - u1 - u2;
                        float z = 1/v1.z*u1/det + 1/v2.z*u2/det + 1/v3.z*u3/det;
                        float near_ = 0.1f;
                        float far_ = 5.0f;
                        if (1.0f/far_ < z && z < 1.0f/near_ && z > game->display_zbuffer[y*game->display_width + x]) {
                            game->display_zbuffer[y*game->display_width + x] = z;
                            OLIVEC_PIXEL(oc, x, y) = 0xFFFF5050;

                            z = 1.0f/z;
                            if (z >= 1.0) {
                                z -= 1.0;
                                uint32_t v = z*255;
                                if (v > 255) v = 255;
                                olivec_blend_color(&OLIVEC_PIXEL(oc, x, y), (v<<(3*8)));
                            }
                        }
                    }
                }
            }
        }
    }

    const int buffer_size = 64;
    char buffer[buffer_size];
    snprintf(buffer, buffer_size, "fps %.1f", game->current_fps);
    olivec_text(oc, buffer, 0, 0, olivec_default_font, 3, RGBA(0, 0, 0, 255));

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
    memset(game->audio, 0, AUDIO_CAPACITY * sizeof(i16));
    
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
                    
                    game->audio[dst_idx] = (i16)CLAMP(mixed, -32768, 32767);
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
                            
                            game->audio[dst_idx] = (i16)CLAMP(mixed, -32768, 32767);
                        }
                    } else {
                        // Need format conversion (implement as needed)
                        // For now, just handle simple cases
                        if (source->channels == 1 && game->audio_channels == 2) {
                            // Mono to stereo
                            i16 mono_sample = (i16)(source->stream_data.stream_buffer[stream_frame_idx] * source->volume);
                            usize dst_idx = output_frame_idx * 2;
                            
                            int mixed_l = (int)game->audio[dst_idx + 0] + mono_sample;
                            int mixed_r = (int)game->audio[dst_idx + 1] + mono_sample;
                            
                            game->audio[dst_idx + 0] = (i16)CLAMP(mixed_l, -32768, 32767);
                            game->audio[dst_idx + 1] = (i16)CLAMP(mixed_r, -32768, 32767);
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
        source->static_data.samples = nullptr;
    } else if (source->type == AUDIO_SOURCE_STREAMING) {
        if (source->stream_data.vorbis) {
            close_stream_source(source);
        }
        // Note: Arena-allocated memory doesn't need explicit freeing
        source->stream_data.filename = nullptr;
        source->stream_data.stream_buffer = nullptr;
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
