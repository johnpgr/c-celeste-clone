#include "glad/glad.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "renderer.h"
#include "input.h"

#ifdef _WIN32
#include <windows.h>
typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);
static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;

internal bool gl_platform_init_vsync() {
    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
    if (wglSwapIntervalEXT) {
        debug_print("  WGL_EXT_swap_control extension loaded\n");
        return true;
    }
    debug_print("  Warning: WGL_EXT_swap_control extension not available\n");
    return false;
}

internal bool gl_platform_set_vsync(bool enable) {
    if (wglSwapIntervalEXT) {
        BOOL result = wglSwapIntervalEXT(enable ? 1 : 0);
        return result == TRUE;
    }
    return false;
}

#elif defined(__linux__)
#include <GL/glx.h>
#include <dlfcn.h>

typedef int (*PFNGLXSWAPINTERVALEXTPROC)(Display *dpy, GLXDrawable drawable, int interval);
typedef int (*PFNGLXSWAPINTERVALMESAPROC)(int interval);
typedef int (*PFNGLXSWAPINTERVALSGIPROC)(int interval);

static PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = NULL;
static PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA = NULL;
static PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI = NULL;
static Display* display = NULL;
static GLXDrawable drawable = 0;

internal bool gl_platform_init_vsync() {
    display = glXGetCurrentDisplay();
    drawable = glXGetCurrentDrawable();
    
    if (!display || !drawable) {
        debug_print("  Warning: Could not get current GLX display/drawable\n");
        return false;
    }

    // Try EXT extension first (most flexible)
    glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddress((const GLubyte*)"glXSwapIntervalEXT");
    if (glXSwapIntervalEXT) {
        debug_print("  GLX_EXT_swap_control extension loaded\n");
        return true;
    }

    // Try MESA extension
    glXSwapIntervalMESA = (PFNGLXSWAPINTERVALMESAPROC)glXGetProcAddress((const GLubyte*)"glXSwapIntervalMESA");
    if (glXSwapIntervalMESA) {
        debug_print("  GLX_MESA_swap_control extension loaded\n");
        return true;
    }

    // Try SGI extension (oldest, least flexible)
    glXSwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC)glXGetProcAddress((const GLubyte*)"glXSwapIntervalSGI");
    if (glXSwapIntervalSGI) {
        debug_print("  GLX_SGI_swap_control extension loaded\n");
        return true;
    }

    debug_print("  Warning: No VSync extensions available on Linux\n");
    return false;
}

internal bool gl_platform_set_vsync(bool enable) {
    int interval = enable ? 1 : 0;
    
    if (glXSwapIntervalEXT && display && drawable) {
        glXSwapIntervalEXT(display, drawable, interval);
        return true;
    }
    
    if (glXSwapIntervalMESA) {
        int result = glXSwapIntervalMESA(interval);
        return result == 0; // 0 means success for MESA
    }
    
    if (glXSwapIntervalSGI && interval >= 0) {
        int result = glXSwapIntervalSGI(interval);
        return result == 0; // 0 means success for SGI
    }
    
    return false;
}

#elif defined(__APPLE__)
#include <OpenGL/OpenGL.h>

internal bool gl_platform_init_vsync() {
    // macOS Core OpenGL always supports VSync control
    debug_print("  macOS Core OpenGL VSync support available\n");
    return true;
}

internal bool gl_platform_set_vsync(bool enable) {
    CGLContextObj context = CGLGetCurrentContext();
    if (!context) {
        debug_print("  Warning: No current OpenGL context on macOS\n");
        return false;
    }
    
    GLint sync = enable ? 1 : 0;
    CGLError error = CGLSetParameter(context, kCGLCPSwapInterval, &sync);
    
    if (error != kCGLNoError) {
        debug_print("  Error setting VSync on macOS: %d\n", error);
        return false;
    }
    
    return true;
}

#else
// Default implementation for unsupported platforms
internal bool gl_platform_init_vsync() {
    debug_print("  Warning: VSync not implemented for this platform\n");
    return false;
}

internal bool gl_platform_set_vsync(bool enable) {
    debug_print("  Warning: VSync not supported on this platform\n");
    return false;
}
#endif

internal struct {
    InputState* input_state;
    RendererState* renderer_state;

    GLuint program;
    GLuint vert_shader;
    GLuint frag_shader;
    GLuint texture;
    GLuint VAO;
    GLuint SBO;
    GLuint screen_size;
    GLuint camera_matrix;

    bool vsync_supported;
} gl_context;

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

