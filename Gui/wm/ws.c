
// --------------------------------------------------------------------------
//  AutoSpace Window System for Linux/Win32 v0.85
//   Writed by pontscho/fresh!mindworkz
// --------------------------------------------------------------------------

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <inttypes.h>

#include "../../config.h"
#include "ws.h"
#include "wsconv.h"
#include "../../postproc/rgb2rgb.h"
#include "../../mp_msg.h"
#include "../../mplayer.h"

#include <X11/extensions/XShm.h>
#ifdef HAVE_XSHAPE
#include <X11/extensions/shape.h>
#endif

#include <sys/ipc.h>
#include <sys/shm.h>

typedef struct
{
 unsigned long flags;
 unsigned long functions;
 unsigned long decorations;
 long input_mode;
 unsigned long status;
} MotifWmHints;

Atom                 wsMotifHints;

int                  wsMaxX         = 0; // Screen width.
int                  wsMaxY         = 0; // Screen height.

Display            * wsDisplay;
int                  wsScreen;
Window               wsRootWin;
XEvent               wsEvent;
int                  wsWindowDepth;
int		     wsWMType = wsWMUnknown;
GC                   wsHGC;
MotifWmHints         wsMotifWmHints;
Atom                 wsTextProperlyAtom = None;
int		     wsLayer = 0;

int                  wsDepthOnScreen = 0;
int                  wsRedMask = 0;
int                  wsGreenMask = 0;
int                  wsBlueMask = 0;
int                  wsOutMask = 0;

int                  wsTrue    = True;

wsTWindow          * wsWindowList[5] = { NULL,NULL,NULL,NULL,NULL };
int                  wsWLCount = 0;

unsigned long        wsKeyTable[512];

int                  wsUseXShm = 1;
int                  wsUseXShape = 1;

int XShmGetEventBase( Display* );
inline int wsSearch( Window win );

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

void wsWindowDecoration( wsTWindow * win,long d )
{
 wsMotifHints=XInternAtom( wsDisplay,"_MOTIF_WM_HINTS",0 );
 if ( wsMotifHints == None ) return;

 memset( &wsMotifWmHints,0,sizeof( MotifWmHints ) );
 wsMotifWmHints.flags=MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS; 
 if ( d )
  {
   wsMotifWmHints.functions=MWM_FUNC_MOVE | MWM_FUNC_CLOSE | MWM_FUNC_MINIMIZE | MWM_FUNC_MAXIMIZE | MWM_FUNC_RESIZE;
   wsMotifWmHints.decorations=MWM_DECOR_ALL;
  }
 XChangeProperty( wsDisplay,win->WindowID,wsMotifHints,wsMotifHints,32,
                  PropModeReplace,(unsigned char *)&wsMotifWmHints,5 );
}

// ----------------------------------------------------------------------------------------------
//   Init X Window System.
// ----------------------------------------------------------------------------------------------

int wsIOErrorHandler( Display * dpy )
{
 fprintf( stderr,"[ws] io error in display.\n" );
 exit( 0 );
}

int wsErrorHandler( Display * dpy,XErrorEvent * Event )
{
 char type[128];
 XGetErrorText( wsDisplay,Event->error_code,type,128 );
 fprintf(stderr,"[ws] Error in display.\n");
 fprintf(stderr,"[ws]  Error code: %d ( %s )\n",Event->error_code,type );
 fprintf(stderr,"[ws]  Request code: %d\n",Event->request_code );
 fprintf(stderr,"[ws]  Minor code: %d\n",Event->minor_code );
 fprintf(stderr,"[ws]  Modules: %s\n",current_module );
 exit( 0 );
}

int wsWindowManagerType( void )
{
 Atom            type;
 int             format;
 unsigned long   nitems, bytesafter;
 unsigned char * args = NULL;

 Window		 win;
 XEvent          xev;
 int		 c = 0;
 int             wm = wsWMUnknown;

// --- gnome
 type=XInternAtom( wsDisplay,"_WIN_SUPPORTING_WM_CHECK",False );
 if ( Success == XGetWindowProperty( wsDisplay,wsRootWin,type,0,65536 / sizeof( long ),False,AnyPropertyType,&type,&format,&nitems,&bytesafter,&args ) && nitems > 0 )
  {
   mp_dbg( MSGT_GPLAYER,MSGL_STATUS,"[ws] Detected wm is Gnome\n" );
   XFree( args );
   return wsWMGnome;
  }

// --- net wm
 type=XInternAtom( wsDisplay,"_NET_SUPPORTED",False );
 if ( Success == XGetWindowProperty( wsDisplay,wsRootWin,type,0,65536 / sizeof( long ),False,AnyPropertyType,&type,&format,&nitems,&bytesafter,&args ) && nitems > 0 )
  {
   mp_dbg( MSGT_GPLAYER,MSGL_STATUS,"[ws] Detected wm is NetWM\n" );
   XFree( args );
   return wsWMNetWM;
  }

// --- other wm
 mp_dbg( MSGT_VO,MSGL_STATUS,"[ws] Create window for WM detect ...\n" );
 win=XCreateSimpleWindow( wsDisplay,wsRootWin,wsMaxX,wsMaxY,1,1,0,0,0 );
 XSelectInput( wsDisplay,win,PropertyChangeMask | StructureNotifyMask );
 XMapWindow( wsDisplay,win );
 XMoveWindow( wsDisplay,win,wsMaxX,wsMaxY );
 do 
  { 
   XCheckWindowEvent( wsDisplay,win,PropertyChangeMask | StructureNotifyMask,&xev );

   if ( xev.type == PropertyNotify )
    {
     char * name = XGetAtomName( wsDisplay,xev.xproperty.atom );
     if ( !name ) break;

     if ( !strncmp( name,"_ICEWM_TRAY",11 ) )
      { mp_dbg( MSGT_VO,MSGL_STATUS,"[ws] Detected wm is IceWM.\n" ); wm=wsWMIceWM; break; }
     if ( !strncmp( name,"_KDE_",5 ) )
      { mp_dbg( MSGT_VO,MSGL_STATUS,"[ws] Detected wm is KDE.\n" ); wm=wsWMKDE; break; }
     if ( !strncmp( name,"KWM_WIN_DESKTOP",15 ) )
      { mp_dbg( MSGT_VO,MSGL_STATUS,"[ws] Detected wm is WindowMaker style.\n" ); wm=wsWMWMaker; break; }
//     fprintf(stderr,"[ws] PropertyNotify ( 0x%x ) %s ( 0x%x )\n",win,name,xev.xproperty.atom );
     XFree( name );
    }
 } while( c++ < 25 );
 XDestroyWindow( wsDisplay,win );
#ifdef MP_DEBUG
 if ( wm == wsWMUnknown ) mp_dbg( MSGT_VO,MSGL_STATUS,"[ws] Unknown wm type...\n" );
#endif
 return wsWMUnknown;
}

