#import <Cocoa/Cocoa.h>
#include <CoreGraphics/CGImage.h>
#include "game.h"
#include "window.h"

// TODO: Store the bitmapContext in a instance variable so that it's not recreated every frame.
// TODO: Make a window_resize() function
// TODO: Make a window_target_fps() function to cap the framerate

/**
 * @file window.m
 * @brief macOS-specific window implementation using Cocoa
 * 
 * This file implements the window system for macOS using Objective-C and Cocoa.
 * It creates a native NSWindow that displays the game's framebuffer using
 * Core Graphics bitmap contexts.
 */

/**
 * @brief Input state tracking structure
 */
typedef struct {
    bool keys[256];          // Keyboard state array
    bool mouse_buttons[3];   // Mouse button states
    int mouse_x, mouse_y;    // Mouse position
} InputState;

/**
 * @brief Custom NSView subclass for rendering the game's framebuffer
 * 
 * GameView is responsible for converting the game's RGBA32 framebuffer
 * into a displayable image using Core Graphics. It handles coordinate
 * system conversion and input event tracking.
 */
@interface GameView : NSView
/** Pointer to the game instance containing the framebuffer */
@property (nonatomic) Game* game;
/** Pointer to input state tracking */
@property (nonatomic) InputState* inputState;
@end

@implementation GameView

/**
 * @brief Initialize the view and enable mouse tracking
 */
- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        // Create tracking area for mouse events
        NSTrackingArea* trackingArea = [[NSTrackingArea alloc]
            initWithRect:frame
            options:(NSTrackingActiveInKeyWindow | NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited)
            owner:self
            userInfo:nil];
        [self addTrackingArea:trackingArea];
    }
    return self;
}

/**
 * @brief Accept first responder status for keyboard events
 */
- (BOOL)acceptsFirstResponder {
    return YES;
}

/**
 * @brief Handle key down events
 */
- (void)keyDown:(NSEvent*)event {
    if (self.inputState) {
        unsigned short keyCode = [event keyCode];
        if (keyCode < 256) {
            self.inputState->keys[keyCode] = true;
        }
    }
}

/**
 * @brief Handle key up events
 */
- (void)keyUp:(NSEvent*)event {
    if (self.inputState) {
        unsigned short keyCode = [event keyCode];
        if (keyCode < 256) {
            self.inputState->keys[keyCode] = false;
        }
    }
}

/**
 * @brief Handle mouse down events
 */
- (void)mouseDown:(NSEvent*)event {
    if (self.inputState) {
        self.inputState->mouse_buttons[WINDOW_MOUSE_LEFT] = true;
    }
}

/**
 * @brief Handle mouse up events
 */
- (void)mouseUp:(NSEvent*)event {
    if (self.inputState) {
        self.inputState->mouse_buttons[WINDOW_MOUSE_LEFT] = false;
    }
}

/**
 * @brief Handle right mouse down events
 */
- (void)rightMouseDown:(NSEvent*)event {
    if (self.inputState) {
        self.inputState->mouse_buttons[WINDOW_MOUSE_RIGHT] = true;
    }
}

/**
 * @brief Handle right mouse up events
 */
- (void)rightMouseUp:(NSEvent*)event {
    if (self.inputState) {
        self.inputState->mouse_buttons[WINDOW_MOUSE_RIGHT] = false;
    }
}

/**
 * @brief Handle other mouse down events (middle button)
 */
- (void)otherMouseDown:(NSEvent*)event {
    if (self.inputState && [event buttonNumber] == 2) {
        self.inputState->mouse_buttons[WINDOW_MOUSE_MIDDLE] = true;
    }
}

/**
 * @brief Handle other mouse up events (middle button)
 */
- (void)otherMouseUp:(NSEvent*)event {
    if (self.inputState && [event buttonNumber] == 2) {
        self.inputState->mouse_buttons[WINDOW_MOUSE_MIDDLE] = false;
    }
}

/**
 * @brief Handle mouse moved events
 */
- (void)mouseMoved:(NSEvent*)event {
    if (self.inputState) {
        NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
        self.inputState->mouse_x = (int)point.x;
        self.inputState->mouse_y = (int)(self.bounds.size.height - point.y); // Flip Y coordinate
    }
}

/**
 * @brief Handle mouse dragged events
 */
