/*
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>
#include <limits.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <assert.h>

#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/XF86keysym.h>

#include <X11/extensions/scrnsaver.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xrandr.h>

#include "config.h"
#include "misc/bstr.h"
#include "options/options.h"
#include "options/m_config.h"
#include "common/common.h"
#include "common/msg.h"
#include "input/input.h"
#include "input/event.h"
#include "video/image_loader.h"
#include "video/mp_image.h"
#include "x11_common.h"
#include "mpv_talloc.h"

#include "vo.h"
#include "win_state.h"
#include "osdep/io.h"
#include "osdep/timer.h"
#include "osdep/subprocess.h"

#include "input/input.h"
#include "input/keycodes.h"

#define vo_wm_LAYER 1
#define vo_wm_FULLSCREEN 2
#define vo_wm_STAYS_ON_TOP 4
#define vo_wm_ABOVE 8
#define vo_wm_BELOW 16

/* EWMH state actions, see
         http://freedesktop.org/Standards/wm-spec/index.html#id2768769 */
#define NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define NET_WM_STATE_ADD           1    /* add/set property */
#define NET_WM_STATE_TOGGLE        2    /* toggle property  */

#define WIN_LAYER_ONBOTTOM               2
#define WIN_LAYER_NORMAL                 4
#define WIN_LAYER_ONTOP                  6
#define WIN_LAYER_ABOVE_DOCK             10

#define DND_VERSION 5

#define XEMBED_VERSION              0
#define XEMBED_MAPPED               (1 << 0)
#define XEMBED_EMBEDDED_NOTIFY      0
#define XEMBED_REQUEST_FOCUS        3

// ----- Motif header: -------

#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)

#define MWM_FUNC_RESIZE         (1L << 1)
#define MWM_FUNC_MOVE           (1L << 2)
#define MWM_FUNC_MINIMIZE       (1L << 3)
#define MWM_FUNC_MAXIMIZE       (1L << 4)
#define MWM_FUNC_CLOSE          (1L << 5)

#define MWM_DECOR_ALL           (1L << 0)

typedef struct
{
    long flags;
    long functions;
    long decorations;
    long input_mode;
    long state;
} MotifWmHints;

static const char x11_icon_16[] =
#include "generated/etc/mpv-icon-8bit-16x16.png.inc"
;

static const char x11_icon_32[] =
#include "generated/etc/mpv-icon-8bit-32x32.png.inc"
;

static const char x11_icon_64[] =
#include "generated/etc/mpv-icon-8bit-64x64.png.inc"
;

static const char x11_icon_128[] =
#include "generated/etc/mpv-icon-8bit-128x128.png.inc"
;

#define ICON_ENTRY(var) { (char *)var, sizeof(var) }
static const struct bstr x11_icons[] = {
    ICON_ENTRY(x11_icon_16),
    ICON_ENTRY(x11_icon_32),
    ICON_ENTRY(x11_icon_64),
    ICON_ENTRY(x11_icon_128),
    {0}
};

static struct mp_log *x11_error_output;
static atomic_int x11_error_silence;

static void vo_x11_update_geometry(struct vo *vo);
static void vo_x11_fullscreen(struct vo *vo);
static void xscreensaver_heartbeat(struct vo_x11_state *x11);
static void set_screensaver(struct vo_x11_state *x11, bool enabled);
static void vo_x11_selectinput_witherr(struct vo *vo, Display *display,
                                       Window w, long event_mask);
static void vo_x11_setlayer(struct vo *vo, bool ontop);
static void vo_x11_xembed_handle_message(struct vo *vo, XClientMessageEvent *ce);
static void vo_x11_xembed_send_message(struct vo_x11_state *x11, long m[4]);
static void vo_x11_move_resize(struct vo *vo, bool move, bool resize,
                               struct mp_rect rc);
static void vo_x11_maximize(struct vo *vo);
static void vo_x11_minimize(struct vo *vo);

#define XA(x11, s) (XInternAtom((x11)->display, # s, False))
#define XAs(x11, s) XInternAtom((x11)->display, s, False)

#define RC_W(rc) ((rc).x1 - (rc).x0)
#define RC_H(rc) ((rc).y1 - (rc).y0)

static char *x11_atom_name_buf(struct vo_x11_state *x11, Atom atom,
                               char *buf, size_t buf_size)
{
    buf[0] = '\0';

    char *new_name = XGetAtomName(x11->display, atom);
    if (new_name) {
        snprintf(buf, buf_size, "%s", new_name);
        XFree(new_name);
    }

    return buf;
}

#define x11_atom_name(x11, atom) x11_atom_name_buf(x11, atom, (char[80]){0}, 80)

// format = 8 (unsigned char), 16 (short), 32 (long, even on LP64 systems)
// *out_nitems = returned number of items of requested format
static void *x11_get_property(struct vo_x11_state *x11, Window w, Atom property,
                              Atom type, int format, int *out_nitems)
{
    assert(format == 8 || format == 16 || format == 32);
    *out_nitems = 0;
    if (!w)
        return NULL;
    long max_len = 128 * 1024 * 1024; // static maximum limit
    Atom ret_type = 0;
    int ret_format = 0;
    unsigned long ret_nitems = 0;
    unsigned long ret_bytesleft = 0;
    unsigned char *ret_prop = NULL;
    if (XGetWindowProperty(x11->display, w, property, 0, max_len, False, type,
                           &ret_type, &ret_format, &ret_nitems, &ret_bytesleft,
                           &ret_prop) != Success)
        return NULL;
    if (ret_format != format || ret_nitems < 1 || ret_bytesleft) {
        XFree(ret_prop);
        ret_prop = NULL;
        ret_nitems = 0;
    }
    *out_nitems = ret_nitems;
    return ret_prop;
}

static bool x11_get_property_copy(struct vo_x11_state *x11, Window w,
                                  Atom property, Atom type, int format,
                                  void *dst, size_t dst_size)
{
    bool ret = false;
    int len;
    void *ptr = x11_get_property(x11, w, property, type, format, &len);
    if (ptr) {
        size_t ib = format == 32 ? sizeof(long) : format / 8;
        if (dst_size <= len * ib) {
            memcpy(dst, ptr, dst_size);
            ret = true;
        }
        XFree(ptr);
    }
    return ret;
}

static void x11_send_ewmh_msg(struct vo_x11_state *x11, char *message_type,
                              long params[5])
{
    if (!x11->window)
        return;

    XEvent xev = {
        .xclient = {
            .type = ClientMessage,
            .send_event = True,
            .message_type = XInternAtom(x11->display, message_type, False),
            .window = x11->window,
            .format = 32,
        },
    };
    for (int n = 0; n < 5; n++)
        xev.xclient.data.l[n] = params[n];

    if (!XSendEvent(x11->display, x11->rootwin, False,
                    SubstructureRedirectMask | SubstructureNotifyMask,
                    &xev))
        MP_ERR(x11, "Couldn't send EWMH %s message!\n", message_type);
}

// change the _NET_WM_STATE hint. Remove or add the state according to "set".
static void x11_set_ewmh_state(struct vo_x11_state *x11, char *state, bool set)
{
    long params[5] = {
        set ? NET_WM_STATE_ADD : NET_WM_STATE_REMOVE,
        XInternAtom(x11->display, state, False),
        0, // No second state
        1, // source indication: normal
    };
    x11_send_ewmh_msg(x11, "_NET_WM_STATE", params);
}

static void vo_update_cursor(struct vo *vo)
{
    Cursor no_ptr;
    Pixmap bm_no;
    XColor black, dummy;
    Colormap colormap;
    const char bm_no_data[] = {0, 0, 0, 0, 0, 0, 0, 0};
    struct vo_x11_state *x11 = vo->x11;
    Display *disp = x11->display;
    Window win = x11->window;
    bool should_hide = x11->has_focus && !x11->mouse_cursor_visible;

    if (should_hide == x11->mouse_cursor_set)
        return;

    x11->mouse_cursor_set = should_hide;

    if (x11->parent == x11->rootwin || !win)
        return;                 // do not hide if playing on the root window

    if (x11->mouse_cursor_set) {
        colormap = DefaultColormap(disp, DefaultScreen(disp));
        if (!XAllocNamedColor(disp, colormap, "black", &black, &dummy))
            return; // color alloc failed, give up
        bm_no = XCreateBitmapFromData(disp, win, bm_no_data, 8, 8);
        no_ptr = XCreatePixmapCursor(disp, bm_no, bm_no, &black, &black, 0, 0);
        XDefineCursor(disp, win, no_ptr);
        XFreeCursor(disp, no_ptr);
        if (bm_no != None)
            XFreePixmap(disp, bm_no);
        XFreeColors(disp, colormap, &black.pixel, 1, 0);
    } else {
        XDefineCursor(x11->display, x11->window, 0);
    }
}

static int x11_errorhandler(Display *display, XErrorEvent *event)
{
    struct mp_log *log = x11_error_output;
    if (!log)
        return 0;

    char msg[60];
    XGetErrorText(display, event->error_code, (char *) &msg, sizeof(msg));

    int lev = atomic_load(&x11_error_silence) ? MSGL_V : MSGL_ERR;
    mp_msg(log, lev, "X11 error: %s\n", msg);
    mp_msg(log, lev, "Type: %x, display: %p, resourceid: %lx, serial: %lx\n",
               event->type, event->display, event->resourceid, event->serial);
    mp_msg(log, lev, "Error code: %x, request code: %x, minor code: %x\n",
           event->error_code, event->request_code, event->minor_code);

    return 0;
}

void vo_x11_silence_xlib(int dir)
{
    atomic_fetch_add(&x11_error_silence, dir);
}

