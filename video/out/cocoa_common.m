/*
 * Cocoa OpenGL Backend
 *
 * This file is part of mplayer2.
 *
 * mplayer2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer2.  If not, see <http://www.gnu.org/licenses/>.
 */

#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h> // for CGDisplayHideCursor
#import <IOKit/pwr_mgt/IOPMLib.h>
#include <dlfcn.h>

#include "cocoa_common.h"

#include "config.h"

#include "core/options.h"
#include "vo.h"
#include "aspect.h"

#include "core/mp_fifo.h"
#include "talloc.h"

#include "core/input/input.h"
#include "core/input/keycodes.h"
#include "osx_common.h"
#include "core/mp_msg.h"

#ifndef NSOpenGLPFAOpenGLProfile
#define NSOpenGLPFAOpenGLProfile 99
#endif

#ifndef NSOpenGLProfileVersionLegacy
#define NSOpenGLProfileVersionLegacy 0x1000
#endif

#ifndef NSOpenGLProfileVersion3_2Core
#define NSOpenGLProfileVersion3_2Core 0x3200
#endif

#define NSLeftAlternateKeyMask (0x000020 | NSAlternateKeyMask)
#define NSRightAlternateKeyMask (0x000040 | NSAlternateKeyMask)

// add methods not available on OSX versions prior to 10.7
#ifndef MAC_OS_X_VERSION_10_7
@interface NSView (IntroducedInLion)
- (NSRect)convertRectToBacking:(NSRect)aRect;
- (void)setWantsBestResolutionOpenGLSurface:(BOOL)aBool;
@end
#endif

// add power management assertion not available on OSX versions prior to 10.7
#ifndef kIOPMAssertionTypePreventUserIdleDisplaySleep
#define kIOPMAssertionTypePreventUserIdleDisplaySleep \
    CFSTR("PreventUserIdleDisplaySleep")
#endif

@interface GLMPlayerWindow : NSWindow <NSWindowDelegate> {
    struct vo *_vo;
}
- (void)setVideoOutput:(struct vo *)vo;
- (BOOL)canBecomeKeyWindow;
- (BOOL)canBecomeMainWindow;
- (void)fullscreen;
- (void)mouseEvent:(NSEvent *)theEvent;
- (void)mulSize:(float)multiplier;
- (void)setContentSize:(NSSize)newSize keepCentered:(BOOL)keepCentered;
@end

@interface GLMPlayerOpenGLView : NSView
@end

struct vo_cocoa_state {
    NSAutoreleasePool *pool;
    GLMPlayerWindow *window;
    NSOpenGLContext *glContext;
    NSOpenGLPixelFormat *pixelFormat;

    NSSize current_video_size;
    NSSize previous_video_size;

    NSRect screen_frame;
    NSScreen *screen_handle;
    NSArray *screen_array;

    NSInteger windowed_mask;
    NSInteger fullscreen_mask;

    NSRect windowed_frame;

    NSString *window_title;

    NSInteger window_level;
    NSInteger fullscreen_window_level;

    int display_cursor;
    int cursor_timer;
    int cursor_autohide_delay;

    bool did_resize;
    bool out_fs_resize;

    IOPMAssertionID power_mgmt_assertion;
};

static int _instances = 0;

static void create_menu(void);

static struct vo_cocoa_state *vo_cocoa_init_state(struct vo *vo)
{
    struct vo_cocoa_state *s = talloc_ptrtype(vo, s);
    *s = (struct vo_cocoa_state){
        .pool = [[NSAutoreleasePool alloc] init],
        .did_resize = NO,
        .current_video_size = {0,0},
        .previous_video_size = {0,0},
        .windowed_mask = NSTitledWindowMask|NSClosableWindowMask|
            NSMiniaturizableWindowMask|NSResizableWindowMask,
        .fullscreen_mask = NSBorderlessWindowMask,
        .windowed_frame = {{0,0},{0,0}},
        .out_fs_resize = NO,
        .display_cursor = 1,
        .cursor_autohide_delay = vo->opts->cursor_autohide_delay,
        .power_mgmt_assertion = kIOPMNullAssertionID,
    };
    if (!vo_border) s->windowed_mask = NSBorderlessWindowMask;
    return s;
}

