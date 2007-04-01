
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>

#include "config.h"
#include "mp_msg.h"
#include "mp_fifo.h"
#include "x11_common.h"

#ifdef X11_FULLSCREEN

#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>

#include "video_out.h"
#include "aspect.h"
#include "geometry.h"
#include "help_mp.h"
#include "osdep/timer.h"

#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#ifdef HAVE_XDPMS
#include <X11/extensions/dpms.h>
#endif

#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#ifdef HAVE_XF86VM
#include <X11/extensions/xf86vmode.h>
#endif

#ifdef HAVE_XF86XK
#include <X11/XF86keysym.h>
#endif

#ifdef HAVE_XV
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

#include "subopt-helper.h"
#endif

#include "input/input.h"
#include "input/mouse.h"

#ifdef HAVE_NEW_GUI
#include "Gui/interface.h"
#include "mplayer.h"
#endif

#define WIN_LAYER_ONBOTTOM               2
#define WIN_LAYER_NORMAL                 4
#define WIN_LAYER_ONTOP                  6
#define WIN_LAYER_ABOVE_DOCK             10

extern int enable_mouse_movements;
int fs_layer = WIN_LAYER_ABOVE_DOCK;
static int orig_layer = 0;
static int old_gravity = NorthWestGravity;

int stop_xscreensaver = 0;

static int dpms_disabled = 0;
static int timeout_save = 0;
static int kdescreensaver_was_running = 0;

char *mDisplayName = NULL;
Display *mDisplay = NULL;
Window mRootWin;
int mScreen;
int mLocalDisplay;

/* output window id */
int vo_mouse_autohide = 0;
int vo_wm_type = 0;
int vo_fs_type = 0; // needs to be accessible for GUI X11 code
static int vo_fs_flip = 0;
char **vo_fstype_list;

/* 1 means that the WM is metacity (broken as hell) */
int metacity_hack = 0;

static Atom XA_NET_SUPPORTED;
static Atom XA_NET_WM_STATE;
static Atom XA_NET_WM_STATE_FULLSCREEN;
static Atom XA_NET_WM_STATE_ABOVE;
static Atom XA_NET_WM_STATE_STAYS_ON_TOP;
static Atom XA_NET_WM_STATE_BELOW;
static Atom XA_NET_WM_PID;
static Atom XA_WIN_PROTOCOLS;
static Atom XA_WIN_LAYER;
static Atom XA_WIN_HINTS;
static Atom XA_BLACKBOX_PID;
static Atom XAWM_PROTOCOLS;
static Atom XAWM_DELETE_WINDOW;

#define XA_INIT(x) XA##x = XInternAtom(mDisplay, #x, False)

static int vo_old_x = 0;
static int vo_old_y = 0;
static int vo_old_width = 0;
static int vo_old_height = 0;

#ifdef HAVE_XF86VM
XF86VidModeModeInfo **vidmodes = NULL;
XF86VidModeModeLine modeline;
#endif

static int vo_x11_get_fs_type(int supported);


/*
 * Sends the EWMH fullscreen state event.
 *
 * action: could be one of _NET_WM_STATE_REMOVE -- remove state
 *                         _NET_WM_STATE_ADD    -- add state
 *                         _NET_WM_STATE_TOGGLE -- toggle
 */
void vo_x11_ewmh_fullscreen(int action)
{
    assert(action == _NET_WM_STATE_REMOVE ||
           action == _NET_WM_STATE_ADD || action == _NET_WM_STATE_TOGGLE);

    if (vo_fs_type & vo_wm_FULLSCREEN)
    {
        XEvent xev;

        /* init X event structure for _NET_WM_FULLSCREEN client message */
        xev.xclient.type = ClientMessage;
        xev.xclient.serial = 0;
        xev.xclient.send_event = True;
        xev.xclient.message_type = XInternAtom(mDisplay,
                                               "_NET_WM_STATE", False);
        xev.xclient.window = vo_window;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = action;
        xev.xclient.data.l[1] = XInternAtom(mDisplay,
                                            "_NET_WM_STATE_FULLSCREEN",
                                            False);
        xev.xclient.data.l[2] = 0;
        xev.xclient.data.l[3] = 0;
        xev.xclient.data.l[4] = 0;

        /* finally send that damn thing */
        if (!XSendEvent(mDisplay, DefaultRootWindow(mDisplay), False,
                        SubstructureRedirectMask | SubstructureNotifyMask,
                        &xev))
        {
            mp_msg(MSGT_VO, MSGL_ERR, MSGTR_EwmhFullscreenStateFailed);
        }
    }
}

void vo_hidecursor(Display * disp, Window win)
{
    Cursor no_ptr;
    Pixmap bm_no;
    XColor black, dummy;
    Colormap colormap;
    static char bm_no_data[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    if (WinID == 0)
        return;                 // do not hide if playing on the root window

    colormap = DefaultColormap(disp, DefaultScreen(disp));
    if ( !XAllocNamedColor(disp, colormap, "black", &black, &dummy) )
    {
      return; // color alloc failed, give up
    }
    bm_no = XCreateBitmapFromData(disp, win, bm_no_data, 8, 8);
    no_ptr = XCreatePixmapCursor(disp, bm_no, bm_no, &black, &black, 0, 0);
    XDefineCursor(disp, win, no_ptr);
    XFreeCursor(disp, no_ptr);
    if (bm_no != None)
        XFreePixmap(disp, bm_no);
    XFreeColors(disp,colormap,&black.pixel,1,0);
}

void vo_showcursor(Display * disp, Window win)
{
    if (WinID == 0)
        return;
    XDefineCursor(disp, win, 0);
}

static int x11_errorhandler(Display * display, XErrorEvent * event)
{
#define MSGLEN 60
    char msg[MSGLEN];

    XGetErrorText(display, event->error_code, (char *) &msg, MSGLEN);

    mp_msg(MSGT_VO, MSGL_ERR, "X11 error: %s\n", msg);

    mp_msg(MSGT_VO, MSGL_V,
           "Type: %x, display: %p, resourceid: %lx, serial: %lx\n",
           event->type, event->display, event->resourceid, event->serial);
    mp_msg(MSGT_VO, MSGL_V,
           "Error code: %x, request code: %x, minor code: %x\n",
           event->error_code, event->request_code, event->minor_code);

//    abort();
    //exit_player("X11 error");
#undef MSGLEN
}

void fstype_help(void)
{
    mp_msg(MSGT_VO, MSGL_INFO, MSGTR_AvailableFsType);
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
           "use _NETWM_STATE_FULLSCREEN hint if availale");
    mp_msg(MSGT_VO, MSGL_INFO, "    %-15s %s\n", "stays_on_top",
           "use _NETWM_STATE_STAYS_ON_TOP hint if available");
    mp_msg(MSGT_VO, MSGL_INFO,
           "You can also negate the settings with simply putting '-' in the beginning");
    mp_msg(MSGT_VO, MSGL_INFO, "\n");
}

static void fstype_dump(int fstype)
{
    if (fstype)
    {
        mp_msg(MSGT_VO, MSGL_V, "[x11] Current fstype setting honours");
        if (fstype & vo_wm_LAYER)
            mp_msg(MSGT_VO, MSGL_V, " LAYER");
        if (fstype & vo_wm_FULLSCREEN)
            mp_msg(MSGT_VO, MSGL_V, " FULLSCREEN");
        if (fstype & vo_wm_STAYS_ON_TOP)
            mp_msg(MSGT_VO, MSGL_V, " STAYS_ON_TOP");
        if (fstype & vo_wm_ABOVE)
            mp_msg(MSGT_VO, MSGL_V, " ABOVE");
        if (fstype & vo_wm_BELOW)
            mp_msg(MSGT_VO, MSGL_V, " BELOW");
        mp_msg(MSGT_VO, MSGL_V, " X atoms\n");
    } else
        mp_msg(MSGT_VO, MSGL_V,
               "[x11] Current fstype setting doesn't honour any X atoms\n");
}

static int net_wm_support_state_test(Atom atom)
{
#define NET_WM_STATE_TEST(x) { if (atom == XA_NET_WM_STATE_##x) { mp_msg( MSGT_VO,MSGL_V, "[x11] Detected wm supports " #x " state.\n" ); return vo_wm_##x; } }

    NET_WM_STATE_TEST(FULLSCREEN);
    NET_WM_STATE_TEST(ABOVE);
    NET_WM_STATE_TEST(STAYS_ON_TOP);
    NET_WM_STATE_TEST(BELOW);
    return 0;
}

static int x11_get_property(Atom type, Atom ** args, unsigned long *nitems)
{
    int format;
    unsigned long bytesafter;

    return (Success ==
            XGetWindowProperty(mDisplay, mRootWin, type, 0, 16384, False,
                               AnyPropertyType, &type, &format, nitems,
                               &bytesafter, (unsigned char **) args)
            && *nitems > 0);
}

static int vo_wm_detect(void)
{
    int i;
    int wm = 0;
    unsigned long nitems;
    Atom *args = NULL;

    if (WinID >= 0)
        return 0;

// -- supports layers
    if (x11_get_property(XA_WIN_PROTOCOLS, &args, &nitems))
    {
        mp_msg(MSGT_VO, MSGL_V, "[x11] Detected wm supports layers.\n");
        for (i = 0; i < nitems; i++)
        {
            if (args[i] == XA_WIN_LAYER)
            {
                wm |= vo_wm_LAYER;
                metacity_hack |= 1;
            } else
                /* metacity is the only window manager I know which reports
                 * supporting only the _WIN_LAYER hint in _WIN_PROTOCOLS.
                 * (what's more support for it is broken) */
                metacity_hack |= 2;
        }
        XFree(args);
        if (wm && (metacity_hack == 1))
        {
            // metacity claims to support layers, but it is not the truth :-)
            wm ^= vo_wm_LAYER;
            mp_msg(MSGT_VO, MSGL_V,
                   "[x11] Using workaround for Metacity bugs.\n");
        }
    }
// --- netwm 
    if (x11_get_property(XA_NET_SUPPORTED, &args, &nitems))
    {
        mp_msg(MSGT_VO, MSGL_V, "[x11] Detected wm supports NetWM.\n");
        for (i = 0; i < nitems; i++)
            wm |= net_wm_support_state_test(args[i]);
        XFree(args);
#if 0
        // ugly hack for broken OpenBox _NET_WM_STATE_FULLSCREEN support
        // (in their implementation it only changes internal window state, nothing more!!!)
        if (wm & vo_wm_FULLSCREEN)
        {
            if (x11_get_property(XA_BLACKBOX_PID, &args, &nitems))
            {
                mp_msg(MSGT_VO, MSGL_V,
                       "[x11] Detected wm is a broken OpenBox.\n");
                wm ^= vo_wm_FULLSCREEN;
            }
            XFree(args);
        }
#endif
    }

    if (wm == 0)
        mp_msg(MSGT_VO, MSGL_V, "[x11] Unknown wm type...\n");
    return wm;
}

