
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
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "widgets.h"

#include "./mplayer.h"
#include "../events.h"
#include "../app.h"

#include "gtk/menu.h"
#include "play.h"
#include "gtk/fs.h"

#include "../../config.h"
#include "../../help_mp.h"

GtkWidget     * SkinBrowser;
GtkWidget     * PlayList;
GtkWidget     * FileSelect;
GtkWidget     * AboutBox;
GtkWidget     * Options;
GtkWidget     * PopUpMenu = NULL;

GtkWidget     * MessageBox;

GtkWidget     * WarningPixmap;
GtkWidget     * ErrorPixmap;

int gtkPopupMenu = 0;
int gtkPopupMenuParam = 0;
int gtkInited = 0;

#include "gtk/sb.h"
#include "gtk/pl.h"
#include "gtk/fs.h"
#include "gtk/mb.h"
#include "gtk/about.h"
#include "gtk/opts.h"
#include "gtk/menu.h"

// --- init & close gtk

void gtkInit( int argc,char* argv[], char *envp[] )
{
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[widget] init gtk ...\n" );
 gtk_set_locale();
 gtk_init( &argc,&argv );
 gdk_set_use_xshm( FALSE );
 
// mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[widget] Create about box.\n" );              AboutBox=create_About();
// mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[widget] Create skin browser.\n" );           SkinBrowser=create_SkinBrowser();
// mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[widget] Create playlist.\n" );               PlayList=create_PlayList();
// mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[widget] Create file selector.\n" );          FileSelect=create_FileSelect();
// mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[widget] Create message box.\n" );            MessageBox=create_MessageBox(0);
// mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[widget] Create preferences dialog box.\n" ); Options=create_Options();
 
 gtkInited=1;
}

void gtkDone( void )
{
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
 for( i=0;i < 25;i++ ) gtk_main_iteration_do( 0 );
}

// --- funcs

void gtkMessageBox( int type,gchar * str )
{
 if ( !gtkInited ) return;
 MessageBox=create_MessageBox(0);
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
}

void gtkSetLayer( GtkWidget * wdg )
{
 GdkWindowPrivate * win = wdg->window;
 wsSetLayer( gdk_display,win->xwindow,appMPlayer.subWindow.isFullScreen );
}

void gtkShow( int type,char * param )
{
 switch( type )
  {
   case evSkinBrowser:
	SkinBrowser=create_SkinBrowser();
//        gtkClearList( SkinList );
        if ( gtkFillSkinList( sbMPlayerPrefixDir ) && gtkFillSkinList( sbMPlayerDirInHome ) )
         {
          gtkSetDefaultToCList( SkinList,param );
          gtk_widget_show( SkinBrowser );
	  gtkSetLayer( SkinBrowser );
         }
        break;
   case evPreferences:
	Options=create_Options();
        gtk_widget_show( Options );
	gtkSetLayer( Options );
        break;
   case evPlayList:
	PlayList=create_PlayList();
        gtk_widget_show( PlayList );
	gtkSetLayer( PlayList );
        break;
   case evLoad:
        ShowFileSelect( fsVideoSelector );
	gtkSetLayer( FileSelect );
        break;
   case evFirstLoad:
        ShowFileSelect( fsVideoSelector );
	gtkSetLayer( FileSelect );
        break;
   case evLoadSubtitle:
        ShowFileSelect( fsSubtitleSelector );
	gtkSetLayer( FileSelect );
        break;
   case evAbout:
	AboutBox=create_About();
        gtk_widget_show( AboutBox );
	gtkSetLayer( AboutBox );
        break;
   case evShowPopUpMenu:
        gtkPopupMenu=evNone;
        gtkPopupMenuParam=0;
        if ( PopUpMenu ) gtk_widget_hide_on_delete( PopUpMenu );
        PopUpMenu=create_PopUpMenu();
        gtk_menu_popup( GTK_MENU( PopUpMenu ),NULL,NULL,NULL,NULL,0,0 );
        break;
   case evHidePopUpMenu:
        if ( PopUpMenu ) gtk_widget_hide_on_delete( PopUpMenu );
        break;
  }
}


