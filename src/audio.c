#include "audio.h"

internal int16* resample_audio(
    Arena* arena,
    int16* input_samples,
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
        int16* output = arena_alloc(arena, total_samples * sizeof(int16));
        memcpy(output, input_samples, total_samples * sizeof(int16));
        return output;
    }
    
    // If input is 22kHz and target is 48kHz, ratio should be 48/22 = 2.18 (upsample)
    // If input is 48kHz and target is 22kHz, ratio should be 22/48 = 0.46 (downsample)
    real64 ratio = (real64)target_rate / (real64)input_rate;
    *output_frames = (usize)(input_frames * ratio + 0.5); // Add 0.5 for rounding
    
    usize output_samples = *output_frames * input_channels;
    int16* output = arena_alloc(arena, output_samples * sizeof(int16));
    if (!output) {
        debug_print("Error: Transient arena out of memory for raw samples\n");
        return nullptr;
    }
    
    debug_print("Resampling ratio: %.3f, output frames: %zu\n", ratio, *output_frames);
    
    for (usize i = 0; i < *output_frames; i++) {
        // For upsampling (ratio > 1): source advances slower than output
        // For downsampling (ratio < 1): source advances faster than output
        real64 source_index = (real64)i / ratio;
        usize index1 = (usize)source_index;
        usize index2 = index1 + 1;
        
        // Boundary check
        if (index1 >= input_frames) index1 = input_frames - 1;
        if (index2 >= input_frames) index2 = input_frames - 1;
        
        real64 fraction = source_index - (real64)index1;
        
        for (int ch = 0; ch < input_channels; ch++) {
            int16 sample1 = input_samples[index1 * input_channels + ch];
            int16 sample2 = input_samples[index2 * input_channels + ch];
            
            // Linear interpolation
            real64 interpolated_d = (real64)sample1 + fraction * ((real64)sample2 - (real64)sample1);
            int16 interpolated = (int16)CLAMP(interpolated_d, -32768.0, 32767.0);
            
            output[i * input_channels + ch] = interpolated;
        }
    }
    
    return output;
}

// Channel conversion helper
internal void convert_channels(
    int16* input,
    int input_channels,
    int16* output,
    int output_channels,
    usize frames
) {
    for (usize frame = 0; frame < frames; frame++) {
        if (input_channels == 1 && output_channels == 2) {
            // Mono to stereo
            int16 mono = input[frame];
            output[frame * 2 + 0] = mono;
            output[frame * 2 + 1] = mono;
        } else if (input_channels == 2 && output_channels == 1) {
            // Stereo to mono
            int16 left = input[frame * 2 + 0];
            int16 right = input[frame * 2 + 1];
            output[frame] = (int16)((left + right) / 2);
        } else {
            // Direct copy or take first available channels
            for (int ch = 0; ch < output_channels; ch++) {
                int src_ch = (ch < input_channels) ? ch : 0;
                output[frame * output_channels + ch] = input[frame * input_channels + src_ch];
            }
        }
    }
}

internal void audio_source_stream_begin(AudioSource* source) {
    assert(source->type == AUDIO_SOURCE_STREAMING 
           && "tried to start stream source of a non-streaming audio source");
    stb_vorbis_seek_start(source->stream_data.vorbis);
}

internal void audio_source_stream_end(AudioSource* source) {
    assert(source->type == AUDIO_SOURCE_STREAMING 
           && "tried to close stream source of a non-streaming audio source");
    stb_vorbis_close(source->stream_data.vorbis);
}

internal bool audio_source_stream_refill_buffer(AudioSource* source) {
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
            return audio_source_stream_refill_buffer(source); // Try again
        }
        
        return false;
    }
}

internal void process_static_audio_source(AudioSource* source, AudioState* audio_state, usize frames_needed) {
    assert(source != nullptr);

    for (usize frame = 0; frame < frames_needed; frame++) {
        if (source->static_data.current_position >= source->static_data.frame_count) {
            if (source->loop) {
                source->static_data.current_position = 0;
            } else {
                source->is_playing = false;
                break;
            }
        }
        
        for (usize ch = 0; ch < AUDIO_CHANNELS; ch++) {
            usize src_idx = source->static_data.current_position * source->channels + ch;
            usize dst_idx = frame * AUDIO_CHANNELS + ch;
            
            int mixed = (int)audio_state->audio[dst_idx] + 
                       (int)(source->static_data.samples[src_idx] * source->volume);
            
            audio_state->audio[dst_idx] = (int16)CLAMP(mixed, -32768, 32767);
        }
        
        source->static_data.current_position++;
    }
}