static void init_atoms(void)
{
    XA_INIT(_NET_SUPPORTED);
    XA_INIT(_NET_WM_STATE);
    XA_INIT(_NET_WM_STATE_FULLSCREEN);
    XA_INIT(_NET_WM_STATE_ABOVE);
    XA_INIT(_NET_WM_STATE_STAYS_ON_TOP);
    XA_INIT(_NET_WM_STATE_BELOW);
    XA_INIT(_NET_WM_PID);
    XA_INIT(_WIN_PROTOCOLS);
    XA_INIT(_WIN_LAYER);
    XA_INIT(_WIN_HINTS);
    XA_INIT(_BLACKBOX_PID);
    XA_INIT(WM_PROTOCOLS);
    XA_INIT(WM_DELETE_WINDOW);
}

void update_xinerama_info(void) {
    int screen = xinerama_screen;
    xinerama_x = xinerama_y = 0;
#ifdef HAVE_XINERAMA
    if (screen >= -1 && XineramaIsActive(mDisplay))
    {
        XineramaScreenInfo *screens;
        int num_screens;

        screens = XineramaQueryScreens(mDisplay, &num_screens);
        if (screen >= num_screens)
            screen = num_screens - 1;
        if (screen == -1) {
            int x = vo_dx + vo_dwidth / 2;
            int y = vo_dy + vo_dheight / 2;
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
        vo_screenwidth = screens[screen].width;
        vo_screenheight = screens[screen].height;
        xinerama_x = screens[screen].x_org;
        xinerama_y = screens[screen].y_org;

        XFree(screens);
    }
#endif
    aspect_save_screenres(vo_screenwidth, vo_screenheight);
}

int vo_init(void)
{
// int       mScreen;
    int depth, bpp;
    unsigned int mask;

// char    * DisplayName = ":0.0";
// Display * mDisplay;
    XImage *mXImage = NULL;

// Window    mRootWin;
    XWindowAttributes attribs;
    char *dispName;
	
	if (vo_rootwin)
		WinID = 0; // use root window

    if (vo_depthonscreen)
    {
        saver_off(mDisplay);
        return 1;               // already called
    }

    XSetErrorHandler(x11_errorhandler);

#if 0
    if (!mDisplayName)
        if (!(mDisplayName = getenv("DISPLAY")))
            mDisplayName = strdup(":0.0");
#else
    dispName = XDisplayName(mDisplayName);
#endif

    mp_msg(MSGT_VO, MSGL_V, "X11 opening display: %s\n", dispName);

    mDisplay = XOpenDisplay(dispName);
    if (!mDisplay)
    {
        mp_msg(MSGT_VO, MSGL_ERR,
               "vo: couldn't open the X11 display (%s)!\n", dispName);
        return 0;
    }
    mScreen = DefaultScreen(mDisplay);  // screen ID
    mRootWin = RootWindow(mDisplay, mScreen);   // root window ID

    init_atoms();

#ifdef HAVE_XF86VM
    {
        int clock;

        XF86VidModeGetModeLine(mDisplay, mScreen, &clock, &modeline);
        if (!vo_screenwidth)
            vo_screenwidth = modeline.hdisplay;
        if (!vo_screenheight)
            vo_screenheight = modeline.vdisplay;
    }
#endif
    {
        if (!vo_screenwidth)
            vo_screenwidth = DisplayWidth(mDisplay, mScreen);
        if (!vo_screenheight)
            vo_screenheight = DisplayHeight(mDisplay, mScreen);
    }
    // get color depth (from root window, or the best visual):
    XGetWindowAttributes(mDisplay, mRootWin, &attribs);
    depth = attribs.depth;

    if (depth != 15 && depth != 16 && depth != 24 && depth != 32)
    {
        Visual *visual;

        depth = vo_find_depth_from_visuals(mDisplay, mScreen, &visual);
        if (depth != -1)
            mXImage = XCreateImage(mDisplay, visual, depth, ZPixmap,
                                   0, NULL, 1, 1, 8, 1);
    } else
        mXImage =
            XGetImage(mDisplay, mRootWin, 0, 0, 1, 1, AllPlanes, ZPixmap);

    vo_depthonscreen = depth;   // display depth on screen

    // get bits/pixel from XImage structure:
    if (mXImage == NULL)
    {
        mask = 0;
    } else
    {
        /*
         * for the depth==24 case, the XImage structures might use
         * 24 or 32 bits of data per pixel.  The global variable
         * vo_depthonscreen stores the amount of data per pixel in the
         * XImage structure!
         *
         * Maybe we should rename vo_depthonscreen to (or add) vo_bpp?
         */
        bpp = mXImage->bits_per_pixel;
        if ((vo_depthonscreen + 7) / 8 != (bpp + 7) / 8)
            vo_depthonscreen = bpp;     // by A'rpi
        mask =
            mXImage->red_mask | mXImage->green_mask | mXImage->blue_mask;
        mp_msg(MSGT_VO, MSGL_V,
               "vo: X11 color mask:  %X  (R:%lX G:%lX B:%lX)\n", mask,
               mXImage->red_mask, mXImage->green_mask, mXImage->blue_mask);
        XDestroyImage(mXImage);
    }
    if (((vo_depthonscreen + 7) / 8) == 2)
    {
        if (mask == 0x7FFF)
            vo_depthonscreen = 15;
        else if (mask == 0xFFFF)
            vo_depthonscreen = 16;
    }
// XCloseDisplay( mDisplay );
/* slightly improved local display detection AST */
    if (strncmp(dispName, "unix:", 5) == 0)
        dispName += 4;
    else if (strncmp(dispName, "localhost:", 10) == 0)
        dispName += 9;
    if (*dispName == ':' && atoi(dispName + 1) < 10)
        mLocalDisplay = 1;
    else
        mLocalDisplay = 0;
    mp_msg(MSGT_VO, MSGL_V,
           "vo: X11 running at %dx%d with depth %d and %d bpp (\"%s\" => %s display)\n",
           vo_screenwidth, vo_screenheight, depth, vo_depthonscreen,
           dispName, mLocalDisplay ? "local" : "remote");

    vo_wm_type = vo_wm_detect();

    vo_fs_type = vo_x11_get_fs_type(vo_wm_type);

    fstype_dump(vo_fs_type);

    saver_off(mDisplay);
    return 1;
}

void vo_uninit(void)
{
    if (!mDisplay)
    {
        mp_msg(MSGT_VO, MSGL_V,
               "vo: x11 uninit called but X11 not inited..\n");
        return;
    }
// if( !vo_depthonscreen ) return;
    mp_msg(MSGT_VO, MSGL_V, "vo: uninit ...\n");
    XSetErrorHandler(NULL);
    XCloseDisplay(mDisplay);
    vo_depthonscreen = 0;
    mDisplay = NULL;
}

#include "osdep/keycodes.h"
#include "wskeys.h"

#ifdef XF86XK_AudioPause
static void vo_x11_putkey_ext(int keysym)
{
    switch (keysym)
    {
        case XF86XK_AudioPause:
            mplayer_put_key(KEY_PAUSE);
            break;
        case XF86XK_AudioStop:
            mplayer_put_key(KEY_STOP);
            break;
        case XF86XK_AudioPrev:
            mplayer_put_key(KEY_PREV);
            break;
        case XF86XK_AudioNext:
            mplayer_put_key(KEY_NEXT);
            break;
        default:
            break;
    }
}
#endif

void vo_x11_putkey(int key)
{
    switch (key)
    {
        case wsLeft:
            mplayer_put_key(KEY_LEFT);
            break;
        case wsRight:
            mplayer_put_key(KEY_RIGHT);
            break;
        case wsUp:
            mplayer_put_key(KEY_UP);
            break;
        case wsDown:
            mplayer_put_key(KEY_DOWN);
            break;
        case wsSpace:
            mplayer_put_key(' ');
            break;
        case wsEscape:
            mplayer_put_key(KEY_ESC);
            break;
        case wsTab:
            mplayer_put_key(KEY_TAB);
            break;
        case wsEnter:
            mplayer_put_key(KEY_ENTER);
            break;
        case wsBackSpace:
            mplayer_put_key(KEY_BS);
            break;
        case wsDelete:
            mplayer_put_key(KEY_DELETE);
            break;
        case wsInsert:
            mplayer_put_key(KEY_INSERT);
            break;
        case wsHome:
            mplayer_put_key(KEY_HOME);
            break;
        case wsEnd:
            mplayer_put_key(KEY_END);
            break;
        case wsPageUp:
            mplayer_put_key(KEY_PAGE_UP);
            break;
        case wsPageDown:
            mplayer_put_key(KEY_PAGE_DOWN);
            break;
        case wsF1:
            mplayer_put_key(KEY_F + 1);
            break;
        case wsF2:
            mplayer_put_key(KEY_F + 2);
            break;
        case wsF3:
            mplayer_put_key(KEY_F + 3);
            break;
        case wsF4:
            mplayer_put_key(KEY_F + 4);
            break;
        case wsF5:
            mplayer_put_key(KEY_F + 5);
            break;
        case wsF6:
            mplayer_put_key(KEY_F + 6);
            break;
        case wsF7:
            mplayer_put_key(KEY_F + 7);
            break;
        case wsF8:
            mplayer_put_key(KEY_F + 8);
            break;
        case wsF9:
            mplayer_put_key(KEY_F + 9);
            break;
        case wsF10:
            mplayer_put_key(KEY_F + 10);
            break;
        case wsF11:
            mplayer_put_key(KEY_F + 11);
            break;
        case wsF12:
            mplayer_put_key(KEY_F + 12);
            break;
        case wsMinus:
        case wsGrayMinus:
            mplayer_put_key('-');
            break;
        case wsPlus:
        case wsGrayPlus:
            mplayer_put_key('+');
            break;
        case wsGrayMul:
        case wsMul:
            mplayer_put_key('*');
            break;
        case wsGrayDiv:
        case wsDiv:
            mplayer_put_key('/');
            break;
        case wsLess:
            mplayer_put_key('<');
            break;
        case wsMore:
            mplayer_put_key('>');
            break;
        case wsGray0:
            mplayer_put_key(KEY_KP0);
            break;
        case wsGrayEnd:
        case wsGray1:
            mplayer_put_key(KEY_KP1);
            break;
        case wsGrayDown:
        case wsGray2:
            mplayer_put_key(KEY_KP2);
            break;
        case wsGrayPgDn:
        case wsGray3:
            mplayer_put_key(KEY_KP3);
            break;
        case wsGrayLeft:
        case wsGray4:
            mplayer_put_key(KEY_KP4);
            break;
        case wsGray5Dup:
        case wsGray5:
            mplayer_put_key(KEY_KP5);
            break;
        case wsGrayRight:
        case wsGray6:
            mplayer_put_key(KEY_KP6);
            break;
        case wsGrayHome:
        case wsGray7:
            mplayer_put_key(KEY_KP7);
            break;
        case wsGrayUp:
        case wsGray8:
            mplayer_put_key(KEY_KP8);
            break;
        case wsGrayPgUp:
        case wsGray9:
            mplayer_put_key(KEY_KP9);
            break;
        case wsGrayDecimal:
            mplayer_put_key(KEY_KPDEC);
            break;
        case wsGrayInsert:
            mplayer_put_key(KEY_KPINS);
            break;
        case wsGrayDelete:
            mplayer_put_key(KEY_KPDEL);
            break;
        case wsGrayEnter:
            mplayer_put_key(KEY_KPENTER);
            break;
        case wsGrave:
            mplayer_put_key('`');
            break;
        case wsTilde:
            mplayer_put_key('~');
            break;
        case wsExclSign:
            mplayer_put_key('!');
            break;
        case wsAt:
            mplayer_put_key('@');
            break;
        case wsHash:
            mplayer_put_key('#');
            break;
        case wsDollar:
            mplayer_put_key('$');
            break;
        case wsPercent:
            mplayer_put_key('%');
            break;
        case wsCircumflex:
            mplayer_put_key('^');
            break;
        case wsAmpersand:
            mplayer_put_key('&');
            break;
        case wsobracket:
            mplayer_put_key('(');
            break;
        case wscbracket:
            mplayer_put_key(')');
            break;
        case wsUnder:
            mplayer_put_key('_');
            break;
        case wsocbracket:
            mplayer_put_key('{');
            break;
        case wsccbracket:
            mplayer_put_key('}');
            break;
        case wsColon:
            mplayer_put_key(':');
            break;
        case wsSemicolon:
            mplayer_put_key(';');
            break;
        case wsDblQuote:
            mplayer_put_key('\"');
            break;
        case wsAcute:
            mplayer_put_key('\'');
            break;
        case wsComma:
            mplayer_put_key(',');
            break;
        case wsPoint:
            mplayer_put_key('.');
            break;
        case wsQuestSign:
            mplayer_put_key('?');
            break;
        case wsBSlash:
            mplayer_put_key('\\');
            break;
        case wsPipe:
            mplayer_put_key('|');
            break;
        case wsEqual:
            mplayer_put_key('=');
            break;
        case wsosbrackets:
            mplayer_put_key('[');
            break;
        case wscsbrackets:
            mplayer_put_key(']');
            break;


        default:
            if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') ||
                (key >= '0' && key <= '9'))
                mplayer_put_key(key);
    }

}


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

