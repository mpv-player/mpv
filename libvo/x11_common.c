#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/mman.h>

#include "config.h"
#include "video_out.h"

#ifdef X11_FULLSCREEN

#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

int vo_depthonscreen=0;
int vo_screenwidth=0;
int vo_screenheight=0;
int vo_dwidth=0;
int vo_dheight=0;

static int dpms_disabled=0;
static int timeout_save=0;

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
#include "wskeys.h"

extern void mplayer_put_key(int code);

void vo_x11_putkey(int key){
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

void vo_x11_decoration( Display * vo_Display,Window w,int d )
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

int vo_x11_check_events(Display *mydisplay){
 int ret=0;
 XEvent         Event;
 char           buf[100];
 KeySym         keySym;
 XComposeStatus stat;
// unsigned long  vo_KeyTable[512];

 while ( XPending( mydisplay ) )
  {
   XNextEvent( mydisplay,&Event );
   switch( Event.type )
    {
       case Expose:
	     ret|=VO_EVENT_EXPOSE;
             break;
       case ConfigureNotify:
             vo_dwidth=Event.xconfigure.width;
	     vo_dheight=Event.xconfigure.height;
	     ret|=VO_EVENT_RESIZE;
             break;
       case KeyPress:
             XLookupString( &Event.xkey,buf,sizeof(buf),&keySym,&stat );
             vo_x11_putkey( ( (keySym&0xff00) != 0?( (keySym&0x00ff) + 256 ):( keySym ) ) );
	     ret|=VO_EVENT_KEYPRESS;
             break;
    }
  }

  return ret;
}

#endif

void saver_on(Display *mDisplay) {

    int nothing;
    if (dpms_disabled)
    {
	if (DPMSQueryExtension(mDisplay, &nothing, &nothing))
	{
	    printf ("Enabling DPMS\n");
	    DPMSEnable(mDisplay);  // restoring power saving settings
	    DPMSQueryExtension(mDisplay, &nothing, &nothing);
	}
    }
    
    if (timeout_save)
    {
	int dummy, interval, prefer_blank, allow_exp;
	XGetScreenSaver(mDisplay, &dummy, &interval, &prefer_blank, &allow_exp);
	XSetScreenSaver(mDisplay, timeout_save, interval, prefer_blank, allow_exp);
	XGetScreenSaver(mDisplay, &timeout_save, &interval, &prefer_blank, &allow_exp);
    }

}

void saver_off(Display *mDisplay) {

    int interval, prefer_blank, allow_exp, nothing;

    if (DPMSQueryExtension(mDisplay, &nothing, &nothing))
    {
	BOOL onoff;
	CARD16 state;
	DPMSInfo(mDisplay, &state, &onoff);
	if (onoff)
	{
	    printf ("Disabling DPMS\n");
	    dpms_disabled=1;
		DPMSDisable(mDisplay);  // monitor powersave off
	}
    }
    XGetScreenSaver(mDisplay, &timeout_save, &interval, &prefer_blank, &allow_exp);
    if (timeout_save)
	XSetScreenSaver(mDisplay, 0, interval, prefer_blank, allow_exp);
		    // turning off screensaver
}
