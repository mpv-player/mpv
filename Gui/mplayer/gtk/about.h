
#ifndef __WIDGET_ABOUT
#define __WIDGET_ABOUT

#include "../../language.h"

GtkWidget   * About;
GdkColormap * abColorMap;

void ab_Ok_released( GtkButton * button,gpointer user_data)
{
 gtk_widget_hide( AboutBox );
 gtkVisibleAboutBox=0;
}

GtkWidget * create_About( void )
{
 GtkWidget     * frame17;
 GtkWidget     * frame18;
 GtkWidget     * frame19;
 GtkWidget     * frame20;
 GtkWidget     * hbox7;
 GtkWidget     * pixmap1;
 GtkWidget     * vbox8;
 GtkWidget     * label4;
 GtkWidget     * hbuttonbox6;
 GtkWidget     * Ok;
 GtkAccelGroup * accel_group;

 GtkStyle      * pixmapstyle;
 GtkPixmap     * pixmapwid;
 GdkBitmap     * mask;

 accel_group=gtk_accel_group_new();

 About=gtk_window_new( GTK_WINDOW_DIALOG );
 gtk_widget_set_name( About,langAbout );
 gtk_object_set_data( GTK_OBJECT( About ),langAbout,About );
 gtk_widget_set_usize( About,290,105 );
 GTK_WIDGET_SET_FLAGS( About,GTK_CAN_FOCUS );
 GTK_WIDGET_SET_FLAGS( About,GTK_CAN_DEFAULT );
 gtk_window_set_title( GTK_WINDOW( About ),langAbout );
 gtk_window_set_position( GTK_WINDOW( About ),GTK_WIN_POS_CENTER );
 gtk_window_set_modal( GTK_WINDOW( About ),TRUE );
 gtk_window_set_policy( GTK_WINDOW( About ),FALSE,FALSE,FALSE );

 frame17=gtk_frame_new( NULL );
 gtk_widget_set_name( frame17,"frame17" );
 gtk_widget_ref( frame17 );
 gtk_object_set_data_full( GTK_OBJECT( About ),"frame17",frame17,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( frame17 );
 gtk_container_add( GTK_CONTAINER( About ),frame17 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame17 ),GTK_SHADOW_IN );

 frame18=gtk_frame_new( NULL );
 gtk_widget_set_name( frame18,"frame18" );
 gtk_widget_ref( frame18 );
 gtk_object_set_data_full( GTK_OBJECT( About ),"frame18",frame18,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( frame18 );
 gtk_container_add( GTK_CONTAINER( frame17 ),frame18 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame18 ),GTK_SHADOW_NONE );

 frame19=gtk_frame_new( NULL );
 gtk_widget_set_name( frame19,"frame19" );
 gtk_widget_ref( frame19 );
 gtk_object_set_data_full( GTK_OBJECT( About ),"frame19",frame19,( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( frame19 );
 gtk_container_add( GTK_CONTAINER( frame18 ),frame19 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame19 ),GTK_SHADOW_ETCHED_OUT );

 frame20=gtk_frame_new( NULL );
 gtk_widget_set_name( frame20,"frame20" );
 gtk_widget_ref( frame20 );
 gtk_object_set_data_full( GTK_OBJECT( About ),"frame20",frame20,( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( frame20 );
 gtk_container_add( GTK_CONTAINER( frame19 ),frame20 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame20 ),GTK_SHADOW_NONE );

 hbox7=gtk_hbox_new( FALSE,0 );
 gtk_widget_set_name( hbox7,"hbox7" );
 gtk_widget_ref( hbox7 );
 gtk_object_set_data_full( GTK_OBJECT( About ),"hbox7",hbox7,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( hbox7 );
 gtk_container_add( GTK_CONTAINER( frame20 ),hbox7 );

 pixmapstyle=gtk_widget_get_style( About );
 pixmapwid=gdk_pixmap_colormap_create_from_xpm_d( About->window,gdk_colormap_get_system(),&mask,&pixmapstyle->bg[GTK_STATE_NORMAL],(gchar **)logo_xpm );pixmap1=gtk_pixmap_new( pixmapwid,mask );

 gtk_widget_set_name( pixmap1,"pixmap1" );
 gtk_widget_ref( pixmap1 );
 gtk_object_set_data_full( GTK_OBJECT( About ),"pixmap1",pixmap1,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( pixmap1 );
 gtk_box_pack_start( GTK_BOX( hbox7 ),pixmap1,TRUE,TRUE,0 );

 vbox8=gtk_vbox_new( FALSE,0 );
 gtk_widget_set_name( vbox8,"vbox8" );
 gtk_widget_ref( vbox8 );
 gtk_object_set_data_full( GTK_OBJECT( About ),"vbox8",vbox8,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( vbox8 );
 gtk_box_pack_start( GTK_BOX( hbox7 ),vbox8,TRUE,TRUE,0 );

 label4=gtk_label_new( "The Movie Player for Linux" );
 gtk_widget_set_name( label4,"label4" );
 gtk_widget_ref( label4 );
 gtk_object_set_data_full( GTK_OBJECT( About ),"label4",label4,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( label4 );
 gtk_box_pack_start( GTK_BOX( vbox8 ),label4,FALSE,FALSE,0 );
 gtk_widget_set_usize( label4,-2,50 );
 gtk_label_set_line_wrap( GTK_LABEL( label4 ),TRUE );

 hbuttonbox6=gtk_hbutton_box_new();
 gtk_widget_set_name( hbuttonbox6,"hbuttonbox6" );
 gtk_widget_ref( hbuttonbox6 );
 gtk_object_set_data_full( GTK_OBJECT( About ),"hbuttonbox6",hbuttonbox6,( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( hbuttonbox6 );
 gtk_box_pack_start( GTK_BOX( vbox8 ),hbuttonbox6,FALSE,FALSE,0 );
 gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox6 ),0 );
 gtk_button_box_set_child_size( GTK_BUTTON_BOX( hbuttonbox6 ),115,33 );

 Ok=gtk_button_new_with_label( langOk );
 gtk_widget_set_name( Ok,langOk );
 gtk_widget_ref( Ok );
 gtk_object_set_data_full( GTK_OBJECT( About ),langOk,Ok,( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( Ok );
 gtk_container_add( GTK_CONTAINER( hbuttonbox6 ),Ok );
 gtk_widget_set_usize( Ok,49,32 );
 GTK_WIDGET_SET_FLAGS( Ok,GTK_CAN_DEFAULT );
 gtk_widget_add_accelerator (Ok, "released",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE);

// gtk_signal_connect( GTK_OBJECT( About ),"destroy",GTK_SIGNAL_FUNC( on_About_destroy ),NULL );
 gtk_signal_connect( GTK_OBJECT( About ),"destroy",GTK_SIGNAL_FUNC( ab_Ok_released ),NULL );
 gtk_signal_connect( GTK_OBJECT( Ok ),"released",GTK_SIGNAL_FUNC( ab_Ok_released ),NULL);

 gtk_window_add_accel_group( GTK_WINDOW( About ),accel_group );

 gtk_widget_grab_focus( Ok );
 return About;
}

#endif