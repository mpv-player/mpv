
//#define SHOW_TIME

/*
 *    vo_xmga.c
 *
 *      Copyright (C) Zoltan Ponekker - Jan 2001
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include "video_out.h"
#include "video_out_internal.h"


#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "drivers/mga_vid.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <errno.h>

#ifdef CONFIG_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "x11_common.h"
#include "sub.h"
#include "aspect.h"

#ifdef SHOW_TIME
#include "osdep/timer.h"
static unsigned int timer = 0;
static unsigned int timerd = 0;
#endif

#ifdef CONFIG_GUI
#include "gui/interface.h"
#endif

static const vo_info_t info = {
    "Matrox G200/G4x0/G550 overlay in X11 window (using /dev/mga_vid)",
    "xmga",
    "Zoltan Ponekker <pontscho@makacs.poliod.hu>",
    ""
};

const LIBVO_EXTERN(xmga)

static uint32_t mDepth;
static XWindowAttributes attribs;
static int colorkey;

static uint32_t mvHeight;
static uint32_t mvWidth;

static Window mRoot;

static XSetWindowAttributes xWAttribs;

static int initialized = 0;

#define VO_XMGA
#include "mga_common.c"
#undef  VO_XMGA

static void mDrawColorKey(void)
{
    XSetBackground(mDisplay, vo_gc, 0);
    XClearWindow(mDisplay, vo_window);
    XSetForeground(mDisplay, vo_gc, colorkey);
    XFillRectangle(mDisplay, vo_window, vo_gc, drwX, drwY, drwWidth,
                   (vo_fs ? drwHeight - 1 : drwHeight));
    XFlush(mDisplay);
}

static void check_events(void)
{
    int e = vo_x11_check_events(mDisplay);

    if (!(e & VO_EVENT_RESIZE) && !(e & VO_EVENT_EXPOSE))
        return;
    set_window();
    mDrawColorKey();
    if (ioctl(f, MGA_VID_CONFIG, &mga_vid_config))
        mp_msg(MSGT_VO, MSGL_WARN,
               "Error in mga_vid_config ioctl (wrong mga_vid.o version?)");
}

static void flip_page(void)
{
#ifdef SHOW_TIME
    unsigned int t;

    t = GetTimer();
    mp_msg(MSGT_VO, MSGL_STATUS,
           "  [timer: %08X  diff: %6d  dd: %6d ]  \n", t, t - timer,
           (t - timer) - timerd);
    timerd = t - timer;
    timer = t;
#endif

    vo_mga_flip_page();
}

static int config(uint32_t width, uint32_t height, uint32_t d_width,
                       uint32_t d_height, uint32_t flags, char *title,
                       uint32_t format)
{
    XVisualInfo vinfo;
    unsigned long xswamask;
    int r, g, b;

    if (mga_init(width, height, format))
        return -1;              // ioctl errors?

    aspect_save_orig(width, height);
    aspect_save_prescale(d_width, d_height);
    update_xinerama_info();

    mvWidth = width;
    mvHeight = height;

    vo_panscan_x = vo_panscan_y = vo_panscan_amount = 0;

    aspect(&d_width, &d_height, A_NOZOOM);
    vo_dx = (vo_screenwidth - d_width) / 2;
    vo_dy = (vo_screenheight - d_height) / 2;
    geometry(&vo_dx, &vo_dy, &d_width, &d_height, vo_screenwidth,
             vo_screenheight);
    vo_dx += xinerama_x;
    vo_dy += xinerama_y;
    vo_dwidth = d_width;
    vo_dheight = d_height;

    r = (vo_colorkey & 0x00ff0000) >> 16;
    g = (vo_colorkey & 0x0000ff00) >> 8;
    b = vo_colorkey & 0x000000ff;
    switch (vo_depthonscreen)
    {
        case 32:
            colorkey = vo_colorkey;
            break;
        case 24:
            colorkey = vo_colorkey & 0x00ffffff;
            break;
        case 16:
            colorkey = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            break;
        case 15:
            colorkey = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
            break;
        default:
            mp_msg(MSGT_VO, MSGL_ERR,
                   "Sorry, this (%d) color depth not supported.\n",
                   vo_depthonscreen);
            return -1;
    }
    mp_msg(MSGT_VO, MSGL_V, "Using colorkey: %x\n", colorkey);

    initialized = 1;

#ifdef CONFIG_GUI
    if (use_gui)
        guiGetEvent(guiSetShVideo, 0);  // the GUI will set up / resize the window
    else
#endif
    {
        if (flags & VOFLAG_FULLSCREEN)
            aspect(&dwidth, &dheight, A_ZOOM);

        XGetWindowAttributes(mDisplay, mRootWin, &attribs);
        mDepth = attribs.depth;
        if (mDepth != 15 && mDepth != 16 && mDepth != 24 && mDepth != 32)
            mDepth = 24;
        XMatchVisualInfo(mDisplay, mScreen, mDepth, TrueColor, &vinfo);
        xWAttribs.colormap =
            XCreateColormap(mDisplay, mRootWin, vinfo.visual, AllocNone);
        xWAttribs.background_pixel = 0;
        xWAttribs.border_pixel = 0;
        xswamask = CWBackPixel | CWBorderPixel | CWColormap;

            vo_x11_create_vo_window(&vinfo, vo_dx, vo_dy, d_width, d_height,
                    flags, xWAttribs.colormap, "xmga", title);
            XChangeWindowAttributes(mDisplay, vo_window, xswamask, &xWAttribs);

    }                           // !GUI

    if ((flags & VOFLAG_FULLSCREEN) && (!WinID))
    {
        vo_dx = 0;
        vo_dy = 0;
        vo_dwidth = vo_screenwidth;
        vo_dheight = vo_screenheight;
        vo_fs = 1;
    }

    panscan_calc();

    mga_vid_config.colkey_on = 1;
    mga_vid_config.colkey_red = r;
    mga_vid_config.colkey_green = g;
    mga_vid_config.colkey_blue = b;

    set_window();               // set up mga_vid_config.dest_width etc

    XSync(mDisplay, False);

    ioctl(f, MGA_VID_ON, 0);

    return 0;
}

static void uninit(void)
{
    mp_msg(MSGT_VO, MSGL_V, "vo: uninit!\n");
    mga_uninit();
    if (!initialized)
        return;                 // no window?
    initialized = 0;
    vo_x11_uninit();            // destroy the window
}
