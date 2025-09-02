#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <string.h>
#include <stdlib.h>
#include "game.h"
#include "window.h"

/**
 * @brief X11 window state structure
 */
static struct {
    Display* display;
    Window window;
    GC gc;
    XImage* ximage;
    XShmSegmentInfo shm_info;
    bool use_shm;
    int screen;
    Visual* visual;
    int depth;
    Game* game;
    bool should_close;
    int cached_width;
    int cached_height;
} g_x11_state;

/**
 * @brief Input state tracking structure
 */
static struct {
    bool keys[WINDOW_KEY_COUNT];
    bool mouse_buttons[3];
    int mouse_x, mouse_y;
} g_input_state;

/**
 * @brief Map X11 key symbols to our key constants
 */
static void update_key_state(KeySym keysym, bool is_pressed) {
    switch (keysym) {
        case XK_Escape:
            g_input_state.keys[WINDOW_KEY_ESCAPE] = is_pressed;
            break;
        case XK_space:
            g_input_state.keys[WINDOW_KEY_SPACE] = is_pressed;
            break;
        case XK_a:
        case XK_A:
            g_input_state.keys[WINDOW_KEY_A] = is_pressed;
            break;
        case XK_d:
        case XK_D:
            g_input_state.keys[WINDOW_KEY_D] = is_pressed;
            break;
        case XK_s:
        case XK_S:
            g_input_state.keys[WINDOW_KEY_S] = is_pressed;
            break;
        case XK_w:
        case XK_W:
            g_input_state.keys[WINDOW_KEY_W] = is_pressed;
            break;
        case XK_f:
        case XK_F:
            g_input_state.keys[WINDOW_KEY_F] = is_pressed;
            break;
        default:
            break;
    }
}

/**
 * @brief Create XImage for framebuffer display
 */
static bool create_ximage(int width, int height) {
    g_x11_state.use_shm = XShmQueryExtension(g_x11_state.display);
    
    if (g_x11_state.use_shm) {
        // Try to use shared memory for better performance
        g_x11_state.ximage = XShmCreateImage(
            g_x11_state.display,
            g_x11_state.visual,
            g_x11_state.depth,
            ZPixmap,
            NULL,
            &g_x11_state.shm_info,
            width,
            height
        );
        
        if (g_x11_state.ximage) {
            g_x11_state.shm_info.shmid = shmget(
                IPC_PRIVATE,
                g_x11_state.ximage->bytes_per_line * g_x11_state.ximage->height,
                IPC_CREAT | 0777
            );
            
            if (g_x11_state.shm_info.shmid >= 0) {
                g_x11_state.shm_info.shmaddr = g_x11_state.ximage->data = 
                    (char*)shmat(g_x11_state.shm_info.shmid, 0, 0);
                g_x11_state.shm_info.readOnly = False;
                
                if (XShmAttach(g_x11_state.display, &g_x11_state.shm_info)) {
                    debug_print("  Using X11 shared memory extension\n");
                    return true;
                }
            }
            
            // Cleanup failed shared memory attempt
            if (g_x11_state.shm_info.shmid >= 0) {
                shmdt(g_x11_state.shm_info.shmaddr);
                shmctl(g_x11_state.shm_info.shmid, IPC_RMID, 0);
            }
            XDestroyImage(g_x11_state.ximage);
        }
    }
    
    // Fallback to regular XImage
    g_x11_state.use_shm = false;
    debug_print("  Using regular XImage (no shared memory)\n");
    
    char* image_data = (char*)malloc(width * height * 4);
    if (!image_data) {
        debug_print("Error: Failed to allocate image data\n");
        return false;
    }
    
    g_x11_state.ximage = XCreateImage(
        g_x11_state.display,
        g_x11_state.visual,
        g_x11_state.depth,
        ZPixmap,
        0,
        image_data,
        width,
        height,
        32,
        0
    );
    
    return g_x11_state.ximage != NULL;
}

/**
 * @brief Initialize the window system with a game instance
 */
