#import <Cocoa/Cocoa.h>
#include "game.h"

/**
 * @file window.m
 * @brief macOS-specific window implementation using Cocoa
 * 
 * This file implements the window system for macOS using Objective-C and Cocoa.
 * It creates a native NSWindow that displays the game's framebuffer using
 * Core Graphics bitmap contexts.
 */

/**
 * @brief Custom NSView subclass for rendering the game's framebuffer
 * 
 * GameView is responsible for converting the game's RGBA32 framebuffer
 * into a displayable image using Core Graphics. It handles coordinate
 * system conversion (Core Graphics uses bottom-up coordinates while
 * the game uses top-down).
 */
@interface GameView : NSView
/** Pointer to the game instance containing the framebuffer */
@property (nonatomic) Game* game;
@end

@implementation GameView

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
        self.game->display,                                         // Framebuffer data
        self.game->display_width,                                   // Width in pixels
        self.game->display_height,                                  // Height in pixels
        8,                                                          // Bits per component (R, G, B, A)
        self.game->display_width * 4,                               // Bytes per row (4 bytes per pixel)
        colorSpace,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Little // The bitmap flags for RGBA32 format
    );

    if (bitmapContext) {
        // Create image from bitmap context
        CGImageRef image = CGBitmapContextCreateImage(bitmapContext);
        if (image) {
            // Flip coordinate system (Core Graphics is bottom-up, we want top-down)
            CGContextSaveGState(cgContext);
            CGContextTranslateCTM(cgContext, 0, self.bounds.size.height);
            CGContextScaleCTM(cgContext, 1, -1);
            
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
@end

@implementation AppDelegate

/**
 * @brief Called when the application finishes launching
 * @param notification Notification object (unused)
 * 
 * Sets up the application menu, creates the main window with the game view,
 * and starts the game loop timer. The window size is determined by the
 * game's display dimensions.
 */
- (void)applicationDidFinishLaunching:(NSNotification*)notification {
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
    
    [self.window setContentView:self.gameView];
    [self.window makeKeyAndOrderFront:nil];

    // Ensure the window gets focus
    [NSApp activateIgnoringOtherApps:YES];
    [self.window orderFrontRegardless];
    
    // Start game loop timer - calls gameLoop: method at the specified FPS
    [NSTimer scheduledTimerWithTimeInterval:(1.0 / self.game->fps)
                                     target:self
                                   selector:@selector(gameLoop:)
                                   userInfo:nil
                                    repeats:YES];
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
 * Terminates the application when the main window closes.
 */
- (void)windowWillClose:(NSNotification*)notification {
    [NSApp terminate:nil];
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
    }
}

/**
 * @brief Start the main window loop
 * 
 * Activates the application and begins the Cocoa event loop. This function
 * blocks until the application terminates (when the window is closed).
 * The window and game loop timer are automatically set up by the application delegate.
 */
void window_run(void) {
    @autoreleasepool {
        [NSApp activateIgnoringOtherApps:YES];
        [NSApp run];  // Blocks until application terminates
    }
}
