
unsigned char * mplMenuDrawBuffer = NULL;
int             mplMenuRender = 1;
int             mplMenuItem = -1;
int             mplOldMenuItem = -1;
int             mplMenuX,mplMenuY;

void mplHideMenu( int mx,int my,int w );

void mplMenuDraw( wsParamDisplay )
{
 unsigned long * buf = NULL;
 unsigned long * drw = NULL;
 int             x,y,tmp;

 if ( !appMPlayer.menuBase.Bitmap.Image ) return;
 if ( !mplMenuRender && !appMPlayer.menuWindow.Visible ) return;

 if ( mplMenuRender || mplMenuItem != mplOldMenuItem )
  {
   memcpy( mplMenuDrawBuffer,appMPlayer.menuBase.Bitmap.Image,appMPlayer.menuBase.Bitmap.ImageSize );
// ---
   if ( mplMenuItem != -1 )
    {
     buf=(unsigned long *)mplMenuDrawBuffer;
     drw=(unsigned long *)appMPlayer.menuSelected.Bitmap.Image;
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

 if ( !appMPlayer.menuBase.Bitmap.Image ) return;

 x=mx;
 if ( x + appMPlayer.menuWindow.Width > wsMaxX ) x=wsMaxX - appMPlayer.menuWindow.Width - 1;
 y=my;
 if ( y + appMPlayer.menuWindow.Height > wsMaxY ) y=wsMaxY - appMPlayer.menuWindow.Height - 1;

 mplMenuX=x; mplMenuY=y;

 mplMenuItem = 0;

 wsMoveWindow( &appMPlayer.menuWindow,False,x,y );
 wsMoveTopWindow( &appMPlayer.menuWindow );
 mplMenuRender=1;
 wsVisibleWindow( &appMPlayer.menuWindow,wsShowWindow );
 wsPostRedisplay( &appMPlayer.menuWindow );
}

void mplHideMenu( int mx,int my,int w )
{
 int x,y,i=mplMenuItem;

 if ( !appMPlayer.menuBase.Bitmap.Image ) return;

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

 if ( !appMPlayer.menuBase.Bitmap.Image ) return;

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

 #ifdef DEBUG
  mp_msg( MSGT_GPLAYER,MSGL_DBG2,"[menu.h] menu: 0x%x\n",(int)appMPlayer.menuWindow.WindowID );
 #endif

 appMPlayer.menuWindow.ReDraw=mplMenuDraw;
// appMPlayer.menuWindow.MouseHandler=mplMenuMouseHandle;
// appMPlayer.menuWindow.KeyHandler=mplMainKeyHandle;
 mplMenuRender=1; wsPostRedisplay( &appMPlayer.menuWindow );
}
