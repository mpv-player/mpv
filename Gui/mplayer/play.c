#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <signal.h>

#include "../wm/ws.h"
#include "../../config.h"
#include "../../help_mp.h"
#include "../../libvo/x11_common.h"
#include "../../input/input.h"

#include "../app.h"

#include "../wm/wskeys.h"
#include "../wm/widget.h"
#include "../interface.h"

#include "widgets.h"
#include "./mplayer.h"
#include "play.h"

#include "../skin/skin.h"
#include "../skin/font.h"

extern float rel_seek_secs;
extern int abs_seek_pos;

void mplFullScreen( void )
{
 static int sx,sy;

// if ( !guiIntfStruct.Playing )
  {
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
   vo_fs=appMPlayer.subWindow.isFullScreen;     
   wsVisibleWindow( &appMPlayer.subWindow,wsShowWindow );
  }// else { vo_x11_fullscreen(); appMPlayer.subWindow.isFullScreen=vo_fs; }
  
 fullscreen=appMPlayer.subWindow.isFullScreen;
 if ( guiIntfStruct.Playing ) wsSetBackgroundRGB( &appMPlayer.subWindow,0,0,0 );
  else wsSetBackgroundRGB( &appMPlayer.subWindow,appMPlayer.subR,appMPlayer.subG,appMPlayer.subB );
}

extern int mplSubRender;

void mplStop()
{
 guiIntfStruct.Playing=0;
 guiIntfStruct.TimeSec=0;
 guiIntfStruct.Position=0;
 guiIntfStruct.AudioType=0;
// if ( !guiIntfStruct.Playing ) return;
 if ( !appMPlayer.subWindow.isFullScreen )
  {
   wsResizeWindow( &appMPlayer.subWindow,appMPlayer.sub.width,appMPlayer.sub.height );
   wsMoveWindow( &appMPlayer.subWindow,True,appMPlayer.sub.x,appMPlayer.sub.y );
  }
 guiGetEvent( guiCEvent,guiSetStop );
 mplSubRender=1;
 wsSetBackgroundRGB( &appMPlayer.subWindow,appMPlayer.subR,appMPlayer.subG,appMPlayer.subB );
 wsClearWindow( appMPlayer.subWindow );
 wsPostRedisplay( &appMPlayer.subWindow );
}

void mplPlay( void )
{
 if ( ( !guiIntfStruct.Filename )||
      ( guiIntfStruct.Filename[0] == 0 )||
      ( guiIntfStruct.Playing == 1 ) ) return;
 if ( guiIntfStruct.Playing == 2 ) { mplPause(); return; }
 guiGetEvent( guiCEvent,guiSetPlay );
 mplSubRender=0;
 wsSetBackgroundRGB( &appMPlayer.subWindow,0,0,0 );
 wsClearWindow( appMPlayer.subWindow );
// wsPostRedisplay( &appMPlayer.subWindow );
}

void mplPause( void )
{
 mp_cmd_t * cmd = (mp_cmd_t *)calloc( 1,sizeof( *cmd ) );
 cmd->id=MP_CMD_PAUSE;
 cmd->name=strdup("pause");
 mp_input_queue_cmd(cmd);
 mplSubRender=0;
}

void mplState( void )
{
 if ( ( guiIntfStruct.Playing == 0 )||( guiIntfStruct.Playing == 2 ) )
  {
   btnModify( evPlaySwitchToPause,btnReleased );
   btnModify( evPauseSwitchToPlay,btnDisabled );
  }
  else
   {
    btnModify( evPlaySwitchToPause,btnDisabled );
    btnModify( evPauseSwitchToPlay,btnReleased );
   }
}

void mplMPlayerInit( int argc,char* argv[], char *envp[] )
{
 guiIntfStruct.Balance=50.0f;
 guiIntfStruct.StreamType=-1;
}

