
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

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

#ifdef HAVE_XF86VM
#include <X11/extensions/xf86vmode.h>
#endif

#ifdef HAVE_NEW_INPUT
#include "../input/input.h"
#include "../input/mouse.h"
#endif

#ifdef HAVE_NEW_GUI
#include "../Gui/interface.h"
#include "../mplayer.h"
#endif

/*
 * If SCAN_VISUALS is defined, vo_init() scans all available TrueColor
 * visuals for the 'best' visual for MPlayer video display.  Note that
 * the 'best' visual might be different from the default visual that
 * is in use on the root window of the display/screen.
 */
#define	SCAN_VISUALS

#define vo_wm_Unknown     0
#define vo_wm_NetWM       1
#define vo_wm_KDE         2
#define vo_wm_IceWM       3
#define vo_wm_WMakerStyle 4

int ice_layer=12;
int stop_xscreensaver=0;

extern int verbose;

static int dpms_disabled=0;
static int timeout_save=0;
static int xscreensaver_was_running=0;

char* mDisplayName=NULL;
Display* mDisplay=NULL;
Window   mRootWin;
int mScreen;
int mLocalDisplay;

/* output window id */
int WinID=-1;
int vo_mouse_autohide = 0;
int vo_wm_type = -1;

#ifdef HAVE_XINERAMA
int xinerama_screen = 0;
int xinerama_x = 0;
int xinerama_y = 0;
#endif
#ifdef HAVE_XF86VM
XF86VidModeModeInfo **vidmodes=NULL;
XF86VidModeModeLine modeline;
#endif

void vo_hidecursor ( Display *disp , Window win )
{
	Cursor no_ptr;
	Pixmap bm_no;
	XColor black,dummy;
	Colormap colormap;
	static unsigned char bm_no_data[] = { 0,0,0,0, 0,0,0,0  };

	if(WinID==0) return;	// do not hide, if we're playing at rootwin
	
	colormap = DefaultColormap(disp,DefaultScreen(disp));
	XAllocNamedColor(disp,colormap,"black",&black,&dummy);	
	bm_no = XCreateBitmapFromData(disp, win, bm_no_data, 8,8);    
	no_ptr=XCreatePixmapCursor(disp, bm_no, bm_no,&black, &black,0, 0);									          
	XDefineCursor(disp,win,no_ptr);
	XFreeCursor( disp,no_ptr );
}

