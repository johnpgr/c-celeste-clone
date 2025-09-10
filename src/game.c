#include <string.h>
#include "game.h"
#include "utils.h"
#include "audio.h"

void game_update(
    GameState* game_state,
    RendererState* renderer_state,
    InputState* input_state,
    AudioState* audio_state
) {
    game_state->camera_position.x = 160;
    game_state->camera_position.y = 90;
    draw_sprite(SPRITE_DICE, vec2(0, 0));
};

void game_key_up([[maybe_unused]] int key) {
    TODO("Implement this");
};

void game_key_down([[maybe_unused]] int key) {
    TODO("Implement this");
};