internal void process_streaming_audio_source(AudioSource* source, AudioState* audio_state, usize frames_needed) {
    assert(source != nullptr);
    usize frames_processed = 0;
    
    while (frames_processed < frames_needed && source->is_playing) {
        if (source->stream_data.buffer_position >= source->stream_data.buffer_valid) {
            if (!audio_source_stream_refill_buffer(source)) {
                source->is_playing = false;
                break;
            }
        }
        
        usize frames_available = source->stream_data.buffer_valid - source->stream_data.buffer_position;
        usize frames_to_process = (frames_needed - frames_processed < frames_available) 
                                ? frames_needed - frames_processed 
                                : frames_available;
        
        for (usize frame = 0; frame < frames_to_process; frame++) {
            usize stream_frame_idx = source->stream_data.buffer_position + frame;
            usize output_frame_idx = frames_processed + frame;
            
            if (source->sample_rate == (int)AUDIO_SAMPLE_RATE && 
                source->channels == (int)AUDIO_CHANNELS) {
                for (usize ch = 0; ch < AUDIO_CHANNELS; ch++) {
                    usize src_idx = stream_frame_idx * source->channels + ch;
                    usize dst_idx = output_frame_idx * AUDIO_CHANNELS + ch;
                    
                    int mixed = (int)audio_state->audio[dst_idx] + (int)(source->stream_data.stream_buffer[src_idx] * source->volume);
                    
                    audio_state->audio[dst_idx] = (int16)CLAMP(mixed, -32768, 32767);
                }
                continue;
            }
            if (source->channels == 1 && AUDIO_CHANNELS == 2) {
                int16 mono_sample = (int16)(source->stream_data.stream_buffer[stream_frame_idx] * source->volume);
                usize dst_idx = output_frame_idx * 2;
                
                int mixed_l = (int)audio_state->audio[dst_idx + 0] + mono_sample;
                int mixed_r = (int)audio_state->audio[dst_idx + 1] + mono_sample;
                
                audio_state->audio[dst_idx + 0] = (int16)CLAMP(mixed_l, -32768, 32767);
                audio_state->audio[dst_idx + 1] = (int16)CLAMP(mixed_r, -32768, 32767);
            }
        }
        
        source->stream_data.buffer_position += frames_to_process;
        frames_processed += frames_to_process;
    }
}

