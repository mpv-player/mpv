
//#define SHOW_TIME

/*
 *    video_out_xmga.c
 *
 *      Copyright (C) Zoltan Ponekker - Jan 2001
 *
 *  This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 *
 *  mpeg2dec is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  mpeg2dec is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

LIBVO_EXTERN( xmga )

#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "drivers/mga_vid.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <errno.h>

#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "x11_common.h"
#include "sub.h"
#include "aspect.h"

#ifdef SHOW_TIME
#include "../linux/timer.h"
static unsigned int timer=0;
static unsigned int timerd=0;
#endif

static vo_info_t vo_info =
{
 "X11 (Matrox G200/G4x0/G550 overlay in window using /dev/mga_vid)",
 "xmga",
 "Zoltan Ponekker <pontscho@makacs.poliod.hu>",
 ""
};

//static Display              * mDisplay;
static XGCValues              wGCV;

static XImage               * myximage;

static uint32_t               mDepth, bpp, mode;
static XWindowAttributes      attribs;
static uint32_t               X_already_started=0;

static uint32_t               wndHeight;
static uint32_t               wndWidth;
static uint32_t               wndX;
static uint32_t               wndY;

static uint32_t               fgColor;

static uint32_t               mvHeight;
static uint32_t               mvWidth;

static Window                 mRoot;
static uint32_t               drwX,drwY,drwWidth,drwHeight,drwBorderWidth,drwDepth;
static uint32_t               drwcX,drwcY,dwidth,dheight;

static XSetWindowAttributes   xWAttribs;

#define VO_XMGA
#include "mga_common.c"
#undef  VO_XMGA

static void mDrawColorKey( void )
{
 XSetBackground( mDisplay,vo_gc,0 );
 XClearWindow( mDisplay,vo_window );
 XSetForeground( mDisplay,vo_gc,fgColor );
 XFillRectangle( mDisplay,vo_window,vo_gc,drwX,drwY,drwWidth,(vo_fs?drwHeight - 1:drwHeight) );
 XFlush( mDisplay );
}

static void set_window(){

         XGetGeometry( mDisplay,vo_window,&mRoot,&drwX,&drwY,&drwWidth,&drwHeight,&drwBorderWidth,&drwDepth );
         fprintf( stderr,"[xmga] x: %d y: %d w: %d h: %d\n",drwX,drwY,drwWidth,drwHeight );
         drwX=0; drwY=0; // drwWidth=wndWidth; drwHeight=wndHeight;
         XTranslateCoordinates( mDisplay,vo_window,mRoot,0,0,&drwcX,&drwcY,&mRoot );
         fprintf( stderr,"[xmga] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",drwcX,drwcY,drwX,drwY,drwWidth,drwHeight );

         aspect(&dwidth,&dheight,A_NOZOOM);
         if ( vo_fs )
          {
           aspect(&dwidth,&dheight,A_ZOOM);
           drwX=( vo_screenwidth - (dwidth > vo_screenwidth?vo_screenwidth:dwidth) ) / 2;
           drwcX+=drwX;
           drwY=( vo_screenheight - (dheight > vo_screenheight?vo_screenheight:dheight) ) / 2;
           drwcY+=drwY;
           drwWidth=(dwidth > vo_screenwidth?vo_screenwidth:dwidth);
           drwHeight=(dheight > vo_screenheight?vo_screenheight:dheight);
           fprintf( stderr,"[xmga-fs] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",drwcX,drwcY,drwX,drwY,drwWidth,drwHeight );
          }

         mDrawColorKey();

#ifdef HAVE_XINERAMA
		 if(XineramaIsActive(mDisplay))
		 {
		 	XineramaScreenInfo *screens;
		 	int num_screens;
		 	int i;

		 	screens = XineramaQueryScreens(mDisplay,&num_screens);

		 	/* find the screen we are on */
		 	i = 0;
		 	while(!(screens[i].x_org <= drwcX && screens[i].y_org <= drwcY &&
		 	       screens[i].x_org + screens[i].width >= drwcX &&
		 	       screens[i].y_org + screens[i].height >= drwcY ))
		 	{
		 		i++;
		 	}

		 	/* set drwcX and drwcY to the right values */
		 	drwcX = drwcX - screens[i].x_org;
		 	drwcY = drwcY - screens[i].y_org;
		 	XFree(screens);
		 }