static bool supports_hidpi(NSView *view)
{
    SEL hdpi_selector = @selector(setWantsBestResolutionOpenGLSurface:);
    return is_osx_version_at_least(10, 7, 0) && view &&
           [view respondsToSelector:hdpi_selector];
}

bool vo_cocoa_gui_running(void)
{
    return _instances > 0;
}

void *vo_cocoa_glgetaddr(const char *s)
{
    void *ret = NULL;
    void *handle = dlopen(
        "/System/Library/Frameworks/OpenGL.framework/OpenGL",
        RTLD_LAZY | RTLD_LOCAL);
    if (!handle)
        return NULL;
    ret = dlsym(handle, s);
    dlclose(handle);
    return ret;
}

static void enable_power_management(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    if (!s->power_mgmt_assertion) return;
    IOPMAssertionRelease(s->power_mgmt_assertion);
    s->power_mgmt_assertion = kIOPMNullAssertionID;
}

static void disable_power_management(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    if (s->power_mgmt_assertion) return;

    CFStringRef assertion_type = kIOPMAssertionTypeNoDisplaySleep;
    if (is_osx_version_at_least(10, 7, 0))
        assertion_type = kIOPMAssertionTypePreventUserIdleDisplaySleep;

    IOPMAssertionCreateWithName(assertion_type, kIOPMAssertionLevelOn,
        CFSTR("org.mplayer2.power_mgmt"), &s->power_mgmt_assertion);
}

int vo_cocoa_init(struct vo *vo)
{
    vo->cocoa = vo_cocoa_init_state(vo);
    _instances++;

    NSApplicationLoad();
    NSApp = [NSApplication sharedApplication];
    [NSApp setActivationPolicy: NSApplicationActivationPolicyRegular];
    disable_power_management(vo);

    return 1;
}

void vo_cocoa_uninit(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    CGDisplayShowCursor(kCGDirectMainDisplay);
    enable_power_management(vo);
    [NSApp setPresentationOptions:NSApplicationPresentationDefault];

    [s->window release];
    s->window = nil;
    [s->glContext release];
    s->glContext = nil;
    [s->pool release];
    s->pool = nil;

    _instances--;
}

void vo_cocoa_pause(struct vo *vo)
{
    enable_power_management(vo);
}

void vo_cocoa_resume(struct vo *vo)
{
    disable_power_management(vo);
}

static int current_screen_has_dock_or_menubar(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    NSRect f  = s->screen_frame;
    NSRect vf = [s->screen_handle visibleFrame];
    return f.size.height > vf.size.height || f.size.width > vf.size.width;
}

static void update_screen_info(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    s->screen_array = [NSScreen screens];
    if (xinerama_screen >= (int)[s->screen_array count]) {
        mp_msg(MSGT_VO, MSGL_INFO, "[cocoa] Device ID %d does not exist, "
            "falling back to main device\n", xinerama_screen);
        xinerama_screen = -1;
    }

    if (xinerama_screen < 0) { // default behaviour
        if (! (s->screen_handle = [s->window screen]) )
            s->screen_handle = [s->screen_array objectAtIndex:0];
    } else {
        s->screen_handle = [s->screen_array objectAtIndex:(xinerama_screen)];
    }

    s->screen_frame = [s->screen_handle frame];
}

void vo_cocoa_update_xinerama_info(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    struct MPOpts *opts = vo->opts;

    update_screen_info(vo);
    aspect_save_screenres(vo, s->screen_frame.size.width,
                              s->screen_frame.size.height);
    opts->vo_screenwidth = s->screen_frame.size.width;
    opts->vo_screenheight = s->screen_frame.size.height;
    xinerama_x = s->screen_frame.origin.x;
    xinerama_y = s->screen_frame.origin.y;
}

