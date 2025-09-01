#pragma once

#include "game.h"

#define NUM_BUFFERS 3

/**
 * @brief Initializes the audio system.
 * @param game A pointer to the game instance.
 */
void audio_init(Game* game);

/**
 * @brief Shuts down the audio system and releases resources.
 */
void audio_cleanup(void);

/**
 * @brief Updates the audio buffer with new data from the game.
 * This should be called from the main thread after game_update_and_render.
 * @param game A pointer to the game instance with updated audio data.
 */
void audio_update_buffer(Game* game);

/**
 * @brief Sets the master volume for audio playback.
 * @param volume Volume level (0.0 to 1.0)
 */
void audio_set_volume(float volume);