#define MWM_TEAROFF_WINDOW      (1L<<0)

typedef struct
{
    long flags;
    long functions;
    long decorations;
    long input_mode;
    long state;
} MotifWmHints;

static MotifWmHints vo_MotifWmHints;
static Atom vo_MotifHints = None;

void vo_x11_decoration(Display * vo_Display, Window w, int d)
{
    static unsigned int olddecor = MWM_DECOR_ALL;
    static unsigned int oldfuncs =
        MWM_FUNC_MOVE | MWM_FUNC_CLOSE | MWM_FUNC_MINIMIZE |
        MWM_FUNC_MAXIMIZE | MWM_FUNC_RESIZE;
    Atom mtype;
    int mformat;
    unsigned long mn, mb;

    if (!WinID)
        return;

    if (vo_fsmode & 8)
    {
        XSetTransientForHint(vo_Display, w,
                             RootWindow(vo_Display, mScreen));
    }

    vo_MotifHints = XInternAtom(vo_Display, "_MOTIF_WM_HINTS", 0);
    if (vo_MotifHints != None)
    {
        if (!d)
        {
            MotifWmHints *mhints = NULL;

            XGetWindowProperty(vo_Display, w, vo_MotifHints, 0, 20, False,
                               vo_MotifHints, &mtype, &mformat, &mn,
                               &mb, (unsigned char **) &mhints);
            if (mhints)
            {
                if (mhints->flags & MWM_HINTS_DECORATIONS)
                    olddecor = mhints->decorations;
                if (mhints->flags & MWM_HINTS_FUNCTIONS)
                    oldfuncs = mhints->functions;
                XFree(mhints);
            }
        }

        memset(&vo_MotifWmHints, 0, sizeof(MotifWmHints));
        vo_MotifWmHints.flags =
            MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS;
        if (d)
        {
            vo_MotifWmHints.functions = oldfuncs;
            d = olddecor;
        }
#if 0
        vo_MotifWmHints.decorations =
            d | ((vo_fsmode & 2) ? 0 : MWM_DECOR_MENU);
#else
        vo_MotifWmHints.decorations =
            d | ((vo_fsmode & 2) ? MWM_DECOR_MENU : 0);
#endif
        XChangeProperty(vo_Display, w, vo_MotifHints, vo_MotifHints, 32,
                        PropModeReplace,
                        (unsigned char *) &vo_MotifWmHints,
                        (vo_fsmode & 4) ? 4 : 5);
    }
}

void vo_x11_classhint(Display * display, Window window, char *name)
{
    XClassHint wmClass;
    pid_t pid = getpid();

    wmClass.res_name = name;
    wmClass.res_class = "MPlayer";
    XSetClassHint(display, window, &wmClass);
    XChangeProperty(display, window, XA_NET_WM_PID, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *) &pid, 1);
}

Window vo_window = None;
GC vo_gc = NULL;
GC f_gc = NULL;
XSizeHints vo_hint;

#ifdef HAVE_NEW_GUI
void vo_setwindow(Window w, GC g)
{
    vo_window = w;
    vo_gc = g;
}
#endif

void vo_x11_uninit(void)
{
    saver_on(mDisplay);
    if (vo_window != None)
        vo_showcursor(mDisplay, vo_window);

    if (f_gc)
    {
        XFreeGC(mDisplay, f_gc);
        f_gc = NULL;
    }
#ifdef HAVE_NEW_GUI
    /* destroy window only if it's not controlled by the GUI */
    if (!use_gui)
#endif
    {
        if (vo_gc)
        {
            XSetBackground(mDisplay, vo_gc, 0);
            XFreeGC(mDisplay, vo_gc);
            vo_gc = NULL;
        }
        if (vo_window != None)
        {
            XClearWindow(mDisplay, vo_window);
            if (WinID < 0)
            {
                XEvent xev;

                XUnmapWindow(mDisplay, vo_window);
                XDestroyWindow(mDisplay, vo_window);
                do
                {
                    XNextEvent(mDisplay, &xev);
                }
                while (xev.type != DestroyNotify
                       || xev.xdestroywindow.event != vo_window);
            }
            vo_window = None;
        }
        vo_fs = 0;
        vo_old_width = vo_old_height = 0;
    }
}

static unsigned int mouse_timer;
static int mouse_waiting_hide;

