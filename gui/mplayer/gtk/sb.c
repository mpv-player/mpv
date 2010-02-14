/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <sys/stat.h>
#include <glob.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#include "sb.h"
#include "gtk_common.h"

#include "gui/app.h"
#include "help_mp.h"

#include "gui/mplayer/widgets.h"

GtkWidget * SkinList = NULL;
char      * sbSelectedSkin=NULL;
/* FIXME: Eventually remove the obsolete directory names. */
char      * sbMPlayerDirInHome=NULL;
char      * sbMPlayerDirInHome_obsolete=NULL;
char      * sbMPlayerPrefixDir=NULL;
char      * sbMPlayerPrefixDir_obsolete=NULL;

char * gtkOldSkin=NULL;
static char * prev=NULL;

GtkWidget * SkinBrowser = NULL;

void ShowSkinBrowser( void )
{
 if ( SkinBrowser ) gtkActive( SkinBrowser );
   else SkinBrowser=create_SkinBrowser();
}

static void HideSkinBrowser( void )
{
 if ( !SkinBrowser ) return;
 gtk_widget_hide( SkinBrowser );
 gtk_widget_destroy( SkinBrowser );
 SkinBrowser=NULL;
}

int gtkFillSkinList( gchar * mdir )
{
 gchar         * str[2];
 gchar         * tmp;
 int             i;
 glob_t          gg;
 struct stat     fs;

 gtkOldSkin=strdup( skinName );
 prev=gtkOldSkin;

 str[0]="default";
 str[1]="";
 if ( gtkFindCList( SkinList,str[0] ) == -1 ) gtk_clist_append( GTK_CLIST( SkinList ),str );

 glob( mdir,GLOB_NOSORT,NULL,&gg );
 for( i=0;i<(int)gg.gl_pathc;i++ )
  {
   if ( !strcmp( gg.gl_pathv[i],"." ) || !strcmp( gg.gl_pathv[i],".." ) ) continue;
   stat( gg.gl_pathv[i],&fs );
   if ( S_ISDIR( fs.st_mode ) )
    {
     tmp=strrchr( gg.gl_pathv[i],'/' ); tmp++;
     if ( !strcmp( tmp,"default" ) ) continue;
     str[0]=tmp;
     if ( gtkFindCList( SkinList,str[0] ) == -1 ) gtk_clist_append( GTK_CLIST( SkinList ),str );
    }
  }
 globfree( &gg );
 return 1;
}

static void prButton( GtkObject * object,gpointer user_data )
{
 if ( sbSelectedSkin )
 {
  switch ( (int)user_data )
   {
    case 0: // cancel
	if ( strcmp( sbSelectedSkin,gtkOldSkin ) ) ChangeSkin( gtkOldSkin );
	break;
   case 1: // ok
	ChangeSkin( sbSelectedSkin );
	if ( skinName ) free( skinName );
	skinName=strdup( sbSelectedSkin );
	break;
  }
 }
 HideSkinBrowser();
}

static void on_SkinList_select_row( GtkCList * clist,gint row,gint column,GdkEvent * bevent,gpointer user_data )
{
 gtk_clist_get_text( clist,row,0,&sbSelectedSkin );
 if ( strcmp( prev,sbSelectedSkin ) )
  {
   prev=sbSelectedSkin;
   ChangeSkin( sbSelectedSkin );
   gtkActive( SkinBrowser );
  }
 if( !bevent ) return;
 if( bevent->type == GDK_2BUTTON_PRESS )
  {
   if ( skinName ) free( skinName );
   skinName=strdup( sbSelectedSkin );
   HideSkinBrowser();
  }
}

