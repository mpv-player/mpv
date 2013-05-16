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
#include <libavutil/common.h>

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

#include "osdep/macosx_application.h"

#ifndef NSOpenGLPFAOpenGLProfile
#define NSOpenGLPFAOpenGLProfile 99
#endif

#ifndef NSOpenGLProfileVersionLegacy
#define NSOpenGLProfileVersionLegacy 0x1000
#endif

#ifndef NSOpenGLProfileVersion3_2Core
#define NSOpenGLProfileVersion3_2Core 0x3200
#endif

#define NSLeftAlternateKeyMask  (0x000020 | NSAlternateKeyMask)
#define NSRightAlternateKeyMask (0x000040 | NSAlternateKeyMask)

static bool LeftAltPressed(NSEvent *event)
{
    return ([event modifierFlags] & NSLeftAlternateKeyMask) ==
            NSLeftAlternateKeyMask;
}

static bool RightAltPressed(NSEvent *event)
{
    return ([event modifierFlags] & NSRightAlternateKeyMask) ==
            NSRightAlternateKeyMask;
}

// add methods not available on OSX versions prior to 10.7
#ifndef MAC_OS_X_VERSION_10_7
@interface NSView (IntroducedInLion)
- (NSRect)convertRectToBacking:(NSRect)aRect;
- (void)setWantsBestResolutionOpenGLSurface:(BOOL)aBool;
@end
@interface NSEvent (IntroducedInLion)
- (BOOL)hasPreciseScrollingDeltas;
@end
#endif

// add power management assertion not available on OSX versions prior to 10.7
#ifndef kIOPMAssertionTypePreventUserIdleDisplaySleep
#define kIOPMAssertionTypePreventUserIdleDisplaySleep \
    CFSTR("PreventUserIdleDisplaySleep")
#endif

@interface GLMPlayerWindow : NSWindow <NSWindowDelegate>
- (BOOL)canBecomeKeyWindow;
- (BOOL)canBecomeMainWindow;
- (void)fullscreen;
- (void)mouseEvent:(NSEvent *)theEvent;
- (void)mulSize:(float)multiplier;
- (int)titleHeight;
- (NSRect)clipFrame:(NSRect)frame withContentAspect:(NSSize) aspect;
- (void)setContentSize:(NSSize)newSize keepCentered:(BOOL)keepCentered;
@property(nonatomic, assign) struct vo *videoOutput;
@end

@interface GLMPlayerOpenGLView : NSView
@end

struct vo_cocoa_input_queue {
    NSMutableArray *fifo;
};

static int vo_cocoa_input_queue_free(void *ptr)
{
    struct vo_cocoa_input_queue *iq = ptr;
    [iq->fifo release];
    return 0;
}

static struct vo_cocoa_input_queue *vo_cocoa_input_queue_init(void *talloc_ctx)
{
    struct vo_cocoa_input_queue *iq = talloc_ptrtype(talloc_ctx, iq);
    *iq = (struct vo_cocoa_input_queue) {
        .fifo  = [[NSMutableArray alloc] init],
    };
    talloc_set_destructor(iq, vo_cocoa_input_queue_free);
    return iq;
}

static void cocoa_async_put_key(struct vo_cocoa_input_queue *iq, int key)
{
    @synchronized (iq->fifo) {
        [iq->fifo addObject:[NSNumber numberWithInt:key]];
    }
}

static int cocoa_sync_get_key(struct vo_cocoa_input_queue *iq)
{
    int r = -1;

    @synchronized (iq->fifo) {
        if ([iq->fifo count] > 0) {
            r = [[iq->fifo objectAtIndex:0] intValue];
            [iq->fifo removeObjectAtIndex:0];
        }
    }

    return r;
}

struct vo_cocoa_state {
    GLMPlayerWindow *window;
    NSOpenGLContext *glContext;
    NSOpenGLPixelFormat *pixelFormat;

    NSSize current_video_size;
    NSSize previous_video_size;

    NSRect screen_frame;
    NSRect fsscreen_frame;
    NSScreen *screen_handle;

    NSInteger windowed_mask;
    NSInteger fullscreen_mask;

