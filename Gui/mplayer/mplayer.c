
#include <inttypes.h>

#include "../app.h"
#include "../skin/skin.h"
#include "../wm/ws.h"
#include "../wm/wsxdnd.h"

#include "../../config.h"
#include "../../help_mp.h"
#include "../../libvo/x11_common.h"

void mplEventHandling( int msg,float param );

extern void mplMainDraw( wsParamDisplay );
extern void mplEventHandling( int msg,float param );
extern void mplMainMouseHandle( int Button,int X,int Y,int RX,int RY );
extern void mplMainKeyHandle( int KeyCode,int Type,int Key );
extern void mplDandDHandler(int num,char** files);
extern void mplSubDraw( wsParamDisplay );
extern void mplSubMouseHandle( int Button,int X,int Y,int RX,int RY );

#include "widgets.h"
#include "play.h"
#include "menu.h"

void mplInit( void * disp )
{
 int i;

 if ( ( mplDrawBuffer = (unsigned char *)calloc( 1,appMPlayer.main.Bitmap.ImageSize ) ) == NULL )
  {
   fprintf( stderr,MSGTR_NEMDB );
   exit( 0 );
  }

 wsCreateWindow( &appMPlayer.subWindow,
  appMPlayer.sub.x,appMPlayer.sub.y,appMPlayer.sub.width,appMPlayer.sub.height,
  wsNoBorder,wsShowMouseCursor|wsHandleMouseButton|wsHandleMouseMove,wsShowFrame|wsHideWindow,"ViDEO" );

 wsDestroyImage( &appMPlayer.subWindow );
 wsCreateImage( &appMPlayer.subWindow,appMPlayer.sub.Bitmap.Width,appMPlayer.sub.Bitmap.Height );
 wsXDNDMakeAwareness(&appMPlayer.subWindow);

 vo_setwindow( appMPlayer.subWindow.WindowID, appMPlayer.subWindow.wGC );

// i=wsHideFrame|wsMaxSize|wsHideWindow;
// if ( appMPlayer.mainDecoration ) i=wsShowFrame|wsMaxSize|wsHideWindow;
 i=wsShowFrame|wsMaxSize|wsHideWindow;
 wsCreateWindow( &appMPlayer.mainWindow,
  appMPlayer.main.x,appMPlayer.main.y,appMPlayer.main.width,appMPlayer.main.height,
  wsNoBorder,wsShowMouseCursor|wsHandleMouseButton|wsHandleMouseMove,i,"MPlayer" ); //wsMinSize|

 wsSetShape( &appMPlayer.mainWindow,appMPlayer.main.Mask.Image );
 wsXDNDMakeAwareness(&appMPlayer.mainWindow);

 mplMenuInit();

 #ifdef DEBUG
  mp_msg( MSGT_GPLAYER,MSGL_DBG2,"[main] Depth on screen: %d\n",wsDepthOnScreen );
  mp_msg( MSGT_GPLAYER,MSGL_DBG2,"[main] parent: 0x%x\n",(int)appMPlayer.mainWindow.WindowID );
  mp_msg( MSGT_GPLAYER,MSGL_DBG2,"[main] sub: 0x%x\n",(int)appMPlayer.subWindow.WindowID );
 #endif

 appMPlayer.mainWindow.ReDraw=mplMainDraw;
 appMPlayer.mainWindow.MouseHandler=mplMainMouseHandle;
 appMPlayer.mainWindow.KeyHandler=mplMainKeyHandle;
 appMPlayer.mainWindow.DandDHandler=mplDandDHandler;

 appMPlayer.subWindow.ReDraw=mplSubDraw;
 appMPlayer.subWindow.MouseHandler=mplSubMouseHandle;
 appMPlayer.subWindow.KeyHandler=mplMainKeyHandle;
 appMPlayer.subWindow.DandDHandler=mplDandDHandler;

 wsSetBackgroundRGB( &appMPlayer.subWindow,appMPlayer.subR,appMPlayer.subG,appMPlayer.subB );
 wsClearWindow( appMPlayer.subWindow );
 if ( appMPlayer.sub.Bitmap.Image ) wsConvert( &appMPlayer.subWindow,appMPlayer.sub.Bitmap.Image,appMPlayer.sub.Bitmap.ImageSize );

 btnModify( evSetVolume,guiIntfStruct.Volume );
 btnModify( evSetBalance,guiIntfStruct.Balance );
 btnModify( evSetMoviePosition,guiIntfStruct.Position );

 wsSetIcon( wsDisplay,appMPlayer.mainWindow.WindowID,guiIcon,guiIconMask );
 wsSetIcon( wsDisplay,appMPlayer.subWindow.WindowID,guiIcon,guiIconMask );
 
 guiIntfStruct.Playing=0;

 if ( !appMPlayer.mainDecoration ) wsWindowDecoration( &appMPlayer.mainWindow,0 );
 
 wsVisibleWindow( &appMPlayer.mainWindow,wsShowWindow );
#if 1
 wsVisibleWindow( &appMPlayer.subWindow,wsShowWindow );

 {
  XEvent xev;
  do { XNextEvent( wsDisplay,&xev ); } while ( xev.type != MapNotify || xev.xmap.event != appMPlayer.subWindow.WindowID );
  appMPlayer.subWindow.Mapped=wsMapped;
 }

 if ( fullscreen )
  {
   mplFullScreen();
   btnModify( evFullScreen,btnPressed );
  }
#endif
 mplSubRender=1;
}

