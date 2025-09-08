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
 * @brief Key codes for input handling
 * These are abstract key identifiers, not platform-specific codes
 */
typedef enum {
    WINDOW_KEY_ESCAPE,
    WINDOW_KEY_SPACE,
    WINDOW_KEY_A,
    WINDOW_KEY_D,
    WINDOW_KEY_S,
    WINDOW_KEY_W,
    WINDOW_KEY_F,
    WINDOW_KEY_M,
    WINDOW_KEY_COUNT  // Keep track of total keys
} WindowKey;

/**
 * @brief Mouse button codes
 */
typedef enum {
    WINDOW_MOUSE_LEFT = 0,
    WINDOW_MOUSE_RIGHT = 1,
    WINDOW_MOUSE_MIDDLE = 2
} WindowMouseButton;

/**
 * @brief Initialize the window system with a game instance
 * @param game Pointer to the game instance containing display settings
 * 
 * Creates the application delegate and sets up the Cocoa application.
 */
void window_init(Game* game);

/**
 * @brief Clean up window resources
 * 
 * Performs cleanup of window system resources. Should be called
 * when shutting down the application.
 */
void window_cleanup(void);

/**
 * @brief Check if the window should close
 * @return true if the window should close, false otherwise
 */
bool window_should_close(void);

/**
 * @brief Process pending window events
 * 
 * Polls and processes all pending window system events such as
 * keyboard input, mouse input, and window events.
 */
void window_poll_events(void);

/**
 * @brief Get the state of a keyboard key
 * @param key The key code to check
 * @return true if the key is currently pressed, false otherwise
 */
bool window_get_key_state(WindowKey key);

/**
 * @brief Get the current mouse position
 * @param x Pointer to store the X coordinate (can be nullptr)
 * @param y Pointer to store the Y coordinate (can be nullptr)
 */
void window_get_mouse_position(int* x, int* y);

/**
 * @brief Get the state of a mouse button
 * @param button The mouse button to check
 * @return true if the button is currently pressed, false otherwise
 */
bool window_get_mouse_button_state(int button);

/**
 * @brief Set the window title
 * @param title The new title string
 */
void window_set_title(const char* title);

/**
 * @brief Get the current window size
 * @param width Pointer to store the width (can be nullptr)
 * @param height Pointer to store the height (can be nullptr)
 */
void window_get_size(int* width, int* height);

/**
 * @brief Set the window size
 * @param width The new width in pixels
 * @param height The new height in pixels
 */
void window_set_size(int width, int height);

/**
 * @brief Set whether the window is resizable
 * @param resizable true to make the window resizable, false otherwise
 */
void window_set_resizable(bool resizable);

/**
 * @brief Present the framebuffer to the screen
 * 
 * Forces an immediate redraw of the window content. Normally
 * the window redraws automatically, but this can be used for
 * manual control.
 */
void window_present(void);