int vo_cocoa_change_attributes(struct vo *vo)
{
    return 0;
}

static void resize_window(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    NSView *view = [s->window contentView];
    NSRect frame;

    if (supports_hidpi(view)) {
        frame = [view convertRectToBacking: [view frame]];
    } else {
        frame = [view frame];
    }

    vo->dwidth  = frame.size.width;
    vo->dheight = frame.size.height;
    [s->glContext update];
}

static void vo_set_level(struct vo *vo, int ontop)
{
    struct vo_cocoa_state *s = vo->cocoa;
    if (ontop) {
        s->window_level = NSNormalWindowLevel + 1;
    } else {
        s->window_level = NSNormalWindowLevel;
    }

    if (!vo_fs)
        [s->window setLevel:s->window_level];
}

void vo_cocoa_ontop(struct vo *vo)
{
    struct MPOpts *opts = vo->opts;
    opts->vo_ontop = !opts->vo_ontop;
    vo_set_level(vo, opts->vo_ontop);
}

static void update_state_sizes(struct vo_cocoa_state *s,
                               uint32_t d_width, uint32_t d_height)
{
    if (s->current_video_size.width > 0 || s->current_video_size.height > 0)
        s->previous_video_size = s->current_video_size;
    s->current_video_size = NSMakeSize(d_width, d_height);
}

