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
#include "renderer.h"

typedef enum {
    MOVE_LEFT,
    MOVE_RIGHT,
    MOVE_UP,
    MOVE_DOWN,
    JUMP,

    MOUSE_LEFT,
    MOUSE_RIGHT,

    GAME_INPUT_COUNT
} GameInputType;

typedef struct {
    bool fps_cap;
    Vec2 camera_position;
    IVec2 player_position;
} GameState;

static GameState* game_state;

static GameState* create_game_state(Arena* arena) {
    GameState* state = (GameState*)arena_alloc(arena, sizeof(GameState));
    memset(state, 0, sizeof(GameState));
    state->fps_cap = true;
    state->camera_position = (Vec2){0.0f, 0.0f};
    state->player_position = (IVec2){0, 0};
    return state;
}

export void game_update(GameState* game_state, RendererState* renderer_state, InputState* input_state, AudioState* audio_state);
