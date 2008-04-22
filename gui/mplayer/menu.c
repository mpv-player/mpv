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

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "config.h"
#include "help_mp.h"
#include "mp_msg.h"
#include "gui/app.h"
#include "gmplayer.h"

#include "widgets.h"

unsigned char * mplMenuDrawBuffer = NULL;
int             mplMenuRender = 1;
int             mplMenuItem = -1;
int             mplOldMenuItem = -1;
int             mplMenuX,mplMenuY;
static int      mplMenuIsInitialized = 0;

void mplMenuDraw( void )
{
 uint32_t * buf = NULL;
 uint32_t * drw = NULL;
 int             x,y,tmp;

 if ( !appMPlayer.menuIsPresent || !appMPlayer.menuBase.Bitmap.Image ) return;
 if ( !mplMenuRender && !appMPlayer.menuWindow.Visible ) return;

 if ( mplMenuRender || mplMenuItem != mplOldMenuItem )
  {
   memcpy( mplMenuDrawBuffer,appMPlayer.menuBase.Bitmap.Image,appMPlayer.menuBase.Bitmap.ImageSize );
// ---
   if ( mplMenuItem != -1 )
    {
     buf=(uint32_t *)mplMenuDrawBuffer;
     drw=(uint32_t *)appMPlayer.menuSelected.Bitmap.Image;
     for ( y=appMPlayer.MenuItems[ mplMenuItem ].y; y < appMPlayer.MenuItems[ mplMenuItem ].y + appMPlayer.MenuItems[ mplMenuItem ].height; y++ )
       for ( x=appMPlayer.MenuItems[ mplMenuItem ].x; x < appMPlayer.MenuItems[ mplMenuItem ].x + appMPlayer.MenuItems[ mplMenuItem ].width; x++ )
         {
          tmp=drw[ y * appMPlayer.menuSelected.width + x ];
          if ( tmp != 0x00ff00ff ) buf[ y * appMPlayer.menuBase.width + x ]=tmp;
         }
    }
   mplOldMenuItem=mplMenuItem;
// ---
   wsConvert( &appMPlayer.menuWindow,mplMenuDrawBuffer,appMPlayer.menuBase.Bitmap.ImageSize );
   mplMenuRender=0;
  }
 wsPutImage( &appMPlayer.menuWindow );
}

void mplMenuMouseHandle( int X,int Y,int RX,int RY )
{
 int x,y,i;

 if ( !appMPlayer.menuBase.Bitmap.Image ) return;

 mplMenuItem=-1;
 x=RX - appMPlayer.menuWindow.X;
 y=RY - appMPlayer.menuWindow.Y;
 if ( ( x < 0 ) || ( y < 0  ) || ( x > appMPlayer.menuBase.width ) || ( y > appMPlayer.menuBase.height ) )
  {
   wsPostRedisplay( &appMPlayer.menuWindow );
   return;
  }

 for( i=0;i<=appMPlayer.NumberOfMenuItems;i++ )
  {
   if ( wgIsRect( x,y,
         appMPlayer.MenuItems[i].x,appMPlayer.MenuItems[i].y,
         appMPlayer.MenuItems[i].x+appMPlayer.MenuItems[i].width,appMPlayer.MenuItems[i].y+appMPlayer.MenuItems[i].height ) ) { mplMenuItem=i; break; }
  }
 wsPostRedisplay( &appMPlayer.menuWindow );
}

void mplShowMenu( int mx,int my )
{
 int x,y;

 if ( !appMPlayer.menuIsPresent || !appMPlayer.menuBase.Bitmap.Image ) return;

 x=mx;
 if ( x + appMPlayer.menuWindow.Width > wsMaxX ) x=wsMaxX - appMPlayer.menuWindow.Width - 1 + wsOrgX;
 y=my;
 if ( y + appMPlayer.menuWindow.Height > wsMaxY ) y=wsMaxY - appMPlayer.menuWindow.Height - 1 + wsOrgY;

 mplMenuX=x; mplMenuY=y;

 mplMenuItem = 0;

 wsMoveWindow( &appMPlayer.menuWindow,False,x,y );
 wsMoveTopWindow( wsDisplay,appMPlayer.menuWindow.WindowID );
 wsSetLayer( wsDisplay,appMPlayer.menuWindow.WindowID,1 );
 mplMenuRender=1;
 wsVisibleWindow( &appMPlayer.menuWindow,wsShowWindow );
 wsPostRedisplay( &appMPlayer.menuWindow );
}

void mplHideMenu( int mx,int my,int w )
{
 int x,y,i=mplMenuItem;

 if ( !appMPlayer.menuIsPresent || !appMPlayer.menuBase.Bitmap.Image ) return;

 x=mx-mplMenuX;
 y=my-mplMenuY;
// x=RX - appMPlayer.menuWindow.X;
// y=RY - appMPlayer.menuWindow.Y;

 wsVisibleWindow( &appMPlayer.menuWindow,wsHideWindow );

 if ( ( x < 0 ) || ( y < 0 ) ) return;

// printf( "---------> %d %d,%d\n",i,x,y );
// printf( "--------> mi: %d,%d %dx%d\n",appMPlayer.MenuItems[i].x,appMPlayer.MenuItems[i].y,appMPlayer.MenuItems[i].width,appMPlayer.MenuItems[i].height );
 if ( wgIsRect( x,y,
        appMPlayer.MenuItems[i].x,appMPlayer.MenuItems[i].y,
        appMPlayer.MenuItems[i].x+appMPlayer.MenuItems[i].width,
        appMPlayer.MenuItems[i].y+appMPlayer.MenuItems[i].height ) )
   {
    mplEventHandling( appMPlayer.MenuItems[i].msg,(float)w );
   }
}

void mplMenuInit( void )
{

 if ( mplMenuIsInitialized || !appMPlayer.menuIsPresent || !appMPlayer.menuBase.Bitmap.Image ) return;

 appMPlayer.menuBase.x=0;
 appMPlayer.menuBase.y=0;

 if ( ( mplMenuDrawBuffer = calloc( 1,appMPlayer.menuBase.Bitmap.ImageSize ) ) == NULL )
  {
#ifdef DEBUG
    mp_msg( MSGT_GPLAYER,MSGL_DBG2,"[menu.h] %s",MSGTR_NEMFMR );
#endif
   gtkMessageBox( GTK_MB_FATAL,MSGTR_NEMFMR );
   return;
  }

 wsCreateWindow( &appMPlayer.menuWindow,
 appMPlayer.menuBase.x,appMPlayer.menuBase.y,appMPlayer.menuBase.width,appMPlayer.menuBase.height,
 wsNoBorder,wsShowMouseCursor|wsHandleMouseButton|wsHandleMouseMove,wsOverredirect|wsHideFrame|wsMaxSize|wsMinSize|wsHideWindow,"MPlayer menu" );

 wsSetShape( &appMPlayer.menuWindow,appMPlayer.menuBase.Mask.Image );

#ifdef DEBUG
  mp_msg( MSGT_GPLAYER,MSGL_DBG2,"[menu.h] menu: 0x%x\n",(int)appMPlayer.menuWindow.WindowID );
#endif

 mplMenuIsInitialized=1;
 appMPlayer.menuWindow.ReDraw=mplMenuDraw;
// appMPlayer.menuWindow.MouseHandler=mplMenuMouseHandle;
// appMPlayer.menuWindow.KeyHandler=mplMainKeyHandle;
 mplMenuRender=1; wsPostRedisplay( &appMPlayer.menuWindow );
}
