#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include "audio.h"
#include "game.h"
#include <pthread.h>
#include <string.h>

typedef struct {
    int16* data;
    usize capacity;
    usize write_pos;
    usize read_pos;
    usize available;
    pthread_mutex_t mutex;
} RingBuffer;

internal struct {
    Game* game;
    pa_simple* pulse_simple;
    RingBuffer ring_buffer;
    pthread_t audio_thread;
    bool initialized;
    bool should_stop;
    uint32 buffer_size;
    uint32 samples_per_buffer;
} global_audio_state;

internal void ring_buffer_init(RingBuffer* rb, usize capacity) {
    // TODO: Handle allocations with our own allocator
    rb->data = calloc(capacity, sizeof(int16));
    rb->capacity = capacity;
    rb->write_pos = 0;
    rb->read_pos = 0;
    rb->available = 0;
    pthread_mutex_init(&rb->mutex, nullptr);
}

internal void ring_buffer_destroy(RingBuffer* rb) {
    pthread_mutex_destroy(&rb->mutex);
    free(rb->data);
    rb->data = nullptr;
}

internal usize ring_buffer_write(RingBuffer* rb, const int16* data, usize samples) {
    pthread_mutex_lock(&rb->mutex);
    
    usize space_available = rb->capacity - rb->available;
    usize samples_to_write = (samples < space_available) ? samples : space_available;
    
    for (usize i = 0; i < samples_to_write; i++) {
        rb->data[rb->write_pos] = data[i];
        rb->write_pos = (rb->write_pos + 1) % rb->capacity;
    }
    
    rb->available += samples_to_write;
    
    pthread_mutex_unlock(&rb->mutex);
    return samples_to_write;
}

internal usize ring_buffer_read(RingBuffer* rb, int16* data, usize samples) {
    pthread_mutex_lock(&rb->mutex);
    
    usize samples_to_read = (samples < rb->available) ? samples : rb->available;
    
    for (usize i = 0; i < samples_to_read; i++) {
        data[i] = rb->data[rb->read_pos];
        rb->read_pos = (rb->read_pos + 1) % rb->capacity;
    }
    
    rb->available -= samples_to_read;
    
    pthread_mutex_unlock(&rb->mutex);
    return samples_to_read;
}

internal void* audio_thread_proc(void* param) {
    Game* game = (Game*)param;
    
    // Calculate buffer size for one frame worth of audio
    uint32 frames_per_buffer = game->audio_sample_rate / game->fps;
    uint32 samples_per_buffer = frames_per_buffer * game->audio_channels;
    uint32 buffer_size = samples_per_buffer * sizeof(int16);
    
    int16* audio_buffer = malloc(buffer_size);
    if (!audio_buffer) {
        debug_print("Error: Could not allocate audio buffer\n");
        return nullptr;
    }
    
    while (!global_audio_state.should_stop) {
        // Read samples from ring buffer
        usize samples_read = ring_buffer_read(
            &global_audio_state.ring_buffer, 
            audio_buffer, 
            samples_per_buffer
        );
        
        // Fill remaining buffer with silence if needed
        if (samples_read < samples_per_buffer) {
            memset(audio_buffer + samples_read, 0, 
                   (samples_per_buffer - samples_read) * sizeof(int16));
        }
        
        // Apply volume
        if (game->audio_volume != 1.0f) {
            for (uint32 i = 0; i < samples_read; i++) {
                audio_buffer[i] = (int16)((real32)audio_buffer[i] * game->audio_volume);
            }
        }
        
        // Write to PulseAudio
        int error;
        if (pa_simple_write(global_audio_state.pulse_simple, audio_buffer, buffer_size, &error) < 0) {
            debug_print("Error: PulseAudio write failed: %s\n", pa_strerror(error));
            break;
        }
    }
    
    free(audio_buffer);
    return nullptr;
}

