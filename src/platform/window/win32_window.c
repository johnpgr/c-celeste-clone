#include <windows.h>
#include <windowsx.h>
#include "game.h"
#include "window.h"

static struct {
    HWND h_wnd;
    HDC hdc;
    HDC mem_dc;
    void* dib_pixels;
    HBITMAP dib_bitmap;
    HBITMAP old_bitmap;
    int cached_client_width;
    int cached_client_height;
    Game* game;
    bool should_close;
    bool is_resizable;
} g_window_state;

/**
 * @brief Input state tracking structure
 */
static struct {
    bool keys[WINDOW_KEY_COUNT];
    bool mouse_buttons[3];
    int mouse_x, mouse_y;
} g_input_state;

/**
 * @brief Extended bitmap info structure that includes color masks for BI_BITFIELDS
 */
struct {
    BITMAPINFOHEADER header;
    DWORD masks[4]; // R, G, B, A masks for BI_BITFIELDS
} g_custom_bitmap_info;

/**
 * @brief Map Win32 virtual key codes to our key constants
 */
static void update_key_state(WPARAM wParam, bool is_pressed) {
    switch (wParam) {
        case VK_ESCAPE:
            g_input_state.keys[WINDOW_KEY_ESCAPE] = is_pressed;
            break;
        case VK_SPACE:
            g_input_state.keys[WINDOW_KEY_SPACE] = is_pressed;
            break;
        case 'A':
            g_input_state.keys[WINDOW_KEY_A] = is_pressed;
            break;
        case 'D':
            g_input_state.keys[WINDOW_KEY_D] = is_pressed;
            break;
        case 'S':
            g_input_state.keys[WINDOW_KEY_S] = is_pressed;
            break;
        case 'W':
            g_input_state.keys[WINDOW_KEY_W] = is_pressed;
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
            g_window_state.should_close = true;
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
            g_input_state.mouse_buttons[WINDOW_MOUSE_LEFT] = true;
            break;
        case WM_LBUTTONUP:
            g_input_state.mouse_buttons[WINDOW_MOUSE_LEFT] = false;
            break;
        case WM_RBUTTONDOWN:
            g_input_state.mouse_buttons[WINDOW_MOUSE_RIGHT] = true;
            break;
        case WM_RBUTTONUP:
            g_input_state.mouse_buttons[WINDOW_MOUSE_RIGHT] = false;
            break;
        case WM_MBUTTONDOWN:
            g_input_state.mouse_buttons[WINDOW_MOUSE_MIDDLE] = true;
            break;
        case WM_MBUTTONUP:
            g_input_state.mouse_buttons[WINDOW_MOUSE_MIDDLE] = false;
            break;
        case WM_MOUSEMOVE:
            g_input_state.mouse_x = GET_X_LPARAM(lParam);
            g_input_state.mouse_y = GET_Y_LPARAM(lParam);
            break;
        case WM_SIZE:
            g_window_state.cached_client_width = LOWORD(lParam);
            g_window_state.cached_client_height = HIWORD(lParam);
            break;
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

static HBITMAP create_dib_section(HDC hdc, int width, int height, void** pixel_data) {
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    return CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, pixel_data, nullptr, 0);
}

static void setup_double_buffering() {
    debug_print("  Setting up double buffering...\n");
    g_window_state.mem_dc = CreateCompatibleDC(g_window_state.hdc);
    g_window_state.dib_bitmap = create_dib_section(
        g_window_state.hdc,
        g_window_state.game->display_width,
        g_window_state.game->display_height,
        &g_window_state.dib_pixels
    );
    g_window_state.old_bitmap = SelectObject(g_window_state.mem_dc, g_window_state.dib_bitmap);
    debug_print("  DIB section created: %dx%d\n", g_window_state.game->display_width, g_window_state.game->display_height);
}

/**
 * @brief Initialize the window system with a game instance
 */
void window_init(Game* game) {
    debug_print("Initializing window system...\n");
    debug_print("  Title: %s\n", game->title);
    debug_print("  Display size: %dx%d\n", game->display_width, game->display_height);
    
    g_window_state.game = game;

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
    DWORD dwStyle = WS_OVERLAPPEDWINDOW;
    RECT rect = { 0, 0, g_window_state.game->display_width, g_window_state.game->display_height };
    AdjustWindowRect(&rect, dwStyle, FALSE);
    
    debug_print("  Adjusted window size: %dx%d\n", rect.right - rect.left, rect.bottom - rect.top);

    g_window_state.h_wnd = CreateWindowEx(
        0,
        "GameWindowClass",
        g_window_state.game->title,
        dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!g_window_state.h_wnd) {
        debug_print("Error: Failed to create window\n");
        return;
    }
    debug_print("  Window created successfully\n");

    // Get a device context for the window
    g_window_state.hdc = GetDC(g_window_state.h_wnd);
    SetStretchBltMode(g_window_state.hdc, HALFTONE); // Better quality scaling
    debug_print("  Device context obtained\n");
    
    setup_double_buffering();
    debug_print("  Double buffering setup complete\n");

    // Show window
    ShowWindow(g_window_state.h_wnd, SW_SHOWDEFAULT);
    UpdateWindow(g_window_state.h_wnd);
    
    debug_print("Window system initialized successfully\n");
}

/**
 * @brief Present the framebuffer to the screen
 */
void window_present(void) {
    if (!g_window_state.h_wnd || !g_window_state.game || !g_window_state.game->display) return;

    memcpy(g_window_state.dib_pixels, g_window_state.game->display, 
           g_window_state.game->display_width * g_window_state.game->display_height * 4);

    StretchBlt(g_window_state.hdc, 
               0, 0, 
               g_window_state.cached_client_width, g_window_state.cached_client_height,
               g_window_state.mem_dc, 
               0, 0, 
               g_window_state.game->display_width, 
               g_window_state.game->display_height,
               SRCCOPY);
}

/**
 * @brief Clean up window resources
 */
void window_cleanup(void) {
    debug_print("Cleaning up window system...\n");
    if (g_window_state.hdc) {
        ReleaseDC(g_window_state.h_wnd, g_window_state.hdc);
        g_window_state.hdc = nullptr;
        debug_print("  Device context released\n");
    }
    if (g_window_state.dib_bitmap) {
        DeleteObject(g_window_state.dib_bitmap);
        g_window_state.dib_bitmap = nullptr;
        debug_print("  DIB bitmap deleted\n");
    }
    if (g_window_state.h_wnd) {
        DestroyWindow(g_window_state.h_wnd);
        g_window_state.h_wnd = nullptr;
        debug_print("  Window destroyed\n");
    }
    debug_print("Window system cleaned up\n");
}

/**
 * @brief Check if the window should close
 */
bool window_should_close(void) {
    return g_window_state.should_close;
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
        return g_input_state.keys[key];
    }
    return false;
}

