
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "./mplayer.h"
#include "../events.h"
#include "../app.h"
#include "../skin/skin.h"
#include "../skin/font.h"
#include "../wm/ws.h"
#include "../wm/wskeys.h"
#include "../wm/widget.h"
#include "../bitmap/bitmap.h"
#include "../timer.h"
#include "../language.h"
#include "../error.h"

#include "../../config.h"
#include "../../libvo/x11_common.h"

#define mplMouseTimerConst  10
#define mplRedrawTimerConst 5

int mplMouseTimer  = mplMouseTimerConst;
int mplRedrawTimer = mplRedrawTimerConst;
int mplGeneralTimer = -1;
int mplTimer = 0;

int mplSkinChanged = 0;

void mplMsgHandle( int msg,float param );

#include "widgets.h"
#include "play.h"
#include "menu.h"
#include "mw.h"
#include "sw.h"
#include "widgets.h"

void mplTimerHandler( int signum )
{
 mplTimer++;
 mplMouseTimer--;
 mplRedrawTimer--;
 mplGeneralTimer--;
 if ( mplMouseTimer == 0 ) mplMsgHandle( evHideMouseCursor,0 );
 if ( mplRedrawTimer == 0 ) mplMsgHandle( evRedraw,0 );
 if ( mplGeneralTimer == 0 ) mplMsgHandle( evGeneralTimer,0 );
}

void mplInit( int argc,char* argv[], char *envp[], void* disp )
{
 int i;
 // allocates shmem to gtkShMem
 // fork() a process which runs gtkThreadProc()  [gtkPID]
 gtkInit( argc,argv,envp );
 strcpy( gtkShMem->sb.name,skinName ); 

 // allocates shmem to mplShMem
 // init fields of this struct to default values
 mplMPlayerInit( argc,argv,envp );

 message=mplErrorHandler;  // error messagebox drawing function

 // opens X display, checks for extensions (XShape, DGA etc)
 wsXInit(disp);

 if ( ( mplDrawBuffer = (unsigned char *)calloc( 1,appMPlayer.main.Bitmap.ImageSize ) ) == NULL )
  {
   fprintf( stderr,langNEMDB );
   exit( 0 );
  }

 wsCreateWindow( &appMPlayer.subWindow,
  appMPlayer.sub.x,appMPlayer.sub.y,appMPlayer.sub.width,appMPlayer.sub.height,
  wsNoBorder,wsShowMouseCursor|wsHandleMouseButton|wsHandleMouseMove,wsShowFrame|wsShowWindow,"ViDEO" );

 vo_setwindow(appMPlayer.subWindow.WindowID, appMPlayer.subWindow.wGC);
 vo_setwindowsize( appMPlayer.sub.width,appMPlayer.sub.height );
 
 i=wsHideFrame|wsMaxSize|wsShowWindow;
 if ( appMPlayer.mainDecoration ) i=wsShowFrame|wsMaxSize|wsShowWindow;
 wsCreateWindow( &appMPlayer.mainWindow,
  appMPlayer.main.x,appMPlayer.main.y,appMPlayer.main.width,appMPlayer.main.height,
  wsNoBorder,wsShowMouseCursor|wsHandleMouseButton|wsHandleMouseMove,i,"MPlayer" ); //wsMinSize|

 wsSetShape( &appMPlayer.mainWindow,appMPlayer.main.Mask.Image );

 mplMenuInit();

 #ifdef DEBUG
  dbprintf( 1,"[main] Depth on screen: %d\n",wsDepthOnScreen );
  dbprintf( 1,"[main] parent: 0x%x\n",(int)appMPlayer.mainWindow.WindowID );
  dbprintf( 1,"[main] sub: 0x%x\n",(int)appMPlayer.subWindow.WindowID );
 #endif

 appMPlayer.mainWindow.ReDraw=mplMainDraw;
 appMPlayer.mainWindow.MouseHandler=mplMainMouseHandle;
 appMPlayer.mainWindow.KeyHandler=mplMainKeyHandle;

 appMPlayer.subWindow.ReDraw=mplSubDraw;
 appMPlayer.subWindow.MouseHandler=mplSubMouseHandle;
 appMPlayer.subWindow.KeyHandler=mplMainKeyHandle;
 appMPlayer.subWindow.ReSize=mplResize;

 wsPostRedisplay( &appMPlayer.mainWindow );
 wsPostRedisplay( &appMPlayer.subWindow );

 btnModify( evSetVolume,mplShMem->Volume );
 btnModify( evSetBalance,mplShMem->Balance );
 btnModify( evSetMoviePosition,mplShMem->Position );

 mplShMem->Playing=0;
 
// timerSetHandler( mplTimerHandler );  // various timer hacks
// timerInit();

// wsMainLoop();  // X event handler (calls mplTimerHandler periodically!)

}

void mplDone(){

 dbprintf( 1,"[mplayer] exit.\n" );

 mplStop();
// timerDone();
 gtkDone();  // kills the gtkThreadProc() process
 wsXDone();

}

