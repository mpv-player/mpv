
// sub window

int             mplSubRender = 1;

int VisibleMainWindow( void )
{
 Window   root,parent,me,subw,mainw;
 Window * childs;
 int      nchilds;
 int      i;
 int      visible = 0;

 me=appMPlayer.mainWindow.WindowID;
 for (;;) 
  {
   XQueryTree( wsDisplay,me,&root,&parent,&childs,&nchilds);
   XFree((char *) childs);
   if (root == parent) break;
   me=parent;
  }
 XQueryTree( wsDisplay,root,&root,&parent,&childs,&nchilds );
 mainw=me;

 me=appMPlayer.subWindow.WindowID;
 for (;;) 
  {
   XQueryTree( wsDisplay,me,&root,&parent,&childs,&nchilds);
   XFree((char *) childs);
   if (root == parent) break;
   me=parent;
  }
 XQueryTree( wsDisplay,root,&root,&parent,&childs,&nchilds );
 subw=me;

 for (i=0; i < nchilds; i++) if ( childs[i]==me ) break;
 for ( ;i<nchilds;i++ ) if ( childs[i] == mainw ) visible=1; 
// printf( "-----------> visible main vindov: %d ---\n",visible );
 return visible;
}

int mainisvisible1;
int mainisvisible2;
int count = 0;

void mplSubDraw( wsParamDisplay )
{
// if ( ( appMPlayer.subWindow.Visible == wsWindowNotVisible )||
//      ( appMPlayer.subWindow.State != wsWindowExpose ) ) return;

 if ( ( mplShMem->Playing ) )//&&( appMPlayer.subWindow.State == wsWindowExpose ) )
  { 
printf( "------> redraw volib.\n" );   
   wsSetBackgroundRGB( &appMPlayer.subWindow,0,0,0 );
   wsClearWindow( appMPlayer.subWindow );
   vo_expose=1; 
   mplSubRender=0;
  }

 if ( mplSubRender )
  {
printf( "------> redraw video.\n" );   
   wsSetBackgroundRGB( &appMPlayer.subWindow,appMPlayer.subR,appMPlayer.subG,appMPlayer.subB );
   wsClearWindow( appMPlayer.subWindow );
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
 static int oldmainisvisible = 0;

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
          oldmainisvisible=VisibleMainWindow();
	  printf( "----> %d %d\n",mainisvisible1,mainisvisible2 );
	  //=mainisvisible;
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
          if ( ( !mplSubMoved )&&
	       ( appMPlayer.subWindow.isFullScreen )&&
	       ( !VisibleMainWindow() ) )
	   {
wsMoveTopWindow( &appMPlayer.mainWindow );
//	     else wsMoveTopWindow( &appMPlayer.mainWindow );
	   }
          msButton=0;
          mplSubMoved=0;
          break;
  }
}