int vo_x11_check_events(Display * mydisplay)
{
    int ret = 0;
    XEvent Event;
    char buf[100];
    KeySym keySym;
    static XComposeStatus stat;

// unsigned long  vo_KeyTable[512];

    if ((vo_mouse_autohide) && mouse_waiting_hide &&
                                 (GetTimerMS() - mouse_timer >= 1000)) {
        vo_hidecursor(mydisplay, vo_window);
        mouse_waiting_hide = 0;
    }

    while (XPending(mydisplay))
    {
        XNextEvent(mydisplay, &Event);
#ifdef HAVE_NEW_GUI
        if (use_gui)
        {
            guiGetEvent(0, (char *) &Event);
            if (vo_window != Event.xany.window)
                continue;
        }
#endif
//       printf("\rEvent.type=%X  \n",Event.type);
        switch (Event.type)
        {
            case Expose:
                ret |= VO_EVENT_EXPOSE;
                break;
            case ConfigureNotify:
//         if (!vo_fs && (Event.xconfigure.width == vo_screenwidth || Event.xconfigure.height == vo_screenheight)) break;
//         if (vo_fs && Event.xconfigure.width != vo_screenwidth && Event.xconfigure.height != vo_screenheight) break;
                if (vo_window == None)
                    break;
                vo_dwidth = Event.xconfigure.width;
                vo_dheight = Event.xconfigure.height;
#if 0
                /* when resizing, x and y are zero :( */
                vo_dx = Event.xconfigure.x;
                vo_dy = Event.xconfigure.y;
#else
                {
                    Window root;
                    int foo;
                    Window win;

                    XGetGeometry(mydisplay, vo_window, &root, &foo, &foo,
                                 &foo /*width */ , &foo /*height */ , &foo,
                                 &foo);
                    XTranslateCoordinates(mydisplay, vo_window, root, 0, 0,
                                          &vo_dx, &vo_dy, &win);
                }
#endif
                ret |= VO_EVENT_RESIZE;
                break;
            case KeyPress:
                {
                    int key;

#ifdef HAVE_NEW_GUI
                    if ( use_gui ) { break; }
#endif

                    XLookupString(&Event.xkey, buf, sizeof(buf), &keySym,
                                  &stat);
#ifdef XF86XK_AudioPause
                    vo_x11_putkey_ext(keySym);
#endif
                    key =
                        ((keySym & 0xff00) !=
                         0 ? ((keySym & 0x00ff) + 256) : (keySym));
                    vo_x11_putkey(key);
                    ret |= VO_EVENT_KEYPRESS;
                }
                break;
            case MotionNotify:
                if(enable_mouse_movements)
                {
                    char cmd_str[40];
                    sprintf(cmd_str,"set_mouse_pos %i %i",Event.xmotion.x, Event.xmotion.y);
                    mp_input_queue_cmd(mp_input_parse_cmd(cmd_str));
                }

                if (vo_mouse_autohide)
                {
                    vo_showcursor(mydisplay, vo_window);
                    mouse_waiting_hide = 1;
                    mouse_timer = GetTimerMS();
                }
                break;
            case ButtonPress:
                if (vo_mouse_autohide)
                {
                    vo_showcursor(mydisplay, vo_window);
                    mouse_waiting_hide = 1;
                    mouse_timer = GetTimerMS();
                }
#ifdef HAVE_NEW_GUI
                // Ignore mouse button 1-3 under GUI.
                if (use_gui && (Event.xbutton.button >= 1)
                    && (Event.xbutton.button <= 3))
                    break;
#endif
                mplayer_put_key((MOUSE_BTN0 + Event.xbutton.button -
                                 1) | MP_KEY_DOWN);
                break;
            case ButtonRelease:
                if (vo_mouse_autohide)
                {
                    vo_showcursor(mydisplay, vo_window);
                    mouse_waiting_hide = 1;
                    mouse_timer = GetTimerMS();
                }
#ifdef HAVE_NEW_GUI
                // Ignore mouse button 1-3 under GUI.
                if (use_gui && (Event.xbutton.button >= 1)
                    && (Event.xbutton.button <= 3))
                    break;
#endif
                mplayer_put_key(MOUSE_BTN0 + Event.xbutton.button - 1);
                break;
            case PropertyNotify:
                {
                    char *name =
                        XGetAtomName(mydisplay, Event.xproperty.atom);

                    if (!name)
                        break;

//          fprintf(stderr,"[ws] PropertyNotify ( 0x%x ) %s ( 0x%x )\n",vo_window,name,Event.xproperty.atom );

                    XFree(name);
                }
                break;
            case MapNotify:
                vo_hint.win_gravity = old_gravity;
                XSetWMNormalHints(mDisplay, vo_window, &vo_hint);
                vo_fs_flip = 0;
                break;
	    case ClientMessage:
                if (Event.xclient.message_type == XAWM_PROTOCOLS &&
                    Event.xclient.data.l[0] == XAWM_DELETE_WINDOW)
                    mplayer_put_key(KEY_CLOSE_WIN);
                break;
        }
    }
    return ret;
}

/**
 * \brief sets the size and position of the non-fullscreen window.
 */
void vo_x11_nofs_sizepos(int x, int y, int width, int height)
{
  if (vo_fs) {
    vo_old_x = x;
    vo_old_y = y;
    vo_old_width = width;
    vo_old_height = height;
  }
  else
  {
   vo_dwidth = width;
   vo_dheight = height;
   XMoveResizeWindow(mDisplay, vo_window, x, y, width, height);
  }
}

void vo_x11_sizehint(int x, int y, int width, int height, int max)
{
    vo_hint.flags = PPosition | PSize | PWinGravity;
    if (vo_keepaspect)
    {
        vo_hint.flags |= PAspect;
        vo_hint.min_aspect.x = width;
        vo_hint.min_aspect.y = height;
        vo_hint.max_aspect.x = width;
        vo_hint.max_aspect.y = height;
    }

    vo_hint.x = x;
    vo_hint.y = y;
    vo_hint.width = width;
    vo_hint.height = height;
    if (max)
    {
        vo_hint.max_width = width;
        vo_hint.max_height = height;
        vo_hint.flags |= PMaxSize;
    } else
    {
        vo_hint.max_width = 0;
        vo_hint.max_height = 0;
    }

    // Set minimum height/width to 4 to avoid off-by-one errors
    // and because mga_vid requires a minimal size of 4 pixels.
    vo_hint.min_width = vo_hint.min_height = 4;
    vo_hint.flags |= PMinSize;

    vo_hint.win_gravity = StaticGravity;
    XSetWMNormalHints(mDisplay, vo_window, &vo_hint);
}

static int vo_x11_get_gnome_layer(Display * mDisplay, Window win)
{
    Atom type;
    int format;
    unsigned long nitems;
    unsigned long bytesafter;
    unsigned short *args = NULL;

    if (XGetWindowProperty(mDisplay, win, XA_WIN_LAYER, 0, 16384,
                           False, AnyPropertyType, &type, &format, &nitems,
                           &bytesafter,
                           (unsigned char **) &args) == Success
        && nitems > 0 && args)
    {
        mp_msg(MSGT_VO, MSGL_V, "[x11] original window layer is %d.\n",
               *args);
        return *args;
    }
    return WIN_LAYER_NORMAL;
}

//
Window vo_x11_create_smooth_window(Display * mDisplay, Window mRoot,
                                   Visual * vis, int x, int y,
                                   unsigned int width, unsigned int height,
                                   int depth, Colormap col_map)
{
    unsigned long xswamask = CWBackingStore | CWBorderPixel;
    XSetWindowAttributes xswa;
    Window ret_win;

    if (col_map != CopyFromParent)
    {
        xswa.colormap = col_map;
        xswamask |= CWColormap;
    }
    xswa.background_pixel = 0;
    xswa.border_pixel = 0;
    xswa.backing_store = Always;
    xswa.bit_gravity = StaticGravity;

    ret_win =
        XCreateWindow(mDisplay, mRootWin, x, y, width, height, 0, depth,
                      CopyFromParent, vis, xswamask, &xswa);
    XSetWMProtocols(mDisplay, ret_win, &XAWM_DELETE_WINDOW, 1);
    if (!f_gc)
        f_gc = XCreateGC(mDisplay, ret_win, 0, 0);
    XSetForeground(mDisplay, f_gc, 0);

    return ret_win;
}


void vo_x11_clearwindow_part(Display * mDisplay, Window vo_window,
                             int img_width, int img_height, int use_fs)
{
    int u_dheight, u_dwidth, left_ov, left_ov2;

    if (!f_gc)
        return;

    u_dheight = use_fs ? vo_screenheight : vo_dheight;
    u_dwidth = use_fs ? vo_screenwidth : vo_dwidth;
    if ((u_dheight <= img_height) && (u_dwidth <= img_width))
        return;

    left_ov = (u_dheight - img_height) / 2;
    left_ov2 = (u_dwidth - img_width) / 2;

    XFillRectangle(mDisplay, vo_window, f_gc, 0, 0, u_dwidth, left_ov);
    XFillRectangle(mDisplay, vo_window, f_gc, 0, u_dheight - left_ov - 1,
                   u_dwidth, left_ov + 1);

    if (u_dwidth > img_width)
    {
        XFillRectangle(mDisplay, vo_window, f_gc, 0, left_ov, left_ov2,
                       img_height);
        XFillRectangle(mDisplay, vo_window, f_gc, u_dwidth - left_ov2 - 1,
                       left_ov, left_ov2, img_height);
    }

    XFlush(mDisplay);
}

void vo_x11_clearwindow(Display * mDisplay, Window vo_window)
{
    if (!f_gc)
        return;
    XFillRectangle(mDisplay, vo_window, f_gc, 0, 0, vo_screenwidth,
                   vo_screenheight);
    //
    XFlush(mDisplay);
}


void vo_x11_setlayer(Display * mDisplay, Window vo_window, int layer)
{
    if (WinID >= 0)
        return;

    if (vo_fs_type & vo_wm_LAYER)
    {
        XClientMessageEvent xev;

        if (!orig_layer)
            orig_layer = vo_x11_get_gnome_layer(mDisplay, vo_window);

        memset(&xev, 0, sizeof(xev));
        xev.type = ClientMessage;
        xev.display = mDisplay;
        xev.window = vo_window;
        xev.message_type = XA_WIN_LAYER;
        xev.format = 32;
        xev.data.l[0] = layer ? fs_layer : orig_layer;  // if not fullscreen, stay on default layer
        xev.data.l[1] = CurrentTime;
        mp_msg(MSGT_VO, MSGL_V,
               "[x11] Layered style stay on top (layer %ld).\n",
               xev.data.l[0]);
        XSendEvent(mDisplay, mRootWin, False, SubstructureNotifyMask,
                   (XEvent *) & xev);
    } else if (vo_fs_type & vo_wm_NETWM)
    {
        XClientMessageEvent xev;
        char *state;

        memset(&xev, 0, sizeof(xev));
        xev.type = ClientMessage;
        xev.message_type = XA_NET_WM_STATE;
        xev.display = mDisplay;
        xev.window = vo_window;
        xev.format = 32;
        xev.data.l[0] = layer;

        if (vo_fs_type & vo_wm_STAYS_ON_TOP)
            xev.data.l[1] = XA_NET_WM_STATE_STAYS_ON_TOP;
        else if (vo_fs_type & vo_wm_ABOVE)
            xev.data.l[1] = XA_NET_WM_STATE_ABOVE;
        else if (vo_fs_type & vo_wm_FULLSCREEN)
            xev.data.l[1] = XA_NET_WM_STATE_FULLSCREEN;
        else if (vo_fs_type & vo_wm_BELOW)
            // This is not fallback. We can safely assume that the situation
            // where only NETWM_STATE_BELOW is supported doesn't exist.
            xev.data.l[1] = XA_NET_WM_STATE_BELOW;

        XSendEvent(mDisplay, mRootWin, False, SubstructureRedirectMask,
                   (XEvent *) & xev);
        state = XGetAtomName(mDisplay, xev.data.l[1]);
        mp_msg(MSGT_VO, MSGL_V,
               "[x11] NET style stay on top (layer %d). Using state %s.\n",
               layer, state);
        XFree(state);
    }
}