void vo_showcursor( Display *disp, Window win )
{ 
 if ( WinID==0 ) return;
 XDefineCursor( disp,win,0 ); 
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
      mp_msg(MSGT_VO,MSGL_V,"vo: X11 truecolor visual %#x, depth %d, R:%lX G:%lX B:%lX\n",
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

static int x11_errorhandler(Display *display, XErrorEvent *event)
{
#define MSGLEN 60
    char msg[MSGLEN];
        
    XGetErrorText(display, event->error_code, (char *)&msg, MSGLEN);
    
    mp_msg(MSGT_VO,MSGL_ERR,"X11 error: %s\n", msg);
    
    mp_msg(MSGT_VO,MSGL_V,"Type: %x, display: %x, resourceid: %x, serial: %x\n",
	    event->type, event->display, event->resourceid, event->serial);
    mp_msg(MSGT_VO,MSGL_V,"Error code: %x, request code: %x, minor code: %x\n",
	    event->error_code, event->request_code, event->minor_code);
    
    abort();
    //exit_player("X11 error");
#undef MSGLEN
}

int vo_wm_string_test( char * name )
{
 if ( !strncmp( name,"_ICEWM_TRAY",11 ) )
  { mp_dbg( MSGT_VO,MSGL_STATUS,"[x11] Detected wm is IceWM.\n" ); return vo_wm_IceWM; }
 if ( !strncmp( name,"_KDE_",5 ) )
  { mp_dbg( MSGT_VO,MSGL_STATUS,"[x11] Detected wm is KDE.\n" ); return vo_wm_KDE; }
 if ( !strncmp( name,"KWM_WIN_DESKTOP",15 ) )
  { mp_dbg( MSGT_VO,MSGL_STATUS,"[x11] Detected wm is WindowMaker style.\n" ); return vo_wm_WMakerStyle; }
// fprintf(stderr,"[ws] PropertyNotify ( 0x%x ) %s ( 0x%x )\n",win,name,xev.xproperty.atom );
 return vo_wm_Unknown;
}

int vo_wm_detect( void )
{
 Atom            type;
 Window		 win;
 XEvent          xev;
 int		 c = 0;
 int             wm = vo_wm_Unknown;
 int             format;
 unsigned long   nitems, bytesafter;
 unsigned char * args = NULL;
 char          * name = NULL;
 
 if ( WinID >= 0 ) return vo_wm_Unknown;
 
#if 1
// --- netwm 
 type=XInternAtom( mDisplay,"_NET_SUPPORTED",False );
 if ( Success == XGetWindowProperty( mDisplay,mRootWin,type,0,16384,False,AnyPropertyType,&type,&format,&nitems,&bytesafter,&args ) && nitems > 0 )
  {
   mp_dbg( MSGT_VO,MSGL_STATUS,"[x11] Detected wm is of class NetWM.\n" );
   XFree( args );
   return vo_wm_NetWM;
  }
#endif
// --- other wm
 mp_dbg( MSGT_VO,MSGL_STATUS,"[x11] Create window for WM detect ...\n" );
 win=XCreateSimpleWindow( mDisplay,mRootWin,vo_screenwidth,vo_screenheight,1,1,0,0,0 );
 XSelectInput( mDisplay,win,PropertyChangeMask | StructureNotifyMask );
 XMapWindow( mDisplay,win );
 XMoveWindow( mDisplay,win,vo_screenwidth,vo_screenheight );
 do 
  { 
   XCheckWindowEvent( mDisplay,win,PropertyChangeMask | StructureNotifyMask,&xev );

   if ( xev.type == PropertyNotify )
    {
     name=XGetAtomName( mDisplay,xev.xproperty.atom );
     if ( !name ) break;

     wm=vo_wm_string_test( name );
     if ( wm != vo_wm_Unknown ) break;
     XFree( name ); name=NULL;
    }
 } while( c++ < 25 );
 if ( name ) XFree( name );
 XUnmapWindow( mDisplay,win );
 XDestroyWindow( mDisplay,win );
#ifdef MP_DEBUG
 if ( wm == vo_wm_Unknown ) mp_dbg( MSGT_VO,MSGL_STATUS,"[x11] Unknown wm type...\n" );
#endif
 return wm;
}    

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
 char* dispName;

 if(vo_depthonscreen) return 1; // already called

 XSetErrorHandler(x11_errorhandler);

#if 0
 if (!mDisplayName)
   if (!(mDisplayName=getenv("DISPLAY")))
     mDisplayName=strdup(":0.0");
#else
  dispName = XDisplayName(mDisplayName);
#endif

 mp_msg(MSGT_VO,MSGL_V,"X11 opening display: %s\n", dispName);

 mDisplay=XOpenDisplay(dispName);
 if ( !mDisplay )
  {
   mp_msg(MSGT_VO,MSGL_ERR,"vo: couldn't open the X11 display (%s)!\n",dispName );
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
  if(xinerama_screen >= num_screens) xinerama_screen = 0;
  if (! vo_screenwidth)
    vo_screenwidth=screens[xinerama_screen].width;
  if (! vo_screenheight)
    vo_screenheight=screens[xinerama_screen].height;
  xinerama_x = screens[xinerama_screen].x_org;
  xinerama_y = screens[xinerama_screen].y_org;

  XFree(screens);
  }
 else
#endif
#ifdef HAVE_XF86VM
 {
  int clock;
  XF86VidModeGetModeLine( mDisplay,mScreen,&clock ,&modeline );
  if ( !vo_screenwidth )  vo_screenwidth=modeline.hdisplay;
  if ( !vo_screenheight ) vo_screenheight=modeline.vdisplay;
 }
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
   mp_msg(MSGT_VO,MSGL_V,"vo: X11 color mask:  %X  (R:%lX G:%lX B:%lX)\n",
	    mask,mXImage->red_mask,mXImage->green_mask,mXImage->blue_mask);
   XDestroyImage( mXImage );
 }
 if(((vo_depthonscreen+7)/8)==2){
   if(mask==0x7FFF) vo_depthonscreen=15; else
   if(mask==0xFFFF) vo_depthonscreen=16;
 }
// XCloseDisplay( mDisplay );
/* slightly improved local display detection AST */
 if ( strncmp(dispName, "unix:", 5) == 0)
		dispName += 4;
 else if ( strncmp(dispName, "localhost:", 10) == 0)
		dispName += 9;
 if (*dispName==':') mLocalDisplay=1; else mLocalDisplay=0;
 mp_msg(MSGT_VO,MSGL_INFO,"vo: X11 running at %dx%d with depth %d and %d bpp (\"%s\" => %s display)\n",
	vo_screenwidth,vo_screenheight,
	depth, vo_depthonscreen,
	dispName,mLocalDisplay?"local":"remote");

 vo_wm_type=vo_wm_detect();

 return 1;
}

