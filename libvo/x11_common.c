
#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#ifdef X11_FULLSCREEN

#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "video_out.h"

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

/*
 * If SCAN_VISUALS is defined, vo_init() scans all available TrueColor
 * visuals for the 'best' visual for MPlayer video display.  Note that
 * the 'best' visual might be different from the default visual that
 * is in use on the root window of the display/screen.
 */
#define	SCAN_VISUALS


extern verbose;

static int dpms_disabled=0;
static int timeout_save=0;

char* mDisplayName=NULL;
Display* mDisplay;
Window   mRootWin;
int mScreen;
int mLocalDisplay;


void vo_hidecursor ( Display *disp , Window win )
{
	Cursor no_ptr;
	Pixmap bm_no;
	XColor black,dummy;
	Colormap colormap;
	static unsigned char bm_no_data[] = { 0,0,0,0, 0,0,0,0  };
	
	colormap = DefaultColormap(disp,DefaultScreen(disp));
	XAllocNamedColor(disp,colormap,"black",&black,&dummy);	
	bm_no = XCreateBitmapFromData(disp, win, bm_no_data, 8,8);    
	no_ptr=XCreatePixmapCursor(disp, bm_no, bm_no,&black, &black,0, 0);									          
	XDefineCursor(disp,win,no_ptr);
}


#ifdef	SCAN_VISUALS
/*
 * Scan the available visuals on this Display/Screen.  Try to find
 * the 'best' available TrueColor visual that has a decent color
 * depth (at least 15bit).  If there are multiple visuals with depth
 * >= 15bit, we prefer visuals with a smaller color depth.
 */
