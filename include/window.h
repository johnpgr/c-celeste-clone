#pragma once
#include "input.h"
#include "renderer.h"

/**
 * @brief Initialize the window system with a game instance
 * @param game Pointer to the game instance containing display settings
 * 
 * Creates the application delegate and sets up the Cocoa application.
 */
void window_init(InputState* input_state, RendererState* renderer_state);

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

/**
 * @brief Show the window
 * 
 * Makes the window visible if it was previously hidden or minimized.
 */
void window_show(void);