static int net_wm_support_state_test(struct vo_x11_state *x11, Atom atom)
{
#define NET_WM_STATE_TEST(x) { \
    if (atom == XA(x11, _NET_WM_STATE_##x)) { \
        MP_DBG(x11, "Detected wm supports " #x " state.\n" ); \
        return vo_wm_##x; \
    } \
}

    NET_WM_STATE_TEST(FULLSCREEN);
    NET_WM_STATE_TEST(ABOVE);
    NET_WM_STATE_TEST(STAYS_ON_TOP);
    NET_WM_STATE_TEST(BELOW);
    return 0;
}

static int vo_wm_detect(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    int i;
    int wm = 0;
    int nitems;
    Atom *args = NULL;
    Window win = x11->rootwin;

    if (x11->parent)
        return 0;

// -- supports layers
    args = x11_get_property(x11, win, XA(x11, _WIN_PROTOCOLS), XA_ATOM, 32,
                            &nitems);
    if (args) {
        for (i = 0; i < nitems; i++) {
            if (args[i] == XA(x11, _WIN_LAYER)) {
                MP_DBG(x11, "Detected wm supports layers.\n");
                wm |= vo_wm_LAYER;
            }
        }
        XFree(args);
    }
// --- netwm
    args = x11_get_property(x11, win, XA(x11, _NET_SUPPORTED), XA_ATOM, 32,
                            &nitems);
    if (args) {
        MP_DBG(x11, "Detected wm supports NetWM.\n");
        if (x11->opts->x11_netwm >= 0) {
            for (i = 0; i < nitems; i++)
                wm |= net_wm_support_state_test(vo->x11, args[i]);
        } else {
            MP_DBG(x11, "NetWM usage disabled by user.\n");
        }
        XFree(args);
    }

    if (wm == 0)
        MP_DBG(x11, "Unknown wm type...\n");
    if (x11->opts->x11_netwm > 0 && !(wm & vo_wm_FULLSCREEN)) {
        MP_WARN(x11, "Forcing NetWM FULLSCREEN support.\n");
        wm |= vo_wm_FULLSCREEN;
    }
    return wm;
}

static void xrandr_read(struct vo_x11_state *x11)
{
    for(int i = 0; i < x11->num_displays; i++)
        talloc_free(x11->displays[i].name);

    x11->num_displays = 0;

    if (x11->xrandr_event < 0) {
        int event_base, error_base;
        if (!XRRQueryExtension(x11->display, &event_base, &error_base)) {
            MP_VERBOSE(x11, "Couldn't init Xrandr.\n");
            return;
        }
        x11->xrandr_event = event_base + RRNotify;
        XRRSelectInput(x11->display, x11->rootwin, RRScreenChangeNotifyMask |
                       RRCrtcChangeNotifyMask | RROutputChangeNotifyMask);
    }

    XRRScreenResources *r = XRRGetScreenResourcesCurrent(x11->display, x11->rootwin);
    if (!r) {
        MP_VERBOSE(x11, "Xrandr doesn't work.\n");
        return;
    }

    int primary_id = -1;
    RROutput primary = XRRGetOutputPrimary(x11->display, x11->rootwin);
    for (int o = 0; o < r->noutput; o++) {
        RROutput output = r->outputs[o];
        XRRCrtcInfo *crtc = NULL;
        XRROutputInfo *out = XRRGetOutputInfo(x11->display, r, output);
        if (!out || !out->crtc)
            goto next;
        crtc = XRRGetCrtcInfo(x11->display, r, out->crtc);
        if (!crtc)
            goto next;
        for (int om = 0; om < out->nmode; om++) {
            RRMode xm = out->modes[om];
            for (int n = 0; n < r->nmode; n++) {
                XRRModeInfo m = r->modes[n];
                if (m.id != xm || crtc->mode != xm)
                    continue;
                if (x11->num_displays >= MAX_DISPLAYS)
                    continue;
                double vTotal = m.vTotal;
                if (m.modeFlags & RR_DoubleScan)
                    vTotal *= 2;
                if (m.modeFlags & RR_Interlace)
                    vTotal /= 2;
                struct xrandr_display d = {
                    .rc = { crtc->x, crtc->y,
                            crtc->x + crtc->width, crtc->y + crtc->height },
                    .fps = m.dotClock / (m.hTotal * vTotal),
                    .name = talloc_strdup(x11, out->name),
                };
                int num = x11->num_displays++;
                MP_VERBOSE(x11, "Display %d (%s): [%d, %d, %d, %d] @ %f FPS\n",
                           num, d.name, d.rc.x0, d.rc.y0, d.rc.x1, d.rc.y1, d.fps);
                x11->displays[num] = d;
                if (output == primary)
                    primary_id = num;
            }
        }
    next:
        if (crtc)
            XRRFreeCrtcInfo(crtc);
        if (out)
            XRRFreeOutputInfo(out);
    }

    for (int i = 0; i < x11->num_displays; i++) {
        struct xrandr_display *d = &(x11->displays[i]);

        if (i == primary_id) {
            d->atom_id = 0;
            continue;
        }
        if (primary_id > 0 && i < primary_id) {
            d->atom_id = i+1;
            continue;
        }
        d->atom_id = i;
    }

    XRRFreeScreenResources(r);
}

static void vo_x11_update_screeninfo(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    struct mp_vo_opts *opts = x11->opts;
    bool all_screens = opts->fullscreen && opts->fsscreen_id == -2;
    x11->screenrc = (struct mp_rect){.x1 = x11->ws_width, .y1 = x11->ws_height};
    if ((opts->screen_id >= -1 || opts->screen_name) && XineramaIsActive(x11->display) &&
         !all_screens)
    {
        int screen = opts->fullscreen ? opts->fsscreen_id : opts->screen_id;
        XineramaScreenInfo *screens;
        int num_screens;

        if (opts->fullscreen && opts->fsscreen_id == -1)
            screen = opts->screen_id;

        if (screen == -1 && (opts->fsscreen_name || opts->screen_name)) {
            char *screen_name = opts->fullscreen ? opts->fsscreen_name : opts->screen_name;
            if (screen_name) {
                bool screen_found = false;
                for (int n = 0; n < x11->num_displays; n++) {
                    char *display_name = x11->displays[n].name;
                    if (!strcmp(display_name, screen_name)) {
                        screen = n;
                        screen_found = true;
                        break;
                    }
                }
                if (!screen_found)
                    MP_WARN(x11, "Screen name %s not found!\n", screen_name);
            }
        }

        screens = XineramaQueryScreens(x11->display, &num_screens);
        if (screen >= num_screens)
            screen = num_screens - 1;
        if (screen == -1) {
            int x = x11->winrc.x0 + RC_W(x11->winrc) / 2;
            int y = x11->winrc.y0 + RC_H(x11->winrc) / 2;
            for (screen = num_screens - 1; screen > 0; screen--) {
                int left = screens[screen].x_org;
                int right = left + screens[screen].width;
                int top = screens[screen].y_org;
                int bottom = top + screens[screen].height;
                if (left <= x && x <= right && top <= y && y <= bottom)
                    break;
            }
        }
        if (screen < 0)
            screen = 0;
        x11->screenrc = (struct mp_rect){
            .x0 = screens[screen].x_org,
            .y0 = screens[screen].y_org,
            .x1 = screens[screen].x_org + screens[screen].width,
            .y1 = screens[screen].y_org + screens[screen].height,
        };

        XFree(screens);
    }
}

// Get the monitors for the 4 edges of the rectangle spanning all screens.
static void vo_x11_get_bounding_monitors(struct vo_x11_state *x11, long b[4])
{
    //top  bottom left   right
    b[0] = b[1] = b[2] = b[3] = 0;
    int num_screens = 0;
    XineramaScreenInfo *screens = XineramaQueryScreens(x11->display, &num_screens);
    if (!screens)
        return;
    for (int n = 0; n < num_screens; n++) {
        XineramaScreenInfo *s = &screens[n];
        if (s->y_org < screens[b[0]].y_org)
            b[0] = n;
        if (s->y_org + s->height > screens[b[1]].y_org + screens[b[1]].height)
            b[1] = n;
        if (s->x_org < screens[b[2]].x_org)
            b[2] = n;
        if (s->x_org + s->width > screens[b[3]].x_org + screens[b[3]].width)
            b[3] = n;
    }
    XFree(screens);
}

int vo_x11_init(struct vo *vo)
{
    char *dispName;

    assert(!vo->x11);

    XInitThreads();

    struct vo_x11_state *x11 = talloc_ptrtype(NULL, x11);
    *x11 = (struct vo_x11_state){
        .log = mp_log_new(x11, vo->log, "x11"),
        .input_ctx = vo->input_ctx,
        .screensaver_enabled = true,
        .xrandr_event = -1,
        .wakeup_pipe = {-1, -1},
        .dpi_scale = 1,
        .opts_cache = m_config_cache_alloc(x11, vo->global, &vo_sub_opts),
    };
    x11->opts = x11->opts_cache->opts;
    vo->x11 = x11;

    x11_error_output = x11->log;
    XSetErrorHandler(x11_errorhandler);

    dispName = XDisplayName(NULL);

    MP_VERBOSE(x11, "X11 opening display: %s\n", dispName);

    x11->display = XOpenDisplay(dispName);
    if (!x11->display) {
        MP_MSG(x11, vo->probing ? MSGL_V : MSGL_ERR,
               "couldn't open the X11 display (%s)!\n", dispName);
        goto error;
    }
    x11->screen = DefaultScreen(x11->display);  // screen ID
    x11->rootwin = RootWindow(x11->display, x11->screen);   // root window ID

    if (x11->opts->WinID >= 0)
        x11->parent = x11->opts->WinID ? x11->opts->WinID : x11->rootwin;

    if (!x11->opts->native_keyrepeat) {
        Bool ok = False;
        XkbSetDetectableAutoRepeat(x11->display, True, &ok);
        x11->no_autorepeat = ok;
    }

    x11->xim = XOpenIM(x11->display, NULL, NULL, NULL);
    if (!x11->xim)
        MP_WARN(x11, "XOpenIM() failed. Unicode input will not work.\n");

    x11->ws_width = DisplayWidth(x11->display, x11->screen);
    x11->ws_height = DisplayHeight(x11->display, x11->screen);

    if (strncmp(dispName, "unix:", 5) == 0)
        dispName += 4;
    else if (strncmp(dispName, "localhost:", 10) == 0)
        dispName += 9;
    x11->display_is_local = dispName[0] == ':' &&
                            strtoul(dispName + 1, NULL, 10) < 10;
    MP_DBG(x11, "X11 running at %dx%d (\"%s\" => %s display)\n",
           x11->ws_width, x11->ws_height, dispName,
           x11->display_is_local ? "local" : "remote");

    int w_mm = DisplayWidthMM(x11->display, x11->screen);
    int h_mm = DisplayHeightMM(x11->display, x11->screen);
    double dpi_x = x11->ws_width * 25.4 / w_mm;
    double dpi_y = x11->ws_height * 25.4 / h_mm;
    double base_dpi = 96;
    if (isfinite(dpi_x) && isfinite(dpi_y) && x11->opts->hidpi_window_scale) {
        int s_x = lrint(MPCLAMP(dpi_x / base_dpi, 0, 10));
        int s_y = lrint(MPCLAMP(dpi_y / base_dpi, 0, 10));
        if (s_x == s_y && s_x > 1 && s_x < 10) {
            x11->dpi_scale = s_x;
            MP_VERBOSE(x11, "Assuming DPI scale %d for prescaling. This can "
                       "be disabled with --hidpi-window-scale=no.\n",
                       x11->dpi_scale);
        }
    }

    x11->wm_type = vo_wm_detect(vo);

    x11->event_fd = ConnectionNumber(x11->display);
    mp_make_wakeup_pipe(x11->wakeup_pipe);

    xrandr_read(x11);

    vo_x11_update_geometry(vo);

    return 1;

error:
    vo_x11_uninit(vo);
    return 0;
}

static const struct mp_keymap keymap[] = {
    // special keys
    {XK_Pause, MP_KEY_PAUSE}, {XK_Escape, MP_KEY_ESC},
    {XK_BackSpace, MP_KEY_BS}, {XK_Tab, MP_KEY_TAB}, {XK_Return, MP_KEY_ENTER},
    {XK_Menu, MP_KEY_MENU}, {XK_Print, MP_KEY_PRINT},
    {XK_Cancel, MP_KEY_CANCEL}, {XK_ISO_Left_Tab, MP_KEY_TAB},

    // cursor keys
    {XK_Left, MP_KEY_LEFT}, {XK_Right, MP_KEY_RIGHT}, {XK_Up, MP_KEY_UP},
    {XK_Down, MP_KEY_DOWN},

    // navigation block
    {XK_Insert, MP_KEY_INSERT}, {XK_Delete, MP_KEY_DELETE},
    {XK_Home, MP_KEY_HOME}, {XK_End, MP_KEY_END}, {XK_Page_Up, MP_KEY_PAGE_UP},
    {XK_Page_Down, MP_KEY_PAGE_DOWN},

    // F-keys
    {XK_F1, MP_KEY_F+1}, {XK_F2, MP_KEY_F+2}, {XK_F3, MP_KEY_F+3},
    {XK_F4, MP_KEY_F+4}, {XK_F5, MP_KEY_F+5}, {XK_F6, MP_KEY_F+6},
    {XK_F7, MP_KEY_F+7}, {XK_F8, MP_KEY_F+8}, {XK_F9, MP_KEY_F+9},
    {XK_F10, MP_KEY_F+10}, {XK_F11, MP_KEY_F+11}, {XK_F12, MP_KEY_F+12},

    // numpad independent of numlock
    {XK_KP_Subtract, '-'}, {XK_KP_Add, '+'}, {XK_KP_Multiply, '*'},
    {XK_KP_Divide, '/'}, {XK_KP_Enter, MP_KEY_KPENTER},

    // numpad with numlock
    {XK_KP_0, MP_KEY_KP0}, {XK_KP_1, MP_KEY_KP1}, {XK_KP_2, MP_KEY_KP2},
    {XK_KP_3, MP_KEY_KP3}, {XK_KP_4, MP_KEY_KP4}, {XK_KP_5, MP_KEY_KP5},
    {XK_KP_6, MP_KEY_KP6}, {XK_KP_7, MP_KEY_KP7}, {XK_KP_8, MP_KEY_KP8},
    {XK_KP_9, MP_KEY_KP9}, {XK_KP_Decimal, MP_KEY_KPDEC},
    {XK_KP_Separator, MP_KEY_KPDEC},

    // numpad without numlock
    {XK_KP_Insert, MP_KEY_KPINS}, {XK_KP_End, MP_KEY_KP1},
    {XK_KP_Down, MP_KEY_KP2}, {XK_KP_Page_Down, MP_KEY_KP3},
    {XK_KP_Left, MP_KEY_KP4}, {XK_KP_Begin, MP_KEY_KP5},
    {XK_KP_Right, MP_KEY_KP6}, {XK_KP_Home, MP_KEY_KP7}, {XK_KP_Up, MP_KEY_KP8},
    {XK_KP_Page_Up, MP_KEY_KP9}, {XK_KP_Delete, MP_KEY_KPDEL},

    {XF86XK_MenuKB, MP_KEY_MENU},
    {XF86XK_AudioPlay, MP_KEY_PLAY}, {XF86XK_AudioPause, MP_KEY_PAUSE},
    {XF86XK_AudioStop, MP_KEY_STOP},
    {XF86XK_AudioPrev, MP_KEY_PREV}, {XF86XK_AudioNext, MP_KEY_NEXT},
    {XF86XK_AudioRewind, MP_KEY_REWIND}, {XF86XK_AudioForward, MP_KEY_FORWARD},
    {XF86XK_AudioMute, MP_KEY_MUTE},
    {XF86XK_AudioLowerVolume, MP_KEY_VOLUME_DOWN},
    {XF86XK_AudioRaiseVolume, MP_KEY_VOLUME_UP},
    {XF86XK_HomePage, MP_KEY_HOMEPAGE}, {XF86XK_WWW, MP_KEY_WWW},
    {XF86XK_Mail, MP_KEY_MAIL}, {XF86XK_Favorites, MP_KEY_FAVORITES},
    {XF86XK_Search, MP_KEY_SEARCH}, {XF86XK_Sleep, MP_KEY_SLEEP},

    {0, 0}
};

static int vo_x11_lookupkey(int key)
{
    const char *passthrough_keys = " -+*/<>`~!@#$%^&()_{}:;\"\',.?\\|=[]";
    int mpkey = 0;
    if ((key >= 'a' && key <= 'z') ||
        (key >= 'A' && key <= 'Z') ||
        (key >= '0' && key <= '9') ||
        (key > 0 && key < 256 && strchr(passthrough_keys, key)))
        mpkey = key;

    if (!mpkey)
        mpkey = lookup_keymap_table(keymap, key);

    // XFree86 keysym range; typically contains obscure "extra" keys
    if (!mpkey && key >= 0x10080001 && key <= 0x1008FFFF) {
        mpkey = MP_KEY_UNKNOWN_RESERVED_START + (key - 0x10080000);
        if (mpkey > MP_KEY_UNKNOWN_RESERVED_LAST)
            mpkey = 0;
    }

    return mpkey;
}

static void vo_x11_decoration(struct vo *vo, bool d)
{
    struct vo_x11_state *x11 = vo->x11;

    if (x11->parent || !x11->window)
        return;

    Atom motif_hints = XA(x11, _MOTIF_WM_HINTS);
    MotifWmHints mhints = {0};
    bool got = x11_get_property_copy(x11, x11->window, motif_hints,
                                     motif_hints, 32, &mhints, sizeof(mhints));
    // hints weren't set, and decorations requested -> assume WM displays them
    if (!got && d)
        return;
    if (!got) {
        mhints.flags = MWM_HINTS_FUNCTIONS;
        mhints.functions = MWM_FUNC_MOVE | MWM_FUNC_CLOSE | MWM_FUNC_MINIMIZE |
                           MWM_FUNC_MAXIMIZE | MWM_FUNC_RESIZE;
    }
    mhints.flags |= MWM_HINTS_DECORATIONS;
    mhints.decorations = d ? MWM_DECOR_ALL : 0;
    XChangeProperty(x11->display, x11->window, motif_hints, motif_hints, 32,
                    PropModeReplace, (unsigned char *) &mhints, 5);
}

static void vo_x11_wm_hints(struct vo *vo, Window window)
{
    struct vo_x11_state *x11 = vo->x11;
    XWMHints hints = {0};
    hints.flags = InputHint | StateHint;
    hints.input = 1;
    hints.initial_state = NormalState;
    XSetWMHints(x11->display, window, &hints);
}

static void vo_x11_classhint(struct vo *vo, Window window, const char *name)
{
    struct vo_x11_state *x11 = vo->x11;
    struct mp_vo_opts *opts = x11->opts;
    XClassHint wmClass;
    long pid = getpid();

    wmClass.res_name = opts->winname ? opts->winname : (char *)name;
    wmClass.res_class = "mpv";
    XSetClassHint(x11->display, window, &wmClass);
    XChangeProperty(x11->display, window, XA(x11, _NET_WM_PID), XA_CARDINAL,
                    32, PropModeReplace, (unsigned char *) &pid, 1);
}

void vo_x11_uninit(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    if (!x11)
        return;

    mp_input_put_key(x11->input_ctx, MP_INPUT_RELEASE_ALL);

    set_screensaver(x11, true);

    if (x11->window != None && x11->window != x11->rootwin) {
        XUnmapWindow(x11->display, x11->window);
        XDestroyWindow(x11->display, x11->window);
    }
    if (x11->xic)
        XDestroyIC(x11->xic);
    if (x11->colormap != None)
        XFreeColormap(vo->x11->display, x11->colormap);

    MP_DBG(x11, "uninit ...\n");
    if (x11->xim)
        XCloseIM(x11->xim);
    if (x11->display) {
        XSetErrorHandler(NULL);
        x11_error_output = NULL;
        XCloseDisplay(x11->display);
    }

    if (x11->wakeup_pipe[0] >= 0) {
        close(x11->wakeup_pipe[0]);
        close(x11->wakeup_pipe[1]);
    }

    talloc_free(x11);
    vo->x11 = NULL;
}

#define DND_PROPERTY "mpv_dnd_selection"

static void vo_x11_dnd_init_window(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;

    Atom version = DND_VERSION;
    XChangeProperty(x11->display, x11->window, XA(x11, XdndAware), XA_ATOM,
                    32, PropModeReplace, (unsigned char *)&version, 1);
}

// The Atom does not always map to a mime type, but often.
static char *x11_dnd_mime_type_buf(struct vo_x11_state *x11, Atom atom,
                                   char *buf, size_t buf_size)
{
    if (atom == XInternAtom(x11->display, "UTF8_STRING", False))
        return "text";
    return x11_atom_name_buf(x11, atom, buf, buf_size);
}

#define x11_dnd_mime_type(x11, atom) \
    x11_dnd_mime_type_buf(x11, atom, (char[80]){0}, 80)

static bool dnd_format_is_better(struct vo_x11_state *x11, Atom cur, Atom new)
{
    int new_score = mp_event_get_mime_type_score(x11->input_ctx,
                                                 x11_dnd_mime_type(x11, new));
    int cur_score = -1;
    if (cur) {
        cur_score = mp_event_get_mime_type_score(x11->input_ctx,
                                                 x11_dnd_mime_type(x11, cur));
    }
    return new_score >= 0 && new_score > cur_score;
}

static void dnd_select_format(struct vo_x11_state *x11, Atom *args, int items)
{
    x11->dnd_requested_format = 0;

    for (int n = 0; n < items; n++) {
        MP_VERBOSE(x11, "DnD type: '%s'\n", x11_atom_name(x11, args[n]));
        // There are other types; possibly not worth supporting.
        if (dnd_format_is_better(x11, x11->dnd_requested_format, args[n]))
            x11->dnd_requested_format = args[n];
    }

    MP_VERBOSE(x11, "Selected DnD type: %s\n", x11->dnd_requested_format ?
                    x11_atom_name(x11, x11->dnd_requested_format) : "(none)");
}

static void dnd_reset(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;

    x11->dnd_src_window = 0;
    x11->dnd_requested_format = 0;
}

static void vo_x11_dnd_handle_message(struct vo *vo, XClientMessageEvent *ce)
{
    struct vo_x11_state *x11 = vo->x11;

    if (!x11->window)
        return;

    if (ce->message_type == XA(x11, XdndEnter)) {
        x11->dnd_requested_format = 0;

        Window src = ce->data.l[0];
        if (ce->data.l[1] & 1) {
            int nitems;
            Atom *args = x11_get_property(x11, src, XA(x11, XdndTypeList),
                                          XA_ATOM, 32, &nitems);
            if (args) {
                dnd_select_format(x11, args, nitems);
                XFree(args);
            }
        } else {
            Atom args[3];
            for (int n = 2; n <= 4; n++)
                args[n - 2] = ce->data.l[n];
            dnd_select_format(x11, args, 3);
        }
    } else if (ce->message_type == XA(x11, XdndPosition)) {
        x11->dnd_requested_action = ce->data.l[4];

        Window src = ce->data.l[0];
        XEvent xev;

        xev.xclient.type = ClientMessage;
        xev.xclient.serial = 0;
        xev.xclient.send_event = True;
        xev.xclient.message_type = XA(x11, XdndStatus);
        xev.xclient.window = src;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = x11->window;
        xev.xclient.data.l[1] = x11->dnd_requested_format ? 1 : 0;
        xev.xclient.data.l[2] = 0;
        xev.xclient.data.l[3] = 0;
        xev.xclient.data.l[4] = XA(x11, XdndActionCopy);

        XSendEvent(x11->display, src, False, 0, &xev);
    } else if (ce->message_type == XA(x11, XdndDrop)) {
        x11->dnd_src_window = ce->data.l[0];
        XConvertSelection(x11->display, XA(x11, XdndSelection),
                          x11->dnd_requested_format, XAs(x11, DND_PROPERTY),
                          x11->window, ce->data.l[2]);
    } else if (ce->message_type == XA(x11, XdndLeave)) {
        dnd_reset(vo);
    }
}

static void vo_x11_dnd_handle_selection(struct vo *vo, XSelectionEvent *se)
{
    struct vo_x11_state *x11 = vo->x11;

    if (!x11->window || !x11->dnd_src_window)
        return;

    bool success = false;

    if (se->selection == XA(x11, XdndSelection) &&
        se->property == XAs(x11, DND_PROPERTY) &&
        se->target == x11->dnd_requested_format)
    {
        int nitems;
        void *prop = x11_get_property(x11, x11->window, XAs(x11, DND_PROPERTY),
                                      x11->dnd_requested_format, 8, &nitems);
        if (prop) {
            enum mp_dnd_action action =
                x11->dnd_requested_action == XA(x11, XdndActionCopy) ?
                DND_REPLACE : DND_APPEND;

            char *mime_type = x11_dnd_mime_type(x11, x11->dnd_requested_format);
            MP_VERBOSE(x11, "Dropping type: %s (%s)\n",
                       x11_atom_name(x11, x11->dnd_requested_format), mime_type);

            // No idea if this is guaranteed to be \0-padded, so use bstr.
            success = mp_event_drop_mime_data(x11->input_ctx, mime_type,
                                              (bstr){prop, nitems}, action) > 0;
            XFree(prop);
        }
    }

    XEvent xev;

    xev.xclient.type = ClientMessage;
    xev.xclient.serial = 0;
    xev.xclient.send_event = True;
    xev.xclient.message_type = XA(x11, XdndFinished);
    xev.xclient.window = x11->dnd_src_window;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = x11->window;
    xev.xclient.data.l[1] = success ? 1 : 0;
    xev.xclient.data.l[2] = success ? XA(x11, XdndActionCopy) : 0;
    xev.xclient.data.l[3] = 0;
    xev.xclient.data.l[4] = 0;

    XSendEvent(x11->display, x11->dnd_src_window, False, 0, &xev);

    dnd_reset(vo);
}

static void update_vo_size(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;

    if (RC_W(x11->winrc) != vo->dwidth || RC_H(x11->winrc) != vo->dheight) {
        vo->dwidth = RC_W(x11->winrc);
        vo->dheight = RC_H(x11->winrc);
        x11->pending_vo_events |= VO_EVENT_RESIZE;
    }
}

static int get_mods(unsigned int state)
{
    int modifiers = 0;
    if (state & ShiftMask)
        modifiers |= MP_KEY_MODIFIER_SHIFT;
    if (state & ControlMask)
        modifiers |= MP_KEY_MODIFIER_CTRL;
    if (state & Mod1Mask)
        modifiers |= MP_KEY_MODIFIER_ALT;
    if (state & Mod4Mask)
        modifiers |= MP_KEY_MODIFIER_META;
    return modifiers;
}

static void vo_x11_update_composition_hint(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;

    long hint = 0;
    switch (x11->opts->x11_bypass_compositor) {
    case 0: hint = 0; break; // leave default
    case 1: hint = 1; break; // always bypass
    case 2: hint = x11->fs ? 1 : 0; break; // bypass in FS
    case 3: hint = 2; break; // always enable
    }

    XChangeProperty(x11->display, x11->window, XA(x11,_NET_WM_BYPASS_COMPOSITOR),
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&hint, 1);
}

static void vo_x11_check_net_wm_state_change(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    struct mp_vo_opts *opts = x11->opts;

    if (x11->parent)
        return;

    if (x11->wm_type & vo_wm_FULLSCREEN) {
        int num_elems;
        long *elems = x11_get_property(x11, x11->window, XA(x11, _NET_WM_STATE),
                                       XA_ATOM, 32, &num_elems);
        int is_fullscreen = 0, is_minimized = 0, is_maximized = 0;
        if (elems) {
            Atom fullscreen_prop = XA(x11, _NET_WM_STATE_FULLSCREEN);
            Atom hidden = XA(x11, _NET_WM_STATE_HIDDEN);
            Atom max_vert = XA(x11, _NET_WM_STATE_MAXIMIZED_VERT);
            Atom max_horiz = XA(x11, _NET_WM_STATE_MAXIMIZED_HORZ);
            for (int n = 0; n < num_elems; n++) {
                if (elems[n] == fullscreen_prop)
                    is_fullscreen = 1;
                if (elems[n] == hidden)
                    is_minimized = 1;
                if (elems[n] == max_vert || elems[n] == max_horiz)
                    is_maximized = 1;
            }
            XFree(elems);
        }

        if (opts->window_maximized && !is_maximized && x11->pending_geometry_change) {
            vo_x11_config_vo_window(vo);
            x11->pending_geometry_change = false;
        }

        opts->window_minimized = is_minimized;
        m_config_cache_write_opt(x11->opts_cache, &opts->window_minimized);
        opts->window_maximized = is_maximized;
        m_config_cache_write_opt(x11->opts_cache, &opts->window_maximized);

        if ((x11->opts->fullscreen && !is_fullscreen) ||
            (!x11->opts->fullscreen && is_fullscreen))
        {
            x11->opts->fullscreen = is_fullscreen;
            x11->fs = is_fullscreen;
            m_config_cache_write_opt(x11->opts_cache, &x11->opts->fullscreen);

            if (!is_fullscreen && (x11->pos_changed_during_fs ||
                                   x11->size_changed_during_fs))
            {
                vo_x11_move_resize(vo, x11->pos_changed_during_fs,
                                       x11->size_changed_during_fs,
                                       x11->nofsrc);
            }

            x11->size_changed_during_fs = false;
            x11->pos_changed_during_fs = false;

            vo_x11_update_composition_hint(vo);
        }
    }
}

static void vo_x11_check_net_wm_desktop_change(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;

    if (x11->parent)
        return;

    long params[1] = {0};
    if (x11_get_property_copy(x11, x11->window, XA(x11, _NET_WM_DESKTOP),
                              XA_CARDINAL, 32, params, sizeof(params)))
    {
        x11->opts->all_workspaces = params[0] == -1; // (gets sign-extended?)
        m_config_cache_write_opt(x11->opts_cache, &x11->opts->all_workspaces);
    }
}

// Releasing all keys on key-up or defocus is simpler and ensures no keys can
// get "stuck".
static void release_all_keys(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;

    if (x11->no_autorepeat)
        mp_input_put_key(x11->input_ctx, MP_INPUT_RELEASE_ALL);
    x11->win_drag_button1_down = false;
}

void vo_x11_check_events(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    Display *display = vo->x11->display;
    XEvent Event;

    xscreensaver_heartbeat(vo->x11);

    while (XPending(display)) {
        XNextEvent(display, &Event);
        MP_TRACE(x11, "XEvent: %d\n", Event.type);
        switch (Event.type) {
        case Expose:
            x11->pending_vo_events |= VO_EVENT_EXPOSE;
            break;
        case ConfigureNotify:
            if (x11->window == None)
                break;
            vo_x11_update_geometry(vo);
            if (x11->parent && Event.xconfigure.window == x11->parent) {
                MP_TRACE(x11, "adjusting embedded window position\n");
                XMoveResizeWindow(x11->display, x11->window,
                                  0, 0, RC_W(x11->winrc), RC_H(x11->winrc));
            }
            break;
        case KeyPress: {
            char buf[100];
            KeySym keySym = 0;
            int modifiers = get_mods(Event.xkey.state);
            if (x11->no_autorepeat)
                modifiers |= MP_KEY_STATE_DOWN;
            if (x11->xic) {
                Status status;
                int len = Xutf8LookupString(x11->xic, &Event.xkey, buf,
                                            sizeof(buf), &keySym, &status);
                int mpkey = vo_x11_lookupkey(keySym);
                if (mpkey) {
                    mp_input_put_key(x11->input_ctx, mpkey | modifiers);
                } else if (status == XLookupChars || status == XLookupBoth) {
                    struct bstr t = { buf, len };
                    mp_input_put_key_utf8(x11->input_ctx, modifiers, t);
                }
            } else {
                XLookupString(&Event.xkey, buf, sizeof(buf), &keySym,
                              &x11->compose_status);
                int mpkey = vo_x11_lookupkey(keySym);
                if (mpkey)
                    mp_input_put_key(x11->input_ctx, mpkey | modifiers);
            }
            break;
        }
        case FocusIn:
            x11->has_focus = true;
            vo_update_cursor(vo);
            x11->pending_vo_events |= VO_EVENT_FOCUS;
            break;
        case FocusOut:
            release_all_keys(vo);
            x11->has_focus = false;
            vo_update_cursor(vo);
            x11->pending_vo_events |= VO_EVENT_FOCUS;
            break;
        case KeyRelease:
            release_all_keys(vo);
            break;
        case MotionNotify:
            if (x11->win_drag_button1_down && !x11->fs &&
                !mp_input_test_dragging(x11->input_ctx, Event.xmotion.x,
                                                        Event.xmotion.y))
            {
                mp_input_put_key(x11->input_ctx, MP_INPUT_RELEASE_ALL);
                XUngrabPointer(x11->display, CurrentTime);

                long params[5] = {
                    Event.xmotion.x_root, Event.xmotion.y_root,
                    8, // _NET_WM_MOVERESIZE_MOVE
                    1, // button 1
                    1, // source indication: normal
                };
                x11_send_ewmh_msg(x11, "_NET_WM_MOVERESIZE", params);
            } else {
                mp_input_set_mouse_pos(x11->input_ctx, Event.xmotion.x,
                                                       Event.xmotion.y);
            }
            x11->win_drag_button1_down = false;
            break;
        case LeaveNotify:
            if (Event.xcrossing.mode != NotifyNormal)
                break;
            x11->win_drag_button1_down = false;
            mp_input_put_key(x11->input_ctx, MP_KEY_MOUSE_LEAVE);
            break;
        case EnterNotify:
            if (Event.xcrossing.mode != NotifyNormal)
                break;
            mp_input_put_key(x11->input_ctx, MP_KEY_MOUSE_ENTER);
            break;
        case ButtonPress:
            if (Event.xbutton.button - 1 >= MP_KEY_MOUSE_BTN_COUNT)
                break;
            if (Event.xbutton.button == 1)
                x11->win_drag_button1_down = true;
            mp_input_put_key(x11->input_ctx,
                             (MP_MBTN_BASE + Event.xbutton.button - 1) |
                             get_mods(Event.xbutton.state) | MP_KEY_STATE_DOWN);
            long msg[4] = {XEMBED_REQUEST_FOCUS};
            vo_x11_xembed_send_message(x11, msg);
            break;
        case ButtonRelease:
            if (Event.xbutton.button - 1 >= MP_KEY_MOUSE_BTN_COUNT)
                break;
            if (Event.xbutton.button == 1)
                x11->win_drag_button1_down = false;
            mp_input_put_key(x11->input_ctx,
                             (MP_MBTN_BASE + Event.xbutton.button - 1) |
                             get_mods(Event.xbutton.state) | MP_KEY_STATE_UP);
            break;
        case MapNotify:
            x11->window_hidden = false;
            x11->pseudo_mapped = true;
            x11->current_icc_screen = -1;
            vo_x11_update_geometry(vo);
            break;
        case DestroyNotify:
            MP_WARN(x11, "Our window was destroyed, exiting\n");
            mp_input_put_key(x11->input_ctx, MP_KEY_CLOSE_WIN);
            x11->window = 0;
            break;
        case ClientMessage:
            if (Event.xclient.message_type == XA(x11, WM_PROTOCOLS) &&
                Event.xclient.data.l[0] == XA(x11, WM_DELETE_WINDOW))
                mp_input_put_key(x11->input_ctx, MP_KEY_CLOSE_WIN);
            vo_x11_dnd_handle_message(vo, &Event.xclient);
            vo_x11_xembed_handle_message(vo, &Event.xclient);
            break;
        case SelectionNotify:
            vo_x11_dnd_handle_selection(vo, &Event.xselection);
            break;
        case PropertyNotify:
            if (Event.xproperty.atom == XA(x11, _NET_FRAME_EXTENTS) ||
                Event.xproperty.atom == XA(x11, WM_STATE))
            {
                if (!x11->pseudo_mapped && !x11->parent) {
                    MP_VERBOSE(x11, "not waiting for MapNotify\n");
                    x11->pseudo_mapped = true;
                }
            } else if (Event.xproperty.atom == XA(x11, _NET_WM_STATE)) {
                vo_x11_check_net_wm_state_change(vo);
            } else if (Event.xproperty.atom == XA(x11, _NET_WM_DESKTOP)) {
                vo_x11_check_net_wm_desktop_change(vo);
            } else if (Event.xproperty.atom == x11->icc_profile_property) {
                x11->pending_vo_events |= VO_EVENT_ICC_PROFILE_CHANGED;
            }
            break;
        default:
            if (Event.type == x11->ShmCompletionEvent) {
                if (x11->ShmCompletionWaitCount > 0)
                    x11->ShmCompletionWaitCount--;
            }
            if (Event.type == x11->xrandr_event) {
                xrandr_read(x11);
                vo_x11_update_geometry(vo);
            }
            break;
        }
    }

    update_vo_size(vo);
}

static void vo_x11_sizehint(struct vo *vo, struct mp_rect rc, bool override_pos)
{
    struct vo_x11_state *x11 = vo->x11;
    struct mp_vo_opts *opts = x11->opts;

    if (!x11->window || x11->parent)
        return;

    bool force_pos = opts->geometry.xy_valid ||     // explicitly forced by user
                     opts->force_window_position || // resize -> reset position
                     opts->screen_id >= 0 ||        // force onto screen area
                     x11->parent ||                 // force to fill parent
                     override_pos;                  // for fullscreen and such

    XSizeHints *hint = XAllocSizeHints();
    if (!hint)
        return; // OOM

    hint->flags |= PSize | (force_pos ? PPosition : 0);
    hint->x = rc.x0;
    hint->y = rc.y0;
    hint->width = RC_W(rc);
    hint->height = RC_H(rc);
    hint->max_width = 0;
    hint->max_height = 0;

    if (opts->keepaspect && opts->keepaspect_window) {
        hint->flags |= PAspect;
        hint->min_aspect.x = hint->width;
        hint->min_aspect.y = hint->height;
        hint->max_aspect.x = hint->width;
        hint->max_aspect.y = hint->height;
    }

    // Set minimum height/width to 4 to avoid off-by-one errors.
    hint->flags |= PMinSize;
    hint->min_width = hint->min_height = 4;

    hint->flags |= PWinGravity;
    hint->win_gravity = StaticGravity;

    XSetWMNormalHints(x11->display, x11->window, hint);
    XFree(hint);
}

static void vo_x11_move_resize(struct vo *vo, bool move, bool resize,
                               struct mp_rect rc)
{
    if (!vo->x11->window)
        return;
    int w = RC_W(rc), h = RC_H(rc);
    XWindowChanges req = {.x = rc.x0, .y = rc.y0, .width = w, .height = h};
    unsigned mask = (move ? CWX | CWY : 0) | (resize ? CWWidth | CWHeight : 0);
    if (mask)
        XConfigureWindow(vo->x11->display, vo->x11->window, mask, &req);
    vo_x11_sizehint(vo, rc, false);
}

// set a X text property that expects a UTF8_STRING type
static void vo_x11_set_property_utf8(struct vo *vo, Atom name, const char *t)
{
    struct vo_x11_state *x11 = vo->x11;

    XChangeProperty(x11->display, x11->window, name, XA(x11, UTF8_STRING), 8,
                    PropModeReplace, t, strlen(t));
}

// set a X text property that expects a STRING or COMPOUND_TEXT type
static void vo_x11_set_property_string(struct vo *vo, Atom name, const char *t)
{
    struct vo_x11_state *x11 = vo->x11;
    XTextProperty prop = {0};

    if (Xutf8TextListToTextProperty(x11->display, (char **)&t, 1,
                                    XStdICCTextStyle, &prop) == Success)
    {
        XSetTextProperty(x11->display, x11->window, &prop, name);
    } else {
        // Strictly speaking this violates the ICCCM, but there's no way we
        // can do this correctly.
        vo_x11_set_property_utf8(vo, name, t);
    }

    if (prop.value)
        XFree(prop.value);
}

static void vo_x11_update_window_title(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;

    if (!x11->window || !x11->window_title)
        return;

    vo_x11_set_property_string(vo, XA_WM_NAME, x11->window_title);
    vo_x11_set_property_string(vo, XA_WM_ICON_NAME, x11->window_title);
    vo_x11_set_property_utf8(vo, XA(x11, _NET_WM_NAME), x11->window_title);
    vo_x11_set_property_utf8(vo, XA(x11, _NET_WM_ICON_NAME), x11->window_title);
}

static void vo_x11_xembed_update(struct vo_x11_state *x11, int flags)
{
    if (!x11->window || !x11->parent)
        return;

    long xembed_info[] = {XEMBED_VERSION, flags};
    Atom name = XA(x11, _XEMBED_INFO);
    XChangeProperty(x11->display, x11->window, name, name, 32,
                    PropModeReplace, (char *)xembed_info, 2);
}

static void vo_x11_xembed_handle_message(struct vo *vo, XClientMessageEvent *ce)
{
    struct vo_x11_state *x11 = vo->x11;
    if (!x11->window || !x11->parent || ce->message_type != XA(x11, _XEMBED))
        return;

    long msg = ce->data.l[1];
    if (msg == XEMBED_EMBEDDED_NOTIFY)
        MP_VERBOSE(x11, "Parent windows supports XEmbed.\n");
}

static void vo_x11_xembed_send_message(struct vo_x11_state *x11, long m[4])
{
    if (!x11->window || !x11->parent)
        return;
    XEvent ev = {.xclient = {
        .type = ClientMessage,
        .window = x11->parent,
        .message_type = XA(x11, _XEMBED),
        .format = 32,
        .data = {.l = { CurrentTime, m[0], m[1], m[2], m[3] }},
    } };
    XSendEvent(x11->display, x11->parent, False, NoEventMask, &ev);
}

static void vo_x11_set_wm_icon(struct vo_x11_state *x11)
{
    int icon_size = 0;
    long *icon = talloc_array(NULL, long, 0);

    for (int n = 0; x11_icons[n].start; n++) {
        struct mp_image *img =
            load_image_png_buf(x11_icons[n].start, x11_icons[n].len, IMGFMT_RGBA);
        if (!img)
            continue;
        int new_size = 2 + img->w * img->h;
        MP_RESIZE_ARRAY(NULL, icon, icon_size + new_size);
        long *cur = icon + icon_size;
        icon_size += new_size;
        *cur++ = img->w;
        *cur++ = img->h;
        for (int y = 0; y < img->h; y++) {
            uint8_t *s = (uint8_t *)img->planes[0] + img->stride[0] * y;
            for (int x = 0; x < img->w; x++) {
                *cur++ = s[x * 4 + 0] | (s[x * 4 + 1] << 8) |
                         (s[x * 4 + 2] << 16) | ((unsigned)s[x * 4 + 3] << 24);
            }
        }
        talloc_free(img);
    }

    XChangeProperty(x11->display, x11->window, XA(x11, _NET_WM_ICON),
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)icon, icon_size);
    talloc_free(icon);
}

