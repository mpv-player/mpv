/*
 * Cocoa OpenGL Backend
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h> // for CGDisplayHideCursor
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <IOKit/IOKitLib.h>
#include <mach/mach.h>

#import "cocoa_common.h"
#import "video/out/cocoa/window.h"
#import "video/out/cocoa/events_view.h"
#import "video/out/cocoa/video_view.h"
#import "video/out/cocoa/mpvadapter.h"

#include "osdep/threads.h"
#include "osdep/macosx_compat.h"
#include "osdep/macosx_events_objc.h"

#include "config.h"

#include "osdep/timer.h"
#include "osdep/macosx_application.h"
#include "osdep/macosx_application_objc.h"

#include "options/options.h"
#include "video/out/vo.h"
#include "win_state.h"

#include "input/input.h"
#include "talloc.h"

#include "common/msg.h"

#define cocoa_lock(s)    pthread_mutex_lock(&s->mutex)
#define cocoa_unlock(s)  pthread_mutex_unlock(&s->mutex)

static void vo_cocoa_fullscreen(struct vo *vo);
static void cocoa_rm_fs_screen_profile_observer(struct vo_cocoa_state *s);

struct vo_cocoa_state {
    NSWindow *window;
    NSView *view;
    MpvVideoView *video;
    MpvCocoaAdapter *adapter;
    NSOpenGLContext *gl_ctx;

    NSScreen *current_screen;
    NSScreen *fs_screen;
    double screen_fps;

    NSInteger window_level;

    int pending_events;

    bool waiting_frame;
    bool skip_swap_buffer;
    bool embedded; // wether we are embedding in another GUI

    IOPMAssertionID power_mgmt_assertion;
    io_connect_t light_sensor;
    uint64_t last_lmuvalue;
    int last_lux;
    IONotificationPortRef light_sensor_io_port;

    pthread_mutex_t mutex;
    struct mp_log *log;

    uint32_t old_dwidth;
    uint32_t old_dheight;

    NSData *icc_wnd_profile;
    NSData *icc_fs_profile;
    id   fs_icc_changed_ns_observer;

    pthread_mutex_t resize_lock;
    pthread_cond_t resize_wakeup;

    // Protected by the resize_lock
    bool vo_ready;                      // the VO is in a state in which it can
                                        // render frames
    int frame_w, frame_h;               // dimensions of the frame rendered
};

static void with_cocoa_lock(struct vo_cocoa_state *s, void(^block)(void))
{
    cocoa_lock(s);
    block();
    cocoa_unlock(s);
}

static void with_cocoa_lock_on_main_thread(struct vo *vo, void(^block)(void))
{
    struct vo_cocoa_state *s = vo->cocoa;
    dispatch_async(dispatch_get_main_queue(), ^{
        with_cocoa_lock(s, block);
    });
}

static void with_cocoa_lock_on_main_thread_sync(struct vo *vo, void(^block)(void))
{
    struct vo_cocoa_state *s = vo->cocoa;
    dispatch_sync(dispatch_get_main_queue(), ^{
        with_cocoa_lock(s, block);
    });
}

static void queue_new_video_size(struct vo *vo, int w, int h)
{
    struct vo_cocoa_state *s = vo->cocoa;
    if ([s->window conformsToProtocol: @protocol(MpvSizing)]) {
        id<MpvSizing> win = (id<MpvSizing>) s->window;
        [win queueNewVideoSize:NSMakeSize(w, h)];
    }
}

static void enable_power_management(struct vo_cocoa_state *s)
{
    if (!s->power_mgmt_assertion) return;
    IOPMAssertionRelease(s->power_mgmt_assertion);
    s->power_mgmt_assertion = kIOPMNullAssertionID;
}

static void disable_power_management(struct vo_cocoa_state *s)
{
    if (s->power_mgmt_assertion) return;
    IOPMAssertionCreateWithName(
            kIOPMAssertionTypePreventUserIdleDisplaySleep,
            kIOPMAssertionLevelOn,
            CFSTR("io.mpv.video_playing_back"),
            &s->power_mgmt_assertion);
}

static const char macosx_icon[] =
#include "osdep/macosx_icon.inc"
;

static void set_application_icon(NSApplication *app)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSData *icon_data = [NSData dataWithBytesNoCopy:(void *)macosx_icon
                                             length:sizeof(macosx_icon)
                                       freeWhenDone:NO];
    NSImage *icon = [[NSImage alloc] initWithData:icon_data];
    [app setApplicationIconImage:icon];
    [icon release];
    [pool release];
}

static int lmuvalue_to_lux(uint64_t v)
{
    // the polinomial approximation for apple lmu value -> lux was empirically
    // derived by firefox developers (Apple provides no documentation).
    // https://bugzilla.mozilla.org/show_bug.cgi?id=793728
    double power_c4 = 1/pow((double)10,27);
    double power_c3 = 1/pow((double)10,19);
    double power_c2 = 1/pow((double)10,12);
    double power_c1 = 1/pow((double)10,5);

    double term4 = -3.0 * power_c4 * pow(v,4);
    double term3 =  2.6 * power_c3 * pow(v,3);
    double term2 = -3.4 * power_c2 * pow(v,2);
    double term1 =  3.9 * power_c1 * v;

    int lux = ceil(term4 + term3 + term2 + term1 - 0.19);
    return lux > 0 ? lux : 0;
}

static void light_sensor_cb(void *ctx, io_service_t srv, natural_t mtype, void *msg)
{
    struct vo *vo = ctx;
    struct vo_cocoa_state *s = vo->cocoa;
    uint32_t outputs = 2;
    uint64_t values[outputs];

    kern_return_t kr = IOConnectCallMethod(
            s->light_sensor, 0, NULL, 0, NULL, 0, values, &outputs, nil, 0);

    if (kr == KERN_SUCCESS) {
        uint64_t mean = (values[0] + values[1]) / 2;
        if (s->last_lmuvalue != mean) {
            s->last_lmuvalue = mean;
            s->last_lux = lmuvalue_to_lux(s->last_lmuvalue);
            s->pending_events |= VO_EVENT_AMBIENT_LIGHTING_CHANGED;
            vo_wakeup(vo);
            return;
        }
    }
}

static void cocoa_init_light_sensor(struct vo *vo)
{
    with_cocoa_lock_on_main_thread(vo, ^{
        struct vo_cocoa_state *s = vo->cocoa;
        io_service_t srv = IOServiceGetMatchingService(
                kIOMasterPortDefault, IOServiceMatching("AppleLMUController"));
        if (srv == IO_OBJECT_NULL) {
            MP_VERBOSE(vo, "can't find an ambient light sensor\n");
            return;
        }

        // subscribe to notifications from the light sensor driver
        s->light_sensor_io_port = IONotificationPortCreate(kIOMasterPortDefault);
        IONotificationPortSetDispatchQueue(
            s->light_sensor_io_port, dispatch_get_main_queue());

        io_object_t n;
        IOServiceAddInterestNotification(
            s->light_sensor_io_port, srv, kIOGeneralInterest, light_sensor_cb,
            vo, &n);

        kern_return_t kr = IOServiceOpen(srv, mach_task_self(), 0,
                                         &s->light_sensor);
        IOObjectRelease(srv);
        if (kr != KERN_SUCCESS) {
            MP_WARN(vo, "can't start ambient light sensor connection\n");
            return;
        }

        light_sensor_cb(vo, 0, 0, NULL);
    });
}

static void cocoa_uninit_light_sensor(struct vo_cocoa_state *s)
{
    if (s->light_sensor_io_port) {
        IONotificationPortDestroy(s->light_sensor_io_port);
        IOObjectRelease(s->light_sensor);
    }
}

int vo_cocoa_init(struct vo *vo)
{
    struct vo_cocoa_state *s = talloc_zero(NULL, struct vo_cocoa_state);
    *s = (struct vo_cocoa_state){
        .waiting_frame = false,
        .power_mgmt_assertion = kIOPMNullAssertionID,
        .log = mp_log_new(s, vo->log, "cocoa"),
        .embedded = vo->opts->WinID >= 0,
    };
    mpthread_mutex_init_recursive(&s->mutex);
    pthread_mutex_init(&s->resize_lock, NULL);
    pthread_cond_init(&s->resize_wakeup, NULL);
    vo->cocoa = s;
    cocoa_init_light_sensor(vo);
    return 1;
}

static int vo_cocoa_set_cursor_visibility(struct vo *vo, bool *visible)
{
    struct vo_cocoa_state *s = vo->cocoa;

    if (s->embedded)
        return VO_NOTIMPL;

    MpvEventsView *v = (MpvEventsView *) s->view;

    if (*visible) {
        CGDisplayShowCursor(kCGDirectMainDisplay);
    } else if ([v canHideCursor]) {
        CGDisplayHideCursor(kCGDirectMainDisplay);
    } else {
        *visible = true;
    }

    return VO_TRUE;
}

void vo_cocoa_uninit(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;

    pthread_mutex_lock(&s->resize_lock);
    s->vo_ready = false;
    pthread_cond_signal(&s->resize_wakeup);
    pthread_mutex_unlock(&s->resize_lock);

    with_cocoa_lock_on_main_thread_sync(vo, ^{
        enable_power_management(s);
        cocoa_uninit_light_sensor(s);
        cocoa_rm_fs_screen_profile_observer(s);

        [s->gl_ctx release];

        // needed to stop resize events triggered by the event's view -clear
        // causing many uses after free
        [s->video removeFromSuperview];

        [s->view removeFromSuperview];
        [(MpvEventsView *)s->view clear];
        [s->view release];

        // if using --wid + libmpv there's no window to release
        if (s->window)
            [s->window release];

        talloc_free(s);
    });
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

static void vo_cocoa_update_screens_pointers(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    struct mp_vo_opts *opts = vo->opts;
    get_screen_handle(vo, opts->screen_id, s->window, &s->current_screen);
    get_screen_handle(vo, opts->fsscreen_id, s->window, &s->fs_screen);
}

static void vo_cocoa_update_screen_fps(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    NSScreen *screen = vo->opts->fullscreen ? s->fs_screen : s->current_screen;
    NSDictionary* sinfo = [screen deviceDescription];
    NSNumber* sid = [sinfo objectForKey:@"NSScreenNumber"];
    CGDirectDisplayID did = [sid longValue];
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(did);
    s->screen_fps = CGDisplayModeGetRefreshRate(mode);
    CGDisplayModeRelease(mode);

    if (s->screen_fps == 0.0) {
        // Fallback to using Nominal refresh rate from DisplayLink,
        // CVDisplayLinkGet *Actual* OutputVideoRefreshPeriod seems to
        // return 0 as well if CG returns 0
        CVDisplayLinkRef link;
        CVDisplayLinkCreateWithCGDisplay(did, &link);
        const CVTime t = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(link);
        if (!(t.flags & kCVTimeIsIndefinite))
            s->screen_fps = (t.timeScale / (double) t.timeValue);
        CVDisplayLinkRelease(link);
    }

    s->pending_events |= VO_EVENT_WIN_STATE;
}

static void vo_cocoa_update_screen_info(struct vo *vo, struct mp_rect *out_rc)
{
    struct vo_cocoa_state *s = vo->cocoa;

    if (s->embedded)
        return;

    vo_cocoa_update_screens_pointers(vo);
    vo_cocoa_update_screen_fps(vo);

    if (out_rc) {
        NSRect r = [s->current_screen frame];
        *out_rc = (struct mp_rect){0, 0, r.size.width, r.size.height};
    }
}

static void resize_window(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    NSRect frame = [s->video frameInPixels];
    vo->dwidth  = frame.size.width;
    vo->dheight = frame.size.height;
    [s->gl_ctx update];
}

static void vo_set_level(struct vo *vo, int ontop)
{
    struct vo_cocoa_state *s = vo->cocoa;

    if (ontop) {
        // +1 is not enough as that will show the icon layer on top of the
        // menubar when the application is not frontmost. so use +2
        s->window_level = NSMainMenuWindowLevel + 2;
    } else {
        s->window_level = NSNormalWindowLevel;
    }

    [[s->view window] setLevel:s->window_level];
    [s->window        setLevel:s->window_level];
}

static int vo_cocoa_ontop(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    if (s->embedded)
        return VO_NOTIMPL;

    struct mp_vo_opts *opts = vo->opts;
    opts->ontop = !opts->ontop;
    vo_set_level(vo, opts->ontop);
    return VO_TRUE;
}

static MpvVideoWindow *create_window(NSRect rect, NSScreen *s, bool border,
                                     MpvCocoaAdapter *adapter)
{
    int window_mask = 0;
    if (border) {
        window_mask = NSTitledWindowMask|NSClosableWindowMask|
                      NSMiniaturizableWindowMask|NSResizableWindowMask;
    } else {
        window_mask = NSBorderlessWindowMask|NSResizableWindowMask;
    }

    MpvVideoWindow *w =
        [[MpvVideoWindow alloc] initWithContentRect:rect
                                          styleMask:window_mask
                                            backing:NSBackingStoreBuffered
                                              defer:NO
                                             screen:s];
    w.adapter = adapter;
    [w setDelegate: w];

    return w;
}

static void create_ui(struct vo *vo, struct mp_rect *win, int geo_flags)
{
    struct vo_cocoa_state *s = vo->cocoa;
    struct mp_vo_opts *opts  = vo->opts;

    MpvCocoaAdapter *adapter = [[MpvCocoaAdapter alloc] init];
    adapter.vout = vo;

    NSView *parent;
    if (s->embedded) {
        parent = (NSView *) (intptr_t) opts->WinID;
    } else {
        const NSRect wr =
            NSMakeRect(win->x0, win->y0, win->x1 - win->x0, win->y1 - win->y0);
        s->window = create_window(wr, s->current_screen, opts->border, adapter);
        parent = [s->window contentView];
    }

    MpvEventsView *view = [[MpvEventsView alloc] initWithFrame:[parent bounds]];
    view.adapter = adapter;
    s->view = view;
    [parent addSubview:s->view];
    // update the cursor position now that the view has been added.
    [view signalMousePosition];
    s->adapter = adapter;

    cocoa_register_menu_item_action(MPM_H_SIZE,   @selector(halfSize));
    cocoa_register_menu_item_action(MPM_N_SIZE,   @selector(normalSize));
    cocoa_register_menu_item_action(MPM_D_SIZE,   @selector(doubleSize));
    cocoa_register_menu_item_action(MPM_MINIMIZE, @selector(performMiniaturize:));
    cocoa_register_menu_item_action(MPM_ZOOM,     @selector(performZoom:));

    s->video = [[MpvVideoView alloc] initWithFrame:[s->view bounds]];
    [s->video setWantsBestResolutionOpenGLSurface:YES];

    [s->view addSubview:s->video];
    [s->gl_ctx setView:s->video];
    [s->video release];

    s->video.adapter = adapter;
    [adapter release];

    if (!s->embedded) {
        [s->window setRestorable:NO];
        [s->window makeMainWindow];
        [s->window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
    }
}

static int cocoa_set_window_title(struct vo *vo, const char *title)
{
    struct vo_cocoa_state *s = vo->cocoa;
    if (s->embedded)
        return VO_NOTIMPL;

    void *talloc_ctx   = talloc_new(NULL);
    struct bstr btitle = bstr_sanitize_utf8_latin1(talloc_ctx, bstr0(title));
    NSString *nstitle  = [NSString stringWithUTF8String:btitle.start];
    if (nstitle) {
        [s->window setTitle: nstitle];
        [s->window displayIfNeeded];
    }
    talloc_free(talloc_ctx);
    return VO_TRUE;
}

static void cocoa_rm_fs_screen_profile_observer(struct vo_cocoa_state *s)
{
    [[NSNotificationCenter defaultCenter]
        removeObserver:s->fs_icc_changed_ns_observer];
}

static void cocoa_add_fs_screen_profile_observer(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;

    if (s->fs_icc_changed_ns_observer)
        cocoa_rm_fs_screen_profile_observer(s);

    if (vo->opts->fsscreen_id < 0)
        return;

    void (^nblock)(NSNotification *n) = ^(NSNotification *n) {
        s->pending_events |= VO_EVENT_ICC_PROFILE_CHANGED;
    };

    s->fs_icc_changed_ns_observer = [[NSNotificationCenter defaultCenter]
        addObserverForName:NSScreenColorSpaceDidChangeNotification
                    object:s->fs_screen
                     queue:nil
                usingBlock:nblock];
}

void vo_cocoa_create_nsgl_ctx(struct vo *vo, void *ctx)
{
    struct vo_cocoa_state *s = vo->cocoa;
    s->gl_ctx = [[NSOpenGLContext alloc] initWithCGLContextObj:ctx];
    [s->gl_ctx makeCurrentContext];
}

void vo_cocoa_release_nsgl_ctx(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    [s->gl_ctx release];
    s->gl_ctx = nil;
}

int vo_cocoa_config_window(struct vo *vo, uint32_t flags)
{
    struct vo_cocoa_state *s = vo->cocoa;
    with_cocoa_lock_on_main_thread(vo, ^{
        struct mp_rect screenrc;
        vo_cocoa_update_screen_info(vo, &screenrc);

        struct vo_win_geometry geo;
        vo_calc_window_geometry(vo, &screenrc, &geo);
        vo_apply_window_geometry(vo, &geo);

        uint32_t width = vo->dwidth;
        uint32_t height = vo->dheight;

        bool reset_size = s->old_dwidth != width || s->old_dheight != height;
        s->old_dwidth  = width;
        s->old_dheight = height;

        if (!(flags & VOFLAG_HIDDEN) && !s->view) {
            create_ui(vo, &geo.win, geo.flags);
        }

        if (!s->embedded && s->window) {
            if (reset_size)
                queue_new_video_size(vo, width, height);
            vo_cocoa_fullscreen(vo);
            cocoa_add_fs_screen_profile_observer(vo);
            cocoa_set_window_title(vo, vo_get_window_title(vo));
            vo_set_level(vo, vo->opts->ontop);
        }

        pthread_mutex_lock(&s->resize_lock);
        s->vo_ready = true;
        pthread_mutex_unlock(&s->resize_lock);

        // trigger a resize -> don't set vo->dwidth and vo->dheight directly
        // since this block is executed asynchronously to the video
        // reconfiguration code.
        s->pending_events |= VO_EVENT_RESIZE;
    });

    if (!s->embedded) {
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        set_application_icon(NSApp);
    }
    return 0;
}

void vo_cocoa_set_current_context(struct vo *vo, bool current)
{
    struct vo_cocoa_state *s = vo->cocoa;

    if (current) {
        cocoa_lock(s);
        if (s->gl_ctx) [s->gl_ctx makeCurrentContext];
    } else {
        [NSOpenGLContext clearCurrentContext];
        cocoa_unlock(s);
    }
}

static void vo_cocoa_resize_redraw(struct vo *vo, int width, int height)
{
    struct vo_cocoa_state *s = vo->cocoa;

    struct timespec e = mp_time_us_to_timespec(mp_add_timeout(mp_time_us(), 0.1));

    pthread_mutex_lock(&s->resize_lock);

    // Make sure at least one frame will be drawn
    s->frame_w = s->frame_h = 0;

    s->pending_events |= VO_EVENT_RESIZE | VO_EVENT_EXPOSE;
    vo_wakeup(vo);

    while (s->frame_w != width && s->frame_h != height && s->vo_ready) {
        if (pthread_cond_timedwait(&s->resize_wakeup, &s->resize_lock, &e))
            break;
    }

    pthread_mutex_unlock(&s->resize_lock);
}

static void draw_changes_after_next_frame(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    if (!s->waiting_frame) {
        s->waiting_frame = true;
        NSDisableScreenUpdates();
    }
}

void vo_cocoa_swap_buffers(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;

    // Don't swap a frame with wrong size
    if (s->pending_events & VO_EVENT_RESIZE)
        return;

    [s->gl_ctx flushBuffer];

    pthread_mutex_lock(&s->resize_lock);
    s->frame_w = vo->dwidth;
    s->frame_h = vo->dheight;
    pthread_cond_signal(&s->resize_wakeup);
    pthread_mutex_unlock(&s->resize_lock);

    if (s->waiting_frame) {
        s->waiting_frame = false;
        NSEnableScreenUpdates();
    }
}

int vo_cocoa_check_events(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    int events = s->pending_events;
    s->pending_events = 0;

    if (events & VO_EVENT_RESIZE) {
        resize_window(vo);
    }

    return events;
}

static int vo_cocoa_fullscreen_sync(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;

    if (s->embedded)
        return VO_NOTIMPL;

    with_cocoa_lock_on_main_thread(vo, ^{
        vo_cocoa_fullscreen(vo);
    });

    return VO_TRUE;
}

static void vo_cocoa_fullscreen(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    struct mp_vo_opts *opts  = vo->opts;

    if (s->embedded)
        return;

    vo_cocoa_update_screen_info(vo, NULL);

    draw_changes_after_next_frame(vo);
    [(MpvEventsView *)s->view setFullScreen:opts->fullscreen];

    if ([s->view window] != s->window) {
        // cocoa implements fullscreen views by moving the view to a fullscreen
        // window. Set that window delegate to the cocoa adapter to trigger
        // calls to -windowDidResignKey: and -windowDidBecomeKey:
        [[s->view window] setDelegate:s->adapter];
    }

    s->pending_events |= VO_EVENT_ICC_PROFILE_CHANGED;
    s->pending_events |= VO_EVENT_RESIZE;
}

static void vo_cocoa_control_get_icc_profile(struct vo *vo, void *arg)
{
    struct vo_cocoa_state *s = vo->cocoa;
    bstr *p = arg;

    vo_cocoa_update_screen_info(vo, NULL);

    NSScreen *screen = vo->opts->fullscreen ? s->fs_screen : s->current_screen;
    NSData *profile = [[screen colorSpace] ICCProfileData];

    p->start = talloc_memdup(NULL, (void *)[profile bytes], [profile length]);
    p->len   = [profile length];
}

int vo_cocoa_control(struct vo *vo, int *events, int request, void *arg)
{
    struct mp_vo_opts *opts  = vo->opts;

    switch (request) {
    case VOCTRL_CHECK_EVENTS:
        *events |= vo_cocoa_check_events(vo);
        return VO_TRUE;
    case VOCTRL_FULLSCREEN:
        opts->fullscreen = !opts->fullscreen;
        return vo_cocoa_fullscreen_sync(vo);
    case VOCTRL_ONTOP:
        return vo_cocoa_ontop(vo);
    case VOCTRL_GET_UNFS_WINDOW_SIZE: {
        int *s = arg;
        with_cocoa_lock(vo->cocoa, ^{
            NSSize size = [vo->cocoa->view frame].size;
            s[0] = size.width;
            s[1] = size.height;
        });
        return VO_TRUE;
    }
    case VOCTRL_SET_UNFS_WINDOW_SIZE: {
        int *s = arg;
        int w, h;
        w = s[0];
        h = s[1];
        with_cocoa_lock_on_main_thread(vo, ^{
            queue_new_video_size(vo, w, h);
        });
        return VO_TRUE;
    }
    case VOCTRL_GET_WIN_STATE: {
        with_cocoa_lock(vo->cocoa, ^{
            const bool minimized = [[vo->cocoa->view window] isMiniaturized];
            *(int *)arg = minimized ? VO_WIN_STATE_MINIMIZED : 0;
        });
        return VO_TRUE;
    }
    case VOCTRL_SET_CURSOR_VISIBILITY:
        return vo_cocoa_set_cursor_visibility(vo, arg);
    case VOCTRL_UPDATE_WINDOW_TITLE:
        return cocoa_set_window_title(vo, (const char *) arg);
    case VOCTRL_RESTORE_SCREENSAVER:
        enable_power_management(vo->cocoa);
        return VO_TRUE;
    case VOCTRL_KILL_SCREENSAVER:
        disable_power_management(vo->cocoa);
        return VO_TRUE;
    case VOCTRL_GET_ICC_PROFILE:
        vo_cocoa_control_get_icc_profile(vo, arg);
        return VO_TRUE;
    case VOCTRL_GET_DISPLAY_FPS:
        if (vo->cocoa->screen_fps > 0.0) {
            *(double *)arg = vo->cocoa->screen_fps;
            return VO_TRUE;
        }
        break;
    case VOCTRL_GET_AMBIENT_LUX:
        if (vo->cocoa->light_sensor != IO_OBJECT_NULL) {
            *(int *)arg = vo->cocoa->last_lux;
            return VO_TRUE;
        }
        break;
    }
    return VO_NOTIMPL;
}

@implementation MpvCocoaAdapter
@synthesize vout = _video_output;

- (void)performAsyncResize:(NSSize)size {
    struct vo_cocoa_state *s = self.vout->cocoa;
    if (!s->waiting_frame)
        vo_cocoa_resize_redraw(self.vout, size.width, size.height);
}

- (BOOL)keyboardEnabled {
    return !!mp_input_vo_keyboard_enabled(self.vout->input_ctx);
}

- (BOOL)mouseEnabled {
    return !!mp_input_mouse_enabled(self.vout->input_ctx);
}

- (void)setNeedsResize {
    struct vo_cocoa_state *s = self.vout->cocoa;
    s->pending_events |= VO_EVENT_RESIZE;
    vo_wakeup(self.vout);
}

- (void)recalcMovableByWindowBackground:(NSPoint)p
{
    BOOL movable = NO;
    if (![self isInFullScreenMode]) {
        movable = !mp_input_test_dragging(self.vout->input_ctx, p.x, p.y);
    }

    [self.vout->cocoa->window setMovableByWindowBackground:movable];
}

- (void)signalMouseMovement:(NSPoint)point {
    mp_input_set_mouse_pos(self.vout->input_ctx, point.x, point.y);
    [self recalcMovableByWindowBackground:point];
}

- (void)putKeyEvent:(NSEvent*)event
{
    cocoa_put_key_event(event);
}

- (void)putKey:(int)mpkey withModifiers:(int)modifiers
{
    cocoa_put_key_with_modifiers(mpkey, modifiers);
}

- (void)putAxis:(int)mpkey delta:(float)delta;
{
    mp_input_put_axis(self.vout->input_ctx, mpkey, delta);
}

- (void)putCommand:(char*)cmd
{
    char *cmd_ = ta_strdup(NULL, cmd);
    mp_cmd_t *cmdt = mp_input_parse_cmd(self.vout->input_ctx, bstr0(cmd_), "");
    mp_input_queue_cmd(self.vout->input_ctx, cmdt);
    ta_free(cmd_);
}

- (BOOL)isInFullScreenMode {
    return self.vout->opts->fullscreen;
}

- (NSScreen *)fsScreen {
    struct vo_cocoa_state *s = self.vout->cocoa;
    return s->fs_screen;
}

- (BOOL)fsModeAllScreens
{
    return self.vout->opts->fs_black_out_screens;
}

- (void)handleFilesArray:(NSArray *)files
{
    [[EventsResponder sharedInstance] handleFilesArray:files];
}

- (void)didChangeWindowedScreenProfile:(NSScreen *)screen
{
    struct vo_cocoa_state *s = self.vout->cocoa;
    s->pending_events |= VO_EVENT_ICC_PROFILE_CHANGED;
}

- (void)didChangeMousePosition
{
    struct vo_cocoa_state *s = self.vout->cocoa;
    [(MpvEventsView *)s->view signalMousePosition];
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    [self didChangeMousePosition];
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    [self didChangeMousePosition];
}

- (void)windowDidMiniaturize:(NSNotification *)notification
{
    struct vo_cocoa_state *s = self.vout->cocoa;
    s->pending_events |= VO_EVENT_WIN_STATE;
}

- (void)windowDidDeminiaturize:(NSNotification *)notification
{
    struct vo_cocoa_state *s = self.vout->cocoa;
    s->pending_events |= VO_EVENT_WIN_STATE;
}

@end