AudioSource* create_audio_source_static(
    Arena* permanent_storage,
    AudioState* audio_state,
    const char* filename,
    bool loop
) {
    int error = 0;
    AudioSource* source = nullptr;
    stb_vorbis* vorbis = stb_vorbis_open_filename(filename, &error, nullptr);

    // TODO: Set a global MAX_AUDIO_SIZE to avoid overflowing this temporary arena
    Arena temp_arena = create_arena(MB(100));

    if (audio_state->audio_sources_size >= MAX_AUDIO_SOURCES) {
        debug_print("Error: Maximum audio sources reached\n");
        goto cleanup;
    }
    
    if (!vorbis) {
        debug_print("Error: Could not open OGG file '%s' (error: %d)\n", filename, error);
        goto cleanup;
    }
    
    stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    usize total_frames = stb_vorbis_stream_length_in_samples(vorbis);
    
    debug_print("Loading static OGG: %s\n", filename);
    debug_print("  Original: %d Hz, %d channels, %zu frames\n", info.sample_rate, info.channels, total_frames);
    debug_print("  Target: %zu Hz, %zu channels\n", AUDIO_SAMPLE_RATE, AUDIO_CHANNELS);
    
    // Decode entire file into transient memory first
    usize total_input_samples = total_frames * info.channels;
    int16* raw_samples = arena_alloc(&temp_arena, total_input_samples * sizeof(int16));
    if (!raw_samples) {
        debug_print("Error: Transient arena out of memory for raw samples\n");
        goto cleanup;
    }
    
    int decoded_frames = stb_vorbis_get_samples_short_interleaved(
        vorbis, info.channels, raw_samples, (int)total_input_samples);
    
    if (decoded_frames <= 0) {
        debug_print("Error: Failed to decode OGG file\n");
        goto cleanup;
    }
    
    debug_print("  Decoded %d frames successfully\n", decoded_frames);
    
    // Step 1: Handle sample rate conversion
    int16* resampled_audio = nullptr;
    usize resampled_frames = 0;
    
    if (info.sample_rate != (usize)AUDIO_SAMPLE_RATE) {
        resampled_audio = resample_audio(
            &temp_arena,
            raw_samples,
            (usize)decoded_frames,
            info.channels, info.sample_rate,
            (int)AUDIO_SAMPLE_RATE, &resampled_frames
        );
        // Note: raw_samples will be reclaimed on arena reset
        debug_print("  After resampling: %zu frames\n", resampled_frames);
    } else {
        resampled_audio = raw_samples;
        resampled_frames = (usize)decoded_frames;
        debug_print("  No resampling needed\n");
    }
    
    // Step 2: Handle channel conversion
    int16* final_audio = nullptr;
    usize final_frames = resampled_frames;
    
    if (info.channels != (int)AUDIO_CHANNELS) {
        debug_print("  Converting channels: %d -> %zu\n", info.channels, AUDIO_CHANNELS);
        
        usize final_samples = final_frames * AUDIO_CHANNELS;
        final_audio = arena_alloc(permanent_storage, final_samples * sizeof(int16));
        if (!final_audio) {
            debug_print("Error: Arena out of memory for final audio\n");
            goto cleanup;
        }
        
        convert_channels(
            resampled_audio,
            info.channels,
            final_audio,
            (int)AUDIO_CHANNELS,
            final_frames
        );

        debug_print("  After channel conversion: %zu samples\n", final_samples);
    } else {
        final_audio = resampled_audio;
        debug_print("  No channel conversion needed\n");
    }

    usize permanent_sample_count = final_frames * AUDIO_CHANNELS;
    int16* permanent_samples = arena_alloc(permanent_storage, permanent_sample_count * sizeof(int16));
    if (!permanent_samples) {
        debug_print("Error: Permanent arena out of memory for audio data\n");
        goto cleanup;
    }
    memcpy(permanent_samples, final_audio, permanent_sample_count * sizeof(int16));
    
    // Find empty slot
    for (usize i = 0; i < MAX_AUDIO_SOURCES; i++) {
        if (audio_state->audio_sources[i].type == AUDIO_SOURCE_NONE) {
            source = &audio_state->audio_sources[i];
            break;
        }
    }
    
    if (!source) {
        debug_print("Error: No available audio source slots\n");
        goto cleanup;
    }
    
    // Initialize static audio source
    memset(source, 0, sizeof(AudioSource));
    source->type = AUDIO_SOURCE_STATIC;
    source->channels = (int)AUDIO_CHANNELS;
    source->sample_rate = (int)AUDIO_SAMPLE_RATE;
    source->is_playing = false;
    source->loop = loop;
    source->volume = 1.0f;
    
    source->static_data.samples = permanent_samples;
    source->static_data.sample_count = final_frames * AUDIO_CHANNELS;
    source->static_data.frame_count = final_frames;
    source->static_data.current_position = 0;
    
    audio_state->audio_sources_size++;
    
    debug_print("Successfully loaded static audio: %zu frames, %d channels, %d Hz\n", 
        source->static_data.frame_count, source->channels, source->sample_rate);

cleanup:
    stb_vorbis_close(vorbis);
    arena_cleanup(&temp_arena);
    return source;
}

