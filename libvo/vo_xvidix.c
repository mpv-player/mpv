/*
    VIDIX accelerated overlay in a X window
    
    (C) Alex Beregszaszi & Nick Kurshev
    
    WS window manager by Pontscho/Fresh!

    Based on vo_gl.c and vo_vesa.c
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

#include "x11_common.h"
#include "aspect.h"
#include "mp_msg.h"

#include "vosub_vidix.h"

LIBVO_EXTERN(xvidix)

static vo_info_t vo_info = 
{
    "X11 (VIDIX)",
    "xvidix",
    "Alex Beregszaszi",
    ""
};

/* X11 related variables */
static Window mywindow;
static int X_already_started = 0;

/* VIDIX related stuff */
static const char *vidix_name = (char *)(-1);
static int pre_init_err = 0;

/* Image parameters */
static uint32_t image_width;
static uint32_t image_height;
static uint32_t image_format;
static uint32_t image_depth;

/* Window parameters */
static uint32_t window_x, window_y;
static uint32_t window_width, window_height;

/* used by XGetGeometry & XTranslateCoordinates */
static Window mRoot;
static uint32_t drwX, drwY, drwWidth, drwHeight, drwBorderWidth,
    drwDepth, drwcX, drwcY, dwidth, dheight, mFullscreen;

static void resize(int x, int y)
{
    XGetGeometry(mDisplay, mywindow, &mRoot, &drwX, &drwY, &drwWidth,
	&drwHeight, &drwBorderWidth, &drwDepth);
    drwX = drwY = 0;
    XTranslateCoordinates(mDisplay, mywindow, mRoot, 0, 0, &drwcX, &drwcY, &mRoot);

    mp_msg(MSGT_VO, MSGL_DBG2, "[xvidix] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",
	drwcX, drwcY, drwX, drwY, drwWidth, drwHeight);

    if ((window_x != drwcX) || (window_y != drwcY) ||
	(window_width != drwWidth) || (window_height != drwHeight))
    {
	window_x = drwcX;
	window_y = drwcY;
	window_width = drwWidth;
	window_height = drwHeight;
	/* FIXME: implement runtime resize/move if possible, this way is very ugly! */
	vidix_term();
	vidix_preinit(vidix_name, &video_out_xvidix);
	if (vidix_init(image_width, image_height, window_x, window_y,
	    window_width, window_height, image_format, vo_depthonscreen, vo_screenwidth, vo_screenheight) != 0)
        {
	    mp_msg(MSGT_VO, MSGL_FATAL, "Can't initialize VIDIX driver: %s: %s\n",
		vidix_name, strerror(errno));
	    vidix_term();
	    uninit();
    	    exit(1); /* !!! */
	    x = window_width;
	    y = window_height;
	}
    }
    
    mp_msg(MSGT_VO, MSGL_INFO, "[xvidix] window properties: pos: %dx%d, size: %dx%d\n",
	window_x, window_y, window_width, window_height);

    return;
}

/* connect to server, create and map window,
 * allocate colors and (shared) memory
 */
