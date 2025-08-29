#pragma once
#include "game.h"

void window_init(Game* game); /* Initialize the window system with a game instance */
void window_run(void); /* Start the main window loop (blocks until window is closed) */