static void vo_x11_create_window(struct vo *vo, XVisualInfo *vis,
                                 struct mp_rect rc)
{
    struct vo_x11_state *x11 = vo->x11;

    assert(x11->window == None);
    assert(!x11->xic);

    XVisualInfo vinfo_storage;
    if (!vis) {
        vis = &vinfo_storage;
        XWindowAttributes att;
        XGetWindowAttributes(x11->display, x11->rootwin, &att);
        XMatchVisualInfo(x11->display, x11->screen, att.depth, TrueColor, vis);
    }

    if (x11->colormap == None) {
        x11->colormap = XCreateColormap(x11->display, x11->rootwin,
                                        vis->visual, AllocNone);
    }

    unsigned long xswamask = CWBorderPixel | CWColormap;
    XSetWindowAttributes xswa = {
        .border_pixel = 0,
        .colormap = x11->colormap,
    };

    Window parent = x11->parent;
    if (!parent)
        parent = x11->rootwin;

    x11->window =
        XCreateWindow(x11->display, parent, rc.x0, rc.y0, RC_W(rc), RC_H(rc), 0,
                      vis->depth, CopyFromParent, vis->visual, xswamask, &xswa);
    Atom protos[1] = {XA(x11, WM_DELETE_WINDOW)};
    XSetWMProtocols(x11->display, x11->window, protos, 1);