void vo_uninit( void )
{
 if (!mDisplay)
 {
    mp_msg(MSGT_VO, MSGL_V, "vo: x11 uninit called but X11 not inited..\n");
    return;
 }
// if( !vo_depthonscreen ) return;
 mp_msg(MSGT_VO,MSGL_V,"vo: uninit ...\n" );
 XSetErrorHandler(NULL);
 XCloseDisplay( mDisplay );
 vo_depthonscreen = 0;
 mDisplay=NULL;
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
   case wsLess:      mplayer_put_key('<'); break;
   case wsMore:      mplayer_put_key('>'); break;
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

  if ( !WinID ) return;

  if(vo_fsmode&1){
    XSetWindowAttributes attr;
    attr.override_redirect = (!d) ? True : False;
    XChangeWindowAttributes(vo_Display, w, CWOverrideRedirect, &attr);
//    XMapWindow(vo_Display, w);
  }

  if(vo_fsmode&8){
    XSetTransientForHint (vo_Display, w, RootWindow(vo_Display,mScreen));
  }

 vo_MotifHints=XInternAtom( vo_Display,"_MOTIF_WM_HINTS",0 );
 if ( vo_MotifHints != None )
  {
   memset( &vo_MotifWmHints,0,sizeof( MotifWmHints ) );
   vo_MotifWmHints.flags=MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS;
   if ( d )
    {
     vo_MotifWmHints.functions=MWM_FUNC_MOVE | MWM_FUNC_CLOSE | MWM_FUNC_MINIMIZE | MWM_FUNC_MAXIMIZE | MWM_FUNC_RESIZE;
     d=MWM_DECOR_ALL;
    }
#if 0
   vo_MotifWmHints.decorations=d|((vo_fsmode&2)?0:MWM_DECOR_MENU);
#else
   vo_MotifWmHints.decorations=d|((vo_fsmode&2)?MWM_DECOR_MENU:0);
#endif
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

Window     vo_window = None;
GC         vo_gc = NULL;
XSizeHints vo_hint;

#ifdef HAVE_NEW_GUI
 void vo_setwindow( Window w,GC g ) {
   vo_window=w; vo_gc=g;
 }
#endif

void vo_x11_uninit()
{
    if(vo_window!=None) vo_showcursor( mDisplay,vo_window );

#ifdef HAVE_NEW_GUI
    /* destroy window only if it's not controlled by GUI */
    if ( !use_gui )
#endif
    {
	if(vo_gc){
	  XSetBackground( mDisplay,vo_gc,0 );
	  vo_gc=NULL;
	}
	if(vo_window!=None){
	  XClearWindow( mDisplay,vo_window );
	  if (WinID < 0){
	    XEvent xev;
	    XUnmapWindow( mDisplay,vo_window );
	    XDestroyWindow(mDisplay, vo_window);
	    do { XNextEvent( mDisplay,&xev ); } while ( xev.type != DestroyNotify || xev.xdestroywindow.event != vo_window );
	  }
	  vo_window=None;
	}
	vo_fs=0;
    }
}

       int vo_mouse_timer_const = 30;
static int vo_mouse_counter = 30;

int vo_x11_check_events(Display *mydisplay){
 int ret=0;
 XEvent         Event;
 char           buf[100];
 KeySym         keySym;
 static XComposeStatus stat;

// unsigned long  vo_KeyTable[512];

 if ( ( vo_mouse_autohide )&&( --vo_mouse_counter == 0 ) ) vo_hidecursor( mydisplay,vo_window );

 while ( XPending( mydisplay ) )
  {
   XNextEvent( mydisplay,&Event );
   #ifdef HAVE_NEW_GUI
    if ( use_gui ) 
     {
      guiGetEvent( 0,(char*)&Event );
      if ( vo_window != Event.xany.window ) continue;
     }
   #endif
//       printf("\rEvent.type=%X  \n",Event.type);
    switch( Event.type )
     {
      case Expose:
           ret|=VO_EVENT_EXPOSE;
           break;
      case ConfigureNotify:
           vo_dwidth=Event.xconfigure.width;
           vo_dheight=Event.xconfigure.height;
#if 0
	   /* when resizing, x and y are zero :( */
	   vo_dx=Event.xconfigure.x;
	   vo_dy=Event.xconfigure.y;
#else
	   {
	    Window root;
	    int foo;
	    Window win;
	    XGetGeometry(mydisplay, vo_window, &root, &foo, &foo, 
		&foo/*width*/, &foo/*height*/, &foo, &foo);
	    XTranslateCoordinates(mydisplay, vo_window, root, 0, 0,
		&vo_dx, &vo_dy, &win);
	    }
#endif
           ret|=VO_EVENT_RESIZE;
           break;
      case KeyPress:
           { 
	    int key;
            XLookupString( &Event.xkey,buf,sizeof(buf),&keySym,&stat );
	    key=( (keySym&0xff00) != 0?( (keySym&0x00ff) + 256 ):( keySym ) );
	    #ifdef HAVE_NEW_GUI
	     if ( ( use_gui )&&( key == wsEnter ) ) break;
	    #endif
            vo_x11_putkey( key );
            ret|=VO_EVENT_KEYPRESS;
	   }
           break;
      case MotionNotify:
           if ( vo_mouse_autohide ) { vo_showcursor( mydisplay,vo_window ); vo_mouse_counter=vo_mouse_timer_const; }
           break;
#ifdef HAVE_NEW_INPUT
      case ButtonPress:
           if ( vo_mouse_autohide ) { vo_showcursor( mydisplay,vo_window ); vo_mouse_counter=vo_mouse_timer_const; }
           // Ignore mouse whell press event
           if(Event.xbutton.button > 3) {
	   mplayer_put_key(MOUSE_BTN0+Event.xbutton.button-1);
	   break;
           }
	   #ifdef HAVE_NEW_GUI
	    // Ignor mouse button 1 - 3 under gui 
	    if ( use_gui && ( Event.xbutton.button >= 1 )&&( Event.xbutton.button <= 3 ) ) break;
	   #endif
           mplayer_put_key((MOUSE_BTN0+Event.xbutton.button-1)|MP_KEY_DOWN);
           break;
      case ButtonRelease:
           if ( vo_mouse_autohide ) { vo_showcursor( mydisplay,vo_window ); vo_mouse_counter=vo_mouse_timer_const; }
           #ifdef HAVE_NEW_GUI
	    // Ignor mouse button 1 - 3 under gui 
	    if ( use_gui && ( Event.xbutton.button >= 1 )&&( Event.xbutton.button <= 3 ) ) break;
	   #endif
           mplayer_put_key(MOUSE_BTN0+Event.xbutton.button-1);
           break;
#endif
      case PropertyNotify: 
    	   {
	    char * name = XGetAtomName( mydisplay,Event.xproperty.atom );
	    int    wm = vo_wm_Unknown;
	    
	    if ( !name ) break;
	    
	    wm=vo_wm_string_test(name);
	    if ( wm != vo_wm_Unknown ) vo_wm_type=wm;

//          fprintf(stderr,"[ws] PropertyNotify ( 0x%x ) %s ( 0x%x )\n",vo_window,name,Event.xproperty.atom );
	      
	    XFree( name );
	   }
	   break;
     }
  }
  return ret;
}

void vo_x11_sizehint( int x, int y, int width, int height, int max )
{
 vo_hint.flags=PPosition | PSize | PWinGravity;
 vo_hint.x=x; vo_hint.y=y; vo_hint.width=width; vo_hint.height=height;
 if ( max )
  {
   vo_hint.max_width=width; vo_hint.max_height=height;
   vo_hint.flags|=PMaxSize;
  } else { vo_hint.max_width=0; vo_hint.max_height=0; }
 vo_hint.win_gravity=StaticGravity;
 XSetWMNormalHints( mDisplay,vo_window,&vo_hint );
}

#define WIN_LAYER_ONBOTTOM               2
#define WIN_LAYER_NORMAL                 4
#define WIN_LAYER_ONTOP                  6

void vo_x11_setlayer( int layer )
{
 Atom            type;
 int             format;
 unsigned long   nitems, bytesafter;
 unsigned char * args = NULL;

 if ( WinID >= 0 ) return;
 
 if ( vo_wm_type == vo_wm_IceWM )
  {
    XClientMessageEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.window = vo_window;
    xev.message_type = XInternAtom(mDisplay, "_WIN_LAYER", False);
    xev.format = 32;
    xev.data.l[0] = layer?ice_layer:4; // if not fullscreen, stay on layer "Normal"
    xev.data.l[1] = CurrentTime;
    mp_dbg( MSGT_VO,MSGL_STATUS,"[x11] IceWM style stay on top ( layer %d ).\n",xev.data.l[0] );
    XSendEvent(mDisplay, mRootWin, False, SubstructureNotifyMask, (XEvent *) &xev);
   return;
  }

 type=XInternAtom( mDisplay,"_NET_SUPPORTED",False );
 if ( Success == XGetWindowProperty( mDisplay,mRootWin,type,0,16384,False,AnyPropertyType,&type,&format,&nitems,&bytesafter,&args ) && nitems > 0 )
  {
   XEvent e;
   
   mp_dbg( MSGT_VO,MSGL_STATUS,"[x11] NET style stay on top ( layer %d ).\n",layer );
   memset( &e,0,sizeof( e ) );
   e.xclient.type=ClientMessage;
   e.xclient.message_type=XInternAtom( mDisplay,"_NET_WM_STATE",False );
   e.xclient.display=mDisplay;
   e.xclient.window=vo_window;
   e.xclient.format=32;
   e.xclient.data.l[0]=layer;
   e.xclient.data.l[1]=XInternAtom( mDisplay,"_NET_WM_STATE_STAYS_ON_TOP",False );
   XSendEvent( mDisplay,mRootWin,False,SubstructureRedirectMask,&e );
								   
   XFree( args );
   return;
  }
 type=XInternAtom( mDisplay,"_WIN_SUPPORTING_WM_CHECK",False );
 if ( Success == XGetWindowProperty( mDisplay,mRootWin,type,0,16384,False,AnyPropertyType,&type,&format,&nitems,&bytesafter,&args ) && nitems > 0 )
  {
   XClientMessageEvent  xev;
   
   mp_dbg( MSGT_VO,MSGL_STATUS,"[x11] Gnome style stay on top ( layer %d ).\n",layer );
   memset( &xev,0,sizeof( xev ) );
   xev.type=ClientMessage;
   xev.window=vo_window;
   xev.message_type=XInternAtom( mDisplay,"_WIN_LAYER",False );
   xev.format=32;
   switch ( layer ) 
    {
     case -1: xev.data.l[0] = WIN_LAYER_ONBOTTOM; break;
     case  0: xev.data.l[0] = WIN_LAYER_NORMAL;   break;
     case  1: xev.data.l[0] = WIN_LAYER_ONTOP;    break;
    }
   XSendEvent( mDisplay,mRootWin,False,SubstructureNotifyMask,(XEvent*)&xev );
   if ( layer ) XRaiseWindow( mDisplay,vo_window );
								              
   XFree( args );
   return;
  }
}

void vo_x11_fullscreen( void )
{
 int x=0,y=0,w=vo_screenwidth,h=vo_screenheight;

 if ( WinID >= 0 ) return;

 if ( vo_fs )
  { vo_fs=VO_FALSE; x=vo_old_x; y=vo_old_y; w=vo_old_width; h=vo_old_height; }
   else { vo_fs=VO_TRUE; vo_old_x=vo_dx; vo_old_y=vo_dy; vo_old_width=vo_dwidth;   vo_old_height=vo_dheight; }

 vo_x11_decoration( mDisplay,vo_window,(vo_fs) ? 0 : 1 );
 vo_x11_sizehint( x,y,w,h,0 );
 vo_x11_setlayer( vo_fs );
 if(vo_wm_type==vo_wm_Unknown && !(vo_fsmode&16))
     XUnmapWindow( mDisplay,vo_window );  // required for MWM
 XMoveResizeWindow( mDisplay,vo_window,x,y,w,h );
 XMapRaised( mDisplay,vo_window );
 XRaiseWindow( mDisplay,vo_window );
 XFlush( mDisplay );
}

void saver_on(Display *mDisplay) {

#ifdef HAVE_XDPMS
    int nothing;
    if (dpms_disabled)
    {
	if (DPMSQueryExtension(mDisplay, &nothing, &nothing))
	{
	    if (!DPMSEnable(mDisplay)) {  // restoring power saving settings
                mp_msg(MSGT_VO,MSGL_WARN,"DPMS not available?\n");
            } else {
                // DPMS does not seem to be enabled unless we call DPMSInfo
	        BOOL onoff;
        	CARD16 state;
        	DPMSInfo(mDisplay, &state, &onoff);
                if (onoff) {
	            mp_msg(MSGT_VO,MSGL_INFO,"Successfully enabled DPMS\n");
                } else {
	            mp_msg(MSGT_VO,MSGL_WARN,"Could not enable DPMS\n");
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

    if (xscreensaver_was_running && stop_xscreensaver)
       system("xscreensaver -no-splash &");

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
	    mp_msg(MSGT_VO,MSGL_INFO,"Disabling DPMS\n");
	    dpms_disabled=1;
	    stat = DPMSDisable(mDisplay);  // monitor powersave off
            mp_msg(MSGT_VO,MSGL_V,"DPMSDisable stat: %d\n", stat);
	}
    }
#endif
    XGetScreenSaver(mDisplay, &timeout_save, &interval, &prefer_blank, &allow_exp);
    if (timeout_save)
	XSetScreenSaver(mDisplay, 0, interval, prefer_blank, allow_exp);
    xscreensaver_was_running = stop_xscreensaver && ! system("xscreensaver-command -exit");
		    // turning off screensaver
}



#ifdef HAVE_XINERAMA
void vo_x11_xinerama_move(Display *dsp, Window w)
{
	if(XineramaIsActive(dsp))
	{
		 /* printf("XXXX Xinerama screen: x: %hd y: %hd\n",xinerama_x,xinerama_y); */
		XMoveWindow(dsp,w,xinerama_x,xinerama_y);
	}
}
#endif

#ifdef HAVE_XF86VM
void vo_vm_switch(uint32_t X, uint32_t Y, int* modeline_width, int* modeline_height)
{
    unsigned int vm_event, vm_error;
    unsigned int vm_ver, vm_rev;
    int i,j,have_vm=0;

    int modecount;
    
    if (XF86VidModeQueryExtension(mDisplay, &vm_event, &vm_error)) {
      XF86VidModeQueryVersion(mDisplay, &vm_ver, &vm_rev);
      mp_msg(MSGT_VO,MSGL_V,"XF86VidMode Extension v%i.%i\n", vm_ver, vm_rev);
      have_vm=1;
    } else
      mp_msg(MSGT_VO,MSGL_WARN,"XF86VidMode Extenstion not available.\n");

    if (have_vm) {
      if (vidmodes==NULL)
        XF86VidModeGetAllModeLines(mDisplay,mScreen,&modecount,&vidmodes);
      j=0;
      *modeline_width=vidmodes[0]->hdisplay;
      *modeline_height=vidmodes[0]->vdisplay;
      
      for (i=1; i<modecount; i++)
        if ((vidmodes[i]->hdisplay >= X) && (vidmodes[i]->vdisplay >= Y))
          if ( (vidmodes[i]->hdisplay <= *modeline_width ) && (vidmodes[i]->vdisplay <= *modeline_height) )
	    {
	      *modeline_width=vidmodes[i]->hdisplay;
	      *modeline_height=vidmodes[i]->vdisplay;
	      j=i;
	    }
      
      mp_msg(MSGT_VO,MSGL_INFO,"XF86VM: Selected video mode %dx%d for image size %dx%d.\n",*modeline_width, *modeline_height, X, Y);
      XF86VidModeLockModeSwitch(mDisplay,mScreen,0);
      XF86VidModeSwitchToMode(mDisplay,mScreen,vidmodes[j]);
      XF86VidModeSwitchToMode(mDisplay,mScreen,vidmodes[j]);
      X=(vo_screenwidth-*modeline_width)/2;
      Y=(vo_screenheight-*modeline_height)/2;
      XF86VidModeSetViewPort(mDisplay,mScreen,X,Y);
    }
}

void vo_vm_close(Display *dpy)
{
 #ifdef HAVE_NEW_GUI
        if (vidmodes!=NULL && vo_window != None)
 #else
        if (vidmodes!=NULL)
 #endif
         {
           int i, modecount;
           int screen; screen=DefaultScreen( dpy );

           free(vidmodes); vidmodes=NULL;
           XF86VidModeGetAllModeLines(mDisplay,mScreen,&modecount,&vidmodes);
           for (i=0; i<modecount; i++)
             if ((vidmodes[i]->hdisplay == vo_screenwidth) && (vidmodes[i]->vdisplay == vo_screenheight)) 
               { 
                 mp_msg(MSGT_VO,MSGL_INFO,"\nReturning to original mode %dx%d\n", vo_screenwidth, vo_screenheight);
                 break;
               }

           XF86VidModeSwitchToMode(dpy,screen,vidmodes[i]);
           XF86VidModeSwitchToMode(dpy,screen,vidmodes[i]);
           free(vidmodes);
         }
}
#endif

#endif
