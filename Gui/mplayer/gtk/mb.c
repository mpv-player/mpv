
#include "../../events.h"
#include "../../../config.h"
#include "../../../help_mp.h"

#include "../pixmaps/error.xpm"
#include "../pixmaps/warning.xpm"

#include "../widgets.h"
#include "mb.h"

GtkWidget * gtkMessageBoxText;
GtkWidget * MessageBox;

int gtkVMessageBox = 0;

void ShowMessageBox( char * msg )
{
 if ( gtkVMessageBox ) { gtk_widget_hide( MessageBox ); gtk_widget_destroy( MessageBox ); }
 MessageBox=create_MessageBox( 0 );
 if ( strlen( msg ) < 20 ) gtk_widget_set_usize( MessageBox,196,-1 );
 gtkIncVisible();
}

static void on_Ok_released( GtkButton * button,gpointer user_data  )
{ gtkVMessageBox=0; gtk_widget_hide( MessageBox ); gtk_widget_destroy( MessageBox ); gtkDecVisible(); }

static void on_MessageBox_show( GtkButton * button,gpointer user_data  )
{ gtkVMessageBox=(int)user_data; }

GtkWidget * create_MessageBox( int type )
{
 GtkWidget * MessageBox;
 GtkWidget * frame1;
 GtkWidget * frame2;
 GtkWidget * frame3;
 GtkWidget * frame4;
 GtkWidget * vbox1;
 GtkWidget * hbox1;
 GtkWidget * hseparator1;
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

 frame1=gtk_frame_new( NULL );
 gtk_widget_set_name( frame1,"frame1" );
 gtk_widget_ref( frame1 );
 gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"frame1",frame1,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( frame1 );
 gtk_container_add( GTK_CONTAINER( MessageBox ),frame1 );
 gtk_container_set_border_width( GTK_CONTAINER( frame1 ),1 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame1 ),GTK_SHADOW_IN );

 frame2=gtk_frame_new( NULL );
 gtk_widget_set_name( frame2,"frame2" );
 gtk_widget_ref( frame2 );
 gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"frame2",frame2,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( frame2 );
 gtk_container_add( GTK_CONTAINER( frame1 ),frame2 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame2 ),GTK_SHADOW_NONE );

 frame3=gtk_frame_new( NULL );
 gtk_widget_set_name( frame3,"frame3" );
 gtk_widget_ref( frame3 );
 gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"frame3",frame3,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( frame3 );
 gtk_container_add( GTK_CONTAINER( frame2 ),frame3 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame3 ),GTK_SHADOW_ETCHED_OUT );

 frame4=gtk_frame_new( NULL );
 gtk_widget_set_name( frame4,"frame4" );
 gtk_widget_ref( frame4 );
 gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"frame4",frame4,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( frame4 );
 gtk_container_add( GTK_CONTAINER( frame3 ),frame4 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame4 ),GTK_SHADOW_NONE );

 vbox1=gtk_vbox_new( FALSE,0 );
 gtk_widget_set_name( vbox1,"vbox1" );
 gtk_widget_ref( vbox1 );
 gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"vbox1",vbox1,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( vbox1 );
 gtk_container_add( GTK_CONTAINER( frame4 ),vbox1 );

 hbox1=gtk_hbox_new( FALSE,0 );
 gtk_widget_set_name( hbox1,"hbox1" );
 gtk_widget_ref( hbox1 );
 gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"hbox1",hbox1,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( hbox1 );
 gtk_box_pack_start( GTK_BOX( vbox1 ),hbox1,TRUE,TRUE,0 );

 pixmapstyle=gtk_widget_get_style( MessageBox );

 pixmapwid=gdk_pixmap_colormap_create_from_xpm_d( MessageBox->window,gdk_colormap_get_system(),&mask,&pixmapstyle->bg[GTK_STATE_NORMAL],(gchar ** )warning_xpm );
 WarningPixmap=gtk_pixmap_new( pixmapwid,mask );
 pixmapwid=gdk_pixmap_colormap_create_from_xpm_d( MessageBox->window,gdk_colormap_get_system(),&mask,&pixmapstyle->bg[GTK_STATE_NORMAL],(gchar ** )error_xpm );
 ErrorPixmap=gtk_pixmap_new( pixmapwid,mask );

 gtk_widget_set_name( WarningPixmap,"pixmap1" );
 gtk_widget_ref( WarningPixmap );
 gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"pixmap1",WarningPixmap,(GtkDestroyNotify )gtk_widget_unref );
 gtk_widget_hide( WarningPixmap );
 gtk_box_pack_start( GTK_BOX( hbox1 ),WarningPixmap,FALSE,FALSE,0 );
 gtk_widget_set_usize( WarningPixmap,55,-2 );

 gtk_widget_set_name( ErrorPixmap,"pixmap1" );
 gtk_widget_ref( ErrorPixmap );
 gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"pixmap1",ErrorPixmap,(GtkDestroyNotify )gtk_widget_unref );
 gtk_widget_hide( ErrorPixmap );
 gtk_box_pack_start( GTK_BOX( hbox1 ),ErrorPixmap,FALSE,FALSE,0 );
 gtk_widget_set_usize( ErrorPixmap,55,-2 );

 gtkMessageBoxText=gtk_label_new( "Text jol. Ha ezt megerted,akkor neked nagyon jo a magyar tudasod,te." );
 gtk_widget_set_name( gtkMessageBoxText,"gtkMessageBoxText" );
 gtk_widget_ref( gtkMessageBoxText );
 gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"gtkMessageBoxText",gtkMessageBoxText,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( gtkMessageBoxText );
 gtk_box_pack_start( GTK_BOX( hbox1 ),gtkMessageBoxText,TRUE,TRUE,0 );
 gtk_label_set_justify( GTK_LABEL( gtkMessageBoxText ),GTK_JUSTIFY_FILL );
 gtk_label_set_line_wrap( GTK_LABEL( gtkMessageBoxText ),FALSE );

 hseparator1=gtk_hseparator_new();
 gtk_widget_set_name( hseparator1,"hseparator1" );
 gtk_widget_ref( hseparator1 );
 gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"hseparator1",hseparator1,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( hseparator1 );
 gtk_box_pack_start( GTK_BOX( vbox1 ),hseparator1,FALSE,FALSE,0 );
 gtk_widget_set_usize( hseparator1,-2,9 );

 hbuttonbox1=gtk_hbutton_box_new();
 gtk_widget_set_name( hbuttonbox1,"hbuttonbox1" );
 gtk_widget_ref( hbuttonbox1 );
 gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"hbuttonbox1",hbuttonbox1,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( hbuttonbox1 );
 gtk_widget_set_usize( hbuttonbox1,-2,25 );
 gtk_button_box_set_child_size( GTK_BUTTON_BOX( hbuttonbox1 ),75,0 );
 gtk_box_pack_start( GTK_BOX( vbox1 ),hbuttonbox1,FALSE,FALSE,0 );

 Ok=gtk_button_new_with_label( MSGTR_Ok );
 gtk_widget_set_name( Ok,MSGTR_Ok );
 gtk_widget_ref( Ok );
 gtk_object_set_data_full( GTK_OBJECT( MessageBox ),MSGTR_Ok,Ok,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( Ok );
 gtk_container_add( GTK_CONTAINER( hbuttonbox1 ),Ok );
 gtk_widget_add_accelerator( Ok,"released",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE );
 gtk_widget_add_accelerator( Ok,"released",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );

 gtk_signal_connect( GTK_OBJECT( MessageBox ),"destroy_event",GTK_SIGNAL_FUNC( on_Ok_released ),NULL );
 gtk_signal_connect( GTK_OBJECT( MessageBox ),"show",GTK_SIGNAL_FUNC( on_MessageBox_show ),(void *)1 );
 gtk_signal_connect( GTK_OBJECT( MessageBox ),"hide",GTK_SIGNAL_FUNC( on_MessageBox_show ),0 );
 gtk_signal_connect( GTK_OBJECT( Ok ),"released",GTK_SIGNAL_FUNC( on_Ok_released ),NULL );

 gtk_window_add_accel_group( GTK_WINDOW( MessageBox ),accel_group );

 return MessageBox;
}
