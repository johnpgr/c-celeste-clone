#define _CRT_SECURE_NO_WARNINGS
#include <math.h>
#include <string.h>
#include "def.h"
#include "game.h"

#define OLIVEC_IMPLEMENTATION
#include "olive.c"
#include "stb_vorbis.c"
#define MAX_AUDIO_SOURCES 16
#define STREAM_BUFFER_FRAMES 4096

static struct {
    u32 display[WIDTH * HEIGHT];
    i16 audio[AUDIO_CAPACITY];
    AudioSource audio_sources[MAX_AUDIO_SOURCES];
} global;

Game game_init(void) {
    return (Game) {
        .title              = (char*)TITLE,
        .fps                = FPS,
        .display            = global.display,
        .display_width      = WIDTH,
        .display_height     = HEIGHT,
        .audio              = global.audio,
        .audio_capacity     = AUDIO_CAPACITY,
        .audio_sample_rate  = AUDIO_SAMPLE_RATE,
        .audio_channels     = AUDIO_CHANNELS,
        .loaded_audio       = global.audio_sources,
        .max_audio_sources  = MAX_AUDIO_SOURCES,
        .active_audio_count = 0,
    };
};

void game_update_and_render(Game* game) {
    // clear the pixels
    for (usize i = 0; i < ARRAY_LEN(global.display); i++) {
        global.display[i] = RGBA(0, 0, 0, 255);
    }

    static float angle = 0.0f;
    usize vert_count = 3;
    float cx = WIDTH / 2.0f;
    float cy = HEIGHT / 2.0f;
    float dangle = 2 * PI / vert_count;
    float mag = cx / 2;

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
};

void game_key_up([[maybe_unused]] int key) {
    TODO("Implement this");
};

void game_key_down([[maybe_unused]] int key) {
    TODO("Implement this");
};


// Fixed resampling function with proper ratio calculation
internal i16* resample_audio(i16* input_samples, usize input_frames, int input_channels, 
                          int input_rate, int target_rate, usize* output_frames) {
    
    debug_print("Resampling: %d Hz -> %d Hz, %d channels, %zu frames\n", 
        input_rate, target_rate, input_channels, input_frames);
    
    if (input_rate == target_rate) {
        *output_frames = input_frames;
        usize total_samples = input_frames * input_channels;
        i16* output = malloc(total_samples * sizeof(i16));
        memcpy(output, input_samples, total_samples * sizeof(i16));
        return output;
    }
    
    // FIXED: Correct ratio calculation
    // If input is 22kHz and target is 48kHz, ratio should be 48/22 = 2.18 (upsample)
    // If input is 48kHz and target is 22kHz, ratio should be 22/48 = 0.46 (downsample)
    double ratio = (double)target_rate / (double)input_rate;
    *output_frames = (usize)(input_frames * ratio + 0.5); // Add 0.5 for rounding
    
    usize output_samples = *output_frames * input_channels;
    i16* output = malloc(output_samples * sizeof(i16));
    
    debug_print("Resampling ratio: %.3f, output frames: %zu\n", ratio, *output_frames);
    
    for (usize i = 0; i < *output_frames; i++) {
        // FIXED: Correct source index calculation
        // For upsampling (ratio > 1): source advances slower than output
        // For downsampling (ratio < 1): source advances faster than output
        double source_index = (double)i / ratio;
        usize index1 = (usize)source_index;
        usize index2 = index1 + 1;
        
        // Boundary check
        if (index1 >= input_frames) index1 = input_frames - 1;
        if (index2 >= input_frames) index2 = input_frames - 1;
        
        double fraction = source_index - (double)index1;
        
        for (int ch = 0; ch < input_channels; ch++) {
            i16 sample1 = input_samples[index1 * input_channels + ch];
            i16 sample2 = input_samples[index2 * input_channels + ch];
            
            // Linear interpolation
            double interpolated_d = (double)sample1 + fraction * ((double)sample2 - (double)sample1);
            i16 interpolated = (i16)CLAMP(interpolated_d, -32768.0, 32767.0);
            
            output[i * input_channels + ch] = interpolated;
        }
    }
    
    return output;
}

