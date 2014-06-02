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
#include <dlfcn.h>

#include "cocoa_common.h"
#include "video/out/cocoa/window.h"
#include "video/out/cocoa/view.h"
#import "video/out/cocoa/mpvadapter.h"

#include "osdep/macosx_compat.h"
#include "osdep/macosx_application.h"
#include "osdep/macosx_application_objc.h"
#include "osdep/macosx_events.h"

#include "config.h"

#include "options/options.h"
#include "video/out/vo.h"
#include "win_state.h"

#include "input/input.h"
#include "talloc.h"

#include "common/msg.h"

#define CF_RELEASE(a) if ((a) != NULL) CFRelease(a)

static void vo_cocoa_fullscreen(struct vo *vo);
static void vo_cocoa_ontop(struct vo *vo);
static void cocoa_change_profile(struct vo *vo, char **store, NSScreen *screen);
static void cocoa_rm_fs_screen_profile_observer(struct vo *vo);

struct vo_cocoa_state {
    MpvVideoWindow *window;
    MpvVideoView *view;
    NSOpenGLContext *gl_ctx;

    NSScreen *current_screen;
    NSScreen *fs_screen;

    NSInteger window_level;

    bool did_resize;
    bool skip_next_swap_buffer;
    bool inside_sync_section;

    IOPMAssertionID power_mgmt_assertion;

    NSLock *lock;
    bool enable_resize_redraw;
    void *ctx;
    void (*gl_clear)(void *ctx);
    void (*resize_redraw)(struct vo *vo, int w, int h);

    struct mp_log *log;

    uint32_t old_dwidth;
    uint32_t old_dheight;

    bool icc_profile_path_changed;
    char *icc_wnd_profile_path;
    char *icc_fs_profile_path;
    id   fs_icc_changed_ns_observer;
};

