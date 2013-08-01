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

#include "core/input/input.h"
#include "talloc.h"

#include "core/mp_msg.h"

#include "osdep/macosx_application.h"
#include "osdep/macosx_events.h"
#include "osdep/macosx_compat.h"

@interface GLMPlayerWindow : NSWindow <NSWindowDelegate>
- (BOOL)canBecomeKeyWindow;
- (BOOL)canBecomeMainWindow;
- (void)fullscreen;
- (void)mulSize:(float)multiplier;
- (int)titleHeight;
- (void)setCenteredContentSize:(NSSize)newSize;
@property(nonatomic, assign) struct vo *videoOutput;
@end

@interface GLMPlayerOpenGLView : NSView
@property(nonatomic, retain) NSTrackingArea *tracker;
@property(nonatomic, assign) struct vo *videoOutput;
- (BOOL)containsMouseLocation;
- (void)recalcDraggableState;
@property(nonatomic, assign, getter=hasMouseDown) BOOL mouseDown;
@end

@interface NSScreen (mpvadditions)
- (BOOL)hasDock;
- (BOOL)hasMenubar;
@end

@interface NSEvent (mpvadditions)
- (int)mpvButtonNumber;
@end

struct vo_cocoa_state {
    GLMPlayerWindow *window;
    GLMPlayerOpenGLView *view;
    NSOpenGLContext *glContext;
    NSOpenGLPixelFormat *pixelFormat;

    NSSize current_video_size;
    NSSize previous_video_size;

    NSScreen *current_screen;
    NSScreen *fs_screen;

    NSInteger window_level;

    struct aspect_data aspdat;

    bool did_resize;
    bool did_async_resize;
    bool out_fs_resize;
    bool want_redraw;

    IOPMAssertionID power_mgmt_assertion;

    CGFloat accumulated_scroll;

    NSLock *lock;
    bool enable_resize_redraw;
    void (*resize_redraw)(struct vo *vo, int w, int h);

    struct mp_log *log;
};

static struct vo_cocoa_state *vo_cocoa_init_state(struct vo *vo)
{
    struct vo_cocoa_state *s = talloc_ptrtype(vo, s);
    *s = (struct vo_cocoa_state){
        .did_resize = NO,
        .did_async_resize = NO,
        .want_redraw = NO,
        .current_video_size = {0,0},
        .previous_video_size = {0,0},
        .out_fs_resize = NO,
        .power_mgmt_assertion = kIOPMNullAssertionID,
        .accumulated_scroll = 0,
        .lock = [[NSLock alloc] init],
        .enable_resize_redraw = NO,
        .log = mp_log_new(s, vo->log, "cocoa"),
    };
    return s;
}

static NSRect to_pixels(struct vo *vo, NSRect frame)
{
    struct vo_cocoa_state *s = vo->cocoa;
    return [s->view convertRectToBacking: frame];
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
    IOPMAssertionCreateWithName(
            kIOPMAssertionTypePreventUserIdleDisplaySleep,
            kIOPMAssertionLevelOn,
            CFSTR("io.mpv.video_playing_back"),
            &s->power_mgmt_assertion);
}

int vo_cocoa_init(struct vo *vo)
{
    vo->cocoa = vo_cocoa_init_state(vo);

    return 1;
}

static void vo_cocoa_set_cursor_visibility(struct vo *vo, bool visible)
{
    struct vo_cocoa_state *s = vo->cocoa;

    if (visible) {
        CGDisplayShowCursor(kCGDirectMainDisplay);
    } else if (vo->opts->fullscreen && ![s->view hasMouseDown] &&
               [s->view containsMouseLocation]) {
        CGDisplayHideCursor(kCGDirectMainDisplay);
    }
}

void vo_cocoa_uninit(struct vo *vo)
{
    dispatch_sync(dispatch_get_main_queue(), ^{
        struct vo_cocoa_state *s = vo->cocoa;
        vo_cocoa_set_cursor_visibility(vo, true);
        enable_power_management(vo);
        [NSApp setPresentationOptions:NSApplicationPresentationDefault];

        if (vo->opts->fullscreen)
            [[s->view window] release];

        [s->window release];
        s->window = nil;
        [s->glContext release];
        s->glContext = nil;
    });
}