void wsXInit( void* mDisplay )
{
 int    eventbase;
 int    errorbase;

if(mDisplay){
 wsDisplay=mDisplay;
} else {
 char * DisplayName = ":0.0";
 if ( getenv( "DISPLAY" ) ) DisplayName=getenv( "DISPLAY" );
 wsDisplay=XOpenDisplay( DisplayName );
 if ( !wsDisplay )
  {
   mp_msg( MSGT_GPLAYER,MSGL_FATAL,"[ws] couldn't open the display !\n" );
   exit( 0 );
  }
}

{ /* on remote display XShm will be disabled - LGB */
 char *dispname=DisplayString(wsDisplay);
 int localdisp=1;
 if (dispname&&*dispname!=':') {
    localdisp=0;
    wsUseXShm=0;
 }
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[ws] Display name: %s => %s display.\n",dispname,localdisp?"local":"REMOTE");
 if (!localdisp) mp_msg( MSGT_GPLAYER,MSGL_STATUS,"[ws] Remote display, disabling XMITSHM\n");
}

 if ( !XShmQueryExtension( wsDisplay ) )
  {
   mp_msg( MSGT_GPLAYER,MSGL_ERR,"[ws] sorry, your system is not supported X shared memory extension.\n" );
   wsUseXShm=0;
  }
#ifdef HAVE_XSHAPE
  if ( !XShapeQueryExtension( wsDisplay,&eventbase,&errorbase ) )
   {
    mp_msg( MSGT_GPLAYER,MSGL_ERR,"[ws] sorry, your system is not supported XShape extension.\n" );
    wsUseXShape=0;
   }
#else
  wsUseXShape=0;
#endif

 XSynchronize( wsDisplay,True );

 wsScreen=DefaultScreen( wsDisplay );
 wsRootWin=RootWindow( wsDisplay,wsScreen );
 wsMaxX=DisplayWidth( wsDisplay,wsScreen );
 wsMaxY=DisplayHeight( wsDisplay,wsScreen );

 wsWMType=wsWindowManagerType();

 wsGetDepthOnScreen();
#ifdef DEBUG
  {
   int minor,major,shp;
   mp_msg( MSGT_GPLAYER,MSGL_DBG2,"[ws] Screen depth: %d\n",wsDepthOnScreen );
   mp_msg( MSGT_GPLAYER,MSGL_DBG2,"[ws]  size: %dx%d\n",wsMaxX,wsMaxY );
   mp_msg( MSGT_GPLAYER,MSGL_DBG2,"[ws]  red mask: 0x%x\n",wsRedMask );
   mp_msg( MSGT_GPLAYER,MSGL_DBG2,"[ws]  green mask: 0x%x\n",wsGreenMask );
   mp_msg( MSGT_GPLAYER,MSGL_DBG2,"[ws]  blue mask: 0x%x\n",wsBlueMask );
   if ( wsUseXShm )
    {
     XShmQueryVersion( wsDisplay,&major,&minor,&shp );
     mp_msg( MSGT_GPLAYER,MSGL_DBG2,"[ws] XShm version is %d.%d\n",major,minor );
    }
   #ifdef HAVE_XSHAPE
    if ( wsUseXShape )
     {
      XShapeQueryVersion( wsDisplay,&major,&minor );
      mp_msg( MSGT_GPLAYER,MSGL_DBG2,"[ws] XShape version is %d.%d\n",major,minor );
     }
   #endif
  }
#endif
 initConverter();
 wsOutMask=wsGetOutMask();
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[ws] Initialized converter: " );
 switch ( wsOutMask )
  {
   case wsRGB32:
     mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"rgb32 to rgb32\n" );
     wsConvFunc=BGR8880_to_RGB8880_c;
     break;
   case wsBGR32:
     mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"rgb32 to bgr32\n" );
     wsConvFunc=rgb32tobgr32;
     break;
   case wsRGB24:
     mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"rgb32 to rgb24\n" );
     wsConvFunc=rgb32to24;
     break;
   case wsBGR24:
     mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"rgb32 to bgr24\n" );
     wsConvFunc=BGR8880_to_BGR888_c;
     break;
   case wsRGB16:
     mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"rgb32 to rgb16\n" );
     wsConvFunc=rgb32to16;
     break;
   case wsBGR16:
     mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"rgb32 to bgr16\n" );
     wsConvFunc=BGR8880_to_BGR565_c;
     break;
   case wsRGB15:
     mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"rgb32 to rgb15\n" );
     wsConvFunc=rgb32to15;
     break;
   case wsBGR15:
     mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"rgb32 to bgr15\n" );
     wsConvFunc=BGR8880_to_BGR555_c;
     break;
  }
 XSetErrorHandler( wsErrorHandler );
}

// ----------------------------------------------------------------------------------------------
//   Create window.
//     X,Y   : window position
//     wX,wY : size of window
//     bW    : border width
//     cV    : visible mouse cursor on window
//     D     : visible frame, title, etc.
//     sR    : screen ratio
// ----------------------------------------------------------------------------------------------

XClassHint           wsClassHint;
XTextProperty        wsTextProperty;
Window               LeaderWindow;

