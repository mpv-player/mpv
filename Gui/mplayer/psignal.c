
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "widgets.h"
#include "play.h"

#include "../app.h"

#include "../skin/skin.h"
#include "../wm/ws.h"
#include "../config.h"
#include "../error.h"
#include "../language.h"

#include "../../config.h"

#include "./mplayer.h"

#define gtkShow( w ) gtkShMem->vs.window=w; gtkSendMessage( evShowWindow );

pid_t mplMPlayerPID = 0;
pid_t mplParentPID = 0;

pid_t gtkChildPID = 0;
pid_t gtkParentPID = 0;

int mplCriticalError = 0;
int gtkIsOk = 0;

void mplErrorHandler( int critical,const char * format, ... )
{
 char    * p;
 va_list   ap;
 int       n;

 if ( (p=(char *)malloc( 512 ) ) == NULL ) return;
 va_start( ap,format );
 n=vsnprintf( p,512,format,ap );
 va_end( ap );
 mplCriticalError=critical;
 gtkMessageBox( p );
// message( False,p );
}

void mplPlayerSigHandler( int s )
{
 #ifdef DEBUG
  dbprintf( 5,"[psignal] mpl sig handler msg: %d\n",mplShMem->message );
 #endif
 if ( s != SIGTYPE ) return;
 switch ( mplShMem->message )
  {
   case mplQuit:
//        exit_player( "Quit" );
        break;
   case mplPauseEvent:
//        if ( osd_function != OSD_PAUSE ) osd_function=OSD_PAUSE;
//          else osd_function=OSD_PLAY;
        break;
   case mplResizeEvent:
//        vo_resize=1;
//        vo_expose=1;
//        dbprintf( 2,"[psignal] resize.\n" );
//        if (video_out != NULL ) video_out->check_events();
        break;
   case mplExposeEvent:
//        vo_expose=1;
//        if (video_out != NULL ) video_out->check_events();
        break;
   case mplSeekEvent:
//        rel_seek_secs+=mplShMem->videodata.seek;
//        if ( rel_seek_secs > 0 ) osd_function=OSD_FFW;
//          else osd_function=OSD_REW;
        break;
   case mplIncAudioBufferDelay:
//        audio_delay+=0.1;  // increase audio buffer delay
//        a_frame-=0.1;
        break;
   case mplDecAudioBufferDelay:
//        audio_delay-=0.1;  // increase audio buffer delay
//        a_frame+=0.1;
        break;
  }
 mplShMem->message=0;
}

void gtkSigHandler( int s )
{
 if ( s != SIGTYPE ) return;
 #ifdef DEBUG
  dbprintf( 5,"[psignal] gtk sig handler msg: %d\n",gtkShMem->message );
 #endif
 switch ( gtkShMem->message )
  {
   case evHideWindow:
        switch ( gtkShMem->vs.window )
         {
          case evPlayList: gtk_widget_hide( PlayList ); gtkVisiblePlayList=0; break;
          case evSkinBrowser: gtk_widget_hide( SkinBrowser ); gtkVisibleSkinBrowser=0; break;
          case evLoad: gtk_widget_hide( FileSelect ); gtkVisibleFileSelect=0; break;
         }
        break;
   case evSkinBrowser:
        if ( gtkVisibleSkinBrowser ) gtk_widget_hide( SkinBrowser );
        gtkClearList( SkinList );
        if ( !gtkFillSkinList( sbMPlayerPrefixDir ) ) break;
        if ( gtkFillSkinList( sbMPlayerDirInHome ) )
         {
          gtkSetDefaultToCList( SkinList,cfgSkin );
          gtk_widget_show( SkinBrowser );
          gtkVisibleSkinBrowser=1;
          gtkShow( evSkinBrowser );
         }
        break;
   case evPreferences:
        if ( gtkVisibleOptions ) gtk_widget_hide( Options );
        gtk_widget_show( Options );
        gtkVisibleOptions=1;
        break;
   case evPlayList:
        if ( gtkVisiblePlayList ) gtk_widget_hide( PlayList );
        gtk_widget_show( PlayList );
        gtkVisiblePlayList=1;
        gtkShow( evPlayList );
        break;
   case evLoad:
        if ( gtkVisibleFileSelect ) gtk_widget_hide( FileSelect );
        gtk_widget_show( FileSelect );
        gtkVisibleFileSelect=1;
        gtkShow( evPlay );
        break;
   case evMessageBox:
        gtk_label_set_text( gtkMessageBoxText,(char *)gtkShMem->mb.str );
        gtk_widget_set_usize( MessageBox,gtkShMem->mb.sx,gtkShMem->mb.sy );
        gtk_widget_set_usize( gtkMessageBoxText,gtkShMem->mb.tsx,gtkShMem->mb.tsy );
        if ( gtkVisibleMessageBox ) gtk_widget_hide( MessageBox );
        gtk_widget_show( MessageBox );
        gtkVisibleMessageBox=1;
        break;
   case evAbout:
        if ( gtkVisibleAboutBox ) gtk_widget_hide( AboutBox );
        gtk_widget_show( AboutBox );
        gtkVisibleAboutBox=1;
        break;
   case evExit:
	gtk_main_quit();
        break;
  }
 gtkShMem->message=0;
}

listItems tmpList;

