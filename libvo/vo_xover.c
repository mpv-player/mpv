/*
  XOver a general x11 vo for mplayer overlay drivers based on :
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
#include <unistd.h>
#include <sys/ioctl.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
//#include <X11/keysym.h>

#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "x11_common.h"
#include "aspect.h"
#include "mp_msg.h"

#include "../mplayer.h" /* exit_player() */

#ifdef HAVE_NEW_GUI
#include "../Gui/interface.h"
#endif


static vo_info_t info = 
{
    "General X11 driver for overlay capable vo's",
    "xover",
    "Albeu",
    ""
};

LIBVO_EXTERN(xover)

#define UNUSED(x) ((void)(x)) /* Removes warning about unused arguments */

/* X11 related variables */
/* Colorkey handling */
static XGCValues mGCV;
static uint32_t	fgColor;
static uint32_t bgColor;

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

#ifdef HAVE_XINERAMA
extern int xinerama_screen;
#endif

static vo_functions_t* sub_vo = NULL;


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

#ifdef HAVE_XINERAMA
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
  XSetBackground( mDisplay,vo_gc,bgColor );
  XClearWindow( mDisplay,vo_window );
  XSetForeground(mDisplay, vo_gc, fgColor);
  XFillRectangle(mDisplay, vo_window, vo_gc, drwX, drwY, drwWidth,
		 (vo_fs ? drwHeight - 1 : drwHeight));
  /* flush, update drawable */
  XFlush(mDisplay);

  return;
}

/* connect to server, create and map window,
 * allocate colors and (shared) memory
 */
static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width,
		       uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
  XVisualInfo vinfo;
  //    XSizeHints hint;
  XSetWindowAttributes xswa;
  unsigned long xswamask;
  XWindowAttributes attribs;
  int window_depth;
  mp_colorkey_t colork;
  char _title[255];

  sprintf(_title,"MPlayer %s X11 Overlay",sub_vo->info->name);
  title = _title;

  panscan_init();

  image_height = height;
  image_width = width;
  image_format = format;
  vo_mouse_autohide=1;

  aspect_save_orig(width, height);
  aspect_save_prescale(d_width, d_height);
  aspect_save_screenres(vo_screenwidth, vo_screenheight);

  vo_dx = 0;
  vo_dy = 0;
  window_width = d_width;
  window_height = d_height;

  /* from xmga.c */
  bgColor = 0x0L;
  switch(vo_depthonscreen)
    {
    case 32:
    case 24:
      fgColor = 0x00ff00ffL;
      break;
    case 16:
      fgColor = 0xf81fL;
      break;
    case 15:
      fgColor = 0x7c1fL;
      break;
    default:
      mp_msg(MSGT_VO, MSGL_ERR, "Sorry, this (%d) color depth is not supported\n",
	     vo_depthonscreen);
    }

  aspect(&d_width, &d_height, A_NOZOOM);

  vo_dx=( vo_screenwidth - d_width ) / 2; vo_dy=( vo_screenheight - d_height ) / 2;    
  vo_dwidth=d_width; vo_dheight=d_height;

