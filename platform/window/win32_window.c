#include <windows.h>
#include <windowsx.h>
#include "def.h"
#include "window.h"
#include "input.h"
#include "utils.h"
#include "glad/glad.h"

static struct {
    HWND hwnd;
    HDC hdc;
    HGLRC hglrc;
    bool should_close;
    bool is_resizable;
} win32_window;

static bool init_opengl_context() {
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

    int32 pixel_format = ChoosePixelFormat(win32_window.hdc, &pfd);
    if (!pixel_format) {
        debug_print("Error: Failed to choose pixel format\n");
        return false;
    }

    if (!SetPixelFormat(win32_window.hdc, pixel_format, &pfd)) {
        debug_print("Error: Failed to set pixel format\n");
        return false;
    }

    win32_window.hglrc = wglCreateContext(win32_window.hdc);
    if (!win32_window.hglrc) {
        debug_print("Error: Failed to create OpenGL context\n");
        return false;
    }

    if (!wglMakeCurrent(win32_window.hdc, win32_window.hglrc)) {
        debug_print("Error: Failed to make OpenGL context current\n");
        wglDeleteContext(win32_window.hglrc);
        win32_window.hglrc = nullptr;
        return false;
    }

    if (!gladLoadGL()) {
        debug_print("Error: Failed to load OpenGL functions\n");
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(win32_window.hglrc);
        win32_window.hglrc = nullptr;
        return false;
    }

    debug_print("  OpenGL %d.%d loaded successfully\n", GLVersion.major, GLVersion.minor);
    debug_print("  OpenGL Vendor: %s\n", glGetString(GL_VENDOR));
    debug_print("  OpenGL Renderer: %s\n", glGetString(GL_RENDERER));

    return true;
}

