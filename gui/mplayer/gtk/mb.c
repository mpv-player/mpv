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

#include "config.h"
#include "gui/app.h"
#include "help_mp.h"

#include "gui/mplayer/pixmaps/error.xpm"
#include "gui/mplayer/pixmaps/warning.xpm"

#include "gui/mplayer/widgets.h"
#include "mb.h"
#include "gtk_common.h"

GtkWidget * gtkMessageBoxText;
GtkWidget * MessageBox = NULL;

void ShowMessageBox( const char * msg )
{
 if ( MessageBox ) { gtk_widget_hide( MessageBox ); gtk_widget_destroy( MessageBox ); }
 MessageBox=create_MessageBox( 0 );
 if ( strlen( msg ) < 20 ) gtk_widget_set_usize( MessageBox,196,-1 );
}

static void on_Ok_released( GtkButton * button,gpointer user_data  )
{ 
 gtk_widget_hide( MessageBox ); 
 gtk_widget_destroy( MessageBox ); 
 MessageBox=NULL;
}

GtkWidget * create_MessageBox( int type )
{
 GtkWidget * vbox1;
 GtkWidget * hbox1;
 GtkWidget * hbuttonbox1;
 GtkWidget * Ok;
 GtkAccelGroup * accel_group;
 GtkStyle * pixmapstyle;
 GdkPixmap * pixmapwid;
 GdkBitmap * mask;

 accel_group=gtk_accel_group_new();

 MessageBox=gtk_window_new( GTK_WINDOW_TOPLEVEL );
 gtk_widget_set_name( MessageBox,"MessageBox" );
 gtk_object_set_data( GTK_OBJECT( MessageBox ),"MessageBox",MessageBox );
 gtk_widget_set_events( MessageBox,GDK_EXPOSURE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_VISIBILITY_NOTIFY_MASK );
 gtk_window_set_title( GTK_WINDOW( MessageBox ),"MPlayer ..." );
 gtk_window_set_position( GTK_WINDOW( MessageBox ),GTK_WIN_POS_CENTER );
 gtk_window_set_modal( GTK_WINDOW( MessageBox ),TRUE );
 gtk_window_set_policy( GTK_WINDOW( MessageBox ),TRUE,TRUE,FALSE );
 gtk_window_set_wmclass( GTK_WINDOW( MessageBox ),"Message","MPlayer" );
 
 gtk_widget_realize( MessageBox );
 gtkAddIcon( MessageBox );

 vbox1=AddVBox( AddDialogFrame( MessageBox ),0 );
 hbox1=AddHBox( vbox1,1 );

 pixmapstyle=gtk_widget_get_style( MessageBox );

 pixmapwid=gdk_pixmap_colormap_create_from_xpm_d( MessageBox->window,gdk_colormap_get_system(),&mask,&pixmapstyle->bg[GTK_STATE_NORMAL],(gchar ** )warning_xpm );
 WarningPixmap=gtk_pixmap_new( pixmapwid,mask );
 pixmapwid=gdk_pixmap_colormap_create_from_xpm_d( MessageBox->window,gdk_colormap_get_system(),&mask,&pixmapstyle->bg[GTK_STATE_NORMAL],(gchar ** )error_xpm );
 ErrorPixmap=gtk_pixmap_new( pixmapwid,mask );

 gtk_widget_set_name( WarningPixmap,"pixmap1" );
 gtk_widget_hide( WarningPixmap );
 gtk_box_pack_start( GTK_BOX( hbox1 ),WarningPixmap,FALSE,FALSE,0 );
 gtk_widget_set_usize( WarningPixmap,55,-2 );

 gtk_widget_set_name( ErrorPixmap,"pixmap1" );
 gtk_widget_hide( ErrorPixmap );
 gtk_box_pack_start( GTK_BOX( hbox1 ),ErrorPixmap,FALSE,FALSE,0 );
 gtk_widget_set_usize( ErrorPixmap,55,-2 );

 gtkMessageBoxText=gtk_label_new( "Text jol. Ha ezt megerted,akkor neked nagyon jo a magyar tudasod,te." );
 gtk_widget_set_name( gtkMessageBoxText,"gtkMessageBoxText" );
 gtk_widget_show( gtkMessageBoxText );
 gtk_box_pack_start( GTK_BOX( hbox1 ),gtkMessageBoxText,TRUE,TRUE,0 );
// gtk_label_set_justify( GTK_LABEL( gtkMessageBoxText ),GTK_JUSTIFY_FILL );
 gtk_label_set_justify( GTK_LABEL( gtkMessageBoxText ),GTK_JUSTIFY_CENTER );
 gtk_label_set_line_wrap( GTK_LABEL( gtkMessageBoxText ),FALSE );

 AddHSeparator( vbox1 );
 hbuttonbox1=AddHButtonBox( vbox1 );
 Ok=AddButton( MSGTR_Ok,hbuttonbox1 );

 gtk_widget_add_accelerator( Ok,"clicked",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE );
 gtk_widget_add_accelerator( Ok,"clicked",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );

 gtk_signal_connect( GTK_OBJECT( MessageBox ),"destroy",GTK_SIGNAL_FUNC( WidgetDestroy ),&MessageBox );
 gtk_signal_connect( GTK_OBJECT( Ok ),"clicked",GTK_SIGNAL_FUNC( on_Ok_released ),NULL );

 gtk_window_add_accel_group( GTK_WINDOW( MessageBox ),accel_group );

 return MessageBox;
}