void wsCreateWindow( wsTWindow * win,int X,int Y,int wX,int hY,int bW,int cV,unsigned char D,char * label )
{
 win->Property=D;
 if ( D & wsShowFrame ) win->Decorations=1;
 wsHGC=DefaultGC( wsDisplay,wsScreen );
// The window position and size.
 switch ( X )
  {
   case -1: win->X=( wsMaxX / 2 ) - ( wX / 2 ); break;
   case -2: win->X=wsMaxX - wX - 1; break;
   default: win->X=X; break;
  }
 switch ( Y )
  {
   case -1: win->Y=( wsMaxY / 2 ) - ( hY / 2 ); break;
   case -2: win->Y=wsMaxY - hY - 1; break;
   default: win->Y=Y; break;
  }
 win->Width=wX;
 win->Height=hY;
 win->OldX=win->X;
 win->OldY=win->Y;
 win->OldWidth=win->Width;
 win->OldHeight=win->Height;

// Border size for window.
 win->BorderWidth=bW;
// Hide Mouse Cursor
 win->wsCursor=None;
 win->wsMouseEventType=cV;
 win->wsCursorData[0]=0;
 win->wsCursorPixmap=XCreateBitmapFromData( wsDisplay,wsRootWin,win->wsCursorData,1,1 );
 if ( !(cV & wsShowMouseCursor) ) win->wsCursor=XCreatePixmapCursor( wsDisplay,win->wsCursorPixmap,win->wsCursorPixmap,&win->wsColor,&win->wsColor,0,0 );

 XGetWindowAttributes( wsDisplay,wsRootWin,&win->Attribs );
 if ( win->Attribs.depth < 15 )
  {
   mp_msg( MSGT_GPLAYER,MSGL_FATAL,"[ws] sorry, this color depth is not enough.\n" );
   exit( 0 );
  }
 XMatchVisualInfo( wsDisplay,wsScreen,win->Attribs.depth,TrueColor,&win->VisualInfo );

// ---
 win->AtomLeaderClient=XInternAtom( wsDisplay,"WM_CLIENT_LEADER",False );
 win->AtomDeleteWindow=XInternAtom( wsDisplay,"WM_DELETE_WINDOW",False );
 win->AtomTakeFocus=XInternAtom( wsDisplay,"WM_TAKE_FOCUS",False );
 win->AtomRolle=XInternAtom( wsDisplay,"WM_WINDOW_ROLE",False );
 win->AtomWMSizeHint=XInternAtom( wsDisplay,"WM_SIZE_HINT",False );
 win->AtomWMNormalHint=XInternAtom( wsDisplay,"WM_NORMAL_HINT",False );
 win->AtomProtocols=XInternAtom( wsDisplay,"WM_PROTOCOLS",False );
 win->AtomsProtocols[0]=win->AtomDeleteWindow;
 win->AtomsProtocols[1]=win->AtomTakeFocus;
 win->AtomsProtocols[2]=win->AtomRolle;
// ---

 win->WindowAttrib.background_pixel=BlackPixel( wsDisplay,wsScreen );
 win->WindowAttrib.border_pixel=WhitePixel( wsDisplay,wsScreen );
 win->WindowAttrib.colormap=XCreateColormap( wsDisplay,wsRootWin,win->VisualInfo.visual,AllocNone );
 win->WindowAttrib.event_mask=StructureNotifyMask | FocusChangeMask |
                              ExposureMask | PropertyChangeMask |
                              EnterWindowMask | LeaveWindowMask |
                              VisibilityChangeMask |
                              KeyPressMask | KeyReleaseMask;
 if ( ( cV & wsHandleMouseButton ) ) win->WindowAttrib.event_mask|=ButtonPressMask | ButtonReleaseMask;
 if ( ( cV & wsHandleMouseMove ) ) win->WindowAttrib.event_mask|=PointerMotionMask;
 win->WindowAttrib.cursor=win->wsCursor;
 win->WindowAttrib.override_redirect=False;
 if ( D & wsOverredirect ) win->WindowAttrib.override_redirect=True;

 win->WindowMask=CWBackPixel | CWBorderPixel |
                 CWColormap | CWEventMask | CWCursor |
                 CWX | CWY | CWWidth | CWHeight |
                 CWOverrideRedirect;

 win->WindowID=XCreateWindow( wsDisplay,
  (win->Parent != 0?win->Parent:wsRootWin),
  win->X,win->Y,win->Width,win->Height,win->BorderWidth,
  win->VisualInfo.depth,
  InputOutput,
  win->VisualInfo.visual,
  win->WindowMask,&win->WindowAttrib );

 wsClassHint.res_name="MPlayer";

 wsClassHint.res_class="MPlayer";
 XSetClassHint( wsDisplay,win->WindowID,&wsClassHint );

 win->SizeHint.flags=PPosition | PSize | PResizeInc | PWinGravity;// | PBaseSize;
 win->SizeHint.x=win->X;
 win->SizeHint.y=win->Y;
 win->SizeHint.width=win->Width;
 win->SizeHint.height=win->Height;

 if ( D & wsMinSize )
  {
   win->SizeHint.flags|=PMinSize;
   win->SizeHint.min_width=win->Width;
   win->SizeHint.min_height=win->Height;
  }
 if ( D & wsMaxSize )
  {
   win->SizeHint.flags|=PMaxSize;
   win->SizeHint.max_width=win->Width;
   win->SizeHint.max_height=win->Height;
  }

 win->SizeHint.height_inc=1;
 win->SizeHint.width_inc=1;
 win->SizeHint.base_width=win->Width;
 win->SizeHint.base_height=win->Height;
 win->SizeHint.win_gravity=StaticGravity;
 XSetWMNormalHints( wsDisplay,win->WindowID,&win->SizeHint );

 win->WMHints.flags=InputHint | StateHint;
 win->WMHints.input=True;
 win->WMHints.initial_state=NormalState;
 XSetWMHints( wsDisplay,win->WindowID,&win->WMHints );

 wsWindowDecoration( win,win->Decorations );
 XStoreName( wsDisplay,win->WindowID,label );
 XmbSetWMProperties( wsDisplay,win->WindowID,label,label,NULL,0,NULL,NULL,NULL );

 XSetWMProtocols( wsDisplay,win->WindowID,win->AtomsProtocols,3 );
 XChangeProperty( wsDisplay,win->WindowID,
                  win->AtomLeaderClient,
                  XA_WINDOW,32,PropModeReplace,
                  (unsigned char *)&LeaderWindow,1 );

 wsTextProperty.value=label;
 wsTextProperty.encoding=XA_STRING;
 wsTextProperty.format=8;
 wsTextProperty.nitems=strlen( label );
 XSetWMIconName( wsDisplay,win->WindowID,&wsTextProperty );

 win->wGC=XCreateGC( wsDisplay,win->WindowID,
  GCForeground | GCBackground,
  &win->wGCV );

 win->Visible=0;
 win->Focused=0;
 win->Mapped=0;
 win->Rolled=0;
 if ( D & wsShowWindow ) XMapWindow( wsDisplay,win->WindowID );

 wsCreateImage( win,win->Width,win->Height );
// --- End of creating --------------------------------------------------------------------------

 wsWindowList[wsWLCount++]=win;

 XFlush( wsDisplay );
 XSync( wsDisplay,False );

 win->ReDraw=NULL;
 win->ReSize=NULL;
 win->Idle=NULL;
 win->MouseHandler=NULL;
 win->KeyHandler=NULL;
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[ws] window is created. ( %s ).\n",label );
}

