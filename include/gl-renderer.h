#pragma once
#include "assets.h"
#include "glad/glad.h"
#include "vector.h"

#define MAX_TRANSFORMS 1000

typedef struct {
    IVec2 atlas_offset;
    IVec2 sprite_size;
    Vec2 pos;
    Vec2 size;
} Transform;

bool gl_init(void);
void gl_cleanup(void);
void gl_render(int window_width, int window_height);

void draw_sprite(SpriteID sprite_id, Vec2 pos, Vec2 size);