    x11->mouse_cursor_set = false;
    x11->mouse_cursor_visible = true;
    vo_update_cursor(vo);

    if (x11->xim) {
        x11->xic = XCreateIC(x11->xim,
                             XNInputStyle, XIMPreeditNone | XIMStatusNone,
                             XNClientWindow, x11->window,
                             XNFocusWindow, x11->window,
                             NULL);
    }

    if (!x11->parent) {
        vo_x11_update_composition_hint(vo);
        vo_x11_set_wm_icon(x11);
        vo_x11_update_window_title(vo);
        vo_x11_dnd_init_window(vo);
        vo_x11_set_property_utf8(vo, XA(x11, _GTK_THEME_VARIANT), "dark");
    }
    vo_x11_xembed_update(x11, 0);
}

static void vo_x11_map_window(struct vo *vo, struct mp_rect rc)
{
    struct vo_x11_state *x11 = vo->x11;

    vo_x11_move_resize(vo, true, true, rc);
    vo_x11_decoration(vo, x11->opts->border);

    if (x11->opts->fullscreen && (x11->wm_type & vo_wm_FULLSCREEN)) {
        Atom state = XA(x11, _NET_WM_STATE_FULLSCREEN);
        XChangeProperty(x11->display, x11->window, XA(x11, _NET_WM_STATE), XA_ATOM,
                        32, PropModeAppend, (unsigned char *)&state, 1);
        x11->fs = 1;
        // The "saved" positions are bogus, so reset them when leaving FS again.
        x11->size_changed_during_fs = true;
        x11->pos_changed_during_fs = true;
    }

    if (x11->opts->fsscreen_id != -1) {
        long params[5] = {0};
        if (x11->opts->fsscreen_id >= 0) {
            for (int n = 0; n < 4; n++)
                params[n] = x11->opts->fsscreen_id;
        } else {
            vo_x11_get_bounding_monitors(x11, &params[0]);
        }
        params[4] = 1; // source indication: normal
        x11_send_ewmh_msg(x11, "_NET_WM_FULLSCREEN_MONITORS", params);
    }

    if (x11->opts->all_workspaces || x11->opts->geometry.ws > 0) {
        long v = x11->opts->all_workspaces
                ? 0xFFFFFFFF : x11->opts->geometry.ws - 1;
        XChangeProperty(x11->display, x11->window, XA(x11, _NET_WM_DESKTOP),
                        XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&v, 1);
    }

    vo_x11_update_composition_hint(vo);

    // map window
    int events = StructureNotifyMask | ExposureMask | PropertyChangeMask |
                 LeaveWindowMask | EnterWindowMask | FocusChangeMask;
    if (mp_input_mouse_enabled(x11->input_ctx))
        events |= PointerMotionMask | ButtonPressMask | ButtonReleaseMask;
    if (mp_input_vo_keyboard_enabled(x11->input_ctx))
        events |= KeyPressMask | KeyReleaseMask;
    vo_x11_selectinput_witherr(vo, x11->display, x11->window, events);
    XMapWindow(x11->display, x11->window);

    if (x11->opts->window_maximized) // don't override WM default on "no"
        vo_x11_maximize(vo);
    if (x11->opts->window_minimized) // don't override WM default on "no"
        vo_x11_minimize(vo);

    if (x11->opts->fullscreen && (x11->wm_type & vo_wm_FULLSCREEN))
        x11_set_ewmh_state(x11, "_NET_WM_STATE_FULLSCREEN", 1);

    vo_x11_xembed_update(x11, XEMBED_MAPPED);
}