void window_init(Game* game) {
    debug_print("Initializing X11 window system...\n");
    debug_print("  Title: %s\n", game->title);
    debug_print("  Display size: %dx%d\n", game->display_width, game->display_height);
    
    g_x11_state.game = game;
    memset(&g_input_state, 0, sizeof(g_input_state));
    
    // Open connection to X server
    g_x11_state.display = XOpenDisplay(NULL);
    if (!g_x11_state.display) {
        debug_print("Error: Cannot connect to X server\n");
        return;
    }
    debug_print("  Connected to X server\n");
    
    g_x11_state.screen = DefaultScreen(g_x11_state.display);
    g_x11_state.visual = DefaultVisual(g_x11_state.display, g_x11_state.screen);
    g_x11_state.depth = DefaultDepth(g_x11_state.display, g_x11_state.screen);
    
    debug_print("  Screen: %d, Depth: %d\n", g_x11_state.screen, g_x11_state.depth);
    
    // Create window
    Window root = RootWindow(g_x11_state.display, g_x11_state.screen);
    
    g_x11_state.window = XCreateSimpleWindow(
        g_x11_state.display,
        root,
        0, 0,
        game->display_width, game->display_height,
        1,
        BlackPixel(g_x11_state.display, g_x11_state.screen),
        WhitePixel(g_x11_state.display, g_x11_state.screen)
    );
    
    if (!g_x11_state.window) {
        debug_print("Error: Failed to create window\n");
        XCloseDisplay(g_x11_state.display);
        return;
    }
    debug_print("  Window created successfully\n");
    
    // Set window properties
    XStoreName(g_x11_state.display, g_x11_state.window, game->title);
    
    // Select input events
    XSelectInput(g_x11_state.display, g_x11_state.window,
                 ExposureMask | KeyPressMask | KeyReleaseMask |
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                 StructureNotifyMask);
    
    // Handle window close button
    Atom wm_delete = XInternAtom(g_x11_state.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_x11_state.display, g_x11_state.window, &wm_delete, 1);
    
    // Create graphics context
    g_x11_state.gc = XCreateGC(g_x11_state.display, g_x11_state.window, 0, NULL);
    
    // Create XImage for framebuffer
    if (!create_ximage(game->display_width, game->display_height)) {
        debug_print("Error: Failed to create XImage\n");
        window_cleanup();
        return;
    }
    
    g_x11_state.cached_width = game->display_width;
    g_x11_state.cached_height = game->display_height;
    
    // Show window
    XMapWindow(g_x11_state.display, g_x11_state.window);
    XFlush(g_x11_state.display);
    
    debug_print("X11 window system initialized successfully\n");
}

/**
 * @brief Present the framebuffer to the screen
 */
void window_present(void) {
    if (!g_x11_state.display || !g_x11_state.window || !g_x11_state.ximage || !g_x11_state.game) {
        return;
    }
    
    // Copy game framebuffer to XImage data
    // Note: X11 typically expects BGRA format like Windows, so our RGBA macro should work
    memcpy(g_x11_state.ximage->data, g_x11_state.game->display,
           g_x11_state.game->display_width * g_x11_state.game->display_height * 4);
    
    // Present to screen
    if (g_x11_state.use_shm) {
        XShmPutImage(g_x11_state.display, g_x11_state.window, g_x11_state.gc,
                     g_x11_state.ximage, 0, 0, 0, 0,
                     g_x11_state.game->display_width, g_x11_state.game->display_height,
                     False);
    } else {
        XPutImage(g_x11_state.display, g_x11_state.window, g_x11_state.gc,
                  g_x11_state.ximage, 0, 0, 0, 0,
                  g_x11_state.game->display_width, g_x11_state.game->display_height);
    }
    
    XFlush(g_x11_state.display);
}

/**
 * @brief Clean up window resources
 */
