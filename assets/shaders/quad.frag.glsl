#version 460 core

layout (location = 0) in vec2 texture_coords_in;
layout (location = 0) out vec4 frag_color;
layout (location = 0) uniform sampler2D texture_atlas;

void main(void) {
    vec4 texture_color = texelFetch(texture_atlas, ivec2(texture_coords_in), 0);

    if (texture_color.a == 0.0) {
        discard;
    }

    frag_color = texture_color;
}
