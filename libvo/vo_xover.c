/*
 * XOver a general x11 vo for MPlayer overlay drivers based on:
 * VIDIX-accelerated overlay in an X window
 *
 * copyright (C) Alex Beregszaszi & Zoltan Ponekker & Nick Kurshev
 *
 * WS window manager by Pontscho/Fresh!
 *
 * based on vo_gl.c and vo_vesa.c and vo_xmga.c (.so mastah! ;))
 *
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
#include <string.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>

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

#ifdef CONFIG_GUI
#include "gui/interface.h"
#endif


static const vo_info_t info =
{
    "General X11 driver for overlay capable video output drivers",
    "xover",
    "Albeu",
    ""
};

LIBVO_EXTERN(xover)

#define UNUSED(x) ((void)(x)) /* Removes warning about unused arguments */

/* X11 related variables */
/* Colorkey handling */
static int colorkey;

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

static const vo_functions_t* sub_vo = NULL;


static void set_window(int force_update)
{
  Window mRoot;
  if ( WinID )
    {
      XGetGeometry(mDisplay, vo_window, &mRoot, &drwX, &drwY, &drwWidth,
		   &drwHeight, &drwBorderWidth, &drwDepth);
      drwX = drwY = 0;

      XTranslateCoordinates(mDisplay, vo_window, mRoot, 0, 0,
			    &drwcX, &drwcY, &mRoot);
      aspect(&dwidth,&dheight,A_NOZOOM);
      if (!vo_fs)
	mp_msg(MSGT_VO, MSGL_V, "[xvidix] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",
	       drwcX, drwcY, drwX, drwY, drwWidth, drwHeight);

      /* following stuff copied from vo_xmga.c */
    }
  else
    {
      aspect(&dwidth,&dheight,A_NOZOOM);
      drwcX=drwX=vo_dx; drwcY=drwY=vo_dy; drwWidth=vo_dwidth; drwHeight=vo_dheight;
    }

#if X11_FULLSCREEN
  if (vo_fs)
    {
      aspect(&dwidth,&dheight,A_ZOOM);
      drwX = (vo_screenwidth - ((int)dwidth > vo_screenwidth ? vo_screenwidth : dwidth)) / 2;
      drwcX = drwX;
      drwY = (vo_screenheight - ((int)dheight > vo_screenheight ? vo_screenheight : dheight)) / 2;
      drwcY = drwY;
      drwWidth = ((int)dwidth > vo_screenwidth ? vo_screenwidth : dwidth);
      drwHeight = ((int)dheight > vo_screenheight ? vo_screenheight : dheight);
      mp_msg(MSGT_VO, MSGL_V, "[xvidix-fs] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",
	     drwcX, drwcY, drwX, drwY, drwWidth, drwHeight);
    }
#endif

  vo_dwidth=drwWidth; vo_dheight=drwHeight;

#ifdef CONFIG_XINERAMA
  if (XineramaIsActive(mDisplay))
    {
      XineramaScreenInfo *screens;
      int num_screens;
      int i = 0;

      screens = XineramaQueryScreens(mDisplay, &num_screens);

      /* find the screen we are on */
      while (i<num_screens &&
	     ((screens[i].x_org < (int)drwcX) ||
	      (screens[i].y_org < (int)drwcY) ||
	      (screens[i].x_org + screens[i].width >= (int)drwcX) ||
	      (screens[i].y_org + screens[i].height >= (int)drwcY)))
	{
	  i++;
	}

      if(i<num_screens)
	{
	  /* save the screen we are on */
	  xinerama_screen = i;
	} else {
	  /* oops.. couldnt find the screen we are on
	   * because the upper left corner left the
	   * visual range. assume we are still on the
	   * same screen
	   */
	  i = xinerama_screen;
	}

      /* set drwcX and drwcY to the right values */
      drwcX = drwcX - screens[i].x_org;
      drwcY = drwcY - screens[i].y_org;
      XFree(screens);
    }
#endif

  if ( vo_panscan > 0.0f && vo_fs )
    {
      drwcX-=vo_panscan_x >> 1;
      drwcY-=vo_panscan_y >> 1;
      drwX-=vo_panscan_x >> 1;
      drwY-=vo_panscan_y >> 1;
      drwWidth+=vo_panscan_x;
      drwHeight+=vo_panscan_y;
    }

  /* set new values in VIDIX */
  if (force_update || (window_x != drwcX) || (window_y != drwcY) ||
      (window_width != drwWidth) || (window_height != drwHeight))
    {
      mp_win_t w;
      // do a backup of window coordinates
      w.x = window_x = drwcX;
      w.y = window_y = drwcY;
      vo_dx = drwcX;
      vo_dy = drwcY;
      w.w = window_width = drwWidth;
      w.h = window_height = drwHeight;

      if(sub_vo->control(VOCTRL_XOVERLAY_SET_WIN,&w) != VO_TRUE)
	mp_msg(MSGT_VO, MSGL_ERR, "xvidx: set_overlay failed\n");

      mp_msg(MSGT_VO, MSGL_V, "[xvidix] window properties: pos: %dx%d, size: %dx%d\n", vo_dx, vo_dy, window_width, window_height);
    }

  /* mDrawColorKey: */

  /* fill drawable with specified color */
  XSetBackground(mDisplay, vo_gc, 0L);
  XClearWindow( mDisplay,vo_window );
  XSetForeground(mDisplay, vo_gc, colorkey);
  XFillRectangle(mDisplay, vo_window, vo_gc, drwX, drwY, drwWidth,
		 (vo_fs ? drwHeight - 1 : drwHeight));

  /* flush, update drawable */
  XFlush(mDisplay);

  return;
}

