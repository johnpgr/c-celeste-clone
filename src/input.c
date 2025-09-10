#include "input.h"

bool key_pressed(KeyCode code) {
    assert(input_state != nullptr);
    Key key = input_state->keys[code];
    return key.is_down && key.half_transition_count == 1
        || key.half_transition_count > 1;
}

bool key_released(KeyCode code) {
    assert(input_state != nullptr);
    Key key = input_state->keys[code];
    return !key.is_down && key.half_transition_count == 1
        || key.half_transition_count > 1;
}

bool key_is_down(KeyCode code) {
    assert(input_state != nullptr);
    Key key = input_state->keys[code];
    return key.is_down;
}