void vo_cocoa_register_resize_callback(struct vo *vo,
                                       void (*cb)(struct vo *vo, int w, int h))
{
    struct vo_cocoa_state *s = vo->cocoa;
    s->resize_redraw = cb;
}

static int get_screen_handle(struct vo *vo, int identifier, NSWindow *window,
                             NSScreen **screen) {
    struct vo_cocoa_state *s = vo->cocoa;
    NSArray *screens  = [NSScreen screens];
    int n_of_displays = [screens count];

    if (identifier >= n_of_displays) { // check if the identifier is out of bounds
        MP_INFO(s, "Screen ID %d does not exist, falling back to main "
                    "device\n", identifier);
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
    get_screen_handle(vo, opts->screen_id, s->window, &s->current_screen);
    get_screen_handle(vo, opts->fsscreen_id, s->window, &s->fs_screen);
}

static void vo_cocoa_update_screen_info(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    struct mp_vo_opts *opts = vo->opts;

    update_screen_info(vo);

    NSRect r = [s->current_screen frame];

    aspect_save_screenres(vo, r.size.width, r.size.height);
    opts->screenwidth  = r.size.width;
    opts->screenheight = r.size.height;
    vo->xinerama_x     = r.origin.x;
    vo->xinerama_y     = r.origin.y;
}

static void resize_window(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    NSRect frame = to_pixels(vo, [s->view frame]);
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

    [[s->view window] setLevel:s->window_level];
    [s->window        setLevel:s->window_level];
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

static void resize_window_from_stored_size(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    [s->window setCenteredContentSize:s->current_video_size];
    [s->window setContentAspectRatio:s->current_video_size];
}

static void create_window(struct vo *vo, uint32_t d_width, uint32_t d_height,
                         uint32_t flags)
{
    struct vo_cocoa_state *s = vo->cocoa;
    struct mp_vo_opts *opts  = vo->opts;

    const NSRect contentRect = NSMakeRect(0, 0, d_width, d_height);

    int window_mask = 0;
    if (opts->border) {
        window_mask = NSTitledWindowMask|NSClosableWindowMask|
                      NSMiniaturizableWindowMask|NSResizableWindowMask;
    } else {
        window_mask = NSBorderlessWindowMask;
    }

    s->window =
        [[GLMPlayerWindow alloc] initWithContentRect:contentRect
                                           styleMask:window_mask
                                             backing:NSBackingStoreBuffered
                                               defer:NO];

    s->view = [[GLMPlayerOpenGLView alloc] initWithFrame:contentRect];
    [s->view setWantsBestResolutionOpenGLSurface:YES];

    cocoa_register_menu_item_action(MPM_H_SIZE,   @selector(halfSize));
    cocoa_register_menu_item_action(MPM_N_SIZE,   @selector(normalSize));
    cocoa_register_menu_item_action(MPM_D_SIZE,   @selector(doubleSize));
    cocoa_register_menu_item_action(MPM_MINIMIZE, @selector(performMiniaturize:));
    cocoa_register_menu_item_action(MPM_ZOOM,     @selector(performZoom:));

    [s->window setRestorable:NO];
    [s->window setContentView:s->view];
    [s->view release];
    [s->glContext setView:s->view];
    s->window.videoOutput = vo;
    s->view.videoOutput   = vo;

    [s->window setDelegate:s->window];
    [s->window makeMainWindow];

    [s->window setFrameOrigin:NSMakePoint(vo->dx, vo->dy)];
    [s->window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    vo_set_level(vo, opts->ontop);

    if (opts->native_fs) {
        [s->window setCollectionBehavior:
            NSWindowCollectionBehaviorFullScreenPrimary];
        [NSApp setPresentationOptions:NSFullScreenWindowMask];
    }
}

static NSOpenGLPixelFormatAttribute get_nsopengl_profile(int gl3profile) {
    if (gl3profile) {
        return NSOpenGLProfileVersion3_2Core;
    } else {
        return NSOpenGLProfileVersionLegacy;
    }
}

static int create_gl_context(struct vo *vo, int gl3profile)
{
    struct vo_cocoa_state *s = vo->cocoa;

    NSOpenGLPixelFormatAttribute attr[] = {
        NSOpenGLPFAOpenGLProfile,
        get_nsopengl_profile(gl3profile),
        NSOpenGLPFADoubleBuffer,
        0
    };

    s->pixelFormat =
        [[[NSOpenGLPixelFormat alloc] initWithAttributes:attr] autorelease];

    if (!s->pixelFormat) {
        MP_ERR(s, "Trying to build invalid OpenGL pixel format\n");
        return -1;
    }

    s->glContext =
        [[NSOpenGLContext alloc] initWithFormat:s->pixelFormat
                                   shareContext:nil];

    [s->glContext makeCurrentContext];

    return 0;
}

static void cocoa_set_window_title(struct vo *vo, const char *title)
{
    struct vo_cocoa_state *s = vo->cocoa;
    [s->window setTitle: [NSString stringWithUTF8String:title]];
}

static void update_window(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;

    if (!CGSizeEqualToSize(s->current_video_size, s->previous_video_size)) {
        if (vo->opts->fullscreen) {
            // we will resize as soon as we get out of fullscreen
            s->out_fs_resize = YES;
        } else {
            // only if we are not in fullscreen and the video size did
            // change we resize the window and set a new aspect ratio
            resize_window_from_stored_size(vo);
        }
    }

    [s->view recalcDraggableState];
    cocoa_set_window_title(vo, vo_get_window_title(vo));

    vo_cocoa_fullscreen(vo);
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
    __block int ctxok = 0;
    s->enable_resize_redraw = NO;

    dispatch_sync(dispatch_get_main_queue(), ^{
        s->aspdat = vo->aspdat;
        update_state_sizes(s, d_width, d_height);

        if (flags & VOFLAG_HIDDEN) {
            // This is certainly the first execution of vo_config_window and
            // is called in order for an OpenGL based VO to perform detection
            // of OpenGL extensions. On OSX to accomplish this task we are
            // allowed only create a OpenGL context without attaching it to
            // a drawable.
            ctxok = create_gl_context(vo, gl3profile);
            if (ctxok < 0) return;
        } else if (!s->glContext || !s->window) {
            // Either glContext+Window or Window alone are not created.
            // Handle each of them independently. This is to handle correctly
            // both VOs like vo_corevideo who skip the the OpenGL detection
            // phase completly and generic OpenGL VOs who use VOFLAG_HIDDEN.
            if (!s->glContext) {
                ctxok = create_gl_context(vo, gl3profile);
                if (ctxok < 0) return;
            }

            if (!s->window)
                create_window(vo, d_width, d_height, flags);
        }

        if (s->window) {
            // Everything is properly initialized
            update_window(vo);
        }
    });

    if (ctxok < 0)
        return ctxok;

    [vo->cocoa->glContext makeCurrentContext];
    s->enable_resize_redraw = YES;

    return 0;
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

int vo_cocoa_check_events(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;

    if (s->did_resize) {
        s->did_resize = NO;
        resize_window(vo);
        return VO_EVENT_RESIZE;
    }

    if (s->want_redraw) {
        s->want_redraw = NO;
        vo->want_redraw = true;
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

int vo_cocoa_control(struct vo *vo, int *events, int request, void *arg)
{
    switch (request) {
    case VOCTRL_CHECK_EVENTS:
        *events |= vo_cocoa_check_events(vo);
        return VO_TRUE;
    case VOCTRL_FULLSCREEN:
        vo_cocoa_fullscreen(vo);
        *events |= VO_EVENT_RESIZE;
        return VO_TRUE;
    case VOCTRL_ONTOP:
        vo_cocoa_ontop(vo);
        return VO_TRUE;
    case VOCTRL_UPDATE_SCREENINFO:
        vo_cocoa_update_screen_info(vo);
        return VO_TRUE;
    case VOCTRL_SET_CURSOR_VISIBILITY: {
        bool visible = *(bool *)arg;
        vo_cocoa_set_cursor_visibility(vo, visible);
        return VO_TRUE;
    }
    case VOCTRL_UPDATE_WINDOW_TITLE: {
        cocoa_set_window_title(vo, (const char *) arg);
        return VO_TRUE;
    }
    case VOCTRL_RESTORE_SCREENSAVER:
        enable_power_management(vo);
        return VO_TRUE;
    case VOCTRL_KILL_SCREENSAVER:
        disable_power_management(vo);
        return VO_TRUE;
    }
    return VO_NOTIMPL;
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

- (void)windowDidChangeBackingProperties:(NSNotification *)notification {
    if (self.videoOutput) {
        struct vo_cocoa_state *s = self.videoOutput->cocoa;
        s->did_resize = YES;
    }
}

- (BOOL)isInFullScreenMode
{
    return (([self styleMask] & NSFullScreenWindowMask) ==
                NSFullScreenWindowMask);
}

- (void)toggleMissionControlFullScreen:(BOOL)willBeFullscreen
{
    struct vo_cocoa_state *s = self.videoOutput->cocoa;

    if (willBeFullscreen && ![self isInFullScreenMode]) {
        [self setContentResizeIncrements:NSMakeSize(1, 1)];
        [self toggleFullScreen:nil];
    }

    if (!willBeFullscreen && [self isInFullScreenMode]) {
        [self setContentAspectRatio:s->current_video_size];
        [self toggleFullScreen:nil];
    }

}
- (void)toggleViewFullscreen:(BOOL)willBeFullscreen
{
    struct vo_cocoa_state *s = self.videoOutput->cocoa;

    if (willBeFullscreen && ![s->view isInFullScreenMode]) {
        NSApplicationPresentationOptions popts =
            NSApplicationPresentationDefault;

        if ([s->fs_screen hasMenubar])
            popts |= NSApplicationPresentationAutoHideMenuBar;

        if ([s->fs_screen hasDock])
            popts |= NSApplicationPresentationAutoHideDock;

        NSDictionary *fsopts = @{
            NSFullScreenModeAllScreens  : @NO,
            NSFullScreenModeApplicationPresentationOptions : @(popts)
        };

        [s->view enterFullScreenMode:s->fs_screen withOptions:fsopts];

        // The original "windowed" window will staty around since sending a
        // view fullscreen wraps it in another window. This is noticeable when
        // sending the View fullscreen to another screen. Make it go away
        // manually.
        [s->window orderOut:self];
    }

    if (!willBeFullscreen && [s->view isInFullScreenMode]) {
        [s->view exitFullScreenModeWithOptions:nil];

        // Show the "windowed" window again.
        [s->window makeKeyAndOrderFront:self];
        [self makeFirstResponder:s->view];
    }
}
- (void)fullscreen
{
    struct vo_cocoa_state *s = self.videoOutput->cocoa;
    struct mp_vo_opts *opts  = self.videoOutput->opts;

    vo_cocoa_update_screen_info(self.videoOutput);

    // Go use the fullscreen API selected by the user. View-based or Mission
    // Control.
    if (opts->native_fs) {
        [self toggleMissionControlFullScreen:opts->fullscreen];
    } else {
        [self toggleViewFullscreen:opts->fullscreen];
    }

    // Do common work such as setting mouse visibility and actually setting
    // the new fullscreen state
    if (opts->fullscreen) {
        vo_cocoa_set_cursor_visibility(self.videoOutput, false);
    } else {
        vo_cocoa_set_cursor_visibility(self.videoOutput, true);
    }

    [s->view recalcDraggableState];

    // Change window size if the core attempted to change it while we were in
    // fullscreen. For example config() might have been called as a result of
    // a new file changing the window size.
    if (!opts->fullscreen && s->out_fs_resize) {
        resize_window_from_stored_size(self.videoOutput);
        s->out_fs_resize = NO;
    }

    // Make the core aware of the view size change.
    resize_window(self.videoOutput);
}

- (BOOL)canBecomeMainWindow { return YES; }
- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)windowShouldClose:(id)sender
{
    cocoa_put_key(MP_KEY_CLOSE_WIN);
    // We have to wait for MPlayer to handle this,
    // otherwise we are in trouble if the
    // MP_KEY_CLOSE_WIN handler is disabled
    return NO;
}

- (void)normalSize { [self mulSize:1.0f]; }

- (void)halfSize { [self mulSize:0.5f];}

- (void)doubleSize { [self mulSize:2.0f];}

- (void)mulSize:(float)multiplier
{
    if (!self.videoOutput->opts->fullscreen) {
        NSSize size = {
            .width  = self.videoOutput->cocoa->aspdat.prew * multiplier,
            .height = self.videoOutput->cocoa->aspdat.preh * multiplier
        };
        [self setCenteredContentSize:size];
    }
}

- (int)titleHeight
{
    NSRect of    = [self frame];
    NSRect cb    = [[self contentView] bounds];
    return of.size.height - cb.size.height;
}

- (void)setCenteredContentSize:(NSSize)ns
{
    NSRect f   = [self frame];
    CGFloat dx = (f.size.width  - ns.width) / 2;
    CGFloat dy = (f.size.height - ns.height - [self titleHeight]) / 2;
    NSRect nf  = NSRectFromCGRect(CGRectInset(NSRectToCGRect(f), dx, dy));

    struct vo *vo = self.videoOutput;
    if (!(vo && !vo->opts->border)) {
        NSRect s = [[self screen] visibleFrame];
        if (nf.origin.y + nf.size.height > s.origin.y + s.size.height)
            nf.origin.y = s.size.height - nf.size.height;
    }

    [self setFrame:nf display:NO animate:NO];
}

- (NSRect)constrainFrameRect:(NSRect)rect toScreen:(NSScreen *)screen
{
    return rect;
}

@end

@implementation GLMPlayerOpenGLView
@synthesize tracker = _tracker;
@synthesize videoOutput = _video_output;
@synthesize mouseDown = _mouse_down;
// mpv uses flipped coordinates, because X11 uses those. So let's just use them
// as well without having to do any coordinate conversion of mouse positions.
- (BOOL)isFlipped { return YES; }

- (void)updateTrackingAreas
{
    if (self.tracker) [self removeTrackingArea:self.tracker];

    NSTrackingAreaOptions trackingOptions =
        NSTrackingEnabledDuringMouseDrag |
        NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
        NSTrackingActiveInActiveApp;

    self.tracker =
        [[[NSTrackingArea alloc] initWithRect:[self bounds]
                                      options:trackingOptions
                                        owner:self
                                     userInfo:nil] autorelease];

    [self addTrackingArea:self.tracker];
}

- (NSPoint)mouseLocation
{
    NSPoint wLoc = [self.window mouseLocationOutsideOfEventStream];
    return [self convertPoint:wLoc fromView:nil];
}

- (BOOL)containsMouseLocation
{
    NSRect vF  = [[self.window screen] visibleFrame];
    NSRect vFW = [self.window convertRectFromScreen:vF];
    NSRect vFV = [self convertRect:vFW fromView:nil];

    // clip bounds to current visibleFrame
    NSRect clippedBounds = CGRectIntersection([self bounds], vFV);
    return CGRectContainsPoint(clippedBounds, [self mouseLocation]);
}

- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)becomeFirstResponder { return YES; }
- (BOOL)resignFirstResponder { return YES; }

- (void)recalcDraggableState
{
    struct vo *vo = self.videoOutput;
    BOOL movable  = NO;

    if (!vo->opts->fullscreen) {
        NSPoint loc = [self mouseLocation];
        movable = !mp_input_test_dragging(vo->input_ctx, loc.x, loc.y);
    }

    [self.window setMovableByWindowBackground:movable];
}

- (void)mouseEntered:(NSEvent *)event
{
    // do nothing!
}

- (void)mouseExited:(NSEvent *)event
{
    cocoa_put_key(MP_KEY_MOUSE_LEAVE);
    vo_cocoa_set_cursor_visibility(self.videoOutput, true);
}

- (void)signalMouseMovement:(NSEvent *)event
{
    [self recalcDraggableState];
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    vo_mouse_movement(self.videoOutput, p.x, p.y);
}

- (void)mouseMoved:(NSEvent *)event   { [self signalMouseMovement:event]; }
- (void)mouseDragged:(NSEvent *)event { [self signalMouseMovement:event]; }
- (void)mouseDown:(NSEvent *)evt      { [self mouseDownEvent:evt]; }
- (void)mouseUp:(NSEvent *)evt        { [self mouseUpEvent:evt]; }
- (void)rightMouseDown:(NSEvent *)evt { [self mouseDownEvent:evt]; }
- (void)rightMouseUp:(NSEvent *)evt   { [self mouseUpEvent:evt]; }
- (void)otherMouseDown:(NSEvent *)evt { [self mouseDownEvent:evt]; }
- (void)otherMouseUp:(NSEvent *)evt   { [self mouseUpEvent:evt]; }

- (void)scrollWheel:(NSEvent *)event
{
    struct vo_cocoa_state *s = self.videoOutput->cocoa;

    CGFloat delta;
    // Use the dimention with the most delta as the scrolling one
    if (FFABS([event deltaY]) > FFABS([event deltaX])) {
        delta = [event deltaY];
    } else {
        delta = - [event deltaX];
    }

    if ([event hasPreciseScrollingDeltas]) {
        s->accumulated_scroll += delta;
        static const CGFloat threshold = 10;
        while (s->accumulated_scroll >= threshold) {
            s->accumulated_scroll -= threshold;
            cocoa_put_key_with_modifiers(MP_MOUSE_BTN3, [event modifierFlags]);
        }
        while (s->accumulated_scroll <= -threshold) {
            s->accumulated_scroll += threshold;
            cocoa_put_key_with_modifiers(MP_MOUSE_BTN4, [event modifierFlags]);
        }
    } else {
        if (delta > 0)
            cocoa_put_key_with_modifiers(MP_MOUSE_BTN3, [event modifierFlags]);
        else
            cocoa_put_key_with_modifiers(MP_MOUSE_BTN4, [event modifierFlags]);
    }
}

- (void)mouseDownEvent:(NSEvent *)event
{
    [self putMouseEvent:event withState:MP_KEY_STATE_DOWN];

    if ([event clickCount] > 1)
        [self putMouseEvent:event withState:MP_KEY_STATE_UP];
}

- (void)mouseUpEvent:(NSEvent *)event
{
    [self putMouseEvent:event withState:MP_KEY_STATE_UP];
}

- (void)putMouseEvent:(NSEvent *)event withState:(int)state
{
    self.mouseDown = (state == MP_KEY_STATE_DOWN);
    int mp_key = (MP_MOUSE_BTN0 + [event mpvButtonNumber]);
    cocoa_put_key_with_modifiers(mp_key | state, [event modifierFlags]);
}

- (void)drawRect:(NSRect)rect
{
    struct vo *vo = [self videoOutput];

    if (vo && resize_callback_registered(vo)) {
        if ([self inLiveResize]) {
            NSSize size = to_pixels(vo, [self bounds]).size;
            resize_redraw(vo, size.width, size.height);
        } else {
            // If not in live resize window was probably resized from
            // fullscreen toggle or resize. Make sure we invoke a real repaint
            // ASAP so that the displayed image is correct.
            struct vo_cocoa_state *s = vo->cocoa;
            s->want_redraw = YES;
        }
    } else {
        [[NSColor blackColor] set];
        NSRectFill([self bounds]);
    }
}
@end

@implementation NSScreen (mpvadditions)
- (BOOL)hasDock
{
    NSRect vF = [self visibleFrame];
    NSRect f  = [self frame];
    return
        // The visible frame's width is smaller: dock is on left or right end
        // of this method's receiver.
        vF.size.width < f.size.width ||
        // The visible frame's veritical origin is bigger: dock is
        // on the bottom of this method's receiver.
        vF.origin.y > f.origin.y;

}
- (BOOL)hasMenubar
{
    return [self isEqual: [NSScreen screens][0]];
}
@end

@implementation NSEvent (mpvadditions)
- (int)mpvButtonNumber
{
    int buttonNumber = [self buttonNumber];
    switch (buttonNumber) {
        case 1:  return 2;
        case 2:  return 1;
        default: return buttonNumber;
    }
}
@end