AudioSource* create_audio_source_static_memory(
    Arena* permanent_storage,
    AudioState* audio_state,
    const uint8* data,
    usize data_size,
    bool loop
) {
    int error = 0;
    stb_vorbis* vorbis = stb_vorbis_open_memory(data, (int)data_size, &error, nullptr);
    AudioSource* source = nullptr;

    // TODO: Set a global MAX_AUDIO_SIZE to avoid overflowing this temporary arena
    Arena temp_arena = create_arena(MB(100));

    if (data_size > INT_MAX) {
        debug_print("Error: ogg size bigger than the maximum allowed in stb_vorbis\n");
        goto cleanup;
    }

    if (audio_state->audio_sources_size >= MAX_AUDIO_SOURCES) {
        debug_print("Error: Maximum audio sources reached\n");
        goto cleanup;
    }

    if (!vorbis) {
        debug_print("Error: Could not open OGG data in memory (error: %d)\n", error);
        goto cleanup;
    }

    stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    usize total_frames = stb_vorbis_stream_length_in_samples(vorbis);

    debug_print("Loading static OGG from memory\n");
    debug_print("  Original: %d Hz, %d channels, %zu frames\n", info.sample_rate, info.channels, total_frames);
    debug_print("  Target: %zu Hz, %zu channels\n", AUDIO_SAMPLE_RATE, AUDIO_CHANNELS);
    debug_print("  Transient arena before loading: %.1f/%.1f KB used\n", 
               arena_get_used(&temp_arena) / 1024.0f, temp_arena.size / 1024.0f);

    // Decode entire file
    usize total_input_samples = total_frames * info.channels;
    int16* raw_samples = arena_alloc(
        &temp_arena,
        total_input_samples * sizeof(int16)
    );
    if (!raw_samples) {
        debug_print("Error: Out of memory allocating raw samples\n");
        goto cleanup;
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
        goto cleanup;
    }

    debug_print("  Decoded %d frames successfully\n", decoded_frames);

    // Step 1: Handle sample rate conversion
    int16* resampled_audio = nullptr;
    usize resampled_frames = 0;

    if (info.sample_rate != (usize)AUDIO_SAMPLE_RATE) {
        resampled_audio = resample_audio(
            &temp_arena,
            raw_samples,
            (usize)decoded_frames,
            info.channels, info.sample_rate,
            (int)AUDIO_SAMPLE_RATE, &resampled_frames
        );
        if (!resampled_audio) {
            debug_print("Error: Resampling failed\n");
            goto cleanup;
        }
        // Note: raw_samples will be reclaimed on arena reset
        debug_print("  After resampling: %zu frames\n", resampled_frames);
    } else {
        resampled_audio = raw_samples;
        resampled_frames = (usize)decoded_frames;
        debug_print("  No resampling needed\n");
    }

    // Step 2: Handle channel conversion
    int16* final_audio = nullptr;
    usize final_frames = resampled_frames;

    if (info.channels != (int)AUDIO_CHANNELS) {
        debug_print("  Converting channels: %d -> %zu\n", info.channels, AUDIO_CHANNELS);

        usize final_samples = final_frames * AUDIO_CHANNELS;
        final_audio = arena_alloc(
            &temp_arena,
            final_samples * sizeof(int16)
        );
        if (!final_audio) {
            debug_print("Error: Transient arena out of memory for final audio\n");
            goto cleanup;
        }

        convert_channels(
            resampled_audio,
            info.channels,
            final_audio,
            (int)AUDIO_CHANNELS,
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
        goto cleanup;
    }

    for (usize i = 0; i < MAX_AUDIO_SOURCES; i++) {
        debug_print("  Checking slot %zu: type=%d\n", i, audio_state->audio_sources[i].type);
        if (audio_state->audio_sources[i].type == AUDIO_SOURCE_NONE) {
            source = &audio_state->audio_sources[i];
            debug_print("  Using slot %zu\n", i);
            break;
        }
    }

    if (!source) {
        debug_print("Error: No available audio source slots\n");
        goto cleanup;
    }

    // Now copy final audio data to permanent storage
    usize permanent_sample_count = final_frames * AUDIO_CHANNELS;
    int16* permanent_samples = arena_alloc(
        permanent_storage,
        permanent_sample_count * sizeof(int16)
    );
    if (!permanent_samples) {
        debug_print("Error: Permanent arena out of memory for audio data\n");
        goto cleanup;
    }
    memcpy(permanent_samples, final_audio, permanent_sample_count * sizeof(int16));

    // Initialize static audio source - CRITICAL: Zero the entire structure first
    memset(source, 0, sizeof(AudioSource));
    source->type = AUDIO_SOURCE_STATIC;
    source->channels = (int)AUDIO_CHANNELS;
    source->sample_rate = (int)AUDIO_SAMPLE_RATE;
    source->is_playing = false;
    source->loop = loop;
    source->volume = 1.0f;

    source->static_data.samples = permanent_samples;
    source->static_data.sample_count = final_frames * AUDIO_CHANNELS;
    source->static_data.frame_count = final_frames;
    source->static_data.current_position = 0;

    audio_state->audio_sources_size++;

    debug_print("Successfully loaded static audio from memory: %zu frames, %d channels, %d Hz (slot %zu)\n",
        source->static_data.frame_count, source->channels, source->sample_rate, 
        (usize)(source - audio_state->audio_sources));

cleanup:
    stb_vorbis_close(vorbis);
    arena_cleanup(&temp_arena);
    return source;
}

AudioSource* create_audio_source_streaming(
    Arena* permanent_storage,
    AudioState* audio_state,
    const char* filename,
    int stream_buffer_frames,
    bool loop
) {
    int error = 0;
    stb_vorbis* vorbis = stb_vorbis_open_filename(filename, &error, nullptr);
    AudioSource* source = nullptr;

    if (!vorbis) {
        debug_print("Error: Could not open OGG file '%s' for streaming\n", filename);
        return nullptr;
    }

    if (audio_state->audio_sources_size >= MAX_AUDIO_SOURCES) {
        debug_print("Error: Maximum audio sources reached\n");
        stb_vorbis_close(vorbis);
        return nullptr;
    }
    
    stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    
    debug_print("Loading streaming OGG: %s (%d Hz, %d channels)\n", filename, info.sample_rate, info.channels);
    
    // Find empty slot
    for (usize i = 0; i < MAX_AUDIO_SOURCES; i++) {
        if (audio_state->audio_sources[i].type == AUDIO_SOURCE_NONE && !audio_state->audio_sources[i].stream_data.vorbis) {
            source = &audio_state->audio_sources[i];
            break;
        }
    }
    
    if (!source) {
        debug_print("Error: No available audio source slots\n");
        stb_vorbis_close(vorbis);
        return source;
    }

    // Initialize streaming source
    memset(source, 0, sizeof(AudioSource));
    source->type = AUDIO_SOURCE_STREAMING;
    source->channels = info.channels;
    source->sample_rate = info.sample_rate;
    source->is_playing = false;
    source->loop = loop;
    source->volume = 1.0f;
    source->stream_data.vorbis = vorbis;
    source->stream_data.filename = arena_alloc(permanent_storage, strlen(filename) + 1);

    if (!source->stream_data.filename) {
        debug_print("Error: Failed to allocate filename for streaming ogg\n");
        stb_vorbis_close(vorbis);
        return nullptr;
    }

    strcpy(source->stream_data.filename, filename);
    source->stream_data.buffer_frames = stream_buffer_frames;
    source->stream_data.stream_buffer = arena_alloc(
        permanent_storage,
        stream_buffer_frames * info.channels * sizeof(int16)
    );
    if (!source->stream_data.stream_buffer) {
        debug_print("Error: Failed to allocate stream_buffer for streaming ogg\n");
        stb_vorbis_close(vorbis);
        return nullptr;
    }

    source->stream_data.buffer_position = 0;
    source->stream_data.buffer_valid = 0;
    source->stream_data.end_of_file = false;
    
    audio_state->audio_sources_size++;
    debug_print("Successfully created streaming audio source: %d Hz, %d channels\n", 
        source->sample_rate, source->channels);

    return source;
}

void audio_source_play(AudioSource* source) {
    if (!source) return;
    
    source->is_playing = true;
    
    if (source->type == AUDIO_SOURCE_STATIC) {
        source->static_data.current_position = 0;
    } else if (source->type == AUDIO_SOURCE_STREAMING) {
        audio_source_stream_begin(source);
        source->stream_data.buffer_position = 0;
        source->stream_data.buffer_valid = 0;
        source->stream_data.end_of_file = false;
    }
}

void audio_source_stop(AudioSource* source) {
    if (source) {
        source->is_playing = false;
    }
}

void audio_source_set_volume(AudioSource* source, float volume) {
    if (source) {
        source->volume = CLAMP(volume, 0.0f, 1.0f);
    }
}

void audio_source_cleanup(AudioSource* source) {
    if (!source) return;
    
    source->is_playing = false;
    
    if (source->type == AUDIO_SOURCE_STATIC) {
        // Note: Arena-allocated memory doesn't need explicit freeing
        source->static_data.samples = nullptr;
    } else if (source->type == AUDIO_SOURCE_STREAMING) {
        if (source->stream_data.vorbis) {
            audio_source_stream_end(source);
        }
        // Note: Arena-allocated memory doesn't need explicit freeing
        source->stream_data.filename = nullptr;
        source->stream_data.stream_buffer = nullptr;
    }
    
    memset(source, 0, sizeof(AudioSource));
}


void audio_state_update(AudioState* audio_state) {
    memset(audio_state->audio, 0, AUDIO_CAPACITY * sizeof(int16));
    
    usize frames_needed = AUDIO_CAPACITY / AUDIO_CHANNELS;
    
    for (usize source_idx = 0; source_idx < MAX_AUDIO_SOURCES; source_idx++) {
        AudioSource* source = &audio_state->audio_sources[source_idx];
        if (!source->is_playing) continue;
        
        if (source->type == AUDIO_SOURCE_STATIC) {
            process_static_audio_source(source, audio_state, frames_needed);
        } else if (source->type == AUDIO_SOURCE_STREAMING) {
            process_streaming_audio_source(source, audio_state, frames_needed);
        }
    }
}

void audio_state_cleanup(AudioState* audio_state) {
    debug_print("Cleaning up audio_state resources...\n");
    
    for (usize i = 0; i < MAX_AUDIO_SOURCES; i++) {
        audio_source_cleanup(&audio_state->audio_sources[i]);
    }
    
    audio_state->audio_sources_size = 0;
}