
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "url.h"
#include "../../interface.h"
#include "../../events.h"
#include "../widgets.h"
#include "../../help_mp.h"

GtkWidget * URL;
int         gtkVURLDialogBox = 0;

static GtkWidget * URLCombo;
static GtkWidget * URLEntry;
static GList     * URLComboEntrys = NULL;

void ShowURLDialogBox( void )
{
 if ( gtkVURLDialogBox ) gtkActive( URL );
   else URL=create_URL();
   
 if ( URLComboEntrys )
  {
   gtk_entry_set_text( GTK_ENTRY( URLEntry ),URLComboEntrys->data );
   gtk_combo_set_popdown_strings( GTK_COMBO( URLCombo ),URLComboEntrys );
  }
 
 gtk_widget_show( URL );
 gtkVURLDialogBox=1;
}

void HideURLDialogBox( void )
{
 if ( !gtkVURLDialogBox ) return;
 gtk_widget_hide( URL );
 gtk_widget_destroy( URL );
 gtkVURLDialogBox=0;
}

static gboolean on_URL_destroy_event( GtkWidget * widget,GdkEvent * event,gpointer user_data )
{
 HideURLDialogBox();
 return FALSE;
}

static void on_Button_pressed( GtkButton * button,gpointer user_data )
{ 
 if ( (int)user_data )
  {
   gchar * str= strdup( gtk_entry_get_text( GTK_ENTRY( URLEntry ) ) );

   if ( str )
    {
     if ( strncmp( str,"http://",7 ) && strncmp( str,"ftp://",6 ) && !strncmp( str,"mms://",6 ) )
      {
       gchar * tmp;
       tmp=malloc( strlen( str ) + 8 );
       sprintf( tmp,"http://%s",str );
       free( str ); str=tmp;
      }
     URLComboEntrys=g_list_prepend( URLComboEntrys,(gchar *)str );

     guiSetFilename( guiIntfStruct.Filename,str ); guiIntfStruct.FilenameChanged=1;
     mplEventHandling( evPlayNetwork,0 );
    }
  }
 HideURLDialogBox(); 
}

static void ab_URL_show( GtkButton * button,gpointer user_data )
{ gtkVURLDialogBox=(int)user_data; }