void wsDestroyWindow( wsTWindow * win )
{
 int l;
 l=wsSearch( win->WindowID );
 wsWindowList[l]=NULL;
 if ( win->wsCursor != None )
   {
    XFreeCursor( wsDisplay,win->wsCursor );
    win->wsCursor=None;
   }
 XUnmapWindow( wsDisplay,win->WindowID );
 wsDestroyImage( win );
 XDestroyWindow( wsDisplay,win->WindowID );
 win->ReDraw=NULL;
 win->ReSize=NULL;
 win->Idle=NULL;
 win->MouseHandler=NULL;
 win->KeyHandler=NULL;
 win->Visible=0;
 win->Focused=0;
 win->Mapped=0;
 win->Rolled=0;
}

// ----------------------------------------------------------------------------------------------
//   Handle events.
// ----------------------------------------------------------------------------------------------

inline int wsSearch( Window win )
{
 int i;
 for ( i=0;i<wsWLCount;i++ ) if ( wsWindowList[i]->WindowID == win ) return i;
 return -1;
}

Bool wsEvents( Display * display,XEvent * Event,XPointer arg )
{
 KeySym        keySym;
 unsigned long i = 0;
 int           l;
 int           x,y;
 Window        child_window = 0;

 l=wsSearch( Event->xany.window );
 if ( l == -1 ) return !wsTrue;
 wsWindowList[l]->State=0;
 switch( Event->type )
  {
   case ClientMessage:
        if ( Event->xclient.message_type == wsWindowList[l]->AtomProtocols )
         {
          if ( Event->xclient.data.l[0] == wsWindowList[l]->AtomDeleteWindow )
           { i=wsWindowClosed; goto expose; }
          if ( Event->xclient.data.l[0] == wsWindowList[l]->AtomTakeFocus )
           { i=wsWindowFocusIn;  wsWindowList[l]->Focused=wsFocused; goto expose; }
          if ( Event->xclient.data.l[0] == wsWindowList[l]->AtomRolle )
           { mp_msg( MSGT_GPLAYER,MSGL_STATUS,"[ws] rolled.\n" ); }
         }
        break;

   case MapNotify:   i=wsWindowMapped;   wsWindowList[l]->Mapped=wsMapped;   goto expose;
   case UnmapNotify: i=wsWindowUnmapped; wsWindowList[l]->Mapped=wsNone;     goto expose;
   case FocusIn:
        if ( wsWindowList[l]->Focused == wsFocused ) break;
        i=wsWindowFocusIn;
        wsWindowList[l]->Focused=wsFocused;
        goto expose;
   case FocusOut:
        if ( wsWindowList[l]->Focused == wsNone ) break;
        i=wsWindowFocusOut;
        wsWindowList[l]->Focused=wsNone;
        goto expose;
   case VisibilityNotify:
        switch( Event->xvisibility.state )
         {
          case VisibilityUnobscured:        i=wsWindowVisible;        wsWindowList[l]->Visible=wsVisible;    goto expose;
          case VisibilityFullyObscured:     i=wsWindowNotVisible;     wsWindowList[l]->Visible=wsNotVisible; goto expose;
          case VisibilityPartiallyObscured: i=wsWindowPartialVisible; wsWindowList[l]->Visible=wsPVisible;   goto expose;
         }
expose:
        wsWindowList[l]->State=i;
        if ( wsWindowList[l]->ReDraw ) wsWindowList[l]->ReDraw( wsDisplay,Event->xany.window );
        break;

   case Expose:
        wsWindowList[l]->State=wsWindowExpose;
        if ( ( wsWindowList[l]->ReDraw )&&( !Event->xexpose.count ) ) wsWindowList[l]->ReDraw( wsDisplay,Event->xany.window );
        break;

   case ConfigureNotify:
        XTranslateCoordinates( wsDisplay,wsWindowList[l]->WindowID,wsRootWin,0,0,&x,&y,&child_window );
        if ( ( wsWindowList[l]->X != x )||( wsWindowList[l]->Y != y )||( wsWindowList[l]->Width != Event->xconfigure.width )||( wsWindowList[l]->Height != Event->xconfigure.height ) )
          {
           wsWindowList[l]->X=x; wsWindowList[l]->Y=y;
           wsWindowList[l]->Width=Event->xconfigure.width; wsWindowList[l]->Height=Event->xconfigure.height;
           if ( wsWindowList[l]->ReSize ) wsWindowList[l]->ReSize( wsWindowList[l]->X,wsWindowList[l]->Y,wsWindowList[l]->Width,wsWindowList[l]->Height );
          }

        wsWindowList[l]->Rolled=wsNone;
        if ( Event->xconfigure.y < 0 )
          { i=wsWindowRolled; wsWindowList[l]->Rolled=wsRolled; goto expose; }

        break;

   case KeyPress:   i=wsKeyPressed;  goto keypressed;
   case KeyRelease: i=wsKeyReleased;
keypressed:
        wsWindowList[l]->Alt=0;
        wsWindowList[l]->Shift=0;
        wsWindowList[l]->NumLock=0;
        wsWindowList[l]->Control=0;
        wsWindowList[l]->CapsLock=0;
        if ( Event->xkey.state & Mod1Mask ) wsWindowList[l]->Alt=1;
        if ( Event->xkey.state & Mod2Mask ) wsWindowList[l]->NumLock=1;
        if ( Event->xkey.state & ControlMask ) wsWindowList[l]->Control=1;
        if ( Event->xkey.state & ShiftMask ) wsWindowList[l]->Shift=1;
        if ( Event->xkey.state & LockMask ) wsWindowList[l]->CapsLock=1;
        keySym=XKeycodeToKeysym( wsDisplay,Event->xkey.keycode,0 );
        if ( keySym != NoSymbol )
         {
          keySym=( (keySym&0xff00) != 0?( (keySym&0x00ff) + 256 ):( keySym ) );
          wsKeyTable[ keySym ]=i;
          if ( wsWindowList[l]->KeyHandler )
            wsWindowList[l]->KeyHandler( Event->xkey.state,i,keySym );
         }
        break;

   case MotionNotify:  i=wsMoveMouse;                 goto buttonreleased;
   case ButtonRelease: i=Event->xbutton.button + 128; goto buttonreleased;
   case ButtonPress:   i=Event->xbutton.button;       goto buttonreleased;
   case EnterNotify:   i=wsEnterWindow;               goto buttonreleased;
   case LeaveNotify:   i=wsLeaveWindow;
buttonreleased:
        if ( wsWindowList[l]->MouseHandler )
          wsWindowList[l]->MouseHandler( i,Event->xbutton.x,Event->xbutton.y,Event->xmotion.x_root,Event->xmotion.y_root );
        break;

   case PropertyNotify:
	{
	 char * name = XGetAtomName( wsDisplay,Event->xproperty.atom );
	 
         if ( !name ) break;
	 if ( !strncmp( name,"_ICEWM_TRAY",11 ) )
	  {
	   wsWMType=wsWMIceWM;
	   mp_dbg( MSGT_GPLAYER,MSGL_STATUS,"[ws] Detected wm is IceWM.\n" );
	  }
	 if ( !strncmp( name,"_KDE_",5 ) )
	  {
	   mp_dbg( MSGT_GPLAYER,MSGL_STATUS,"[ws] Detected wm is KDE.\n" );
	   wsWMType=wsWMKDE;
	  }
	 if ( !strncmp( name,"KWM_WIN_DESKTOP",15 ) )
	  {
	   mp_dbg( MSGT_GPLAYER,MSGL_STATUS,"[ws] Detected wm is WindowMaker style.\n" );
	   wsWMType=wsWMWMaker;
	  }
	      
//         fprintf(stderr,"[ws] PropertyNotify %s ( 0x%x )\n",name,Event->xproperty.atom );
							  
	 XFree( name );
	}
        break;

  }
 XFlush( wsDisplay );
 XSync( wsDisplay,False );
 return !wsTrue;
}

