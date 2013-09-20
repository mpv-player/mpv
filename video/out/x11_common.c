/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>
#include <limits.h>

#include "config.h"
#include "mpvcore/bstr.h"
#include "mpvcore/options.h"
#include "mpvcore/mp_msg.h"
#include "mpvcore/input/input.h"
#include "libavutil/common.h"
#include "x11_common.h"
#include "talloc.h"

#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "vo.h"
#include "aspect.h"
#include "osdep/timer.h"

#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

#ifdef CONFIG_XSS
#include <X11/extensions/scrnsaver.h>
#endif

#ifdef CONFIG_XDPMS
#include <X11/extensions/dpms.h>
#endif

#ifdef CONFIG_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#ifdef CONFIG_XF86VM
#include <X11/extensions/xf86vmode.h>
#endif

#ifdef CONFIG_XF86XK
#include <X11/XF86keysym.h>
#endif

#if CONFIG_ZLIB
#include <zlib.h>
#endif

#include "mpvcore/input/input.h"
#include "mpvcore/input/keycodes.h"

#define vo_wm_LAYER 1
#define vo_wm_FULLSCREEN 2
#define vo_wm_STAYS_ON_TOP 4
#define vo_wm_ABOVE 8
#define vo_wm_BELOW 16
#define vo_wm_MWM 32
#define vo_wm_NETWM (vo_wm_FULLSCREEN | vo_wm_STAYS_ON_TOP | vo_wm_ABOVE | \
                     vo_wm_BELOW)

/* EWMH state actions, see
         http://freedesktop.org/Standards/wm-spec/index.html#id2768769 */
#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

#define WIN_LAYER_ONBOTTOM               2
#define WIN_LAYER_NORMAL                 4
#define WIN_LAYER_ONTOP                  6
#define WIN_LAYER_ABOVE_DOCK             10

// ----- Motif header: -------

#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define MWM_HINTS_INPUT_MODE    (1L << 2)
#define MWM_HINTS_STATUS        (1L << 3)

#define MWM_FUNC_ALL            (1L << 0)
#define MWM_FUNC_RESIZE         (1L << 1)
#define MWM_FUNC_MOVE           (1L << 2)
#define MWM_FUNC_MINIMIZE       (1L << 3)
#define MWM_FUNC_MAXIMIZE       (1L << 4)
#define MWM_FUNC_CLOSE          (1L << 5)

#define MWM_DECOR_ALL           (1L << 0)
#define MWM_DECOR_BORDER        (1L << 1)
#define MWM_DECOR_RESIZEH       (1L << 2)
#define MWM_DECOR_TITLE         (1L << 3)
#define MWM_DECOR_MENU          (1L << 4)
#define MWM_DECOR_MINIMIZE      (1L << 5)
#define MWM_DECOR_MAXIMIZE      (1L << 6)

#define MWM_INPUT_MODELESS 0
#define MWM_INPUT_PRIMARY_APPLICATION_MODAL 1
#define MWM_INPUT_SYSTEM_MODAL 2
#define MWM_INPUT_FULL_APPLICATION_MODAL 3
#define MWM_INPUT_APPLICATION_MODAL MWM_INPUT_PRIMARY_APPLICATION_MODAL

#define MWM_TEAROFF_WINDOW      (1L << 0)

typedef struct
{
    long flags;
    long functions;
    long decorations;
    long input_mode;
    long state;
} MotifWmHints;

static const char x11_icon[] =
#include "video/out/x11_icon.inc"
;

static void vo_x11_update_geometry(struct vo *vo);
static void vo_x11_fullscreen(struct vo *vo);
static int vo_x11_get_fs_type(struct vo *vo);
static void xscreensaver_heartbeat(struct vo_x11_state *x11);
static void saver_on(struct vo_x11_state *x11);
static void saver_off(struct vo_x11_state *x11);
static void vo_x11_selectinput_witherr(struct vo *vo, Display *display,
                                       Window w, long event_mask);
static void vo_x11_setlayer(struct vo *vo, Window vo_window, int layer);
static void vo_x11_create_colormap(struct vo_x11_state *x11,
                                   XVisualInfo *vinfo);

/*
 * Sends the EWMH fullscreen state event.
 *
 * action: could be one of _NET_WM_STATE_REMOVE -- remove state
 *                         _NET_WM_STATE_ADD    -- add state
 *                         _NET_WM_STATE_TOGGLE -- toggle
 */
static void vo_x11_ewmh_fullscreen(struct vo_x11_state *x11, int action)
{
    assert(action == _NET_WM_STATE_REMOVE || action == _NET_WM_STATE_ADD ||
           action == _NET_WM_STATE_TOGGLE);

    if (x11->fs_type & vo_wm_FULLSCREEN) {
        XEvent xev;

        /* init X event structure for _NET_WM_FULLSCREEN client message */
        xev.xclient.type = ClientMessage;
        xev.xclient.serial = 0;
        xev.xclient.send_event = True;
        xev.xclient.message_type = x11->XA_NET_WM_STATE;
        xev.xclient.window = x11->window;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = action;
        xev.xclient.data.l[1] = x11->XA_NET_WM_STATE_FULLSCREEN;
        xev.xclient.data.l[2] = 0;
        xev.xclient.data.l[3] = 0;
        xev.xclient.data.l[4] = 0;

        /* finally send that damn thing */
        if (!XSendEvent(x11->display, DefaultRootWindow(x11->display), False,
                        SubstructureRedirectMask | SubstructureNotifyMask,
                        &xev))
        {
            MP_ERR(x11, "Couldn't send EWMH fullscreen event!\n");
        }
    }
}

