
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include <gdk/gdkprivate.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "widgets.h"

#include "./mplayer.h"
#include "../events.h"
#include "../app.h"
#include "../wm/ws.h"


#include "gtk/menu.h"
#include "play.h"
#include "gtk/fs.h"

#include "../../config.h"
#include "../../help_mp.h"

GtkWidget     * PopUpMenu = NULL;

GtkWidget     * WarningPixmap;
GtkWidget     * ErrorPixmap;

int gtkPopupMenu = 0;
int gtkPopupMenuParam = 0;
int gtkInited = 0;
int gtkVisible = 0;

#include "gtk/sb.h"
#include "gtk/pl.h"
#include "gtk/fs.h"
#include "gtk/mb.h"
#include "gtk/about.h"
#include "gtk/opts.h"
#include "gtk/menu.h"
#include "gtk/url.h"
#include "gtk/eq.h"

#include "pixmaps/MPlayer_mini.xpm"

// --- init & close gtk

GdkPixmap * gtkIcon = NULL;
GdkBitmap * gtkIconMask = NULL;
Pixmap	    guiIcon;
Pixmap	    guiIconMask;

void gtkInit( void )
{
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[widget] init gtk ...\n" );
 gtk_set_locale();
// gtk_init( &argc,&argv );
 gtk_init( 0,NULL );
// gdk_set_use_xshm( TRUE );

 {
  GtkWidget * win;
  win=gtk_window_new( GTK_WINDOW_TOPLEVEL );

  if ( !gtkIcon ) 
    gtkIcon=gdk_pixmap_colormap_create_from_xpm_d( win->window,gdk_colormap_get_system(),&gtkIconMask,&win->style->bg[GTK_STATE_NORMAL],MPlayer_mini_xpm );

  guiIcon=GDK_WINDOW_XWINDOW( gtkIcon );
  guiIconMask=GDK_WINDOW_XWINDOW( gtkIconMask );

  gtk_widget_destroy( win );
 }
 
 gtkInited=1;
}

void gtkDone( void )
{
}

void gtkAddIcon( GtkWidget * window )
{
 GdkWindowPrivate * win = (GdkWindowPrivate *)window->window;
 wsSetIcon( gdk_display,win->xwindow,guiIcon,guiIconMask );
}

void gtkClearList( GtkWidget * list )
{ gtk_clist_clear( GTK_CLIST( list ) ); }

int gtkFindCList( GtkWidget * list,char * item )
{
 gint    j,t;
 gchar * tmpstr;
 for( t=0,j=0;j<GTK_CLIST( list )->rows;j++ )
  {
   gtk_clist_get_text( GTK_CLIST( list ),j,0,&tmpstr );
   if ( !strcmp( tmpstr,item ) ) return j;
  }
 return -1;
}

void gtkSetDefaultToCList( GtkWidget * list,char * item )
{
 gint    i;
 if ( ( i=gtkFindCList( list,item ) ) > -1 ) gtk_clist_select_row( GTK_CLIST( list ),i,0 );
}

void gtkEventHandling( void )
{
 int i;
 if ( gtkVisible > 0 )
   for( i=0;i < 25;i++ ) gtk_main_iteration_do( 0 );
}

// --- funcs

void gtkMessageBox( int type,gchar * str )
{
 if ( !gtkInited ) return;
 ShowMessageBox( str );
 gtk_label_set_text( GTK_LABEL( gtkMessageBoxText ),str );
 switch( type)
  {
    case GTK_MB_FATAL:
         gtk_window_set_title( GTK_WINDOW( MessageBox ),MSGTR_MSGBOX_LABEL_FatalError );
         gtk_widget_hide( WarningPixmap );
         gtk_widget_show( ErrorPixmap );
         break;
    case GTK_MB_ERROR:
         gtk_window_set_title( GTK_WINDOW( MessageBox ),MSGTR_MSGBOX_LABEL_Error );
         gtk_widget_hide( WarningPixmap );
         gtk_widget_show( ErrorPixmap );
         break;
    case GTK_MB_WARNING:
         gtk_window_set_title( GTK_WINDOW( MessageBox ),MSGTR_MSGBOX_LABEL_Warning );
         gtk_widget_show( WarningPixmap );
         gtk_widget_hide( ErrorPixmap );
         break;
  }
 gtk_widget_show( MessageBox );
 gtkSetLayer( MessageBox );
 gtkActive( MessageBox );
 if ( type == GTK_MB_FATAL )
  while ( gtkVMessageBox ) gtk_main_iteration_do( 0 );
}

void gtkSetLayer( GtkWidget * wdg )
{
 GdkWindowPrivate * win = (GdkWindowPrivate *)wdg->window;
 wsSetLayer( gdk_display,win->xwindow,appMPlayer.subWindow.isFullScreen );
}

void gtkActive( GtkWidget * wdg )
{
 GdkWindowPrivate * win = (GdkWindowPrivate *)wdg->window;
 wsMoveTopWindow( gdk_display,win->xwindow );
}

void gtkShow( int type,char * param )
{
 switch( type )
  {
   case evEqualizer:
	ShowEqualizer();
	gtkSetLayer( Equalizer );
	break;
   case evSkinBrowser:
	ShowSkinBrowser();
//        gtkClearList( SkinList );
        if ( gtkFillSkinList( sbMPlayerPrefixDir ) && gtkFillSkinList( sbMPlayerDirInHome ) )
         {
          gtkSetDefaultToCList( SkinList,param );
	  gtk_clist_sort( GTK_CLIST( SkinList ) );
          gtk_widget_show( SkinBrowser );
	  gtkSetLayer( SkinBrowser );
         } 
	 else 
	  {
	   gtk_widget_destroy( SkinBrowser ); gtkDecVisible();
	   gtkMessageBox( GTK_MB_ERROR,"Skin dirs not found ... Please install skins." );
	  }
        break;
   case evPreferences:
        ShowPreferences();
        break;
   case evPlayList:
        ShowPlayList();
	gtkSetLayer( PlayList );
        break;
   case evLoad:
        ShowFileSelect( fsVideoSelector,0 );
	gtkSetLayer( fsFileSelect );
        break;
   case evFirstLoad:
        ShowFileSelect( fsVideoSelector,0 );
	gtkSetLayer( fsFileSelect );
        break;
   case evLoadSubtitle:
        ShowFileSelect( fsSubtitleSelector,0 );
	gtkSetLayer( fsFileSelect );
        break;
   case evLoadAudioFile:
	ShowFileSelect( fsAudioSelector,0 );
	gtkSetLayer( fsFileSelect );
	break;
   case evAbout:
	ShowAboutBox();
	gtkSetLayer( AboutBox );
        break;
   case evShowPopUpMenu:
        gtkPopupMenu=evNone;
        gtkPopupMenuParam=0;
        if ( PopUpMenu ) { gtk_widget_hide_on_delete( PopUpMenu ); gtkDecVisible(); }
        PopUpMenu=create_PopUpMenu();
        gtk_menu_popup( GTK_MENU( PopUpMenu ),NULL,NULL,NULL,NULL,0,0 );
	gtkIncVisible();
        break;
   case evHidePopUpMenu:
        if ( PopUpMenu ) { gtk_widget_hide_on_delete( PopUpMenu ); /*gtkVisible--;*/ PopUpMenu=NULL; }
        break;
   case evPlayNetwork:
	ShowURLDialogBox();
	gtkSetLayer( URL );
	break;
  }
}

void gtkIncVisible( void ) { gtkVisible++; }
void gtkDecVisible( void ) { if ( gtkVisible > 0 ) gtkVisible--; }

