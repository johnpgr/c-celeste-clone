#import <Cocoa/Cocoa.h>
#include "game.h"

@interface GameView : NSView
@property (nonatomic) Game* game;
@end

@implementation GameView

- (void)drawRect:(NSRect)dirtyRect {
    if (!self.game) return;

    NSGraphicsContext* context = [NSGraphicsContext currentContext];
    CGContextRef cgContext = [context CGContext];

    // Clear with black background
    CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.0, 1.0);
    CGContextFillRect(cgContext, self.bounds);

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef bitmapContext = CGBitmapContextCreate(
        self.game->display,
        self.game->display_width,
        self.game->display_height,
        8, /* bits per component */
        self.game->display_width * 4, /* bytes per row * (RGBA32) */
        colorSpace,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Little
    );

    if (bitmapContext) {
        CGImageRef image = CGBitmapContextCreateImage(bitmapContext);
        if (image) {
            /* Flip coordinate system (Core Graphics is bottom-up, we want top-down) */
            CGContextSaveGState(cgContext);
            CGContextTranslateCTM(cgContext, 0, self.bounds.size.height);
            CGContextScaleCTM(cgContext, 1, -1);
            CGContextDrawImage(cgContext, self.bounds, image);
            CGContextRestoreGState(cgContext);
            CGImageRelease(image);
        }
        CGContextRelease(bitmapContext);
    }

    CGColorSpaceRelease(colorSpace);
}

@end

@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property (nonatomic, strong) NSWindow* window;
@property (nonatomic, strong) GameView* gameView;
@property (nonatomic) Game* game;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    // Set up the application menu for CMD+Q to work
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
    
    /* Create the window */
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
    
    // Create game view
    self.gameView = [[GameView alloc] initWithFrame:frame];
    self.gameView.game = self.game;
    
    [self.window setContentView:self.gameView];
    [self.window makeKeyAndOrderFront:nil];

    // Ensure the window gets focus
    [NSApp activateIgnoringOtherApps:YES];
    [self.window orderFrontRegardless];
    
    // Start game loop timer
    [NSTimer scheduledTimerWithTimeInterval:(1.0 / self.game->fps)
                                     target:self
                                   selector:@selector(gameLoop:)
                                   userInfo:nil
                                    repeats:YES];
}

- (void)gameLoop:(NSTimer*)timer {
    game_update();
    [self.gameView setNeedsDisplay:YES];
}

- (BOOL)applicationShouldTerminateWhenLastWindowClosed:(NSApplication*)sender {
    return YES;
}

- (void)windowWillClose:(NSNotification*)notification {
    [NSApp terminate:nil];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    return NSTerminateNow;
}

@end

static AppDelegate* appDelegate = nil;

void window_init(Game* game) {
    @autoreleasepool {
        [NSApplication sharedApplication];

        appDelegate = [[AppDelegate alloc] init];
        appDelegate.game = game;

        [NSApp setDelegate:appDelegate];
        [NSApp setActivationPolicy:
            NSApplicationActivationPolicyRegular];
    }
}

void window_run(void) {
    @autoreleasepool {
        [NSApp activateIgnoringOtherApps:YES];
        [NSApp run];
    }
}