static int create_window(struct vo *vo, uint32_t d_width, uint32_t d_height,
                         uint32_t flags, int gl3profile)
{
    struct vo_cocoa_state *s = vo->cocoa;
    struct MPOpts *opts = vo->opts;

    const NSRect window_rect = NSMakeRect(xinerama_x, xinerama_y,
                                          d_width, d_height);
    const NSRect glview_rect = NSMakeRect(0, 0, 100, 100);

    s->window =
        [[GLMPlayerWindow alloc] initWithContentRect:window_rect
                                           styleMask:s->windowed_mask
                                             backing:NSBackingStoreBuffered
                                               defer:NO];

    GLMPlayerOpenGLView *glView =
        [[GLMPlayerOpenGLView alloc] initWithFrame:glview_rect];

    // check for HiDPI support and enable it (available on 10.7 +)
    if (supports_hidpi(glView))
        [glView setWantsBestResolutionOpenGLSurface:YES];

    int i = 0;
    NSOpenGLPixelFormatAttribute attr[32];
    if (is_osx_version_at_least(10, 7, 0)) {
      attr[i++] = NSOpenGLPFAOpenGLProfile;
      if (gl3profile) {
          attr[i++] = NSOpenGLProfileVersion3_2Core;
      } else {
          attr[i++] = NSOpenGLProfileVersionLegacy;
      }
    } else if(gl3profile) {
        mp_msg(MSGT_VO, MSGL_ERR,
            "[cocoa] Invalid pixel format attribute "
            "(GL3 is not supported on OSX versions prior to 10.7)\n");
        return -1;
    }
    attr[i++] = NSOpenGLPFADoubleBuffer; // double buffered
    attr[i] = (NSOpenGLPixelFormatAttribute)0;

    s->pixelFormat =
        [[[NSOpenGLPixelFormat alloc] initWithAttributes:attr] autorelease];
    if (!s->pixelFormat) {
        mp_msg(MSGT_VO, MSGL_ERR,
            "[cocoa] Invalid pixel format attribute "
            "(GL3 not supported?)\n");
        return -1;
    }
    s->glContext =
        [[NSOpenGLContext alloc] initWithFormat:s->pixelFormat
                                   shareContext:nil];

    create_menu();

    [s->window setContentView:glView];
    [glView release];
    [s->window setAcceptsMouseMovedEvents:YES];
    [s->glContext setView:glView];
    [s->glContext makeCurrentContext];
    [s->window setVideoOutput:vo];

    [NSApp setDelegate:s->window];
    [s->window setDelegate:s->window];
    [s->window setContentSize:s->current_video_size];
    [s->window setContentAspectRatio:s->current_video_size];
    [s->window setFrameOrigin:NSMakePoint(vo->dx, vo->dy)];

    if (flags & VOFLAG_HIDDEN) {
        [s->window orderOut:nil];
    } else {
        [s->window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
    }

    if (flags & VOFLAG_FULLSCREEN)
        vo_cocoa_fullscreen(vo);

    vo_set_level(vo, opts->vo_ontop);

    return 0;
}

static void update_window(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;

    if (s->current_video_size.width  != s->previous_video_size.width ||
        s->current_video_size.height != s->previous_video_size.height) {
        if (vo_fs) {
            // we will resize as soon as we get out of fullscreen
            s->out_fs_resize = YES;
        } else {
            // only if we are not in fullscreen and the video size did
            // change we resize the window and set a new aspect ratio
            [s->window setContentSize:s->current_video_size
                         keepCentered:YES];
            [s->window setContentAspectRatio:s->current_video_size];
        }
    }
}

int vo_cocoa_create_window(struct vo *vo, uint32_t d_width,
                           uint32_t d_height, uint32_t flags,
                           int gl3profile)
{
    struct vo_cocoa_state *s = vo->cocoa;

    update_state_sizes(s, d_width, d_height);

    if (!(s->window || s->glContext)) {
        if (create_window(vo, d_width, d_height, flags, gl3profile) < 0)
            return -1;
    } else {
        update_window(vo);
    }

    resize_window(vo);

    if (s->window_title)
        [s->window_title release];

    s->window_title =
        [[NSString alloc] initWithUTF8String:vo_get_window_title(vo)];
    [s->window setTitle: s->window_title];

    return 0;
}

void vo_cocoa_swap_buffers(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    [s->glContext flushBuffer];
}

static void vo_cocoa_display_cursor(struct vo *vo, int requested_state)
{
    struct vo_cocoa_state *s = vo->cocoa;
    if (requested_state) {
        if (!vo_fs || s->cursor_autohide_delay > -2) {
            s->display_cursor = requested_state;
            CGDisplayShowCursor(kCGDirectMainDisplay);
        }
    } else {
        if (s->cursor_autohide_delay != -1) {
            s->display_cursor = requested_state;
            CGDisplayHideCursor(kCGDirectMainDisplay);
        }
    }
}

int vo_cocoa_check_events(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    NSEvent *event;
    int ms_time = (int) ([[NSProcessInfo processInfo] systemUptime] * 1000);

    // automatically hide mouse cursor
    if (vo_fs && s->display_cursor &&
        (ms_time - s->cursor_timer >= s->cursor_autohide_delay)) {
        vo_cocoa_display_cursor(vo, 0);
        s->cursor_timer = ms_time;
    }

    event = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:nil
                   inMode:NSEventTrackingRunLoopMode dequeue:YES];
    if (event == nil)
        return 0;
    [NSApp sendEvent:event];

    if (s->did_resize) {
        s->did_resize = NO;
        resize_window(vo);
        return VO_EVENT_RESIZE;
    }
    // Without SDL's bootstrap code (include SDL.h in mplayer.c),
    // on Leopard, we have trouble to get the play window automatically focused
    // when the app is actived. The Following code fix this problem.
    if ([event type] == NSAppKitDefined
            && [event subtype] == NSApplicationActivatedEventType) {
        [s->window makeMainWindow];
        [s->window makeKeyAndOrderFront:nil];
    }
    return 0;
}

void vo_cocoa_fullscreen(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    [s->window fullscreen];
    resize_window(vo);
}