static int vo_x11_get_fs_type(int supported)
{
    int i;
    int type = supported;

    if (vo_fstype_list)
    {
        i = 0;
        for (i = 0; vo_fstype_list[i]; i++)
        {
            int neg = 0;
            char *arg = vo_fstype_list[i];

            if (vo_fstype_list[i][0] == '-')
            {
                neg = 1;
                arg = vo_fstype_list[i] + 1;
            }

            if (!strncmp(arg, "layer", 5))
            {
                if (!neg && (arg[5] == '='))
                {
                    char *endptr = NULL;
                    int layer = strtol(vo_fstype_list[i] + 6, &endptr, 10);

                    if (endptr && *endptr == '\0' && layer >= 0
                        && layer <= 15)
                        fs_layer = layer;
                }
                if (neg)
                    type &= ~vo_wm_LAYER;
                else
                    type |= vo_wm_LAYER;
            } else if (!strcmp(arg, "above"))
            {
                if (neg)
                    type &= ~vo_wm_ABOVE;
                else
                    type |= vo_wm_ABOVE;
            } else if (!strcmp(arg, "fullscreen"))
            {
                if (neg)
                    type &= ~vo_wm_FULLSCREEN;
                else
                    type |= vo_wm_FULLSCREEN;
            } else if (!strcmp(arg, "stays_on_top"))
            {
                if (neg)
                    type &= ~vo_wm_STAYS_ON_TOP;
                else
                    type |= vo_wm_STAYS_ON_TOP;
            } else if (!strcmp(arg, "below"))
            {
                if (neg)
                    type &= ~vo_wm_BELOW;
                else
                    type |= vo_wm_BELOW;
            } else if (!strcmp(arg, "netwm"))
            {
                if (neg)
                    type &= ~vo_wm_NETWM;
                else
                    type |= vo_wm_NETWM;
            } else if (!strcmp(arg, "none"))
                return 0;
        }
    }

    return type;
}

void vo_x11_fullscreen(void)
{
    int x, y, w, h;

    if (WinID >= 0 || vo_fs_flip)
        return;

    if (vo_fs)
    {
        // fs->win
        if ( ! (vo_fs_type & vo_wm_FULLSCREEN) ) // not needed with EWMH fs
        {
            if (vo_dwidth != vo_screenwidth && vo_dheight != vo_screenheight)
                return;
            x = vo_old_x;
            y = vo_old_y;
            w = vo_old_width;
            h = vo_old_height;
	}

        vo_x11_ewmh_fullscreen(_NET_WM_STATE_REMOVE);   // removes fullscreen state if wm supports EWMH
        vo_fs = VO_FALSE;
    } else
    {
        // win->fs
        vo_x11_ewmh_fullscreen(_NET_WM_STATE_ADD);      // sends fullscreen state to be added if wm supports EWMH

        vo_fs = VO_TRUE;
        if ( ! (vo_fs_type & vo_wm_FULLSCREEN) ) // not needed with EWMH fs
        {
            if (vo_old_width &&
                (vo_dwidth == vo_screenwidth && vo_dwidth != vo_old_width) &&
                (vo_dheight == vo_screenheight && vo_dheight != vo_old_height))
                return;
            vo_old_x = vo_dx;
            vo_old_y = vo_dy;
            vo_old_width = vo_dwidth;
            vo_old_height = vo_dheight;
            update_xinerama_info();
            x = xinerama_x;
            y = xinerama_y;
            w = vo_screenwidth;
            h = vo_screenheight;
        }
    }
    {
        long dummy;

        XGetWMNormalHints(mDisplay, vo_window, &vo_hint, &dummy);
        if (!(vo_hint.flags & PWinGravity))
            old_gravity = NorthWestGravity;
        else
            old_gravity = vo_hint.win_gravity;
    }
    if (vo_wm_type == 0 && !(vo_fsmode & 16))
    {
        XUnmapWindow(mDisplay, vo_window);      // required for MWM
        XWithdrawWindow(mDisplay, vo_window, mScreen);
        vo_fs_flip = 1;
    }

    if ( ! (vo_fs_type & vo_wm_FULLSCREEN) ) // not needed with EWMH fs
    {
        vo_x11_decoration(mDisplay, vo_window, (vo_fs) ? 0 : 1);
        vo_x11_sizehint(x, y, w, h, 0);
        vo_x11_setlayer(mDisplay, vo_window, vo_fs);


        XMoveResizeWindow(mDisplay, vo_window, x, y, w, h);
    }
    /* some WMs lose ontop after fullscreen */
    if ((!(vo_fs)) & vo_ontop)
        vo_x11_setlayer(mDisplay, vo_window, vo_ontop);

    XMapRaised(mDisplay, vo_window);
    if ( ! (vo_fs_type & vo_wm_FULLSCREEN) ) // some WMs change window pos on map
        XMoveResizeWindow(mDisplay, vo_window, x, y, w, h);
    XRaiseWindow(mDisplay, vo_window);
    XFlush(mDisplay);
}

void vo_x11_ontop(void)
{
    vo_ontop = (!(vo_ontop));

    vo_x11_setlayer(mDisplay, vo_window, vo_ontop);
}

/*
 * XScreensaver stuff
 */

static int got_badwindow;
static XErrorHandler old_handler;

static int badwindow_handler(Display * dpy, XErrorEvent * error)
{
    if (error->error_code != BadWindow)
        return (*old_handler) (dpy, error);

    got_badwindow = True;
    return 0;
}

static Window find_xscreensaver_window(Display * dpy)
{
    int i;
    Window root = RootWindowOfScreen(DefaultScreenOfDisplay(dpy));
    Window root2, parent, *kids;
    Window retval = 0;
    Atom xs_version;
    unsigned int nkids = 0;

    xs_version = XInternAtom(dpy, "_SCREENSAVER_VERSION", True);

    if (!(xs_version != None &&
          XQueryTree(dpy, root, &root2, &parent, &kids, &nkids) &&
          kids && nkids))
        return 0;

    old_handler = XSetErrorHandler(badwindow_handler);

    for (i = 0; i < nkids; i++)
    {
        Atom type;
        int format;
        unsigned long nitems, bytesafter;
        char *v;
        int status;

        got_badwindow = False;
        status =
            XGetWindowProperty(dpy, kids[i], xs_version, 0, 200, False,
                               XA_STRING, &type, &format, &nitems,
                               &bytesafter, (unsigned char **) &v);
        XSync(dpy, False);
        if (got_badwindow)
            status = BadWindow;

        if (status == Success && type != None)
        {
            retval = kids[i];
            break;
        }
    }
    XFree(kids);
    XSetErrorHandler(old_handler);

    return retval;
}

static Window xs_windowid = 0;
static Atom deactivate;
static Atom screensaver;

static unsigned int time_last;

void xscreensaver_heartbeat(void)
{
    unsigned int time = GetTimerMS();
    XEvent ev;

    if (mDisplay && xs_windowid && (time - time_last) > 30000)
    {
        time_last = time;

        ev.xany.type = ClientMessage;
        ev.xclient.display = mDisplay;
        ev.xclient.window = xs_windowid;
        ev.xclient.message_type = screensaver;
        ev.xclient.format = 32;
        memset(&ev.xclient.data, 0, sizeof(ev.xclient.data));
        ev.xclient.data.l[0] = (long) deactivate;

        mp_msg(MSGT_VO, MSGL_DBG2, "Pinging xscreensaver.\n");
        old_handler = XSetErrorHandler(badwindow_handler);
        XSendEvent(mDisplay, xs_windowid, False, 0L, &ev);
        XSync(mDisplay, False);
        XSetErrorHandler(old_handler);        
    }
}

static void xscreensaver_disable(Display * dpy)
{
    mp_msg(MSGT_VO, MSGL_DBG2, "xscreensaver_disable()\n");

    xs_windowid = find_xscreensaver_window(dpy);
    if (!xs_windowid)
    {
        mp_msg(MSGT_VO, MSGL_INFO, MSGTR_CouldNotFindXScreenSaver);
        return;
    }
    mp_msg(MSGT_VO, MSGL_INFO,
           "xscreensaver_disable: xscreensaver wid=%ld.\n", xs_windowid);

    deactivate = XInternAtom(dpy, "DEACTIVATE", False);
    screensaver = XInternAtom(dpy, "SCREENSAVER", False);
}

static void xscreensaver_enable(void)
{
    xs_windowid = 0;
}

/*
 * End of XScreensaver stuff
 */

void saver_on(Display * mDisplay)
{

#ifdef HAVE_XDPMS
    int nothing;

    if (dpms_disabled)
    {
        if (DPMSQueryExtension(mDisplay, &nothing, &nothing))
        {
            if (!DPMSEnable(mDisplay))
            {                   // restoring power saving settings
                mp_msg(MSGT_VO, MSGL_WARN, "DPMS not available?\n");
            } else
            {
                // DPMS does not seem to be enabled unless we call DPMSInfo
                BOOL onoff;
                CARD16 state;

                DPMSForceLevel(mDisplay, DPMSModeOn);
                DPMSInfo(mDisplay, &state, &onoff);
                if (onoff)
                {
                    mp_msg(MSGT_VO, MSGL_V,
                           "Successfully enabled DPMS\n");
                } else
                {
                    mp_msg(MSGT_VO, MSGL_WARN, "Could not enable DPMS\n");
                }
            }
        }
        dpms_disabled = 0;
    }
#endif

    if (timeout_save)
    {
        int dummy, interval, prefer_blank, allow_exp;

        XGetScreenSaver(mDisplay, &dummy, &interval, &prefer_blank,
                        &allow_exp);
        XSetScreenSaver(mDisplay, timeout_save, interval, prefer_blank,
                        allow_exp);
        XGetScreenSaver(mDisplay, &timeout_save, &interval, &prefer_blank,
                        &allow_exp);
        timeout_save = 0;
    }

    if (stop_xscreensaver)
        xscreensaver_enable();
    if (kdescreensaver_was_running && stop_xscreensaver)
    {
        system
            ("dcop kdesktop KScreensaverIface enable true 2>/dev/null >/dev/null");
        kdescreensaver_was_running = 0;
    }


}