Bool wsDummyEvents( Display * display,XEvent * Event,XPointer arg )
{ return True; }

void wsHandleEvents( void ){
 // handle pending events
 while ( XPending(wsDisplay) ){
   XNextEvent( wsDisplay,&wsEvent );
//   printf("### X event: %d  [%d]\n",wsEvent.type,delay);
   wsEvents( wsDisplay,&wsEvent,NULL );
 }
}

extern void mplTimerHandler( void );

void wsMainLoop( void )
{
 int delay=20;
 mp_msg( MSGT_GPLAYER,MSGL_STATUS,"[ws] init threads: %d\n",XInitThreads() );
 XSynchronize( wsDisplay,False );
 XLockDisplay( wsDisplay );
// XIfEvent( wsDisplay,&wsEvent,wsEvents,NULL );

#if 1

while(wsTrue){
 // handle pending events
 while ( XPending(wsDisplay) ){
   XNextEvent( wsDisplay,&wsEvent );
   wsEvents( wsDisplay,&wsEvent,NULL );
   delay=0;
 }
 mplTimerHandler(); // handle timer event
 usleep(delay*1000); // FIXME!
 if(delay<10*20) delay+=20; // pump up delay up to 0.2 sec (low activity)
}

#else

 while( wsTrue )
  {
   XIfEvent( wsDisplay,&wsEvent,wsDummyEvents,NULL );
   wsEvents( wsDisplay,&wsEvent,NULL );
  }
#endif

 XUnlockDisplay( wsDisplay );
}

// ----------------------------------------------------------------------------------------------
//    Move window to selected layer
// ----------------------------------------------------------------------------------------------

#define WIN_LAYER_ONBOTTOM               2
#define WIN_LAYER_NORMAL                 4
#define WIN_LAYER_ONTOP                  6

void wsSetLayer( Display * wsDisplay, Window win, int layer )
{
 Atom            type;
 int             format;
 unsigned long   nitems, bytesafter;
 unsigned char * args = NULL;

 type=XInternAtom( wsDisplay,"_NET_SUPPORTED",False );
 if ( Success == XGetWindowProperty( wsDisplay,wsRootWin,type,0,65536 / sizeof( long ),False,AnyPropertyType,&type,&format,&nitems,&bytesafter,&args ) && nitems > 0 )
  {
   XEvent e;
   e.xclient.type=ClientMessage;
   e.xclient.message_type=XInternAtom( wsDisplay,"_NET_WM_STATE",False );
   e.xclient.display=wsDisplay;
   e.xclient.window=win;
   e.xclient.format=32;
   e.xclient.data.l[0]=layer;
   e.xclient.data.l[1]=XInternAtom( wsDisplay,"_NET_WM_STATE_STAYS_ON_TOP",False );
   e.xclient.data.l[2]=0l;
   e.xclient.data.l[3]=0l;
   e.xclient.data.l[4]=0l;
   XSendEvent( wsDisplay,wsRootWin,False,SubstructureRedirectMask,&e );
								   
   XFree( args );
   return;
  }
 type=XInternAtom( wsDisplay,"_WIN_SUPPORTING_WM_CHECK",False );
 if ( Success == XGetWindowProperty( wsDisplay,wsRootWin,type,0,65536 / sizeof( long ),False,AnyPropertyType,&type,&format,&nitems,&bytesafter,&args ) && nitems > 0 )
  {
   XClientMessageEvent  xev;
   
   memset( &xev,0,sizeof( xev ) );
   xev.type=ClientMessage;
   xev.window=win;
   xev.message_type=XInternAtom( wsDisplay,"_WIN_LAYER",False );
   xev.format=32;
   switch ( layer ) 
    {
     case -1: xev.data.l[0] = WIN_LAYER_ONBOTTOM; break;
     case  0: xev.data.l[0] = WIN_LAYER_NORMAL;   break;
     case  1: xev.data.l[0] = WIN_LAYER_ONTOP;    break;
    }
   XSendEvent( wsDisplay,wsRootWin,False,SubstructureNotifyMask,(XEvent*)&xev );
   if ( layer ) XRaiseWindow( wsDisplay,win );
								              
   XFree( args );
   return;
  }
}

// ----------------------------------------------------------------------------------------------
//    Switch to fullscreen.
// ----------------------------------------------------------------------------------------------
void wsFullScreen( wsTWindow * win )
{
 int decoration = 0;
 
 if ( wsWMType == wsWMUnknown ) XUnmapWindow( wsDisplay,win->WindowID );

 switch ( wsWMType )
   {
    case wsWMUnknown:
           XUnmapWindow( wsDisplay,win->WindowID );
	   break;
    case wsWMIceWM:
           if ( !win->isFullScreen ) XUnmapWindow( wsDisplay,win->WindowID );
           break;
   }

 if ( win->isFullScreen )
  {
   win->X=win->OldX;
   win->Y=win->OldY;
   win->Width=win->OldWidth;
   win->Height=win->OldHeight;
   win->isFullScreen=False;
   decoration=win->Decorations;
   wsScreenSaverOn( wsDisplay );
  }
  else
   {
    win->OldX=win->X; win->OldY=win->Y;
    win->OldWidth=win->Width; win->OldHeight=win->Height;
    win->X=0; win->Y=0;
    win->Width=wsMaxX; win->Height=wsMaxY;
    win->isFullScreen=True;
    wsScreenSaverOff( wsDisplay );
   }

 win->SizeHint.flags=PPosition | PSize | PWinGravity;// | PBaseSize;
 win->SizeHint.x=win->X;              win->SizeHint.y=win->Y;
 win->SizeHint.width=win->Width;      win->SizeHint.height=win->Height;
 win->SizeHint.base_width=win->Width; win->SizeHint.base_height=win->Height;
 
 win->SizeHint.win_gravity=StaticGravity;
 if ( win->Property & wsMaxSize )
  {
   win->SizeHint.flags|=PMaxSize;
   win->SizeHint.max_width=win->Width;
   win->SizeHint.max_height=win->Height;
  }
 if ( win->Property & wsMinSize )
  {
   win->SizeHint.flags|=PMinSize;
   win->SizeHint.min_width=win->Width;
   win->SizeHint.min_height=win->Height;
  }
 XSetWMNormalHints( wsDisplay,win->WindowID,&win->SizeHint );

 wsSetLayer( wsDisplay,win->WindowID,win->isFullScreen );
 XMoveResizeWindow( wsDisplay,win->WindowID,win->X,win->Y,win->Width,win->Height );
 wsWindowDecoration( win,decoration );
 XMapRaised( wsDisplay,win->WindowID );
 XRaiseWindow( wsDisplay,win->WindowID );
}

