
#include "../../events.h"
#include "../../../config.h"
#include "../../../help_mp.h"

#include "../widgets.h"
#include "pl.h"

void HidePlayList( void )
{
 gtk_widget_destroy( PlayList );
}

void pl_PlayList_destroy( GtkObject * object,gpointer user_data )
{ HidePlayList(); }

void pl_Add_released( GtkButton * button,gpointer user_data )
{
}

void pl_Remove_released( GtkButton * button,gpointer user_data )
{
}

void pl_Ok_released( GtkButton * button,gpointer user_data )
{ HidePlayList(); }

void pl_Cancel_released( GtkButton * button,gpointer user_data )
{ HidePlayList(); }

void pl_DirTree_select_child( GtkTree * tree,GtkWidget * widget,gpointer user_data )
{
}

void pl_DirTree_selection_changed( GtkTree * tree,gpointer user_data )
{
}

void pl_DirTree_unselect_child( GtkTree * tree,GtkWidget * widget,gpointer user_data )
{
}

void pl_FNameList_select_child( GtkList * list,GtkWidget * widget,gpointer user_data )
{
}

void pl_FNameList_selection_changed( GtkList * list,gpointer user_data )
{
}

void pl_FNameList_unselect_child( GtkList * list,GtkWidget * widget,gpointer user_data )
{
}

void pl_SelectedList_select_child( GtkList * list,GtkWidget * widget,gpointer user_data )
{
}

void pl_SelectedList_selection_changed( GtkList * list,gpointer user_data )
{
}

void pl_SelectedList_unselect_child( GtkList * list,GtkWidget * widget,gpointer user_data )
{
}