static void vo_set_cursor_hidden(struct vo *vo, bool cursor_hidden)
{
    Cursor no_ptr;
    Pixmap bm_no;
    XColor black, dummy;
    Colormap colormap;
    const char bm_no_data[] = {0, 0, 0, 0, 0, 0, 0, 0};
    struct vo_x11_state *x11 = vo->x11;
    Display *disp = x11->display;
    Window win = x11->window;

    if (cursor_hidden == x11->mouse_cursor_hidden)
        return;

    x11->mouse_cursor_hidden = cursor_hidden;

    if (vo->opts->WinID == 0 || win == 0)
        return;                 // do not hide if playing on the root window

    if (x11->mouse_cursor_hidden) {
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
    char msg[60];

    XGetErrorText(display, event->error_code, (char *) &msg, sizeof(msg));

    mp_msg(MSGT_VO, MSGL_ERR, "X11 error: %s\n", msg);

    mp_msg(MSGT_VO, MSGL_V,
           "Type: %x, display: %p, resourceid: %lx, serial: %lx\n",
           event->type, event->display, event->resourceid, event->serial);
    mp_msg(MSGT_VO, MSGL_V,
           "Error code: %x, request code: %x, minor code: %x\n",
           event->error_code, event->request_code, event->minor_code);

//    abort();
    return 0;
}

void fstype_help(void)
{
    mp_tmsg(MSGT_VO, MSGL_INFO, "Available fullscreen layer change modes:\n");
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_FULL_SCREEN_TYPES\n");

    mp_msg(MSGT_VO, MSGL_INFO, "    %-15s %s\n", "none",
           "don't set fullscreen window layer");
    mp_msg(MSGT_VO, MSGL_INFO, "    %-15s %s\n", "layer",
           "use _WIN_LAYER hint with default layer");
    mp_msg(MSGT_VO, MSGL_INFO, "    %-15s %s\n", "layer=<0..15>",
           "use _WIN_LAYER hint with a given layer number");
    mp_msg(MSGT_VO, MSGL_INFO, "    %-15s %s\n", "netwm",
           "force NETWM style");
    mp_msg(MSGT_VO, MSGL_INFO, "    %-15s %s\n", "above",
           "use _NETWM_STATE_ABOVE hint if available");
    mp_msg(MSGT_VO, MSGL_INFO, "    %-15s %s\n", "below",
           "use _NETWM_STATE_BELOW hint if available");
    mp_msg(MSGT_VO, MSGL_INFO, "    %-15s %s\n", "fullscreen",
           "use _NETWM_STATE_FULLSCREEN hint if available");
    mp_msg(MSGT_VO, MSGL_INFO, "    %-15s %s\n", "stays_on_top",
           "use _NETWM_STATE_STAYS_ON_TOP hint if available");
    mp_msg(MSGT_VO, MSGL_INFO, "    %-15s %s\n", "mwm_hack",
           "enable MWM hack");
    mp_msg(MSGT_VO, MSGL_INFO,
           "You can also negate the settings with simply putting '-' in the beginning");
    mp_msg(MSGT_VO, MSGL_INFO, "\n");
}

static void fstype_dump(struct vo_x11_state *x11)
{
    int fstype = x11->fs_type;
    if (fstype) {
        MP_VERBOSE(x11, "Current fstype setting honours");
        if (fstype & vo_wm_LAYER)
            MP_VERBOSE(x11, " LAYER");
        if (fstype & vo_wm_FULLSCREEN)
            MP_VERBOSE(x11, " FULLSCREEN");
        if (fstype & vo_wm_STAYS_ON_TOP)
            MP_VERBOSE(x11, " STAYS_ON_TOP");
        if (fstype & vo_wm_ABOVE)
            MP_VERBOSE(x11, " ABOVE");
        if (fstype & vo_wm_BELOW)
            MP_VERBOSE(x11, " BELOW");
        if (fstype & vo_wm_MWM)
            MP_VERBOSE(x11, " mwm_hack");
        MP_VERBOSE(x11, " X atoms\n");
    } else {
        MP_VERBOSE(x11, "Current fstype setting doesn't honour any X atoms\n");
    }
}

static int net_wm_support_state_test(struct vo_x11_state *x11, Atom atom)
{
#define NET_WM_STATE_TEST(x) { \
    if (atom == x11->XA_NET_WM_STATE_##x) { \
        MP_VERBOSE(x11, "Detected wm supports " #x " state.\n" ); \
        return vo_wm_##x; \
    } \
}

    NET_WM_STATE_TEST(FULLSCREEN);
    NET_WM_STATE_TEST(ABOVE);
    NET_WM_STATE_TEST(STAYS_ON_TOP);
    NET_WM_STATE_TEST(BELOW);
    return 0;
}

static int x11_get_property(struct vo_x11_state *x11, Atom type, Atom **args,
                            unsigned long *nitems)
{
    int format;
    unsigned long bytesafter;

    return Success ==
           XGetWindowProperty(x11->display, x11->rootwin, type, 0, 16384, False,
                              AnyPropertyType, &type, &format, nitems,
                              &bytesafter, (unsigned char **) args)
           && *nitems > 0;
}

static int vo_wm_detect(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    int i;
    int wm = 0;
    unsigned long nitems;
    Atom *args = NULL;

    if (vo->opts->WinID >= 0)
        return 0;

// -- supports layers
    if (x11_get_property(x11, x11->XA_WIN_PROTOCOLS, &args, &nitems)) {
        MP_VERBOSE(x11, "Detected wm supports layers.\n");
        int metacity_hack = 0;
        for (i = 0; i < nitems; i++) {
            if (args[i] == x11->XA_WIN_LAYER) {
                wm |= vo_wm_LAYER;
                metacity_hack |= 1;
            } else {
                /* metacity is the only window manager I know which reports
                 * supporting only the _WIN_LAYER hint in _WIN_PROTOCOLS.
                 * (what's more support for it is broken) */
                metacity_hack |= 2;
            }
        }
        XFree(args);
        if (wm && (metacity_hack == 1)) {
            // metacity claims to support layers, but it is not the truth :-)
            wm ^= vo_wm_LAYER;
            MP_VERBOSE(x11, "Using workaround for Metacity bugs.\n");
        }
    }
// --- netwm
    if (x11_get_property(x11, x11->XA_NET_SUPPORTED, &args, &nitems)) {
        MP_VERBOSE(x11, "Detected wm supports NetWM.\n");
        for (i = 0; i < nitems; i++)
            wm |= net_wm_support_state_test(vo->x11, args[i]);
        XFree(args);
    }

    if (wm == 0)
        MP_VERBOSE(x11, "Unknown wm type...\n");
    return wm;
}

#define XA_INIT(x) x11->XA ## x = XInternAtom(x11->display, # x, False)
static void init_atoms(struct vo_x11_state *x11)
{
    XA_INIT(_NET_SUPPORTED);
    XA_INIT(_NET_WM_STATE);
    XA_INIT(_NET_WM_STATE_FULLSCREEN);
    XA_INIT(_NET_WM_STATE_ABOVE);
    XA_INIT(_NET_WM_STATE_STAYS_ON_TOP);
    XA_INIT(_NET_WM_STATE_BELOW);
    XA_INIT(_NET_WM_PID);
    XA_INIT(_NET_WM_NAME);
    XA_INIT(_NET_WM_ICON_NAME);
    XA_INIT(_NET_WM_ICON);
    XA_INIT(_WIN_PROTOCOLS);
    XA_INIT(_WIN_LAYER);
    XA_INIT(_WIN_HINTS);
    XA_INIT(WM_PROTOCOLS);
    XA_INIT(WM_DELETE_WINDOW);
    XA_INIT(UTF8_STRING);
    char buf[50];
    sprintf(buf, "_NET_WM_CM_S%d", x11->screen);
    x11->XA_NET_WM_CM = XInternAtom(x11->display, buf, False);
}

static void vo_x11_update_screeninfo(struct vo *vo)
{
    struct mp_vo_opts *opts = vo->opts;
    struct vo_x11_state *x11 = vo->x11;
    bool all_screens = opts->fullscreen && opts->fsscreen_id == -2;
    vo->xinerama_x = vo->xinerama_y = 0;
    if (all_screens) {
        opts->screenwidth = x11->ws_width;
        opts->screenheight = x11->ws_height;
    }
#ifdef CONFIG_XINERAMA
    if (opts->screen_id >= -1 && XineramaIsActive(x11->display) &&
        !all_screens)
    {
        int screen = opts->fullscreen ? opts->fsscreen_id : opts->screen_id;
        XineramaScreenInfo *screens;
        int num_screens;

        screens = XineramaQueryScreens(x11->display, &num_screens);
        if (screen >= num_screens)
            screen = num_screens - 1;
        if (screen == -1) {
            int x = x11->win_x + x11->win_width / 2;
            int y = x11->win_y + x11->win_height / 2;
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
        opts->screenwidth = screens[screen].width;
        opts->screenheight = screens[screen].height;
        vo->xinerama_x = screens[screen].x_org;
        vo->xinerama_y = screens[screen].y_org;

        XFree(screens);
    }
#endif
    aspect_save_screenres(vo, opts->screenwidth, opts->screenheight);
}

int vo_x11_init(struct vo *vo)
{
    struct mp_vo_opts *opts = vo->opts;
    char *dispName;

    assert(!vo->x11);

    struct vo_x11_state *x11 = talloc_ptrtype(NULL, x11);
    *x11 = (struct vo_x11_state){
        .log = mp_log_new(x11, vo->log, "x11"),
        .olddecor = MWM_DECOR_ALL,
        .oldfuncs = MWM_FUNC_MOVE | MWM_FUNC_CLOSE | MWM_FUNC_MINIMIZE |
                    MWM_FUNC_MAXIMIZE | MWM_FUNC_RESIZE,
        .old_gravity = NorthWestGravity,
        .fs_layer = WIN_LAYER_ABOVE_DOCK,
    };
    vo->x11 = x11;

    XSetErrorHandler(x11_errorhandler);

    dispName = XDisplayName(NULL);

    MP_VERBOSE(x11, "X11 opening display: %s\n", dispName);

    x11->display = XOpenDisplay(dispName);
    if (!x11->display) {
        MP_MSG(x11, vo->probing ? MSGL_V : MSGL_ERR,
               "couldn't open the X11 display (%s)!\n", dispName);

        talloc_free(x11);
        vo->x11 = NULL;
        return 0;
    }
    x11->screen = DefaultScreen(x11->display);  // screen ID
    x11->rootwin = RootWindow(x11->display, x11->screen);   // root window ID

    if (!opts->native_keyrepeat) {
        Bool ok = False;
        XkbSetDetectableAutoRepeat(x11->display, True, &ok);
        x11->no_autorepeat = ok;
    }

    x11->xim = XOpenIM(x11->display, NULL, NULL, NULL);
    if (!x11->xim)
        MP_WARN(x11, "XOpenIM() failed. Unicode input will not work.\n");

    init_atoms(vo->x11);

    x11->ws_width = opts->screenwidth;
    x11->ws_height = opts->screenheight;

    if (!x11->ws_width)
        x11->ws_width = DisplayWidth(x11->display, x11->screen);
    if (!x11->ws_height)
        x11->ws_height = DisplayHeight(x11->display, x11->screen);

    opts->screenwidth = x11->ws_width;
    opts->screenheight = x11->ws_height;

    if (strncmp(dispName, "unix:", 5) == 0)
        dispName += 4;
    else if (strncmp(dispName, "localhost:", 10) == 0)
        dispName += 9;
    if (*dispName == ':' && atoi(dispName + 1) < 10)
        x11->display_is_local = 1;
    else
        x11->display_is_local = 0;
    MP_VERBOSE(x11, "X11 running at %dx%d (\"%s\" => %s display)\n",
               opts->screenwidth, opts->screenheight, dispName,
               x11->display_is_local ? "local" : "remote");

    x11->wm_type = vo_wm_detect(vo);

    x11->fs_type = vo_x11_get_fs_type(vo);

    fstype_dump(x11);

    vo->event_fd = ConnectionNumber(x11->display);

    return 1;
}

static const struct mp_keymap keymap[] = {
    // special keys
    {XK_Pause, MP_KEY_PAUSE}, {XK_Escape, MP_KEY_ESC},
    {XK_BackSpace, MP_KEY_BS}, {XK_Tab, MP_KEY_TAB}, {XK_Return, MP_KEY_ENTER},
    {XK_Menu, MP_KEY_MENU}, {XK_Print, MP_KEY_PRINT},

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

#ifdef XF86XK_AudioPause
    {XF86XK_MenuKB, MP_KEY_MENU},
    {XF86XK_AudioPlay, MP_KEY_PLAY}, {XF86XK_AudioPause, MP_KEY_PAUSE},
    {XF86XK_AudioStop, MP_KEY_STOP}, {XF86XK_AudioPrev, MP_KEY_PREV},
    {XF86XK_AudioNext, MP_KEY_NEXT}, {XF86XK_AudioMute, MP_KEY_MUTE},
    {XF86XK_AudioLowerVolume, MP_KEY_VOLUME_DOWN},
    {XF86XK_AudioRaiseVolume, MP_KEY_VOLUME_UP},
    {XF86XK_HomePage, MP_KEY_HOMEPAGE}, {XF86XK_WWW, MP_KEY_WWW},
    {XF86XK_Mail, MP_KEY_MAIL}, {XF86XK_Favorites, MP_KEY_FAVORITES},
    {XF86XK_Search, MP_KEY_SEARCH}, {XF86XK_Sleep, MP_KEY_SLEEP},
#endif

    {0, 0}
};

static int vo_x11_lookupkey(int key)
{
    static const char *passthrough_keys = " -+*/<>`~!@#$%^&()_{}:;\"\',.?\\|=[]";
    int mpkey = 0;
    if ((key >= 'a' && key <= 'z') ||
        (key >= 'A' && key <= 'Z') ||
        (key >= '0' && key <= '9') ||
        (key > 0 && key < 256 && strchr(passthrough_keys, key)))
        mpkey = key;

    if (!mpkey)
        mpkey = lookup_keymap_table(keymap, key);

    return mpkey;
}

static void vo_x11_decoration(struct vo *vo, int d)
{
    struct vo_x11_state *x11 = vo->x11;
    Atom mtype;
    int mformat;
    unsigned long mn, mb;
    Atom vo_MotifHints;
    MotifWmHints vo_MotifWmHints;

    if (!vo->opts->WinID)
        return;

    if (vo->opts->fsmode & 8) {
        XSetTransientForHint(x11->display, x11->window,
                             RootWindow(x11->display, x11->screen));
    }

    vo_MotifHints = XInternAtom(x11->display, "_MOTIF_WM_HINTS", 0);
    if (vo_MotifHints != None) {
        if (!d) {
            MotifWmHints *mhints = NULL;

            XGetWindowProperty(x11->display, x11->window,
                               vo_MotifHints, 0, 20, False,
                               vo_MotifHints, &mtype, &mformat, &mn,
                               &mb, (unsigned char **) &mhints);
            if (mhints) {
                if (mhints->flags & MWM_HINTS_DECORATIONS)
                    x11->olddecor = mhints->decorations;
                if (mhints->flags & MWM_HINTS_FUNCTIONS)
                    x11->oldfuncs = mhints->functions;
                XFree(mhints);
            }
        }

        memset(&vo_MotifWmHints, 0, sizeof(MotifWmHints));
        vo_MotifWmHints.flags =
            MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS;
        if (d) {
            vo_MotifWmHints.functions = x11->oldfuncs;
            d = x11->olddecor;
        }
        vo_MotifWmHints.decorations =
            d | ((vo->opts->fsmode & 2) ? MWM_DECOR_MENU : 0);
        XChangeProperty(x11->display, x11->window, vo_MotifHints,
                        vo_MotifHints, 32,
                        PropModeReplace,
                        (unsigned char *) &vo_MotifWmHints,
                        (vo->opts->fsmode & 4) ? 4 : 5);
    }
}

static void vo_x11_classhint(struct vo *vo, Window window, const char *name)
{
    struct mp_vo_opts *opts = vo->opts;
    struct vo_x11_state *x11 = vo->x11;
    XClassHint wmClass;
    pid_t pid = getpid();

    wmClass.res_name = opts->winname ? opts->winname : (char *)name;
    wmClass.res_class = "mpv";
    XSetClassHint(x11->display, window, &wmClass);
    XChangeProperty(x11->display, window, x11->XA_NET_WM_PID, XA_CARDINAL,
                    32, PropModeReplace, (unsigned char *) &pid, 1);
}

void vo_x11_uninit(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    assert(x11);

    mp_input_put_key(vo->input_ctx, MP_INPUT_RELEASE_ALL);

    saver_on(x11);
    if (x11->window != None)
        vo_set_cursor_hidden(vo, false);

    if (x11->f_gc != None)
        XFreeGC(vo->x11->display, x11->f_gc);
    if (x11->vo_gc != None)
        XFreeGC(vo->x11->display, x11->vo_gc);
    if (x11->window != None) {
        XClearWindow(x11->display, x11->window);
        XUnmapWindow(x11->display, x11->window);

        XSelectInput(x11->display, x11->window, StructureNotifyMask);
        XDestroyWindow(x11->display, x11->window);
        XEvent xev;
        do {
            XNextEvent(x11->display, &xev);
        } while (xev.type != DestroyNotify ||
                    xev.xdestroywindow.event != x11->window);
    }
    if (x11->xic)
        XDestroyIC(x11->xic);
    if (x11->colormap != None)
        XFreeColormap(vo->x11->display, x11->colormap);

    MP_VERBOSE(x11, "uninit ...\n");
    if (x11->xim)
        XCloseIM(x11->xim);
    XSetErrorHandler(NULL);
    XCloseDisplay(x11->display);

    talloc_free(x11);
    vo->x11 = NULL;
}

static void update_vo_size(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;

    if (x11->win_width != vo->dwidth || x11->win_height != vo->dheight) {
        vo->dwidth = x11->win_width;
        vo->dheight = x11->win_height;
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

int vo_x11_check_events(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    Display *display = vo->x11->display;
    XEvent Event;

    xscreensaver_heartbeat(vo->x11);

    if (vo->opts->WinID > 0)
        vo_x11_update_geometry(vo);
    while (XPending(display)) {
        XNextEvent(display, &Event);
//       printf("\rEvent.type=%X  \n",Event.type);
        switch (Event.type) {
        case Expose:
            x11->pending_vo_events |= VO_EVENT_EXPOSE;
            break;
        case ConfigureNotify:
            if (x11->window == None)
                break;
            vo_x11_update_geometry(vo);
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
                    mp_input_put_key(vo->input_ctx, mpkey | modifiers);
                } else if (status == XLookupChars || status == XLookupBoth) {
                    struct bstr t = { buf, len };
                    mp_input_put_key_utf8(vo->input_ctx, modifiers, t);
                }
            } else {
                XLookupString(&Event.xkey, buf, sizeof(buf), &keySym,
                              &x11->compose_status);
                int mpkey = vo_x11_lookupkey(keySym);
                if (mpkey)
                    mp_input_put_key(vo->input_ctx, mpkey | modifiers);
            }
            break;
        }
        // Releasing all keys in these situations is simpler and ensures no
        // keys can be get "stuck".
        case FocusOut:
        case KeyRelease:
        {
            if (x11->no_autorepeat)
                mp_input_put_key(vo->input_ctx, MP_INPUT_RELEASE_ALL);
            break;
        }
        case MotionNotify:
            vo_mouse_movement(vo, Event.xmotion.x, Event.xmotion.y);
            break;
        case EnterNotify:
            vo_mouse_movement(vo, Event.xcrossing.x, Event.xcrossing.y);
            break;
        case LeaveNotify:
            mp_input_put_key(vo->input_ctx, MP_KEY_MOUSE_LEAVE);
            break;
        case ButtonPress:
            mp_input_put_key(vo->input_ctx,
                             (MP_MOUSE_BTN0 + Event.xbutton.button - 1) |
                             get_mods(Event.xbutton.state) | MP_KEY_STATE_DOWN);
            break;
        case ButtonRelease:
            mp_input_put_key(vo->input_ctx,
                             (MP_MOUSE_BTN0 + Event.xbutton.button - 1) |
                             get_mods(Event.xbutton.state) | MP_KEY_STATE_UP);
            break;
        case PropertyNotify: {
            char *name = XGetAtomName(display, Event.xproperty.atom);
            if (!name)
                break;
            XFree(name);
            break;
        }
        case MapNotify:
            x11->vo_hint.win_gravity = x11->old_gravity;
            XSetWMNormalHints(display, x11->window, &x11->vo_hint);
            x11->fs_flip = 0;
            break;
        case DestroyNotify:
            MP_WARN(x11, "Our window was destroyed, exiting\n");
            mp_input_put_key(vo->input_ctx, MP_KEY_CLOSE_WIN);
            break;
        case ClientMessage:
            if (Event.xclient.message_type == x11->XAWM_PROTOCOLS &&
                Event.xclient.data.l[0] == x11->XAWM_DELETE_WINDOW)
                mp_input_put_key(vo->input_ctx, MP_KEY_CLOSE_WIN);
            break;
        default:
            if (Event.type == x11->ShmCompletionEvent) {
                if (x11->ShmCompletionWaitCount > 0)
                    x11->ShmCompletionWaitCount--;
            }
            break;
        }
    }

    update_vo_size(vo);
    if (vo->opts->WinID >= 0 && (x11->pending_vo_events & VO_EVENT_RESIZE)) {
        int x = x11->win_x, y = x11->win_y;
        unsigned int w = x11->win_width, h = x11->win_height;
        XMoveResizeWindow(x11->display, x11->window, x, y, w, h);
    }
    int ret = x11->pending_vo_events;
    x11->pending_vo_events = 0;
    return ret;
}

static void vo_x11_sizehint(struct vo *vo, int x, int y, int width, int height,
                            bool override_pos)
{
    struct mp_vo_opts *opts = vo->opts;
    struct vo_x11_state *x11 = vo->x11;

    bool force_pos = opts->geometry.xy_valid ||     // explicitly forced by user
                     opts->force_window_position || // resize -> reset position
                     opts->screen_id >= 0 ||        // force onto screen area
                     opts->WinID >= 0 ||            // force to fill parent
                     override_pos;                  // for fullscreen and such

    x11->vo_hint.flags = 0;
    if (opts->keepaspect) {
        x11->vo_hint.flags |= PAspect;
        x11->vo_hint.min_aspect.x = width;
        x11->vo_hint.min_aspect.y = height;
        x11->vo_hint.max_aspect.x = width;
        x11->vo_hint.max_aspect.y = height;
    }

    x11->vo_hint.flags |= PSize | (force_pos ? PPosition : 0);
    x11->vo_hint.x = x;
    x11->vo_hint.y = y;
    x11->vo_hint.width = width;
    x11->vo_hint.height = height;
    x11->vo_hint.max_width = 0;
    x11->vo_hint.max_height = 0;

    // Set minimum height/width to 4 to avoid off-by-one errors.
    x11->vo_hint.flags |= PMinSize;
    x11->vo_hint.min_width = x11->vo_hint.min_height = 4;

    // Set the base size. A window manager might display the window
    // size to the user relative to this.
    // Setting these to width/height might be nice, but e.g. fluxbox can't handle it.
    x11->vo_hint.flags |= PBaseSize;
    x11->vo_hint.base_width = 0 /*width*/;
    x11->vo_hint.base_height = 0 /*height*/;

    x11->vo_hint.flags |= PWinGravity;
    x11->vo_hint.win_gravity = StaticGravity;
    XSetWMNormalHints(x11->display, x11->window, &x11->vo_hint);
}

static void vo_x11_move_resize(struct vo *vo, bool move, bool resize,
                               int x, int y, int w, int h)
{
    XWindowChanges req = {.x = x, .y = y, .width = w, .height = h};
    unsigned mask = (move ? CWX | CWY : 0) | (resize ? CWWidth | CWHeight : 0);
    if (mask)
        XConfigureWindow(vo->x11->display, vo->x11->window, mask, &req);
    vo_x11_sizehint(vo, x, y, w, h, false);
}

static int vo_x11_get_gnome_layer(struct vo_x11_state *x11, Window win)
{
    Atom type;
    int format;
    unsigned long nitems;
    unsigned long bytesafter;
    unsigned short *args = NULL;

    if (XGetWindowProperty(x11->display, win, x11->XA_WIN_LAYER, 0, 16384,
                           False, AnyPropertyType, &type, &format, &nitems,
                           &bytesafter,
                           (unsigned char **) &args) == Success
        && nitems > 0 && args)
    {
        MP_VERBOSE(x11, "original window layer is %d.\n", *args);
        return *args;
    }
    return WIN_LAYER_NORMAL;
}

// set a X text property that expects a UTF8_STRING type
static void vo_x11_set_property_utf8(struct vo *vo, Atom name, const char *t)
{
    struct vo_x11_state *x11 = vo->x11;

    XChangeProperty(x11->display, x11->window, name, x11->XAUTF8_STRING, 8,
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

    if (!x11->window)
        return;

    const char *title = vo_get_window_title(vo);
    vo_x11_set_property_string(vo, XA_WM_NAME, title);
    vo_x11_set_property_string(vo, XA_WM_ICON_NAME, title);
    vo_x11_set_property_utf8(vo, x11->XA_NET_WM_NAME, title);
    vo_x11_set_property_utf8(vo, x11->XA_NET_WM_ICON_NAME, title);
}

#if CONFIG_ZLIB
static bstr decompress_gz(bstr in)
{
    bstr res = {0};
    z_stream zstream;
    uint8_t *dest;
    size_t size = in.len;
    int result;

    zstream.zalloc = (alloc_func) 0;
    zstream.zfree = (free_func) 0;
    zstream.opaque = (voidpf) 0;
    // 32 for gzip header, 15 for max. window bits
    if (inflateInit2(&zstream, 32 + 15) != Z_OK)
        goto error;
    zstream.next_in = (Bytef *) in.start;
    zstream.avail_in = size;

    dest = NULL;
    zstream.avail_out = size;
    do {
        size += 4000;
        dest = talloc_realloc_size(NULL, dest, size);
        zstream.next_out = (Bytef *) (dest + zstream.total_out);
        result = inflate(&zstream, Z_NO_FLUSH);
        if (result != Z_OK && result != Z_STREAM_END) {
            talloc_free(dest);
            dest = NULL;
            inflateEnd(&zstream);
            goto error;
        }
        zstream.avail_out += 4000;
    } while (zstream.avail_out == 4000 && zstream.avail_in != 0
             && result != Z_STREAM_END);

    size = zstream.total_out;
    inflateEnd(&zstream);

    res.start = dest;
    res.len = size;
error:
    return res;
}
#else
static bstr decompress_gz(bstr in)
{
    return (bstr){0};
}
#endif

#define MAX_ICONS 10

static void vo_x11_set_wm_icon(struct vo_x11_state *x11)
{
    int num_icons = 0;
    void *icon_data[MAX_ICONS];
    int icon_w[MAX_ICONS], icon_h[MAX_ICONS];

    bstr uncompressed = decompress_gz((bstr){(char *)x11_icon, sizeof(x11_icon)});
    bstr data = uncompressed;
    while (data.len && num_icons < MAX_ICONS) {
        bstr line = bstr_getline(data, &data);
        if (bstr_eatstart0(&line, "icon: ")) {
            int w, h;
            if (bstr_sscanf(line, "%d %d", &w, &h) == 2) {
                int size = w * h * 4;
                icon_w[num_icons] = w;
                icon_h[num_icons] = h;
                icon_data[num_icons] = data.start;
                num_icons++;
                data = bstr_cut(data, size);
            }
        }
    }

    int icon_size = 0;
    for (int n = 0; n < num_icons; n++)
        icon_size += 2 + icon_w[n] * icon_h[n];
    long *icon = talloc_array(NULL, long, icon_size);
    long *cur = icon;
    for (int n = 0; n < num_icons; n++) {
        *cur++ = icon_w[n];
        *cur++ = icon_h[n];
        uint32_t *src = icon_data[n];
        for (int i = 0; i < icon_h[n] * icon_w[n]; i++)
            *cur++ = src[i];
    }

    XChangeProperty(x11->display, x11->window, x11->XA_NET_WM_ICON,
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)icon, icon_size);
    talloc_free(icon);
    talloc_free(uncompressed.start);
}

static void find_default_visual(struct vo_x11_state *x11, XVisualInfo *vis)
{
    Display *display = x11->display;
    XWindowAttributes attribs;
    XGetWindowAttributes(display, DefaultRootWindow(display), &attribs);
    XMatchVisualInfo(display, x11->screen, attribs.depth, TrueColor, vis);
}

static void vo_x11_create_window(struct vo *vo, XVisualInfo *vis, int x, int y,
                                 unsigned int w, unsigned int h)
{
    struct vo_x11_state *x11 = vo->x11;

    assert(x11->window == None);
    assert(!x11->xic);

    XVisualInfo vinfo_storage;
    if (!vis) {
        vis = &vinfo_storage;
        find_default_visual(x11, vis);
    }

    vo_x11_create_colormap(x11, vis);
    unsigned long xswamask = CWBorderPixel | CWColormap;
    XSetWindowAttributes xswa = {
        .border_pixel = 0,
        .colormap = x11->colormap,
    };

    Window parent = vo->opts->WinID >= 0 ? vo->opts->WinID : x11->rootwin;

    x11->window =
        XCreateWindow(x11->display, parent, x, y, w, h, 0, vis->depth,
                      CopyFromParent, vis->visual, xswamask, &xswa);
    XSetWMProtocols(x11->display, x11->window, &x11->XAWM_DELETE_WINDOW, 1);
    x11->f_gc = XCreateGC(x11->display, x11->window, 0, 0);
    x11->vo_gc = XCreateGC(x11->display, x11->window, 0, NULL);
    XSetForeground(x11->display, x11->f_gc, 0);

    if (x11->mouse_cursor_hidden) {
        x11->mouse_cursor_hidden = false;
        vo_set_cursor_hidden(vo, true);
    }
    if (x11->xim) {
        x11->xic = XCreateIC(x11->xim,
                             XNInputStyle, XIMPreeditNone | XIMStatusNone,
                             XNClientWindow, x11->window,
                             XNFocusWindow, x11->window,
                             NULL);
    }

    vo_x11_set_wm_icon(x11);
    vo_x11_update_window_title(vo);
}

static void vo_x11_map_window(struct vo *vo, int x, int y, int w, int h)
{
    struct vo_x11_state *x11 = vo->x11;

    x11->window_hidden = false;
    vo_x11_move_resize(vo, true, true, x, y, w, h);
    if (!vo->opts->border)
        vo_x11_decoration(vo, 0);
    // map window
    vo_x11_selectinput_witherr(vo, x11->display, x11->window,
                               StructureNotifyMask | ExposureMask |
                               KeyPressMask | KeyReleaseMask |
                               ButtonPressMask | ButtonReleaseMask |
                               PointerMotionMask | EnterWindowMask |
                               LeaveWindowMask);
    XMapWindow(x11->display, x11->window);
    vo_x11_clearwindow(vo, x11->window);
}

/* Create and setup a window suitable for display
 * vis: Visual to use for creating the window (NULL for default)
 * x, y: position of window (might be ignored)
 * width, height: size of window
 * flags: flags for window creation (VOFLAG_*)
 * classname: name to use for the X11 classhint
 *
 * If the window already exists, it just moves and resizes it.
 */
void vo_x11_config_vo_window(struct vo *vo, XVisualInfo *vis, int x, int y,
                             unsigned int width, unsigned int height, int flags,
                             const char *classname)
{
    struct mp_vo_opts *opts = vo->opts;
    struct vo_x11_state *x11 = vo->x11;

    if (opts->WinID >= 0) {
        XSelectInput(x11->display, opts->WinID, StructureNotifyMask);
        vo_x11_update_geometry(vo);
        x = x11->win_x; y = x11->win_y;
        width = x11->win_width; height = x11->win_height;
    }
    if (x11->window == None) {
        vo_x11_create_window(vo, vis, x, y, width, height);
        vo_x11_classhint(vo, x11->window, classname);
        x11->window_hidden = true;
    }

    if (flags & VOFLAG_HIDDEN)
        return;

    bool reset_size = !(x11->old_dwidth == width && x11->old_dheight == height);
    if (x11->window_hidden) {
        x11->nofs_x = x;
        x11->nofs_y = y;
        reset_size = true;
    }

    x11->old_dwidth = width;
    x11->old_dheight = height;

    if (reset_size) {
        x11->nofs_width = width;
        x11->nofs_height = height;
    }

    if (x11->window_hidden) {
        vo_x11_map_window(vo, x, y, width, height);
    } else if (reset_size) {
        bool reset_pos = opts->force_window_position;
        if (reset_pos) {
            x11->nofs_x = x;
            x11->nofs_y = y;
        }
        if (opts->fullscreen) {
            x11->size_changed_during_fs = true;
            x11->pos_changed_during_fs = reset_pos;
            vo_x11_sizehint(vo, x, y, width, height, false);
        } else {
            vo_x11_move_resize(vo, reset_pos, true, x, y, width, height);
        }
    }

    if (opts->ontop)
        vo_x11_setlayer(vo, x11->window, opts->ontop);

    vo_x11_fullscreen(vo);

    XSync(x11->display, False);

    vo_x11_update_geometry(vo);
    update_vo_size(vo);
    x11->pending_vo_events &= ~VO_EVENT_RESIZE; // implicitly done by the VO
}

static void fill_rect(struct vo *vo, GC gc, int x0, int y0, int x1, int y1)
{
    struct vo_x11_state *x11 = vo->x11;

    x0 = FFMAX(x0, 0);
    y0 = FFMAX(y0, 0);
    x1 = FFMIN(x1, x11->win_width);
    y1 = FFMIN(y1, x11->win_height);

    if (x11->window && x1 > x0 && y1 > y0)
        XFillRectangle(x11->display, x11->window, gc, x0, y0, x1 - x0, y1 - y0);
}

// Clear everything outside of rc with the background color
void vo_x11_clear_background(struct vo *vo, const struct mp_rect *rc)
{
    struct vo_x11_state *x11 = vo->x11;
    GC gc = x11->f_gc;

    int w = x11->win_width;
    int h = x11->win_height;

    fill_rect(vo, gc, 0,      0,      w,      rc->y0); // top
    fill_rect(vo, gc, 0,      rc->y1, w,      h);      // bottom
    fill_rect(vo, gc, 0,      rc->y0, rc->x0, rc->y1); // left
    fill_rect(vo, gc, rc->x1, rc->y0, w,      rc->y1); // right

    XFlush(x11->display);
}

void vo_x11_clearwindow(struct vo *vo, Window vo_window)
{
    struct vo_x11_state *x11 = vo->x11;
    struct mp_vo_opts *opts = vo->opts;
    if (x11->f_gc == None)
        return;
    XFillRectangle(x11->display, vo_window, x11->f_gc, 0, 0,
                   opts->screenwidth, opts->screenheight);
    XFlush(x11->display);
}


static void vo_x11_setlayer(struct vo *vo, Window vo_window, int layer)
{
    struct vo_x11_state *x11 = vo->x11;
    if (vo->opts->WinID >= 0)
        return;

    if (x11->fs_type & vo_wm_LAYER) {
        XClientMessageEvent xev;

        if (!x11->orig_layer)
            x11->orig_layer = vo_x11_get_gnome_layer(x11, vo_window);

        memset(&xev, 0, sizeof(xev));
        xev.type = ClientMessage;
        xev.display = x11->display;
        xev.window = vo_window;
        xev.message_type = x11->XA_WIN_LAYER;
        xev.format = 32;
        // if not fullscreen, stay on default layer
        xev.data.l[0] = layer ? x11->fs_layer : x11->orig_layer;
        xev.data.l[1] = CurrentTime;
        MP_VERBOSE(x11, "Layered style stay on top (layer %ld).\n",
                   xev.data.l[0]);
        XSendEvent(x11->display, x11->rootwin, False, SubstructureNotifyMask,
                   (XEvent *) &xev);
    } else if (x11->fs_type & vo_wm_NETWM) {
        XClientMessageEvent xev;
        char *state;

        memset(&xev, 0, sizeof(xev));
        xev.type = ClientMessage;
        xev.message_type = x11->XA_NET_WM_STATE;
        xev.display = x11->display;
        xev.window = vo_window;
        xev.format = 32;
        xev.data.l[0] = layer;

        if (x11->fs_type & vo_wm_STAYS_ON_TOP) {
            xev.data.l[1] = x11->XA_NET_WM_STATE_STAYS_ON_TOP;
        } else if (x11->fs_type & vo_wm_ABOVE) {
            xev.data.l[1] = x11->XA_NET_WM_STATE_ABOVE;
        } else if (x11->fs_type & vo_wm_FULLSCREEN) {
            xev.data.l[1] = x11->XA_NET_WM_STATE_FULLSCREEN;
        } else if (x11->fs_type & vo_wm_BELOW) {
            // This is not fallback. We can safely assume that the situation
            // where only NETWM_STATE_BELOW is supported doesn't exist.
            xev.data.l[1] = x11->XA_NET_WM_STATE_BELOW;
        }

        XSendEvent(x11->display, x11->rootwin, False, SubstructureRedirectMask,
                   (XEvent *) &xev);
        state = XGetAtomName(x11->display, xev.data.l[1]);
        MP_VERBOSE(x11, "NET style stay on top (layer %d). Using state %s.\n",
                   layer, state);
        XFree(state);
    }
}

static int vo_x11_get_fs_type(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    int type = x11->wm_type;
    char **fstype_list = vo->opts->fstype_list;
    int i;

    if (fstype_list) {
        for (i = 0; fstype_list[i]; i++) {
            int neg = 0;
            char *arg = fstype_list[i];

            if (fstype_list[i][0] == '-') {
                neg = 1;
                arg = fstype_list[i] + 1;
            }

            if (!strncmp(arg, "layer", 5)) {
                if (!neg && (arg[5] == '=')) {
                    char *endptr = NULL;
                    int layer = strtol(fstype_list[i] + 6, &endptr, 10);

                    if (endptr && *endptr == '\0' && layer >= 0
                        && layer <= 15)
                        x11->fs_layer = layer;
                }
                if (neg)
                    type &= ~vo_wm_LAYER;
                else
                    type |= vo_wm_LAYER;
            } else if (!strcmp(arg, "above")) {
                if (neg)
                    type &= ~vo_wm_ABOVE;
                else
                    type |= vo_wm_ABOVE;
            } else if (!strcmp(arg, "fullscreen")) {
                if (neg)
                    type &= ~vo_wm_FULLSCREEN;
                else
                    type |= vo_wm_FULLSCREEN;
            } else if (!strcmp(arg, "stays_on_top")) {
                if (neg)
                    type &= ~vo_wm_STAYS_ON_TOP;
                else
                    type |= vo_wm_STAYS_ON_TOP;
            } else if (!strcmp(arg, "below")) {
                if (neg)
                    type &= ~vo_wm_BELOW;
                else
                    type |= vo_wm_BELOW;
            } else if (!strcmp(arg, "netwm")) {
                if (neg)
                    type &= ~vo_wm_NETWM;
                else
                    type |= vo_wm_NETWM;
            } else if (!strcmp(arg, "mwm_hack")) {
                if (neg)
                    type &= ~vo_wm_MWM;
                else
                    type |= vo_wm_MWM;
            } else if (!strcmp(arg, "none"))
                type = 0;  // clear; keep parsing
        }
    }

    return type;
}

// update x11->win_x, x11->win_y, x11->win_width and x11->win_height with current values of vo->x11->window
static void vo_x11_update_geometry(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    unsigned w, h, dummy_uint;
    int dummy_int;
    Window dummy_win;
    Window win = vo->opts->WinID >= 0 ? vo->opts->WinID : x11->window;
    XGetGeometry(x11->display, win, &dummy_win, &dummy_int, &dummy_int,
                 &w, &h, &dummy_int, &dummy_uint);
    if (w <= INT_MAX && h <= INT_MAX) {
        x11->win_width = w;
        x11->win_height = h;
    }
    if (vo->opts->WinID >= 0) {
        x11->win_x = 0;
        x11->win_y = 0;
    } else {
        XTranslateCoordinates(x11->display, x11->window, x11->rootwin, 0, 0,
                              &x11->win_x, &x11->win_y, &dummy_win);
    }
}

static void vo_x11_fullscreen(struct vo *vo)
{
    struct mp_vo_opts *opts = vo->opts;
    struct vo_x11_state *x11 = vo->x11;
    int x, y, w, h;

    if (opts->fullscreen == x11->fs)
        return;
    if (opts->WinID >= 0) {
        x11->fs = opts->fullscreen;
        return;
    }
    if (x11->fs_flip)
        return;

    if (!opts->fullscreen) {
        // fs->win
        opts->fullscreen = x11->fs = 0;

        x = x11->nofs_x;
        y = x11->nofs_y;
        w = x11->nofs_width;
        h = x11->nofs_height;

        vo_x11_ewmh_fullscreen(x11, _NET_WM_STATE_REMOVE);   // removes fullscreen state if wm supports EWMH
        if ((x11->fs_type & vo_wm_FULLSCREEN) && opts->fsscreen_id != -1) {
            x11->size_changed_during_fs = true;
            x11->pos_changed_during_fs = true;
        }

        if (x11->fs_type & vo_wm_FULLSCREEN) {
            vo_x11_move_resize(vo, x11->pos_changed_during_fs,
                               x11->size_changed_during_fs, x, y, w, h);
        }
    } else {
        // win->fs
        opts->fullscreen = x11->fs = 1;

        vo_x11_update_geometry(vo);
        x11->nofs_x = x11->win_x;
        x11->nofs_y = x11->win_y;
        x11->nofs_width = x11->win_width;
        x11->nofs_height = x11->win_height;

        vo_x11_update_screeninfo(vo);

        x = vo->xinerama_x;
        y = vo->xinerama_y;
        w = opts->screenwidth;
        h = opts->screenheight;

        if ((x11->fs_type & vo_wm_FULLSCREEN) && opts->fsscreen_id != -1) {
            // The EWMH fullscreen hint always works on the current screen, so
            // change the current screen forcibly.
            // This was observed to work under IceWM, but not Unity/Compiz and
            // awesome (but --screen etc. doesn't really work on these either).
            XMoveResizeWindow(x11->display, x11->window, x, y, w, h);
        }

        vo_x11_ewmh_fullscreen(x11, _NET_WM_STATE_ADD);      // sends fullscreen state to be added if wm supports EWMH
    }
    {
        long dummy;

        XGetWMNormalHints(x11->display, x11->window, &x11->vo_hint, &dummy);
        if (!(x11->vo_hint.flags & PWinGravity))
            x11->old_gravity = NorthWestGravity;
        else
            x11->old_gravity = x11->vo_hint.win_gravity;
    }
    if (x11->fs_type & vo_wm_MWM) {
        XUnmapWindow(x11->display, x11->window);      // required for MWM
        XWithdrawWindow(x11->display, x11->window, x11->screen);
        x11->fs_flip = 1;
    }

    if (!(x11->fs_type & vo_wm_FULLSCREEN)) {  // not needed with EWMH fs
        vo_x11_decoration(vo, opts->border && !x11->fs);
        vo_x11_sizehint(vo, x, y, w, h, true);
        vo_x11_setlayer(vo, x11->window, x11->fs);


        XMoveResizeWindow(x11->display, x11->window, x, y, w, h);
    }
    /* some WMs lose ontop after fullscreen */
    if ((!(x11->fs)) & opts->ontop)
        vo_x11_setlayer(vo, x11->window, opts->ontop);

    XMapRaised(x11->display, x11->window);
    if (!(x11->fs_type & vo_wm_FULLSCREEN))    // some WMs change window pos on map
        XMoveResizeWindow(x11->display, x11->window, x, y, w, h);
    XRaiseWindow(x11->display, x11->window);
    XFlush(x11->display);

    x11->size_changed_during_fs = false;
    x11->pos_changed_during_fs = false;
}

static void vo_x11_ontop(struct vo *vo)
{
    struct mp_vo_opts *opts = vo->opts;
    opts->ontop = !opts->ontop;

    vo_x11_setlayer(vo, vo->x11->window, opts->ontop);
}

static void vo_x11_border(struct vo *vo)
{
    vo->opts->border = !vo->opts->border;
    vo_x11_decoration(vo, vo->opts->border && !vo->x11->fs);
}

int vo_x11_control(struct vo *vo, int *events, int request, void *arg)
{
    struct vo_x11_state *x11 = vo->x11;
    switch (request) {
    case VOCTRL_CHECK_EVENTS:
        *events |= vo_x11_check_events(vo);
        return VO_TRUE;
    case VOCTRL_FULLSCREEN:
        vo_x11_fullscreen(vo);
        *events |= VO_EVENT_RESIZE;
        return VO_TRUE;
    case VOCTRL_ONTOP:
        vo_x11_ontop(vo);
        return VO_TRUE;
    case VOCTRL_BORDER:
        vo_x11_border(vo);
        *events |= VO_EVENT_RESIZE;
        return VO_TRUE;
    case VOCTRL_UPDATE_SCREENINFO:
        vo_x11_update_screeninfo(vo);
        return VO_TRUE;
    case VOCTRL_SET_CURSOR_VISIBILITY:
        vo_set_cursor_hidden(vo, !(*(bool *)arg));
        return VO_TRUE;
    case VOCTRL_KILL_SCREENSAVER:
        saver_off(x11);
        return VO_TRUE;
    case VOCTRL_RESTORE_SCREENSAVER:
        saver_on(x11);
        return VO_TRUE;
    case VOCTRL_UPDATE_WINDOW_TITLE:
        vo_x11_update_window_title(vo);
        return VO_TRUE;
    }
    return VO_NOTIMPL;
}

static void xscreensaver_heartbeat(struct vo_x11_state *x11)
{
    double time = mp_time_sec();

    if (x11->display && x11->screensaver_off &&
        (time - x11->screensaver_time_last) > 30)
    {
        x11->screensaver_time_last = time;

        XResetScreenSaver(x11->display);
    }
}

static int xss_suspend(Display *mDisplay, Bool suspend)
{
#ifndef CONFIG_XSS
    return 0;
#else
    int event, error, major, minor;
    if (XScreenSaverQueryExtension(mDisplay, &event, &error) != True ||
        XScreenSaverQueryVersion(mDisplay, &major, &minor) != True)
        return 0;
    if (major < 1 || (major == 1 && minor < 1))
        return 0;
    XScreenSaverSuspend(mDisplay, suspend);
    return 1;
#endif
}

static void saver_on(struct vo_x11_state *x11)
{
    Display *mDisplay = x11->display;
    if (!x11->screensaver_off)
        return;
    x11->screensaver_off = 0;
    if (xss_suspend(mDisplay, False))
        return;
#ifdef CONFIG_XDPMS
    if (x11->dpms_disabled) {
        int nothing;
        if (DPMSQueryExtension(mDisplay, &nothing, &nothing)) {
            if (!DPMSEnable(mDisplay)) {    // restoring power saving settings
                MP_WARN(x11, "DPMS not available?\n");
            } else {
                // DPMS does not seem to be enabled unless we call DPMSInfo
                BOOL onoff;
                CARD16 state;

                DPMSForceLevel(mDisplay, DPMSModeOn);
                DPMSInfo(mDisplay, &state, &onoff);
                if (onoff) {
                    MP_VERBOSE(x11, "Successfully enabled DPMS\n");
                } else {
                    MP_WARN(x11, "Could not enable DPMS\n");
                }
            }
        }
        x11->dpms_disabled = 0;
    }
#endif
}

static void saver_off(struct vo_x11_state *x11)
{
    Display *mDisplay = x11->display;
    int nothing;

    if (x11->screensaver_off)
        return;
    x11->screensaver_off = 1;
    if (xss_suspend(mDisplay, True))
        return;
#ifdef CONFIG_XDPMS
    if (DPMSQueryExtension(mDisplay, &nothing, &nothing)) {
        BOOL onoff;
        CARD16 state;

        DPMSInfo(mDisplay, &state, &onoff);
        if (onoff) {
            Status stat;

            MP_VERBOSE(x11, "Disabling DPMS\n");
            x11->dpms_disabled = 1;
            stat = DPMSDisable(mDisplay);       // monitor powersave off
            MP_VERBOSE(x11, "DPMSDisable stat: %d\n", stat);
        }
    }
#endif
}

static void vo_x11_selectinput_witherr(struct vo *vo,
                                       Display *display,
                                       Window w,
                                       long event_mask)
{
    if (!vo->opts->enable_mouse_movements)
        event_mask &= ~(ButtonPressMask | ButtonReleaseMask);

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

#ifdef CONFIG_XF86VM
double vo_x11_vm_get_fps(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    int clock;
    XF86VidModeModeLine modeline;
    if (!XF86VidModeGetModeLine(x11->display, x11->screen, &clock, &modeline))
        return 0;
    if (modeline.privsize)
        XFree(modeline.private);
    return 1e3 * clock / modeline.htotal / modeline.vtotal;
}
#else /* CONFIG_XF86VM */
double vo_x11_vm_get_fps(struct vo *vo)
{
    return 0;
}
#endif


static void vo_x11_create_colormap(struct vo_x11_state *x11,
                                   XVisualInfo *vinfo)
{
    unsigned k, r, g, b, ru, gu, bu, m, rv, gv, bv, rvu, gvu, bvu;

    if (x11->colormap != None)
        return;

    if (vinfo->class != DirectColor) {
        x11->colormap = XCreateColormap(x11->display, x11->rootwin,
                                        vinfo->visual, AllocNone);
        return;
    }

    // DirectColor is requested by vo_x11 by default, for the equalizer

    x11->cm_size = vinfo->colormap_size;
    x11->red_mask = vinfo->red_mask;
    x11->green_mask = vinfo->green_mask;
    x11->blue_mask = vinfo->blue_mask;
    ru = (x11->red_mask & (x11->red_mask - 1)) ^ x11->red_mask;
    gu = (x11->green_mask & (x11->green_mask - 1)) ^ x11->green_mask;
    bu = (x11->blue_mask & (x11->blue_mask - 1)) ^ x11->blue_mask;
    rvu = 65536ull * ru / (x11->red_mask + ru);
    gvu = 65536ull * gu / (x11->green_mask + gu);
    bvu = 65536ull * bu / (x11->blue_mask + bu);
    r = g = b = 0;
    rv = gv = bv = 0;
    m = DoRed | DoGreen | DoBlue;
    for (k = 0; k < x11->cm_size; k++) {
        int t;

        x11->cols[k].pixel = r | g | b;
        x11->cols[k].red = rv;
        x11->cols[k].green = gv;
        x11->cols[k].blue = bv;
        x11->cols[k].flags = m;
        t = (r + ru) & x11->red_mask;
        if (t < r)
            m &= ~DoRed;
        r = t;
        t = (g + gu) & x11->green_mask;
        if (t < g)
            m &= ~DoGreen;
        g = t;
        t = (b + bu) & x11->blue_mask;
        if (t < b)
            m &= ~DoBlue;
        b = t;
        rv += rvu;
        gv += gvu;
        bv += bvu;
    }
    x11->colormap = XCreateColormap(x11->display, x11->rootwin, vinfo->visual,
                                    AllocAll);
    XStoreColors(x11->display, x11->colormap, x11->cols, x11->cm_size);
}

static int transform_color(float val,
                           float brightness, float contrast, float gamma)
{
    float s = pow(val, gamma);
    s = (s - 0.5) * contrast + 0.5;
    s += brightness;
    if (s < 0)
        s = 0;
    if (s > 1)
        s = 1;
    return (unsigned short) (s * 65535);
}

uint32_t vo_x11_set_equalizer(struct vo *vo, const char *name, int value)
{
    struct vo_x11_state *x11 = vo->x11;
    float gamma, brightness, contrast;
    float rf, gf, bf;
    int k;
    int red_mask = x11->red_mask;
    int green_mask = x11->green_mask;
    int blue_mask = x11->blue_mask;

    /*
     * IMPLEMENTME: consider using XF86VidModeSetGammaRamp in the case
     * of TrueColor-ed window but be careful:
     * Unlike the colormaps, which are private for the X client
     * who created them and thus automatically destroyed on client
     * disconnect, this gamma ramp is a system-wide (X-server-wide)
     * setting and _must_ be restored before the process exits.
     * Unforunately when the process crashes (or gets killed
     * for some reason) it is impossible to restore the setting,
     * and such behaviour could be rather annoying for the users.
     */
    if (x11->colormap == None)
        return VO_NOTAVAIL;

    if (!strcasecmp(name, "brightness"))
        x11->vo_brightness = value;
    else if (!strcasecmp(name, "contrast"))
        x11->vo_contrast = value;
    else if (!strcasecmp(name, "gamma"))
        x11->vo_gamma = value;
    else
        return VO_NOTIMPL;

    brightness = 0.01 * x11->vo_brightness;
    contrast = tan(0.0095 * (x11->vo_contrast + 100) * M_PI / 4);
    gamma = pow(2, -0.02 * x11->vo_gamma);

    rf = (float) ((red_mask & (red_mask - 1)) ^ red_mask) / red_mask;
    gf = (float) ((green_mask & (green_mask - 1)) ^ green_mask) /
         green_mask;
    bf = (float) ((blue_mask & (blue_mask - 1)) ^ blue_mask) / blue_mask;

    /* now recalculate the colormap using the newly set value */
    for (k = 0; k < x11->cm_size; k++) {
        x11->cols[k].red   = transform_color(rf * k, brightness, contrast, gamma);
        x11->cols[k].green = transform_color(gf * k, brightness, contrast, gamma);
        x11->cols[k].blue  = transform_color(bf * k, brightness, contrast, gamma);
    }

    XStoreColors(vo->x11->display, x11->colormap, x11->cols, x11->cm_size);
    XFlush(vo->x11->display);
    return VO_TRUE;
}

uint32_t vo_x11_get_equalizer(struct vo *vo, const char *name, int *value)
{
    struct vo_x11_state *x11 = vo->x11;
    if (x11->colormap == None)
        return VO_NOTAVAIL;
    if (!strcasecmp(name, "brightness"))
        *value = x11->vo_brightness;
    else if (!strcasecmp(name, "contrast"))
        *value = x11->vo_contrast;
    else if (!strcasecmp(name, "gamma"))
        *value = x11->vo_gamma;
    else
        return VO_NOTIMPL;
    return VO_TRUE;
}

bool vo_x11_screen_is_composited(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    return XGetSelectionOwner(x11->display, x11->XA_NET_WM_CM) != None;
}