static void vo_x11_highlevel_resize(struct vo *vo, struct mp_rect rc)
{
    struct vo_x11_state *x11 = vo->x11;
    struct mp_vo_opts *opts = x11->opts;

    bool reset_pos = opts->force_window_position;
    if (reset_pos) {
        x11->nofsrc = rc;
    } else {
        x11->nofsrc.x1 = x11->nofsrc.x0 + RC_W(rc);
        x11->nofsrc.y1 = x11->nofsrc.y0 + RC_H(rc);
    }

    if (opts->fullscreen) {
        x11->size_changed_during_fs = true;
        x11->pos_changed_during_fs = reset_pos;
        vo_x11_sizehint(vo, rc, false);
    } else {
        vo_x11_move_resize(vo, reset_pos, true, rc);
    }
}

static void wait_until_mapped(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    if (!x11->pseudo_mapped)
        x11_send_ewmh_msg(x11, "_NET_REQUEST_FRAME_EXTENTS", (long[5]){0});
    while (!x11->pseudo_mapped && x11->window) {
        XWindowAttributes att;
        XGetWindowAttributes(x11->display, x11->window, &att);
        if (att.map_state != IsUnmapped) {
            x11->pseudo_mapped = true;
            break;
        }
        XEvent unused;
        XPeekEvent(x11->display, &unused);
        vo_x11_check_events(vo);
    }
}

