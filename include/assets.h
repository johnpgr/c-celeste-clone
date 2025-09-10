#pragma once
#include "def.h"

typedef enum {
    SPRITE_DICE,
    SPRITE_COUNT,
} SpriteID;

typedef struct {
    IVec2 atlas_offset;
    IVec2 size;
} Sprite;

Sprite get_sprite(SpriteID sprite_id) {
    Sprite sprite = {};

    switch (sprite_id) {
        case SPRITE_DICE:
            sprite.atlas_offset = ivec2(0, 0);
            sprite.size = ivec2(16, 16);
            break;
        case SPRITE_COUNT:
            assert(false && "SPRITE_COUNT should never be used as a value of get_sprite");
    }

    return sprite;
}