    NSRect windowed_frame;

    NSString *window_title;

    NSInteger window_level;

    struct aspect_data aspdat;

    int display_cursor;
    int cursor_timer;
    int vo_cursor_autohide_delay;
    bool will_make_front;

    bool did_resize;
    bool did_async_resize;
    bool out_fs_resize;

    IOPMAssertionID power_mgmt_assertion;

    CGFloat accumulated_scroll;

    NSLock *lock;

    bool enable_resize_redraw;
    void (*resize_redraw)(struct vo *vo, int w, int h);

    struct vo_cocoa_input_queue *input_queue;
};

static struct vo_cocoa_state *vo_cocoa_init_state(struct vo *vo)
{
    struct vo_cocoa_state *s = talloc_ptrtype(vo, s);
    *s = (struct vo_cocoa_state){
        .did_resize = NO,
        .did_async_resize = NO,
        .current_video_size = {0,0},
        .previous_video_size = {0,0},
        .windowed_mask = NSTitledWindowMask|NSClosableWindowMask|
            NSMiniaturizableWindowMask|NSResizableWindowMask,
        .fullscreen_mask = NSBorderlessWindowMask,
        .windowed_frame = {{0,0},{0,0}},
        .out_fs_resize = NO,
        .display_cursor = 1,
        .vo_cursor_autohide_delay = vo->opts->cursor_autohide_delay,
        .will_make_front = YES,
        .power_mgmt_assertion = kIOPMNullAssertionID,
        .accumulated_scroll = 0,
        .lock = [[NSLock alloc] init],
        .input_queue = vo_cocoa_input_queue_init(s),
        .enable_resize_redraw = NO,
    };
    if (!vo->opts->border) s->windowed_mask = NSBorderlessWindowMask;

    return s;
}

static bool supports_hidpi(NSView *view)
{
    SEL hdpi_selector = @selector(setWantsBestResolutionOpenGLSurface:);
    return is_osx_version_at_least(10, 7, 0) && view &&
           [view respondsToSelector:hdpi_selector];
}

static NSRect to_pixels(struct vo *vo, NSRect frame)
{
    struct vo_cocoa_state *s = vo->cocoa;
    NSView *view = [s->window contentView];

    if (supports_hidpi(view)) {
        return [view convertRectToBacking: frame];
    } else {
        return frame;
    }
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
        CFSTR("io.mpv.power_management"), &s->power_mgmt_assertion);
}

int vo_cocoa_init(struct vo *vo)
{
    vo->cocoa = vo_cocoa_init_state(vo);
    vo->wakeup_period = 0.02;
    disable_power_management(vo);

    return 1;
}

void vo_cocoa_uninit(struct vo *vo)
{
    dispatch_sync(dispatch_get_main_queue(), ^{
        struct vo_cocoa_state *s = vo->cocoa;
        CGDisplayShowCursor(kCGDirectMainDisplay);
        enable_power_management(vo);
        [NSApp setPresentationOptions:NSApplicationPresentationDefault];

        [s->window release];
        s->window = nil;
        [s->glContext release];
        s->glContext = nil;
    });
}

void vo_cocoa_pause(struct vo *vo)
{
    enable_power_management(vo);
}

void vo_cocoa_resume(struct vo *vo)
{
    disable_power_management(vo);
}

void vo_cocoa_register_resize_callback(struct vo *vo,
                                       void (*cb)(struct vo *vo, int w, int h))
{
    struct vo_cocoa_state *s = vo->cocoa;
    s->resize_redraw = cb;
}

static int current_screen_has_dock_or_menubar(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    NSRect f  = s->screen_frame;
    NSRect vf = [s->screen_handle visibleFrame];
    return f.size.height > vf.size.height || f.size.width > vf.size.width;
}

static int get_screen_handle(int identifier, NSWindow *window, NSScreen **screen) {
    NSArray *screens  = [NSScreen screens];
    int n_of_displays = [screens count];

    if (identifier >= n_of_displays) { // check if the identifier is out of bounds
        mp_msg(MSGT_VO, MSGL_INFO, "[cocoa] Screen ID %d does not exist, "
            "falling back to main device\n", identifier);
        identifier = -1;
    }

    if (identifier < 0) {
        // default behaviour gets either the window screen or the main screen
        // if window is not available
        if (! (*screen = [window screen]) )
            *screen = [screens objectAtIndex:0];
        return 0;
    } else {
        *screen = [screens objectAtIndex:(identifier)];
        return 1;
    }
}

