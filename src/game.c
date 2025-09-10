#define _CRT_SECURE_NO_WARNINGS
#include "game.h"
#include "audio.h"

static bool just_pressed(GameInputType type) {
    KeyMapping mapping = game_state->key_mappings[type];

    for (usize idx = 0; idx < mapping.keys.count; idx++) {
        if (input_state->keys[array_get(mapping.keys, idx)].just_pressed) {
            return true;
        }
    }

    return false;
}

static bool is_down(GameInputType type) {
    KeyMapping mapping = game_state->key_mappings[type];
    for (usize idx = 0; idx < mapping.keys.count; idx++) {
        if (input_state->keys[array_get(mapping.keys, idx)].is_down) {
            return true;
        }
    }
    return false;
}

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
    draw_sprite(SPRITE_DICE, vec2iv2(game_state->player_position));

    if (is_down(QUIT)) {
        game_state->should_quit = true;
    }

    if (is_down(MOVE_LEFT)) {
        game_state->player_position.x -= 10;
    }
    if (is_down(MOVE_RIGHT)) {
        game_state->player_position.x += 10;
    }
    if (is_down(MOVE_UP)) {
        game_state->player_position.y -= 10;
    }
    if (is_down(MOVE_DOWN)) {
        game_state->player_position.y += 10;
    }
};
