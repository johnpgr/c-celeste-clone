#define _CRT_SECURE_NO_WARNINGS
#include "game.h"
#include "audio.h"

void game_update(
    GameState* game_state_in,
    RendererState* renderer_state_in,
    InputState* input_state_in,
    AudioState* audio_state_in
) {
    if (game_state_in != game_state) {
        game_state = game_state_in;
        renderer_state = renderer_state_in;
        input_state = input_state_in;
        audio_state = audio_state_in;
    }

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