/**
 * @brief Get the current mouse position
 */
void window_get_mouse_position(int* x, int* y) {
    if (x) *x = g_input_state.mouse_x;
    if (y) *y = g_input_state.mouse_y;
}

/**
 * @brief Get the state of a mouse button
 */
bool window_get_mouse_button_state(int button) {
    if (button >= 0 && button < 3) {
        return g_input_state.mouse_buttons[button];
    }
    return false;
}

/**
 * @brief Set the window title
 */
void window_set_title(const char* title) {
    if (g_window_state.h_wnd && title) {
        SetWindowText(g_window_state.h_wnd, title);
    }
}

/**
 * @brief Get the current window size
 */
void window_get_size(int* width, int* height) {
    if (!g_window_state.h_wnd) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }

    RECT client_rect;
    GetClientRect(g_window_state.h_wnd, &client_rect);
    if (width) *width = client_rect.right - client_rect.left;
    if (height) *height = client_rect.bottom - client_rect.top;
}

/**
 * @brief Set the window size
 */
void window_set_size(int width, int height) {
    if (!g_window_state.h_wnd) return;

    RECT rect = { 0, 0, width, height };
    DWORD dwStyle = GetWindowLong(g_window_state.h_wnd, GWL_STYLE);
    AdjustWindowRect(&rect, dwStyle, FALSE);

    SetWindowPos(g_window_state.h_wnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER);
}

/**
 * @brief Set whether the window is resizable
 */
void window_set_resizable(bool resizable) {
    if (!g_window_state.h_wnd) return;
    
    DWORD dwStyle = GetWindowLong(g_window_state.h_wnd, GWL_STYLE);
    if (resizable) {
        dwStyle |= WS_THICKFRAME;
    } else {
        dwStyle &= ~WS_THICKFRAME;
    }
    
    SetWindowLong(g_window_state.h_wnd, GWL_STYLE, dwStyle);
}