void saver_off(Display * mDisplay)
{

    int interval, prefer_blank, allow_exp;

#ifdef HAVE_XDPMS
    int nothing;

    if (DPMSQueryExtension(mDisplay, &nothing, &nothing))
    {
        BOOL onoff;
        CARD16 state;

        DPMSInfo(mDisplay, &state, &onoff);
        if (onoff)
        {
            Status stat;

            mp_msg(MSGT_VO, MSGL_V, "Disabling DPMS\n");
            dpms_disabled = 1;
            stat = DPMSDisable(mDisplay);       // monitor powersave off
            mp_msg(MSGT_VO, MSGL_V, "DPMSDisable stat: %d\n", stat);
        }
    }
#endif
    if (!timeout_save)
    {
        XGetScreenSaver(mDisplay, &timeout_save, &interval, &prefer_blank,
                        &allow_exp);
        if (timeout_save)
            XSetScreenSaver(mDisplay, 0, interval, prefer_blank,
                            allow_exp);
    }
    // turning off screensaver
    if (stop_xscreensaver)
        xscreensaver_disable(mDisplay);
    if (stop_xscreensaver && !kdescreensaver_was_running)
    {
        kdescreensaver_was_running =
            (system
             ("dcop kdesktop KScreensaverIface isEnabled 2>/dev/null | sed 's/1/true/g' | grep true 2>/dev/null >/dev/null")
             == 0);
        if (kdescreensaver_was_running)
            system
                ("dcop kdesktop KScreensaverIface enable false 2>/dev/null >/dev/null");
    }
}

static XErrorHandler old_handler = NULL;
static int selectinput_err = 0;
static int x11_selectinput_errorhandler(Display * display,
                                        XErrorEvent * event)
{
    if (event->error_code == BadAccess)
    {
        selectinput_err = 1;
        mp_msg(MSGT_VO, MSGL_ERR,
               "X11 error: BadAccess during XSelectInput Call\n");
        mp_msg(MSGT_VO, MSGL_ERR,
               "X11 error: The 'ButtonPressMask' mask of specified window has probably already used by another appication (see man XSelectInput)\n");
        /* If you think MPlayer should shutdown with this error,
         * comment out the following line */
        return 0;
    }
    if (old_handler != NULL)
        old_handler(display, event);
    else
        x11_errorhandler(display, event);
    return 0;
}

void vo_x11_selectinput_witherr(Display * display, Window w,
                                long event_mask)
{
    XSync(display, False);
    old_handler = XSetErrorHandler(x11_selectinput_errorhandler);
    selectinput_err = 0;
    if (vo_nomouse_input)
    {
        XSelectInput(display, w,
                     event_mask &
                     (~(ButtonPressMask | ButtonReleaseMask)));
    } else
    {
        XSelectInput(display, w, event_mask);
    }
    XSync(display, False);
    XSetErrorHandler(old_handler);
    if (selectinput_err)
    {
        mp_msg(MSGT_VO, MSGL_ERR,
               "X11 error: MPlayer discards mouse control (reconfiguring)\n");
        XSelectInput(display, w,
                     event_mask &
                     (~
                      (ButtonPressMask | ButtonReleaseMask |
                       PointerMotionMask)));
    }
}

#ifdef HAVE_XF86VM
void vo_vm_switch(uint32_t X, uint32_t Y, int *modeline_width,
                  int *modeline_height)
{
    int vm_event, vm_error;
    int vm_ver, vm_rev;
    int i, j, have_vm = 0;

    int modecount;

    if (XF86VidModeQueryExtension(mDisplay, &vm_event, &vm_error))
    {
        XF86VidModeQueryVersion(mDisplay, &vm_ver, &vm_rev);
        mp_msg(MSGT_VO, MSGL_V, "XF86VidMode extension v%i.%i\n", vm_ver,
               vm_rev);
        have_vm = 1;
    } else
        mp_msg(MSGT_VO, MSGL_WARN,
               "XF86VidMode extension not available.\n");

    if (have_vm)
    {
        if (vidmodes == NULL)
            XF86VidModeGetAllModeLines(mDisplay, mScreen, &modecount,
                                       &vidmodes);
        j = 0;
        *modeline_width = vidmodes[0]->hdisplay;
        *modeline_height = vidmodes[0]->vdisplay;

        for (i = 1; i < modecount; i++)
            if ((vidmodes[i]->hdisplay >= X)
                && (vidmodes[i]->vdisplay >= Y))
                if ((vidmodes[i]->hdisplay <= *modeline_width)
                    && (vidmodes[i]->vdisplay <= *modeline_height))
                {
                    *modeline_width = vidmodes[i]->hdisplay;
                    *modeline_height = vidmodes[i]->vdisplay;
                    j = i;
                }

        mp_msg(MSGT_VO, MSGL_INFO, MSGTR_SelectedVideoMode,
               *modeline_width, *modeline_height, X, Y);
        XF86VidModeLockModeSwitch(mDisplay, mScreen, 0);
        XF86VidModeSwitchToMode(mDisplay, mScreen, vidmodes[j]);
        XF86VidModeSwitchToMode(mDisplay, mScreen, vidmodes[j]);
        X = (vo_screenwidth - *modeline_width) / 2;
        Y = (vo_screenheight - *modeline_height) / 2;
        XF86VidModeSetViewPort(mDisplay, mScreen, X, Y);
    }
}

void vo_vm_close(Display * dpy)
{
#ifdef HAVE_NEW_GUI
    if (vidmodes != NULL && vo_window != None)
#else
    if (vidmodes != NULL)
#endif
    {
        int i, modecount;
        int screen;

        screen = DefaultScreen(dpy);

        free(vidmodes);
        vidmodes = NULL;
        XF86VidModeGetAllModeLines(mDisplay, mScreen, &modecount,
                                   &vidmodes);
        for (i = 0; i < modecount; i++)
            if ((vidmodes[i]->hdisplay == vo_screenwidth)
                && (vidmodes[i]->vdisplay == vo_screenheight))
            {
                mp_msg(MSGT_VO, MSGL_INFO,
                       "Returning to original mode %dx%d\n",
                       vo_screenwidth, vo_screenheight);
                break;
            }

        XF86VidModeSwitchToMode(dpy, screen, vidmodes[i]);
        XF86VidModeSwitchToMode(dpy, screen, vidmodes[i]);
        free(vidmodes);
        vidmodes = NULL;
    }
}
#endif

#endif                          /* X11_FULLSCREEN */


/*
 * Scan the available visuals on this Display/Screen.  Try to find
 * the 'best' available TrueColor visual that has a decent color
 * depth (at least 15bit).  If there are multiple visuals with depth
 * >= 15bit, we prefer visuals with a smaller color depth.
 */
int vo_find_depth_from_visuals(Display * dpy, int screen,
                               Visual ** visual_return)
{
    XVisualInfo visual_tmpl;
    XVisualInfo *visuals;
    int nvisuals, i;
    int bestvisual = -1;
    int bestvisual_depth = -1;

    visual_tmpl.screen = screen;
    visual_tmpl.class = TrueColor;
    visuals = XGetVisualInfo(dpy,
                             VisualScreenMask | VisualClassMask,
                             &visual_tmpl, &nvisuals);
    if (visuals != NULL)
    {
        for (i = 0; i < nvisuals; i++)
        {
            mp_msg(MSGT_VO, MSGL_V,
                   "vo: X11 truecolor visual %#lx, depth %d, R:%lX G:%lX B:%lX\n",
                   visuals[i].visualid, visuals[i].depth,
                   visuals[i].red_mask, visuals[i].green_mask,
                   visuals[i].blue_mask);
            /*
             * Save the visual index and its depth, if this is the first
             * truecolor visul, or a visual that is 'preferred' over the
             * previous 'best' visual.
             */
            if (bestvisual_depth == -1
                || (visuals[i].depth >= 15
                    && (visuals[i].depth < bestvisual_depth
                        || bestvisual_depth < 15)))
            {
                bestvisual = i;
                bestvisual_depth = visuals[i].depth;
            }
        }

        if (bestvisual != -1 && visual_return != NULL)
            *visual_return = visuals[bestvisual].visual;

        XFree(visuals);
    }
    return bestvisual_depth;
}


static Colormap cmap = None;
static XColor cols[256];
static int cm_size, red_mask, green_mask, blue_mask;


Colormap vo_x11_create_colormap(XVisualInfo * vinfo)
{
    unsigned k, r, g, b, ru, gu, bu, m, rv, gv, bv, rvu, gvu, bvu;

    if (vinfo->class != DirectColor)
        return XCreateColormap(mDisplay, mRootWin, vinfo->visual,
                               AllocNone);

    /* can this function get called twice or more? */
    if (cmap)
        return cmap;
    cm_size = vinfo->colormap_size;
    red_mask = vinfo->red_mask;
    green_mask = vinfo->green_mask;
    blue_mask = vinfo->blue_mask;
    ru = (red_mask & (red_mask - 1)) ^ red_mask;
    gu = (green_mask & (green_mask - 1)) ^ green_mask;
    bu = (blue_mask & (blue_mask - 1)) ^ blue_mask;
    rvu = 65536ull * ru / (red_mask + ru);
    gvu = 65536ull * gu / (green_mask + gu);
    bvu = 65536ull * bu / (blue_mask + bu);
    r = g = b = 0;
    rv = gv = bv = 0;
    m = DoRed | DoGreen | DoBlue;
    for (k = 0; k < cm_size; k++)
    {
        int t;

        cols[k].pixel = r | g | b;
        cols[k].red = rv;
        cols[k].green = gv;
        cols[k].blue = bv;
        cols[k].flags = m;
        t = (r + ru) & red_mask;
        if (t < r)
            m &= ~DoRed;
        r = t;
        t = (g + gu) & green_mask;
        if (t < g)
            m &= ~DoGreen;
        g = t;
        t = (b + bu) & blue_mask;
        if (t < b)
            m &= ~DoBlue;
        b = t;
        rv += rvu;
        gv += gvu;
        bv += bvu;
    }
    cmap = XCreateColormap(mDisplay, mRootWin, vinfo->visual, AllocAll);
    XStoreColors(mDisplay, cmap, cols, cm_size);
    return cmap;
}

