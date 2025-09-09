#include <windows.h>
#include <windowsx.h>
#include "game.h"
#include "window.h"
#include "glad/glad.h"

typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);

static struct {
    HWND hwnd;
    HDC hdc;
    HGLRC hglrc;
    int32 client_width;
    int32 client_height;
    Game* game;
    bool should_close;
    bool is_resizable;
    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
} WIN32Window;

/**
 * @brief Input state tracking structure
 */
static struct {
    bool keys[WINDOW_KEY_COUNT];
    bool mouse_buttons[3];
    int32 mouse_x, mouse_y;
} WIN32Input;

internal void init_vsync_extension() {
    WIN32Window.wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
    
    if (WIN32Window.wglSwapIntervalEXT) {
        debug_print("  WGL_EXT_swap_control extension loaded\n");
        WIN32Window.wglSwapIntervalEXT(0);
        debug_print("  VSync disabled\n");
    } else {
        debug_print("  Warning: WGL_EXT_swap_control extension not available\n");
    }
}

internal bool init_opengl_context() {
    debug_print("  Initializing OpenGL context.\n");

    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,                     // version number
        PFD_DRAW_TO_WINDOW |   // support window
        PFD_SUPPORT_OPENGL |   // support OpenGL
        PFD_DOUBLEBUFFER,      // double buffered
        PFD_TYPE_RGBA,         // RGBA type
        32,                    // 32-bit color depth
        0, 0, 0, 0, 0, 0,      // color bits ignored
        0,                     // no alpha buffer
        0,                     // shift bit ignored
        0,                     // no accumulation buffer
        0, 0, 0, 0,            // accum bits ignored
        24,                    // 24-bit z-buffer
        8,                     // 8-bit stencil buffer
        0,                     // no auxiliary buffer
        PFD_MAIN_PLANE,        // main drawing layer
        0,                     // reserved
        0, 0, 0                // layer masks ignored
    };

    int32 pixel_format = ChoosePixelFormat(WIN32Window.hdc, &pfd);
    if (!pixel_format) {
        debug_print("Error: Failed to choose pixel format\n");
        return false;
    }

    if (!SetPixelFormat(WIN32Window.hdc, pixel_format, &pfd)) {
        debug_print("Error: Failed to set pixel format\n");
        return false;
    }

    WIN32Window.hglrc = wglCreateContext(WIN32Window.hdc);
    if (!WIN32Window.hglrc) {
        debug_print("Error: Failed to create OpenGL context\n");
        return false;
    }

    if (!wglMakeCurrent(WIN32Window.hdc, WIN32Window.hglrc)) {
        debug_print("Error: Failed to make OpenGL context current\n");
        wglDeleteContext(WIN32Window.hglrc);
        WIN32Window.hglrc = nullptr;
        return false;
    }

    if (!gladLoadGL()) {
        debug_print("Error: Failed to load OpenGL functions\n");
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(WIN32Window.hglrc);
        WIN32Window.hglrc = nullptr;
        return false;
    }

    debug_print("  OpenGL %d.%d loaded successfully\n", GLVersion.major, GLVersion.minor);
    debug_print("  OpenGL Vendor: %s\n", glGetString(GL_VENDOR));
    debug_print("  OpenGL Renderer: %s\n", glGetString(GL_RENDERER));

    return true;
}

/**
 * @brief Map Win32 virtual key codes to our key constants
 */
internal void update_key_state(WPARAM wParam, bool is_pressed) {
    switch (wParam) {
        case VK_ESCAPE:
            WIN32Input.keys[WINDOW_KEY_ESCAPE] = is_pressed;
            break;
        case VK_SPACE:
            WIN32Input.keys[WINDOW_KEY_SPACE] = is_pressed;
            break;
        case 'A':
            WIN32Input.keys[WINDOW_KEY_A] = is_pressed;
            break;
        case 'D':
            WIN32Input.keys[WINDOW_KEY_D] = is_pressed;
            break;
        case 'S':
            WIN32Input.keys[WINDOW_KEY_S] = is_pressed;
            break;
        case 'W':
            WIN32Input.keys[WINDOW_KEY_W] = is_pressed;
            break;
        case 'F':
            WIN32Input.keys[WINDOW_KEY_F] = is_pressed;
            break;
        case 'M':
            WIN32Input.keys[WINDOW_KEY_M] = is_pressed;
            break;
        default:
            break;
    }
}