// ----------------------------------------------------------------------------------------------
//    Redraw screen.
// ----------------------------------------------------------------------------------------------
void wsPostRedisplay( wsTWindow * win )
{
 if ( win->ReDraw )
  {
   win->ReDraw( wsDisplay,win->WindowID );
   XFlush( wsDisplay );
  }
}

// ----------------------------------------------------------------------------------------------
//    Do Exit.
// ----------------------------------------------------------------------------------------------
void wsDoExit( void )
{ wsTrue=False; wsResizeWindow( wsWindowList[0],32,32 ); }

// ----------------------------------------------------------------------------------------------
//    Put 'Image' to window.
// ----------------------------------------------------------------------------------------------
void wsConvert( wsTWindow * win,unsigned char * Image,unsigned int Size )
{ if ( wsConvFunc ) wsConvFunc( Image,win->ImageData,win->xImage->width * win->xImage->height * 4 ); }

void wsPutImage( wsTWindow * win )
{
 if ( wsUseXShm )
  {
   XShmPutImage( wsDisplay,win->WindowID,win->wGC,win->xImage,
    0,0,
    ( win->Width - win->xImage->width ) / 2,( win->Height - win->xImage->height ) / 2,
    win->xImage->width,win->xImage->height,0 );
  }
  else
   {
    XPutImage( wsDisplay,win->WindowID,win->wGC,win->xImage,
    0,0,
    ( win->Width - win->xImage->width ) / 2,( win->Height - win->xImage->height ) / 2,
    win->xImage->width,win->xImage->height );
   }
}

// ----------------------------------------------------------------------------------------------
//    Move window to x, y.
// ----------------------------------------------------------------------------------------------
void wsMoveWindow( wsTWindow * win,int b,int x, int y )
{
 if ( b )
  {
   switch ( x )
    {
     case -1: win->X=( wsMaxX / 2 ) - ( win->Width / 2 ); break;
     case -2: win->X=wsMaxX - win->Width; break;
     default: win->X=x; break;
    }
   switch ( y )
    {
     case -1: win->Y=( wsMaxY / 2 ) - ( win->Height / 2 ); break;
     case -2: win->Y=wsMaxY - win->Height; break;
     default: win->Y=y; break;
    }
  }
  else { win->X=x; win->Y=y; }

 win->SizeHint.flags=PPosition | PWinGravity;
 win->SizeHint.x=win->X;
 win->SizeHint.y=win->Y;
 win->SizeHint.win_gravity=StaticGravity;
 XSetWMNormalHints( wsDisplay,win->WindowID,&win->SizeHint );

 XMoveWindow( wsDisplay,win->WindowID,win->X,win->Y );
 if ( win->ReSize ) win->ReSize( win->X,win->Y,win->Width,win->Height );
}

// ----------------------------------------------------------------------------------------------
//    Resize window to sx, sy.
// ----------------------------------------------------------------------------------------------
void wsResizeWindow( wsTWindow * win,int sx, int sy )
{
 win->Width=sx;
 win->Height=sy;

 win->SizeHint.flags=PPosition | PSize | PWinGravity;// | PBaseSize;
 win->SizeHint.x=win->X;
 win->SizeHint.y=win->Y;
 win->SizeHint.width=win->Width;
 win->SizeHint.height=win->Height;

 if ( win->Property & wsMinSize )
  {
   win->SizeHint.flags|=PMinSize;
   win->SizeHint.min_width=win->Width;
   win->SizeHint.min_height=win->Height;
  }
 if ( win->Property & wsMaxSize )
  {
   win->SizeHint.flags|=PMaxSize;
   win->SizeHint.max_width=win->Width;
   win->SizeHint.max_height=win->Height;
  }

 win->SizeHint.win_gravity=StaticGravity;
 win->SizeHint.base_width=sx; win->SizeHint.base_height=sy;

 if ( wsWMType == wsWMUnknown ) XUnmapWindow( wsDisplay,win->WindowID );

 XSetWMNormalHints( wsDisplay,win->WindowID,&win->SizeHint );
 XResizeWindow( wsDisplay,win->WindowID,sx,sy );
 XMapRaised( wsDisplay,win->WindowID );
 if ( win->ReSize ) win->ReSize( win->X,win->Y,win->Width,win->Height );
}

// ----------------------------------------------------------------------------------------------
//    Iconify window.
// ----------------------------------------------------------------------------------------------
void wsIconify( wsTWindow win )
{ XIconifyWindow( wsDisplay,win.WindowID,0 ); }

// ----------------------------------------------------------------------------------------------
//    Move top the window.
// ----------------------------------------------------------------------------------------------
void wsMoveTopWindow( wsTWindow * win )
{
 if ( wsWMType == wsWMIceWM )
  {
   XUnmapWindow( wsDisplay,win->WindowID );
   XMapWindow( wsDisplay,win->WindowID );
   return;
  }
/*  
 if ( XInternAtom( wsDisplay,"_NET_ACTIVE_WINDOW",False ) != None )
  {
   XEvent e;

   e.xclient.type=ClientMessage;
   e.xclient.message_type=XInternAtom( wsDisplay,"_NET_ACTIVE_WINDOW",False );
   e.xclient.display=wsDisplay;
   e.xclient.window=win->WindowID;
   e.xclient.format=32;
   e.xclient.data.l[0]=0;
   XSendEvent( wsDisplay,wsRootWin,False,SubstructureRedirectMask,&e );
  }
   else */
    {
     XMapRaised( wsDisplay,win->WindowID );
     XRaiseWindow( wsDisplay,win->WindowID );
    }
}

