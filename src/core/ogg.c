#define _CRT_SECURE_NO_WARNINGS
#include "ogg.h"
#include "game.h"
#include "stb_vorbis.c"

void start_stream_source(AudioSource* source) {
    assert(source->type == AUDIO_SOURCE_STREAMING && "tried to start stream source of a non-streaming audio source");
    stb_vorbis_seek_start(source->stream_data.vorbis);
}

void close_stream_source(AudioSource* source) {
    assert(source->type == AUDIO_SOURCE_STREAMING && "tried to close stream source of a non-streaming audio source");
    stb_vorbis_close(source->stream_data.vorbis);
}

// Fill the stream buffer when needed
bool refill_stream_buffer(AudioSource* source) {
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

inline i16* resample_audio(
    Arena* arena,
    i16* input_samples,
    usize input_frames,
    int input_channels,
    int input_rate,
    int target_rate,
    usize* output_frames
) {
    assert(arena != nullptr);

    debug_print("Resampling: %d Hz -> %d Hz, %d channels, %zu frames\n", 
        input_rate, target_rate, input_channels, input_frames);
    
    if (input_rate == target_rate) {
        *output_frames = input_frames;
        usize total_samples = input_frames * input_channels;
        i16* output = arena_alloc(arena, total_samples * sizeof(i16));
        memcpy(output, input_samples, total_samples * sizeof(i16));
        return output;
    }
    
    // If input is 22kHz and target is 48kHz, ratio should be 48/22 = 2.18 (upsample)
    // If input is 48kHz and target is 22kHz, ratio should be 22/48 = 0.46 (downsample)
    double ratio = (double)target_rate / (double)input_rate;
    *output_frames = (usize)(input_frames * ratio + 0.5); // Add 0.5 for rounding
    
    usize output_samples = *output_frames * input_channels;
    i16* output = arena_alloc(arena, output_samples * sizeof(i16));
    if (!output) {
        debug_print("Error: Transient arena out of memory for raw samples\n");
        return nullptr;
    }
    
    debug_print("Resampling ratio: %.3f, output frames: %zu\n", ratio, *output_frames);
    
    for (usize i = 0; i < *output_frames; i++) {
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
inline void convert_channels(
    i16* input,
    int input_channels,
    i16* output,
    int output_channels,
    usize frames
) {
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

AudioSource* load_ogg_static(
    Game* game,
    const char* filename,
    bool loop
) {
    if (game->active_audio_count >= game->max_audio_sources) {
        debug_print("Error: Maximum audio sources reached\n");
        return nullptr;
    }
    
    int error = 0;
    stb_vorbis* vorbis = stb_vorbis_open_filename(filename, &error, nullptr);
    if (!vorbis) {
        debug_print("Error: Could not open OGG file '%s' (error: %d)\n", filename, error);
        return nullptr;
    }
    
    stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    usize total_frames = stb_vorbis_stream_length_in_samples(vorbis);
    
    debug_print("Loading static OGG: %s\n", filename);
    debug_print("  Original: %d Hz, %d channels, %zu frames\n", info.sample_rate, info.channels, total_frames);
    debug_print("  Target: %zu Hz, %zu channels\n", game->audio_sample_rate, game->audio_channels);
    
    // Decode entire file into transient memory first
    usize total_input_samples = total_frames * info.channels;
    i16* raw_samples = arena_alloc(&game->transient_arena, total_input_samples * sizeof(i16));
    if (!raw_samples) {
        debug_print("Error: Transient arena out of memory for raw samples\n");
        stb_vorbis_close(vorbis);
        return nullptr;
    }
    
    int decoded_frames = stb_vorbis_get_samples_short_interleaved(
        vorbis, info.channels, raw_samples, (int)total_input_samples);
    
    if (decoded_frames <= 0) {
        debug_print("Error: Failed to decode OGG file\n");
        stb_vorbis_close(vorbis);
        // Note: raw_samples will be reclaimed on arena reset
        return nullptr;
    }
    
    debug_print("  Decoded %d frames successfully\n", decoded_frames);
    
    // Step 1: Handle sample rate conversion
    i16* resampled_audio = nullptr;
    usize resampled_frames = 0;
    
    if (info.sample_rate != (usize)game->audio_sample_rate) {
        resampled_audio = resample_audio(
            &game->transient_arena,
            raw_samples,
            (usize)decoded_frames,
            info.channels, info.sample_rate,
            (int)game->audio_sample_rate, &resampled_frames
        );
        // Note: raw_samples will be reclaimed on arena reset
        debug_print("  After resampling: %zu frames\n", resampled_frames);
    } else {
        resampled_audio = raw_samples;
        resampled_frames = (usize)decoded_frames;
        debug_print("  No resampling needed\n");
    }
    
    // Step 2: Handle channel conversion
    i16* final_audio = nullptr;
    usize final_frames = resampled_frames;
    
    if (info.channels != (int)game->audio_channels) {
        debug_print("  Converting channels: %d -> %zu\n", info.channels, game->audio_channels);
        
        usize final_samples = final_frames * game->audio_channels;
        final_audio = arena_alloc(&game->transient_arena, final_samples * sizeof(i16));
        if (!final_audio) {
            debug_print("Error: Arena out of memory for final audio\n");
            stb_vorbis_close(vorbis);
            return nullptr;
        }
        
        convert_channels(
            resampled_audio,
            info.channels,
            final_audio,
            (int)game->audio_channels,
            final_frames
        );

        // Note: resampled_audio will be reclaimed on arena reset
        debug_print("  After channel conversion: %zu samples\n", final_samples);
    } else {
        final_audio = resampled_audio;
        debug_print("  No channel conversion needed\n");
    }

    // Now copy final audio data to permanent storage
    usize permanent_sample_count = final_frames * game->audio_channels;
    i16* permanent_samples = arena_alloc(&game->permanent_arena, permanent_sample_count * sizeof(i16));
    if (!permanent_samples) {
        debug_print("Error: Permanent arena out of memory for audio data\n");
        stb_vorbis_close(vorbis);
        return nullptr;
    }
    memcpy(permanent_samples, final_audio, permanent_sample_count * sizeof(i16));
    
    // Find empty slot
    AudioSource* source = nullptr;
    for (usize i = 0; i < game->max_audio_sources; i++) {
        if (game->loaded_audio[i].type == AUDIO_SOURCE_NONE) {
            source = &game->loaded_audio[i];
            break;
        }
    }
    
    if (!source) {
        // Note: final_audio will be reclaimed on arena reset
        debug_print("Error: No available audio source slots\n");
        stb_vorbis_close(vorbis);
        return nullptr;
    }
    
    // Initialize static audio source
    memset(source, 0, sizeof(AudioSource));
    source->type = AUDIO_SOURCE_STATIC;
    source->channels = (int)game->audio_channels;
    source->sample_rate = (int)game->audio_sample_rate;
    source->is_playing = false;
    source->loop = loop;
    source->volume = 1.0f;
    
    source->static_data.samples = permanent_samples;
    source->static_data.sample_count = final_frames * game->audio_channels;
    source->static_data.frame_count = final_frames;
    source->static_data.current_position = 0;
    
    game->active_audio_count++;
    
    debug_print("Successfully loaded static audio: %zu frames, %d channels, %d Hz\n", 
        source->static_data.frame_count, source->channels, source->sample_rate);

    // Clean up vorbis and reset transient arena to reclaim temporary processing memory
    stb_vorbis_close(vorbis);
    arena_reset(&game->transient_arena);
    
    return source;
}

AudioSource* load_ogg_static_from_memory(
    Game* game,
    const u8* data,
    usize data_size,
    bool loop
) {
    if (game->active_audio_count >= game->max_audio_sources) {
        debug_print("Error: Maximum audio sources reached\n");
        return nullptr;
    }

    int error = 0;
    stb_vorbis* vorbis = stb_vorbis_open_memory((const unsigned char*)data, (int)data_size, &error, nullptr);
    if (!vorbis) {
        debug_print("Error: Could not open OGG data in memory (error: %d)\n", error);
        return nullptr;
    }

    stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    usize total_frames = stb_vorbis_stream_length_in_samples(vorbis);

    debug_print("Loading static OGG from memory\n");
    debug_print("  Original: %d Hz, %d channels, %zu frames\n", info.sample_rate, info.channels, total_frames);
    debug_print("  Target: %zu Hz, %zu channels\n", game->audio_sample_rate, game->audio_channels);
    debug_print("  Transient arena before loading: %.1f/%.1f KB used\n", 
               arena_get_used(&game->transient_arena) / 1024.0f, game->transient_arena.size / 1024.0f);

    // Decode entire file
    usize total_input_samples = total_frames * info.channels;
    i16* raw_samples = arena_alloc(
        &game->transient_arena,
        total_input_samples * sizeof(i16)
    );
    if (!raw_samples) {
        debug_print("Error: Out of memory allocating raw samples\n");
        stb_vorbis_close(vorbis);
        return nullptr;
    }

    int decoded_frames = stb_vorbis_get_samples_short_interleaved(
        vorbis,
        info.channels,
        raw_samples,
        (int)total_input_samples
    );

    if (decoded_frames <= 0) {
        // Note: raw_samples will be reclaimed on arena reset
        debug_print("Error: Failed to decode OGG data in memory\n");
        stb_vorbis_close(vorbis);
        return nullptr;
    }

    debug_print("  Decoded %d frames successfully\n", decoded_frames);

    // Step 1: Handle sample rate conversion
    i16* resampled_audio = nullptr;
    usize resampled_frames = 0;

    if (info.sample_rate != (usize)game->audio_sample_rate) {
        resampled_audio = resample_audio(
            &game->transient_arena,
            raw_samples,
            (usize)decoded_frames,
            info.channels, info.sample_rate,
            (int)game->audio_sample_rate, &resampled_frames
        );
        if (!resampled_audio) {
            debug_print("Error: Resampling failed\n");
            stb_vorbis_close(vorbis);
            return nullptr;
        }
        // Note: raw_samples will be reclaimed on arena reset
        debug_print("  After resampling: %zu frames\n", resampled_frames);
    } else {
        resampled_audio = raw_samples;
        resampled_frames = (usize)decoded_frames;
        debug_print("  No resampling needed\n");
    }

    // Step 2: Handle channel conversion
    i16* final_audio = nullptr;
    usize final_frames = resampled_frames;

    if (info.channels != (int)game->audio_channels) {
        debug_print("  Converting channels: %d -> %zu\n", info.channels, game->audio_channels);

        usize final_samples = final_frames * game->audio_channels;
        final_audio = arena_alloc(
            &game->transient_arena,
            final_samples * sizeof(i16)
        );
        if (!final_audio) {
            debug_print("Error: Transient arena out of memory for final audio\n");
            stb_vorbis_close(vorbis);
            return nullptr;
        }

        convert_channels(
            resampled_audio,
            info.channels,
            final_audio,
            (int)game->audio_channels,
            final_frames
        );

        // Note: resampled_audio will be reclaimed on arena reset
        debug_print("  After channel conversion: %zu samples\n", final_samples);
    } else {
        final_audio = resampled_audio;
        debug_print("  No channel conversion needed\n");
    }

    if (!final_audio) {
        debug_print("Error: final_audio is null - this should not happen\n");
        stb_vorbis_close(vorbis);
        return nullptr;
    }

    AudioSource* source = nullptr;
    for (usize i = 0; i < game->max_audio_sources; i++) {
        debug_print("  Checking slot %zu: type=%d\n", i, game->loaded_audio[i].type);
        if (game->loaded_audio[i].type == AUDIO_SOURCE_NONE) {
            source = &game->loaded_audio[i];
            debug_print("  Using slot %zu\n", i);
            break;
        }
    }

    if (!source) {
        debug_print("Error: No available audio source slots\n");
        stb_vorbis_close(vorbis);
        return nullptr;
    }

    // Now copy final audio data to permanent storage
    usize permanent_sample_count = final_frames * game->audio_channels;
    i16* permanent_samples = arena_alloc(
        &game->permanent_arena,
        permanent_sample_count * sizeof(i16)
    );
    if (!permanent_samples) {
        debug_print("Error: Permanent arena out of memory for audio data\n");
        stb_vorbis_close(vorbis);
        return nullptr;
    }
    memcpy(permanent_samples, final_audio, permanent_sample_count * sizeof(i16));

    // Initialize static audio source - CRITICAL: Zero the entire structure first
    memset(source, 0, sizeof(AudioSource));
    source->type = AUDIO_SOURCE_STATIC;
    source->channels = (int)game->audio_channels;
    source->sample_rate = (int)game->audio_sample_rate;
    source->is_playing = false;
    source->loop = loop;
    source->volume = 1.0f;

    source->static_data.samples = permanent_samples;
    source->static_data.sample_count = final_frames * game->audio_channels;
    source->static_data.frame_count = final_frames;
    source->static_data.current_position = 0;

    game->active_audio_count++;

    debug_print("Successfully loaded static audio from memory: %zu frames, %d channels, %d Hz (slot %zu)\n",
        source->static_data.frame_count, source->channels, source->sample_rate, 
        (usize)(source - game->loaded_audio));

    // Clean up vorbis and reset transient arena to reclaim temporary processing memory
    stb_vorbis_close(vorbis);
    arena_reset(&game->transient_arena);

    return source;
}

AudioSource* load_ogg_streaming(
    Game* game,
    const char* filename,
    int stream_buffer_frames,
    bool loop
) {
    if (game->active_audio_count >= game->max_audio_sources) {
        debug_print("Error: Maximum audio sources reached\n");
        return nullptr;
    }
    
    int error = 0;
    stb_vorbis* vorbis = stb_vorbis_open_filename(filename, &error, nullptr);
    if (!vorbis) {
        debug_print("Error: Could not open OGG file '%s' for streaming\n", filename);
        return nullptr;
    }
    
    stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    
    debug_print("Loading streaming OGG: %s (%d Hz, %d channels)\n", filename, info.sample_rate, info.channels);
    
    // Find empty slot
    AudioSource* source = nullptr;
    for (usize i = 0; i < game->max_audio_sources; i++) {
        if (game->loaded_audio[i].type == AUDIO_SOURCE_NONE && !game->loaded_audio[i].stream_data.vorbis) {
            source = &game->loaded_audio[i];
            break;
        }
    }
    
    if (!source) {
        debug_print("Error: No available audio source slots\n");
        stb_vorbis_close(vorbis);
        return nullptr;
    }

    memset(source, 0, sizeof(AudioSource));
    
    // Initialize streaming source
    source->type = AUDIO_SOURCE_STREAMING;
    source->channels = info.channels;
    source->sample_rate = info.sample_rate;
    source->is_playing = false;
    source->loop = loop;
    source->volume = 1.0f;
    
    source->stream_data.vorbis = vorbis;
    source->stream_data.filename = arena_alloc(&game->permanent_arena, strlen(filename) + 1);
    if (!source->stream_data.filename) {
        debug_print("Error: Failed to allocate filename for streaming ogg\n");
        stb_vorbis_close(vorbis);
        return nullptr;
    }

    strcpy(source->stream_data.filename, filename);
    source->stream_data.buffer_frames = stream_buffer_frames;
    source->stream_data.stream_buffer = arena_alloc(
        &game->permanent_arena,
        stream_buffer_frames * info.channels * sizeof(i16)
    );
    if (!source->stream_data.stream_buffer) {
        debug_print("Error: Failed to allocate stream_buffer for streaming ogg\n");
        stb_vorbis_close(vorbis);
        return nullptr;
    }

    source->stream_data.buffer_position = 0;
    source->stream_data.buffer_valid = 0;
    source->stream_data.end_of_file = false;
    
    game->active_audio_count++;
    return source;
}
