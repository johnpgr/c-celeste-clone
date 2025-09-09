#pragma once
#include "assets.h"
#include "game.h"
#include "glad/glad.h"

#define MAX_TRANSFORMS 1000

typedef struct {
    real32 zoom;
    Vec2 dimensions;
    Vec2 position;
} OrthographicCamera2D;

typedef struct {
    IVec2 atlas_offset;
    IVec2 sprite_size;
    Vec2 pos;
    Vec2 size;
} Transform;

bool gl_init(Game* game);
void gl_render(Game* game);
void gl_cleanup(void);

void draw_sprite(SpriteID sprite_id, Vec2 pos);
