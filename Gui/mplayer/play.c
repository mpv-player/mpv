
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <signal.h>

int    mplParent = 1;

int    moviex,moviey,moviewidth,movieheight;

#include "../app.h"

#include "../wm/ws.h"
#include "../wm/wskeys.h"
#include "../wm/widget.h"

#include "../../config.h"
#include "../../help_mp.h"
#include "../../libvo/x11_common.h"

#include "widgets.h"
#include "./mplayer.h"
#include "psignal.h"
#include "play.h"

#include "../skin/skin.h"
#include "../error.h"

mplCommStruct * mplShMem;
char          * Filename = NULL;

extern float rel_seek_secs;
extern int abs_seek_pos;

void mplFullScreen( void )
{
// if ( appMPlayer.subWindow.isFullScreen )
//  {
//  }
 wsFullScreen( &appMPlayer.subWindow ); 
 mplResize( 0,0,appMPlayer.subWindow.Width,appMPlayer.subWindow.Height );
}

extern int mplSubRender;

void mplStop()
{
 if ( !mplShMem->Playing ) return;
 mplShMem->Playing=0;
 mplShMem->TimeSec=0;
 mplShMem->Position=0;
 mplShMem->AudioType=0;
 if ( !appMPlayer.subWindow.isFullScreen )
  {
   wsMoveWindow( &appMPlayer.subWindow,appMPlayer.sub.x,appMPlayer.sub.y );
   wsResizeWindow( &appMPlayer.subWindow,appMPlayer.sub.width,appMPlayer.sub.height );
  }
 mplSubRender=1;
 wsPostRedisplay( &appMPlayer.subWindow );
}

void mplPlay( void )
{
 if ( ( mplShMem->Filename[0] == 0 )||
      ( mplShMem->Playing == 1 ) ) return;
 if ( mplShMem->Playing == 2 ) { mplPause(); return; }
 mplShMem->Playing=1;
 mplSubRender=0;
 wsSetBackgroundRGB( &appMPlayer.subWindow,0,0,0 );
 wsClearWindow( appMPlayer.subWindow ); 
 wsPostRedisplay( &appMPlayer.subWindow );
}

void mplPause( void )
{
 switch( mplShMem->Playing )
  {
   case 1: // playing
        mplShMem->Playing=2;
	btnModify( evPlaySwitchToPause,btnReleased );
	btnModify( evPauseSwitchToPlay,btnDisabled );
	break;
    case 2: // paused
	mplShMem->Playing=1;
	btnModify( evPlaySwitchToPause,btnDisabled );
	btnModify( evPauseSwitchToPlay,btnReleased );
	break;
  }
 mplSubRender=0;
}

void mplResize( unsigned int X,unsigned int Y,unsigned int width,unsigned int height )
{
// printf( "----resize---> %dx%d --- \n",width,height );
 vo_setwindowsize( width,height );
 vo_resize=1;
}

void mplMPlayerInit( int argc,char* argv[], char *envp[] )
{
 struct sigaction sa;

 mplShMem=calloc( 1,sizeof( mplCommStruct ) );
 mplShMem->Balance=50.0f;
 memset(&sa, 0, sizeof(sa));
 sa.sa_handler = mplMainSigHandler;
 sigaction( SIGTYPE,&sa,NULL );
}

float mplGetPosition( void )
{ // return 0.0 ... 100.0
 return mplShMem->Position;
}

void mplRelSeek( float s )
{ // -+s
 rel_seek_secs=s; abs_seek_pos=0;
}

void mplAbsSeek( float s )
{ // 0.0 ... 100.0
 rel_seek_secs=0.01*s; abs_seek_pos=3;
}

listItems tmpList;

void ChangeSkin( void )
{
 int ret;
 if ( !strcmp( skinName,gtkShMem->sb.name ) ) return;
#ifdef DEBUG
 dbprintf( 1,"[psignal] skin: %s\n",gtkShMem->sb.name );
#endif

 mainVisible=0;

 appInitStruct( &tmpList );
 skinAppMPlayer=&tmpList;
 fntFreeFont();
 ret=skinRead( gtkShMem->sb.name );

 appInitStruct( &tmpList );
 skinAppMPlayer=&appMPlayer;
 appInitStruct( &appMPlayer );
 if ( !ret ) strcpy( skinName,gtkShMem->sb.name );
 skinRead( skinName );
 if ( ret )
  {
   mainVisible=1;
   return;
  }

 if ( appMPlayer.menuBase.Bitmap.Image )
  {
   if ( mplMenuDrawBuffer ) free( mplMenuDrawBuffer );
   if ( ( mplMenuDrawBuffer = (unsigned char *)calloc( 1,appMPlayer.menuBase.Bitmap.ImageSize ) ) == NULL )
    { message( False,MSGTR_NEMDB ); return; }
   wsResizeWindow( &appMPlayer.menuWindow,appMPlayer.menuBase.width,appMPlayer.menuBase.height );
   wsResizeImage( &appMPlayer.menuWindow,appMPlayer.menuBase.width,appMPlayer.menuBase.height );
  }

 mplSkinChanged=1;
 if ( appMPlayer.sub.Bitmap.Image ) wsResizeImage( &appMPlayer.subWindow,appMPlayer.sub.Bitmap.Width,appMPlayer.sub.Bitmap.Height );
 if ( !mplShMem->Playing )
  {
   mplSkinChanged=0;
   if ( !appMPlayer.subWindow.isFullScreen ) 
    {
     wsResizeWindow( &appMPlayer.subWindow,appMPlayer.sub.width,appMPlayer.sub.height );
     wsMoveWindow( &appMPlayer.subWindow,appMPlayer.sub.x,appMPlayer.sub.y );
    } 
   wsSetBackgroundRGB( &appMPlayer.subWindow,appMPlayer.subR,appMPlayer.subG,appMPlayer.subB );
   wsClearWindow( appMPlayer.subWindow );
   mplSubRender=1; wsPostRedisplay( &appMPlayer.subWindow );
  }

 if ( mplDrawBuffer ) free( mplDrawBuffer );
 if ( ( mplDrawBuffer = (unsigned char *)calloc( 1,appMPlayer.main.Bitmap.ImageSize ) ) == NULL )
  { message( False,MSGTR_NEMDB ); return; }
 wsVisibleWindow( &appMPlayer.mainWindow,wsHideWindow );
 wsResizeWindow( &appMPlayer.mainWindow,appMPlayer.main.width,appMPlayer.main.height );
 wsMoveWindow( &appMPlayer.mainWindow,appMPlayer.main.x,appMPlayer.main.y );
 wsResizeImage( &appMPlayer.mainWindow,appMPlayer.main.width,appMPlayer.main.height );
 wsSetShape( &appMPlayer.mainWindow,appMPlayer.main.Mask.Image );
 mainVisible=1; mplMainRender=1; wsPostRedisplay( &appMPlayer.mainWindow );
 wsWindowDecoration( &appMPlayer.mainWindow,appMPlayer.mainDecoration );
 wsVisibleWindow( &appMPlayer.mainWindow,wsShowWindow );
   
 btnModify( evSetVolume,mplShMem->Volume );
 btnModify( evSetBalance,mplShMem->Balance );
 btnModify( evSetMoviePosition,mplShMem->Position );
}

void EventHandling( void )
{
 wsHandleEvents();mplTimerHandler(0); // handle GUI timer events
 if ( mplShMem->SkinChange ) { ChangeSkin(); mplShMem->SkinChange=0;  }
}