// Create the X11 window. There is only 1, and it must be created before
// vo_x11_config_vo_window() is called. vis can be NULL for default.
bool vo_x11_create_vo_window(struct vo *vo, XVisualInfo *vis,
                             const char *classname)
{
    struct vo_x11_state *x11 = vo->x11;
    assert(!x11->window);

    if (x11->parent) {
        if (x11->parent == x11->rootwin) {
            x11->window = x11->rootwin;
            x11->pseudo_mapped = true;
            XSelectInput(x11->display, x11->window, StructureNotifyMask);
        } else {
            XSelectInput(x11->display, x11->parent, StructureNotifyMask);
        }
    }
    if (x11->window == None) {
        vo_x11_create_window(vo, vis, (struct mp_rect){.x1 = 320, .y1 = 200 });
        vo_x11_classhint(vo, x11->window, classname);
        vo_x11_wm_hints(vo, x11->window);
        x11->window_hidden = true;
    }

    return !!x11->window;
}

// Resize the window (e.g. new file, or video resolution change)
void vo_x11_config_vo_window(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    struct mp_vo_opts *opts = x11->opts;

    assert(x11->window);

    vo_x11_update_screeninfo(vo);

    struct vo_win_geometry geo;
    vo_calc_window_geometry2(vo, &x11->screenrc, x11->dpi_scale, &geo);
    vo_apply_window_geometry(vo, &geo);

    struct mp_rect rc = geo.win;

    if (x11->parent) {
        vo_x11_update_geometry(vo);
        rc = (struct mp_rect){0, 0, RC_W(x11->winrc), RC_H(x11->winrc)};
    }

    bool reset_size = x11->old_dw != RC_W(rc) || x11->old_dh != RC_H(rc);
    x11->old_dw = RC_W(rc);
    x11->old_dh = RC_H(rc);

    if (x11->window_hidden) {
        x11->nofsrc = rc;
        vo_x11_map_window(vo, rc);
    } else if (reset_size) {
        vo_x11_highlevel_resize(vo, rc);
    }

    if (opts->ontop)
        vo_x11_setlayer(vo, opts->ontop);

    vo_x11_fullscreen(vo);

    wait_until_mapped(vo);
    vo_x11_update_geometry(vo);
    update_vo_size(vo);
    x11->pending_vo_events &= ~VO_EVENT_RESIZE; // implicitly done by the VO
}

