#pragma once
#include "game.h"

/**
 * @file window.h
 * @brief Cross-platform window management for the software renderer
 * 
 * This module provides a platform-specific window implementation using Cocoa
 * on macOS. It creates a native window, handles the main event loop, and
 * displays the game's framebuffer using Core Graphics.
 */

/**
 * @brief Initialize the window system with a game instance
 * @param game Pointer to the game instance containing display settings
 * 
 * Creates the application delegate and sets up the Cocoa application.
 * Must be called before window_run().
 */
void window_init(Game* game);

/**
 * @brief Start the main window loop
 * 
 * Begins the Cocoa event loop and displays the window. This function
 * blocks until the window is closed. The game loop timer is automatically
 * started to update and render at the specified FPS.
 */
void window_run(void);