float mplGetPosition( void )
{ // return 0.0 ... 100.0
 return guiIntfStruct.Position;
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

void ChangeSkin( char * name )
{
 int ret;
// if ( !strcmp( skinName,name ) ) return;
 mainVisible=0;

 appInitStruct( &tmpList );
 skinAppMPlayer=&tmpList;
 fntFreeFont();
 ret=skinRead( name );

 appInitStruct( &tmpList );
 skinAppMPlayer=&appMPlayer;
 appInitStruct( &appMPlayer );
 if ( ret ) name=skinName;
 if ( skinRead( name ) )
  {
   mainVisible=1;
   return;
  }

 if ( appMPlayer.menuBase.Bitmap.Image )
  {
   if ( mplMenuDrawBuffer ) free( mplMenuDrawBuffer );
   if ( ( mplMenuDrawBuffer = (unsigned char *)calloc( 1,appMPlayer.menuBase.Bitmap.ImageSize ) ) == NULL )
    { mp_msg( MSGT_GPLAYER,MSGL_STATUS,MSGTR_NEMDB ); return; }
   wsResizeWindow( &appMPlayer.menuWindow,appMPlayer.menuBase.width,appMPlayer.menuBase.height );
   wsResizeImage( &appMPlayer.menuWindow,appMPlayer.menuBase.width,appMPlayer.menuBase.height );
  }

 if ( appMPlayer.sub.Bitmap.Image ) wsResizeImage( &appMPlayer.subWindow,appMPlayer.sub.Bitmap.Width,appMPlayer.sub.Bitmap.Height );
 if ( ( !appMPlayer.subWindow.isFullScreen )&&( !guiIntfStruct.Playing ) )
  {
   wsResizeWindow( &appMPlayer.subWindow,appMPlayer.sub.width,appMPlayer.sub.height );
   wsMoveWindow( &appMPlayer.subWindow,True,appMPlayer.sub.x,appMPlayer.sub.y );
  }
 if ( appMPlayer.sub.Bitmap.Image ) wsConvert( &appMPlayer.subWindow,appMPlayer.sub.Bitmap.Image,appMPlayer.sub.Bitmap.ImageSize );
 if ( !guiIntfStruct.Playing )
  {
   mplSubRender=1;
   wsSetBackgroundRGB( &appMPlayer.subWindow,appMPlayer.subR,appMPlayer.subG,appMPlayer.subB );
   wsClearWindow( appMPlayer.subWindow );
   wsPostRedisplay( &appMPlayer.subWindow );
  }

 if ( mplDrawBuffer ) free( mplDrawBuffer );
 if ( ( mplDrawBuffer = (unsigned char *)calloc( 1,appMPlayer.main.Bitmap.ImageSize ) ) == NULL )
  { mp_msg( MSGT_GPLAYER,MSGL_STATUS,MSGTR_NEMDB ); return; }
 wsVisibleWindow( &appMPlayer.mainWindow,wsHideWindow );
 wsResizeWindow( &appMPlayer.mainWindow,appMPlayer.main.width,appMPlayer.main.height );
 wsMoveWindow( &appMPlayer.mainWindow,True,appMPlayer.main.x,appMPlayer.main.y );
 wsResizeImage( &appMPlayer.mainWindow,appMPlayer.main.width,appMPlayer.main.height );
 wsSetShape( &appMPlayer.mainWindow,appMPlayer.main.Mask.Image );
 wsWindowDecoration( &appMPlayer.mainWindow,appMPlayer.mainDecoration );
 mainVisible=1; mplMainRender=1; wsPostRedisplay( &appMPlayer.mainWindow );
 wsVisibleWindow( &appMPlayer.mainWindow,wsShowWindow );

 btnModify( evSetVolume,guiIntfStruct.Volume );
 btnModify( evSetBalance,guiIntfStruct.Balance );
 btnModify( evSetMoviePosition,guiIntfStruct.Position );
 btnModify( evFullScreen,!appMPlayer.subWindow.isFullScreen );
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
 if ( !fname ) return;
 if ( guiIntfStruct.Filename ) free( guiIntfStruct.Filename );
 guiIntfStruct.Filename=strdup( fname );
}

void mplPrev( void )
{
 int stop = 0;
 switch ( guiIntfStruct.StreamType )
  {
//   case STREAMTYPE_FILE:
   case STREAMTYPE_DVD:
	if ( guiIntfStruct.Playing == 2 ) break;
	if ( --guiIntfStruct.DVD.current_chapter == 0 )
	 {
	  guiIntfStruct.DVD.current_chapter=1;
	  if ( --guiIntfStruct.DVD.current_title <= 0 ) { guiIntfStruct.DVD.current_title=1; stop=1; }
	 }
	guiIntfStruct.Track=guiIntfStruct.DVD.current_title;
	if ( stop ) mplEventHandling( evStop,0 );
	if ( guiIntfStruct.Playing == 1 ) mplEventHandling( evPlay,0 );
	break;
  }
}

void mplNext( void )
{
 int stop = 0;
 switch ( guiIntfStruct.StreamType )
  {
//   case STREAMTYPE_FILE:
   case STREAMTYPE_DVD:
	if ( guiIntfStruct.DVD.current_chapter++ == guiIntfStruct.DVD.chapters )
	 {
	  guiIntfStruct.DVD.current_chapter=1;
	  if ( ++guiIntfStruct.DVD.current_title > guiIntfStruct.DVD.titles ) { guiIntfStruct.DVD.current_title=guiIntfStruct.DVD.titles; stop=1; }
	 }
	guiIntfStruct.Track=guiIntfStruct.DVD.current_title;
	if ( stop ) mplEventHandling( evStop,0 );
	if ( guiIntfStruct.Playing == 1 ) mplEventHandling( evPlay,0 );
	break;
  }
}