// Channel conversion helper
internal void convert_channels(i16* input, int input_channels, i16* output, int output_channels, usize frames) {
    for (usize frame = 0; frame < frames; frame++) {
        if (input_channels == 1 && output_channels == 2) {
            // Mono to stereo
            i16 mono = input[frame];
            output[frame * 2 + 0] = mono;
            output[frame * 2 + 1] = mono;
        } else if (input_channels == 2 && output_channels == 1) {
            // Stereo to mono
            i16 left = input[frame * 2 + 0];
            i16 right = input[frame * 2 + 1];
            output[frame] = (i16)((left + right) / 2);
        } else {
            // Direct copy or take first available channels
            for (int ch = 0; ch < output_channels; ch++) {
                int src_ch = (ch < input_channels) ? ch : 0;
                output[frame * output_channels + ch] = input[frame * input_channels + src_ch];
            }
        }
    }
}

AudioSource* game_load_ogg_static(Game* game, const char* filename, bool loop) {
    if (game->active_audio_count >= game->max_audio_sources) {
        debug_print("Error: Maximum audio sources reached\n");
        return NULL;
    }
    
    int error = 0;
    stb_vorbis* vorbis = stb_vorbis_open_filename(filename, &error, NULL);
    if (!vorbis) {
        debug_print("Error: Could not open OGG file '%s' (error: %d)\n", filename, error);
        return NULL;
    }
    
    stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    usize total_frames = stb_vorbis_stream_length_in_samples(vorbis);
    
    debug_print("Loading static OGG: %s\n", filename);
    debug_print("  Original: %d Hz, %d channels, %zu frames\n", info.sample_rate, info.channels, total_frames);
    debug_print("  Target: %zu Hz, %zu channels\n", game->audio_sample_rate, game->audio_channels);
    
    // Decode entire file
    usize total_input_samples = total_frames * info.channels;
    i16* raw_samples = malloc(total_input_samples * sizeof(i16));
    
    int decoded_frames = stb_vorbis_get_samples_short_interleaved(
        vorbis, info.channels, raw_samples, (int)total_input_samples);
    
    stb_vorbis_close(vorbis);
    
    if (decoded_frames <= 0) {
        debug_print("Error: Failed to decode OGG file\n");
        free(raw_samples);
        return NULL;
    }
    
    debug_print("  Decoded %d frames successfully\n", decoded_frames);
    
    // Step 1: Handle sample rate conversion
    i16* resampled_audio = NULL;
    usize resampled_frames = 0;
    
    if (info.sample_rate != (usize)game->audio_sample_rate) {
        resampled_audio = resample_audio(raw_samples, (usize)decoded_frames,
                                       info.channels, info.sample_rate, 
                                       (int)game->audio_sample_rate, &resampled_frames);
        free(raw_samples);
        debug_print("  After resampling: %zu frames\n", resampled_frames);
    } else {
        resampled_audio = raw_samples;
        resampled_frames = (usize)decoded_frames;
        debug_print("  No resampling needed\n");
    }
    
    // Step 2: Handle channel conversion
    i16* final_audio = NULL;
    usize final_frames = resampled_frames;
    
    if (info.channels != (int)game->audio_channels) {
        debug_print("  Converting channels: %d -> %zu\n", info.channels, game->audio_channels);
        
        usize final_samples = final_frames * game->audio_channels;
        final_audio = malloc(final_samples * sizeof(i16));
        
        convert_channels(resampled_audio, info.channels, 
                        final_audio, (int)game->audio_channels, final_frames);
        
        free(resampled_audio);
        debug_print("  After channel conversion: %zu samples\n", final_samples);
    } else {
        final_audio = resampled_audio;
        debug_print("  No channel conversion needed\n");
    }
    
    // Find empty slot
    AudioSource* source = NULL;
    for (usize i = 0; i < game->max_audio_sources; i++) {
        if (game->loaded_audio[i].type == 0) {
            source = &game->loaded_audio[i];
            break;
        }
    }
    
    if (!source) {
        debug_print("Error: No available audio source slots\n");
        free(final_audio);
        return NULL;
    }
    
    // Initialize static audio source
    memset(source, 0, sizeof(AudioSource));
    source->type = AUDIO_SOURCE_STATIC;
    source->channels = (int)game->audio_channels;
    source->sample_rate = (int)game->audio_sample_rate;
    source->is_playing = false;
    source->loop = loop;
    source->volume = 1.0f;
    
    source->static_data.samples = final_audio;
    source->static_data.sample_count = final_frames * game->audio_channels;
    source->static_data.frame_count = final_frames;
    source->static_data.current_position = 0;
    
    game->active_audio_count++;
    
    debug_print("Successfully loaded static audio: %zu frames, %d channels, %d Hz\n", 
        source->static_data.frame_count, source->channels, source->sample_rate);
    
    return source;
}

