
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
 "X11 (Matrox G200/G400 overlay in window using /dev/mga_vid)",
 "xmga",
 "Zoltan Ponekker <pontscho@makacs.poliod.hu>",
 ""
};

//static Display              * mDisplay;
static Window                 mWindow;
static GC                     mGC;
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
static uint32_t               drwcX,drwcY,dwidth,dheight,mFullscreen;

#ifdef HAVE_NEW_GUI
 static uint32_t               mdwidth,mdheight;
#endif

static XSetWindowAttributes   xWAttribs;

#include "mga_common.c"

static void mDrawColorKey( void )
{
 XSetForeground( mDisplay,mGC,fgColor );
 XFillRectangle( mDisplay,mWindow,mGC,drwX,drwY,drwWidth,(mFullscreen?drwHeight - 1:drwHeight) );
 XFlush( mDisplay );
}

static void set_window(){

         #ifdef HAVE_NEW_GUI
          if ( vo_window != None )
           {
            mFullscreen=0;
            dwidth=mdwidth; dheight=mdheight;
            if ( ( vo_dwidth == vo_screenwidth )&&( vo_dheight == vo_screenheight ) )
             {
              mFullscreen=1;
              dwidth=vo_screenwidth;
              dheight=vo_screenwidth * mdheight / mdwidth;
             }
           }
         #endif

         XGetGeometry( mDisplay,mWindow,&mRoot,&drwX,&drwY,&drwWidth,&drwHeight,&drwBorderWidth,&drwDepth );
         drwX=0; drwY=0; // drwWidth=wndWidth; drwHeight=wndHeight;
         XTranslateCoordinates( mDisplay,mWindow,mRoot,0,0,&drwcX,&drwcY,&mRoot );
         fprintf( stderr,"[xmga] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",drwcX,drwcY,drwX,drwY,drwWidth,drwHeight );

         aspect(&dwidth,&dheight,A_NOZOOM);
         if ( mFullscreen )
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

    if(e&VO_EVENT_RESIZE){
         set_window();
         if ( ioctl( f,MGA_VID_CONFIG,&mga_vid_config ) )
          {
           printf( "Error in mga_vid_config ioctl (wrong mga_vid.o version?)" );
//           exit( 0 );
          }

    } else
    if(e&VO_EVENT_EXPOSE) mDrawColorKey();

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

static uint32_t init( uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format )
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

 switch(format)
  {
   case IMGFMT_YV12:
        mga_vid_config.format=MGA_VID_FORMAT_YV12;
        mga_vid_config.frame_size=( ( width + 31 ) & ~31 ) * height + ( ( ( width + 31 ) & ~31 ) * height ) / 2;
        break;
   case IMGFMT_I420:
        mga_vid_config.format=MGA_VID_FORMAT_I420;
        mga_vid_config.frame_size=( ( width + 31 ) & ~31 ) * height + ( ( ( width + 31 ) & ~31 ) * height ) / 2;
        break;
   case IMGFMT_IYUV:
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
 #ifdef HAVE_NEW_GUI
//  mdwidth=d_width; mdheight=d_height;
  mdwidth=width; mdheight=height;
 #endif
 mFullscreen=fullscreen&1;

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
   if ( mFullscreen )
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
   xWAttribs.event_mask=StructureNotifyMask | ExposureMask | KeyPressMask;
   xswamask=CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

    if ( WinID>=0 ){
      mWindow = WinID ? ((Window)WinID) : RootWindow(mDisplay,mScreen);
      XUnmapWindow( mDisplay,mWindow );
      XChangeWindowAttributes( mDisplay,mWindow,xswamask,&xWAttribs);
    } else 
   mWindow=XCreateWindow( mDisplay,RootWindow( mDisplay,mScreen ),
     wndX,wndY,
     wndWidth,wndHeight,
     xWAttribs.border_pixel,
     mDepth,
     InputOutput,
     vinfo.visual,xswamask,&xWAttribs );
   vo_x11_classhint( mDisplay,mWindow,"xmga" );
   vo_hidecursor(mDisplay,mWindow);

   if ( mFullscreen ) vo_x11_decoration( mDisplay,mWindow,0 );

   XGetNormalHints( mDisplay,mWindow,&hint );
   hint.x=wndX; hint.y=wndY;
   hint.width=wndWidth; hint.height=wndHeight;
   hint.base_width=wndWidth; hint.base_height=wndHeight;
   hint.flags=USPosition | USSize;
   XSetNormalHints( mDisplay,mWindow,&hint );
   XStoreName( mDisplay,mWindow,mTitle );
   XMapWindow( mDisplay,mWindow );
#ifdef HAVE_XINERAMA
   vo_x11_xinerama_move(mDisplay,mWindow);
#endif
   mGC=XCreateGC( mDisplay,mWindow,GCForeground,&wGCV );
#ifdef HAVE_NEW_GUI
  }
  else
    {
     mWindow=vo_window;
//     fprintf( stderr,"[xmga] width: %d height: %d d_width: %d d_height: %d\n",width,height,d_width,d_height );
//     if ( vo_screenwidth != d_width )
//      {
//       XMoveWindow( mDisplay,mWindow,( vo_screenwidth - d_width ) / 2,( vo_screenheight - d_height ) / 2 );
//       XResizeWindow( mDisplay,mWindow,d_width,d_height );
//      }
//      else mFullscreen=1;
      mGC=vo_gc; //XCreateGC( mDisplay,mWindow,GCForeground,&wGCV );
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

#ifdef HAVE_NEW_GUI
 if ( vo_window == None )
#endif
 {
   XFlush( mDisplay );
   XSync( mDisplay,False );
 }
 
#ifdef HAVE_NEW_GUI
 if ( vo_window == None )
#endif
  saver_off(mDisplay);

 return 0;
}

static const vo_info_t* get_info( void )
{ return &vo_info; }


static void
uninit(void)
{
#ifdef HAVE_NEW_GUI
 if ( vo_window == None )
#endif
 {
  saver_on(mDisplay);
  XDestroyWindow( mDisplay,mWindow );
 }
 mga_uninit();
 printf("vo: uninit!\n");
}
