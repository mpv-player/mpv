/*
 * AutoSpace Window System for Linux/Win32 v0.61
 * written by pontscho/fresh!mindworkz
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

#ifndef MPLAYER_GUI_WS_H
#define MPLAYER_GUI_WS_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/extensions/XShm.h>
#ifdef CONFIG_XDPMS
#include <X11/extensions/dpms.h>
#endif

#define  wsKeyReleased   0
#define  wsKeyPressed    1

#define  wsShift        (1L<<0)
#define  wsLock         (1L<<1)
#define  wsCtrl         (1L<<2)
#define  wsAlt          (1L<<3)

#define  wsPLMouseButton 1
#define  wsPMMouseButton 2
#define  wsPRMouseButton 3
#define  wsP4MouseButton 4
#define  wsP5MouseButton 5
#define  wsRLMouseButton (1 + 128)
#define  wsRMMouseButton (2 + 128)
#define  wsRRMouseButton (3 + 128)
#define  wsR4MouseButton (4 + 128)
#define  wsR5MouseButton (5 + 128)
#define  wsEnterWindow   253
#define  wsLeaveWindow   254
#define  wsMoveMouse     255

#define  wsShowMouseCursor   1
#define  wsMouse             1
#define  wsHideMouseCursor   0
#define  wsNoMouse           0
#define  wsHandleMouseButton 2
#define  wsHandleMouseMove   4

#define  wsHideFrame    0
#define  wsNoFrame      0
#define  wsShowFrame    1
#define  wsFrame        1
#define  wsMaxSize      2
#define  wsMinSize      4
#define  wsShowWindow   8
#define  wsHideWindow   16
#define  wsOverredirect 32

#define  wsNoBorder 0

#define  wsSysName "AutoSpace Window System LiTe"

#define wsRGB32 1
#define wsBGR32 2
#define wsRGB24 3
#define wsBGR24 4
#define wsRGB16 5
#define wsBGR16 6
#define wsRGB15 7
#define wsBGR15 8

#define wsWindowVisible          1
#define wsWindowPartialVisible   2
#define wsWindowNotVisible       4
#define wsWindowMapped           8
#define wsWindowUnmapped        16
#define wsWindowFocusIn         32
#define wsWindowFocusOut        64
#define wsWindowExpose         128
#define wsWindowRolled         256
#define wsWindowClosed         512

#define wsNone       0
#define wsMapped     1
#define wsFocused    2
#define wsVisible    3
#define wsNotVisible 4
#define wsPVisible   5
#define wsRolled     6

#define wsWMUnknown  0
#define wsWMNetWM    1
#define wsWMKDE      2
#define wsWMIceWM    3
#define wsWMWMaker   4

typedef   void (*wsTReDraw)( void );
typedef   void (*wsTReSize)( unsigned int X,unsigned int Y,unsigned int width,unsigned int height );
typedef   void (*wsTIdle)( void );
typedef   void (*wsTKeyHandler)( int KeyCode,int Type,int Key );
typedef   void (*wsTMouseHandler)( int Button,int X,int Y,int RX,int RY  );
typedef   void (*wsTDNDHandler)( int num,char ** str );

typedef struct
{
 Window               WindowID;
 Window               Parent;
 int                  X,Y,Width,Height;
 int                  OldX,OldY,OldWidth,OldHeight;
 int                  MaxX,MaxY;
 int                  isFullScreen;
 int                  BorderWidth;
 int                  Property;
 unsigned char *      bImage;
 XImage        *      xImage;
 Pixmap               Mask;
 int                  Decorations;

 int                  State;
 int                  Visible;
 int                  Mapped;
 int                  Focused;
 int                  Rolled;

 wsTReDraw            ReDraw;
 wsTReSize            ReSize;
 wsTIdle              Idle;
 wsTKeyHandler        KeyHandler;
 wsTMouseHandler      MouseHandler;
 wsTDNDHandler        DandDHandler;

 int                  Alt;
 int                  Shift;
 int                  Control;
 int                  NumLock;
 int                  CapsLock;
// --- Misc -------------------------------------------------------------------------------------

 Atom                 AtomDeleteWindow;
 Atom                 AtomTakeFocus;
 Atom                 AtomRolle;
 Atom                 AtomProtocols;
 Atom                 AtomsProtocols[3];
 Atom                 AtomLeaderClient;
 Atom                 AtomRemote;
 Atom		      AtomWMSizeHint;
 Atom		      AtomWMNormalHint;

 XShmSegmentInfo      Shminfo;
 unsigned char      * ImageData;
 unsigned short int * ImageDataw;
 unsigned int       * ImageDatadw;
 GC                   wGC;
 XGCValues            wGCV;
 unsigned long        WindowMask;
 XVisualInfo          VisualInfo;
 XSetWindowAttributes WindowAttrib;
 XSizeHints           SizeHint;
 XWMHints             WMHints;

 XFontStruct        * Font;
 int                  FontHeight;

 Cursor               wsCursor;
 char                 wsCursorData[1];
 Pixmap               wsCursorPixmap;
 int                  wsMouseEventType;
 XColor               wsColor;
} wsTWindow;

extern int                  wsMaxX;
extern int                  wsMaxY;
extern int                  wsOrgX;
extern int                  wsOrgY;

extern Display            * wsDisplay;
extern int                  wsScreen;
extern Window               wsRootWin;
extern int		    wsLayer;

extern unsigned char      * wsImageData;

extern XEvent               wsEvent;

extern int                  wsDepthOnScreen;
extern int                  wsRedMask;
extern int                  wsGreenMask;
extern int                  wsBlueMask;

extern int                  wsUseXShm;

// ----------------------------------------------------------------------------------------------
//  wsKeyTable
// ----------------------------------------------------------------------------------------------
extern unsigned long        wsKeyTable[512];

void wsXDone( void );
void wsXInit( void* disp );

int wsGetDepthOnScreen( void );

void wsDoExit( void );
void wsMainLoop( void );
Bool wsEvents( Display * display, XEvent * Event, XPointer arg );
void wsHandleEvents( void );

// ----------------------------------------------------------------------------------------------
//  wsCrateWindow: create a new window on the screen.
//   X,Y   : window position
//   wX,hY : window size
//   bW    : window frame size
//   cV    : mouse cursor visible
//   D     : "decoration", visible titlebar, etc ...
// ----------------------------------------------------------------------------------------------
void wsCreateWindow( wsTWindow * win, int X, int Y, int wX, int hY, int bW, int cV, unsigned char D, char * label );
void wsDestroyWindow( wsTWindow * win );
void wsMoveWindow( wsTWindow * win, int b, int x, int y );
void wsResizeWindow( wsTWindow * win, int sx, int sy );
void wsIconify( wsTWindow win );
void wsMoveTopWindow( Display * wsDisplay, Window win );
void wsSetBackground( wsTWindow * win, int color );
void wsSetForegroundRGB( wsTWindow * win, int r, int g, int b );
void wsSetBackgroundRGB( wsTWindow * win, int r, int g, int b );
#define wsClearWindow( win ) XClearWindow( wsDisplay, win.WindowID )
void wsSetTitle( wsTWindow * win, char * name );
void wsVisibleWindow( wsTWindow * win, int show );
void wsWindowDecoration( wsTWindow * win, long d );
void wsSetLayer( Display * wsDisplay, Window win, int layer );
void wsFullScreen( wsTWindow * win );
void wsPostRedisplay( wsTWindow * win );
void wsSetShape( wsTWindow * win, char * data );
void wsSetIcon( Display * dsp, Window win, Pixmap icon, Pixmap mask );

// ----------------------------------------------------------------------------------------------
//    Draw string at x,y with fc ( foreground color ) and bc ( background color ).
// ----------------------------------------------------------------------------------------------
void wsDrawString( wsTWindow win, int x, int y, char * str, int fc, int bc );
int  wsTextWidth( wsTWindow win, char * str );

// ----------------------------------------------------------------------------------------------
//    Show / hide mouse cursor.
// ----------------------------------------------------------------------------------------------
void wsVisibleMouse( wsTWindow * win, int m );
void wsSetMousePosition( wsTWindow * win, int x, int y );

// ----------------------------------------------------------------------------------------------
// Image handling
// ----------------------------------------------------------------------------------------------
void wsCreateImage( wsTWindow * win, int Width, int Height );
void wsConvert( wsTWindow * win, unsigned char * Image, unsigned int Size );
void wsPutImage( wsTWindow * win );
void wsResizeImage( wsTWindow * win, int Width, int Height );
void wsDestroyImage( wsTWindow * win );
int  wsGetOutMask( void );

void wsScreenSaverOn( Display *mDisplay );
void wsScreenSaverOff( Display * mDisplay );

#define wgIsRect( X,Y,tX,tY,bX,bY ) ( ( (X) > (tX) )&&( (Y) > (tY) )&&( (X) < (bX) )&&( (Y) < (bY) ) )

#endif /* MPLAYER_GUI_WS_H */