AudioSource* game_load_ogg_static_from_memory(Game* game, const u8* data, usize data_size, bool loop) {
    if (game->active_audio_count >= game->max_audio_sources) {
        debug_print("Error: Maximum audio sources reached\n");
        return NULL;
    }

    int error = 0;
    stb_vorbis* vorbis = stb_vorbis_open_memory((const unsigned char*)data, (int)data_size, &error, NULL);
    if (!vorbis) {
        debug_print("Error: Could not open OGG data in memory (error: %d)\n", error);
        return NULL;
    }

    stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    usize total_frames = stb_vorbis_stream_length_in_samples(vorbis);

    debug_print("Loading static OGG from memory\n");
    debug_print("  Original: %d Hz, %d channels, %zu frames\n", info.sample_rate, info.channels, total_frames);
    debug_print("  Target: %zu Hz, %zu channels\n", game->audio_sample_rate, game->audio_channels);

    // Decode entire file
    usize total_input_samples = total_frames * info.channels;
    i16* raw_samples = malloc(total_input_samples * sizeof(i16));
    if (!raw_samples) {
        stb_vorbis_close(vorbis);
        debug_print("Error: Out of memory allocating raw samples\n");
        return NULL;
    }

    int decoded_frames = stb_vorbis_get_samples_short_interleaved(
        vorbis, info.channels, raw_samples, (int)total_input_samples);

    stb_vorbis_close(vorbis);

    if (decoded_frames <= 0) {
        debug_print("Error: Failed to decode OGG data in memory\n");
        free(raw_samples);
        return NULL;
    }

    debug_print("  Decoded %d frames successfully\n", decoded_frames);

    // Step 1: Handle sample rate conversion
    i16* resampled_audio = NULL;
    usize resampled_frames = 0;

    if (info.sample_rate != (usize)game->audio_sample_rate) {
        resampled_audio = resample_audio(raw_samples, (usize)decoded_frames,
                                       info.channels, info.sample_rate,
                                       (int)game->audio_sample_rate, &resampled_frames);
        free(raw_samples);
        debug_print("  After resampling: %zu frames\n", resampled_frames);
    } else {
        resampled_audio = raw_samples;
        resampled_frames = (usize)decoded_frames;
        debug_print("  No resampling needed\n");
    }

    // Step 2: Handle channel conversion
    i16* final_audio = NULL;
    usize final_frames = resampled_frames;

    if (info.channels != (int)game->audio_channels) {
        debug_print("  Converting channels: %d -> %zu\n", info.channels, game->audio_channels);

        usize final_samples = final_frames * game->audio_channels;
        final_audio = malloc(final_samples * sizeof(i16));
        if (!final_audio) {
            free(resampled_audio);
            debug_print("Error: Out of memory allocating final audio\n");
            return NULL;
        }

        convert_channels(resampled_audio, info.channels,
                        final_audio, (int)game->audio_channels, final_frames);

        free(resampled_audio);
        debug_print("  After channel conversion: %zu samples\n", final_samples);
    } else {
        final_audio = resampled_audio;
        debug_print("  No channel conversion needed\n");
    }

    // Find empty slot
    AudioSource* source = NULL;
    for (usize i = 0; i < game->max_audio_sources; i++) {
        if (game->loaded_audio[i].type == 0) {
            source = &game->loaded_audio[i];
            break;
        }
    }

    if (!source) {
        debug_print("Error: No available audio source slots\n");
        free(final_audio);
        return NULL;
    }

    // Initialize static audio source
    memset(source, 0, sizeof(AudioSource));
    source->type = AUDIO_SOURCE_STATIC;
    source->channels = (int)game->audio_channels;
    source->sample_rate = (int)game->audio_sample_rate;
    source->is_playing = false;
    source->loop = loop;
    source->volume = 1.0f;

    source->static_data.samples = final_audio;
    source->static_data.sample_count = final_frames * game->audio_channels;
    source->static_data.frame_count = final_frames;
    source->static_data.current_position = 0;

    game->active_audio_count++;

    debug_print("Successfully loaded static audio from memory: %zu frames, %d channels, %d Hz\n",
        source->static_data.frame_count, source->channels, source->sample_rate);

    return source;
}