static void dispatch_on_main_thread(struct vo *vo, void(^block)(void))
{
    struct vo_cocoa_state *s = vo->cocoa;
    if (!s->inside_sync_section) {
        dispatch_sync(dispatch_get_main_queue(), ^{
            s->inside_sync_section = true;
            block();
            s->inside_sync_section = false;
        });
    } else {
        block();
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
    IOPMAssertionCreateWithName(
            kIOPMAssertionTypePreventUserIdleDisplaySleep,
            kIOPMAssertionLevelOn,
            CFSTR("io.mpv.video_playing_back"),
            &s->power_mgmt_assertion);
}

int vo_cocoa_init(struct vo *vo)
{
    struct vo_cocoa_state *s = talloc_zero(vo, struct vo_cocoa_state);
    *s = (struct vo_cocoa_state){
        .did_resize = false,
        .skip_next_swap_buffer = false,
        .inside_sync_section = false,
        .power_mgmt_assertion = kIOPMNullAssertionID,
        .lock = [[NSLock alloc] init],
        .enable_resize_redraw = NO,
        .log = mp_log_new(s, vo->log, "cocoa"),
        .icc_profile_path_changed = false,
    };
    vo->cocoa = s;
    return 1;
}

static void vo_cocoa_set_cursor_visibility(struct vo *vo, bool *visible)
{
    struct vo_cocoa_state *s = vo->cocoa;

    if (*visible) {
        CGDisplayShowCursor(kCGDirectMainDisplay);
    } else if ([s->view canHideCursor]) {
        CGDisplayHideCursor(kCGDirectMainDisplay);
    } else {
        *visible = true;
    }
}

void vo_cocoa_uninit(struct vo *vo)
{
    dispatch_sync(dispatch_get_main_queue(), ^{
        struct vo_cocoa_state *s = vo->cocoa;
        enable_power_management(vo);
        cocoa_rm_fs_screen_profile_observer(vo);
        [NSApp setPresentationOptions:NSApplicationPresentationDefault];

        // XXX: It looks like there are some circular retain cycles for the
        // video view / video window that cause them to not be deallocated,
        // This is a workaround to make the fullscreen window be released,
        // but the retain cycle problems should be investigated.
        if ([s->view isInFullScreenMode])
            [[s->view window] release];

        [s->window release];
        s->window = nil;

        [s->gl_ctx release];
        s->gl_ctx = nil;

        [s->lock release];
        s->lock = nil;
    });
}

void vo_cocoa_register_resize_callback(struct vo *vo,
                                       void (*cb)(struct vo *vo, int w, int h))
{
    struct vo_cocoa_state *s = vo->cocoa;
    s->resize_redraw = cb;
}

void vo_cocoa_register_gl_clear_callback(struct vo *vo, void *ctx,
                                         void (*cb)(void *ctx))
{
    struct vo_cocoa_state *s = vo->cocoa;
    s->ctx = ctx;
    s->gl_clear = cb;
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

static void vo_cocoa_update_screen_info(struct vo *vo, struct mp_rect *out_rc)
{
    struct vo_cocoa_state *s = vo->cocoa;

    vo_cocoa_update_screens_pointers(vo);

    if (out_rc) {
        NSRect r = [s->current_screen frame];
        *out_rc = (struct mp_rect){0, 0, r.size.width, r.size.height};
    }
}

static void resize_window(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    NSRect frame = [s->view frameInPixels];
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

static void vo_cocoa_ontop(struct vo *vo)
{
    struct mp_vo_opts *opts = vo->opts;
    opts->ontop = !opts->ontop;
    vo_set_level(vo, opts->ontop);
}

static void create_window(struct vo *vo, struct mp_rect *win, int geo_flags)
{
    struct vo_cocoa_state *s = vo->cocoa;
    struct mp_vo_opts *opts  = vo->opts;

    const NSRect contentRect =
        NSMakeRect(win->x0, win->y0, win->x1 - win->x0, win->y1 - win->y0);

    int window_mask = 0;
    if (opts->border) {
        window_mask = NSTitledWindowMask|NSClosableWindowMask|
                      NSMiniaturizableWindowMask|NSResizableWindowMask;
    } else {
        window_mask = NSBorderlessWindowMask|NSResizableWindowMask;
    }

    s->window =
        [[MpvVideoWindow alloc] initWithContentRect:contentRect
                                          styleMask:window_mask
                                            backing:NSBackingStoreBuffered
                                              defer:NO
                                             screen:s->current_screen];
    s->view = [[[MpvVideoView alloc] initWithFrame:contentRect] autorelease];

    [s->view setWantsBestResolutionOpenGLSurface:YES];

    cocoa_register_menu_item_action(MPM_H_SIZE,   @selector(halfSize));
    cocoa_register_menu_item_action(MPM_N_SIZE,   @selector(normalSize));
    cocoa_register_menu_item_action(MPM_D_SIZE,   @selector(doubleSize));
    cocoa_register_menu_item_action(MPM_MINIMIZE, @selector(performMiniaturize:));
    cocoa_register_menu_item_action(MPM_ZOOM,     @selector(performZoom:));

    [s->window setRestorable:NO];
    [s->window setContentView:s->view];
    [s->gl_ctx setView:s->view];

    MpvCocoaAdapter *adapter = [[[MpvCocoaAdapter alloc] init] autorelease];
    adapter.vout = vo;
    s->view.adapter = adapter;
    s->window.adapter = adapter;

    [s->window setDelegate:s->window];
    [s->window makeMainWindow];

    [s->window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    vo_set_level(vo, opts->ontop);

    if (opts->fs_missioncontrol) {
        [s->window setCollectionBehavior:
            NSWindowCollectionBehaviorFullScreenPrimary];
    }
}

static CGLOpenGLProfile cgl_profile(int gl3profile) {
    if (gl3profile) {
        return kCGLOGLPVersion_3_2_Core;
    } else {
        return kCGLOGLPVersion_Legacy;
    }
}

static int create_gl_context(struct vo *vo, int gl3profile)
{
    struct vo_cocoa_state *s = vo->cocoa;
    CGLError err;

    CGLPixelFormatAttribute attrs[] = {
        kCGLPFAOpenGLProfile,
        (CGLPixelFormatAttribute) cgl_profile(gl3profile),
        kCGLPFADoubleBuffer,
        kCGLPFAAccelerated,
        0
    };

    CGLPixelFormatObj pix;
    GLint npix;
    if ((err = CGLChoosePixelFormat(attrs, &pix, &npix)) != kCGLNoError) {
        MP_FATAL(s, "error creating CGL pixel format: %s (%d)\n",
                 CGLErrorString(err), err);
    }

    CGLContextObj ctx;
    if ((err = CGLCreateContext(pix, 0, &ctx)) != kCGLNoError) {
        MP_FATAL(s, "error creating CGL context: %s (%d)\n",
                 CGLErrorString(err), err);
        return -1;
    }

    s->gl_ctx = [[NSOpenGLContext alloc] initWithCGLContextObj:ctx];
    [s->gl_ctx makeCurrentContext];

    CGLReleasePixelFormat(pix);
    CGLReleaseContext(ctx);

    return 0;
}

static void cocoa_set_window_title(struct vo *vo, const char *title)
{
    struct vo_cocoa_state *s = vo->cocoa;
    void *talloc_ctx   = talloc_new(NULL);
    struct bstr btitle = bstr_sanitize_utf8_latin1(talloc_ctx, bstr0(title));
    NSString *nstitle  = [NSString stringWithUTF8String:btitle.start];
    if (nstitle)
        [s->window setTitle: nstitle];
    talloc_free(talloc_ctx);
}

static void vo_cocoa_resize_redraw(struct vo *vo, int width, int height)
{
    struct vo_cocoa_state *s = vo->cocoa;

    if (!s->resize_redraw)
        return;

    vo_cocoa_set_current_context(vo, true);

    [s->gl_ctx update];

    if (s->enable_resize_redraw) {
        s->resize_redraw(vo, width, height);
        s->skip_next_swap_buffer = true;
    } else {
        s->gl_clear(s->ctx);
    }

    [s->gl_ctx flushBuffer];
    vo_cocoa_set_current_context(vo, false);
}

static void cocoa_rm_fs_screen_profile_observer(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    [[NSNotificationCenter defaultCenter]
        removeObserver:s->fs_icc_changed_ns_observer];
}

static void cocoa_add_fs_screen_profile_observer(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;

    if (s->fs_icc_changed_ns_observer)
        cocoa_rm_fs_screen_profile_observer(vo);

    if (vo->opts->fsscreen_id < 0)
        return;

    void (^nblock)(NSNotification *n) = ^(NSNotification *n) {
        cocoa_change_profile(vo, &s->icc_fs_profile_path, s->fs_screen);
        s->icc_profile_path_changed = true;
    };

    s->fs_icc_changed_ns_observer = [[NSNotificationCenter defaultCenter]
        addObserverForName:NSScreenColorSpaceDidChangeNotification
                    object:s->fs_screen
                     queue:nil
                usingBlock:nblock];
}

int vo_cocoa_config_window(struct vo *vo, uint32_t flags, int gl3profile)
{
    struct vo_cocoa_state *s = vo->cocoa;
    __block int ctxok = 0;

    dispatch_on_main_thread(vo, ^{
        s->enable_resize_redraw = false;

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

        if (flags & VOFLAG_HIDDEN) {
            // This is certainly the first execution of vo_config_window and
            // is called in order for an OpenGL based VO to perform detection
            // of OpenGL extensions. On OSX to accomplish this task we are
            // allowed only create a OpenGL context without attaching it to
            // a drawable.
            ctxok = create_gl_context(vo, gl3profile);
            if (ctxok < 0) return;
        } else if (!s->gl_ctx || !s->window) {
            // Either gl_ctx+window or window alone is not created.
            // Handle each of them independently. This is to handle correctly
            // both VOs like vo_corevideo who skip the the OpenGL detection
            // phase completly and generic OpenGL VOs who use VOFLAG_HIDDEN.
            if (!s->gl_ctx) {
                ctxok = create_gl_context(vo, gl3profile);
                if (ctxok < 0) return;
            }

            if (!s->window)
                create_window(vo, &geo.win, geo.flags);
        }

        if (s->window) {
            // Everything is properly initialized
            if (reset_size)
                [s->window queueNewVideoSize:NSMakeSize(width, height)];
            cocoa_set_window_title(vo, vo_get_window_title(vo));
            vo_cocoa_fullscreen(vo);
            cocoa_add_fs_screen_profile_observer(vo);
        }

        s->enable_resize_redraw = true;
    });

    if (ctxok < 0)
        return ctxok;

    [vo->cocoa->gl_ctx makeCurrentContext];

    return 0;
}

void vo_cocoa_set_current_context(struct vo *vo, bool current)
{
    struct vo_cocoa_state *s = vo->cocoa;

    if (current) {
        if (!s->inside_sync_section)
            [s->lock lock];

        [s->gl_ctx makeCurrentContext];
    } else {
        [NSOpenGLContext clearCurrentContext];

        if (!s->inside_sync_section)
            [s->lock unlock];
    }
}

void vo_cocoa_swap_buffers(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    if (s->skip_next_swap_buffer) {
        // When in live resize the GL view asynchronously updates itself from
        // it's drawRect: implementation and calls flushBuffer. This means the
        // backbuffer is probably in an inconsistent state, so we skip one
        // flushBuffer call here on the playloop thread.
        s->skip_next_swap_buffer = false;
    } else {
        [s->gl_ctx flushBuffer];
    }
}

int vo_cocoa_check_events(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;

    if (s->did_resize) {
        s->did_resize = false;
        resize_window(vo);
        return VO_EVENT_RESIZE;
    }

    if (s->icc_profile_path_changed) {
        s->icc_profile_path_changed = false;
        return VO_EVENT_ICC_PROFILE_PATH_CHANGED;
    }

    return 0;
}

static void vo_cocoa_fullscreen_sync(struct vo *vo)
{
    dispatch_on_main_thread(vo, ^{
        vo->cocoa->enable_resize_redraw = false;
        vo_cocoa_fullscreen(vo);
        vo->cocoa->enable_resize_redraw = true;
    });
}

static void vo_cocoa_fullscreen(struct vo *vo)
{
    struct vo_cocoa_state *s = vo->cocoa;
    struct mp_vo_opts *opts  = vo->opts;

    vo_cocoa_update_screen_info(vo, NULL);

    if (opts->fs_missioncontrol) {
        [s->window setFullScreen:opts->fullscreen];
    } else {
        [s->view setFullScreen:opts->fullscreen];
    }

    if (s->icc_fs_profile_path != s->icc_wnd_profile_path)
        s->icc_profile_path_changed = true;

    // Make the core aware of the view size change.
    resize_window(vo);
}

static char *cocoa_get_icc_profile_path(struct vo *vo, NSScreen *screen)
{
    assert(screen);

    struct vo_cocoa_state *s = vo->cocoa;
    char *result = NULL;
    CFDictionaryRef device_info = NULL;

    CGDirectDisplayID displayID = (CGDirectDisplayID)
        [[screen deviceDescription][@"NSScreenNumber"] unsignedLongValue];

    CFUUIDRef uuid = CGDisplayCreateUUIDFromDisplayID(displayID);
    if (CFGetTypeID(uuid) == CFNullGetTypeID()) {
        MP_ERR(s, "cannot get display UUID.\n");
        goto get_icc_profile_path_err_out;
    }

    device_info =
        ColorSyncDeviceCopyDeviceInfo(kColorSyncDisplayDeviceClass, uuid);

    CFRelease(uuid);

    if (!device_info) {
        MP_ERR(s, "cannot get display info.\n");
        goto get_icc_profile_path_err_out;
    }

    CFDictionaryRef factory_info =
        CFDictionaryGetValue(device_info, kColorSyncFactoryProfiles);
    if (!factory_info) {
        MP_ERR(s, "cannot get display factory settings.\n");
        goto get_icc_profile_path_err_out;
    }

    CFStringRef default_profile_id =
        CFDictionaryGetValue(factory_info, kColorSyncDeviceDefaultProfileID);
    if (!default_profile_id) {
        MP_ERR(s, "cannot get display default profile ID.\n");
        goto get_icc_profile_path_err_out;
    }

    CFURLRef icc_url;
    CFDictionaryRef custom_profile_info =
        CFDictionaryGetValue(device_info, kColorSyncCustomProfiles);
    if (custom_profile_info) {
        icc_url = CFDictionaryGetValue(custom_profile_info, default_profile_id);
        // If icc_url is NULL, the ICC profile URL could not be retrieved
        // although a custom profile was specified. This points to a
        // configuration error, so we should not fall back to the factory
        // profile, but return an error instead.
        if (!icc_url) {
            MP_ERR(s, "cannot get display profile URL\n");
            goto get_icc_profile_path_err_out;
        }
    } else {
        // No custom profile specified; try factory profile for the device
        CFDictionaryRef factory_profile_info =
            CFDictionaryGetValue(factory_info, default_profile_id);
        if (!factory_profile_info) {
            MP_ERR(s, "cannot get display profile info\n");
            goto get_icc_profile_path_err_out;
        }

        icc_url = CFDictionaryGetValue(factory_profile_info,
                                       kColorSyncDeviceProfileURL);
        if (!icc_url) {
            MP_ERR(s, "cannot get display factory profile URL.\n");
            goto get_icc_profile_path_err_out;
        }
    }

   result = talloc_strdup(vo, (char *)[[(NSURL *)icc_url path] UTF8String]);
   if (!result)
       MP_ERR(s, "cannot get display profile path.\n");

get_icc_profile_path_err_out:
    CF_RELEASE(device_info);
    return result;
}

static void cocoa_change_profile(struct vo *vo, char **store, NSScreen *screen)
{
    if (*store)
        talloc_free(*store);
    *store = cocoa_get_icc_profile_path(vo, screen);
}

static void vo_cocoa_control_get_icc_profile_path(struct vo *vo, void *arg)
{
    struct vo_cocoa_state *s = vo->cocoa;
    char **p = arg;

    vo_cocoa_update_screen_info(vo, NULL);

    NSScreen *screen;
    char **path;

    if (vo->opts->fullscreen) {
        screen = s->fs_screen;
        path   = &s->icc_fs_profile_path;
    } else {
        screen = s->current_screen;
        path   = &s->icc_wnd_profile_path;
    }

    cocoa_change_profile(vo, path, screen);
    *p = *path;
}

int vo_cocoa_control(struct vo *vo, int *events, int request, void *arg)
{
    switch (request) {
    case VOCTRL_CHECK_EVENTS:
        *events |= vo_cocoa_check_events(vo);
        return VO_TRUE;
    case VOCTRL_FULLSCREEN:
        vo_cocoa_fullscreen_sync(vo);
        *events |= VO_EVENT_RESIZE;
        return VO_TRUE;
    case VOCTRL_ONTOP:
        vo_cocoa_ontop(vo);
        return VO_TRUE;
    case VOCTRL_GET_WINDOW_SIZE: {
        int *s = arg;
        dispatch_on_main_thread(vo, ^{
            NSSize size = [vo->cocoa->view frame].size;
            s[0] = size.width;
            s[1] = size.height;
        });
        return VO_TRUE;
    }
    case VOCTRL_SET_WINDOW_SIZE: {
        dispatch_on_main_thread(vo, ^{
            int *s = arg;
            [vo->cocoa->window queueNewVideoSize:NSMakeSize(s[0], s[1])];
        });
        return VO_TRUE;
    }
    case VOCTRL_SET_CURSOR_VISIBILITY:
        vo_cocoa_set_cursor_visibility(vo, arg);
        return VO_TRUE;
    case VOCTRL_UPDATE_WINDOW_TITLE:
        cocoa_set_window_title(vo, (const char *) arg);
        return VO_TRUE;
    case VOCTRL_RESTORE_SCREENSAVER:
        enable_power_management(vo);
        return VO_TRUE;
    case VOCTRL_KILL_SCREENSAVER:
        disable_power_management(vo);
        return VO_TRUE;
    case VOCTRL_GET_ICC_PROFILE_PATH:
        vo_cocoa_control_get_icc_profile_path(vo, arg);
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
    return [s->gl_ctx CGLContextObj];
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

@implementation MpvCocoaAdapter
@synthesize vout = _video_output;

- (void)setNeedsResize {
    struct vo_cocoa_state *s = self.vout->cocoa;
    s->did_resize = true;
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
    vo_mouse_movement(self.vout, point.x, point.y);
    [self recalcMovableByWindowBackground:point];
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

- (void)performAsyncResize:(NSSize)size {
    vo_cocoa_resize_redraw(self.vout, size.width, size.height);
}

- (BOOL)isInFullScreenMode {
    return self.vout->opts->fullscreen;
}

- (NSScreen *)fsScreen {
    struct vo_cocoa_state *s = self.vout->cocoa;
    return s->fs_screen;
}

- (void)handleFilesArray:(NSArray *)files
{
    [mpv_shared_app() handleFilesArray:files];
}

- (void)didChangeWindowedScreenProfile:(NSScreen *)screen
{
    struct vo_cocoa_state *s = self.vout->cocoa;
    cocoa_change_profile(self.vout, &s->icc_wnd_profile_path, screen);
    s->icc_profile_path_changed = true;
}
@end
