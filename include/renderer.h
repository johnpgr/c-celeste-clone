#pragma once
#include "def.h"
#include "assets.h"
#include "consts.h"
#include "arena.h"
#include "array.h"
#include "input.h"

typedef struct {
    real32 zoom;
    Vec2 dimensions;
    Vec2 position;
} OrthographicCamera2D;

typedef struct {
    Vec2 pos;
    Vec2 size;
    IVec2 atlas_offset;
    IVec2 sprite_size;
} Transform;

typedef struct {
    OrthographicCamera2D game_camera;
    OrthographicCamera2D ui_camera;

    ARRAY(Transform, 1000) transforms;
} RendererState;

static RendererState* renderer_state;

static RendererState* create_renderer_state(Arena* arena) {
    RendererState* state = (RendererState*)arena_alloc(arena, sizeof(RendererState));
    if (state) {
        memset(state, 0, sizeof(RendererState));
        state->transforms.capacity = 1000;
        state->game_camera.zoom = 1.0f;
        state->game_camera.dimensions = vec2(WORLD_WIDTH, WORLD_HEIGHT);
        state->game_camera.position = vec2(160, -90);

        state->ui_camera.zoom = 1.0f;
        state->ui_camera = state->game_camera;
    }
    return state;
}

static IVec2 screen_to_world(IVec2 screen_pos) {
    OrthographicCamera2D camera = renderer_state->game_camera;

    int x = (real32)screen_pos.x / 
            (real32)input_state->screen_size.x * 
            camera.dimensions.x; // [0; dimensions.x]

    // Offset using dimensions and position
    x += -camera.dimensions.x / 2.0f + camera.position.x;

    int y = (real32)screen_pos.y / 
            (real32)input_state->screen_size.y * 
            camera.dimensions.y; // [0; dimensions.y]

    // Offset using dimensions and position
    y += camera.dimensions.y / 2.0f + camera.position.y;

    return (IVec2){x, y};
}

static void draw_sprite(SpriteID sprite_id, Vec2 pos) {
    Sprite sprite = get_sprite(sprite_id);

    Transform transform = {};
    transform.pos = vec2_minus(pos, vec2_div(vec2iv2(sprite.size), 2.0f));
    transform.size = vec2iv2(sprite.size);
    transform.atlas_offset = sprite.atlas_offset;
    transform.sprite_size = sprite.size;

    array_push(renderer_state->transforms, transform);
}

static void draw_quad(Vec2 pos, Vec2 size) {
    Transform transform = {
        .pos = vec2_minus(pos, vec2_div(size, 2.0f)),
        .size = size,
        .atlas_offset = ivec2(0, 0),
        .sprite_size = ivec2(1, 1),
    };

    array_push(renderer_state->transforms, transform);
}

// Functions provided by the platform renderer
bool renderer_init();
void renderer_set_vsync(bool enable);
void renderer_cleanup(void);
void renderer_render();