int vo_find_depth_from_visuals(Display *dpy, int screen, Visual **visual_return)
{
  XVisualInfo visual_tmpl;
  XVisualInfo *visuals;
  int nvisuals, i;
  int bestvisual = -1;
  int bestvisual_depth = -1;

  visual_tmpl.screen = screen;
  visual_tmpl.class = TrueColor;
  visuals = XGetVisualInfo(dpy,
			   VisualScreenMask | VisualClassMask, &visual_tmpl,
			   &nvisuals);
  if (visuals != NULL) {
    for (i = 0; i < nvisuals; i++) {
      if (verbose)
	printf("vo: X11 truecolor visual %#x, depth %d, R:%lX G:%lX B:%lX\n",
	       visuals[i].visualid, visuals[i].depth,
	       visuals[i].red_mask, visuals[i].green_mask,
	       visuals[i].blue_mask);
      /*
       * save the visual index and it's depth, if this is the first
       * truecolor visul, or a visual that is 'preferred' over the
       * previous 'best' visual
       */
      if (bestvisual_depth == -1
	  || (visuals[i].depth >= 15 
	      && (   visuals[i].depth < bestvisual_depth
		  || bestvisual_depth < 15))) {
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
#endif


int vo_init( void )
{
// int       mScreen;
 int depth, bpp;
 unsigned int mask;
// char    * DisplayName = ":0.0";
// Display * mDisplay;
 XImage  * mXImage = NULL;
// Window    mRootWin;
 XWindowAttributes attribs;

 if(vo_depthonscreen) return 1; // already called

 if (!mDisplayName)
   if (!(mDisplayName=getenv("DISPLAY")))
     mDisplayName=strdup(":0.0");

 mDisplay=XOpenDisplay(mDisplayName);
 if ( !mDisplay )
  {
   printf( "vo: couldn't open the X11 display (%s)!\n",mDisplayName );
   return 0;
  }
 mScreen=DefaultScreen( mDisplay );     // Screen ID.
 mRootWin=RootWindow( mDisplay,mScreen );// Root window ID.

#ifdef HAVE_XINERAMA
 if(XineramaIsActive(mDisplay))
  {
  XineramaScreenInfo *screens;
  int num_screens;
  screens = XineramaQueryScreens(mDisplay, &num_screens);
  vo_screenwidth=screens[0].width;
  vo_screenheight=screens[0].height;
  }
 else
#endif
 {
 if (! vo_screenwidth)
   vo_screenwidth=DisplayWidth( mDisplay,mScreen );
 if (! vo_screenheight)
   vo_screenheight=DisplayHeight( mDisplay,mScreen );
 }
 // get color depth (from root window, or the best visual):
 XGetWindowAttributes(mDisplay, mRootWin, &attribs);
 depth=attribs.depth;

#ifdef	SCAN_VISUALS
 if (depth != 15 && depth != 16 && depth != 24 && depth != 32) {
   Visual *visual;

   depth = vo_find_depth_from_visuals(mDisplay, mScreen, &visual);
   if (depth != -1)
     mXImage=XCreateImage(mDisplay, visual, depth, ZPixmap,
			  0, NULL, 1, 1, 8, 1);
 } else
#endif
 mXImage=XGetImage( mDisplay,mRootWin,0,0,1,1,AllPlanes,ZPixmap );

 vo_depthonscreen = depth;	// display depth on screen

 // get bits/pixel from XImage structure:
 if (mXImage == NULL) {
   mask = 0;
 } else {
   /*
    * for the depth==24 case, the XImage structures might use
    * 24 or 32 bits of data per pixel.  The global variable
    * vo_depthonscreen stores the amount of data per pixel in the
    * XImage structure!
    *
    * Maybe we should rename vo_depthonscreen to (or add) vo_bpp?
    */
   bpp=mXImage->bits_per_pixel;
   if((vo_depthonscreen+7)/8 != (bpp+7)/8) vo_depthonscreen=bpp; // by A'rpi
   mask=mXImage->red_mask|mXImage->green_mask|mXImage->blue_mask;
   XDestroyImage( mXImage );
   if(verbose)
     printf("vo: X11 color mask:  %X  (R:%lX G:%lX B:%lX)\n",
	    mask,mXImage->red_mask,mXImage->green_mask,mXImage->blue_mask);
 }
 if(((vo_depthonscreen+7)/8)==2){
   if(mask==0x7FFF) vo_depthonscreen=15; else
   if(mask==0xFFFF) vo_depthonscreen=16;
 }
// XCloseDisplay( mDisplay );
/* slightly improved local display detection AST */
 if ( strncmp(mDisplayName, "unix:", 5) == 0)
		mDisplayName += 4;
 else if ( strncmp(mDisplayName, "localhost:", 10) == 0)
		mDisplayName += 9;
 if (*mDisplayName==':') mLocalDisplay=1; else mLocalDisplay=0;
 printf("vo: X11 running at %dx%d with depth %d and %d bits/pixel (\"%s\" => %s display)\n",
	vo_screenwidth,vo_screenheight,
	depth, vo_depthonscreen,
	mDisplayName,mLocalDisplay?"local":"remote");
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
   case wsPageUp:    mplayer_put_key(KEY_PAGE_UP); break;
   case wsPageDown:  mplayer_put_key(KEY_PAGE_DOWN); break;
   case wsq:
   case wsQ:         mplayer_put_key('q'); break;
   case wsp:
   case wsP:         mplayer_put_key('p'); break;
   case wsMinus:
   case wsGrayMinus: mplayer_put_key('-'); break;
   case wsPlus:
   case wsGrayPlus:  mplayer_put_key('+'); break;
   case wsGrayMul:
   case wsMul:       mplayer_put_key('*'); break;
   case wsGrayDiv:
   case wsDiv:       mplayer_put_key('/'); break;
   case wsm:
   case wsM:	     mplayer_put_key('m'); break;
   case wso:
   case wsO:         mplayer_put_key('o'); break;
   default: if((key>='a' && key<='z')||(key>='A' && key<='Z')||
	       (key>='0' && key<='9')) mplayer_put_key(key);
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

extern MotifWmHints vo_MotifWmHints;
extern Atom         vo_MotifHints;
extern int          vo_depthonscreen;
extern int          vo_screenwidth;
extern int          vo_screenheight;

static MotifWmHints   vo_MotifWmHints;
static Atom           vo_MotifHints  = None;

// Note: always d==0 !
void vo_x11_decoration( Display * vo_Display,Window w,int d )
{

  if(vo_fsmode&1){
    XSetWindowAttributes attr;
    attr.override_redirect = True;
    XChangeWindowAttributes(vo_Display, w, CWOverrideRedirect, &attr);
//    XMapWindow(vo_Display], w);
  }

  if(vo_fsmode&8){
    XSetTransientForHint (vo_Display, w, RootWindow(vo_Display,mScreen));
  }

 vo_MotifHints=XInternAtom( vo_Display,"_MOTIF_WM_HINTS",0 );
 if ( vo_MotifHints != None )
  {
   memset( &vo_MotifWmHints,0,sizeof( MotifWmHints ) );
   vo_MotifWmHints.flags=MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS;
   vo_MotifWmHints.functions=MWM_FUNC_MOVE | MWM_FUNC_CLOSE | MWM_FUNC_MINIMIZE | MWM_FUNC_MAXIMIZE;
   if ( d ) d=MWM_DECOR_ALL;
   vo_MotifWmHints.decorations=d|((vo_fsmode&2)?0:MWM_DECOR_MENU);
   XChangeProperty( vo_Display,w,vo_MotifHints,vo_MotifHints,32,
                    PropModeReplace,(unsigned char *)&vo_MotifWmHints,(vo_fsmode&4)?4:5 );
  }
}

void vo_x11_classhint( Display * display,Window window,char *name ){
	    XClassHint wmClass;
	    wmClass.res_name = name;
	    wmClass.res_class = "MPlayer";
	    XSetClassHint(display,window,&wmClass);
}

#ifdef HAVE_NEW_GUI
 Window    vo_window = None;
 GC        vo_gc;
 int       vo_xeventhandling = 1;
 int       vo_resize = 0;
 int       vo_expose = 0;

 void vo_setwindow( Window w,GC g ) {
   vo_window=w; vo_gc=g;
   vo_xeventhandling=0;
 }
 void vo_setwindowsize( int w,int h ) {
    vo_dwidth=w; vo_dheight=h;
 }
#endif

int vo_x11_check_events(Display *mydisplay){
 int ret=0;
 XEvent         Event;
 char           buf[100];
 KeySym         keySym;
 static XComposeStatus stat;
// unsigned long  vo_KeyTable[512];

#ifdef HAVE_NEW_GUI
 if ( vo_xeventhandling )
   {
#endif
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
#ifdef HAVE_NEW_GUI
    }
    else
     {
      if ( vo_resize )
       {
        vo_resize=0;
        ret|=VO_EVENT_RESIZE;
       }
      if ( vo_expose )
       {
        vo_expose=0;
        ret|=VO_EVENT_EXPOSE;
       }
     }
#endif

  return ret;
}

void saver_on(Display *mDisplay) {

#ifdef HAVE_XDPMS
    int nothing;
    if (dpms_disabled)
    {
	if (DPMSQueryExtension(mDisplay, &nothing, &nothing))
	{
	    if (!DPMSEnable(mDisplay)) {  // restoring power saving settings
                printf("DPMS not available?\n");
            } else {
                // DPMS does not seem to be enabled unless we call DPMSInfo
	        BOOL onoff;
        	CARD16 state;
        	DPMSInfo(mDisplay, &state, &onoff);
                if (onoff) {
	            printf ("Successfully enabled DPMS\n");
                } else {
	            printf ("Could not enable DPMS\n");
                }
            }
	}
    }
#endif

    if (timeout_save)
    {
	int dummy, interval, prefer_blank, allow_exp;
	XGetScreenSaver(mDisplay, &dummy, &interval, &prefer_blank, &allow_exp);
	XSetScreenSaver(mDisplay, timeout_save, interval, prefer_blank, allow_exp);
	XGetScreenSaver(mDisplay, &timeout_save, &interval, &prefer_blank, &allow_exp);
    }

}

void saver_off(Display *mDisplay) {

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
	    printf ("Disabling DPMS\n");
	    dpms_disabled=1;
	    stat = DPMSDisable(mDisplay);  // monitor powersave off
            printf ("stat: %d\n", stat);
	}
    }
#endif
    XGetScreenSaver(mDisplay, &timeout_save, &interval, &prefer_blank, &allow_exp);
    if (timeout_save)
	XSetScreenSaver(mDisplay, 0, interval, prefer_blank, allow_exp);
		    // turning off screensaver
}

#endif
