/*
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

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>

#include "config.h"
#include "help_mp.h"
#include "libvo/x11_common.h"
#include "libvo/video_out.h"
#include "input/input.h"

#include "gui/wm/ws.h"
#include "gui/wm/wsxdnd.h"

#include "gui/app.h"
#include "gui/wm/wskeys.h"
#include "gui/interface.h"

#include "widgets.h"
#include "gmplayer.h"
#include "play.h"

#include "gui/skin/skin.h"
#include "gui/skin/font.h"

#include "stream/stream.h"

extern float rel_seek_secs;
extern int abs_seek_pos;

int mplGotoTheNext = 1;

void mplFullScreen( void )
{
 if ( guiIntfStruct.NoWindow && guiIntfStruct.Playing ) return;

  if ( ( guiIntfStruct.Playing )&&( appMPlayer.subWindow.isFullScreen ) )
   { 
    appMPlayer.subWindow.OldWidth=guiIntfStruct.MovieWidth; appMPlayer.subWindow.OldHeight=guiIntfStruct.MovieHeight; 
    switch ( appMPlayer.sub.x )
     {
      case -1: appMPlayer.subWindow.OldX=( wsMaxX / 2 ) - ( appMPlayer.subWindow.OldWidth / 2 ) + wsOrgX; break;
      case -2: appMPlayer.subWindow.OldX=wsMaxX - appMPlayer.subWindow.OldWidth + wsOrgX; break;
      default: appMPlayer.subWindow.OldX=appMPlayer.sub.x; break;
     }
    switch ( appMPlayer.sub.y )
     {
      case -1: appMPlayer.subWindow.OldY=( wsMaxY / 2 ) - ( appMPlayer.subWindow.OldHeight / 2 ) + wsOrgY; break;
      case -2: appMPlayer.subWindow.OldY=wsMaxY - appMPlayer.subWindow.OldHeight + wsOrgY; break;
      default: appMPlayer.subWindow.OldY=appMPlayer.sub.y; break;
     }
   }
  if ( guiIntfStruct.Playing || gtkShowVideoWindow ) wsFullScreen( &appMPlayer.subWindow );
  fullscreen=vo_fs=appMPlayer.subWindow.isFullScreen;
  wsSetLayer( wsDisplay,appMPlayer.mainWindow.WindowID,appMPlayer.subWindow.isFullScreen );
  if ( appMPlayer.menuIsPresent ) wsSetLayer( wsDisplay,appMPlayer.menuWindow.WindowID,appMPlayer.subWindow.isFullScreen );

 if ( guiIntfStruct.Playing ) wsSetBackgroundRGB( &appMPlayer.subWindow,0,0,0 );
  else wsSetBackgroundRGB( &appMPlayer.subWindow,appMPlayer.sub.R,appMPlayer.sub.G,appMPlayer.sub.B );
}

void mplEnd( void )
{
 plItem * next;

 if ( !mplGotoTheNext && guiIntfStruct.Playing) { mplGotoTheNext=1; return; }

 if ( guiIntfStruct.Playing && (next=gtkSet( gtkGetNextPlItem,0,NULL )) && plLastPlayed != next )
  {
   plLastPlayed=next;
   guiSetDF( guiIntfStruct.Filename,next->path,next->name );
   guiIntfStruct.StreamType=STREAMTYPE_FILE;
   guiIntfStruct.FilenameChanged=guiIntfStruct.NewPlay=1;
   gfree( (void **)&guiIntfStruct.AudioFile );
   gfree( (void **)&guiIntfStruct.Subtitlename );
  } 
  else
    {
     if ( guiIntfStruct.FilenameChanged || guiIntfStruct.NewPlay ) return;

     guiIntfStruct.TimeSec=0;
     guiIntfStruct.Position=0;
     guiIntfStruct.AudioType=0;
     guiIntfStruct.NoWindow=False;

#ifdef CONFIG_DVDREAD
     guiIntfStruct.DVD.current_title=1;
     guiIntfStruct.DVD.current_chapter=1;
     guiIntfStruct.DVD.current_angle=1;
#endif

     if ( !appMPlayer.subWindow.isFullScreen && gtkShowVideoWindow)
      {
       wsResizeWindow( &appMPlayer.subWindow,appMPlayer.sub.width,appMPlayer.sub.height );
       wsMoveWindow( &appMPlayer.subWindow,True,appMPlayer.sub.x,appMPlayer.sub.y );
      }
      else wsVisibleWindow( &appMPlayer.subWindow,wsHideWindow );
     guiGetEvent( guiCEvent,guiSetStop );
     mplSubRender=1;
     wsSetBackgroundRGB( &appMPlayer.subWindow,appMPlayer.sub.R,appMPlayer.sub.G,appMPlayer.sub.B );
     wsClearWindow( appMPlayer.subWindow );
     wsPostRedisplay( &appMPlayer.subWindow );
    }
}

void mplPlay( void )
{
 if ( ( !guiIntfStruct.Filename )||
      ( guiIntfStruct.Filename[0] == 0 )||
      ( guiIntfStruct.Playing == 1 ) ) return;
 if ( guiIntfStruct.Playing == 2 ) { mplPause(); return; }
 guiGetEvent( guiCEvent,(void *)guiSetPlay );
 mplSubRender=0;
 wsSetBackgroundRGB( &appMPlayer.subWindow,0,0,0 );
 wsClearWindow( appMPlayer.subWindow );
}

void mplPause( void )
{
 if ( !guiIntfStruct.Playing ) return;
 if ( guiIntfStruct.Playing == 1 )
  {
   mp_cmd_t * cmd = calloc( 1,sizeof( *cmd ) );
   cmd->id=MP_CMD_PAUSE;
   cmd->name=strdup("pause");
   mp_input_queue_cmd(cmd);
  } else guiIntfStruct.Playing=1;
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
 if ( guiIntfStruct.StreamType == STREAMTYPE_STREAM ) return;
 rel_seek_secs=0.01*s; abs_seek_pos=3;
}

listItems tmpList;

void ChangeSkin( char * name )
{
 int ret;
 int prev = appMPlayer.menuIsPresent;
 int bprev = appMPlayer.barIsPresent;

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

// --- reload menu window

 if ( prev && appMPlayer.menuIsPresent )
  {
   if ( mplMenuDrawBuffer ) free( mplMenuDrawBuffer );
   if ( ( mplMenuDrawBuffer = calloc( 1,appMPlayer.menuBase.Bitmap.ImageSize ) ) == NULL )
    { mp_msg( MSGT_GPLAYER,MSGL_STATUS,MSGTR_NEMDB ); return; }
   wsResizeWindow( &appMPlayer.menuWindow,appMPlayer.menuBase.width,appMPlayer.menuBase.height );
   wsResizeImage( &appMPlayer.menuWindow,appMPlayer.menuBase.width,appMPlayer.menuBase.height );
   wsSetShape( &appMPlayer.menuWindow,appMPlayer.menuBase.Mask.Image );
   wsVisibleWindow( &appMPlayer.menuWindow,wsHideWindow );
  } else { mplMenuInit(); }

// --- reload sub window
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
   wsSetBackgroundRGB( &appMPlayer.subWindow,appMPlayer.sub.R,appMPlayer.sub.G,appMPlayer.sub.B );
   wsClearWindow( appMPlayer.subWindow );
   wsPostRedisplay( &appMPlayer.subWindow );
  }

// --- reload play bar
 if ( bprev ) wsDestroyWindow( &appMPlayer.barWindow );
 mplPBInit();

// --- reload main window
 if ( mplDrawBuffer ) free( mplDrawBuffer );
 if ( ( mplDrawBuffer = calloc( 1,appMPlayer.main.Bitmap.ImageSize ) ) == NULL )
  { mp_msg( MSGT_GPLAYER,MSGL_STATUS,MSGTR_NEMDB ); return; }

 wsDestroyWindow( &appMPlayer.mainWindow );

 wsCreateWindow( &appMPlayer.mainWindow,
   appMPlayer.main.x,appMPlayer.main.y,appMPlayer.main.width,appMPlayer.main.height,
   wsNoBorder,wsShowMouseCursor|wsHandleMouseButton|wsHandleMouseMove,wsShowFrame|wsMaxSize|wsHideWindow,"MPlayer" );
 wsCreateImage( &appMPlayer.mainWindow,appMPlayer.main.Bitmap.Width,appMPlayer.main.Bitmap.Height );
 wsSetShape( &appMPlayer.mainWindow,appMPlayer.main.Mask.Image );
 wsSetIcon( wsDisplay,appMPlayer.mainWindow.WindowID,guiIcon,guiIconMask );

 appMPlayer.mainWindow.ReDraw=(void *)mplMainDraw;
 appMPlayer.mainWindow.MouseHandler=mplMainMouseHandle;
 appMPlayer.mainWindow.KeyHandler=mplMainKeyHandle;
 appMPlayer.mainWindow.DandDHandler=mplDandDHandler;

 wsXDNDMakeAwareness( &appMPlayer.mainWindow );
 if ( !appMPlayer.mainDecoration ) wsWindowDecoration( &appMPlayer.mainWindow,0 );
 wsVisibleWindow( &appMPlayer.mainWindow,wsShowWindow );
 mainVisible=1;
// ---

 btnModify( evSetVolume,guiIntfStruct.Volume );
 btnModify( evSetBalance,guiIntfStruct.Balance );
 btnModify( evSetMoviePosition,guiIntfStruct.Position );
 btnModify( evFullScreen,!appMPlayer.subWindow.isFullScreen );

 wsSetLayer( wsDisplay,appMPlayer.mainWindow.WindowID,appMPlayer.subWindow.isFullScreen );
 wsSetLayer( wsDisplay,appMPlayer.menuWindow.WindowID,appMPlayer.subWindow.isFullScreen );
 
}

void mplSetFileName( char * dir,char * name,int type )
{
 if ( !name ) return;
 
 if ( !dir ) guiSetFilename( guiIntfStruct.Filename,name )
  else guiSetDF( guiIntfStruct.Filename,dir,name );

// filename=guiIntfStruct.Filename;
 guiIntfStruct.StreamType=type;
 gfree( (void **)&guiIntfStruct.AudioFile );
 gfree( (void **)&guiIntfStruct.Subtitlename );
}

void mplCurr( void )
{
 plItem * curr;
 int      stop = 0;
 
 if ( guiIntfStruct.Playing == 2 ) return;
 switch ( guiIntfStruct.StreamType )
  {
#ifdef CONFIG_DVDREAD
   case STREAMTYPE_DVD:
	break;
#endif
#ifdef CONFIG_VCD
   case STREAMTYPE_VCD:
	break;
#endif
   default: 
	if ( (curr=gtkSet( gtkGetCurrPlItem,0,NULL)) )
	 {
	  mplSetFileName( curr->path,curr->name,STREAMTYPE_FILE );
	  mplGotoTheNext=0;
	  break;
	 }
	return;
  }
 if ( stop ) mplEventHandling( evStop,0 );
 if ( guiIntfStruct.Playing == 1 ) mplEventHandling( evPlay,0 );
}


void mplPrev( void )
{
 plItem * prev;
 int      stop = 0;
 
 if ( guiIntfStruct.Playing == 2 ) return;
 switch ( guiIntfStruct.StreamType )
  {
#ifdef CONFIG_DVDREAD
   case STREAMTYPE_DVD:
	if ( --guiIntfStruct.DVD.current_chapter == 0 )
	 {
	  guiIntfStruct.DVD.current_chapter=1;
	  if ( --guiIntfStruct.DVD.current_title <= 0 ) { guiIntfStruct.DVD.current_title=1; stop=1; }
	 }
	guiIntfStruct.Track=guiIntfStruct.DVD.current_title;
	break;
#endif
#ifdef CONFIG_VCD
   case STREAMTYPE_VCD:
	if ( --guiIntfStruct.Track == 0 ) { guiIntfStruct.Track=1; stop=1; }
	break;
#endif
   default: 
	if ( (prev=gtkSet( gtkGetPrevPlItem,0,NULL)) )
	 {
	  mplSetFileName( prev->path,prev->name,STREAMTYPE_FILE );
	  mplGotoTheNext=0;
	  break;
	 }
	return;
  }
 if ( stop ) mplEventHandling( evStop,0 );
 if ( guiIntfStruct.Playing == 1 ) mplEventHandling( evPlay,0 );
}

void mplNext( void )
{
 int      stop = 0;
 plItem * next;

 if ( guiIntfStruct.Playing == 2 ) return;
 switch ( guiIntfStruct.StreamType )
  {
#ifdef CONFIG_DVDREAD
   case STREAMTYPE_DVD:
	if ( guiIntfStruct.DVD.current_chapter++ == guiIntfStruct.DVD.chapters )
	 {
	  guiIntfStruct.DVD.current_chapter=1;
	  if ( ++guiIntfStruct.DVD.current_title > guiIntfStruct.DVD.titles ) { guiIntfStruct.DVD.current_title=guiIntfStruct.DVD.titles; stop=1; }
	 }
	guiIntfStruct.Track=guiIntfStruct.DVD.current_title;
	break;
#endif
#ifdef CONFIG_VCD
   case STREAMTYPE_VCD:
	if ( ++guiIntfStruct.Track > guiIntfStruct.VCDTracks ) { guiIntfStruct.Track=guiIntfStruct.VCDTracks; stop=1; }
	break;
#endif
   default:
	if ( (next=gtkSet( gtkGetNextPlItem,0,NULL)) ) 
	 { 
	  mplSetFileName( next->path,next->name,STREAMTYPE_FILE );
	  mplGotoTheNext=0;
	  break;
	 }
	return;
  }
 if ( stop ) mplEventHandling( evStop,0 );
 if ( guiIntfStruct.Playing == 1 ) mplEventHandling( evPlay,0 );
}