int vo_cocoa_swap_interval(int enabled)
{
    [[NSOpenGLContext currentContext] setValues:&enabled
                                   forParameter:NSOpenGLCPSwapInterval];
    return 0;
}

void *vo_cocoa_cgl_context(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    return [s->glContext CGLContextObj];
}

void *vo_cocoa_cgl_pixel_format(struct vo *vo)
{
    return CGLGetPixelFormat(vo_cocoa_cgl_context(vo));
}

int vo_cocoa_cgl_color_size(struct vo *vo)
{
    GLint value;
    CGLDescribePixelFormat(vo_cocoa_cgl_pixel_format(vo), 0,
                           kCGLPFAColorSize, &value);
    switch (value) {
        case 32:
        case 24:
            return 8;
        case 16:
            return 5;
    }

    return 8;
}

static NSMenuItem *new_menu_item(NSMenu *parent_menu, NSString *title,
                                 SEL action, NSString *key_equivalent)
{
    NSMenuItem *new_item =
        [[NSMenuItem alloc] initWithTitle:title action:action
                                         keyEquivalent:key_equivalent];
    [parent_menu addItem:new_item];
    return [new_item autorelease];
}

static NSMenuItem *new_main_menu_item(NSMenu *parent_menu, NSMenu *child_menu,
                                      NSString *title)
{
    NSMenuItem *new_item =
        [[NSMenuItem alloc] initWithTitle:title action:nil
                                         keyEquivalent:@""];
    [new_item setSubmenu:child_menu];
    [parent_menu addItem:new_item];
    return [new_item autorelease];
}

void create_menu()
{
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    NSMenu *main_menu, *m_menu, *w_menu;
    NSMenuItem *app_menu_item;

    main_menu = [[NSMenu new] autorelease];
    app_menu_item = [[NSMenuItem new] autorelease];
    [main_menu addItem:app_menu_item];
    [NSApp setMainMenu: main_menu];

    m_menu = [[[NSMenu alloc] initWithTitle:@"Movie"] autorelease];
    new_menu_item(m_menu, @"Half Size", @selector(halfSize), @"0");
    new_menu_item(m_menu, @"Normal Size", @selector(normalSize), @"1");
    new_menu_item(m_menu, @"Double Size", @selector(doubleSize), @"2");

    new_main_menu_item(main_menu, m_menu, @"Movie");

    w_menu = [[[NSMenu alloc] initWithTitle:@"Window"] autorelease];
    new_menu_item(w_menu, @"Minimize", @selector(performMiniaturize:), @"m");
    new_menu_item(w_menu, @"Zoom", @selector(performZoom:), @"z");

    new_main_menu_item(main_menu, w_menu, @"Window");
    [pool release];
}

@implementation GLMPlayerWindow
- (void)setVideoOutput:(struct vo *)vo
{
    _vo = vo;
}

- (void)windowDidResize:(NSNotification *) notification
{
    if (_vo) {
        struct vo_cocoa_state *s = _vo->cocoa;
        s->did_resize = YES;
    }
}

- (void)fullscreen
{
    struct vo_cocoa_state *s = _vo->cocoa;
    if (!vo_fs) {
        update_screen_info(_vo);
        if (current_screen_has_dock_or_menubar(_vo))
            [NSApp setPresentationOptions:NSApplicationPresentationHideDock|
                NSApplicationPresentationHideMenuBar];
        s->windowed_frame = [self frame];
        [self setHasShadow:NO];
        [self setStyleMask:s->fullscreen_mask];
        [self setFrame:s->screen_frame display:YES animate:NO];
        vo_fs = VO_TRUE;
        vo_cocoa_display_cursor(_vo, 0);
        [self setMovableByWindowBackground: NO];
    } else {
        [NSApp setPresentationOptions:NSApplicationPresentationDefault];
        [self setHasShadow:YES];
        [self setStyleMask:s->windowed_mask];
        [self setTitle:s->window_title];
        [self setFrame:s->windowed_frame display:YES animate:NO];
        if (s->out_fs_resize) {
            [self setContentSize:s->current_video_size keepCentered:YES];
            s->out_fs_resize = NO;
        }
        [self setContentAspectRatio:s->current_video_size];
        vo_fs = VO_FALSE;
        vo_cocoa_display_cursor(_vo, 1);
        [self setMovableByWindowBackground: YES];
    }
}