static void vo_x11_setlayer(struct vo *vo, bool ontop)
{
    struct vo_x11_state *x11 = vo->x11;
    if (x11->parent || !x11->window)
        return;

    if (x11->wm_type & (vo_wm_STAYS_ON_TOP | vo_wm_ABOVE)) {
        char *state = "_NET_WM_STATE_ABOVE";

        // Not in EWMH - but the old code preferred this (maybe it is "better")
        if (x11->wm_type & vo_wm_STAYS_ON_TOP)
            state = "_NET_WM_STATE_STAYS_ON_TOP";

        x11_set_ewmh_state(x11, state, ontop);

        MP_VERBOSE(x11, "NET style stay on top (%d). Using state %s.\n",
                   ontop, state);
    } else if (x11->wm_type & vo_wm_LAYER) {
        if (!x11->orig_layer) {
            x11->orig_layer = WIN_LAYER_NORMAL;
            x11_get_property_copy(x11, x11->window, XA(x11, _WIN_LAYER),
                                  XA_CARDINAL, 32, &x11->orig_layer, sizeof(long));
            MP_VERBOSE(x11, "original window layer is %ld.\n", x11->orig_layer);
        }

        long params[5] = {0};
        // if not fullscreen, stay on default layer
        params[0] = ontop ? WIN_LAYER_ABOVE_DOCK : x11->orig_layer;
        params[1] = CurrentTime;
        MP_VERBOSE(x11, "Layered style stay on top (layer %ld).\n", params[0]);
        x11_send_ewmh_msg(x11, "_WIN_LAYER", params);
    }
}

static bool rc_overlaps(struct mp_rect rc1, struct mp_rect rc2)
{
    return mp_rect_intersection(&rc1, &rc2); // changes the first argument
}

// which screen's ICC profile we're going to use
static int get_icc_screen(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    int cx = x11->winrc.x0 + (x11->winrc.x1 - x11->winrc.x0)/2,
    cy = x11->winrc.y0 + (x11->winrc.y1 - x11->winrc.y0)/2;
    int screen = x11->current_icc_screen; // xinerama screen number
    for (int n = 0; n < x11->num_displays; n++) {
        struct xrandr_display *disp = &x11->displays[n];
        if (mp_rect_contains(&disp->rc, cx, cy)) {
            screen = n;
            break;
        }
    }
    return screen;
}

// update x11->winrc with current boundaries of vo->x11->window
static void vo_x11_update_geometry(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    int x = 0, y = 0;
    unsigned w, h, dummy_uint;
    int dummy_int;
    Window dummy_win;
    Window win = x11->parent ? x11->parent : x11->window;
    x11->winrc = (struct mp_rect){0, 0, 0, 0};
    if (win) {
        XGetGeometry(x11->display, win, &dummy_win, &dummy_int, &dummy_int,
                     &w, &h, &dummy_int, &dummy_uint);
        if (w > INT_MAX || h > INT_MAX)
            w = h = 0;
        XTranslateCoordinates(x11->display, win, x11->rootwin, 0, 0,
                              &x, &y, &dummy_win);
        x11->winrc = (struct mp_rect){x, y, x + w, y + h};
    }
    double fps = 1000.0;
    for (int n = 0; n < x11->num_displays; n++) {
        struct xrandr_display *disp = &x11->displays[n];
        disp->overlaps = rc_overlaps(disp->rc, x11->winrc);
        if (disp->overlaps)
            fps = MPMIN(fps, disp->fps);
    }
    double fallback = x11->num_displays > 0 ? x11->displays[0].fps : 0;
    fps = fps < 1000.0 ? fps : fallback;
    if (fps != x11->current_display_fps)
        MP_VERBOSE(x11, "Current display FPS: %f\n", fps);
    x11->current_display_fps = fps;
    // might have changed displays
    x11->pending_vo_events |= VO_EVENT_WIN_STATE;
    int icc_screen = get_icc_screen(vo);
    if (x11->current_icc_screen != icc_screen) {
        x11->current_icc_screen = icc_screen;
        x11->pending_vo_events |= VO_EVENT_ICC_PROFILE_CHANGED;
    }
}

static void vo_x11_fullscreen(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    struct mp_vo_opts *opts = x11->opts;

    if (opts->fullscreen == x11->fs)
        return;
    x11->fs = opts->fullscreen; // x11->fs now contains the new state
    if (x11->parent || !x11->window)
        return;

    // Save old state before entering fullscreen
    if (x11->fs) {
        vo_x11_update_geometry(vo);
        x11->nofsrc = x11->winrc;
    }

    struct mp_rect rc = x11->nofsrc;

    if (x11->wm_type & vo_wm_FULLSCREEN) {
        x11_set_ewmh_state(x11, "_NET_WM_STATE_FULLSCREEN", x11->fs);
        if (!x11->fs && (x11->pos_changed_during_fs ||
                         x11->size_changed_during_fs))
        {
            if (x11->screenrc.x0 == rc.x0 && x11->screenrc.x1 == rc.x1 &&
                x11->screenrc.y0 == rc.y0 && x11->screenrc.y1 == rc.y1)
            {
                // Workaround for some WMs switching back to FS in this case.
                MP_VERBOSE(x11, "avoiding triggering old-style fullscreen\n");
                rc.x1 -= 1;
                rc.y1 -= 1;
            }
            vo_x11_move_resize(vo, x11->pos_changed_during_fs,
                                   x11->size_changed_during_fs, rc);
        }
    } else {
        if (x11->fs) {
            vo_x11_update_screeninfo(vo);
            rc = x11->screenrc;
        }

        vo_x11_decoration(vo, opts->border && !x11->fs);
        vo_x11_sizehint(vo, rc, true);

        XMoveResizeWindow(x11->display, x11->window, rc.x0, rc.y0,
                          RC_W(rc), RC_H(rc));

        vo_x11_setlayer(vo, x11->fs || opts->ontop);

        XRaiseWindow(x11->display, x11->window);
        XFlush(x11->display);
    }

    x11->size_changed_during_fs = false;
    x11->pos_changed_during_fs = false;

    vo_x11_update_composition_hint(vo);
}

static void vo_x11_maximize(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;

    long params[5] = {
        x11->opts->window_maximized ? NET_WM_STATE_ADD : NET_WM_STATE_REMOVE,
        XA(x11, _NET_WM_STATE_MAXIMIZED_VERT),
        XA(x11, _NET_WM_STATE_MAXIMIZED_HORZ),
        1, // source indication: normal
    };
    x11_send_ewmh_msg(x11, "_NET_WM_STATE", params);
}

static void vo_x11_minimize(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;

    if (x11->opts->window_minimized) {
        XIconifyWindow(x11->display, x11->window, x11->screen);
    } else {
        long params[5] = {0};
        x11_send_ewmh_msg(x11, "_NET_ACTIVE_WINDOW", params);
    }
}

static void vo_x11_set_geometry(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;

    if (!x11->window)
        return;

    if (x11->opts->window_maximized) {
        x11->pending_geometry_change = true;
    } else {
        vo_x11_config_vo_window(vo);
    }
}

