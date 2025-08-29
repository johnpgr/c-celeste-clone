#pragma once
#include "def.h"

typedef struct {
    char* title;
    usize fps;

    u32* display;               /* RGBA32 FORMAT */
    usize display_width;
    usize display_height;

    i16* audio;
    usize audio_sample_rate;
    usize audio_channels;
} Game;

Game game_init(void);
void game_update(void);
void game_key_up(int key);
void game_key_down(int key);
void game_display_pixel(Game* game, u32 x, u32 y, u32 value);
void game_display_debug(Game* game);
