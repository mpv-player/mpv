/*
 * main window
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

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "config.h"
#include "gui/app.h"
#include "gui/skin/font.h"
#include "gui/skin/skin.h"
#include "gui/wm/ws.h"

#include "help_mp.h"
#include "libvo/x11_common.h"
#include "libvo/fastmemcpy.h"

#include "stream/stream.h"
#include "mixer.h"
#include "libvo/sub.h"

#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "codec-cfg.h"

#include "gmplayer.h"
#include "play.h"
#include "widgets.h"
#include "gui_common.h"

unsigned int GetTimerMS( void );
unsigned int GetTimer( void );

unsigned char * mplPBDrawBuffer = NULL;
int		mplPBVisible = 0;
int  		mplPBLength = 0;
int		mplPBFade = 0;

static void mplPBDraw( void )
{
 int x;

 if ( !appMPlayer.subWindow.isFullScreen ) return;
 if ( !mplPBVisible || !appMPlayer.barIsPresent ) return;

// appMPlayer.bar.x=( appMPlayer.subWindow.Width - appMPlayer.bar.width ) / 2;
 switch( appMPlayer.bar.x )
  {
   case -1: x=( appMPlayer.subWindow.Width - appMPlayer.bar.width ) / 2; break;
   case -2: x=( appMPlayer.subWindow.Width - appMPlayer.bar.width ); break;
   default: x=appMPlayer.bar.x;
  }

 switch ( mplPBFade )
  {
   case 1: // fade in
        mplPBLength--;
        if ( appMPlayer.subWindow.Height - appMPlayer.bar.height >= mplPBLength )
	 {
	  mplPBLength=appMPlayer.subWindow.Height - appMPlayer.bar.height;
	  mplPBFade=0;
	  vo_mouse_autohide=0;
	 }
        wsMoveWindow( &appMPlayer.barWindow,0,x,mplPBLength );
	break;
   case 2: // fade out
	mplPBLength+=10;
	if ( mplPBLength > appMPlayer.subWindow.Height )
	 {
	  mplPBLength=appMPlayer.subWindow.Height;
	  mplPBFade=mplPBVisible=0;
          vo_mouse_autohide=1;
          wsVisibleWindow( &appMPlayer.barWindow,wsHideWindow );
	  return;
	 }
        wsMoveWindow( &appMPlayer.barWindow,0,x,mplPBLength );
	break;
  }

// --- render
 if ( appMPlayer.barWindow.State == wsWindowExpose )
  {
   btnModify( evSetMoviePosition,guiIntfStruct.Position );
   btnModify( evSetVolume,guiIntfStruct.Volume );

   vo_mouse_autohide=0;

   fast_memcpy( mplPBDrawBuffer,appMPlayer.bar.Bitmap.Image,appMPlayer.bar.Bitmap.ImageSize );
   Render( &appMPlayer.barWindow,appMPlayer.barItems,appMPlayer.NumberOfBarItems,mplPBDrawBuffer,appMPlayer.bar.Bitmap.ImageSize );
   wsConvert( &appMPlayer.barWindow,mplPBDrawBuffer,appMPlayer.bar.Bitmap.ImageSize );
  }
 wsPutImage( &appMPlayer.barWindow );
}

#define itPLMButton (itNULL - 1)
#define itPRMButton (itNULL - 2)

static void mplPBMouseHandle( int Button, int X, int Y, int RX, int RY )
{
 static int     itemtype = 0;
        int     i;
        wItem * item = NULL;
	float   value = 0.0f;

 static int     SelectedItem = -1;
	int     currentselected = -1;

 for ( i=0;i < appMPlayer.NumberOfBarItems + 1;i++ )
   if ( ( appMPlayer.barItems[i].pressed != btnDisabled )&&
      ( wgIsRect( X,Y,appMPlayer.barItems[i].x,appMPlayer.barItems[i].y,appMPlayer.barItems[i].x+appMPlayer.barItems[i].width,appMPlayer.barItems[i].y+appMPlayer.barItems[i].height ) ) )
    { currentselected=i; break; }

 switch ( Button )
  {
   case wsPMMouseButton:
        gtkShow( evHidePopUpMenu,NULL );
        mplShowMenu( RX,RY );
        break;
   case wsRMMouseButton:
        mplHideMenu( RX,RY,0 );
        break;
   case wsRRMouseButton:
        gtkShow( evShowPopUpMenu,NULL );
	break;
// ---
   case wsPLMouseButton:
	gtkShow( evHidePopUpMenu,NULL );
        SelectedItem=currentselected;
        if ( SelectedItem == -1 ) break; // yeees, i'm move the fucking window
        item=&appMPlayer.barItems[SelectedItem];
	itemtype=item->type;
	item->pressed=btnPressed;

	switch( item->type )
	 {
	  case itButton:
	       if ( ( SelectedItem > -1 ) &&
	         ( ( ( item->msg == evPlaySwitchToPause && item->msg == evPauseSwitchToPlay ) ) ||
		 ( ( item->msg == evPauseSwitchToPlay && item->msg == evPlaySwitchToPause ) ) ) )
		 { item->pressed=btnDisabled; }
	       break;
	 }

	break;
   case wsRLMouseButton:
	item=&appMPlayer.barItems[SelectedItem];
	item->pressed=btnReleased;
	SelectedItem=-1;
	if ( currentselected == - 1 ) { itemtype=0; break; }
	value=0;

	switch( itemtype )
	 {
	  case itPotmeter:
	  case itHPotmeter:
	       btnModify( item->msg,(float)( X - item->x ) / item->width * 100.0f );
	       mplEventHandling( item->msg,item->value );
	       value=item->value;
	       break;
	  case itVPotmeter:
	       btnModify( item->msg, ( 1. - (float)( Y - item->y ) / item->height) * 100.0f );
	       mplEventHandling( item->msg,item->value );
	       value=item->value;
	       break;
	 }
	mplEventHandling( item->msg,value );

	itemtype=0;
	break;
// ---
   case wsP5MouseButton: value=-2.5f; goto rollerhandled;
   case wsP4MouseButton: value= 2.5f;
rollerhandled:
        item=&appMPlayer.barItems[currentselected];
        if ( ( item->type == itHPotmeter )||( item->type == itVPotmeter )||( item->type == itPotmeter ) )
	 {
	  item->value+=value;
	  btnModify( item->msg,item->value );
	  mplEventHandling( item->msg,item->value );
	 }
	break;
// ---
   case wsMoveMouse:
        item=&appMPlayer.barItems[SelectedItem];
	switch ( itemtype )
	 {
	  case itPRMButton:
	       mplMenuMouseHandle( X,Y,RX,RY );
	       break;
	  case itPotmeter:
	       item->value=(float)( X - item->x ) / item->width * 100.0f;
	       goto potihandled;
	  case itVPotmeter:
	       item->value=(1. - (float)( Y - item->y ) / item->height) * 100.0f;
	       goto potihandled;
	  case itHPotmeter:
	       item->value=(float)( X - item->x ) / item->width * 100.0f;
potihandled:
	       if ( item->value > 100.0f ) item->value=100.0f;
	       if ( item->value < 0.0f ) item->value=0.0f;
	       mplEventHandling( item->msg,item->value );
	       break;
	 }
        break;
  }
}

void mplPBShow( int x, int y )
{
 if ( !appMPlayer.barIsPresent || !gtkEnablePlayBar ) return;
 if ( !appMPlayer.subWindow.isFullScreen ) return;

 if ( y > appMPlayer.subWindow.Height - appMPlayer.bar.height )
  {
   if ( !mplPBFade ) wsVisibleWindow( &appMPlayer.barWindow,wsShowWindow );
   mplPBFade=1; mplPBVisible=1; wsPostRedisplay( &appMPlayer.barWindow );
  }
  else if ( !mplPBFade ) mplPBFade=2;
}

void mplPBInit( void )
{
 if ( !appMPlayer.barIsPresent ) return;

 gfree( (void**)&mplPBDrawBuffer );

 if ( ( mplPBDrawBuffer = malloc( appMPlayer.bar.Bitmap.ImageSize ) ) == NULL )
  {
   mp_msg( MSGT_GPLAYER,MSGL_FATAL,MSGTR_NEMDB );
   exit( 0 );
  }

 appMPlayer.barWindow.Parent=appMPlayer.subWindow.WindowID;
 wsCreateWindow( &appMPlayer.barWindow,
   appMPlayer.bar.x,appMPlayer.bar.y,appMPlayer.bar.width,appMPlayer.bar.height,
   wsNoBorder,wsShowMouseCursor|wsHandleMouseButton|wsHandleMouseMove,wsHideFrame|wsHideWindow,"PlayBar" );

 wsSetShape( &appMPlayer.barWindow,appMPlayer.bar.Mask.Image );

 appMPlayer.barWindow.ReDraw=(void *)mplPBDraw;
 appMPlayer.barWindow.MouseHandler=mplPBMouseHandle;
 appMPlayer.barWindow.KeyHandler=mplMainKeyHandle;

 mplPBLength=appMPlayer.subWindow.Height;
}