static void win32_fill_input_lookup_table() {
    keycode_lookup_table[VK_LBUTTON] = KEY_MOUSE_LEFT;
    keycode_lookup_table[VK_MBUTTON] = KEY_MOUSE_MIDDLE;
    keycode_lookup_table[VK_RBUTTON] = KEY_MOUSE_RIGHT;

    keycode_lookup_table['A'] = KEY_A;
    keycode_lookup_table['B'] = KEY_B;
    keycode_lookup_table['C'] = KEY_C;
    keycode_lookup_table['D'] = KEY_D;
    keycode_lookup_table['E'] = KEY_E;
    keycode_lookup_table['F'] = KEY_F;
    keycode_lookup_table['G'] = KEY_G;
    keycode_lookup_table['H'] = KEY_H;
    keycode_lookup_table['I'] = KEY_I;
    keycode_lookup_table['J'] = KEY_J;
    keycode_lookup_table['K'] = KEY_K;
    keycode_lookup_table['L'] = KEY_L;
    keycode_lookup_table['M'] = KEY_M;
    keycode_lookup_table['N'] = KEY_N;
    keycode_lookup_table['O'] = KEY_O;
    keycode_lookup_table['P'] = KEY_P;
    keycode_lookup_table['Q'] = KEY_Q;
    keycode_lookup_table['R'] = KEY_R;
    keycode_lookup_table['S'] = KEY_S;
    keycode_lookup_table['T'] = KEY_T;
    keycode_lookup_table['U'] = KEY_U;
    keycode_lookup_table['V'] = KEY_V;
    keycode_lookup_table['W'] = KEY_W;
    keycode_lookup_table['X'] = KEY_X;
    keycode_lookup_table['Y'] = KEY_Y;
    keycode_lookup_table['Z'] = KEY_Z;
    keycode_lookup_table['0'] = KEY_0;
    keycode_lookup_table['1'] = KEY_1;
    keycode_lookup_table['2'] = KEY_2;
    keycode_lookup_table['3'] = KEY_3;
    keycode_lookup_table['4'] = KEY_4;
    keycode_lookup_table['5'] = KEY_5;
    keycode_lookup_table['6'] = KEY_6;
    keycode_lookup_table['7'] = KEY_7;
    keycode_lookup_table['8'] = KEY_8;
    keycode_lookup_table['9'] = KEY_9;

    keycode_lookup_table[VK_SPACE] = KEY_SPACE,
    keycode_lookup_table[VK_OEM_3] = KEY_TICK,
    keycode_lookup_table[VK_OEM_MINUS] = KEY_MINUS,

    keycode_lookup_table[VK_OEM_PLUS] = KEY_EQUAL,
    keycode_lookup_table[VK_OEM_4] = KEY_LEFT_BRACKET,
    keycode_lookup_table[VK_OEM_6] = KEY_RIGHT_BRACKET,
    keycode_lookup_table[VK_OEM_1] = KEY_SEMICOLON,
    keycode_lookup_table[VK_OEM_7] = KEY_QUOTE,
    keycode_lookup_table[VK_OEM_COMMA] = KEY_COMMA,
    keycode_lookup_table[VK_OEM_PERIOD] = KEY_PERIOD,
    keycode_lookup_table[VK_OEM_2] = KEY_FORWARD_SLASH,
    keycode_lookup_table[VK_OEM_5] = KEY_BACKWARD_SLASH,
    keycode_lookup_table[VK_TAB] = KEY_TAB,
    keycode_lookup_table[VK_ESCAPE] = KEY_ESCAPE,
    keycode_lookup_table[VK_PAUSE] = KEY_PAUSE,
    keycode_lookup_table[VK_UP] = KEY_UP,
    keycode_lookup_table[VK_DOWN] = KEY_DOWN,
    keycode_lookup_table[VK_LEFT] = KEY_LEFT,
    keycode_lookup_table[VK_RIGHT] = KEY_RIGHT,
    keycode_lookup_table[VK_BACK] = KEY_BACKSPACE,
    keycode_lookup_table[VK_RETURN] = KEY_RETURN,
    keycode_lookup_table[VK_DELETE] = KEY_DELETE,
    keycode_lookup_table[VK_INSERT] = KEY_INSERT,
    keycode_lookup_table[VK_HOME] = KEY_HOME,
    keycode_lookup_table[VK_END] = KEY_END,
    keycode_lookup_table[VK_PRIOR] = KEY_PAGE_UP,
    keycode_lookup_table[VK_NEXT] = KEY_PAGE_DOWN,
    keycode_lookup_table[VK_CAPITAL] = KEY_CAPS_LOCK,
    keycode_lookup_table[VK_NUMLOCK] = KEY_NUM_LOCK,
    keycode_lookup_table[VK_SCROLL] = KEY_SCROLL_LOCK,
    keycode_lookup_table[VK_APPS] = KEY_MENU,

    keycode_lookup_table[VK_SHIFT] = KEY_SHIFT,
    keycode_lookup_table[VK_LSHIFT] = KEY_SHIFT,
    keycode_lookup_table[VK_RSHIFT] = KEY_SHIFT,

    keycode_lookup_table[VK_CONTROL] = KEY_CONTROL,
    keycode_lookup_table[VK_LCONTROL] = KEY_CONTROL,
    keycode_lookup_table[VK_RCONTROL] = KEY_CONTROL,

    keycode_lookup_table[VK_MENU] = KEY_ALT,
    keycode_lookup_table[VK_LMENU] = KEY_ALT,
    keycode_lookup_table[VK_RMENU] = KEY_ALT,

    keycode_lookup_table[VK_F1] = KEY_F1;
    keycode_lookup_table[VK_F2] = KEY_F2;
    keycode_lookup_table[VK_F3] = KEY_F3;
    keycode_lookup_table[VK_F4] = KEY_F4;
    keycode_lookup_table[VK_F5] = KEY_F5;
    keycode_lookup_table[VK_F6] = KEY_F6;
    keycode_lookup_table[VK_F7] = KEY_F7;
    keycode_lookup_table[VK_F8] = KEY_F8;
    keycode_lookup_table[VK_F9] = KEY_F9;
    keycode_lookup_table[VK_F10] = KEY_F10;
    keycode_lookup_table[VK_F11] = KEY_F11;
    keycode_lookup_table[VK_F12] = KEY_F12;

    keycode_lookup_table[VK_NUMPAD0] = KEY_NUMPAD_0;
    keycode_lookup_table[VK_NUMPAD1] = KEY_NUMPAD_1;
    keycode_lookup_table[VK_NUMPAD2] = KEY_NUMPAD_2;
    keycode_lookup_table[VK_NUMPAD3] = KEY_NUMPAD_3;
    keycode_lookup_table[VK_NUMPAD4] = KEY_NUMPAD_4;
    keycode_lookup_table[VK_NUMPAD5] = KEY_NUMPAD_5;
    keycode_lookup_table[VK_NUMPAD6] = KEY_NUMPAD_6;
    keycode_lookup_table[VK_NUMPAD7] = KEY_NUMPAD_7;
    keycode_lookup_table[VK_NUMPAD8] = KEY_NUMPAD_8;
    keycode_lookup_table[VK_NUMPAD9] = KEY_NUMPAD_9;
}

/**
 * @brief The main window procedure that handles messages
 */
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CLOSE:
            win32_window.should_close = true;
            DestroyWindow(hWnd);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            bool is_down = (uMsg == WM_KEYDOWN) || (uMsg == WM_SYSKEYDOWN) || (uMsg == WM_LBUTTONDOWN);
            KeyCode keycode = keycode_lookup_table[wParam];
            Key* key = &input_state->keys[keycode];
            key->just_pressed = !key->just_pressed && !key->is_down && is_down;
            key->just_released = !key->just_released && key->is_down && !is_down;
            key->is_down = is_down;
            key->half_transition_count++;
        } break;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP: {
            bool is_down = (uMsg == WM_LBUTTONDOWN || uMsg == WM_RBUTTONDOWN || uMsg == WM_MBUTTONDOWN);
            int mousecode =
                (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP) ? VK_LBUTTON :
                (uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONUP) ? VK_MBUTTON : VK_RBUTTON;

            KeyCode keycode = keycode_lookup_table[mousecode];
            Key* key = &input_state->keys[keycode];
            key->just_pressed = !key->just_pressed && !key->is_down && is_down;
            key->just_released = !key->just_released && key->is_down && !is_down;
            key->is_down = is_down;
            key->half_transition_count++;
        } break;
        case WM_SIZE:
            input_state->screen_size.x = LOWORD(lParam);
            input_state->screen_size.y = HIWORD(lParam);

            break;
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}


