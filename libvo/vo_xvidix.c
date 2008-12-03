/*
    VIDIX accelerated overlay in a X window
    
    (C) Alex Beregszaszi & Zoltan Ponekker & Nick Kurshev
    
    WS window manager by Pontscho/Fresh!

    Based on vo_gl.c and vo_vesa.c and vo_xmga.c (.so mastah! ;))
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
//#include <X11/keysym.h>

#ifdef CONFIG_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "x11_common.h"
#include "aspect.h"
#include "mp_msg.h"

#include "vosub_vidix.h"
#include "vidix/vidix.h"

#ifdef CONFIG_GUI
#include "gui/interface.h"
#endif


static const vo_info_t info = {
    "X11 (VIDIX)",
    "xvidix",
    "Alex Beregszaszi",
    ""
};

LIBVO_EXTERN(xvidix)
#define UNUSED(x) ((void)(x))   /* Removes warning about unused arguments */
/* X11 related variables */
/* Colorkey handling */
static int colorkey;
static vidix_grkey_t gr_key;

/* VIDIX related */
static char *vidix_name;

/* Image parameters */
static uint32_t image_width;
static uint32_t image_height;
static uint32_t image_format;

/* Window parameters */
static uint32_t window_x, window_y;
static uint32_t window_width, window_height;

/* used by XGetGeometry & XTranslateCoordinates for moving/resizing window */
static uint32_t drwX, drwY, drwWidth, drwHeight, drwBorderWidth,
    drwDepth, drwcX, drwcY, dwidth, dheight;

void set_video_eq(int cap);


static void set_window(int force_update)
{
    Window mRoot;

    if (WinID)
    {
        XGetGeometry(mDisplay, vo_window, &mRoot, &drwX, &drwY, &drwWidth,
                     &drwHeight, &drwBorderWidth, &drwDepth);
        drwX = drwY = 0;

        XTranslateCoordinates(mDisplay, vo_window, mRoot, 0, 0,
                              &drwcX, &drwcY, &mRoot);
        aspect(&dwidth, &dheight, A_NOZOOM);
        if (!vo_fs)
            mp_msg(MSGT_VO, MSGL_V,
                   "[xvidix] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",
                   drwcX, drwcY, drwX, drwY, drwWidth, drwHeight);

        /* following stuff copied from vo_xmga.c */
    } else
    {
        aspect(&dwidth, &dheight, A_NOZOOM);
        drwcX = drwX = vo_dx;
        drwcY = drwY = vo_dy;
        drwWidth = vo_dwidth;
        drwHeight = vo_dheight;
    }

#if X11_FULLSCREEN
    if (vo_fs)
    {
        aspect(&dwidth, &dheight, A_ZOOM);
        drwX =
            (vo_screenwidth -
             (dwidth > vo_screenwidth ? vo_screenwidth : dwidth)) / 2;
        drwcX = drwX;
        drwY =
            (vo_screenheight -
             (dheight > vo_screenheight ? vo_screenheight : dheight)) / 2;
        drwcY = drwY;
        drwWidth = (dwidth > vo_screenwidth ? vo_screenwidth : dwidth);
        drwHeight =
            (dheight > vo_screenheight ? vo_screenheight : dheight);
        mp_msg(MSGT_VO, MSGL_V,
               "[xvidix-fs] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",
               drwcX, drwcY, drwX, drwY, drwWidth, drwHeight);
    }
#endif

    vo_dwidth = drwWidth;
    vo_dheight = drwHeight;

    update_xinerama_info();
    drwcX -= xinerama_x;
    drwcY -= xinerama_y;

    if (vo_panscan > 0.0f && vo_fs)
    {
        drwcX -= vo_panscan_x >> 1;
        drwcY -= vo_panscan_y >> 1;
        drwX -= vo_panscan_x >> 1;
        drwY -= vo_panscan_y >> 1;
        drwWidth += vo_panscan_x;
        drwHeight += vo_panscan_y;
    }

    /* set new values in VIDIX */
    if (force_update || (window_x != drwcX) || (window_y != drwcY) ||
        (window_width != drwWidth) || (window_height != drwHeight))
    {
        // do a backup of window coordinates
        window_x = drwcX;
        window_y = drwcY;
        vo_dx = drwcX;
        vo_dy = drwcY;
        window_width = drwWidth;
        window_height = drwHeight;

        /* FIXME: implement runtime resize/move if possible, this way is very ugly! */
        vidix_stop();
        if (vidix_init(image_width, image_height, vo_dx, vo_dy,
                       window_width, window_height, image_format,
                       vo_depthonscreen, vo_screenwidth,
                       vo_screenheight) != 0)
        {
            mp_msg(MSGT_VO, MSGL_FATAL,
                   "Can't initialize VIDIX driver: %s\n", strerror(errno));
            abort();
        }
        vidix_start();
    }

    mp_msg(MSGT_VO, MSGL_V,
           "[xvidix] window properties: pos: %dx%d, size: %dx%d\n", vo_dx,
           vo_dy, window_width, window_height);

    /* mDrawColorKey: */

    /* fill drawable with specified color */
    if (!(vo_colorkey & 0xff000000))
    {
        XSetBackground(mDisplay, vo_gc, 0L);
        XClearWindow(mDisplay, vo_window);
        XSetForeground(mDisplay, vo_gc, colorkey);
        XFillRectangle(mDisplay, vo_window, vo_gc, drwX, drwY, drwWidth,
                       (vo_fs ? drwHeight - 1 : drwHeight));
    }
    /* flush, update drawable */
    XFlush(mDisplay);

    return;
}