- (void)mouseDragged:(NSEvent*)event {
    [self mouseMoved:event];
}

/**
 * @brief Handle right mouse dragged events
 */
- (void)rightMouseDragged:(NSEvent*)event {
    [self mouseMoved:event];
}

/**
 * @brief Handle other mouse dragged events
 */
- (void)otherMouseDragged:(NSEvent*)event {
    [self mouseMoved:event];
}

/**
 * @brief Renders the game's framebuffer to the view
 * @param dirtyRect The rectangle that needs to be redrawn (unused)
 * 
 * This method is called by Cocoa whenever the view needs to be redrawn.
 * It creates a CGBitmapContext from the game's framebuffer data and
 * draws it to the view, handling coordinate system conversion.
 */
- (void)drawRect:(NSRect)dirtyRect {
    if (!self.game) return;

    NSGraphicsContext* context = [NSGraphicsContext currentContext];
    CGContextRef cgContext = [context CGContext];

    // Create color space for RGB display
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    
    // Create bitmap context from game's framebuffer
    // The game's display buffer is RGBA32 with premultiplied alpha
    CGContextRef bitmapContext = CGBitmapContextCreate(
        self.game->display,                    // Framebuffer data
        self.game->display_width,              // Width in pixels
        self.game->display_height,             // Height in pixels
        8,                                     // Bits per component (R, G, B, A)
        self.game->display_width * 4,          // Bytes per row (4 bytes per pixel)
        colorSpace,
        kCGImageAlphaPremultipliedFirst | kCGImageByteOrder32Little
    );

    if (bitmapContext) {
        // Create image from bitmap context
        CGImageRef image = CGBitmapContextCreateImage(bitmapContext);
        if (image) {
            CGContextSaveGState(cgContext);

            // Draw the image to fill the entire view bounds
            CGContextDrawImage(cgContext, self.bounds, image);

            CGContextRestoreGState(cgContext);
            CGImageRelease(image);
        }
        CGContextRelease(bitmapContext);
    }

    CGColorSpaceRelease(colorSpace);
}

@end

/**
 * @brief Application delegate handling window lifecycle and game loop
 * 
 * AppDelegate manages the main window, sets up the application menu,
 * and runs the game loop timer. It implements NSApplicationDelegate
 * and NSWindowDelegate protocols to handle application and window events.
 */
@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
/** The main application window */
@property (nonatomic, strong) NSWindow* window;
/** The custom view that renders the game */
@property (nonatomic, strong) GameView* gameView;
/** Pointer to the game instance */
@property (nonatomic) Game* game;
/** Game loop timer */
@property (nonatomic, strong) NSTimer* gameTimer;
/** Input state tracking */
@property (nonatomic) InputState inputState;
/** Should close flag */
@property (nonatomic) bool shouldClose;
/** VSync enabled flag */
@property (nonatomic) bool vsyncEnabled;
/** Create and display the window manually */
- (void)createWindow;
@end

@implementation AppDelegate

/**
 * @brief Initialize the delegate
 */
- (instancetype)init {
    self = [super init];
    if (self) {
        memset(&_inputState, 0, sizeof(InputState));
        _shouldClose = false;
        _vsyncEnabled = false;
    }
    return self;
}

/**
 * @brief Create and display the window
 * 
 * This replaces the applicationDidFinishLaunching method for manual setup.
 */
- (void)createWindow {
    // Set up the application menu for CMD+Q to work properly
    NSMenu* mainMenu = [[NSMenu alloc] init];
    NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
    [mainMenu addItem:appMenuItem];
    
    NSMenu* appMenu = [[NSMenu alloc] init];
    NSString* appName = [[NSProcessInfo processInfo] processName];
    NSString* quitTitle = [NSString stringWithFormat:@"Quit %@", appName];
    NSMenuItem* quitMenuItem = [[NSMenuItem alloc] initWithTitle:quitTitle
                                                          action:@selector(terminate:)
                                                   keyEquivalent:@"q"];
    [appMenu addItem:quitMenuItem];
    [appMenuItem setSubmenu:appMenu];
    [NSApp setMainMenu:mainMenu];
    
    // Create the window with dimensions matching the game's display
    NSRect frame = NSMakeRect(0, 0, self.game->display_width, self.game->display_height);

    self.window = [[NSWindow alloc]
        initWithContentRect:frame
        styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
        backing:NSBackingStoreBuffered
        defer:NO
        ];

    [self.window setTitle:[NSString stringWithUTF8String:self.game->title]];
    [self.window setDelegate:self];
    [self.window center];
    
    // Create and configure the game view
    self.gameView = [[GameView alloc] initWithFrame:frame];
    self.gameView.game = self.game;
    self.gameView.inputState = &_inputState;
    
    [self.window setContentView:self.gameView];
    [self.window makeKeyAndOrderFront:nil];

    // Ensure the window gets focus
    [NSApp activateIgnoringOtherApps:YES];
    [self.window orderFrontRegardless];
    [self.window makeFirstResponder:self.gameView];
}