void mplMainSigHandler( int s )
{
 #ifdef DEBUG
  if ( gtkShMem->message ) dbprintf( 5,"[psignal] main sig handler gtk msg: %d\n",gtkShMem->message );
  if ( mplShMem->message ) dbprintf( 5,"[psignal] main sig handler mpl msg: %d\n",mplShMem->message );
 #endif

 if ( s != SIGTYPE ) return;

 switch ( gtkShMem->message )
  {
   case evGtkIsOk:
        #ifdef DEBUG
         dbprintf( 1,"[psignal] gtk is ok.\n" );
        #endif
        gtkIsOk=True;
        break;
   case evShowWindow:
        switch ( gtkShMem->vs.window )
         {
          case evPlayList: gtkVisiblePlayList=1; break;
          case evLoad: gtkVisibleFileSelect=1; break;
          case evSkinBrowser: gtkVisibleSkinBrowser=1; break;
         }
        break;
   case evHideWindow:
        switch ( gtkShMem->vs.window )
         {
          case evPlayList:
               btnModify( evPlayList,btnReleased ); gtkVisiblePlayList=0;
               mplMainRender=1; wsPostRedisplay( &appMPlayer.mainWindow );
               break;
          case evSkinBrowser: gtkVisibleSkinBrowser=0; break;
          case evLoad: gtkVisibleFileSelect=0; break;
         }
        break;
   case evSkinBrowser:
        if ( strcmp( cfgSkin,gtkShMem->sb.name ) )
         {
          int ret;
          #ifdef DEBUG
           dbprintf( 1,"[psignal] skin: %s\n",gtkShMem->sb.name );
          #endif

          mainVisible=0;

          appInitStruct( &tmpList );
          skinAppMPlayer=&tmpList;
          ret=skinRead( gtkShMem->sb.name );

          appInitStruct( &tmpList );
          skinAppMPlayer=&appMPlayer;
          appInitStruct( &appMPlayer );
          if ( !ret ) strcpy( cfgSkin,gtkShMem->sb.name );
          skinRead( cfgSkin );

          if ( ret )
           {
            mainVisible=1;
            break;
           }

//          appCopy( &appMPlayer,&tmpList );
//          appInitStruct( &tmpList );
//          skinAppMPlayer=&appMPlayer;
//          strcpy( cfgSkin,gtkShMem->sb.name );

          if ( mplDrawBuffer ) free( mplDrawBuffer );
          if ( ( mplDrawBuffer = (unsigned char *)calloc( 1,appMPlayer.main.Bitmap.ImageSize ) ) == NULL )
           { message( False,langNEMDB ); break; }
          wsResizeWindow( &appMPlayer.mainWindow,appMPlayer.main.width,appMPlayer.main.height );
          wsMoveWindow( &appMPlayer.mainWindow,appMPlayer.main.x,appMPlayer.main.y );
          wsResizeImage( &appMPlayer.mainWindow );
          wsSetShape( &appMPlayer.mainWindow,appMPlayer.main.Mask.Image );
          mainVisible=1; mplMainRender=1; wsPostRedisplay( &appMPlayer.mainWindow );
          btnModify( evSetVolume,mplShMem->Volume );
          btnModify( evSetBalance,mplShMem->Balance );
          btnModify( evSetMoviePosition,mplShMem->Position );

          if ( appMPlayer.menuBase.Bitmap.Image )
           {
            if ( mplMenuDrawBuffer ) free( mplMenuDrawBuffer );
            if ( ( mplMenuDrawBuffer = (unsigned char *)calloc( 1,appMPlayer.menuBase.Bitmap.ImageSize ) ) == NULL )
             { message( False,langNEMDB ); break; }
            wsResizeWindow( &appMPlayer.menuWindow,appMPlayer.menuBase.width,appMPlayer.menuBase.height );
            wsResizeImage( &appMPlayer.menuWindow );
           }

          mplSkinChanged=1;
          if ( !mplShMem->Playing )
           {
            mplSkinChanged=0;
            if ( appMPlayer.subWindow.isFullScreen ) wsFullScreen( &appMPlayer.subWindow );
            wsResizeWindow( &appMPlayer.subWindow,appMPlayer.sub.width,appMPlayer.sub.height );
            wsMoveWindow( &appMPlayer.subWindow,appMPlayer.sub.x,appMPlayer.sub.y );
            if ( appMPlayer.sub.Bitmap.Image ) wsResizeImage( &appMPlayer.subWindow );
            mplSubRender=1; wsPostRedisplay( &appMPlayer.subWindow );
           }
         }
        break;
   case evFileLoaded:
        if ( Filename ) free( Filename );
        Filename=(char *)malloc( strlen( gtkShMem->fs.dir ) + strlen( gtkShMem->fs.filename ) + 2 );
        strcpy( Filename,gtkShMem->fs.dir ); strcat( Filename,"/" ); strcat( Filename,gtkShMem->fs.filename );
        if ( mplMainAutoPlay ) mplGeneralTimer=1;
        break;
   case evMessageBox:
        if ( mplCriticalError )
         { gtkSendMessage( evExit ); exit( 1 ); }
        mplCriticalError=0;
        break;
  }

// switch( mplShMem->message )
//  {
//  }
 gtkShMem->message=0;
 mplShMem->message=0;
}

void mplSendMessage( int msg )
{
 if ( !mplShMem->Playing ) return;
 mplShMem->message=msg;
 kill( mplMPlayerPID,SIGTYPE ); usleep( 10 );
 kill( mplMPlayerPID,SIGTYPE ); usleep( 10 );
 kill( mplMPlayerPID,SIGTYPE );
}

void gtkSendMessage( int msg )
{
 if ( !gtkIsOk ) return;
 gtkShMem->message=msg;
 kill( gtkChildPID,SIGTYPE );
}
