
// sub window

int             mplSubRender = 1;
int             SubVisible = 0;

void mplSubDraw( wsParamDisplay )
{
 if ( appMPlayer.subWindow.State == wsWindowClosed ) exit_player( MSGTR_Exit_quit );
 
 if ( appMPlayer.subWindow.State == wsWindowFocusIn ) SubVisible++;
 if ( appMPlayer.subWindow.State == wsWindowFocusOut && wsWMType != wsWMKDE && wsWMType != wsWMIceWM ) SubVisible--;

 if ( !appMPlayer.subWindow.Mapped ||
      appMPlayer.subWindow.Visible == wsWindowNotVisible ) return;

 if ( guiIntfStruct.Playing ) mplSubRender=0;

 if ( mplSubRender )
  {
   if ( appMPlayer.sub.Bitmap.Image ) wsPutImage( &appMPlayer.subWindow );
  }
 appMPlayer.subWindow.State=0;
}

void mplSubMouseHandle( int Button,int X,int Y,int RX,int RY )
{
 static int mplSubMoved = 0;
 static int msButton = 0;

 switch( Button )
  {
   case wsPMMouseButton:
          gtkShow( evShowPopUpMenu,NULL );
          break;
   case wsPRMouseButton:
          gtkShow( evHidePopUpMenu,NULL );
          mplShowMenu( RX,RY );
          msButton=wsPRMouseButton;
          break;
   case wsRRMouseButton:
          mplHideMenu( RX,RY,1 );
          msButton=0;
          break;
// ---
   case wsPLMouseButton:
          gtkShow( evHidePopUpMenu,NULL );
          sx=X; sy=Y;
          msButton=wsPLMouseButton;
          mplSubMoved=0;
          break;
   case wsMoveMouse:
          switch ( msButton )
           {
            case wsPLMouseButton:
                   mplSubMoved=1;
                   if ( !appMPlayer.subWindow.isFullScreen ) wsMoveWindow( &appMPlayer.subWindow,False,RX - sx,RY - sy );
                   break;
            case wsPRMouseButton:
                   mplMenuMouseHandle( X,Y,RX,RY );
                   break;
           }
          break;
   case wsRLMouseButton:
          if ( ( !mplSubMoved )&&( appMPlayer.subWindow.isFullScreen ) )
           {
            if( SubVisible++%2 ) wsMoveTopWindow( wsDisplay,appMPlayer.mainWindow.WindowID );
             else if ( wsWMType != wsWMNetWM && wsWMType != wsWMKDE ) wsMoveTopWindow( wsDisplay,appMPlayer.subWindow.WindowID );
    }
          msButton=0;
          mplSubMoved=0;
          break;
  }
}