/* connect to server, create and map window,
 * allocate colors and (shared) memory
 */
static int config(uint32_t width, uint32_t height, uint32_t d_width,
                       uint32_t d_height, uint32_t flags, char *title,
                       uint32_t format)
{
    XVisualInfo vinfo;

//    XSizeHints hint;
    XSetWindowAttributes xswa;
    unsigned long xswamask;
    XWindowAttributes attribs;
    int window_depth, r, g, b;

    title = "MPlayer VIDIX X11 Overlay";

    image_height = height;
    image_width = width;
    image_format = format;

    window_width = d_width;
    window_height = d_height;

//    vo_fs = flags&0x01;
//    if (vo_fs)
//     { vo_old_width=d_width; vo_old_height=d_height; }

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
                   "Sorry, this (%d) color depth is not supported\n",
                   vo_depthonscreen);
    }
    mp_msg(MSGT_VO, MSGL_V, "Using colorkey: %x\n", colorkey);

#ifdef CONFIG_GUI
    if (use_gui)
        guiGetEvent(guiSetShVideo, 0);  // the GUI will set up / resize the window
    else
    {
#endif

#ifdef X11_FULLSCREEN
        if ((flags & VOFLAG_FULLSCREEN) || (flags & VOFLAG_SWSCALE))
            aspect(&d_width, &d_height, A_ZOOM);
#endif
        dwidth = d_width;
        dheight = d_height;
        /* Make the window */
        XGetWindowAttributes(mDisplay, DefaultRootWindow(mDisplay),
                             &attribs);

        /* from vo_x11 */
        window_depth = attribs.depth;
        if ((window_depth != 15) && (window_depth != 16)
            && (window_depth != 24) && (window_depth != 32))
            window_depth = 24;
        XMatchVisualInfo(mDisplay, mScreen, window_depth, TrueColor,
                         &vinfo);

        xswa.background_pixel = BlackPixel(mDisplay, mScreen);
        xswa.border_pixel = 0;
        xswa.colormap =
            XCreateColormap(mDisplay, RootWindow(mDisplay, mScreen),
                            vinfo.visual, AllocNone);
        xswamask = CWBackPixel | CWBorderPixel | CWColormap;

            vo_x11_create_vo_window(&vinfo, vo_dx, vo_dy,
                    window_width, window_height, flags,
                    CopyFromParent, "xvidix", title);
            XChangeWindowAttributes(mDisplay, vo_window, xswamask, &xswa);

#ifdef CONFIG_GUI
    }