/**
 * @brief Called when the application finishes launching (now unused)
 */
- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    // Window creation is now handled manually in createWindow
}

/**
 * @brief Start the game loop timer
 */
- (void)startGameLoop {
    if (self.gameTimer) {
        [self.gameTimer invalidate];
    }
    
    double interval = 1.0 / self.game->fps;
    self.gameTimer = [NSTimer scheduledTimerWithTimeInterval:interval
                                                      target:self
                                                    selector:@selector(gameLoop:)
                                                    userInfo:nil
                                                     repeats:YES];
}

/**
 * @brief Stop the game loop timer
 */
- (void)stopGameLoop {
    if (self.gameTimer) {
        [self.gameTimer invalidate];
        self.gameTimer = nil;
    }
}

/**
 * @brief Game loop callback called by the timer
 * @param timer The timer that triggered this call (unused)
 * 
 * Updates the game logic and triggers a redraw of the game view.
 * This method is called at the frequency specified by the game's FPS setting.
 */
- (void)gameLoop:(NSTimer*)timer {
    game_update();  // Update game logic
    [self.gameView setNeedsDisplay:YES];  // Trigger a redraw
}

/**
 * @brief Determines if the application should terminate when the last window closes
 * @param sender The application instance (unused)
 * @return YES to terminate the application when the window closes
 */
- (BOOL)applicationShouldTerminateWhenLastWindowClosed:(NSApplication*)sender {
    return YES;
}

/**
 * @brief Called when the window is about to close
 * @param notification Notification object (unused)
 * 
 * Sets the should close flag and stops the game loop.
 */
- (void)windowWillClose:(NSNotification*)notification {
    self.shouldClose = true;
    [self stopGameLoop];
}

/**
 * @brief Called when the application is asked to terminate
 * @param sender The application instance (unused)
 * @return NSTerminateNow to allow immediate termination
 */
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    return NSTerminateNow;
}

@end

/** Global application delegate instance */
static AppDelegate* appDelegate = nil;

/**
 * @brief Map macOS key codes to our key constants
 */
[[maybe_unused]]
static int mapKeyCode(unsigned short keyCode) {
    switch (keyCode) {
        case 53: return WINDOW_KEY_ESCAPE;  // Escape
        case 49: return WINDOW_KEY_SPACE;   // Space
        case 0:  return WINDOW_KEY_A;       // A
        case 2:  return WINDOW_KEY_D;       // D
        case 1:  return WINDOW_KEY_S;       // S
        case 13: return WINDOW_KEY_W;       // W
        default: return keyCode;
    }
}

/**
 * @brief Map abstract key to macOS key code for lookup
 */
static unsigned short mapAbstractKeyToMac(WindowKey key) {
    switch (key) {
        case WINDOW_KEY_ESCAPE: return 53;
        case WINDOW_KEY_SPACE: return 49;
        case WINDOW_KEY_A: return 0;
        case WINDOW_KEY_D: return 2;
        case WINDOW_KEY_S: return 1;
        case WINDOW_KEY_W: return 13;
        default: return 0;
    }
}

/**
 * @brief Initialize the window system with a game instance
 * @param game Pointer to the game instance containing display settings
 * 
 * Creates the NSApplication shared instance, initializes the application
 * delegate with the game reference, and sets up the application activation policy.
 * This function must be called before window_run().
 */
void window_init(Game* game) {
    @autoreleasepool {
        // Get the shared application instance
        [NSApplication sharedApplication];

        // Create and configure the application delegate
        appDelegate = [[AppDelegate alloc] init];
        appDelegate.game = game;

        // Set the delegate and activation policy for a regular GUI application
        [NSApp setDelegate:appDelegate];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        // Manually trigger window creation instead of waiting for applicationDidFinishLaunching
        [appDelegate createWindow];
    }
}