GtkWidget * create_URL( void )
{
 GtkWidget * frame1;
 GtkWidget * frame2;
 GtkWidget * frame3;
 GtkWidget * frame4;
 GtkWidget * vbox1;
 GtkWidget * hbox1;
 GtkWidget * label1;
 GtkWidget * hsep;
 GtkWidget * hbuttonbox1;
 GtkWidget * Ok;
 GtkWidget * Cancel;
 GtkAccelGroup * accel_group;

 accel_group=gtk_accel_group_new();

 URL=gtk_window_new( GTK_WINDOW_DIALOG );
 gtk_widget_set_name( URL,"URL" );
 gtk_object_set_data( GTK_OBJECT( URL ),"URL",URL );
 gtk_widget_set_usize( URL,384,70 );
 GTK_WIDGET_SET_FLAGS( URL,GTK_CAN_DEFAULT );
 gtk_window_set_title( GTK_WINDOW( URL ),MSGTR_Network );
 gtk_window_set_position( GTK_WINDOW( URL ),GTK_WIN_POS_CENTER );
 gtk_window_set_policy( GTK_WINDOW( URL ),TRUE,TRUE,FALSE );
 gtk_window_set_wmclass( GTK_WINDOW( URL ),MSGTR_Network,"MPlayer" );
 
 gtk_widget_realize( URL );
 gtkAddIcon( URL );

 frame1=gtk_frame_new( NULL );
 gtk_widget_set_name( frame1,"frame1" );
 gtk_widget_ref( frame1 );
 gtk_object_set_data_full( GTK_OBJECT( URL ),"frame1",frame1,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( frame1 );
 gtk_container_add( GTK_CONTAINER( URL ),frame1 );
 gtk_container_set_border_width( GTK_CONTAINER( frame1 ),1 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame1 ),GTK_SHADOW_IN );

 frame2=gtk_frame_new( NULL );
 gtk_widget_set_name( frame2,"frame2" );
 gtk_widget_ref( frame2 );
 gtk_object_set_data_full( GTK_OBJECT( URL ),"frame2",frame2,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( frame2 );
 gtk_container_add( GTK_CONTAINER( frame1 ),frame2 );
 gtk_container_set_border_width( GTK_CONTAINER( frame2 ),1 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame2 ),GTK_SHADOW_NONE );

 frame3=gtk_frame_new( NULL );
 gtk_widget_set_name( frame3,"frame3" );
 gtk_widget_ref( frame3 );
 gtk_object_set_data_full( GTK_OBJECT( URL ),"frame3",frame3,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( frame3 );
 gtk_container_add( GTK_CONTAINER( frame2 ),frame3 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame3 ),GTK_SHADOW_ETCHED_OUT );

 frame4=gtk_frame_new( NULL );
 gtk_widget_set_name( frame4,"frame4" );
 gtk_widget_ref( frame4 );
 gtk_object_set_data_full( GTK_OBJECT( URL ),"frame4",frame4,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( frame4 );
 gtk_container_add( GTK_CONTAINER( frame3 ),frame4 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame4 ),GTK_SHADOW_NONE );

 vbox1=gtk_vbox_new( FALSE,0 );
 gtk_widget_set_name( vbox1,"vbox1" );
 gtk_widget_ref( vbox1 );
 gtk_object_set_data_full( GTK_OBJECT( URL ),"vbox1",vbox1,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( vbox1 );
 gtk_container_add( GTK_CONTAINER( frame4 ),vbox1 );

 hbox1=gtk_hbox_new( FALSE,0 );
 gtk_widget_set_name( hbox1,"hbox1" );
 gtk_widget_ref( hbox1 );
 gtk_object_set_data_full( GTK_OBJECT( URL ),"hbox1",hbox1,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( hbox1 );
 gtk_box_pack_start( GTK_BOX( vbox1 ),hbox1,TRUE,TRUE,0 );

 label1=gtk_label_new( "URL: " );
 gtk_widget_set_name( label1,"label1" );
 gtk_widget_ref( label1 );
 gtk_object_set_data_full( GTK_OBJECT( URL ),"label1",label1,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( label1 );
 gtk_box_pack_start( GTK_BOX( hbox1 ),label1,FALSE,FALSE,0 );
 gtk_widget_set_usize( label1,38,-2 );
 gtk_label_set_justify( GTK_LABEL( label1 ),GTK_JUSTIFY_FILL );
 gtk_misc_set_alignment( GTK_MISC( label1 ),0.5,0.49 );

 URLCombo=gtk_combo_new();
 gtk_widget_set_name( URLCombo,"URLCombo" );
 gtk_widget_ref( URLCombo );
 gtk_object_set_data_full( GTK_OBJECT( URL ),"URLCombo",URLCombo,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( URLCombo );
 gtk_box_pack_start( GTK_BOX( hbox1 ),URLCombo,TRUE,TRUE,0 );

 URLEntry=GTK_COMBO( URLCombo )->entry;
 gtk_widget_set_name( URLEntry,"URLEntry" );
 gtk_widget_ref( URLEntry );
 gtk_object_set_data_full( GTK_OBJECT( URL ),"URLEntry",URLEntry,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( URLEntry );

 hsep=gtk_hseparator_new();
 gtk_widget_set_name( hsep,"hsep" );
 gtk_widget_ref( hsep );
 gtk_object_set_data_full( GTK_OBJECT( URL ),"hsep",hsep,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( hsep );
 gtk_box_pack_start( GTK_BOX( vbox1 ),hsep,FALSE,TRUE,0 );
 gtk_widget_set_usize( hsep,-2,8 );

 hbuttonbox1=gtk_hbutton_box_new();
 gtk_widget_set_name( hbuttonbox1,"hbuttonbox1" );
 gtk_widget_ref( hbuttonbox1 );
 gtk_object_set_data_full( GTK_OBJECT( URL ),"hbuttonbox1",hbuttonbox1,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( hbuttonbox1 );
 gtk_box_pack_start( GTK_BOX( vbox1 ),hbuttonbox1,FALSE,FALSE,0 );
 gtk_button_box_set_layout( GTK_BUTTON_BOX( hbuttonbox1 ),GTK_BUTTONBOX_END );
 gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox1 ),10 );
 gtk_button_box_set_child_size( GTK_BUTTON_BOX( hbuttonbox1 ),85,20 );
 gtk_button_box_set_child_ipadding( GTK_BUTTON_BOX( hbuttonbox1 ),0,0 );

 Ok=gtk_button_new_with_label( MSGTR_Ok );
 gtk_widget_set_name( Ok,"Ok" );
 gtk_widget_ref( Ok );
 gtk_object_set_data_full( GTK_OBJECT( URL ),"Ok",Ok,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( Ok );
 gtk_container_add( GTK_CONTAINER( hbuttonbox1 ),Ok );

 Cancel=gtk_button_new_with_label( MSGTR_Cancel );
 gtk_widget_set_name( Cancel,"Cancel" );
 gtk_widget_ref( Cancel );
 gtk_object_set_data_full( GTK_OBJECT( URL ),"Cancel",Cancel,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( Cancel );
 gtk_container_add( GTK_CONTAINER( hbuttonbox1 ),Cancel );
 
 gtk_widget_add_accelerator( Ok,"pressed",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE );
// gtk_widget_add_accelerator( Ok,"pressed",accel_group,GDK_O,GDK_MOD1_MASK,GTK_ACCEL_VISIBLE );
// gtk_widget_add_accelerator( Ok,"pressed",accel_group,GDK_o,GDK_MOD1_MASK,GTK_ACCEL_VISIBLE );
 gtk_widget_add_accelerator( Cancel,"pressed",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );
// gtk_widget_add_accelerator( Cancel,"pressed",accel_group,GDK_C,GDK_MOD1_MASK,GTK_ACCEL_VISIBLE );
// gtk_widget_add_accelerator( Cancel,"pressed",accel_group,GDK_c,GDK_MOD1_MASK,GTK_ACCEL_VISIBLE );

 gtk_signal_connect( GTK_OBJECT( URL ),"destroy",GTK_SIGNAL_FUNC( on_URL_destroy_event ),NULL );
 gtk_signal_connect( GTK_OBJECT( URL ),"show",GTK_SIGNAL_FUNC( ab_URL_show ),(void *)1 );
 gtk_signal_connect( GTK_OBJECT( URL ),"hide",GTK_SIGNAL_FUNC( ab_URL_show ),0 );

 gtk_signal_connect( GTK_OBJECT( Ok ),"pressed",GTK_SIGNAL_FUNC( on_Button_pressed ),(void *)1 );
 gtk_signal_connect( GTK_OBJECT( Cancel ),"pressed",GTK_SIGNAL_FUNC( on_Button_pressed ),NULL );

 gtk_widget_grab_focus( URLEntry );
 gtk_window_add_accel_group( GTK_WINDOW( URL ),accel_group );

 return URL;
}