/*
 * Via colormaps/gamma ramps we can do gamma, brightness, contrast,
 * hue and red/green/blue intensity, but we cannot do saturation.
 * Currently only gamma, brightness and contrast are implemented.
 * Is there sufficient interest for hue and/or red/green/blue intensity?
 */
/* these values have range [-100,100] and are initially 0 */
static int vo_gamma = 0;
static int vo_brightness = 0;
static int vo_contrast = 0;


uint32_t vo_x11_set_equalizer(char *name, int value)
{
    float gamma, brightness, contrast;
    float rf, gf, bf;
    int k;

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
    if (cmap == None)
        return VO_NOTAVAIL;

    if (!strcasecmp(name, "brightness"))
        vo_brightness = value;
    else if (!strcasecmp(name, "contrast"))
        vo_contrast = value;
    else if (!strcasecmp(name, "gamma"))
        vo_gamma = value;
    else
        return VO_NOTIMPL;

    brightness = 0.01 * vo_brightness;
    contrast = tan(0.0095 * (vo_contrast + 100) * M_PI / 4);
    gamma = pow(2, -0.02 * vo_gamma);

    rf = (float) ((red_mask & (red_mask - 1)) ^ red_mask) / red_mask;
    gf = (float) ((green_mask & (green_mask - 1)) ^ green_mask) /
        green_mask;
    bf = (float) ((blue_mask & (blue_mask - 1)) ^ blue_mask) / blue_mask;

    /* now recalculate the colormap using the newly set value */
    for (k = 0; k < cm_size; k++)
    {
        float s;

        s = pow(rf * k, gamma);
        s = (s - 0.5) * contrast + 0.5;
        s += brightness;
        if (s < 0)
            s = 0;
        if (s > 1)
            s = 1;
        cols[k].red = (unsigned short) (s * 65535);

        s = pow(gf * k, gamma);
        s = (s - 0.5) * contrast + 0.5;
        s += brightness;
        if (s < 0)
            s = 0;
        if (s > 1)
            s = 1;
        cols[k].green = (unsigned short) (s * 65535);

        s = pow(bf * k, gamma);
        s = (s - 0.5) * contrast + 0.5;
        s += brightness;
        if (s < 0)
            s = 0;
        if (s > 1)
            s = 1;
        cols[k].blue = (unsigned short) (s * 65535);
    }

    XStoreColors(mDisplay, cmap, cols, cm_size);
    XFlush(mDisplay);
    return VO_TRUE;
}

uint32_t vo_x11_get_equalizer(char *name, int *value)
{
    if (cmap == None)
        return VO_NOTAVAIL;
    if (!strcasecmp(name, "brightness"))
        *value = vo_brightness;
    else if (!strcasecmp(name, "contrast"))
        *value = vo_contrast;
    else if (!strcasecmp(name, "gamma"))
        *value = vo_gamma;
    else
        return VO_NOTIMPL;
    return VO_TRUE;
}

#ifdef HAVE_XV
int vo_xv_set_eq(uint32_t xv_port, char *name, int value)
{
    XvAttribute *attributes;
    int i, howmany, xv_atom;

    mp_dbg(MSGT_VO, MSGL_V, "xv_set_eq called! (%s, %d)\n", name, value);

    /* get available attributes */
    attributes = XvQueryPortAttributes(mDisplay, xv_port, &howmany);
    for (i = 0; i < howmany && attributes; i++)
        if (attributes[i].flags & XvSettable)
        {
            xv_atom = XInternAtom(mDisplay, attributes[i].name, True);
/* since we have SET_DEFAULTS first in our list, we can check if it's available
   then trigger it if it's ok so that the other values are at default upon query */
            if (xv_atom != None)
            {
                int hue = 0, port_value, port_min, port_max;

                if (!strcmp(attributes[i].name, "XV_BRIGHTNESS") &&
                    (!strcasecmp(name, "brightness")))
                    port_value = value;
                else if (!strcmp(attributes[i].name, "XV_CONTRAST") &&
                         (!strcasecmp(name, "contrast")))
                    port_value = value;
                else if (!strcmp(attributes[i].name, "XV_SATURATION") &&
                         (!strcasecmp(name, "saturation")))
                    port_value = value;
                else if (!strcmp(attributes[i].name, "XV_HUE") &&
                         (!strcasecmp(name, "hue")))
                {
                    port_value = value;
                    hue = 1;
                } else
                    /* Note: since 22.01.2002 GATOS supports these attrs for radeons (NK) */
                if (!strcmp(attributes[i].name, "XV_RED_INTENSITY") &&
                        (!strcasecmp(name, "red_intensity")))
                    port_value = value;
                else if (!strcmp(attributes[i].name, "XV_GREEN_INTENSITY")
                         && (!strcasecmp(name, "green_intensity")))
                    port_value = value;
                else if (!strcmp(attributes[i].name, "XV_BLUE_INTENSITY")
                         && (!strcasecmp(name, "blue_intensity")))
                    port_value = value;
                else
                    continue;

                port_min = attributes[i].min_value;
                port_max = attributes[i].max_value;

                /* nvidia hue workaround */
                if (hue && port_min == 0 && port_max == 360)
                {
                    port_value =
                        (port_value >=
                         0) ? (port_value - 100) : (port_value + 100);
                }
                // -100 -> min
                //   0  -> (max+min)/2
                // +100 -> max
                port_value =
                    (port_value + 100) * (port_max - port_min) / 200 +
                    port_min;
                XvSetPortAttribute(mDisplay, xv_port, xv_atom, port_value);
                return (VO_TRUE);
            }
        }
    return (VO_FALSE);
}

int vo_xv_get_eq(uint32_t xv_port, char *name, int *value)
{

    XvAttribute *attributes;
    int i, howmany, xv_atom;

    /* get available attributes */
    attributes = XvQueryPortAttributes(mDisplay, xv_port, &howmany);
    for (i = 0; i < howmany && attributes; i++)
        if (attributes[i].flags & XvGettable)
        {
            xv_atom = XInternAtom(mDisplay, attributes[i].name, True);
/* since we have SET_DEFAULTS first in our list, we can check if it's available
   then trigger it if it's ok so that the other values are at default upon query */
            if (xv_atom != None)
            {
                int val, port_value = 0, port_min, port_max;

                XvGetPortAttribute(mDisplay, xv_port, xv_atom,
                                   &port_value);

                port_min = attributes[i].min_value;
                port_max = attributes[i].max_value;
                val =
                    (port_value - port_min) * 200 / (port_max - port_min) -
                    100;

                if (!strcmp(attributes[i].name, "XV_BRIGHTNESS") &&
                    (!strcasecmp(name, "brightness")))
                    *value = val;
                else if (!strcmp(attributes[i].name, "XV_CONTRAST") &&
                         (!strcasecmp(name, "contrast")))
                    *value = val;
                else if (!strcmp(attributes[i].name, "XV_SATURATION") &&
                         (!strcasecmp(name, "saturation")))
                    *value = val;
                else if (!strcmp(attributes[i].name, "XV_HUE") &&
                         (!strcasecmp(name, "hue")))
                {
                    /* nasty nvidia detect */
                    if (port_min == 0 && port_max == 360)
                        *value = (val >= 0) ? (val - 100) : (val + 100);
                    else
                        *value = val;
                } else
                    /* Note: since 22.01.2002 GATOS supports these attrs for radeons (NK) */
                if (!strcmp(attributes[i].name, "XV_RED_INTENSITY") &&
                        (!strcasecmp(name, "red_intensity")))
                    *value = val;
                else if (!strcmp(attributes[i].name, "XV_GREEN_INTENSITY")
                         && (!strcasecmp(name, "green_intensity")))
                    *value = val;
                else if (!strcmp(attributes[i].name, "XV_BLUE_INTENSITY")
                         && (!strcasecmp(name, "blue_intensity")))
                    *value = val;
                else
                    continue;

                mp_dbg(MSGT_VO, MSGL_V, "xv_get_eq called! (%s, %d)\n",
                       name, *value);
                return (VO_TRUE);
            }
        }
    return (VO_FALSE);
}

/** \brief contains flags changing the execution of the colorkeying code */
xv_ck_info_t xv_ck_info = { CK_METHOD_MANUALFILL, CK_SRC_CUR };
unsigned long xv_colorkey; ///< The color used for manual colorkeying.
unsigned int xv_port; ///< The selected Xv port.

/**
 * \brief Interns the requested atom if it is available.
 *
 * \param atom_name String containing the name of the requested atom.
 *
 * \return Returns the atom if available, else None is returned.
 *
 */
static Atom xv_intern_atom_if_exists( char const * atom_name )
{
  XvAttribute * attributes;
  int attrib_count,i;
  Atom xv_atom = None;

  attributes = XvQueryPortAttributes( mDisplay, xv_port, &attrib_count );
  if( attributes!=NULL )
  {
    for ( i = 0; i < attrib_count; ++i )
    {
      if ( strcmp(attributes[i].name, atom_name ) == 0 )
      {
        xv_atom = XInternAtom( mDisplay, atom_name, False );
        break; // found what we want, break out
      }
    }
    XFree( attributes );
  }

  return xv_atom;
}

/**
 * \brief Try to enable vsync for xv.
 * \return Returns -1 if not available, 0 on failure and 1 on success.
 */
int vo_xv_enable_vsync(void)
{
  Atom xv_atom = xv_intern_atom_if_exists("XV_SYNC_TO_VBLANK");
  if (xv_atom == None)
    return -1;
  return XvSetPortAttribute(mDisplay, xv_port, xv_atom, 1) == Success;
}

/**
 * \brief Get maximum supported source image dimensions.
 *
 *   This function does not set the variables pointed to by
 * width and height if the information could not be retrieved,
 * so the caller is reponsible for properly initializing them.
 *
 * \param width [out] The maximum width gets stored here.
 * \param height [out] The maximum height gets stored here.
 *
 */