void window_cleanup(void) {
    debug_print("Cleaning up X11 window system...\n");
    
    if (g_x11_state.ximage) {
        if (g_x11_state.use_shm) {
            XShmDetach(g_x11_state.display, &g_x11_state.shm_info);
            shmdt(g_x11_state.shm_info.shmaddr);
            shmctl(g_x11_state.shm_info.shmid, IPC_RMID, 0);
        }
        XDestroyImage(g_x11_state.ximage);
        g_x11_state.ximage = NULL;
        debug_print("  XImage destroyed\n");
    }
    
    if (g_x11_state.gc) {
        XFreeGC(g_x11_state.display, g_x11_state.gc);
        g_x11_state.gc = NULL;
    }
    
    if (g_x11_state.window) {
        XDestroyWindow(g_x11_state.display, g_x11_state.window);
        g_x11_state.window = 0;
        debug_print("  Window destroyed\n");
    }
    
    if (g_x11_state.display) {
        XCloseDisplay(g_x11_state.display);
        g_x11_state.display = NULL;
        debug_print("  X11 connection closed\n");
    }
    
    debug_print("X11 window system cleaned up\n");
}

/**
 * @brief Check if the window should close
 */
bool window_should_close(void) {
    return g_x11_state.should_close;
}

/**
 * @brief Process pending window events
 */
void window_poll_events(void) {
    if (!g_x11_state.display) return;
    
    XEvent event;
    while (XPending(g_x11_state.display)) {
        XNextEvent(g_x11_state.display, &event);
        
        switch (event.type) {
            case ClientMessage:
                // Handle window close
                g_x11_state.should_close = true;
                break;
                
            case KeyPress: {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                update_key_state(keysym, true);
                break;
            }
            
            case KeyRelease: {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                update_key_state(keysym, false);
                break;
            }
            
            case ButtonPress:
                if (event.xbutton.button == Button1) {
                    g_input_state.mouse_buttons[WINDOW_MOUSE_LEFT] = true;
                } else if (event.xbutton.button == Button3) {
                    g_input_state.mouse_buttons[WINDOW_MOUSE_RIGHT] = true;
                } else if (event.xbutton.button == Button2) {
                    g_input_state.mouse_buttons[WINDOW_MOUSE_MIDDLE] = true;
                }
                break;
                
            case ButtonRelease:
                if (event.xbutton.button == Button1) {
                    g_input_state.mouse_buttons[WINDOW_MOUSE_LEFT] = false;
                } else if (event.xbutton.button == Button3) {
                    g_input_state.mouse_buttons[WINDOW_MOUSE_RIGHT] = false;
                } else if (event.xbutton.button == Button2) {
                    g_input_state.mouse_buttons[WINDOW_MOUSE_MIDDLE] = false;
                }
                break;
                
            case MotionNotify:
                g_input_state.mouse_x = event.xmotion.x;
                g_input_state.mouse_y = event.xmotion.y;
                break;
                
            case ConfigureNotify:
                g_x11_state.cached_width = event.xconfigure.width;
                g_x11_state.cached_height = event.xconfigure.height;
                break;
                
            default:
                break;
        }
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
    if (g_x11_state.display && g_x11_state.window && title) {
        XStoreName(g_x11_state.display, g_x11_state.window, title);
        XFlush(g_x11_state.display);
    }
}

/**
 * @brief Get the current window size
 */
void window_get_size(int* width, int* height) {
    if (width) *width = g_x11_state.cached_width;
    if (height) *height = g_x11_state.cached_height;
}

/**
 * @brief Set the window size
 */
void window_set_size(int width, int height) {
    if (g_x11_state.display && g_x11_state.window) {
        XResizeWindow(g_x11_state.display, g_x11_state.window, width, height);
        XFlush(g_x11_state.display);
    }
}

/**
 * @brief Set whether the window is resizable
 */
void window_set_resizable(bool resizable) {
    if (!g_x11_state.display || !g_x11_state.window) return;
    
    XSizeHints* hints = XAllocSizeHints();
    if (hints) {
        if (resizable) {
            hints->flags = PMinSize;
            hints->min_width = 100;
            hints->min_height = 100;
        } else {
            hints->flags = PMinSize | PMaxSize;
            hints->min_width = hints->max_width = g_x11_state.cached_width;
            hints->min_height = hints->max_height = g_x11_state.cached_height;
        }
        
        XSetWMNormalHints(g_x11_state.display, g_x11_state.window, hints);
        XFree(hints);
        XFlush(g_x11_state.display);
    }
}