#ifdef HAVE_NEW_GUI
  if(use_gui) guiGetEvent( guiSetShVideo,0 ); // the GUI will set up / resize the window
  else
    {
#endif

#ifdef X11_FULLSCREEN
      if ( ( flags&1 )||(flags & 0x04) ) aspect(&d_width, &d_height, A_ZOOM);
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
      xswa.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask | PropertyChangeMask |
	((WinID==0)?0:(ButtonPressMask | ButtonReleaseMask | PointerMotionMask));
      xswamask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

      if (WinID >= 0)
	{
	  vo_window = WinID ? ((Window)WinID) : RootWindow(mDisplay, mScreen);
	  if ( WinID )
	    {
	      XUnmapWindow(mDisplay, vo_window);
	      XChangeWindowAttributes(mDisplay, vo_window, xswamask, &xswa);
	      vo_x11_selectinput_witherr( mDisplay,vo_window,StructureNotifyMask | KeyPressMask | PropertyChangeMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask | ExposureMask );
	    } else XSelectInput( mDisplay,vo_window,ExposureMask );
	}
      else
	{
	  if ( vo_window == None )
	    {
	      vo_window = XCreateWindow(mDisplay, RootWindow(mDisplay, mScreen),
					vo_dx, vo_dy, window_width, window_height, xswa.border_pixel,
					vinfo.depth, InputOutput, vinfo.visual, xswamask, &xswa);

	      vo_x11_classhint(mDisplay, vo_window, "xvidix");
	      vo_hidecursor(mDisplay, vo_window);
	      vo_x11_sizehint( vo_dx,vo_dy,vo_dwidth,vo_dheight,0 );

	      XStoreName(mDisplay, vo_window, title);
	      XMapWindow(mDisplay, vo_window);
    
	      if ( flags&1 ) vo_x11_fullscreen();
    
#ifdef HAVE_XINERAMA
	      vo_x11_xinerama_move(mDisplay, vo_window);
#endif
	    } else if ( !(flags&1) ) XMoveResizeWindow( mDisplay,vo_window,vo_dx,vo_dy,vo_dwidth,vo_dheight );
	}
	 
      if ( vo_gc != None ) XFreeGC( mDisplay,vo_gc );
      vo_gc = XCreateGC(mDisplay, vo_window, GCForeground, &mGCV);
#ifdef HAVE_NEW_GUI
    }
#endif

  if ( ( !WinID )&&( flags&1 ) ) { vo_dx=0; vo_dy=0; vo_dwidth=vo_screenwidth; vo_dheight=vo_screenheight; vo_fs=1; }

  if(sub_vo->config(image_width,image_height,vo_dwidth,vo_dheight,
		    flags | VOFLAG_XOVERLAY_SUB_VO,NULL,format)) {
    mp_msg(MSGT_VO, MSGL_ERR, "xover: sub vo config failed\n");
    return 1;
  }
  colork.x11 = fgColor;
  colork.r = 255;
  colork.g = 0;
  colork.b = 255;
  if(sub_vo->control(VOCTRL_XOVERLAY_SET_COLORKEY,&colork) != VO_TRUE)
    mp_msg(MSGT_VO, MSGL_WARN, "xover: set_colorkey failed\n");

  set_window(1);

  XFlush(mDisplay);
  XSync(mDisplay, False);

  panscan_calc();

  saver_off(mDisplay); /* turning off screen saver */
    
  vo_config_count++;

  return(0);
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

static uint32_t draw_slice(uint8_t *src[], int stride[],
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

static uint32_t draw_frame(uint8_t *src[])
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
  saver_on(mDisplay); /* screen saver back on */
  vo_x11_uninit();
  // Restore our callbacks
  video_out_xover.draw_frame = draw_frame;
  video_out_xover.draw_slice = draw_slice;
  video_out_xover.flip_page = flip_page;
  video_out_xover.draw_osd  = draw_osd;
}

static uint32_t preinit(const char *arg)
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
    mp_msg(MSGT_VO, MSGL_ERR, "VO XOverlay: Subdriver %s not found\n");
    return 1;
  }
  if(video_out_drivers[i]->control(VOCTRL_XOVERLAY_SUPPORT,NULL) != VO_TRUE) {
    mp_msg(MSGT_VO, MSGL_ERR, "VO XOverlay: %s doesn't support XOverlay\n");
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

static uint32_t control(uint32_t request, void *data, ...)
{
  if(!sub_vo) return VO_ERROR;
  switch (request) {
  case VOCTRL_GUISUPPORT:
    return VO_TRUE;
  case VOCTRL_GET_PANSCAN:
    if ( !vo_config_count || !vo_fs ) return VO_FALSE;
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
