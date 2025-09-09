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

Sprite get_sprite(SpriteID sprite_id);