- (BOOL)canBecomeMainWindow { return YES; }
- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)becomeFirstResponder { return YES; }
- (BOOL)resignFirstResponder { return YES; }
- (BOOL)windowShouldClose:(id)sender
{
    mplayer_put_key(_vo->key_fifo, KEY_CLOSE_WIN);
    // We have to wait for MPlayer to handle this,
    // otherwise we are in trouble if the
    // KEY_CLOSE_WIN handler is disabled
    return NO;
}

- (BOOL)isMovableByWindowBackground
{
    // this is only valid as a starting value. it will be rewritten in the
    // -fullscreen method.
    return !vo_fs;
}

- (void)handleQuitEvent:(NSAppleEventDescriptor*)e
         withReplyEvent:(NSAppleEventDescriptor*)r
{
    mplayer_put_key(_vo->key_fifo, KEY_CLOSE_WIN);
}

- (void)keyDown:(NSEvent *)theEvent
{
    unsigned char charcode;
    if (([theEvent modifierFlags] & NSRightAlternateKeyMask) ==
            NSRightAlternateKeyMask)
        charcode = *[[theEvent characters] UTF8String];
    else
        charcode = [[theEvent charactersIgnoringModifiers] characterAtIndex:0];

    int key = convert_key([theEvent keyCode], charcode);

    if (key > -1) {
        if ([theEvent modifierFlags] & NSShiftKeyMask)
            key |= KEY_MODIFIER_SHIFT;
        if ([theEvent modifierFlags] & NSControlKeyMask)
            key |= KEY_MODIFIER_CTRL;
        if (([theEvent modifierFlags] & NSLeftAlternateKeyMask) ==
                NSLeftAlternateKeyMask)
            key |= KEY_MODIFIER_ALT;
        if ([theEvent modifierFlags] & NSCommandKeyMask)
            key |= KEY_MODIFIER_META;
        mplayer_put_key(_vo->key_fifo, key);
    }
}

- (void)mouseMoved: (NSEvent *) theEvent
{
    if (vo_fs)
        vo_cocoa_display_cursor(_vo, 1);
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    [self mouseEvent: theEvent];
}

- (void)mouseDown:(NSEvent *)theEvent
{
    [self mouseEvent: theEvent];
}

- (void)mouseUp:(NSEvent *)theEvent
{
    [self mouseEvent: theEvent];
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    [self mouseEvent: theEvent];
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
    [self mouseEvent: theEvent];
}

- (void)otherMouseDown:(NSEvent *)theEvent
{
    [self mouseEvent: theEvent];
}

- (void)otherMouseUp:(NSEvent *)theEvent
{
    [self mouseEvent: theEvent];
}

- (void)scrollWheel:(NSEvent *)theEvent
{
    if ([theEvent deltaY] > 0)
        mplayer_put_key(_vo->key_fifo, MOUSE_BTN3);
    else
        mplayer_put_key(_vo->key_fifo, MOUSE_BTN4);
}