static void update_screen_info(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    struct mp_vo_opts *opts = vo->opts;
    NSScreen *ws, *fss;

    get_screen_handle(opts->screen_id, s->window, &ws);
    s->screen_frame = [ws frame];

    get_screen_handle(opts->fsscreen_id, s->window, &fss);
    s->fsscreen_frame = [fss frame];
}

void vo_cocoa_update_xinerama_info(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    struct mp_vo_opts *opts = vo->opts;

    update_screen_info(vo);
    aspect_save_screenres(vo, s->screen_frame.size.width,
                              s->screen_frame.size.height);
    opts->screenwidth = s->screen_frame.size.width;
    opts->screenheight = s->screen_frame.size.height;
    vo->xinerama_x = s->screen_frame.origin.x;
    vo->xinerama_y = s->screen_frame.origin.y;
}

static void resize_window(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    NSView *view = [s->window contentView];
    NSRect frame = to_pixels(vo, [view frame]);
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

    [s->window setLevel:s->window_level];
}

void vo_cocoa_ontop(struct vo *vo)
{
    struct mp_vo_opts *opts = vo->opts;
    opts->ontop = !opts->ontop;
    vo_set_level(vo, opts->ontop);
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
    const NSRect window_rect = NSMakeRect(vo->xinerama_x, vo->xinerama_y,
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

    cocoa_register_menu_item_action(MPM_H_SIZE,   @selector(halfSize));
    cocoa_register_menu_item_action(MPM_N_SIZE,   @selector(normalSize));
    cocoa_register_menu_item_action(MPM_D_SIZE,   @selector(doubleSize));
    cocoa_register_menu_item_action(MPM_MINIMIZE, @selector(performMiniaturize:));
    cocoa_register_menu_item_action(MPM_ZOOM,     @selector(performZoom:));

    [s->window setRestorable:NO];
    [s->window setContentView:glView];
    [glView release];
    [s->window setAcceptsMouseMovedEvents:YES];
    [s->glContext setView:glView];
    s->window.videoOutput = vo;

    [s->window setDelegate:s->window];
    [s->window makeMainWindow];

    [s->window setContentSize:s->current_video_size keepCentered:YES];
    [s->window setContentAspectRatio:s->current_video_size];

    return 0;
}

static void update_window(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;

    if (s->current_video_size.width  != s->previous_video_size.width ||
        s->current_video_size.height != s->previous_video_size.height) {
        if (vo->opts->fs) {
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

static void resize_redraw(struct vo *vo, int width, int height)
{
    struct vo_cocoa_state *s = vo->cocoa;
    if (s->resize_redraw) {
        vo_cocoa_set_current_context(vo, true);
        [s->glContext update];
        s->resize_redraw(vo, width, height);
        [s->glContext flushBuffer];
        s->did_async_resize = YES;
        vo_cocoa_set_current_context(vo, false);
    }
}

int vo_cocoa_config_window(struct vo *vo, uint32_t d_width,
                           uint32_t d_height, uint32_t flags,
                           int gl3profile)
{
    struct vo_cocoa_state *s = vo->cocoa;
    struct mp_vo_opts *opts = vo->opts;
    __block int rv = 0;
    s->enable_resize_redraw = NO;

    dispatch_sync(dispatch_get_main_queue(), ^{
        if (vo->config_count > 0) {
            NSPoint origin = [s->window frame].origin;
            vo->dx = origin.x;
            vo->dy = origin.y;
        }

        s->aspdat = vo->aspdat;
        update_state_sizes(s, d_width, d_height);

        if (!(s->window || s->glContext)) {
            if (create_window(vo, d_width, d_height, flags, gl3profile) < 0)
                rv = -1;
        } else {
            update_window(vo);
        }

        [s->window setFrameOrigin:NSMakePoint(vo->dx, vo->dy)];


        if (flags & VOFLAG_HIDDEN) {
            [s->window orderOut:nil];
        } else if (![s->window isVisible] && s->will_make_front) {
            s->will_make_front = NO;
            [s->window makeKeyAndOrderFront:nil];
            [NSApp activateIgnoringOtherApps:YES];
        }

        if (flags & VOFLAG_FULLSCREEN && !vo->opts->fs)
            vo_cocoa_fullscreen(vo);

        vo_set_level(vo, opts->ontop);

        resize_window(vo);

        if (s->window_title)
            [s->window_title release];

        s->window_title =
            [[NSString alloc] initWithUTF8String:vo_get_window_title(vo)];
        [s->window setTitle: s->window_title];

        if (opts->native_fs) {
            [s->window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
            [NSApp setPresentationOptions:NSFullScreenWindowMask];
        }
    });

    [vo->cocoa->glContext makeCurrentContext];
    s->enable_resize_redraw = YES;

    return rv;
}

static bool resize_callback_registered(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    return s->enable_resize_redraw && !!s->resize_redraw;
}

void vo_cocoa_set_current_context(struct vo *vo, bool current)
{
    struct vo_cocoa_state *s = vo->cocoa;
    if (current) {
        [s->lock lock];
        [s->glContext makeCurrentContext];
    } else {
        [NSOpenGLContext clearCurrentContext];
        [s->lock unlock];
    }
}

void vo_cocoa_swap_buffers(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    if (s->did_async_resize && resize_callback_registered(vo)) {
        // when in live resize the GL view asynchronously updates itself from
        // it's drawRect: implementation and calls flushBuffer. This means the
        // backbuffer is probably in an inconsistent state, so we skip one
        // flushBuffer call here on the playloop thread.
        s->did_async_resize = NO;
    } else {
        [s->glContext flushBuffer];
    }
}

static void vo_cocoa_display_cursor(struct vo *vo, int requested_state)
{
    struct vo_cocoa_state *s = vo->cocoa;
    if (requested_state) {
        if (!vo->opts->fs || s->vo_cursor_autohide_delay > -2) {
            s->display_cursor = requested_state;
            CGDisplayShowCursor(kCGDirectMainDisplay);
        }
    } else {
        if (s->vo_cursor_autohide_delay != -1) {
            s->display_cursor = requested_state;
            CGDisplayHideCursor(kCGDirectMainDisplay);
        }
    }
}

int vo_cocoa_check_events(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;

    int ms_time = (int) ([[NSProcessInfo processInfo] systemUptime] * 1000);

    // automatically hide mouse cursor
    if (vo->opts->fs && s->display_cursor &&
        (ms_time - s->cursor_timer >= s->vo_cursor_autohide_delay)) {
        vo_cocoa_display_cursor(vo, 0);
        s->cursor_timer = ms_time;
    }

    int key = cocoa_sync_get_key(s->input_queue);
    if (key >= 0) mplayer_put_key(vo->key_fifo, key);

    if (s->did_resize) {
        s->did_resize = NO;
        resize_window(vo);
        return VO_EVENT_RESIZE;
    }

    return 0;
}

void vo_cocoa_fullscreen(struct vo *vo)
{
    if (![NSThread isMainThread]) {
        // This is the secondary thread, unlock since we are going to invoke a
        // method synchronously on the GUI thread using Cocoa.
        vo_cocoa_set_current_context(vo, false);
    }

    struct vo_cocoa_state *s = vo->cocoa;
    [s->window performSelectorOnMainThread:@selector(fullscreen)
                                withObject:nil
                             waitUntilDone:YES];


    if (![NSThread isMainThread]) {
        // Now lock again!
        vo_cocoa_set_current_context(vo, true);
    }
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

@implementation GLMPlayerWindow
@synthesize videoOutput = _video_output;
- (void)windowDidResize:(NSNotification *) notification
{
    if (self.videoOutput) {
        struct vo_cocoa_state *s = self.videoOutput->cocoa;
        s->did_resize = YES;
    }
}
- (void)fullscreen
{
    struct vo_cocoa_state *s = self.videoOutput->cocoa;
    struct mp_vo_opts *opts = self.videoOutput->opts;

    if (opts->native_fs) {
        if (!opts->fs) {
            opts->fs = VO_TRUE;
            [self setContentResizeIncrements:NSMakeSize(1, 1)];
            vo_cocoa_display_cursor(self.videoOutput, 0);
        } else {
            opts->fs = VO_FALSE;
            [self setContentAspectRatio:s->current_video_size];
            vo_cocoa_display_cursor(self.videoOutput, 1);
        }

        [self toggleFullScreen:nil];
    } else {
        if (!opts->fs) {
            opts->fs = VO_TRUE;
            update_screen_info(self.videoOutput);
            if (current_screen_has_dock_or_menubar(self.videoOutput))
                [NSApp setPresentationOptions:NSApplicationPresentationHideDock|
                    NSApplicationPresentationHideMenuBar];
            s->windowed_frame = [self frame];
            [self setHasShadow:NO];
            [self setStyleMask:s->fullscreen_mask];
            [self setFrame:s->fsscreen_frame display:YES animate:NO];
            vo_cocoa_display_cursor(self.videoOutput, 0);
        } else {
            opts->fs = VO_FALSE;
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
            vo_cocoa_display_cursor(self.videoOutput, 1);
        }
    }

    resize_window(self.videoOutput);
}

- (BOOL)canBecomeMainWindow { return YES; }
- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)becomeFirstResponder { return YES; }
- (BOOL)resignFirstResponder { return YES; }
- (BOOL)windowShouldClose:(id)sender
{
    struct vo_cocoa_state *s = self.videoOutput->cocoa;
    cocoa_async_put_key(s->input_queue, MP_KEY_CLOSE_WIN);
    // We have to wait for MPlayer to handle this,
    // otherwise we are in trouble if the
    // MP_KEY_CLOSE_WIN handler is disabled
    return NO;
}

- (BOOL)isMovableByWindowBackground
{
    if (self.videoOutput) {
        return !self.videoOutput->opts->fs;
    } else {
        return YES;
    }
}

- (void)keyDown:(NSEvent *)theEvent
{
    struct vo_cocoa_state *s = self.videoOutput->cocoa;
    NSString *chars;

    if (RightAltPressed(theEvent))
        chars = [theEvent characters];
    else
        chars = [theEvent charactersIgnoringModifiers];

    int key = convert_key([theEvent keyCode], *[chars UTF8String]);

    if (key > -1) {
        if ([theEvent modifierFlags] & NSShiftKeyMask)
            key |= MP_KEY_MODIFIER_SHIFT;
        if ([theEvent modifierFlags] & NSControlKeyMask)
            key |= MP_KEY_MODIFIER_CTRL;
        if (LeftAltPressed(theEvent))
            key |= MP_KEY_MODIFIER_ALT;
        if ([theEvent modifierFlags] & NSCommandKeyMask)
            key |= MP_KEY_MODIFIER_META;

        cocoa_async_put_key(s->input_queue, key);
    }
}

- (void)mouseMoved: (NSEvent *) theEvent
{
    if (self.videoOutput->opts->fs)
        vo_cocoa_display_cursor(self.videoOutput, 1);
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
    struct vo_cocoa_state *s = self.videoOutput->cocoa;

    CGFloat delta;
    // Use the dimention with the most delta as the scrolling one
    if (FFABS([theEvent deltaY]) > FFABS([theEvent deltaX])) {
        delta = [theEvent deltaY];
    } else {
        delta = - [theEvent deltaX];
    }

    if (is_osx_version_at_least(10, 7, 0) &&
        [theEvent hasPreciseScrollingDeltas]) {
        s->accumulated_scroll += delta;
        static const CGFloat threshold = 10;
        while (s->accumulated_scroll >= threshold) {
            s->accumulated_scroll -= threshold;
            cocoa_async_put_key(s->input_queue, MP_MOUSE_BTN3);
        }
        while (s->accumulated_scroll <= -threshold) {
            s->accumulated_scroll += threshold;
            cocoa_async_put_key(s->input_queue, MP_MOUSE_BTN4);
        }
    } else {
        if (delta > 0)
            cocoa_async_put_key(s->input_queue, MP_MOUSE_BTN3);
        else
            cocoa_async_put_key(s->input_queue, MP_MOUSE_BTN4);
    }
}

- (void)mouseEvent:(NSEvent *)theEvent
{
    if ([theEvent buttonNumber] >= 0 && [theEvent buttonNumber] <= 9) {
        struct vo_cocoa_state *s = self.videoOutput->cocoa;
        int buttonNumber = [theEvent buttonNumber];
        // Fix to mplayer defined button order: left, middle, right
        if (buttonNumber == 1)  buttonNumber = 2;
        else if (buttonNumber == 2) buttonNumber = 1;
        switch ([theEvent type]) {
            case NSLeftMouseDown:
            case NSRightMouseDown:
            case NSOtherMouseDown:
                cocoa_async_put_key(
                    s->input_queue,
                    (MP_MOUSE_BTN0 + buttonNumber) | MP_KEY_STATE_DOWN);
                // Looks like Cocoa doesn't create MouseUp events when we are
                // doing the second click in a double click. Put in the key_fifo
                // the key that would be put from the MouseUp handling code.
                if([theEvent clickCount] == 2)
                    cocoa_async_put_key(s->input_queue,
                                        MP_MOUSE_BTN0 + buttonNumber);
                break;
            case NSLeftMouseUp:
            case NSRightMouseUp:
            case NSOtherMouseUp:
                cocoa_async_put_key(s->input_queue,
                                    MP_MOUSE_BTN0 + buttonNumber);
                break;
        }
    }
}

- (void)normalSize { [self mulSize:1.0f]; }

- (void)halfSize { [self mulSize:0.5f];}

- (void)doubleSize { [self mulSize:2.0f];}

- (void)mulSize:(float)multiplier
{
    if (!self.videoOutput->opts->fs) {
        NSSize size = {
            .width  = self.videoOutput->cocoa->aspdat.prew * multiplier,
            .height = self.videoOutput->cocoa->aspdat.preh * multiplier
        };
        [self setContentSize:size keepCentered:YES];
    }
}

- (int)titleHeight
{
    NSRect of    = [self frame];
    NSRect cb    = [[self contentView] bounds];
    return of.size.height - cb.size.height;
}

- (NSRect)clipFrame:(NSRect)frame withContentAspect:(NSSize) aspect
{
    NSRect vf    = [[self screen] visibleFrame];
    double ratio = (double)aspect.width / (double)aspect.height;

    // clip frame to screens visibile frame
    frame = CGRectIntersection(frame, vf);

    NSSize s = frame.size;
    s.height -= [self titleHeight];

    if (s.width > s.height) {
        s.width  = ((double)s.height * ratio);
    } else {
        s.height = ((double)s.width * 1.0/ratio);
    }

    s.height += [self titleHeight];
    frame.size = s;

    return frame;
}

- (void)setCenteredContentSize:(NSSize)ns
{
#define get_center(x) NSMakePoint(CGRectGetMidX((x)), CGRectGetMidY((x)))
    NSRect of    = [self frame];
    NSRect vf    = [[self screen] visibleFrame];
    NSPoint old_center = get_center(of);

    NSRect nf = NSMakeRect(vf.origin.x, vf.origin.y,
                           ns.width, ns.height + [self titleHeight]);

    nf = [self clipFrame:nf withContentAspect:ns];

    NSPoint new_center = get_center(nf);

    int dx0 = old_center.x - new_center.x;
    int dy0 = old_center.y - new_center.y;

    nf.origin.x += dx0;
    nf.origin.y += dy0;

    [self setFrame:nf display:YES animate:NO];
#undef get_center
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
    GLMPlayerWindow *window = (GLMPlayerWindow *)[self window];
    struct vo *vo = [window videoOutput];
    if (vo && resize_callback_registered(vo)) {
        NSSize size = to_pixels(vo, [self bounds]).size;
        resize_redraw(vo, size.width, size.height);
    } else {
        [[NSColor clearColor] set];
        NSRectFill([self bounds]);
    }
}
@end
