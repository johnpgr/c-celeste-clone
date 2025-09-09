#include "assets.h"

Sprite get_sprite(SpriteID sprite_id) {
    Sprite sprite = {};

    switch (sprite_id) {
        case SPRITE_DICE:
            sprite.atlas_offset = (IVec2){0, 0};
            sprite.size = (IVec2){16, 16};
            break;
        case SPRITE_COUNT:
            assert(false && "SPRITE_COUNT should never be used as a value of get_sprite");
    }

    return sprite;
}