AudioSource* game_load_ogg_streaming(Game* game, const char* filename, bool loop) {
    if (game->active_audio_count >= game->max_audio_sources) {
        debug_print("Error: Maximum audio sources reached\n");
        return NULL;
    }
    
    int error = 0;
    stb_vorbis* vorbis = stb_vorbis_open_filename(filename, &error, NULL);
    if (!vorbis) {
        debug_print("Error: Could not open OGG file '%s' for streaming\n", filename);
        return NULL;
    }
    
    stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    
    debug_print("Loading streaming OGG: %s (%d Hz, %d channels)\n", filename, info.sample_rate, info.channels);
    
    // Find empty slot
    AudioSource* source = NULL;
    for (usize i = 0; i < game->max_audio_sources; i++) {
        if (game->loaded_audio[i].type == 0 && !game->loaded_audio[i].stream_data.vorbis) {
            source = &game->loaded_audio[i];
            break;
        }
    }
    
    if (!source) {
        stb_vorbis_close(vorbis);
        return NULL;
    }
    
    // Initialize streaming source
    source->type = AUDIO_SOURCE_STREAMING;
    source->channels = info.channels;
    source->sample_rate = info.sample_rate;
    source->is_playing = false;
    source->loop = loop;
    source->volume = 1.0f;
    
    source->stream_data.vorbis = vorbis;
    source->stream_data.filename = malloc(strlen(filename) + 1);
    strcpy(source->stream_data.filename, filename);
    source->stream_data.buffer_frames = STREAM_BUFFER_FRAMES;
    source->stream_data.stream_buffer = malloc(STREAM_BUFFER_FRAMES * info.channels * sizeof(i16));
    source->stream_data.buffer_position = 0;
    source->stream_data.buffer_valid = 0;
    source->stream_data.end_of_file = false;
    
    game->active_audio_count++;
    return source;
}

// Fill the stream buffer when needed
internal bool refill_stream_buffer(AudioSource* source) {
    if (source->type != AUDIO_SOURCE_STREAMING || !source->stream_data.vorbis) {
        return false;
    }
    
    // Read more data from the OGG file
    int samples_read = stb_vorbis_get_samples_short_interleaved(
        source->stream_data.vorbis,
        source->channels,
        source->stream_data.stream_buffer,
        (int)(source->stream_data.buffer_frames * source->channels)
    );
    
    if (samples_read > 0) {
        source->stream_data.buffer_valid = (usize)samples_read;
        source->stream_data.buffer_position = 0;
        source->stream_data.end_of_file = false;
        return true;
    } else {
        // End of file reached
        source->stream_data.end_of_file = true;
        
        if (source->loop) {
            // Restart the file
            stb_vorbis_seek_start(source->stream_data.vorbis);
            return refill_stream_buffer(source); // Try again
        }
        
        return false;
    }
}

void game_generate_audio(Game* game) {
    // Clear audio buffer
    memset(game->audio, 0, AUDIO_CAPACITY * sizeof(i16));
    
    usize frames_needed = AUDIO_CAPACITY / game->audio_channels;
    
    // Mix all active audio sources
    for (usize source_idx = 0; source_idx < game->max_audio_sources; source_idx++) {
        AudioSource* source = &game->loaded_audio[source_idx];
        
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
        stb_vorbis_seek_start(source->stream_data.vorbis);
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
        free(source->static_data.samples);
    } else if (source->type == AUDIO_SOURCE_STREAMING) {
        if (source->stream_data.vorbis) {
            stb_vorbis_close(source->stream_data.vorbis);
        }
        free(source->stream_data.filename);
        free(source->stream_data.stream_buffer);
    }
    
    memset(source, 0, sizeof(AudioSource));
}
