#include <math.h>
#include <stdio.h>
#include "def.h"
#include "game.h"

#define OLIVEC_IMPLEMENTATION
#include "olive.c"

constexpr char TITLE[]          = "The game";
constexpr int FPS               = 60;
constexpr float DELTA_TIME      = 1.0f/FPS;
constexpr int WIDTH             = 800;
constexpr int HEIGHT            = 600;
constexpr int AUDIO_SAMPLE_RATE = 44100;
constexpr int AUDIO_CHANNELS    = 2;
constexpr int AUDIO_CAPACITY    = (AUDIO_SAMPLE_RATE/FPS * AUDIO_CHANNELS);

static_assert(AUDIO_SAMPLE_RATE % FPS == 0);

static u32 global_display[WIDTH * HEIGHT];
static i16 global_audio[AUDIO_CAPACITY];

static Olivec_Canvas oc = {
    .pixels = global_display,
    .width  = WIDTH,
    .height = HEIGHT,
    .stride = WIDTH,
};

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

static void game_display_draw() {
    // clear the pixels
    for (usize i = 0; i < ARRAY_LEN(global_display); i++) {
        global_display[i] = RGBA(0,0,0,255);
    }


    static float angle = 0.0f;
    usize vert_count = 3;
    float cx = WIDTH / 2.0f;
    float cy = HEIGHT / 2.0f;
    float dangle = 2 * M_PI / vert_count;
    float mag = cx / 2;

    olivec_triangle3c(
        oc,
        cx + cosf(dangle * 0 + angle) * mag, cy + sinf(dangle * 0 + angle) * mag,
        cx + cosf(dangle * 1 + angle) * mag, cy + sinf(dangle * 1 + angle) * mag,
        cx + cosf(dangle * 2 + angle) * mag, cy + sinf(dangle * 2 + angle) * mag,
        RGBA(255,0,0,255),
        RGBA(0,255,0,255),
        RGBA(0,0,255,255)
    );
    angle += 2*M_PI*DELTA_TIME;
}

static void game_audio_playback() {
    static double audio_time = 0.0;
    double tone_hz = 440.0;
    double sine_step = 2.0 * M_PI * tone_hz / AUDIO_SAMPLE_RATE;

    for (usize i = 0; i < AUDIO_CAPACITY / AUDIO_CHANNELS; i++) {
        float samplef = sin(audio_time) * 0.1f;
        i16 samplei16 = (i16)(samplef * 32767.0f);

        global_audio[i * AUDIO_CHANNELS + 0] = samplei16;
        global_audio[i * AUDIO_CHANNELS + 1] = samplei16;

        audio_time += sine_step;
    }
}

void game_update() {
    game_display_draw();
    game_audio_playback();
};

void game_key_up([[maybe_unused]] int key) {
    TODO("Implement this");
};

void game_key_down([[maybe_unused]] int key) {
    TODO("Implement this");
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
