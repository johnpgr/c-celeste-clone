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

static Tile* get_tile(int x, int y) {
    Tile* tile = nullptr;

    if (x >= 0 && x < WORLD_GRID.x && y >= 0 && y < WORLD_GRID.y) {
        tile = &game_state->world_grid[x][y];
    }

    return tile;
}

static Tile* get_tile_iv2(IVec2 world_pos) {
    int x = world_pos.x / TILESIZE;
    int y = world_pos.y / TILESIZE;

    return get_tile(x, y);
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
    if (is_down(MOUSE1)) {
        IVec2 mouse_pos_world = input_state->mouse_pos_world;
        Tile* tile = get_tile_iv2(mouse_pos_world);
        if (tile) {
            tile->is_visible = true;
        }
    }
    if (is_down(MOUSE2)) {
        IVec2 mouse_pos_world = input_state->mouse_pos_world;
        Tile* tile = get_tile_iv2(mouse_pos_world);
        if (tile) {
            tile->is_visible = false;
        }
    }
    
    for (int y = 0; y < WORLD_GRID.y; y++) {
        for (int x = 0; x < WORLD_GRID.x; x++) {
            Tile* tile = get_tile(x, y);

            if (!tile->is_visible) { continue; };

            Vec2 tile_pos = vec2(x * (real32)TILESIZE + (real32)TILESIZE / 2.0f,
                                 y * (real32)TILESIZE + (real32)TILESIZE / 2.0f);
            draw_quad(tile_pos, vec2(TILESIZE, TILESIZE));
        }
    }
};