#endif
         mga_vid_config.x_org=drwcX;
         mga_vid_config.y_org=drwcY;
         mga_vid_config.dest_width=drwWidth;
         mga_vid_config.dest_height=drwHeight;

}

static void check_events(void)
{
 int e=vo_x11_check_events(mDisplay);
 if ( !(e&VO_EVENT_RESIZE) && !(e&VO_EVENT_EXPOSE) ) return;
 if(e&VO_EVENT_EXPOSE) mDrawColorKey();
 set_window();
 if ( ioctl( f,MGA_VID_CONFIG,&mga_vid_config ) )
   printf( "Error in mga_vid_config ioctl (wrong mga_vid.o version?)" );
}

static void draw_osd(void)
{ vo_draw_text(mga_vid_config.src_width,mga_vid_config.src_height,draw_alpha);}

static void flip_page(void){
#ifdef SHOW_TIME
    unsigned int t;
    t=GetTimer();
    printf("  [timer: %08X  diff: %6d  dd: %6d ]  \n",t,t-timer,(t-timer)-timerd);
    timerd=t-timer;
    timer=t;
#endif

   vo_mga_flip_page();
}

static uint32_t config( uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format,const vo_tune_info_t* info)
{
 char                 * frame_mem;
// uint32_t               frame_size;
// int                    mScreen;
 unsigned int           fg, bg;
 char                 * mTitle=(title == NULL) ? "XMGA render" : title;
 char                 * name=":0.0";
 XSizeHints             hint;
 XVisualInfo            vinfo;
 XEvent                 xev;

 XGCValues              xgcv;
 unsigned long          xswamask;

  char *devname=vo_subdevice?vo_subdevice:"/dev/mga_vid";

	f = open(devname,O_RDWR);
	if(f == -1)
	{
		perror("open");
		printf("Couldn't open %s\n",devname); 
		return(-1);
	}

 width+=width&1;

 switch(format)
  {
   case IMGFMT_YV12:
	height+=height&1;
        mga_vid_config.format=MGA_VID_FORMAT_YV12;
        mga_vid_config.frame_size=( ( width + 31 ) & ~31 ) * height + ( ( ( width + 31 ) & ~31 ) * height ) / 2;
        break;
   case IMGFMT_I420:
	height+=height&1;
        mga_vid_config.format=MGA_VID_FORMAT_I420;
        mga_vid_config.frame_size=( ( width + 31 ) & ~31 ) * height + ( ( ( width + 31 ) & ~31 ) * height ) / 2;
        break;
   case IMGFMT_IYUV:
	height+=height&1;
        mga_vid_config.format=MGA_VID_FORMAT_IYUV;
        mga_vid_config.frame_size=( ( width + 31 ) & ~31 ) * height + ( ( ( width + 31 ) & ~31 ) * height ) / 2;
        break;
   case IMGFMT_YUY2:
        mga_vid_config.format=MGA_VID_FORMAT_YUY2;
        mga_vid_config.frame_size=( ( width + 31 ) & ~31 ) * height * 2;
        break;
   case IMGFMT_UYVY:
        mga_vid_config.format=MGA_VID_FORMAT_UYVY;
        mga_vid_config.frame_size=( ( width + 31 ) & ~31 ) * height * 2;
        break;
   default:          printf("mga: invalid output format %0X\n",format); return (-1);
  }

 if ( X_already_started ) return -1;

 if (!vo_init()) return -1;

 aspect_save_orig(width,height);
 aspect_save_prescale(d_width,d_height);
 aspect_save_screenres(vo_screenwidth,vo_screenheight);

 mvWidth=width; mvHeight=height;

 wndX=0; wndY=0;
 wndWidth=d_width; wndHeight=d_height;
 vo_fs=fullscreen&1;
 vo_dwidth=d_width; vo_dheight=d_height;
 if ( vo_fs )
  { vo_old_width=d_width; vo_old_height=d_height; }

 switch ( vo_depthonscreen )
  {
   case 32:
   case 24: fgColor=0x00ff00ffL; break;
   case 16: fgColor=0xf81fL; break;
   case 15: fgColor=0x7c1fL; break;
   default: printf( "Sorry, this (%d) color depth not supported.\n",vo_depthonscreen ); return -1;
  }

  aspect(&d_width,&d_height,A_NOZOOM);
#ifdef HAVE_NEW_GUI
 if ( vo_window == None )
  {
#endif
   if ( vo_fs )
    {
     wndWidth=vo_screenwidth;
     wndHeight=vo_screenheight;
#ifdef X11_FULLSCREEN
     aspect(&d_width,&d_height,A_ZOOM);
#endif
    }
   dwidth=d_width; dheight=d_height;

   XGetWindowAttributes( mDisplay,DefaultRootWindow( mDisplay ),&attribs );
   mDepth=attribs.depth;
   if ( mDepth != 15 && mDepth != 16 && mDepth != 24 && mDepth != 32 ) mDepth=24;
   XMatchVisualInfo( mDisplay,mScreen,mDepth,TrueColor,&vinfo );
   xWAttribs.colormap=XCreateColormap( mDisplay,RootWindow( mDisplay,mScreen ),vinfo.visual,AllocNone );
   xWAttribs.background_pixel=0;
   xWAttribs.border_pixel=0;
   xWAttribs.event_mask=StructureNotifyMask | ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask;
   xswamask=CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

    if ( WinID>=0 ){
      vo_window = WinID ? ((Window)WinID) : RootWindow(mDisplay,mScreen);
      XUnmapWindow( mDisplay,vo_window );
      XChangeWindowAttributes( mDisplay,vo_window,xswamask,&xWAttribs);
    } else 
   vo_window=XCreateWindow( mDisplay,RootWindow( mDisplay,mScreen ),
     wndX,wndY,
     wndWidth,wndHeight,
     xWAttribs.border_pixel,
     mDepth,
     InputOutput,
     vinfo.visual,xswamask,&xWAttribs );
   vo_x11_classhint( mDisplay,vo_window,"xmga" );
   vo_hidecursor(mDisplay,vo_window);
   vo_x11_sizehint( wndX,wndY,wndWidth,wndHeight );

   if ( vo_fs ) vo_x11_decoration( mDisplay,vo_window,0 );

   XStoreName( mDisplay,vo_window,mTitle );
   XMapWindow( mDisplay,vo_window );
		   
#ifdef HAVE_XINERAMA
   vo_x11_xinerama_move(mDisplay,vo_window);
#endif
   vo_gc=XCreateGC( mDisplay,vo_window,GCForeground,&wGCV );
#ifdef HAVE_NEW_GUI
  }
#endif

 set_window();

 mga_vid_config.src_width=width;
 mga_vid_config.src_height=height;

 mga_vid_config.colkey_on=1;
 mga_vid_config.colkey_red=255;
 mga_vid_config.colkey_green=0;
 mga_vid_config.colkey_blue=255;

 if(mga_init()) return -1;
 
 set_window();

 XFlush( mDisplay );
 XSync( mDisplay,False );

 saver_off(mDisplay);

 return 0;
}

static const vo_info_t* get_info( void )
{ return &vo_info; }


static void
uninit(void)
{
 saver_on(mDisplay);
#ifdef HAVE_NEW_GUI
 if ( vo_window == None )
#endif
 {
  XDestroyWindow( mDisplay,vo_window );
 }
 mga_uninit();
 printf("vo: uninit!\n");
}