/**
 * @brief Clean up window resources
 */
void window_cleanup(void) {
    @autoreleasepool {
        if (appDelegate) {
            [appDelegate stopGameLoop];
            appDelegate = nil;
        }
    }
}

/**
 * @brief Check if the window should close
 */
bool window_should_close(void) {
    if (!appDelegate) return true;
    return appDelegate.shouldClose;
}

/**
 * @brief Process pending window events
 * 
 * Processes all pending window system events such as keyboard input,
 * mouse input, and window events. Returns after all pending events
 * are processed.
 */
void window_poll_events(void) {
    @autoreleasepool {
        NSEvent* event;
        // Process ALL pending events until there are none left
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                           untilDate:[NSDate distantPast]
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES])) {
            [NSApp sendEvent:event];
        }
        
        // Also process any pending drawing/UI updates
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode 
                                 beforeDate:[NSDate distantPast]];
    }
}

/**
 * @brief Get the state of a keyboard key
 */
bool window_get_key_state(WindowKey key) {
    if (!appDelegate) return false;
    
    // Convert abstract key to platform-specific code
    unsigned short macKeyCode = mapAbstractKeyToMac(key);
    if (macKeyCode < 256) {
        return appDelegate.inputState.keys[macKeyCode];
    }
    
    return false;
}

/**
 * @brief Get the current mouse position
 */
void window_get_mouse_position(int* x, int* y) {
    if (!appDelegate) {
        if (x) *x = 0;
        if (y) *y = 0;
        return;
    }
    
    if (x) *x = appDelegate.inputState.mouse_x;
    if (y) *y = appDelegate.inputState.mouse_y;
}

/**
 * @brief Get the state of a mouse button
 */
bool window_get_mouse_button_state(int button) {
    if (!appDelegate) return false;
    
    if (button >= 0 && button < 3) {
        return appDelegate.inputState.mouse_buttons[button];
    }
    
    return false;
}

/**
 * @brief Set the window title
 */
void window_set_title(const char* title) {
    if (!appDelegate || !appDelegate.window || !title) return;
    
    @autoreleasepool {
        NSString* nsTitle = [NSString stringWithUTF8String:title];
        [appDelegate.window setTitle:nsTitle];
    }
}

/**
 * @brief Get the current window size
 */
void window_get_size(int* width, int* height) {
    if (!appDelegate || !appDelegate.window) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }
    
    NSRect frame = [appDelegate.window.contentView bounds];
    if (width) *width = (int)frame.size.width;
    if (height) *height = (int)frame.size.height;
}

/**
 * @brief Set the window size
 */
void window_set_size(int width, int height) {
    if (!appDelegate || !appDelegate.window) return;
    
    NSRect frame = [appDelegate.window frame];
    NSRect contentRect = NSMakeRect(frame.origin.x, frame.origin.y, width, height);
    NSRect windowRect = [appDelegate.window frameRectForContentRect:contentRect];
    
    [appDelegate.window setFrame:windowRect display:YES animate:YES];
}

/**
 * @brief Set whether the window is resizable
 */
void window_set_resizable(bool resizable) {
    if (!appDelegate || !appDelegate.window) return;
    
    NSWindowStyleMask styleMask = [appDelegate.window styleMask];
    
    if (resizable) {
        styleMask |= NSWindowStyleMaskResizable;
    } else {
        styleMask &= ~NSWindowStyleMaskResizable;
    }
    
    [appDelegate.window setStyleMask:styleMask];
}

/**
 * @brief Present the framebuffer to the screen
 * 
 * Forces an immediate redraw of the window content.
 */
void window_present(void) {
    if (!appDelegate || !appDelegate.gameView) return;
    
    @autoreleasepool {
        [appDelegate.gameView setNeedsDisplay:YES];
        [appDelegate.gameView displayIfNeeded]; // Force immediate draw
    }
}

/**
 * @brief Set vertical synchronization
 */
void window_set_vsync(bool enabled) {
    if (!appDelegate) return;
    
    appDelegate.vsyncEnabled = enabled;
    // Restart game loop with new timing if needed
    if (appDelegate.gameTimer) {
        [appDelegate startGameLoop];
    }
}