GtkWidget * create_SkinBrowser( void )
{
 GtkWidget     * vbox5;
 GtkWidget     * scrolledwindow1;
 GtkWidget     * hbuttonbox4;
 GtkWidget     * Cancel;
 GtkWidget     * Ok;
 GtkAccelGroup * accel_group;

 accel_group = gtk_accel_group_new ();

 SkinBrowser=gtk_window_new( GTK_WINDOW_TOPLEVEL );
 gtk_widget_set_name( SkinBrowser,MSGTR_SkinBrowser );
 gtk_object_set_data( GTK_OBJECT( SkinBrowser ),MSGTR_SkinBrowser,SkinBrowser );
 gtk_widget_set_usize( SkinBrowser,256,320 );
 gtk_container_set_border_width( GTK_CONTAINER( SkinBrowser ),1 );
 GTK_WIDGET_SET_FLAGS( SkinBrowser,GTK_CAN_DEFAULT );
 gtk_widget_set_events( SkinBrowser,GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_FOCUS_CHANGE_MASK | GDK_STRUCTURE_MASK | GDK_PROPERTY_CHANGE_MASK | GDK_VISIBILITY_NOTIFY_MASK );
 gtk_window_set_title( GTK_WINDOW( SkinBrowser ),MSGTR_SkinBrowser );
 gtk_window_set_position( GTK_WINDOW( SkinBrowser ),GTK_WIN_POS_CENTER );
 gtk_window_set_policy( GTK_WINDOW( SkinBrowser ),FALSE,FALSE,TRUE );
 gtk_window_set_wmclass( GTK_WINDOW( SkinBrowser ),"SkinBrowser","MPlayer" );

 gtk_widget_realize( SkinBrowser );
 gtkAddIcon( SkinBrowser );

 vbox5=AddVBox( AddDialogFrame( SkinBrowser ),0 );
 AddLabel( MSGTR_SKIN_LABEL,vbox5 );
 AddHSeparator( vbox5 );

 scrolledwindow1=gtk_scrolled_window_new( NULL,NULL );
 gtk_widget_set_name( scrolledwindow1,"scrolledwindow1" );
 gtk_widget_ref( scrolledwindow1 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"scrolledwindow1",scrolledwindow1,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( scrolledwindow1 );
 gtk_box_pack_start( GTK_BOX( vbox5 ),scrolledwindow1,TRUE,TRUE,0 );
 gtk_container_set_border_width( GTK_CONTAINER( scrolledwindow1 ),2 );
 gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolledwindow1 ),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC );

 SkinList=gtk_clist_new( 1 );
 gtk_widget_set_name( SkinList,"SkinList" );
 gtk_widget_ref( SkinList );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"SkinList",SkinList,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( SkinList );
 gtk_container_add( GTK_CONTAINER( scrolledwindow1 ),SkinList );
 gtk_clist_set_column_width( GTK_CLIST( SkinList ),0,80 );
 gtk_clist_set_selection_mode( GTK_CLIST( SkinList ),GTK_SELECTION_SINGLE );
 gtk_clist_column_titles_hide( GTK_CLIST( SkinList ) );
 gtk_clist_set_shadow_type( GTK_CLIST( SkinList ),GTK_SHADOW_ETCHED_OUT );

 AddHSeparator( vbox5 );

 hbuttonbox4=AddHButtonBox( vbox5 );
  gtk_button_box_set_layout( GTK_BUTTON_BOX( hbuttonbox4 ),GTK_BUTTONBOX_SPREAD );
  gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox4 ),10 );

 Ok=AddButton( MSGTR_Ok,hbuttonbox4 );
 Cancel=AddButton( MSGTR_Cancel,hbuttonbox4 );

 gtk_widget_add_accelerator( Ok,"clicked",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE );
 gtk_widget_add_accelerator( Cancel,"clicked",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );

 gtk_signal_connect( GTK_OBJECT( SkinBrowser ),"destroy",GTK_SIGNAL_FUNC( WidgetDestroy ),&SkinBrowser );
 gtk_signal_connect( GTK_OBJECT( SkinList ),"select_row",GTK_SIGNAL_FUNC( on_SkinList_select_row ),NULL );
 gtk_signal_connect( GTK_OBJECT( Ok ),"clicked",GTK_SIGNAL_FUNC( prButton ),(void *)1 );
 gtk_signal_connect( GTK_OBJECT( Cancel ),"clicked",GTK_SIGNAL_FUNC( prButton ),(void *)0 );

 if ( ( sbMPlayerDirInHome_obsolete=calloc( 1,strlen( skinDirInHome_obsolete ) + 4 ) ) != NULL )
  { strcpy( sbMPlayerDirInHome_obsolete,skinDirInHome_obsolete ); strcat( sbMPlayerDirInHome_obsolete,"/*" ); }
 if ( ( sbMPlayerDirInHome=calloc( 1,strlen( skinDirInHome ) + 4 ) ) != NULL )
  { strcpy( sbMPlayerDirInHome,skinDirInHome ); strcat( sbMPlayerDirInHome,"/*" ); }
 if ( ( sbMPlayerPrefixDir_obsolete=calloc( 1,strlen( skinMPlayerDir ) + 4 ) ) != NULL )
  { strcpy( sbMPlayerPrefixDir_obsolete,skinMPlayerDir ); strcat( sbMPlayerPrefixDir_obsolete,"/*" ); }
 if ( ( sbMPlayerPrefixDir=calloc( 1,strlen( skinMPlayerDir ) + 4 ) ) != NULL )
  { strcpy( sbMPlayerPrefixDir,skinMPlayerDir ); strcat( sbMPlayerPrefixDir,"/*" ); }

 gtk_window_add_accel_group( GTK_WINDOW( SkinBrowser ),accel_group );
 gtk_widget_grab_focus( SkinList );

 return SkinBrowser;
}