void audio_init(Game* game) {
    memset(&global_audio_state, 0, sizeof(global_audio_state));
    global_audio_state.game = game;
    game->audio_volume = 1.0f;
    
    // Initialize ring buffer with 1 second capacity
    usize ring_buffer_capacity = game->audio_sample_rate * game->audio_channels;
    ring_buffer_init(&global_audio_state.ring_buffer, ring_buffer_capacity);
    
    // Set up PulseAudio sample specification
    pa_sample_spec sample_spec = {
        .format = PA_SAMPLE_S16LE,  // 16-bit signed little-endian
        .rate = game->audio_sample_rate,
        .channels = game->audio_channels
    };
    
    // Set up buffer attributes for low latency
    pa_buffer_attr buffer_attr = {
        .maxlength = (uint32_t)-1,  // Maximum length of buffer
        .tlength = pa_usec_to_bytes(1000000 / game->fps, &sample_spec), // Target length (one frame)
        .prebuf = (uint32_t)-1,     // Pre-buffering
        .minreq = (uint32_t)-1,     // Minimum request
        .fragsize = (uint32_t)-1    // Fragment size
    };
    
    // Create PulseAudio simple connection
    int error;
    global_audio_state.pulse_simple = pa_simple_new(
        nullptr,                       // Use default server
        "C Software Renderer",         // Application name
        PA_STREAM_PLAYBACK,            // Stream direction
        nullptr,                       // Use default device
        "Game Audio",                  // Stream description
        &sample_spec,                  // Sample format
        nullptr,                       // Use default channel map
        &buffer_attr,                  // Buffer attributes
        &error                         // Error code
    );
    
    if (!global_audio_state.pulse_simple) {
        debug_print("Error: Could not create PulseAudio stream: %s\n", pa_strerror(error));
        ring_buffer_destroy(&global_audio_state.ring_buffer);
        return;
    }
    
    // Calculate buffer sizes
    global_audio_state.buffer_size = game->audio_channels * 
        (game->audio_sample_rate / game->fps) * sizeof(int16) * NUM_BUFFERS;
    global_audio_state.samples_per_buffer = global_audio_state.buffer_size / sizeof(int16);
    
    // Start audio thread
    global_audio_state.should_stop = false;
    
    int result = pthread_create(
        &global_audio_state.audio_thread,
        nullptr,
        audio_thread_proc,
        game
    );
    
    if (result != 0) {
        debug_print("Error: Could not create audio thread (error: %d)\n", result);
        pa_simple_free(global_audio_state.pulse_simple);
        global_audio_state.pulse_simple = nullptr;
        ring_buffer_destroy(&global_audio_state.ring_buffer);
        return;
    }
    
    global_audio_state.initialized = true;
    debug_print("PulseAudio audio system initialized successfully\n");
}

void audio_update_buffer(Game* game) {
    if (!global_audio_state.initialized || !game->audio) {
        return;
    }
    
    usize samples_written = ring_buffer_write(
        &global_audio_state.ring_buffer,
        game->audio,
        AUDIO_CAPACITY
    );
    
    if (samples_written < AUDIO_CAPACITY) {
        /* debug_print("Ring buffer is full, some audio data was dropped\n"); */
    }
}

void audio_set_volume(real32 volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    global_audio_state.game->audio_volume = volume;
}

void audio_cleanup(void) {
    // Signal thread to stop
    global_audio_state.should_stop = true;
    
    // Wait for audio thread to finish
    if (global_audio_state.audio_thread) {
        pthread_join(global_audio_state.audio_thread, nullptr);
        global_audio_state.audio_thread = 0;
    }
    
    // Clean up PulseAudio
    if (global_audio_state.pulse_simple) {
        // Drain any remaining audio
        int error;
        pa_simple_drain(global_audio_state.pulse_simple, &error);
        
        // Free the connection
        pa_simple_free(global_audio_state.pulse_simple);
        global_audio_state.pulse_simple = nullptr;
    }
    
    // Clean up ring buffer
    ring_buffer_destroy(&global_audio_state.ring_buffer);
    global_audio_state.initialized = false;
    
    debug_print("PulseAudio audio system cleaned up\n");
}
