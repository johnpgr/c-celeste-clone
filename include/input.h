#pragma once
#include "def.h"
#include "consts.h"
#include "arena.h"
#include "math3d.h"

typedef enum
{
  KEY_MOUSE_LEFT,
  KEY_MOUSE_MIDDLE,
  KEY_MOUSE_RIGHT,
  
  KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
  KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
  KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
  
  KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
  
  KEY_SPACE,
  KEY_TICK,
  KEY_MINUS,
  KEY_EQUAL,
  KEY_LEFT_BRACKET,
  KEY_RIGHT_BRACKET,
  KEY_SEMICOLON,
  KEY_QUOTE,
  KEY_COMMA,
  KEY_PERIOD,
  KEY_FORWARD_SLASH,
  KEY_BACKWARD_SLASH,
  KEY_TAB,
  KEY_ESCAPE,
  KEY_PAUSE,
  KEY_UP,
  KEY_DOWN,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_BACKSPACE,
  KEY_RETURN,
  KEY_DELETE,
  KEY_INSERT,
  KEY_HOME,
  KEY_END,
  KEY_PAGE_UP,
  KEY_PAGE_DOWN,
  KEY_CAPS_LOCK,
  KEY_NUM_LOCK,
  KEY_SCROLL_LOCK,
  KEY_MENU,
  KEY_SHIFT,
  KEY_CONTROL,
  KEY_ALT,
  KEY_COMMAND,
  
  KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, 
  KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
  
  KEY_NUMPAD_0,
  KEY_NUMPAD_1,
  KEY_NUMPAD_2,
  KEY_NUMPAD_3,
  KEY_NUMPAD_4,
  KEY_NUMPAD_5,
  KEY_NUMPAD_6,
  KEY_NUMPAD_7,
  KEY_NUMPAD_8,
  KEY_NUMPAD_9,
  
  KEY_NUMPAD_STAR,
  KEY_NUMPAD_PLUS,
  KEY_NUMPAD_MINUS,
  KEY_NUMPAD_DOT,
  KEY_NUMPAD_SLASH,
  
  KEY_COUNT = 255
} KeyCode;

typedef struct {
    bool is_down;
    bool just_pressed;
    bool just_released;
    uint8 half_transition_count;
} Key;

typedef struct {
    IVec2 screen_size;
    IVec2 mouse_pos;
    IVec2 mouse_pos_prev;
    IVec2 mouse_delta;

    IVec2 mouse_pos_world;
    IVec2 mouse_pos_world_prev;
    IVec2 mouse_delta_world;

    Key keys[KEY_COUNT];
} InputState;

static InputState* input_state;

static InputState* create_input_state(Arena* arena) {
    InputState* state = (InputState*)arena_alloc(arena, sizeof(InputState));
    memset(state, 0, sizeof(InputState));
    return state;
}

static KeyCode keycode_lookup_table[KEY_COUNT] = {};

bool key_pressed(KeyCode code) {
    Key key = input_state->keys[code];
    return key.is_down && (key.half_transition_count == 1
        || key.half_transition_count > 1);
}

bool key_released(KeyCode code) {
    Key key = input_state->keys[code];
    return !key.is_down && (key.half_transition_count == 1
        || key.half_transition_count > 1);
}

bool key_is_down(KeyCode code) {
    Key key = input_state->keys[code];
    return key.is_down;
}
