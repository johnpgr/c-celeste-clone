/**
 * @file platform_audio.h
 * @brief Platform-specific audio interface.
 */
#pragma once
#include "def.h"
#include "consts.h"
#include "audio.h"

/**
 * @brief Initializes the audio system.
 * @param game A pointer to the game instance.
 */
void platform_audio_init(AudioState* audio_state);

/**
 * @brief Shuts down the audio system and releases resources.
 */
void platform_audio_cleanup(void);

/**
 * @brief Updates the audio buffer with new data from the game.
 * This should be called from the main thread after game_update_and_render.
 * @param game A pointer to the game instance with updated audio data.
 */
void platform_audio_update_buffer();

/**
 * @brief Sets the master volume for audio playback.
 * @param volume Volume level (0.0 to 1.0)
 */
void platform_audio_set_volume(real32 volume);