void vo_xv_get_max_img_dim( uint32_t * width, uint32_t * height )
{
  XvEncodingInfo * encodings;
  //unsigned long num_encodings, idx; to int or too long?!
  unsigned int num_encodings, idx;

  XvQueryEncodings( mDisplay, xv_port, &num_encodings, &encodings);

  if ( encodings )
  {
      for ( idx = 0; idx < num_encodings; ++idx )
      {
          if ( strcmp( encodings[idx].name, "XV_IMAGE" ) == 0 )
          {
              *width  = encodings[idx].width;
              *height = encodings[idx].height;
              break;
          }
      }
  }

  mp_msg( MSGT_VO, MSGL_V,
          "[xv common] Maximum source image dimensions: %ux%u\n",
          *width, *height );

  XvFreeEncodingInfo( encodings );
}

/**
 * \brief Print information about the colorkey method and source.
 *
 * \param ck_handling Integer value containing the information about
 *                    colorkey handling (see x11_common.h).
 *
 * Outputs the content of |ck_handling| as a readable message.
 *
 */
void vo_xv_print_ck_info(void)
{
  mp_msg( MSGT_VO, MSGL_V, "[xv common] " );

  switch ( xv_ck_info.method )
  {
    case CK_METHOD_NONE:
      mp_msg( MSGT_VO, MSGL_V, "Drawing no colorkey.\n" ); return;
    case CK_METHOD_AUTOPAINT:
      mp_msg( MSGT_VO, MSGL_V, "Colorkey is drawn by Xv." ); break;
    case CK_METHOD_MANUALFILL:
      mp_msg( MSGT_VO, MSGL_V, "Drawing colorkey manually." ); break;
    case CK_METHOD_BACKGROUND:
      mp_msg( MSGT_VO, MSGL_V, "Colorkey is drawn as window background." ); break;
  }

  mp_msg( MSGT_VO, MSGL_V, "\n[xv common] " );

  switch ( xv_ck_info.source )
  {
    case CK_SRC_CUR:      
      mp_msg( MSGT_VO, MSGL_V, "Using colorkey from Xv (0x%06lx).\n",
              xv_colorkey );
      break;
    case CK_SRC_USE:
      if ( xv_ck_info.method == CK_METHOD_AUTOPAINT )
      {
        mp_msg( MSGT_VO, MSGL_V,
                "Ignoring colorkey from MPlayer (0x%06lx).\n",
                xv_colorkey );
      }
      else
      {
        mp_msg( MSGT_VO, MSGL_V,
                "Using colorkey from MPlayer (0x%06lx)."
                " Use -colorkey to change.\n",
                xv_colorkey );
      }
      break;
    case CK_SRC_SET:
      mp_msg( MSGT_VO, MSGL_V,
              "Setting and using colorkey from MPlayer (0x%06lx)."
              " Use -colorkey to change.\n",
              xv_colorkey );
      break;
  }
}
/**
 * \brief Init colorkey depending on the settings in xv_ck_info.
 *
 * \return Returns 0 on failure and 1 on success.
 *
 * Sets the colorkey variable according to the CK_SRC_* and CK_METHOD_*
 * flags in xv_ck_info.
 *
 * Possiblilities:
 *   * Methods
 *     - manual colorkey drawing ( CK_METHOD_MANUALFILL )
 *     - set colorkey as window background ( CK_METHOD_BACKGROUND )
 *     - let Xv paint the colorkey ( CK_METHOD_AUTOPAINT )
 *   * Sources
 *     - use currently set colorkey ( CK_SRC_CUR )
 *     - use colorkey in vo_colorkey ( CK_SRC_USE )
 *     - use and set colorkey in vo_colorkey ( CK_SRC_SET )
 *
 * NOTE: If vo_colorkey has bits set after the first 3 low order bytes
 *       we don't draw anything as this means it was forced to off.
 */
int vo_xv_init_colorkey(void)
{
  Atom xv_atom;
  int rez;

  /* check if colorkeying is needed */
  xv_atom = xv_intern_atom_if_exists( "XV_COLORKEY" );

  /* if we have to deal with colorkeying ... */
  if( xv_atom != None && !(vo_colorkey & 0xFF000000) )
  {
    /* check if we should use the colorkey specified in vo_colorkey */
    if ( xv_ck_info.source != CK_SRC_CUR )
    {
      xv_colorkey = vo_colorkey;
  
      /* check if we have to set the colorkey too */
      if ( xv_ck_info.source == CK_SRC_SET )
      {
        xv_atom = XInternAtom(mDisplay, "XV_COLORKEY",False);
  
        rez = XvSetPortAttribute( mDisplay, xv_port, xv_atom, vo_colorkey );
        if ( rez != Success )
        {
          mp_msg( MSGT_VO, MSGL_FATAL,
                  "[xv common] Couldn't set colorkey!\n" );
          return 0; // error setting colorkey
        }
      }
    }
    else 
    {
      int colorkey_ret;

      rez=XvGetPortAttribute(mDisplay,xv_port, xv_atom, &colorkey_ret);
      if ( rez == Success )
      {
         xv_colorkey = colorkey_ret;
      }
      else
      {
        mp_msg( MSGT_VO, MSGL_FATAL,
                "[xv common] Couldn't get colorkey!"
                "Maybe the selected Xv port has no overlay.\n" );
        return 0; // error getting colorkey
      }
    }

    xv_atom = xv_intern_atom_if_exists( "XV_AUTOPAINT_COLORKEY" );    

    /* should we draw the colorkey ourselves or activate autopainting? */
    if ( xv_ck_info.method == CK_METHOD_AUTOPAINT )
    {
      rez = !Success; // reset rez to something different than Success
 
      if ( xv_atom != None ) // autopaint is supported
      {
        rez = XvSetPortAttribute( mDisplay, xv_port, xv_atom, 1 );
      }

      if ( rez != Success )
      {
        // fallback to manual colorkey drawing
        xv_ck_info.method = CK_METHOD_MANUALFILL;
      }
    }
    else // disable colorkey autopainting if supported
    {
      if ( xv_atom != None ) // we have autopaint attribute
      {
        XvSetPortAttribute( mDisplay, xv_port, xv_atom, 0 );
      }
    }
  }
  else // do no colorkey drawing at all
  {
    xv_ck_info.method = CK_METHOD_NONE;
  } /* end: should we draw colorkey */

  /* output information about the current colorkey settings */
  vo_xv_print_ck_info();

  return 1; // success
}

/**
 * \brief Draw the colorkey on the video window.
 *
 * Draws the colorkey depending on the set method ( colorkey_handling ).
 *
 * Also draws the black bars ( when the video doesn't fit the display in
 * fullscreen ) separately, so they don't overlap with the video area.
 * It doesn't call XFlush.
 *
 */
inline void vo_xv_draw_colorkey(  int32_t x,  int32_t y,
                                  int32_t w,  int32_t h  )
{
  if( xv_ck_info.method == CK_METHOD_MANUALFILL ||
      xv_ck_info.method == CK_METHOD_BACKGROUND   )//less tearing than XClearWindow()
  {
    XSetForeground( mDisplay, vo_gc, xv_colorkey );
    XFillRectangle( mDisplay, vo_window, vo_gc,
                    x, y,
                    w, h );
  }

  /* draw black bars if needed */
  /* TODO! move this to vo_x11_clearwindow_part() */
  if ( vo_fs )
  {
    XSetForeground( mDisplay, vo_gc, 0 );
    /* making non-overlap fills, requires 8 checks instead of 4 */
    if ( y > 0 )
      XFillRectangle( mDisplay, vo_window, vo_gc,
                      0, 0,
                      vo_screenwidth, y);
    if (x > 0)
      XFillRectangle( mDisplay, vo_window, vo_gc,
                      0, 0,
                      x, vo_screenheight);
    if (x + w < vo_screenwidth)
      XFillRectangle( mDisplay, vo_window, vo_gc,
                      x + w, 0,
                      vo_screenwidth, vo_screenheight);
    if (y + h < vo_screenheight)
      XFillRectangle( mDisplay, vo_window, vo_gc,
                      0, y + h,
                      vo_screenwidth, vo_screenheight);
  }
}

/** \brief Tests if a valid argument for the ck suboption was given. */
int xv_test_ck( void * arg )
{
  strarg_t * strarg = (strarg_t *)arg;

  if ( strargcmp( strarg, "use" ) == 0 ||
       strargcmp( strarg, "set" ) == 0 ||
       strargcmp( strarg, "cur" ) == 0    )
  {
    return 1;
  }

  return 0;
}
/** \brief Tests if a valid arguments for the ck-method suboption was given. */
int xv_test_ckm( void * arg )
{
  strarg_t * strarg = (strarg_t *)arg;

  if ( strargcmp( strarg, "bg" ) == 0 ||
       strargcmp( strarg, "man" ) == 0 ||
       strargcmp( strarg, "auto" ) == 0    )
  {
    return 1;
  }

  return 0;
}

/**
 * \brief Modify the colorkey_handling var according to str
 *
 * Checks if a valid pointer ( not NULL ) to the string
 * was given. And in that case modifies the colorkey_handling
 * var to reflect the requested behaviour.
 * If nothing happens the content of colorkey_handling stays
 * the same.
 *
 * \param str Pointer to the string or NULL
 *
 */
void xv_setup_colorkeyhandling( char const * ck_method_str,
                                char const * ck_str )
{
  /* check if a valid pointer to the string was passed */
  if ( ck_str )
  {
    if ( strncmp( ck_str, "use", 3 ) == 0 )
    {
      xv_ck_info.source = CK_SRC_USE;
    }
    else if ( strncmp( ck_str, "set", 3 ) == 0 )
    {
      xv_ck_info.source = CK_SRC_SET;
    }
  }
  /* check if a valid pointer to the string was passed */
  if ( ck_method_str )
  {
    if ( strncmp( ck_method_str, "bg", 2 ) == 0 )
    {
      xv_ck_info.method = CK_METHOD_BACKGROUND;
    }
    else if ( strncmp( ck_method_str, "man", 3 ) == 0 )
    {
      xv_ck_info.method = CK_METHOD_MANUALFILL;
    }    
    else if ( strncmp( ck_method_str, "auto", 4 ) == 0 )
    {
      xv_ck_info.method = CK_METHOD_AUTOPAINT;
    }    
  }
}

#endif
