
// sub window

int             mplSubRender = 1;

void mplSubDraw( wsParamDisplay )
{

// if ( ( appMPlayer.subWindow.Visible == wsWindowNotVisible )||
//      ( appMPlayer.subWindow.State != wsWindowExpose ) ) return;

 if ( ( mplShMem->Playing ) )//&&( appMPlayer.subWindow.State == wsWindowExpose ) )
  { 
   vo_expose=1; 
   mplSubRender=0;
  }

 if ( mplSubRender )
  {
   wsSetForegroundRGB( &appMPlayer.subWindow,appMPlayer.subR,appMPlayer.subG,appMPlayer.subB );
   XFillRectangle( wsDisplay,appMPlayer.subWindow.WindowID,appMPlayer.subWindow.wGC,0,0,
    appMPlayer.subWindow.Width,appMPlayer.subWindow.Height );
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
          msButton=0;
          mplSubMoved=0;
          break;
  }
}
