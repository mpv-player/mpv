
// sub window

int             mplSubRender = 1;
int             mplSubMoved = 0;

void mplSubDraw( wsParamDisplay )
{
 if ( !appMPlayer.subWindow.Visible || mplShMem->Playing ) return;

 if ( mplSubRender )
  {
   wsSetBackgroundRGB( &appMPlayer.subWindow,appMPlayer.subR,appMPlayer.subG,appMPlayer.subB );
   wsClearWindow( appMPlayer.subWindow );
   if ( appMPlayer.sub.Bitmap.Image ) wsConvert( &appMPlayer.subWindow,appMPlayer.sub.Bitmap.Image,appMPlayer.sub.Bitmap.ImageSize );
   mplSubRender=0;
   if ( appMPlayer.sub.Bitmap.Image ) wsPutImage( &appMPlayer.subWindow );
   XFlush( wsDisplay );
  }
}

void mplSubMouseHandle( int Button,int X,int Y,int RX,int RY )
{
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
                   wsMoveWindow( &appMPlayer.subWindow,RX - sx,RY - sy );
                   break;
            case wsPRMouseButton:
                   mplMenuMouseHandle( X,Y,RX,RY );
                   mplMouseTimer=mplMouseTimerConst;
                   break;
           }
          break;
   case wsRLMouseButton:
          if ( !mplSubMoved ) wsMoveTopWindow( &appMPlayer.mainWindow );
          msButton=0;
          mplSubMoved=0;
          break;
  }
}

//void mplSubResizeHandle( unsigned int X,unsigned int Y,unsigned int width,unsigned int height )
//{ mplResize( X,Y,width,height ); }