/* connect to server, create and map window,
 * allocate colors and (shared) memory
 */
static int config(uint32_t width, uint32_t height, uint32_t d_width,
		       uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
  XVisualInfo vinfo;
  //    XSizeHints hint;
  XSetWindowAttributes xswa;
  unsigned long xswamask;
  XWindowAttributes attribs;
  int window_depth, r, g, b;
  mp_colorkey_t colork;
  char _title[255];

  sprintf(_title,"MPlayer %s X11 Overlay",sub_vo->info->name);
  title = _title;

  panscan_init();

  image_height = height;
  image_width = width;
  image_format = format;

  aspect_save_orig(width, height);
  aspect_save_prescale(d_width, d_height);
  update_xinerama_info();

  window_width = d_width;
  window_height = d_height;

  r = (vo_colorkey & 0x00ff0000) >> 16;
  g = (vo_colorkey & 0x0000ff00) >> 8;
  b = vo_colorkey & 0x000000ff;
  switch(vo_depthonscreen)
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
      mp_msg(MSGT_VO, MSGL_ERR, "Sorry, this (%d) color depth is not supported\n",
	     vo_depthonscreen);
    }
  mp_msg(MSGT_VO, MSGL_V, "Using colorkey: %x\n", colorkey);

  aspect(&d_width, &d_height, A_NOZOOM);

  vo_dx=( vo_screenwidth - d_width ) / 2; vo_dy=( vo_screenheight - d_height ) / 2;
  vo_dx += xinerama_x;
  vo_dy += xinerama_y;
  vo_dwidth=d_width; vo_dheight=d_height;

#ifdef CONFIG_GUI
  if(use_gui) guiGetEvent( guiSetShVideo,0 ); // the GUI will set up / resize the window
  else
    {
#endif

#ifdef X11_FULLSCREEN
      if ( ( flags&VOFLAG_FULLSCREEN )||(flags & VOFLAG_SWSCALE) ) aspect(&d_width, &d_height, A_ZOOM);
#endif
      dwidth = d_width;
      dheight = d_height;
      /* Make the window */
      XGetWindowAttributes(mDisplay, DefaultRootWindow(mDisplay), &attribs);

      /* from vo_x11 */
      window_depth = attribs.depth;
      if ((window_depth != 15) && (window_depth != 16) && (window_depth != 24)
	  && (window_depth != 32))
        window_depth = 24;
      XMatchVisualInfo(mDisplay, mScreen, window_depth, TrueColor, &vinfo);

      xswa.background_pixel = BlackPixel(mDisplay, mScreen);
      xswa.border_pixel     = 0;
      xswa.colormap         = XCreateColormap(mDisplay, RootWindow(mDisplay, mScreen),
					      vinfo.visual, AllocNone);
      xswamask = CWBackPixel | CWBorderPixel | CWColormap;

	    vo_x11_create_vo_window(&vinfo, vo_dx, vo_dy,
                  window_width, window_height, flags,
	          xswa.colormap, "xvidix", title);
	    XChangeWindowAttributes(mDisplay, vo_window, xswamask, &xswa);

#ifdef CONFIG_GUI
    }