- (void)mouseEvent:(NSEvent *)theEvent
{
    if ([theEvent buttonNumber] >= 0 && [theEvent buttonNumber] <= 9) {
        int buttonNumber = [theEvent buttonNumber];
        // Fix to mplayer defined button order: left, middle, right
        if (buttonNumber == 1)  buttonNumber = 2;
        else if (buttonNumber == 2) buttonNumber = 1;
        switch ([theEvent type]) {
            case NSLeftMouseDown:
            case NSRightMouseDown:
            case NSOtherMouseDown:
                mplayer_put_key(_vo->key_fifo,
                                (MOUSE_BTN0 + buttonNumber) | MP_KEY_DOWN);
                // Looks like Cocoa doesn't create MouseUp events when we are
                // doing the second click in a double click. Put in the key_fifo
                // the key that would be put from the MouseUp handling code.
                if([theEvent clickCount] == 2)
                   mplayer_put_key(_vo->key_fifo, MOUSE_BTN0 + buttonNumber);
                break;
            case NSLeftMouseUp:
            case NSRightMouseUp:
            case NSOtherMouseUp:
                mplayer_put_key(_vo->key_fifo, MOUSE_BTN0 + buttonNumber);
                break;
        }
    }
}

- (void)applicationWillBecomeActive:(NSNotification *)aNotification
{
    if (vo_fs && current_screen_has_dock_or_menubar(_vo)) {
        [NSApp setPresentationOptions:NSApplicationPresentationHideDock|
                                      NSApplicationPresentationHideMenuBar];
    }
}

- (void)applicationWillResignActive:(NSNotification *)aNotification
{
    if (vo_fs) {
        [NSApp setPresentationOptions:NSApplicationPresentationDefault];
    }
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
    // Install an event handler so the Quit menu entry works
    // The proper way using NSApp setDelegate: and
    // applicationShouldTerminate: does not work,
    // probably NSApplication never installs its handler.
    [[NSAppleEventManager sharedAppleEventManager]
        setEventHandler:self
        andSelector:@selector(handleQuitEvent:withReplyEvent:)
        forEventClass:kCoreEventClass
        andEventID:kAEQuitApplication];
}

- (void)normalSize
{
    struct vo_cocoa_state *s = _vo->cocoa;
    if (!vo_fs)
        [self setContentSize:s->current_video_size keepCentered:YES];
}

- (void)halfSize { [self mulSize:0.5f];}

- (void)doubleSize { [self mulSize:2.0f];}

- (void)mulSize:(float)multiplier
{
    if (!vo_fs) {
        struct vo_cocoa_state *s = _vo->cocoa;
        NSSize size = [[self contentView] frame].size;
        size.width  = s->current_video_size.width  * (multiplier);
        size.height = s->current_video_size.height * (multiplier);
        [self setContentSize:size keepCentered:YES];
    }
}

- (void)setCenteredContentSize:(NSSize)ns
{
    NSRect nf = [self frame];
    NSRect vf = [[self screen] visibleFrame];
    NSRect cb = [[self contentView] bounds];
    int title_height = nf.size.height - cb.size.height;
    double ratio = (double)ns.width / (double)ns.height;

    // clip the new size to the visibleFrame's size if needed
    if (ns.width > vf.size.width || ns.height + title_height > vf.size.height) {
        ns = vf.size;
        ns.height -= title_height; // make space for the title bar

        if (ns.width > ns.height) {
            ns.height = ((double)ns.width * 1/ratio + 0.5);
        } else {
            ns.width = ((double)ns.height * ratio + 0.5);
        }
    }

    int dw = nf.size.width - ns.width;
    int dh = nf.size.height - ns.height - title_height;

    nf.origin.x += dw / 2;
    nf.origin.y += dh / 2;

    NSRect new_frame =
        NSMakeRect(nf.origin.x, nf.origin.y, ns.width, ns.height + title_height);
    [self setFrame:new_frame display:YES animate:NO];
}

- (void)setContentSize:(NSSize)ns keepCentered:(BOOL)keepCentered
{
    if (keepCentered) {
        [self setCenteredContentSize:ns];
    } else {
        [self setContentSize:ns];
    }
}
@end

@implementation GLMPlayerOpenGLView
- (void)drawRect: (NSRect)rect
{
    [[NSColor clearColor] set];
    NSRectFill([self bounds]);
}
@end
