
// sub window

int             mplSubRender = 1;
int             SubVisible = 0;

void mplSubDraw( wsParamDisplay )
{
 if ( appMPlayer.subWindow.State == wsFocused ||
      appMPlayer.subWindow.State ==
 
 ) SubVisible=0;
 
 if ( !appMPlayer.subWindow.Mapped ||
      appMPlayer.subWindow.Visible == wsWindowNotVisible ) return;

 if ( mplShMem->Playing )
  { 
   wsSetBackgroundRGB( &appMPlayer.subWindow,0,0,0 );
   wsClearWindow( appMPlayer.subWindow );
   vo_expose=1; 
   mplSubRender=0;
  }

 if ( mplSubRender )
  {
   wsSetBackgroundRGB( &appMPlayer.subWindow,appMPlayer.subR,appMPlayer.subG,appMPlayer.subB );
   if ( appMPlayer.sub.Bitmap.Image )
    {
     wsConvert( &appMPlayer.subWindow,appMPlayer.sub.Bitmap.Image,appMPlayer.sub.Bitmap.ImageSize );
     wsPutImage( &appMPlayer.subWindow );
    } 
   XFlush( wsDisplay );
  }
 appMPlayer.subWindow.State=0; 
}

void mplSubMouseHandle( int Button,int X,int Y,int RX,int RY )
{
 static int mplSubMoved = 0;

 mplMouseTimer=mplMouseTimerConst;
 wsVisibleMouse( &appMPlayer.subWindow,wsShowMouseCursor );

 switch( Button )
  {
   case wsPRMouseButton:
          mplShowMenu( RX,RY );
          msButton=wsPRMouseButton;
          break;
   case wsRRMouseButton:
          mplHideMenu( RX,RY );
          msButton=0;
          break;
// ---	  
   case wsPLMouseButton:
          sx=X; sy=Y;
          msButton=wsPLMouseButton;
          mplSubMoved=0;
          break;
   case wsMoveMouse:
          switch ( msButton )
           {
            case wsPLMouseButton:
                   mplSubMoved=1;
                   if ( !appMPlayer.subWindow.isFullScreen ) wsMoveWindow( &appMPlayer.subWindow,RX - sx,RY - sy );
                   break;
            case wsPRMouseButton:
                   mplMenuMouseHandle( X,Y,RX,RY );
                   mplMouseTimer=mplMouseTimerConst;
                   break;
           }
          break;
   case wsRLMouseButton:
          if ( ( !mplSubMoved )&&( !( SubVisible++%2 ) ) ) wsMoveTopWindow( &appMPlayer.mainWindow );
          msButton=0;
          mplSubMoved=0;
          break;
  }
}