#endif

  if ( ( !WinID )&&( flags&VOFLAG_FULLSCREEN ) ) { vo_dx=0; vo_dy=0; vo_dwidth=vo_screenwidth; vo_dheight=vo_screenheight; vo_fs=1; }

  if(sub_vo->config(image_width,image_height,vo_dwidth,vo_dheight,
		    flags | VOFLAG_XOVERLAY_SUB_VO,NULL,format)) {
    mp_msg(MSGT_VO, MSGL_ERR, "xover: sub vo config failed\n");
    return 1;
  }
  colork.x11 = colorkey;
  colork.r = r;
  colork.g = g;
  colork.b = b;
  if(sub_vo->control(VOCTRL_XOVERLAY_SET_COLORKEY,&colork) != VO_TRUE)
    mp_msg(MSGT_VO, MSGL_WARN, "xover: set_colorkey failed\n");

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
  sub_vo->check_events();
  return;
}

/* draw_osd, flip_page, draw_slice, draw_frame should be
   overwritten with vidix functions (vosub_vidix.c) */
static void draw_osd(void)
{
  mp_msg(MSGT_VO, MSGL_FATAL, "xover error: didn't used sub vo draw_osd!\n");
}

static void flip_page(void)
{
  mp_msg(MSGT_VO, MSGL_FATAL, "xover error: didn't used sub vo flip_page!\n");
}

static int draw_slice(uint8_t *src[], int stride[],
			   int w, int h, int x, int y)
{
  UNUSED(src);
  UNUSED(stride);
  UNUSED(w);
  UNUSED(h);
  UNUSED(x);
  UNUSED(y);
  mp_msg(MSGT_VO, MSGL_FATAL, "xover error: didn't used sub vo draw_slice!\n");
  return 1;
}

static int draw_frame(uint8_t *src[])
{
  UNUSED(src);
  mp_msg(MSGT_VO, MSGL_FATAL, "xover error: didn't used sub vo draw_frame!\n");
  return 1;
}

static void uninit(void)
{
  if(!vo_config_count) return;
  if(sub_vo) sub_vo->uninit();
  sub_vo = NULL;
  vo_x11_uninit();
  // Restore our callbacks
  video_out_xover.draw_frame = draw_frame;
  video_out_xover.draw_slice = draw_slice;
  video_out_xover.flip_page = flip_page;
  video_out_xover.draw_osd  = draw_osd;
}

static int preinit(const char *arg)
{
  int i;

  if(!arg) {
    mp_msg(MSGT_VO, MSGL_ERR, "VO XOverlay need a subdriver\n");
    return 1;
  }

  for(i = 0 ; video_out_drivers[i] != NULL ; i++) {
    if(!strcmp(video_out_drivers[i]->info->short_name,arg) &&
       strcmp(video_out_drivers[i]->info->short_name,"xover"))
      break;
  }
  if(!video_out_drivers[i]) {
    mp_msg(MSGT_VO, MSGL_ERR, "VO XOverlay: Subdriver %s not found\n", arg);
    return 1;
  }
  if(video_out_drivers[i]->control(VOCTRL_XOVERLAY_SUPPORT,NULL) != VO_TRUE) {
    mp_msg(MSGT_VO, MSGL_ERR, "VO XOverlay: %s doesn't support XOverlay\n", arg);
    return 1;
  }
  // X11 init
  if (!vo_init()) return VO_FALSE;
  if(video_out_drivers[i]->preinit(NULL)) {
    mp_msg(MSGT_VO, MSGL_ERR, "VO XOverlay: Subvo init failed\n");
    return 1;
  }
  sub_vo = video_out_drivers[i];
  // Setup the sub vo callbacks
  video_out_xover.draw_frame = sub_vo->draw_frame;
  video_out_xover.draw_slice = sub_vo->draw_slice;
  video_out_xover.flip_page = sub_vo->flip_page;
  video_out_xover.draw_osd  = sub_vo->draw_osd;
  return 0;
}

static int control(uint32_t request, void *data, ...)
{
  if(!sub_vo) return VO_ERROR;
  switch (request) {
  case VOCTRL_GUISUPPORT:
    return VO_TRUE;
  case VOCTRL_GET_PANSCAN:
    if ( !vo_config_count || !vo_fs ) return VO_FALSE;
    return VO_TRUE;
  case VOCTRL_ONTOP:
    vo_x11_ontop();
    return VO_TRUE;
  case VOCTRL_FULLSCREEN:
    vo_x11_fullscreen();
  case VOCTRL_SET_PANSCAN:
    if ( vo_fs && ( vo_panscan != vo_panscan_amount ) )
      {
	panscan_calc();
	set_window(0);
      }
    return VO_TRUE;
  default:
    // Safe atm bcs nothing use more than 1 arg
    return sub_vo->control(request,data);
  }
  return VO_NOTIMPL;
}