int vo_x11_control(struct vo *vo, int *events, int request, void *arg)
{
    struct vo_x11_state *x11 = vo->x11;
    struct mp_vo_opts *opts = x11->opts;
    switch (request) {
    case VOCTRL_CHECK_EVENTS:
        vo_x11_check_events(vo);
        *events |= x11->pending_vo_events;
        x11->pending_vo_events = 0;
        return VO_TRUE;
    case VOCTRL_VO_OPTS_CHANGED: {
        void *opt;
        while (m_config_cache_get_next_changed(x11->opts_cache, &opt)) {
            if (opt == &opts->fullscreen)
                vo_x11_fullscreen(vo);
            if (opt == &opts->ontop)
                vo_x11_setlayer(vo, opts->ontop);
            if (opt == &opts->border)
                vo_x11_decoration(vo, opts->border);
            if (opt == &opts->all_workspaces) {
                long params[5] = {0xFFFFFFFF, 1};
                if (!opts->all_workspaces) {
                    x11_get_property_copy(x11, x11->rootwin,
                                          XA(x11, _NET_CURRENT_DESKTOP),
                                          XA_CARDINAL, 32, &params[0],
                                          sizeof(params[0]));
                }
                x11_send_ewmh_msg(x11, "_NET_WM_DESKTOP", params);
            }
            if (opt == &opts->window_minimized)
                vo_x11_minimize(vo);
            if (opt == &opts->window_maximized)
                vo_x11_maximize(vo);
            if (opt == &opts->geometry || opt == &opts->autofit ||
                opt == &opts->autofit_smaller || opt == &opts->autofit_larger)
            {
                vo_x11_set_geometry(vo);
            }
        }
        return VO_TRUE;
    }
    case VOCTRL_GET_UNFS_WINDOW_SIZE: {
        int *s = arg;
        if (!x11->window || x11->parent)
            return VO_FALSE;
        s[0] = (x11->fs ? RC_W(x11->nofsrc) : RC_W(x11->winrc)) / x11->dpi_scale;
        s[1] = (x11->fs ? RC_H(x11->nofsrc) : RC_H(x11->winrc)) / x11->dpi_scale;
        return VO_TRUE;
    }
    case VOCTRL_SET_UNFS_WINDOW_SIZE: {
        int *s = arg;
        if (!x11->window || x11->parent)
            return VO_FALSE;
        int w = s[0] * x11->dpi_scale;
        int h = s[1] * x11->dpi_scale;
        struct mp_rect rc = x11->winrc;
        rc.x1 = rc.x0 + w;
        rc.y1 = rc.y0 + h;
        if (x11->opts->window_maximized) {
            x11->opts->window_maximized = false;
            m_config_cache_write_opt(x11->opts_cache,
                    &x11->opts->window_maximized);
            vo_x11_maximize(vo);
        }
        vo_x11_highlevel_resize(vo, rc);
        if (!x11->fs) { // guess new window size, instead of waiting for X
            x11->winrc.x1 = x11->winrc.x0 + w;
            x11->winrc.y1 = x11->winrc.y0 + h;
        }
        return VO_TRUE;
    }
    case VOCTRL_GET_FOCUSED: {
        *(bool *)arg = x11->has_focus;
        return VO_TRUE;
    }
    case VOCTRL_GET_DISPLAY_NAMES: {
        if (!x11->pseudo_mapped)
            return VO_FALSE;
        char **names = NULL;
        int displays_spanned = 0;
        for (int n = 0; n < x11->num_displays; n++) {
            if (rc_overlaps(x11->displays[n].rc, x11->winrc))
                MP_TARRAY_APPEND(NULL, names, displays_spanned,
                                 talloc_strdup(NULL, x11->displays[n].name));
        }
        MP_TARRAY_APPEND(NULL, names, displays_spanned, NULL);
        *(char ***)arg = names;
        return VO_TRUE;
    }
    case VOCTRL_GET_ICC_PROFILE: {
        if (!x11->pseudo_mapped)
            return VO_NOTAVAIL;
        int screen = get_icc_screen(vo);
        int atom_id = x11->displays[screen].atom_id;
        char prop[80];
        snprintf(prop, sizeof(prop), "_ICC_PROFILE");
        if (atom_id > 0)
            mp_snprintf_cat(prop, sizeof(prop), "_%d", atom_id);
        x11->icc_profile_property = XAs(x11, prop);
        int len;
        MP_VERBOSE(x11, "Retrieving ICC profile for display: %d\n", screen);
        void *icc = x11_get_property(x11, x11->rootwin, x11->icc_profile_property,
                                     XA_CARDINAL, 8, &len);
        if (!icc)
            return VO_FALSE;
        *(bstr *)arg = bstrdup(NULL, (bstr){icc, len});
        XFree(icc);
        // Watch x11->icc_profile_property
        XSelectInput(x11->display, x11->rootwin, PropertyChangeMask);
        return VO_TRUE;
    }
    case VOCTRL_SET_CURSOR_VISIBILITY:
        x11->mouse_cursor_visible = *(bool *)arg;
        vo_update_cursor(vo);
        return VO_TRUE;
    case VOCTRL_KILL_SCREENSAVER:
        set_screensaver(x11, false);
        return VO_TRUE;
    case VOCTRL_RESTORE_SCREENSAVER:
        set_screensaver(x11, true);
        return VO_TRUE;
    case VOCTRL_UPDATE_WINDOW_TITLE:
        talloc_free(x11->window_title);
        x11->window_title = talloc_strdup(x11, (char *)arg);
        if (!x11->parent)
            vo_x11_update_window_title(vo);
        return VO_TRUE;
    case VOCTRL_GET_DISPLAY_FPS: {
        double fps = x11->current_display_fps;
        if (fps <= 0)
            break;
        *(double *)arg = fps;
        return VO_TRUE;
    }
    case VOCTRL_GET_DISPLAY_RES: {
        if (!x11->window || x11->parent)
            return VO_NOTAVAIL;
        ((int *)arg)[0] = x11->screenrc.x1;
        ((int *)arg)[1] = x11->screenrc.y1;
        return VO_TRUE;
    }
    case VOCTRL_GET_HIDPI_SCALE:
        *(double *)arg = x11->dpi_scale;
        return VO_TRUE;
    }
    return VO_NOTIMPL;
}

void vo_x11_wakeup(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;

    (void)write(x11->wakeup_pipe[1], &(char){0}, 1);
}

void vo_x11_wait_events(struct vo *vo, int64_t until_time_us)
{
    struct vo_x11_state *x11 = vo->x11;

    struct pollfd fds[2] = {
        { .fd = x11->event_fd, .events = POLLIN },
        { .fd = x11->wakeup_pipe[0], .events = POLLIN },
    };
    int64_t wait_us = until_time_us - mp_time_us();
    int timeout_ms = MPCLAMP((wait_us + 999) / 1000, 0, 10000);

    poll(fds, 2, timeout_ms);

    if (fds[1].revents & POLLIN)
        mp_flush_wakeup_pipe(x11->wakeup_pipe[0]);
}

static void xscreensaver_heartbeat(struct vo_x11_state *x11)
{
    double time = mp_time_sec();

    if (x11->display && !x11->screensaver_enabled &&
        (time - x11->screensaver_time_last) >= 10)
    {
        x11->screensaver_time_last = time;
        XResetScreenSaver(x11->display);
    }
}

static int xss_suspend(Display *mDisplay, Bool suspend)
{
    int event, error, major, minor;
    if (XScreenSaverQueryExtension(mDisplay, &event, &error) != True ||
        XScreenSaverQueryVersion(mDisplay, &major, &minor) != True)
        return 0;
    if (major < 1 || (major == 1 && minor < 1))
        return 0;
    XScreenSaverSuspend(mDisplay, suspend);
    return 1;
}

static void set_screensaver(struct vo_x11_state *x11, bool enabled)
{
    Display *mDisplay = x11->display;
    if (!mDisplay || x11->screensaver_enabled == enabled)
        return;
    MP_VERBOSE(x11, "%s screensaver.\n", enabled ? "Enabling" : "Disabling");
    x11->screensaver_enabled = enabled;
    if (xss_suspend(mDisplay, !enabled))
        return;
    int nothing;
    if (DPMSQueryExtension(mDisplay, &nothing, &nothing)) {
        BOOL onoff = 0;
        CARD16 state;
        DPMSInfo(mDisplay, &state, &onoff);
        if (!x11->dpms_touched && enabled)
            return; // enable DPMS only we we disabled it before
        if (enabled != !!onoff) {
            MP_VERBOSE(x11, "Setting DMPS: %s.\n", enabled ? "on" : "off");
            if (enabled) {
                DPMSEnable(mDisplay);
            } else {
                DPMSDisable(mDisplay);
                x11->dpms_touched = true;
            }
            DPMSInfo(mDisplay, &state, &onoff);
            if (enabled != !!onoff)
                MP_WARN(x11, "DPMS state could not be set.\n");
        }
    }
}

static void vo_x11_selectinput_witherr(struct vo *vo,
                                       Display *display,
                                       Window w,
                                       long event_mask)
{
    XSelectInput(display, w, NoEventMask);

    // NOTE: this can raise BadAccess, which should be ignored by the X error
    //       handler; also see below
    XSelectInput(display, w, event_mask);

    // Test whether setting the event mask failed (with a BadAccess X error,
    // although we don't know whether this really happened).
    // This is needed for obscure situations like using --rootwin with a window
    // manager active.
    XWindowAttributes a;
    if (XGetWindowAttributes(display, w, &a)) {
        long bad = ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
        if ((event_mask & bad) && (a.all_event_masks & bad) &&
            ((a.your_event_mask & bad) != (event_mask & bad)))
        {
            MP_ERR(vo->x11, "X11 error: error during XSelectInput "
                   "call, trying without mouse events\n");
            XSelectInput(display, w, event_mask & ~bad);
        }
    }
}

bool vo_x11_screen_is_composited(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    char buf[50];
    sprintf(buf, "_NET_WM_CM_S%d", x11->screen);
    Atom NET_WM_CM = XInternAtom(x11->display, buf, False);
    return XGetSelectionOwner(x11->display, NET_WM_CM) != None;
}

// Return whether the given visual has alpha (when compositing is used).
bool vo_x11_is_rgba_visual(XVisualInfo *v)
{
    // This is a heuristic at best. Note that normal 8 bit Visuals use
    // a depth of 24, even if the pixels are padded to 32 bit. If the
    // depth is higher than 24, the remaining bits must be alpha.
    // Note: vinfo->bits_per_rgb appears to be useless (is always 8).
    unsigned long mask = v->depth == sizeof(unsigned long) * 8 ?
        (unsigned long)-1 : (1UL << v->depth) - 1;
    return mask & ~(v->red_mask | v->green_mask | v->blue_mask);
}