static uint32_t init(uint32_t width, uint32_t height, uint32_t d_width,
    uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
    unsigned int fg, bg;
    XVisualInfo vinfo;
    XEvent xev;
    XSizeHints hint;
    XSetWindowAttributes xswa;
    unsigned long xswamask;
    XWindowAttributes attribs;
    int window_depth;

    if(pre_init_err)
    {
	mp_msg(MSGT_VO, MSGL_INFO, "[xvidix] Quiting due preinit error\n");
	return -1;
    }
//    if (title)
//	free(title);
    title = strdup("MPlayer VIDIX X11 Overlay");

    image_height = height;
    image_width = width;
    image_format = format;
    
    if (IMGFMT_IS_RGB(format))
    {
	image_depth = IMGFMT_RGB_DEPTH(format);
    }
    else
    if (IMGFMT_IS_BGR(format))
    {
	image_depth = IMGFMT_BGR_DEPTH(format);
    }
    else
    switch(format)
    {
	case IMGFMT_IYUV:
	case IMGFMT_I420:
	case IMGFMT_YV12:
	    image_depth = 12;
	    break;
	case IMGFMT_YUY2:
	    image_depth = 16;
	    break;
	default:
	    mp_msg(MSGT_VO, MSGL_FATAL, "Unknown image format: %s",
		vo_format_name(format));
	    return(-1);
    }

    if (X_already_started)
        return(-1);
    if (!vo_init())
        return(-1);

    aspect_save_orig(width,height);
    aspect_save_prescale(d_width,d_height);
    aspect_save_screenres(vo_screenwidth,vo_screenheight);

    X_already_started++;

    aspect(&d_width, &d_height, A_NOZOOM);
#ifdef X11_FULLSCREEN
    if (flags & 0x01) /* fullscreen */
      if(flags & 0x04)	aspect(&d_width, &d_height, A_ZOOM);
      else
      {
	d_width = vo_screenwidth;
	d_height = vo_screenheight;
      }
#endif

    hint.x = 0;
    hint.y = 0;
    hint.width = d_width;
    hint.height = d_height;
    hint.flags = PPosition | PSize;

    /* Get some colors */
    bg = WhitePixel(mDisplay, mScreen);
    fg = BlackPixel(mDisplay, mScreen);

    /* Make the window */
    XGetWindowAttributes(mDisplay, DefaultRootWindow(mDisplay), &attribs);

    /* from vo_x11 */
    window_depth = attribs.depth;
    if ((window_depth != 15) && (window_depth != 16) && (window_depth != 24)
	&& (window_depth != 32))
        window_depth = 24;
    XMatchVisualInfo(mDisplay, mScreen, window_depth, TrueColor, &vinfo);

    xswa.background_pixel = 0;
    xswa.border_pixel     = 1;
    xswa.colormap         = XCreateColormap(mDisplay, RootWindow(mDisplay, mScreen),
					    vinfo.visual, AllocNone);
    xswamask = CWBackPixel | CWBorderPixel | CWColormap;
//    xswamask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask | CWCursor | CWOverrideRedirect | CWSaveUnder | CWX | CWY | CWWidth | CWHeight;

    if (WinID >= 0)
    {
	mywindow = WinID ? ((Window)WinID) : RootWindow(mDisplay, mScreen);
	XUnmapWindow(mDisplay, mywindow);
	XChangeWindowAttributes(mDisplay, mywindow, xswamask, &xswa);
    }
    else
	mywindow = XCreateWindow(mDisplay, RootWindow(mDisplay, mScreen),
	    hint.x, hint.y, hint.width, hint.height, xswa.border_pixel,
	    vinfo.depth, CopyFromParent, vinfo.visual, xswamask, &xswa);

    vo_x11_classhint(mDisplay, mywindow, "xvidix");
    vo_hidecursor(mDisplay, mywindow);

    if (flags & 0x01) /* fullscreen */
	vo_x11_decoration(mDisplay, mywindow, 0);

    XSelectInput(mDisplay, mywindow, StructureNotifyMask);

    /* Tell other applications about this window */
    XSetStandardProperties(mDisplay, mywindow, title, title, None, NULL, 0, &hint);

    /* Map window. */
    XMapWindow(mDisplay, mywindow);
#if 0
#ifdef HAVE_XINERAMA
    vo_x11_xinerama_move(mDisplay, mywindow);
#endif
#endif

    /* Wait for map. */
    do 
    {
    	XNextEvent(mDisplay, &xev);
    }
    while ((xev.type != MapNotify) || (xev.xmap.event != mywindow));

    XSelectInput(mDisplay, mywindow, NoEventMask);

    XGetGeometry(mDisplay, mywindow, &mRoot, &drwX, &drwY, &drwWidth,
	&drwHeight, &drwBorderWidth, &drwDepth);
    drwX = drwY = 0;
    XTranslateCoordinates(mDisplay, mywindow, mRoot, 0, 0, &drwcX, &drwcY, &mRoot);

    window_x = drwcX;
    window_y = drwcY;
    window_width = drwWidth;
    window_height = drwHeight;
    
    mp_msg(MSGT_VO, MSGL_INFO, "[xvidix] image properties: %dx%d depth: %d\n",
	image_width, image_height, image_depth);
    mp_msg(MSGT_VO, MSGL_INFO, "[xvidix] window properties: pos: %dx%d, size: %dx%d\n",
	window_x, window_y, window_width, window_height);

    if (vidix_init(image_width, image_height, window_x, window_y, window_width,
	window_height, format, vo_depthonscreen, vo_screenwidth, vo_screenheight) != 0)
    {
	mp_msg(MSGT_VO, MSGL_FATAL, "Can't initialize VIDIX driver: %s: %s\n",
	    vidix_name, strerror(errno));
	vidix_term();
	return(-1);
    }

    XFlush(mDisplay);
    XSync(mDisplay, False);

    XSelectInput(mDisplay, mywindow, StructureNotifyMask | KeyPressMask );

    saver_off(mDisplay); /* turning off screen saver */

    return(0);
}

static const vo_info_t *get_info(void)
{
    return(&vo_info);
}

static void Terminate_Display_Process(void) 
{
    getchar();	/* wait for enter to remove window */
    vidix_term();
    XDestroyWindow(mDisplay, mywindow);
    XCloseDisplay(mDisplay);
    X_already_started = 0;

    return;
}

static void check_events(void)
{
    const int event = vo_x11_check_events(mDisplay);

    if (event & VO_EVENT_RESIZE)
	resize(vo_dwidth, vo_dheight);
    return;
}

/* draw_osd, flip_page, draw_slice, draw_frame should be
   overwritten with vidix functions (vosub_vidix.c) */
static void draw_osd(void)
{
    mp_msg(MSGT_VO, MSGL_FATAL, "[xvidix] error: didn't used vidix draw_osd!\n");
    return;
}

static void flip_page(void)
{
    mp_msg(MSGT_VO, MSGL_FATAL, "[xvidix] error: didn't used vidix flip_page!\n");
    return;
}

static uint32_t draw_slice(uint8_t *src[], int stride[],
    int w, int h, int x, int y)
{
    mp_msg(MSGT_VO, MSGL_FATAL, "[xvidix] error: didn't used vidix draw_slice!\n");
    return(0);
}

static uint32_t draw_frame(uint8_t *src[])
{
    mp_msg(MSGT_VO, MSGL_FATAL, "[xvidix] error: didn't used vidix draw_frame!\n");
    return(0);
}

static uint32_t query_format(uint32_t format)
{
  static int first = 1;
  if(first)
  {
    first = 0;
    if (vidix_name == (char *)(-1))
    {
	if (vo_subdevice)
	    vidix_name = strdup(vo_subdevice);
	else
	{
    	    mp_msg(MSGT_VO, MSGL_INFO, "No vidix driver name provided, probing available drivers!\n");
	    vidix_name = NULL;
	}
    }

    if (vidix_preinit(vidix_name, &video_out_xvidix) != 0)
    {
	pre_init_err = 1;
	return(0);
    }
  }    
  return pre_init_err ? 0 : vidix_query_fourcc(format);
}


static void uninit(void)
{
#ifdef HAVE_NEW_GUI
    if (vo_window == None)
#endif
    {
	vidix_term();
	saver_on(mDisplay); /* screen saver back on */
	XDestroyWindow(mDisplay, mywindow);
//	XCloseDisplay(mDisplay);
    }
}
