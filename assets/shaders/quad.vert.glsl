#version 460 core

struct Transform {
    ivec2 atlas_offset;
    ivec2 sprite_size;
    vec2 pos;
    vec2 size;
};

layout (std430, binding = 0) buffer SBO {
    Transform transforms[];
};

uniform vec2 screen_size;

layout (location = 0) out vec2 texture_coords_out;

void main(void) {
    Transform transform = transforms[gl_InstanceID];

    vec2 vertices[4] = {
        transform.pos,                                     // TL
        vec2(transform.pos + vec2(0.0, transform.size.y)), // BL
        vec2(transform.pos + vec2(transform.size.x, 0.0)), // TR
        transform.pos + transform.size,                    // BR
    };

    int indices[6] = int[6](0, 1, 2, 2, 1, 3);

    float left   = transform.atlas_offset.x;
    float top    = transform.atlas_offset.y;
    float right  = transform.atlas_offset.x + transform.sprite_size.x;
    float bottom = transform.atlas_offset.y + transform.sprite_size.y;

    vec2 texture_coords[4] = {
        vec2(left, top),
        vec2(left, bottom),
        vec2(right, top),
        vec2(right, bottom)
    };

    vec2 vertex_pos = vertices[indices[gl_VertexID]];
    vertex_pos.y = -vertex_pos.y + screen_size.y;
    vertex_pos = 2.0 * (vertex_pos / screen_size) - 1.0;
    gl_Position = vec4(vertex_pos, 0.0, 1.0);
    texture_coords_out = texture_coords[indices[gl_VertexID]];
}
