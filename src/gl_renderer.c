#include "gl_renderer.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "game.h"

static struct {
    GLuint program;
    GLuint vert_shader;
    GLuint frag_shader;
    GLuint texture;
    GLuint VAO;
    GLuint SBO;
    GLuint screen_size;
    GLuint camera_matrix;
} GLContext;

static struct {
    OrthographicCamera2D game_camera;
    OrthographicCamera2D ui_camera;
    usize transform_count;
    Transform transforms[MAX_TRANSFORMS];
} RendererState;

internal void APIENTRY gl_debug_callback(
    [[maybe_unused]] GLenum source,
    [[maybe_unused]] GLenum type,
    [[maybe_unused]] GLuint id,
    GLenum severity,
    [[maybe_unused]] GLsizei length,
    const GLchar* message,
    [[maybe_unused]] const void* user
){
    bool is_error = severity == GL_DEBUG_SEVERITY_LOW
        || severity == GL_DEBUG_SEVERITY_MEDIUM
        || severity == GL_DEBUG_SEVERITY_HIGH;

    if (is_error) {
        debug_print("OpenGL Error: %s\n", message);
        return;
    }

    debug_print("%s\n", message);
}

internal GLuint create_shader(
    GLenum shader_type,
    const char* source,
    usize source_size
) {
    GLuint shader = glCreateShader(shader_type);
    
    glShaderSource(shader, 1, (const GLchar*[]){source}, (GLint*)&source_size);
    glCompileShader(shader);
    
    int success;
    char shader_log[1024];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    
    if (!success) {
        glGetShaderInfoLog(shader, 1024, 0, shader_log);
        const char* shader_type_str = (shader_type == GL_VERTEX_SHADER) ? "Vertex" : "Fragment";
        debug_print("Failed to compile %s Shader: %s", shader_type_str, shader_log);
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

internal GLuint load_texture(const uint8* png_data, usize png_size) {
    GLuint texture;
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);

    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, channels;
    uint8* image = stbi_load_from_memory(png_data, png_size, &width, &height, &channels, 4);

    if (!image) {
        debug_print("Failed to load texture from PNG data\n");
        glDeleteTextures(1, &texture);
        return 0;
    }

    glTextureStorage2D(texture, 1, GL_RGBA8, width, height);
    glTextureSubImage2D(texture, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, image);

    stbi_image_free(image);

    return texture;
}

bool gl_init(Game* game) {
    glDebugMessageCallback(&gl_debug_callback, nullptr);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glEnable(GL_DEBUG_OUTPUT);

    static char vert_shader_source[] = {
        #embed "assets/shaders/quad.vert.glsl"
    };
    int vert_shader_size = sizeof(vert_shader_source);

    static char frag_shader_source[] = {
        #embed "assets/shaders/quad.frag.glsl"
    };
    int frag_shader_size = sizeof(frag_shader_source);

    GLContext.vert_shader = create_shader(
        GL_VERTEX_SHADER,
        vert_shader_source,
        vert_shader_size
    );
    GLContext.frag_shader = create_shader(
        GL_FRAGMENT_SHADER,
        frag_shader_source,
        frag_shader_size
    );

    if (!GLContext.vert_shader || !GLContext.frag_shader) {
        return false;
    }

    GLContext.program = glCreateProgram();
    glAttachShader(GLContext.program, GLContext.vert_shader);
    glAttachShader(GLContext.program, GLContext.frag_shader);
    glLinkProgram(GLContext.program);

    glDetachShader(GLContext.program, GLContext.vert_shader);
    glDetachShader(GLContext.program, GLContext.frag_shader);
    glDeleteShader(GLContext.vert_shader);
    glDeleteShader(GLContext.frag_shader);

    glCreateVertexArrays(1, &GLContext.VAO);
    glBindVertexArray(GLContext.VAO);

    static uint8 texture_atlas_source[] = {
        #embed "assets/images/TEXTURE_ATLAS.png"
    };
    usize texture_atlas_size = sizeof(texture_atlas_source);

    GLContext.texture = load_texture(texture_atlas_source, texture_atlas_size);
    glBindTextureUnit(0, GLContext.texture);

    glCreateBuffers(1, &GLContext.SBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, GLContext.SBO);
    glNamedBufferData(
        GLContext.SBO,
        sizeof(Transform) * MAX_TRANSFORMS,
        RendererState.transforms,
        GL_DYNAMIC_DRAW
    );

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);

    glUseProgram(GLContext.program);

    GLContext.screen_size = glGetUniformLocation(GLContext.program, "screen_size");
    GLContext.camera_matrix = glGetUniformLocation(GLContext.program, "camera_matrix");

    RendererState.game_camera.zoom = 0.0;
    RendererState.game_camera.dimensions = vec2(WORLD_WIDTH, WORLD_HEIGHT);
    RendererState.game_camera.position = game->camera_position;
    RendererState.ui_camera = RendererState.game_camera;

    return true;
}

void gl_render(Game* game) {
    RendererState.game_camera.position = game->camera_position;

    glClearColor(RGBA(181, 101, 174, 255));
    glClearDepth(0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, game->window_width, game->window_height);
    glUniform2fv(
        GLContext.screen_size,
        1,
        (real32[]){game->window_width, game->window_height}
    );

    OrthographicCamera2D camera = RendererState.game_camera;
    Mat4x4 camera_matrix = create_orthographic(
        camera.position.x - camera.dimensions.x / 2.0,
        camera.position.x + camera.dimensions.x / 2.0,
        camera.position.y - camera.dimensions.y / 2.0,
        camera.position.y + camera.dimensions.y / 2.0
    );
    glUniformMatrix4fv(GLContext.camera_matrix, 1, GL_FALSE, &camera_matrix.ax);

    glNamedBufferSubData(
        GLContext.SBO,
        0,
        sizeof(Transform) * RendererState.transform_count,
        RendererState.transforms
    );

    glDrawArraysInstanced(
        GL_TRIANGLES,
        0,
        6,
        RendererState.transform_count
    );

    RendererState.transform_count = 0;
}

void gl_cleanup() {
    glDeleteProgram(GLContext.program);
    glDeleteVertexArrays(1, &GLContext.VAO);
    glDeleteBuffers(1, &GLContext.SBO);
    glDeleteTextures(1, &GLContext.texture);
}

void draw_sprite(SpriteID sprite_id, Vec2 pos) {
    assert(RendererState.transform_count + 1 <= MAX_TRANSFORMS
           && "Max transform count reached");

    Sprite sprite = get_sprite(sprite_id);

    Vec2 sprite_size = vec2iv2(sprite.size);

    Transform transform = {
        .atlas_offset = sprite.atlas_offset,
        .sprite_size = sprite.size,
        .pos = vec2_div(vec2_minus(pos, sprite_size), 2.0f),
        .size = sprite_size,
    };

    RendererState.transforms[RendererState.transform_count++] = transform;
}