/**
 * @brief The main window procedure that handles messages
 */
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CLOSE:
            WIN32Window.should_close = true;
            DestroyWindow(hWnd);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            update_key_state(wParam, true);
            break;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            update_key_state(wParam, false);
            break;
        case WM_LBUTTONDOWN:
            WIN32Input.mouse_buttons[WINDOW_MOUSE_LEFT] = true;
            break;
        case WM_LBUTTONUP:
            WIN32Input.mouse_buttons[WINDOW_MOUSE_LEFT] = false;
            break;
        case WM_RBUTTONDOWN:
            WIN32Input.mouse_buttons[WINDOW_MOUSE_RIGHT] = true;
            break;
        case WM_RBUTTONUP:
            WIN32Input.mouse_buttons[WINDOW_MOUSE_RIGHT] = false;
            break;
        case WM_MBUTTONDOWN:
            WIN32Input.mouse_buttons[WINDOW_MOUSE_MIDDLE] = true;
            break;
        case WM_MBUTTONUP:
            WIN32Input.mouse_buttons[WINDOW_MOUSE_MIDDLE] = false;
            break;
        case WM_MOUSEMOVE:
            WIN32Input.mouse_x = GET_X_LPARAM(lParam);
            WIN32Input.mouse_y = GET_Y_LPARAM(lParam);
            break;
        case WM_SIZE:
            WIN32Window.client_width = LOWORD(lParam);
            WIN32Window.client_height = HIWORD(lParam);
            
            if (WIN32Window.game) {
                WIN32Window.game->window_width = WIN32Window.client_width;
                WIN32Window.game->window_height = WIN32Window.client_height;
            }

            break;
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}


/**
 * @brief Initialize the window system with a game instance
 */
void window_init(Game* game, int32 width, int32 height) {
    debug_print("Initializing window system...\n");
    debug_print("  Title: %s\n", game->title);
    
    WIN32Window.game = game;

    // Get the current instance handle
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    // Define the window class
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "GameWindowClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    // Register the window class
    if (!RegisterClass(&wc)) {
        debug_print("Error: Failed to register window class\n");
        return;
    }
    debug_print("  Window class registered successfully\n");

    // Create the window
    DWORD dwStyle = WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX;
    RECT rect = { 0, 0, width, height };
    AdjustWindowRect(&rect, dwStyle, FALSE);
    
    debug_print("  Adjusted window size: %dx%d\n", rect.right - rect.left, rect.bottom - rect.top);

    WIN32Window.hwnd = CreateWindowEx(
        0,
        "GameWindowClass",
        WIN32Window.game->title,
        dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!WIN32Window.hwnd) {
        debug_print("Error: Failed to create window\n");
        return;
    }
    debug_print("  Window created successfully\n");

    // Get a device context for the window
    WIN32Window.hdc = GetDC(WIN32Window.hwnd);
    debug_print("  Device context obtained\n");

    if (!init_opengl_context()) {
        debug_print("Error: Failed to setup OpenGL context\n");
        return;
    }

    init_vsync_extension();

    // ShowWindow(WIN32Window.hwnd, SW_SHOWDEFAULT);
    UpdateWindow(WIN32Window.hwnd);
    
    debug_print("Window system initialized successfully\n");
}

/**
 * @brief Present the framebuffer to the screen
 */
void window_present(void) {
    if (!WIN32Window.hwnd || !WIN32Window.hdc) return;

    SwapBuffers(WIN32Window.hdc);
}

/**
 * @brief Show the window
 */