// ----------------------------------------------------------------------------------------------
//    Set window background to 'color'.
// ----------------------------------------------------------------------------------------------
void wsSetBackground( wsTWindow * win,int color )
{ XSetWindowBackground( wsDisplay,win->WindowID,color ); }

void wsSetBackgroundRGB( wsTWindow * win,int r,int g,int b )
{
 int color = 0;
 switch ( wsOutMask )
  {
   case wsRGB32:
   case wsRGB24: color=( r << 16 ) + ( g << 8 ) + b;  break;
   case wsBGR32:
   case wsBGR24: color=( b << 16 ) + ( g << 8 ) + r;  break;
   case wsRGB16: PACK_RGB16( b,g,r,color ); break;
   case wsBGR16: PACK_RGB16( r,g,b,color ); break;
   case wsRGB15: PACK_RGB15( b,g,r,color ); break;
   case wsBGR15: PACK_RGB15( r,g,b,color ); break;
  }
 XSetWindowBackground( wsDisplay,win->WindowID,color );
}

void wsSetForegroundRGB( wsTWindow * win,int r,int g,int b )
{
 int color = 0;
 switch ( wsOutMask )
  {
   case wsRGB32:
   case wsRGB24: color=( r << 16 ) + ( g << 8 ) + b;  break;
   case wsBGR32:
   case wsBGR24: color=( b << 16 ) + ( g << 8 ) + r;  break;
   case wsRGB16: PACK_RGB16( b,g,r,color ); break;
   case wsBGR16: PACK_RGB16( r,g,b,color ); break;
   case wsRGB15: PACK_RGB15( b,g,r,color ); break;
   case wsBGR15: PACK_RGB15( r,g,b,color ); break;
  }
 XSetForeground( wsDisplay,win->wGC,color );
}

// ----------------------------------------------------------------------------------------------
//    Draw string at x,y with fc ( foreground color ) and bc ( background color ).
// ----------------------------------------------------------------------------------------------
void wsDrawString( wsTWindow win,int x,int y,char * str,int fc,int bc )
{
 XSetForeground( wsDisplay,win.wGC,bc );
 XFillRectangle( wsDisplay,win.WindowID,win.wGC,x,y,
   XTextWidth( win.Font,str,strlen( str ) ) + 20,
   win.FontHeight + 2 );
 XSetForeground( wsDisplay,win.wGC,fc );
 XDrawString( wsDisplay,win.WindowID,win.wGC,x + 10,y + 13,str,strlen( str ) );
}

// ----------------------------------------------------------------------------------------------
//    Calculation string width.
// ----------------------------------------------------------------------------------------------
int wsTextWidth( wsTWindow win,char * str )
{ return XTextWidth( win.Font,str,strlen( str ) ) + 20; }

// ----------------------------------------------------------------------------------------------
//    Show / hide mouse cursor.
// ----------------------------------------------------------------------------------------------
void wsVisibleMouse( wsTWindow * win,int m )
{
 switch ( m )
  {
   case wsShowMouseCursor:
    if ( win->wsCursor != None )
     {
      XFreeCursor( wsDisplay,win->wsCursor );
      win->wsCursor=None;
     }
    XDefineCursor( wsDisplay,win->WindowID,0 );
    break;
   case wsHideMouseCursor:
    win->wsCursor=XCreatePixmapCursor( wsDisplay,win->wsCursorPixmap,win->wsCursorPixmap,&win->wsColor,&win->wsColor,0,0 );
    XDefineCursor( wsDisplay,win->WindowID,win->wsCursor );
    break;
  }
 XFlush( wsDisplay );
}

int wsGetDepthOnScreen( void )
{
 int                 bpp,ibpp;
 XImage            * mXImage;
 XWindowAttributes   attribs;

 mXImage=XGetImage( wsDisplay,wsRootWin,0,0,1,1,AllPlanes,ZPixmap );
 bpp=mXImage->bits_per_pixel;

 XGetWindowAttributes( wsDisplay,wsRootWin,&attribs );
 ibpp=attribs.depth;
 mXImage=XGetImage( wsDisplay,wsRootWin,0,0,1,1,AllPlanes,ZPixmap );
 bpp=mXImage->bits_per_pixel;
 if ( ( ibpp + 7 ) / 8 != ( bpp + 7 ) / 8 ) ibpp=bpp;
 wsDepthOnScreen=ibpp;
 wsRedMask=mXImage->red_mask;
 wsGreenMask=mXImage->green_mask;
 wsBlueMask=mXImage->blue_mask;
 XDestroyImage( mXImage );
 return ibpp;
}

void wsXDone( void )
{
 XCloseDisplay( wsDisplay );
}

void wsVisibleWindow( wsTWindow * win,int show )
{
 switch( show )
  {
   case wsShowWindow: XMapRaised( wsDisplay,win->WindowID ); break;
   case wsHideWindow: XUnmapWindow( wsDisplay,win->WindowID ); break;
  }
 XFlush( wsDisplay );
}

void wsDestroyImage( wsTWindow * win )
{
 if ( win->xImage )
  {
   XDestroyImage( win->xImage );
   if ( wsUseXShm )
    {
     XShmDetach( wsDisplay,&win->Shminfo );
     shmdt( win->Shminfo.shmaddr );
    }
  }
 win->xImage=NULL;
}

