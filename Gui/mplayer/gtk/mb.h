
#ifndef __MY_MESSAGEBOX
#define __MY_MESSAGEBOX

GtkWidget * gtkMessageBoxText;

void on_MessageBox_destroy( GtkObject * object,gpointer user_data )
{
 gtk_widget_hide( MessageBox );
 gtkVisibleMessageBox=0;
}

void on_Ok_released( GtkButton * button,gpointer user_data )
{
 gtk_widget_hide( MessageBox );
 gtkVisibleMessageBox=0;
 gtkSendMessage( evMessageBox );
}

GtkWidget * create_MessageBox( void )
{
  GtkWidget *MessageBox;
  GtkWidget *frame1;
  GtkWidget *frame2;
  GtkWidget *frame3;
  GtkWidget *frame4;
  GtkWidget *vbox1;
  GtkWidget *vbox2;
  GtkWidget *hseparator1;
  GtkWidget *hbuttonbox1;
  GtkWidget *Ok;
  GtkAccelGroup *accel_group;

  accel_group=gtk_accel_group_new();

  MessageBox=gtk_window_new( GTK_WINDOW_DIALOG );
  gtk_widget_set_name( MessageBox,MSGTR_MessageBox );
  gtk_object_set_data( GTK_OBJECT( MessageBox ),MSGTR_MessageBox,MessageBox );
  gtk_widget_set_usize( MessageBox,420,128 );
  GTK_WIDGET_SET_FLAGS( MessageBox,GTK_CAN_FOCUS );
  GTK_WIDGET_SET_FLAGS( MessageBox,GTK_CAN_DEFAULT );
  gtk_widget_set_events( MessageBox,GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_FOCUS_CHANGE_MASK | GDK_STRUCTURE_MASK );
  gtk_window_set_title( GTK_WINDOW( MessageBox ),MSGTR_MessageBox );
  gtk_window_set_position( GTK_WINDOW( MessageBox ),GTK_WIN_POS_CENTER );
  gtk_window_set_modal( GTK_WINDOW( MessageBox ),TRUE );
  gtk_window_set_policy( GTK_WINDOW( MessageBox ),FALSE,FALSE,FALSE );
  gtk_window_set_wmclass( GTK_WINDOW( MessageBox ),MSGTR_MessageBox,MSGTR_MessageBox );

  frame1=gtk_frame_new( NULL );
  gtk_widget_set_name( frame1,"frame1" );
  gtk_widget_ref( frame1 );
  gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"frame1",frame1,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( frame1 );
  gtk_container_add( GTK_CONTAINER( MessageBox ),frame1 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame1 ),GTK_SHADOW_IN );

  frame2=gtk_frame_new( NULL );
  gtk_widget_set_name( frame2,"frame2" );
  gtk_widget_ref( frame2 );
  gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"frame2",frame2,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( frame2 );
  gtk_container_add( GTK_CONTAINER( frame1 ),frame2 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame2 ),GTK_SHADOW_NONE );

  frame3=gtk_frame_new( NULL );
  gtk_widget_set_name( frame3,"frame3" );
  gtk_widget_ref( frame3 );
  gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"frame3",frame3,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( frame3 );
  gtk_container_add( GTK_CONTAINER( frame2 ),frame3 );

  frame4=gtk_frame_new( NULL );
  gtk_widget_set_name( frame4,"frame4" );
  gtk_widget_ref( frame4 );
  gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"frame4",frame4,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( frame4 );
  gtk_container_add( GTK_CONTAINER( frame3 ),frame4 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame4 ),GTK_SHADOW_NONE );

  vbox1=gtk_vbox_new( FALSE,0 );
  gtk_widget_set_name( vbox1,"vbox1" );
  gtk_widget_ref( vbox1 );
  gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"vbox1",vbox1,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( vbox1 );
  gtk_container_add( GTK_CONTAINER( frame4 ),vbox1 );

  vbox2=gtk_vbox_new( FALSE,0 );
  gtk_widget_set_name( vbox2,"vbox2" );
  gtk_widget_ref( vbox2 );
  gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"vbox2",vbox2,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( vbox2 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),vbox2,TRUE,FALSE,0 );

  gtkMessageBoxText=gtk_label_new( "Ez." );
  gtk_widget_set_name( gtkMessageBoxText,"gtkMessageBoxText" );
  gtk_widget_ref( gtkMessageBoxText );
  gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"gtkMessageBoxText",gtkMessageBoxText,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( gtkMessageBoxText );
  gtk_box_pack_start( GTK_BOX( vbox2 ),gtkMessageBoxText,FALSE,FALSE,0 );
//  gtk_widget_set_usize( gtkMessageBoxText,-2,77 );
  gtk_widget_set_usize( gtkMessageBoxText,384,77 );
  gtk_label_set_justify( GTK_LABEL( gtkMessageBoxText ),GTK_JUSTIFY_FILL );
  gtk_label_set_line_wrap( GTK_LABEL( gtkMessageBoxText ),TRUE );

  hseparator1=gtk_hseparator_new();
  gtk_widget_set_name( hseparator1,"hseparator1" );
  gtk_widget_ref( hseparator1 );
  gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"hseparator1",hseparator1,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( hseparator1 );
  gtk_box_pack_start( GTK_BOX( vbox2 ),hseparator1,TRUE,TRUE,0 );

  hbuttonbox1=gtk_hbutton_box_new();
  gtk_widget_set_name( hbuttonbox1,"hbuttonbox1" );
  gtk_widget_ref( hbuttonbox1 );
  gtk_object_set_data_full( GTK_OBJECT( MessageBox ),"hbuttonbox1",hbuttonbox1,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( hbuttonbox1 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),hbuttonbox1,FALSE,FALSE,0 );
  GTK_WIDGET_SET_FLAGS( hbuttonbox1,GTK_CAN_FOCUS );
  GTK_WIDGET_SET_FLAGS( hbuttonbox1,GTK_CAN_DEFAULT );
  gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox1 ),0 );
  gtk_button_box_set_child_size( GTK_BUTTON_BOX( hbuttonbox1 ),60,0 );
  gtk_button_box_set_child_ipadding( GTK_BUTTON_BOX( hbuttonbox1 ),10,0 );

  Ok=gtk_button_new_with_label( MSGTR_Ok );
  gtk_widget_set_name( Ok,MSGTR_Ok );
  gtk_widget_ref( Ok );
  gtk_object_set_data_full( GTK_OBJECT( MessageBox ),MSGTR_Ok,Ok,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( Ok );
  gtk_container_add( GTK_CONTAINER( hbuttonbox1 ),Ok );
  gtk_widget_set_usize( Ok,100,-2 );
  GTK_WIDGET_SET_FLAGS( Ok,GTK_CAN_DEFAULT );
  gtk_widget_add_accelerator( Ok,"released",accel_group,
                              GDK_Return,0,
                              GTK_ACCEL_VISIBLE );

  gtk_signal_connect( GTK_OBJECT( MessageBox ),"destroy",
                      GTK_SIGNAL_FUNC( on_MessageBox_destroy ),
                      NULL );
  gtk_signal_connect( GTK_OBJECT( Ok ),"released",
                      GTK_SIGNAL_FUNC( on_Ok_released ),
                      NULL );

  gtk_window_add_accel_group( GTK_WINDOW( MessageBox ),accel_group );

  return MessageBox;
}

#endif
