#pragma once
#include "def.h"
#include "assets.h"
#include "input.h"
#include "consts.h"
#include "arena.h"

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

typedef struct {
    OrthographicCamera2D game_camera;
    OrthographicCamera2D ui_camera;

    usize transform_count;
    Transform transforms[MAX_TRANSFORMS];
} RendererState;

static RendererState* renderer_state;

static RendererState* create_renderer_state(Arena* arena) {
    RendererState* state = (RendererState*)arena_alloc(arena, sizeof(RendererState));
    if (state) {
        memset(state, 0, sizeof(RendererState));
        state->game_camera.zoom = 1.0f;
        state->game_camera.dimensions = (Vec2){ WORLD_WIDTH * SCALE, WORLD_HEIGHT * SCALE };
        state->game_camera.position = (Vec2){ 0.0f, 0.0f };

        state->ui_camera.zoom = 1.0f;
        state->ui_camera = state->game_camera;
    }
    return state;
}

static void draw_sprite(SpriteID sprite_id, Vec2 pos) {
    assert(renderer_state->transform_count + 1 <= MAX_TRANSFORMS
           && "Max transform count reached");

    Sprite sprite = get_sprite(sprite_id);

    Vec2 sprite_size = vec2_mult(vec2iv2(sprite.size), SCALE);

    Transform transform = {
        .atlas_offset = sprite.atlas_offset,
        .sprite_size = sprite.size,
        .pos = vec2_div(vec2_minus(pos, sprite_size), 2.0f),
        .size = sprite_size,
    };

    renderer_state->transforms[renderer_state->transform_count++] = transform;
}

// Functions provided by the platform renderer
bool renderer_init(InputState* input_state, RendererState* renderer_state);
void renderer_set_vsync(bool enable);
void renderer_cleanup(void);
void renderer_render();
