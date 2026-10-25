#import <Cocoa/Cocoa.h>

#include "host.h"

#include "lippus/layout.h"

#include <mach/mach_time.h>
#include <math.h>
#include <stdlib.h>

extern const uint32_t lippus_orientation;
extern void lippus_main(void);

@class LippusDelegate;

static uint16_t key_held;
static uint16_t key_pressed;
static uint16_t key_released;
static int quit_requested;
static int game_w;
static int game_h;
static int pixel_scale;
static uint8_t *scaled;
static NSWindow *game_window;
static NSView *game_view;
static LippusDelegate *app_delegate;
static mach_timebase_info_data_t timebase;

static uint16_t bit_for_key(unsigned short code) {
    switch (code) {
    case 126:
        return LIPPUS_UP;
    case 125:
        return LIPPUS_DOWN;
    case 123:
        return LIPPUS_LEFT;
    case 124:
        return LIPPUS_RIGHT;
    case 0:
        return LIPPUS_A;
    case 1:
        return LIPPUS_B;
    case 2:
        return LIPPUS_X;
    case 3:
        return LIPPUS_Y;
    case 18:
        return LIPPUS_SELECT;
    case 19:
        return LIPPUS_START;
    case 49:
        return LIPPUS_CLICK;
    default:
        return 0;
    }
}

static void note_key(unsigned short code, int down) {
    uint16_t bit = bit_for_key(code);
    if (!bit) {
        return;
    }
    if (down) {
        if ((key_held & bit) == 0) {
            key_pressed |= bit;
        }
        key_held |= bit;
    } else if (key_held & bit) {
        key_held = (uint16_t)(key_held & ~bit);
        key_released |= bit;
    }
}

uint16_t host_keys_held(void) { return key_held; }

void host_keys_read(lippus_pad_t *out) {
    out->held = key_held;
    out->pressed = key_pressed;
    out->released = key_released;
    key_pressed = 0;
    key_released = 0;
}

int host_quit_requested(void) { return quit_requested; }

void host_quit(void) { exit(0); }

uint32_t host_ticks(void) {
    uint64_t ticks = mach_absolute_time();
    uint64_t ns = ticks * timebase.numer / timebase.denom;
    return (uint32_t)(ns / 1000000ull);
}

static void store_rgb(uint8_t *pixel, uint16_t color) {
    unsigned r = (color >> 11) & 31u;
    unsigned g = (color >> 5) & 63u;
    unsigned b = color & 31u;
    pixel[0] = (uint8_t)((r << 3) | (r >> 2));
    pixel[1] = (uint8_t)((g << 2) | (g >> 4));
    pixel[2] = (uint8_t)((b << 3) | (b >> 2));
    pixel[3] = 255;
}

@interface LippusView : NSView
@end

@implementation LippusView
- (BOOL)acceptsFirstResponder {
    return YES;
}
- (BOOL)isFlipped {
    return YES;
}
- (void)keyDown:(NSEvent *)event {
    note_key(event.keyCode, 1);
}
- (void)keyUp:(NSEvent *)event {
    note_key(event.keyCode, 0);
}
- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    if (!scaled) {
        return;
    }
    int bw = game_w * pixel_scale;
    int bh = game_h * pixel_scale;
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef src = CGBitmapContextCreate(scaled, (size_t)bw, (size_t)bh, 8, (size_t)bw * 4, space,
                                             kCGBitmapByteOrder32Big | (CGBitmapInfo)kCGImageAlphaNoneSkipLast);
    CGImageRef image = CGBitmapContextCreateImage(src);
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(ctx);
    /* La finestra disegna il fotogramma con l'alto in basso. Si corregge solo
       quell'asse: una rotazione di 180 scambierebbe anche destra e sinistra. */
    CGContextTranslateCTM(ctx, 0, self.bounds.size.height);
    CGContextScaleCTM(ctx, 1, -1);
    CGContextSetInterpolationQuality(ctx, kCGInterpolationNone);
    CGContextDrawImage(ctx, self.bounds, image);
    CGContextRestoreGState(ctx);
    CGImageRelease(image);
    CGContextRelease(src);
    CGColorSpaceRelease(space);
}
@end

@interface LippusDelegate : NSObject <NSWindowDelegate>
@end

@implementation LippusDelegate
- (void)windowWillClose:(NSNotification *)note {
    (void)note;
    quit_requested = 1;
}
- (void)quitGame:(id)sender {
    (void)sender;
    quit_requested = 1;
}
@end

void host_pump(void) {
    while (1) {
        NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate dateWithTimeIntervalSinceNow:0]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        if (!event) {
            break;
        }
        [NSApp sendEvent:event];
    }
}

void host_window_present(void) {
    const uint16_t *fb = host_fb();
    int stride = host_fb_width();
    int bw = game_w * pixel_scale;
    for (int y = 0; y < game_h; y++) {
        for (int x = 0; x < game_w; x++) {
            uint16_t color = fb[(size_t)y * (size_t)stride + (size_t)x];
            for (int dy = 0; dy < pixel_scale; dy++) {
                uint8_t *row = scaled + (((size_t)(y * pixel_scale + dy) * (size_t)bw) + (size_t)(x * pixel_scale)) * 4;
                for (int dx = 0; dx < pixel_scale; dx++) {
                    store_rgb(row + (size_t)dx * 4, color);
                }
            }
        }
    }
    [game_view setNeedsDisplay:YES];
    [game_view display];
}

void host_window_open(const char *title, int width, int height) {
    game_w = width;
    game_h = height;

    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp finishLaunching];

    app_delegate = [LippusDelegate new];
    LippusDelegate *delegate = app_delegate;
    NSMenu *bar = [NSMenu new];
    NSMenuItem *appItem = [NSMenuItem new];
    [bar addItem:appItem];
    NSMenu *appMenu = [NSMenu new];
    NSMenuItem *quit = [[NSMenuItem alloc] initWithTitle:@"Esci"
                                                  action:@selector(quitGame:)
                                           keyEquivalent:@"q"];
    quit.target = delegate;
    [appMenu addItem:quit];
    appItem.submenu = appMenu;
    NSApp.mainMenu = bar;

    NSRect frame = NSMakeRect(0, 0, width * 3, height * 3);
    game_window = [[NSWindow alloc] initWithContentRect:frame
                                                   styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                             NSWindowStyleMaskMiniaturizable
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    NSWindow *window = game_window;
    window.title = [NSString stringWithUTF8String:title];
    window.delegate = delegate;
    LippusView *view = [[LippusView alloc] initWithFrame:frame];
    window.contentView = view;
    game_view = view;
    [window center];
    [window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    [window makeFirstResponder:view];
    int backing = (int)llround(window.backingScaleFactor);
    if (backing < 1) {
        backing = 1;
    }
    pixel_scale = 3 * backing;
    scaled = calloc((size_t)width * (size_t)pixel_scale * (size_t)height * (size_t)pixel_scale, 4);
}

int main(int argc, char **argv) {
    mach_timebase_info(&timebase);
    const char *title = argc > 1 ? argv[1] : "Lippus";
    int portrait = lippus_orientation == LIPPUS_PORTRAIT;
    int width = portrait ? LIPPUS_SCREEN_H : LIPPUS_SCREEN_W;
    int height = portrait ? LIPPUS_SCREEN_W : LIPPUS_SCREEN_H;
    host_bind_api(width, height);
    host_window_open(title, width, height);
    host_audio_init();
    lippus_main();
    return 0;
}
