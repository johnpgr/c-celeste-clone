/**
 * TODO:
 * - Save game locations
 * - Threading (launch a thread)
 * - Fullscreen support
 * - WM_SETCURSOR (control cursor visibility)
*/
#pragma once
#include "def.h"
#include "arena.h"
#include "audio.h"
#include "input.h"
#include "math3d.h"
#include "renderer.h"
#include "array.h"

typedef enum {
    MOVE_LEFT,
    MOVE_RIGHT,
    MOVE_UP,
    MOVE_DOWN,
    JUMP,

    MOUSE1,
    MOUSE2,

    QUIT,

    GAME_INPUT_COUNT
} GameInputType;

typedef struct {
    ARRAY(KeyCode, 3) keys;
} KeyMapping;

typedef struct {
    int neighbour_mask;
    bool is_visible;
} Tile;

typedef struct {
    bool should_quit;
    bool fps_cap;
    IVec2 player_position;

    Tile world_grid[WORLD_GRID.x][WORLD_GRID.y];
    KeyMapping key_mappings[GAME_INPUT_COUNT];
} GameState;

static GameState* game_state;

static void init_key_mappings(GameState* state) {
    array_push(state->key_mappings[MOVE_UP].keys, KEY_W);
    array_push(state->key_mappings[MOVE_UP].keys, KEY_UP);
    array_push(state->key_mappings[MOVE_LEFT].keys, KEY_A);
    array_push(state->key_mappings[MOVE_LEFT].keys, KEY_LEFT);
    array_push(state->key_mappings[MOVE_DOWN].keys, KEY_S);
    array_push(state->key_mappings[MOVE_DOWN].keys, KEY_DOWN);
    array_push(state->key_mappings[MOVE_RIGHT].keys, KEY_D);
    array_push(state->key_mappings[MOVE_RIGHT].keys, KEY_RIGHT);
    array_push(state->key_mappings[MOUSE1].keys, KEY_MOUSE_LEFT);
    array_push(state->key_mappings[MOUSE2].keys, KEY_MOUSE_RIGHT);
    array_push(state->key_mappings[QUIT].keys, KEY_ESCAPE);
}

static GameState* create_game_state(Arena* arena) {
    GameState* state = (GameState*)arena_alloc(arena, sizeof(GameState));
    memset(state, 0, sizeof(GameState));

    state->fps_cap = true;
    state->player_position = ivec2(0, 0);
    for (int i = 0; i < GAME_INPUT_COUNT; i++) {
        state->key_mappings[i].keys.capacity = 3;
    }

    init_key_mappings(state);

    return state;
}

EXPORT_FN void game_update(GameState* game_state, RendererState* renderer_state, InputState* input_state, AudioState* audio_state);
