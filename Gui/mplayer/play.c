
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
 static int sx,sy;

 wsVisibleWindow( &appMPlayer.subWindow,wsHideWindow );
 if ( appMPlayer.subWindow.isFullScreen )
  {
   wsResizeWindow( &appMPlayer.subWindow,sx,sy );
   wsMoveWindow( &appMPlayer.subWindow,True,appMPlayer.sub.x,appMPlayer.sub.y );
   wsWindowDecoration( &appMPlayer.subWindow,appMPlayer.subWindow.Decorations );
   appMPlayer.subWindow.isFullScreen=0;
  }
  else
   {
    sx=appMPlayer.subWindow.Width; sy=appMPlayer.subWindow.Height;
    wsResizeWindow( &appMPlayer.subWindow,wsMaxX,wsMaxY );
    wsMoveWindow( &appMPlayer.subWindow,True,0,0 );
    wsWindowDecoration( &appMPlayer.subWindow,0 );
    appMPlayer.subWindow.isFullScreen=1;
   }
 if ( mplShMem->Playing ) wsSetBackgroundRGB( &appMPlayer.subWindow,0,0,0 );
  else wsSetBackgroundRGB( &appMPlayer.subWindow,appMPlayer.subR,appMPlayer.subG,appMPlayer.subB );
 wsVisibleWindow( &appMPlayer.subWindow,wsShowWindow );
 mplResize( 0,0,appMPlayer.subWindow.Width,appMPlayer.subWindow.Height );
}

extern int mplSubRender;

void mplStop()
{
 mplShMem->Playing=0;
 mplShMem->TimeSec=0;
 mplShMem->Position=0;
 mplShMem->AudioType=0;
// if ( !mplShMem->Playing ) return;
 if ( !appMPlayer.subWindow.isFullScreen )
  {
   wsResizeWindow( &appMPlayer.subWindow,appMPlayer.sub.width,appMPlayer.sub.height );
   wsMoveWindow( &appMPlayer.subWindow,True,appMPlayer.sub.x,appMPlayer.sub.y );
  }
 mplSubRender=1;
 wsSetBackgroundRGB( &appMPlayer.subWindow,appMPlayer.subR,appMPlayer.subG,appMPlayer.subB );
 wsClearWindow( appMPlayer.subWindow );
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
 mplShMem->StreamType=-1;
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

 if ( appMPlayer.sub.Bitmap.Image ) wsResizeImage( &appMPlayer.subWindow,appMPlayer.sub.Bitmap.Width,appMPlayer.sub.Bitmap.Height );
 if ( ( !appMPlayer.subWindow.isFullScreen )&&( !mplShMem->Playing ) )
  {
   wsResizeWindow( &appMPlayer.subWindow,appMPlayer.sub.width,appMPlayer.sub.height );
   wsMoveWindow( &appMPlayer.subWindow,True,appMPlayer.sub.x,appMPlayer.sub.y );
  } 
 if ( appMPlayer.sub.Bitmap.Image ) wsConvert( &appMPlayer.subWindow,appMPlayer.sub.Bitmap.Image,appMPlayer.sub.Bitmap.ImageSize );
 if ( !mplShMem->Playing ) 
  {
   mplSubRender=1; 
   wsSetBackgroundRGB( &appMPlayer.subWindow,appMPlayer.subR,appMPlayer.subG,appMPlayer.subB );
   wsClearWindow( appMPlayer.subWindow );
   wsPostRedisplay( &appMPlayer.subWindow );
  }

 if ( mplDrawBuffer ) free( mplDrawBuffer );
 if ( ( mplDrawBuffer = (unsigned char *)calloc( 1,appMPlayer.main.Bitmap.ImageSize ) ) == NULL )
  { message( False,MSGTR_NEMDB ); return; }
 wsVisibleWindow( &appMPlayer.mainWindow,wsHideWindow );
 wsResizeWindow( &appMPlayer.mainWindow,appMPlayer.main.width,appMPlayer.main.height );
 wsMoveWindow( &appMPlayer.mainWindow,True,appMPlayer.main.x,appMPlayer.main.y );
 wsResizeImage( &appMPlayer.mainWindow,appMPlayer.main.width,appMPlayer.main.height );
 wsSetShape( &appMPlayer.mainWindow,appMPlayer.main.Mask.Image );
 wsWindowDecoration( &appMPlayer.mainWindow,appMPlayer.mainDecoration );
 mainVisible=1; mplMainRender=1; wsPostRedisplay( &appMPlayer.mainWindow );
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

void mplResizeToMovieSize( unsigned int width,unsigned int height )
{
 if ( !appMPlayer.subWindow.isFullScreen )
  {
   wsResizeWindow( &appMPlayer.subWindow,width,height );
   wsMoveWindow( &appMPlayer.subWindow,True,appMPlayer.sub.x,appMPlayer.sub.y );
  } 
}

void mplSetFileName( char * fname )
{
 if ( ( fname )&&( gtkShMem ) ) strcpy( gtkShMem->fs.filename,fname );
}
