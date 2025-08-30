#include <math.h>
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

static struct {
    u32 display[WIDTH * HEIGHT];
    i16 audio[AUDIO_CAPACITY];
} global;

static Olivec_Canvas oc = {
    .pixels = global.display,
    .width  = WIDTH,
    .height = HEIGHT,
    .stride = WIDTH,
};

Game game_init(void) {
    return (Game){
        .title             = (char*)TITLE,
        .fps               = FPS,
        .display           = global.display,
        .display_width     = WIDTH,
        .display_height    = HEIGHT,
        .audio             = global.audio,
        .audio_sample_rate = AUDIO_SAMPLE_RATE,
        .audio_channels    = AUDIO_CHANNELS,
    };
};

void game_update_and_render() {
    // clear the pixels
    for (usize i = 0; i < ARRAY_LEN(global.display); i++) {
        global.display[i] = RGBA(0, 0, 0, 255);
    }

    static float angle = 0.0f;
    usize vert_count = 3;
    float cx = WIDTH / 2.0f;
    float cy = HEIGHT / 2.0f;
    float dangle = 2 * PI / vert_count;
    float mag = cx / 2;

    olivec_triangle3c(
        oc,
        cx + cosf(dangle * 0 + angle) * mag, cy + sinf(dangle * 0 + angle) * mag,
        cx + cosf(dangle * 1 + angle) * mag, cy + sinf(dangle * 1 + angle) * mag,
        cx + cosf(dangle * 2 + angle) * mag, cy + sinf(dangle * 2 + angle) * mag,
        RGBA(255, 0, 0, 255),
        RGBA(0, 255, 0, 255),
        RGBA(0, 0, 255, 255)
    );
    angle += 2 * PI * DELTA_TIME;
};

void game_key_up([[maybe_unused]] int key) {
    TODO("Implement this");
};

void game_key_down([[maybe_unused]] int key) {
    TODO("Implement this");
};