void wsCreateImage( wsTWindow * win,int Width,int Height )
{
 int CompletionType = -1;
 if ( wsUseXShm )
  {
   CompletionType=XShmGetEventBase( wsDisplay ) + ShmCompletion;
   win->xImage=XShmCreateImage( wsDisplay,win->VisualInfo.visual,
                   win->Attribs.depth,ZPixmap,NULL,&win->Shminfo,Width,Height );
   if ( win->xImage == NULL )
    {
     mp_msg( MSGT_GPLAYER,MSGL_FATAL,"[ws] shared memory extension error.\n" );
     exit( 0 );
    }
   win->Shminfo.shmid=shmget( IPC_PRIVATE,win->xImage->bytes_per_line * win->xImage->height,IPC_CREAT|0777 );
   if ( win->Shminfo.shmid < 0 )
    {
     XDestroyImage( win->xImage );
     mp_msg( MSGT_GPLAYER,MSGL_FATAL,"[ws] shared memory extension error.\n" );
     exit( 0 );
    }
   win->Shminfo.shmaddr=(char *)shmat( win->Shminfo.shmid,0,0 );

   if ( win->Shminfo.shmaddr == ((char *) -1) )
    {
     XDestroyImage( win->xImage );
     if ( win->Shminfo.shmaddr != ((char *) -1) ) shmdt( win->Shminfo.shmaddr );
     mp_msg( MSGT_GPLAYER,MSGL_FATAL,"[ws] shared memory extension error.\n" );
     exit( 0 );
    }
   win->xImage->data=win->Shminfo.shmaddr;
   win->Shminfo.readOnly=0;
   XShmAttach( wsDisplay,&win->Shminfo );
   shmctl( win->Shminfo.shmid,IPC_RMID,0 );
  }
  else
   {
    win->xImage=XCreateImage( wsDisplay,win->VisualInfo.visual,win->Attribs.depth,
                              ZPixmap,0,0,Width,Height,
                              (wsDepthOnScreen == 3) ? 32 : wsDepthOnScreen,
                              0 );
    if ( ( win->xImage->data=malloc( win->xImage->bytes_per_line * win->xImage->height ) ) == NULL )
     {
      mp_msg( MSGT_GPLAYER,MSGL_FATAL,"[ws] sorry, not enough memory for draw buffer.\n" );
      exit( 0 );
     }
   }
 win->ImageData=(unsigned char *)win->xImage->data;
 win->ImageDataw=(unsigned short int *)win->xImage->data;
 win->ImageDatadw=(unsigned int *)win->xImage->data;
}

void wsResizeImage( wsTWindow * win,int Width,int Height )
{ wsDestroyImage( win ); wsCreateImage( win,Width,Height ); }

int wsGetOutMask( void )
{
 if ( ( wsDepthOnScreen == 32 )&&( wsRedMask == 0xff0000 )&&( wsGreenMask == 0x00ff00 )&&( wsBlueMask == 0x0000ff ) ) return wsRGB32;
 if ( ( wsDepthOnScreen == 32 )&&( wsRedMask == 0x0000ff )&&( wsGreenMask == 0x00ff00 )&&( wsBlueMask == 0xff0000 ) ) return wsBGR32;
 if ( ( wsDepthOnScreen == 24 )&&( wsRedMask == 0xff0000 )&&( wsGreenMask == 0x00ff00 )&&( wsBlueMask == 0x0000ff ) ) return wsRGB24;
 if ( ( wsDepthOnScreen == 24 )&&( wsRedMask == 0x0000ff )&&( wsGreenMask == 0x00ff00 )&&( wsBlueMask == 0xff0000 ) ) return wsBGR24;
 if ( ( wsDepthOnScreen == 16 )&&( wsRedMask == 0xf800 )&&( wsGreenMask == 0x7e0 )&&( wsBlueMask ==   0x1f ) ) return wsRGB16;
 if ( ( wsDepthOnScreen == 16 )&&( wsRedMask ==   0x1f )&&( wsGreenMask == 0x7e0 )&&( wsBlueMask == 0xf800 ) ) return wsBGR16;
 if ( ( wsDepthOnScreen == 15 )&&( wsRedMask == 0x7c00 )&&( wsGreenMask == 0x3e0 )&&( wsBlueMask ==   0x1f ) ) return wsRGB15;
 if ( ( wsDepthOnScreen == 15 )&&( wsRedMask ==   0x1f )&&( wsGreenMask == 0x3e0 )&&( wsBlueMask == 0x7c00 ) ) return wsBGR15;
 return 0;
}

void wsSetTitle( wsTWindow * win,char * name )
{ XStoreName( wsDisplay,win->WindowID,name ); }

void wsSetMousePosition( wsTWindow * win,int x, int y )
{ XWarpPointer( wsDisplay,wsRootWin,win->WindowID,0,0,0,0,x,y ); }

static int dpms_disabled=0;
static int timeout_save=0;

void wsScreenSaverOn( Display *mDisplay )
{
 int nothing;
#ifdef HAVE_XDPMS
 if ( dpms_disabled )
  {
   if ( DPMSQueryExtension( mDisplay,&nothing,&nothing ) )
    {
     if ( !DPMSEnable( mDisplay ) ) mp_msg( MSGT_GPLAYER,MSGL_ERR,"DPMS not available ?\n" ); // restoring power saving settings
      else
       {
        // DPMS does not seem to be enabled unless we call DPMSInfo
        BOOL onoff;
        CARD16 state;
        DPMSInfo( mDisplay,&state,&onoff );
        if ( onoff ) mp_msg( MSGT_GPLAYER,MSGL_STATUS,"Successfully enabled DPMS.\n" );
         else mp_msg( MSGT_GPLAYER,MSGL_STATUS,"Could not enable DPMS.\n" );
       }
    }
  }
#endif
 if ( timeout_save )
  {
   int dummy, interval, prefer_blank, allow_exp;
   XGetScreenSaver( mDisplay,&dummy,&interval,&prefer_blank,&allow_exp );
   XSetScreenSaver( mDisplay,timeout_save,interval,prefer_blank,allow_exp );
   XGetScreenSaver( mDisplay,&timeout_save,&interval,&prefer_blank,&allow_exp );
  }
}

void wsScreenSaverOff( Display * mDisplay )
{
 int interval,prefer_blank,allow_exp,nothing;
#ifdef HAVE_XDPMS
 if ( DPMSQueryExtension( mDisplay,&nothing,&nothing ) )
  {
   BOOL onoff;
   CARD16 state;
   DPMSInfo( mDisplay,&state,&onoff );
   if ( onoff )
    {
      Status stat;
      mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"Disabling DPMS.\n" );
      dpms_disabled=1;
      stat=DPMSDisable( mDisplay );  // monitor powersave off
      mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"stat: %d.\n",stat );
   }
  }
#endif
 XGetScreenSaver( mDisplay,&timeout_save,&interval,&prefer_blank,&allow_exp );
 if ( timeout_save ) XSetScreenSaver( mDisplay,0,interval,prefer_blank,allow_exp ); // turning off screensaver
}

void wsSetShape( wsTWindow * win,char * data )
{
#ifdef HAVE_XSHAPE
 if ( !wsUseXShape ) return;
 if ( data )
  {
   win->Mask=XCreateBitmapFromData( wsDisplay,win->WindowID,data,win->Width,win->Height );
   XShapeCombineMask( wsDisplay,win->WindowID,ShapeBounding,0,0,win->Mask,ShapeSet );
   XFreePixmap( wsDisplay,win->Mask );
  }
  else XShapeCombineMask( wsDisplay,win->WindowID,ShapeBounding,0,0,None,ShapeSet );
#endif
}

#include "wsmkeys.h"
