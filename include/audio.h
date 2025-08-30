#pragma once
#include "game.h"

/**
 * @brief Initializes the audio system.
 * @param game A pointer to the game instance.
 *
 * Sets up the audio output device, format, and playback queue.
 */
void audio_init(Game* game);

/**
 * @brief Shuts down the audio system and releases resources.
 */
void audio_cleanup(void);