bool renderer_init(InputState* input_state, RendererState* renderer_state) {
    gl_context.input_state = input_state;
    gl_context.renderer_state = renderer_state;

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

    gl_context.vert_shader = create_shader(
        GL_VERTEX_SHADER,
        vert_shader_source,
        vert_shader_size
    );
    gl_context.frag_shader = create_shader(
        GL_FRAGMENT_SHADER,
        frag_shader_source,
        frag_shader_size
    );

    if (!gl_context.vert_shader || !gl_context.frag_shader) {
        return false;
    }

    gl_context.program = glCreateProgram();
    glAttachShader(gl_context.program, gl_context.vert_shader);
    glAttachShader(gl_context.program, gl_context.frag_shader);
    glLinkProgram(gl_context.program);

    glDetachShader(gl_context.program, gl_context.vert_shader);
    glDetachShader(gl_context.program, gl_context.frag_shader);
    glDeleteShader(gl_context.vert_shader);
    glDeleteShader(gl_context.frag_shader);

    glCreateVertexArrays(1, &gl_context.VAO);
    glBindVertexArray(gl_context.VAO);

    static uint8 texture_atlas_source[] = {
        #embed "assets/images/TEXTURE_ATLAS.png"
    };
    usize texture_atlas_size = sizeof(texture_atlas_source);

    gl_context.texture = load_texture(texture_atlas_source, texture_atlas_size);
    glBindTextureUnit(0, gl_context.texture);

    glCreateBuffers(1, &gl_context.SBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gl_context.SBO);
    glNamedBufferData(
        gl_context.SBO,
        sizeof(Transform) * MAX_TRANSFORMS,
        gl_context.renderer_state->transforms,
        GL_DYNAMIC_DRAW
    );

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);

    glUseProgram(gl_context.program);

    gl_context.screen_size = glGetUniformLocation(gl_context.program, "screen_size");
    gl_context.camera_matrix = glGetUniformLocation(gl_context.program, "camera_matrix");

    gl_context.vsync_supported = gl_platform_init_vsync();
    if (gl_context.vsync_supported) {
        gl_platform_set_vsync(false);
    }

    return true;
}

void renderer_render() {
    glClearColor(RGBA(181, 101, 174, 255));
    glClearDepth(0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, gl_context.input_state->screen_size.x, gl_context.input_state->screen_size.y);
    glUniform2fv(
        gl_context.screen_size,
        1,
        (real32[]){gl_context.input_state->screen_size.x, gl_context.input_state->screen_size.y}
    );

    OrthographicCamera2D camera = gl_context.renderer_state->game_camera;
    Mat4x4 camera_matrix = create_orthographic(
        camera.position.x - camera.dimensions.x / 2.0,
        camera.position.x + camera.dimensions.x / 2.0,
        camera.position.y - camera.dimensions.y / 2.0,
        camera.position.y + camera.dimensions.y / 2.0
    );
    glUniformMatrix4fv(gl_context.camera_matrix, 1, GL_FALSE, &camera_matrix.ax);

    glNamedBufferSubData(
        gl_context.SBO,
        0,
        sizeof(Transform) * gl_context.renderer_state->transform_count,
        gl_context.renderer_state->transforms
    );

    glDrawArraysInstanced(
        GL_TRIANGLES,
        0,
        6,
        gl_context.renderer_state->transform_count
    );

    gl_context.renderer_state->transform_count = 0;
}

void renderer_set_vsync(bool enable) {
    if (gl_context.vsync_supported) {
        if (!gl_platform_set_vsync(enable)) {
            debug_print("  Warning: Failed to set VSync state\n");
        }
    } else {
        debug_print("  Warning: VSync not supported on this platform\n");
    }
}

void renderer_cleanup() {
    glDeleteProgram(gl_context.program);
    glDeleteVertexArrays(1, &gl_context.VAO);
    glDeleteBuffers(1, &gl_context.SBO);
    glDeleteTextures(1, &gl_context.texture);
}

void draw_sprite(SpriteID sprite_id, Vec2 pos) {
    assert(gl_context.renderer_state->transform_count + 1 <= MAX_TRANSFORMS
           && "Max transform count reached");

    Sprite sprite = get_sprite(sprite_id);

    Vec2 sprite_size = vec2iv2(sprite.size);

    Transform transform = {
        .atlas_offset = sprite.atlas_offset,
        .sprite_size = sprite.size,
        .pos = vec2_div(vec2_minus(pos, sprite_size), 2.0f),
        .size = sprite_size,
    };

    gl_context.renderer_state->transforms[gl_context.renderer_state->transform_count++] = transform;
}