void window_show(void) {
    if (!WIN32Window.hwnd) return;
    
    ShowWindow(WIN32Window.hwnd, SW_SHOWDEFAULT);
    UpdateWindow(WIN32Window.hwnd);
    SetForegroundWindow(WIN32Window.hwnd);
    SetFocus(WIN32Window.hwnd);
}

/**
 * @brief Clean up window resources
 */
void window_cleanup(void) {
    debug_print("Cleaning up window system...\n");

    if (WIN32Window.hglrc) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(WIN32Window.hglrc);
        WIN32Window.hglrc = nullptr;
        debug_print("  OpenGL context deleted\n");
    }

    if (WIN32Window.hdc) {
        ReleaseDC(WIN32Window.hwnd, WIN32Window.hdc);
        WIN32Window.hdc = nullptr;
        debug_print("  Device context released\n");
    }
    if (WIN32Window.hwnd) {
        DestroyWindow(WIN32Window.hwnd);
        WIN32Window.hwnd = nullptr;
        debug_print("  Window destroyed\n");
    }
    debug_print("Window system cleaned up\n");
}

/**
 * @brief Check if the window should close
 */
bool window_should_close(void) {
    return WIN32Window.should_close;
}

/**
 * @brief Process pending window events
 */
void window_poll_events(void) {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

/**
 * @brief Get the state of a keyboard key
 */
bool window_get_key_state(WindowKey key) {
    if (key >= 0 && key < WINDOW_KEY_COUNT) {
        return WIN32Input.keys[key];
    }
    return false;
}

/**
 * @brief Get the current mouse position
 */
void window_get_mouse_position(int* x, int* y) {
    if (x) *x = WIN32Input.mouse_x;
    if (y) *y = WIN32Input.mouse_y;
}

/**
 * @brief Get the state of a mouse button
 */
bool window_get_mouse_button_state(int button) {
    if (button >= 0 && button < 3) {
        return WIN32Input.mouse_buttons[button];
    }
    return false;
}

/**
 * @brief Set the window title
 */
void window_set_title(const char* title) {
    if (WIN32Window.hwnd && title) {
        SetWindowText(WIN32Window.hwnd, title);
    }
}

/**
 * @brief Get the current window size
 */
void window_get_size(int* width, int* height) {
    if (!WIN32Window.hwnd) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }

    RECT client_rect;
    GetClientRect(WIN32Window.hwnd, &client_rect);
    if (width) *width = client_rect.right - client_rect.left;
    if (height) *height = client_rect.bottom - client_rect.top;
}

/**
 * @brief Set the window size
 */
void window_set_size(int width, int height) {
    if (!WIN32Window.hwnd) return;

    RECT rect = { 0, 0, width, height };
    DWORD dwStyle = GetWindowLong(WIN32Window.hwnd, GWL_STYLE);
    AdjustWindowRect(&rect, dwStyle, FALSE);

    SetWindowPos(WIN32Window.hwnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER);
}

/**
 * @brief Set whether the window is resizable
 */
void window_set_resizable(bool resizable) {
    if (!WIN32Window.hwnd) return;
    WIN32Window.is_resizable = resizable;
    
    DWORD dwStyle = GetWindowLong(WIN32Window.hwnd, GWL_STYLE);
    if (resizable) {
        dwStyle |= WS_THICKFRAME;
        dwStyle |= WS_MAXIMIZEBOX;
    } else {
        dwStyle &= ~WS_THICKFRAME;
        dwStyle &= ~WS_MAXIMIZEBOX;
    }
    
    SetWindowLong(WIN32Window.hwnd, GWL_STYLE, dwStyle);
}

/**
* @brief Set wheter the window has vsync
*/
void window_set_vsync(bool enable) {
    if (WIN32Window.wglSwapIntervalEXT) {
        WIN32Window.wglSwapIntervalEXT(enable ? 1 : 0);
        debug_print("VSync %s\n", enable ? "enabled" : "disabled");
    }
}
