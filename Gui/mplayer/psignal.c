
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
#include "../error.h"

#include "../../config.h"
#include "../../libvo/x11_common.h"
#include "../../libmpdemux/stream.h"

#include "./mplayer.h"

#define gtkShow( w ) gtkShMem->vs.window=w; gtkSendMessage( evShowWindow );

pid_t mplMPlayerPID = 0;
pid_t mplParentPID = 0;

pid_t gtkPID = 0;

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
 gtkMessageBox( GTK_MB_ERROR,p );
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
        if ( gtkFillSkinList( sbMPlayerPrefixDir )&&gtkFillSkinList( sbMPlayerDirInHome ) )
         {
          gtkSetDefaultToCList( SkinList,skinName );
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
   case evFirstLoad:
        if ( gtkVisibleFileSelect ) gtk_widget_hide( FileSelect );
        gtk_widget_show( FileSelect );
        gtkVisibleFileSelect=1;
        gtkShow( evFirstLoad );
        break;
   case evMessageBox:
        gtk_label_set_text( gtkMessageBoxText,(char *)gtkShMem->mb.str );
        if( gtkVisibleMessageBox ) gtk_widget_hide( MessageBox ); 
        switch( gtkShMem->mb.type)
         {
           case GTK_MB_FATAL:
				  gtk_window_set_title (GTK_WINDOW (MessageBox), "fatal error ...");
				  gtk_widget_hide( WarningPixmap );
				  gtk_widget_show( ErrorPixmap );
                  break;
           case GTK_MB_ERROR: 
				  gtk_window_set_title (GTK_WINDOW (MessageBox), "error ...");
				  gtk_widget_hide( WarningPixmap );
				  gtk_widget_show( ErrorPixmap );
                  break;
           case GTK_MB_WARNING: 
 				 gtk_window_set_title (GTK_WINDOW (MessageBox), "warning ...");
				  gtk_widget_show( WarningPixmap );
				  gtk_widget_hide( ErrorPixmap );
           	   break;
         }
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

void mplMainSigHandler( int s )
{
 if ( s != SIGTYPE ) return;

// #ifdef DEBUG
//  if ( gtkShMem->message ) dbprintf( 5,"[psignal] main sig handler gtk msg: %d\n",gtkShMem->message );
//  if ( mplShMem->message ) dbprintf( 5,"[psignal] main sig handler mpl msg: %d\n",mplShMem->message );
// #endif

 switch ( gtkShMem->message )
  {
   case evGtkIsOk:
        #ifdef DEBUG
         dbprintf( 5,"[psignal] gtk is ok.\n" );
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
   case evSkinBrowser: mplShMem->SkinChange=1; break;
   case evFileLoaded:
        printf("*** gtkShMem->fs.dir == '%s'  \n",gtkShMem->fs.dir);
        printf("*** gtkShMem->fs.filename == '%s'  \n",gtkShMem->fs.filename);
        strcpy( mplShMem->Filename,gtkShMem->fs.dir ); 
	strcat( mplShMem->Filename,"/" ); 
	strcat( mplShMem->Filename,gtkShMem->fs.filename );
	printf( "%%%%%%%% Playing file: %s\n",mplShMem->Filename );
	mplShMem->FilenameChanged=1;
	mplShMem->StreamType=STREAMTYPE_FILE;
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
// mplShMem->message=0;
}

void gtkSendMessage( int msg )
{
 if ( !gtkIsOk ) return;
 gtkShMem->message=msg;
 kill( gtkPID,SIGTYPE );
}