GtkWidget* create_PlayList( void )
{
  GtkWidget *PlayList;
  GtkWidget *frame9;
  GtkWidget *frame10;
  GtkWidget *frame11;
  GtkWidget *frame12;
  GtkWidget *hbox5;
  GtkWidget *frame13;
  GtkWidget *frame14;
  GtkWidget *DirTree;
  GtkWidget *vbox6;
  GtkWidget *frame15;
  GtkWidget *FNameList;
  GtkWidget *frame16;
  GtkWidget *SelectedList;
  GtkWidget *hseparator6;
  GtkWidget *hbuttonbox5;
  GtkWidget *Add;
  GtkWidget *Remove;
  GtkWidget *Ok;
  GtkWidget *Cancel;

  PlayList = gtk_window_new( GTK_WINDOW_DIALOG );
  gtk_object_set_data( GTK_OBJECT( PlayList ),MSGTR_PlayList,PlayList );
  gtk_widget_set_usize( PlayList,512,256 );
  GTK_WIDGET_SET_FLAGS( PlayList,GTK_CAN_FOCUS );
  GTK_WIDGET_SET_FLAGS( PlayList,GTK_CAN_DEFAULT );
  gtk_widget_set_events( PlayList,GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_FOCUS_CHANGE_MASK | GDK_STRUCTURE_MASK | GDK_PROPERTY_CHANGE_MASK | GDK_VISIBILITY_NOTIFY_MASK );
  gtk_window_set_title( GTK_WINDOW( PlayList ),MSGTR_PlayList );
  gtk_window_set_position( GTK_WINDOW( PlayList ),GTK_WIN_POS_CENTER );
  gtk_window_set_policy( GTK_WINDOW( PlayList ),FALSE,FALSE,TRUE );

  frame9 = gtk_frame_new( NULL );
  gtk_widget_ref( frame9 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"frame9",frame9,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( frame9 );
  gtk_container_add( GTK_CONTAINER( PlayList ),frame9 );
  gtk_container_set_border_width( GTK_CONTAINER( frame9 ),1 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame9 ),GTK_SHADOW_IN );

  frame10 = gtk_frame_new( NULL );
  gtk_widget_ref( frame10 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"frame10",frame10,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( frame10 );
  gtk_container_add( GTK_CONTAINER( frame9 ),frame10 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame10 ),GTK_SHADOW_NONE );

  frame11 = gtk_frame_new( NULL );
  gtk_widget_ref( frame11 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"frame11",frame11,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( frame11 );
  gtk_container_add( GTK_CONTAINER( frame10 ),frame11 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame11 ),GTK_SHADOW_ETCHED_OUT );

  frame12 = gtk_frame_new( NULL );
  gtk_widget_ref( frame12 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"frame12",frame12,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( frame12 );
  gtk_container_add( GTK_CONTAINER( frame11 ),frame12 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame12 ),GTK_SHADOW_NONE );

  hbox5 = gtk_hbox_new( FALSE,0 );
  gtk_widget_ref( hbox5 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"hbox5",hbox5,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( hbox5 );
  gtk_container_add( GTK_CONTAINER( frame12 ),hbox5 );

  frame13 = gtk_frame_new( NULL );
  gtk_widget_ref( frame13 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"frame13",frame13,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( frame13 );
  gtk_box_pack_start( GTK_BOX( hbox5 ),frame13,TRUE,TRUE,0 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame13 ),GTK_SHADOW_ETCHED_OUT );

  frame14 = gtk_frame_new( NULL );
  gtk_widget_ref( frame14 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"frame14",frame14,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( frame14 );
  gtk_container_add( GTK_CONTAINER( frame13 ),frame14 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame14 ),GTK_SHADOW_NONE );

  DirTree = gtk_tree_new();
  gtk_widget_ref( DirTree );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"DirTree",DirTree,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( DirTree );
  gtk_container_add( GTK_CONTAINER( frame14 ),DirTree );
  gtk_widget_set_usize( DirTree,217,-2 );

  vbox6 = gtk_vbox_new( FALSE,0 );
  gtk_widget_ref( vbox6 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"vbox6",vbox6,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( vbox6 );
  gtk_box_pack_start( GTK_BOX( hbox5 ),vbox6,TRUE,TRUE,0 );

  frame15 = gtk_frame_new( NULL );
  gtk_widget_ref( frame15 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"frame15",frame15,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( frame15 );
  gtk_box_pack_start( GTK_BOX( vbox6 ),frame15,TRUE,TRUE,0 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame15 ),GTK_SHADOW_ETCHED_OUT );

  FNameList = gtk_list_new();
  gtk_widget_ref( FNameList );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"FNameList",FNameList,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( FNameList );
  gtk_container_add( GTK_CONTAINER( frame15 ),FNameList );

  frame16 = gtk_frame_new( NULL );
  gtk_widget_ref( frame16 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"frame16",frame16,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( frame16 );
  gtk_box_pack_start( GTK_BOX( vbox6 ),frame16,TRUE,TRUE,0 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame16 ),GTK_SHADOW_ETCHED_OUT );

  SelectedList = gtk_list_new();
  gtk_widget_ref( SelectedList );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"SelectedList",SelectedList,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( SelectedList );
  gtk_container_add( GTK_CONTAINER( frame16 ),SelectedList );

  hseparator6 = gtk_hseparator_new();
  gtk_widget_ref( hseparator6 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"hseparator6",hseparator6,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( hseparator6 );
  gtk_box_pack_start( GTK_BOX( vbox6 ),hseparator6,FALSE,TRUE,0 );
  gtk_widget_set_usize( hseparator6,-2,11 );

  hbuttonbox5 = gtk_hbutton_box_new();
  gtk_widget_ref( hbuttonbox5 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"hbuttonbox5",hbuttonbox5,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( hbuttonbox5 );
  gtk_box_pack_start( GTK_BOX( vbox6 ),hbuttonbox5,FALSE,FALSE,0 );
  gtk_button_box_set_layout( GTK_BUTTON_BOX( hbuttonbox5 ),GTK_BUTTONBOX_END );
  gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox5 ),0 );
  gtk_button_box_set_child_size( GTK_BUTTON_BOX( hbuttonbox5 ),65,27 );
  gtk_button_box_set_child_ipadding( GTK_BUTTON_BOX( hbuttonbox5 ),2,0 );

  Add = gtk_button_new_with_label( MSGTR_Add );
  gtk_widget_ref( Add );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),MSGTR_Add,Add,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( Add );
  gtk_container_add( GTK_CONTAINER( hbuttonbox5 ),Add );
  gtk_widget_set_usize( Add,45,-2 );
  GTK_WIDGET_SET_FLAGS( Add,GTK_CAN_DEFAULT );

  Remove = gtk_button_new_with_label( MSGTR_Remove );
  gtk_widget_ref( Remove );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),MSGTR_Remove,Remove,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( Remove );
  gtk_container_add( GTK_CONTAINER( hbuttonbox5 ),Remove );
  gtk_widget_set_usize( Remove,45,-2 );
  GTK_WIDGET_SET_FLAGS( Remove,GTK_CAN_DEFAULT );

  Ok = gtk_button_new_with_label( MSGTR_Ok );
  gtk_widget_ref( Ok );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),MSGTR_Ok,Ok,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( Ok );
  gtk_container_add( GTK_CONTAINER( hbuttonbox5 ),Ok );
  gtk_widget_set_usize( Ok,45,-2 );
  GTK_WIDGET_SET_FLAGS( Ok,GTK_CAN_DEFAULT );

  Cancel = gtk_button_new_with_label( MSGTR_Cancel );
  gtk_widget_ref( Cancel );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),MSGTR_Cancel,Cancel,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( Cancel );
  gtk_container_add( GTK_CONTAINER( hbuttonbox5 ),Cancel );
  gtk_widget_set_usize( Cancel,45,-2 );
  GTK_WIDGET_SET_FLAGS( Cancel,GTK_CAN_DEFAULT );

  gtk_signal_connect( GTK_OBJECT( PlayList ),"destroy",
                      GTK_SIGNAL_FUNC( pl_PlayList_destroy ),
                      NULL );
  gtk_signal_connect( GTK_OBJECT( DirTree ),"select_child",
                      GTK_SIGNAL_FUNC( pl_DirTree_select_child ),
                      NULL );
  gtk_signal_connect( GTK_OBJECT( DirTree ),"selection_changed",
                      GTK_SIGNAL_FUNC( pl_DirTree_selection_changed ),
                      NULL );
  gtk_signal_connect( GTK_OBJECT( DirTree ),"unselect_child",
                      GTK_SIGNAL_FUNC( pl_DirTree_unselect_child ),
                      NULL );
  gtk_signal_connect( GTK_OBJECT( FNameList ),"select_child",
                      GTK_SIGNAL_FUNC( pl_FNameList_select_child ),
                      NULL );
  gtk_signal_connect( GTK_OBJECT( FNameList ),"selection_changed",
                      GTK_SIGNAL_FUNC( pl_FNameList_selection_changed ),
                      NULL );
  gtk_signal_connect( GTK_OBJECT( FNameList ),"unselect_child",
                      GTK_SIGNAL_FUNC( pl_FNameList_unselect_child ),
                      NULL );
  gtk_signal_connect( GTK_OBJECT( SelectedList ),"select_child",
                      GTK_SIGNAL_FUNC( pl_SelectedList_select_child ),
                      NULL );
  gtk_signal_connect( GTK_OBJECT( SelectedList ),"selection_changed",
                      GTK_SIGNAL_FUNC( pl_SelectedList_selection_changed ),
                      NULL );
  gtk_signal_connect( GTK_OBJECT( SelectedList ),"unselect_child",
                      GTK_SIGNAL_FUNC( pl_SelectedList_unselect_child ),
                      NULL );
  gtk_signal_connect( GTK_OBJECT( Add ),"released",
                      GTK_SIGNAL_FUNC( pl_Add_released ),
                      NULL );
  gtk_signal_connect( GTK_OBJECT( Remove ),"released",
                      GTK_SIGNAL_FUNC( pl_Remove_released ),
                      NULL );
  gtk_signal_connect( GTK_OBJECT( Ok ),"released",
                      GTK_SIGNAL_FUNC( pl_Ok_released ),
                      NULL );
  gtk_signal_connect( GTK_OBJECT( Cancel ),"released",
                      GTK_SIGNAL_FUNC( pl_Cancel_released ),
                      NULL );

  return PlayList;
}

