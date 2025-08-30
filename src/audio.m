#import <AudioToolbox/AudioToolbox.h>
#include "audio.h"
#include "game.h"

#define NUM_BUFFERS 3

static struct {
    Game* game;
    AudioQueueRef queue;
    AudioQueueBufferRef buffers[NUM_BUFFERS];
} global_audio_state;

/**
 * @brief The callback function that Audio Queue Services calls to get more audio data.
 *
 * @param user_data    A custom pointer we provide (we'll use it for our Game pointer).
 * @param aq          The audio queue that is requesting data.
 * @param Buffer      The buffer that needs to be filled with audio data.
 */
static void audio_callback(void* user_data, AudioQueueRef aq, AudioQueueBufferRef buffer) {
    Game* game = (Game*)user_data;

    if(!game || !game->audio) {
        memset(buffer->mAudioData, 0, buffer->mAudioDataBytesCapacity);
        buffer->mAudioDataByteSize = buffer->mAudioDataBytesCapacity;
        AudioQueueEnqueueBuffer(aq, buffer, 0, NULL);
        return;
    }

    memcpy(buffer->mAudioData, game->audio, game->audio_channels * (game->audio_sample_rate / game->fps) * sizeof(i16));

    buffer->mAudioDataByteSize = game->audio_channels * (game->audio_sample_rate / game->fps) * sizeof(i16);

    AudioQueueEnqueueBuffer(aq, buffer, 0, NULL);
}

void audio_init(Game* game) {
    global_audio_state.game = game;

    AudioStreamBasicDescription format = {
        .mSampleRate = game->audio_sample_rate,
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked,
        .mFramesPerPacket = 1,
        .mChannelsPerFrame = game->audio_channels,
        .mBitsPerChannel = sizeof(i16) * 8,
        .mBytesPerPacket = game->audio_channels * sizeof(i16),
        .mBytesPerFrame = game->audio_channels * sizeof(i16),
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
            audio_callback(game, global_audio_state.queue, global_audio_state.buffers[i]);
        }
    }

    AudioQueueStart(global_audio_state.queue, NULL);
}

void audio_cleanup(void) {
    if (global_audio_state.queue) {
        AudioQueueStop(global_audio_state.queue, true);
        AudioQueueDispose(global_audio_state.queue, true);
        global_audio_state.queue = NULL;
    }
}