/**
 * @brief Initialize the window system with a game instance
 */
void window_init(const char* title, int width, int height) {
    debug_print("Initializing window system...\n");

    // Get the current instance handle
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    // Define the window class
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = title;
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
    AdjustWindowRect(&rect, dwStyle, false);
    
    debug_print("  Adjusted window size: %dx%d\n", rect.right - rect.left, rect.bottom - rect.top);

    win32_window.hwnd = CreateWindowEx(
        0,
        title,
        title,
        dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!win32_window.hwnd) {
        debug_print("Error: Failed to create window\n");
        return;
    }
    debug_print("  Window created successfully\n");

    // Get a device context for the window
    win32_window.hdc = GetDC(win32_window.hwnd);
    debug_print("  Device context obtained\n");

    if (!init_opengl_context()) {
        debug_print("Error: Failed to setup OpenGL context\n");
        return;
    }

    UpdateWindow(win32_window.hwnd);
    
    win32_fill_input_lookup_table();

    debug_print("Window system initialized successfully\n");
}

/**
 * @brief Present the framebuffer to the screen
 */
void window_present(void) {
    SwapBuffers(win32_window.hdc);
}

/**
 * @brief Show the window
 */
void window_show(void) {
    if (!win32_window.hwnd) return;
    
    ShowWindow(win32_window.hwnd, SW_SHOWDEFAULT);
    UpdateWindow(win32_window.hwnd);
    SetForegroundWindow(win32_window.hwnd);
    SetFocus(win32_window.hwnd);
}

/**
 * @brief Clean up window resources
 */
void window_cleanup(void) {
    debug_print("Cleaning up window system...\n");

    if (win32_window.hglrc) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(win32_window.hglrc);
        win32_window.hglrc = nullptr;
        debug_print("  OpenGL context deleted\n");
    }

    if (win32_window.hdc) {
        ReleaseDC(win32_window.hwnd, win32_window.hdc);
        win32_window.hdc = nullptr;
        debug_print("  Device context released\n");
    }
    if (win32_window.hwnd) {
        DestroyWindow(win32_window.hwnd);
        win32_window.hwnd = nullptr;
        debug_print("  Window destroyed\n");
    }
    debug_print("Window system cleaned up\n");
}

/**
 * @brief Check if the window should close
 */
bool window_should_close(void) {
    return win32_window.should_close;
}

/**
 * @brief Process pending window events
 */
void window_poll_events(void) {
    for (int keycode = 0; keycode < KEY_COUNT; keycode++) {
        input_state->keys[keycode].just_pressed = false;
        input_state->keys[keycode].just_released = false;
        input_state->keys[keycode].half_transition_count = 0;
    }

    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Update mouse position
    POINT cursor_pos = {};
    GetCursorPos(&cursor_pos);
    ScreenToClient(win32_window.hwnd, &cursor_pos);

    input_state->mouse_pos_prev = input_state->mouse_pos;
    input_state->mouse_pos.x = cursor_pos.x;
    input_state->mouse_pos.y = cursor_pos.y;
    input_state->mouse_delta = ivec2_minus(
        input_state->mouse_pos,
        input_state->mouse_pos_prev
    );

    input_state->mouse_pos_world = screen_to_world(input_state->mouse_pos);
}

/**
 * @brief Set the window title
 */
void window_set_title(const char* title) {
    if (win32_window.hwnd && title) {
        SetWindowText(win32_window.hwnd, title);
    }
}

/**
 * @brief Get the current window size
 */
void window_get_size(int* width, int* height) {
    if (!win32_window.hwnd) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }

    RECT client_rect;
    GetClientRect(win32_window.hwnd, &client_rect);
    if (width) *width = client_rect.right - client_rect.left;
    if (height) *height = client_rect.bottom - client_rect.top;
}

/**
 * @brief Set the window size
 */
void window_set_size(int width, int height) {
    if (!win32_window.hwnd) return;

    RECT rect = { 0, 0, width, height };
    DWORD dwStyle = GetWindowLong(win32_window.hwnd, GWL_STYLE);
    AdjustWindowRect(&rect, dwStyle, false);

    SetWindowPos(
        win32_window.hwnd,
        nullptr,
        0, 0,
        rect.right - rect.left,
        rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER
    );
}

/**
 * @brief Set whether the window is resizable
 */
void window_set_resizable(bool resizable) {
    if (!win32_window.hwnd) return;
    win32_window.is_resizable = resizable;
    
    DWORD dwStyle = GetWindowLong(win32_window.hwnd, GWL_STYLE);
    if (resizable) {
        dwStyle |= WS_THICKFRAME;
        dwStyle |= WS_MAXIMIZEBOX;
    } else {
        dwStyle &= ~WS_THICKFRAME;
        dwStyle &= ~WS_MAXIMIZEBOX;
    }
    
    SetWindowLong(win32_window.hwnd, GWL_STYLE, dwStyle);
}
