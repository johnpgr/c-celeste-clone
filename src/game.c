#include "def.h"
#include "game.h"
#include <stdio.h>

constexpr char TITLE[]          = "Software rendering in C";
constexpr int FPS               = 60;
constexpr int WIDTH             = 800;
constexpr int HEIGHT            = 600;
constexpr int AUDIO_SAMPLE_RATE = 44100;
constexpr int AUDIO_CHANNELS    = 2;
constexpr int AUDIO_CAPACITY    = (AUDIO_SAMPLE_RATE/FPS * AUDIO_CHANNELS);

static_assert(AUDIO_SAMPLE_RATE % FPS == 0);

static u32 global_display[WIDTH * HEIGHT] = {0};
static i16 global_audio[AUDIO_CAPACITY]   = {0};

Game game_init(void) {
    return (Game){
        .title             = (char*)TITLE,
        .fps               = FPS,
        .display           = global_display,
        .display_width     = WIDTH,
        .display_height    = HEIGHT,
        .audio             = global_audio,
        .audio_sample_rate = AUDIO_SAMPLE_RATE,
        .audio_channels    = AUDIO_CHANNELS,
    };
};

void game_update(void) {};
void game_key_up(int key) {
    (void)key;
};
void game_key_down(int key) {
    (void)key;
};

void game_display_pixel(Game* game, u32 x, u32 y, u32 value) {
    game->display[y * WIDTH + x] = value;
}

void game_display_debug(Game* game) {
    FILE* file = fopen("output.ppm", "w");
    if (!file) {
        printf("Error: Could not create output file\n");
        return;
    }
    
    // PPM header
    fprintf(file, "P3\n");
    fprintf(file, "%d %d\n", WIDTH, HEIGHT);
    fprintf(file, "255\n");
    
    // Write pixel data
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            u32 pixel = game->display[y * WIDTH + x];
            u32 r     = (pixel >> 16) & 0xFF;
            u32 g     = (pixel >> 8) & 0xFF;
            u32 b     = pixel & 0xFF;
            fprintf(file, "%u %u %u ", r, g, b);
        }
        fprintf(file, "\n");
    }
    
    fclose(file);
    printf("Display written to output.ppm\n");
}
