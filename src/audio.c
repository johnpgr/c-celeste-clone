#import <AudioToolbox/AudioToolbox.h>
#include "audio.h"
#include "game.h"

#define NUM_BUFFERS 3

static struct {
    Game* game;
    AudioQueueRef queue;
    AudioQueueBufferRef buffers[NUM_BUFFERS];
    double phase; // Track phase continuously
} global_audio_state;

/**
 * @brief The callback function that Audio Queue Services calls to get more audio data.
 * This now generates audio directly instead of copying from a shared buffer.
 */
static void audio_callback(void* user_data, AudioQueueRef aq, AudioQueueBufferRef buffer) {
    Game* game = (Game*)user_data;
    if(!game) {
        memset(buffer->mAudioData, 0, buffer->mAudioDataBytesCapacity);
        buffer->mAudioDataByteSize = buffer->mAudioDataBytesCapacity;
        AudioQueueEnqueueBuffer(aq, buffer, 0, NULL);
        return;
    }

    i16* audio_data = (i16*)buffer->mAudioData;
    u32 frames_needed = game->audio_sample_rate / game->fps;
    u32 buffer_size = frames_needed * game->audio_channels * sizeof(i16);
    
    // Ensure we don't exceed buffer capacity
    if (buffer_size > buffer->mAudioDataBytesCapacity) {
        buffer_size = buffer->mAudioDataBytesCapacity;
        frames_needed = buffer_size / (game->audio_channels * sizeof(i16));
    }

    // Generate audio directly in the callback
    const double tone_hz = 440.0;
    const double phase_increment = 2.0 * M_PI * tone_hz / game->audio_sample_rate;
    
    for (u32 i = 0; i < frames_needed; i++) {
        // Generate sine wave sample
        float sample   = sinf((float)global_audio_state.phase) * 0.1f;
        i16 sample_i16 = (i16)(sample * 32767.0f);

        // Fill both channels (stereo)
        audio_data[i * game->audio_channels + 0] = sample_i16; // Left
        audio_data[i * game->audio_channels + 1] = sample_i16; // Right

        // Increment phase and wrap to prevent precision loss
        global_audio_state.phase += phase_increment;
        if (global_audio_state.phase >= 2.0 * M_PI) {
            global_audio_state.phase -= 2.0 * M_PI;
        }
    }

    buffer->mAudioDataByteSize = buffer_size;
    AudioQueueEnqueueBuffer(aq, buffer, 0, NULL);
}

void audio_init(Game* game) {
    global_audio_state.game  = game;
    global_audio_state.phase = 0.0;

    AudioStreamBasicDescription format = {
        .mSampleRate       = game->audio_sample_rate,
        .mFormatID         = kAudioFormatLinearPCM,
        .mFormatFlags      = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked,
        .mFramesPerPacket  = 1,
        .mChannelsPerFrame = game->audio_channels,
        .mBitsPerChannel   = sizeof(i16) * 8,
        .mBytesPerPacket   = game->audio_channels * sizeof(i16),
        .mBytesPerFrame    = game->audio_channels * sizeof(i16),
    };

    OSStatus status = AudioQueueNewOutput(&format, audio_callback, game, NULL, NULL, 0, &global_audio_state.queue);
    if(status != noErr) {
        fprintf(stderr, "Error: Could not create audio queue\n");
        return;
    }

    u32 buffer_size = game->audio_channels * (game->audio_sample_rate / game->fps) * sizeof(i16);
    for (int i = 0; i < NUM_BUFFERS; i++) {
        status = AudioQueueAllocateBuffer(global_audio_state.queue, buffer_size, &global_audio_state.buffers[i]);
        if (status == noErr) {
            // Initialize buffers with silence
            memset(global_audio_state.buffers[i]->mAudioData, 0, buffer_size);
            global_audio_state.buffers[i]->mAudioDataByteSize = buffer_size;
            AudioQueueEnqueueBuffer(global_audio_state.queue, global_audio_state.buffers[i], 0, NULL);
        } else {
            fprintf(stderr, "Error: Could not allocate audio buffer %d (status: %d)\n", i, (int)status);
        }
    }

    status = AudioQueueStart(global_audio_state.queue, NULL);
    if (status != noErr) {
        fprintf(stderr, "Error: Could not start audio queue (status: %d)\n", (int)status);
    }
}

void audio_cleanup(void) {
    if (global_audio_state.queue) {
        AudioQueueStop(global_audio_state.queue, true);
        AudioQueueDispose(global_audio_state.queue, true);
        global_audio_state.queue = NULL;
    }
}