#endif

    if ((!WinID) && (flags & VOFLAG_FULLSCREEN))
    {
        vo_dx = 0;
        vo_dy = 0;
        vo_dwidth = vo_screenwidth;
        vo_dheight = vo_screenheight;
        vo_fs = 1;
    }

    if (vidix_grkey_support())
    {
        vidix_grkey_get(&gr_key);
        gr_key.key_op = KEYS_PUT;
        if (!(vo_colorkey & 0xff000000))
        {
            gr_key.ckey.op = CKEY_TRUE;
            gr_key.ckey.red = r;
            gr_key.ckey.green = g;
            gr_key.ckey.blue = b;
        } else
            gr_key.ckey.op = CKEY_FALSE;
        vidix_grkey_set(&gr_key);
    }

    set_window(1);

    XSync(mDisplay, False);

    panscan_calc();

    return 0;
}

static void check_events(void)
{
    const int event = vo_x11_check_events(mDisplay);

    if ((event & VO_EVENT_RESIZE) || (event & VO_EVENT_EXPOSE))
        set_window(0);

    return;
}

/* draw_osd, flip_page, draw_slice, draw_frame should be
   overwritten with vidix functions (vosub_vidix.c) */
static void draw_osd(void)
{
    mp_msg(MSGT_VO, MSGL_FATAL,
           "[xvidix] error: didn't used vidix draw_osd!\n");
    return;
}

static void flip_page(void)
{
    mp_msg(MSGT_VO, MSGL_FATAL,
           "[xvidix] error: didn't used vidix flip_page!\n");
    return;
}

static int draw_slice(uint8_t * src[], int stride[],
                           int w, int h, int x, int y)
{
    UNUSED(src);
    UNUSED(stride);
    UNUSED(w);
    UNUSED(h);
    UNUSED(x);
    UNUSED(y);
    mp_msg(MSGT_VO, MSGL_FATAL,
           "[xvidix] error: didn't used vidix draw_slice!\n");
    return -1;
}

static int draw_frame(uint8_t * src[])
{
    UNUSED(src);
    mp_msg(MSGT_VO, MSGL_FATAL,
           "[xvidix] error: didn't used vidix draw_frame!\n");
    return -1;
}

static int query_format(uint32_t format)
{
    return vidix_query_fourcc(format);
}

static void uninit(void)
{
    if (!vo_config_count)
        return;
    vidix_term();

    if (vidix_name)
    {
        free(vidix_name);
        vidix_name = NULL;
    }

    vo_x11_uninit();
}

static int preinit(const char *arg)
{

    if (arg)
        vidix_name = strdup(arg);
    else
    {
        mp_msg(MSGT_VO, MSGL_INFO,
               "No vidix driver name provided, probing available ones (-v option for details)!\n");
        vidix_name = NULL;
    }

    if (!vo_init())
        return -1;

    if (vidix_preinit(vidix_name, &video_out_xvidix) != 0)
        return 1;

    return 0;
}

static int control(uint32_t request, void *data, ...)
{
    switch (request)
    {
        case VOCTRL_QUERY_FORMAT:
            return query_format(*((uint32_t *) data));
        case VOCTRL_GUISUPPORT:
            return VO_TRUE;
        case VOCTRL_GET_PANSCAN:
            if (!vo_config_count || !vo_fs)
                return VO_FALSE;
            return VO_TRUE;
        case VOCTRL_ONTOP:
            vo_x11_ontop();
            return VO_TRUE;
        case VOCTRL_FULLSCREEN:
            vo_x11_fullscreen();
        case VOCTRL_SET_PANSCAN:
            if (vo_fs && (vo_panscan != vo_panscan_amount))
            {
                panscan_calc();
                set_window(0);
            }
            return VO_TRUE;
        case VOCTRL_SET_EQUALIZER:
            {
                va_list ap;
                int value;

                va_start(ap, data);
                value = va_arg(ap, int);

                va_end(ap);

                return vidix_control(request, data, value);
            }
        case VOCTRL_GET_EQUALIZER:
            {
                va_list ap;
                int *value;

                va_start(ap, data);
                value = va_arg(ap, int *);

                va_end(ap);

                return vidix_control(request, data, value);
            }
        case VOCTRL_UPDATE_SCREENINFO:
            aspect_save_screenres(vo_screenwidth, vo_screenheight);
            return VO_TRUE;

    }
    return vidix_control(request, data);
//  return VO_NOTIMPL;
}
