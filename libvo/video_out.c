/*
 * video_out.c,
 *
 * Copyright (C) Aaron Holtzman - June 2000
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

#include <unistd.h>
#include <sys/mman.h>

#include "config.h"
#include "video_out.h"

#include "../linux/shmem.h"

//
// Externally visible list of all vo drivers
//

extern vo_functions_t video_out_mga;
extern vo_functions_t video_out_xmga;
extern vo_functions_t video_out_x11;
extern vo_functions_t video_out_xv;
extern vo_functions_t video_out_gl;
extern vo_functions_t video_out_dga;
extern vo_functions_t video_out_sdl;
extern vo_functions_t video_out_3dfx;
extern vo_functions_t video_out_null;
extern vo_functions_t video_out_odivx;
extern vo_functions_t video_out_pgm;
extern vo_functions_t video_out_md5;
extern vo_functions_t video_out_syncfb;

vo_functions_t* video_out_drivers[] =
{
#ifdef HAVE_MGA
#ifdef HAVE_X11
        &video_out_xmga,
#endif
        &video_out_mga,
#endif
#ifdef HAVE_SYNCFB
        &video_out_syncfb,
#endif
#ifdef HAVE_3DFX
        &video_out_3dfx,
#endif
#ifdef HAVE_XV
        &video_out_xv,
#endif
#ifdef HAVE_X11
        &video_out_x11,
#endif
#ifdef HAVE_GL
        &video_out_gl,
#endif
#ifdef HAVE_DGA
        &video_out_dga,
#endif
#ifdef HAVE_SDL
        &video_out_sdl,
#endif
        &video_out_null,
        &video_out_odivx,
        &video_out_pgm,
        &video_out_md5,
        NULL
};

#ifdef X11_FULLSCREEN

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

int vo_depthonscreen=0;
int vo_screenwidth=0;
int vo_screenheight=0;

int vo_init( void )
{
 int       CompletionType = -1;
 int       mScreen;
 int bpp;
 char    * DisplayName = ":0.0";
 Display * mDisplay;
 XImage  * mXImage;
 Window    mRootWin;
 static XWindowAttributes attribs;

 if(vo_depthonscreen) return 1; // already called

 if ( getenv( "DISPLAY" ) ) DisplayName=getenv( "DISPLAY" );
 mDisplay=XOpenDisplay( DisplayName );
 if ( !mDisplay )
  {
   fprintf( stderr,"vo: couldn't open the X11 display!\n" );
   return 0;
  }
 mScreen=DefaultScreen( mDisplay );     // Screen ID.
 mRootWin=RootWindow( mDisplay,mScreen );// Root window ID.
 vo_screenwidth=DisplayWidth( mDisplay,mScreen );
 vo_screenheight=DisplayHeight( mDisplay,mScreen );
 // get color depth:
// XGetWindowAttributes(mydisplay, DefaultRootWindow(mDisplay), &attribs);
 XGetWindowAttributes(mDisplay, mRootWin, &attribs);
 vo_depthonscreen=attribs.depth;
 // get bits/pixel:
   mXImage=XGetImage( mDisplay,mRootWin,0,0,1,1,AllPlanes,ZPixmap );
   bpp=mXImage->bits_per_pixel;
   XDestroyImage( mXImage );
 if((vo_depthonscreen+7)/8 != (bpp+7)/8) vo_depthonscreen=bpp; // by A'rpi
 XCloseDisplay( mDisplay );
 printf("X11 running at %dx%d depth: %d\n",vo_screenwidth,vo_screenheight,vo_depthonscreen);
 return 1;
}

#include "../linux/keycodes.h"
extern void mplayer_put_key(int code);

void vo_keyboard( int key )
{
 switch ( key )
  {
   case wsLeft:      mplayer_put_key(KEY_LEFT); break;
   case wsRight:     mplayer_put_key(KEY_RIGHT); break;
   case wsUp:        mplayer_put_key(KEY_UP); break;
   case wsDown:      mplayer_put_key(KEY_DOWN); break;
   case wsSpace:     mplayer_put_key(' '); break;
   case wsEscape:    mplayer_put_key(KEY_ESC); break;
   case wsEnter:     mplayer_put_key(KEY_ENTER); break;
   case wsq:
   case wsQ:         mplayer_put_key('q'); break;
   case wsp:
   case wsP:         mplayer_put_key('p'); break;
   case wsMinus:
   case wsGrayMinus: mplayer_put_key('-'); break;
   case wsPlus:
   case wsGrayPlus:  mplayer_put_key('+'); break;
  }
}


// ----- Motif header: -------

#define MWM_HINTS_DECORATIONS   2

typedef struct
{
  long flags;
  long functions;
  long decorations;
  long input_mode;
} MotifWmHints;

extern MotifWmHints vo_MotifWmHints;
extern Atom         vo_MotifHints;
extern int          vo_depthonscreen;
extern int          vo_screenwidth;
extern int          vo_screenheight;

static MotifWmHints   vo_MotifWmHints;
static Atom           vo_MotifHints  = None;

void vo_decoration( Display * vo_Display,Window w,int d )
{
 vo_MotifHints=XInternAtom( vo_Display,"_MOTIF_WM_HINTS",0 );
 if ( vo_MotifHints != None )
  {
   vo_MotifWmHints.flags=2;
   vo_MotifWmHints.decorations=d;
   XChangeProperty( vo_Display,w,vo_MotifHints,vo_MotifHints,32,
                    PropModeReplace,(unsigned char *)&vo_MotifWmHints,4 );
  }
}

#include <signal.h>

int vo_eventhandler_pid=-1;

void vo_kill_eventhandler(){
	if(vo_eventhandler_pid!=-1) kill(vo_eventhandler_pid,SIGTERM);

}

#endif

