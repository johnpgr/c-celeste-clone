#import <AudioToolbox/AudioToolbox.h>
#include "audio.h"
#include "game.h"
#include <pthread.h>

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
    AudioQueueRef queue;
    AudioQueueBufferRef buffers[NUM_BUFFERS];
    RingBuffer ring_buffer;
    bool initialized;
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

internal void audio_callback(void* user_data, AudioQueueRef aq, AudioQueueBufferRef buffer) {
    Game* game = (Game*)user_data;
    if (!game || !global_audio_state.initialized) {
        memset(buffer->mAudioData, 0, buffer->mAudioDataBytesCapacity);
        buffer->mAudioDataByteSize = buffer->mAudioDataBytesCapacity;
        AudioQueueEnqueueBuffer(aq, buffer, 0, nullptr);
        return;
    }

    int16* audio_data = (int16*)buffer->mAudioData;
    uint32 frames_needed = game->audio_sample_rate / game->fps;
    uint32 total_samples = frames_needed * game->audio_channels;
    uint32 buffer_size = total_samples * sizeof(int16);
    
    if (buffer_size > buffer->mAudioDataBytesCapacity) {
        buffer_size = buffer->mAudioDataBytesCapacity;
        total_samples = buffer_size / sizeof(int16);
        frames_needed = total_samples / game->audio_channels;
    }

    usize samples_read = ring_buffer_read(&global_audio_state.ring_buffer, audio_data, total_samples);
    
    if (samples_read < total_samples) {
        memset(audio_data + samples_read, 0, (total_samples - samples_read) * sizeof(int16));
    }
    
    if (game->audio_volume != 1.0f) {
        for (uint32 i = 0; i < samples_read; i++) {
            audio_data[i] = (int16)((real32)audio_data[i] * game->audio_volume);
        }
    }

    buffer->mAudioDataByteSize = buffer_size;
    AudioQueueEnqueueBuffer(aq, buffer, 0, nullptr);
}

void audio_init(Game* game) {
    memset(&global_audio_state, 0, sizeof(global_audio_state));
    global_audio_state.game = game;
    game->audio_volume = 1.0f;

    usize ring_buffer_capacity = game->audio_sample_rate * game->audio_channels;
    ring_buffer_init(&global_audio_state.ring_buffer, ring_buffer_capacity);

    AudioStreamBasicDescription format = {
        .mSampleRate       = game->audio_sample_rate,
        .mFormatID         = kAudioFormatLinearPCM,
        .mFormatFlags      = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked,
        .mFramesPerPacket  = 1,
        .mChannelsPerFrame = game->audio_channels,
        .mBitsPerChannel   = sizeof(int16) * 8,
        .mBytesPerPacket   = game->audio_channels * sizeof(int16),
        .mBytesPerFrame    = game->audio_channels * sizeof(int16),
    };

    OSStatus status = AudioQueueNewOutput(
        &format,
        audio_callback,
        game,
        nullptr,
        nullptr,
        0,
        &global_audio_state.queue
    );
    
    if (status != noErr) {
        debug_print("Error: Could not create audio queue (status: %d)\n", (int)status);
        return;
    }

    uint32 buffer_size = game->audio_channels * (game->audio_sample_rate / game->fps) * sizeof(int16);
    for (int i = 0; i < NUM_BUFFERS; i++) {
        status = AudioQueueAllocateBuffer(global_audio_state.queue, buffer_size, &global_audio_state.buffers[i]);
        if (status == noErr) {
            memset(global_audio_state.buffers[i]->mAudioData, 0, buffer_size);
            global_audio_state.buffers[i]->mAudioDataByteSize = buffer_size;
            AudioQueueEnqueueBuffer(global_audio_state.queue, global_audio_state.buffers[i], 0, nullptr);
        } else {
            debug_print("Error: Could not allocate audio buffer %d (status: %d)\n", i, (int)status);
            return;
        }
    }

    status = AudioQueueStart(global_audio_state.queue, nullptr);
    if (status != noErr) {
        debug_print("Error: Could not start audio queue (status: %d)\n", (int)status);
        return;
    }
    
    global_audio_state.initialized = true;
    debug_print("Audio system initialized successfully\n");
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
    if (global_audio_state.queue) {
        AudioQueueStop(global_audio_state.queue, true);
        AudioQueueDispose(global_audio_state.queue, true);
        global_audio_state.queue = nullptr;
    }
    
    ring_buffer_destroy(&global_audio_state.ring_buffer);
    global_audio_state.initialized = false;

    debug_print("CoreAudio audio system cleaned up\n");
}
