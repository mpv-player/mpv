
#include <stdlib.h>
#include <stdio.h>

#include "../mplayer.h"

#include "../../events.h"
#include "../../../config.h"
#include "../../../help_mp.h"

#include "../widgets.h"
#include "opts.h"

GtkWidget * opAudioFrameLabel;
GtkWidget * opAudioLabel;
GtkWidget * opAudioDriverMenu;
GtkWidget * opDelayLabel;
GtkWidget * opAudioDelaySkinButton;
GtkWidget * opAudioConfigureButton;
GtkWidget * OpVideoFrameLabel;
GtkWidget * opVideoLabel;
GtkWidget * opVideoMenu;
GtkWidget * opOsdCheckButton;
GtkWidget * opVideoConfButton;
GtkWidget * opOSDLabel;
GtkWidget * opOSDLevelSpinButton;
GtkWidget * opAutoFullscreenCheckBox;
GtkWidget * opOk;
GtkWidget * opCancel;

int opShift = 0;

void HideOptions( void )
{ gtk_widget_destroy( Options ); }

gboolean on_window2_key_press_event( GtkWidget * widget,GdkEventKey * event,gpointer user_data )
{
 switch ( event->keyval )
  {
   case GDK_Shift_L:
   case GDK_Shift_R:
        opShift=1;
        break;
  }
 return 0;
}

gboolean on_window2_key_release_event( GtkWidget * widget,GdkEventKey * event,gpointer user_data )
{
 switch ( event->keyval )
  {
   case GDK_Escape:
   case GDK_Return:
        if ( !opShift ) HideOptions();
        break;
   case GDK_Tab:
//        if ( sbShift )
//         { if ( (--sbItemsListCounter) < 0 ) sbItemsListCounter=2; }
//         else
//          { if ( (++sbItemsListCounter) > 2 ) sbItemsListCounter=0; }
//        gtk_widget_grab_focus( sbItemsList[sbItemsListCounter] );
        break;
   case GDK_Shift_L:
   case GDK_Shift_R:
        opShift=0;
        break;
  }
 return FALSE;
}

gboolean on_window2_destroy_event( GtkWidget * widget,GdkEvent * event,gpointer user_data)
{
  HideOptions();
  return FALSE;
}

void on_opAudioDriverMenu_released( GtkButton * button,gpointer user_data )
{
}

void on_opAudioDelaySkinButton_changed( GtkEditable * editable,gpointer user_data )
{
}

void on_opAudioDelaySkinButton_move_to_column( GtkEditable * editable,gint column,gpointer user_data )
{
}

void on_opAudioDelaySkinButton_move_to_row( GtkEditable * editable,gint row,gpointer user_data )
{
}

void on_opAudioConfigureButton_released( GtkButton * button,gpointer user_data )
{
}

void on_opVideoMenu_released( GtkButton * button,gpointer user_data )
{
 fprintf( stderr,"[opts] data: %s\n",(char *)user_data );
}

void on_opVideoMenu_pressed( GtkButton * button,gpointer user_data )
{
 fprintf( stderr,"[opts] data: %s\n",(char *)user_data );
}

void on_opVideoMenu_clicked( GtkButton * button,gpointer user_data)
{
 fprintf( stderr,"[opts] data(2): %s\n",(char *)user_data );
}

gboolean on_opVideoMenu_button_release_event( GtkWidget * widget,GdkEventButton * event,gpointer user_data )
{
 fprintf( stderr,"[opts] video menu.\n" );
 return FALSE;
}

void on_opOsdCheckButton_toggled( GtkToggleButton * togglebutton,gpointer user_data )
{
}

void on_opVideoConfButton_released( GtkButton * button,gpointer user_data )
{
}

void on_opOSDLevelSpinButton_changed( GtkEditable * editable,gpointer user_data )
{
}

void on_opOSDLevelSpinButton_move_to_column( GtkEditable * editable,gint column,gpointer user_data )
{
}

void on_opOSDLevelSpinButton_move_to_row( GtkEditable * editable,gint row,gpointer user_data )
{
}

void on_opOk_released( GtkButton * button,gpointer user_data )
{ HideOptions(); }

void on_opCancel_released( GtkButton * button,gpointer user_data )
{ HideOptions(); }

gboolean on_confOSS_destroy_event    ( GtkWidget * widget,GdkEvent * event,gpointer user_data)
{
  return FALSE;
}

gboolean on_confOSS_key_press_event  ( GtkWidget * widget,GdkEventKey * event,gpointer user_data )
{
  return FALSE;
}

gboolean on_confOSS_key_release_event( GtkWidget * widget,GdkEventKey * event,gpointer user_data )
{
  return FALSE;
}

void on_opOSSDSPCombo_set_focus_child( GtkContainer * container,GtkWidget * widget,gpointer user_data )
{
}

void on_opOSSDSPComboEntry_changed( GtkEditable * editable,gpointer user_data )
{
}

void on_opOSSMixerCombo_set_focus_child( GtkContainer * container,GtkWidget * widget,gpointer user_data )
{
}

void on_opOSSMixerComboEntry_changed( GtkEditable * editable,gpointer user_data )
{
}

void on_opOSSOk_released( GtkButton * button,gpointer user_data )
{
}

void on_opOSSCancel_released( GtkButton * button,gpointer user_data )
{
}

void on_opAutoFullscreenCheckBox_toggled( GtkToggleButton *togglebutton,gpointer user_data )
{
}

GtkWidget * create_Options( void )
{
 GtkWidget *frame1;
 GtkWidget *frame2;
 GtkWidget *frame3;
 GtkWidget *frame4;
 GtkWidget *vbox1;
 GtkWidget *notebook1;
 GtkWidget *frame5;
 GtkWidget *frame6;
 GtkWidget *hbox1;
 GtkWidget *table1;
 GtkWidget *opAudioDriverMenu_menu;
 GtkWidget *glade_menuitem;
 GtkObject *opAudioDelaySkinButton_adj;
 GtkWidget *hbuttonbox2;
 GtkWidget *frame16;
 GtkWidget *table2;
 GtkWidget *opVideoMenu_menu;
 GtkWidget *hbuttonbox3;
 GtkWidget *label5;
 GtkObject *opOSDLevelSpinButton_adj;
 GtkWidget *frame17;
 GtkWidget *opAudio;
 GtkWidget *frame11;
 GtkWidget *frame12;
 GtkWidget *hbox2;
 GtkWidget *frame13;
 GtkWidget *table4;
 GtkWidget *vseparator1;
 GtkWidget *frame14;
 GtkWidget *opMisc;
 GtkWidget *hseparator1;
 GtkWidget *hbuttonbox1;
 GtkTooltips *tooltips;

 tooltips = gtk_tooltips_new ();

 Options = gtk_window_new (GTK_WINDOW_TOPLEVEL);
 gtk_widget_set_name (Options, "Options");
 gtk_object_set_data (GTK_OBJECT (Options), "Options", Options);
 gtk_widget_set_usize (Options, 448, 260);
 gtk_widget_set_events (Options, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_STRUCTURE_MASK | GDK_VISIBILITY_NOTIFY_MASK);
 gtk_window_set_title (GTK_WINDOW (Options),"Options");
 gtk_window_set_policy (GTK_WINDOW (Options), FALSE, FALSE, FALSE);
 gtk_window_set_wmclass (GTK_WINDOW (Options), "Options", "Options");

 frame1 = gtk_frame_new (NULL);
 gtk_widget_set_name (frame1, "frame1");
 gtk_widget_ref (frame1);
 gtk_object_set_data_full (GTK_OBJECT (Options), "frame1", frame1,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (frame1);
 gtk_container_add (GTK_CONTAINER (Options), frame1);
 gtk_container_set_border_width (GTK_CONTAINER (frame1), 1);
 gtk_frame_set_shadow_type (GTK_FRAME (frame1), GTK_SHADOW_IN);

 frame2 = gtk_frame_new (NULL);
 gtk_widget_set_name (frame2, "frame2");
 gtk_widget_ref (frame2);
 gtk_object_set_data_full (GTK_OBJECT (Options), "frame2", frame2,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (frame2);
 gtk_container_add (GTK_CONTAINER (frame1), frame2);
 gtk_frame_set_shadow_type (GTK_FRAME (frame2), GTK_SHADOW_NONE);

 frame3 = gtk_frame_new (NULL);
 gtk_widget_set_name (frame3, "frame3");
 gtk_widget_ref (frame3);
 gtk_object_set_data_full (GTK_OBJECT (Options), "frame3", frame3,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (frame3);
 gtk_container_add (GTK_CONTAINER (frame2), frame3);
 gtk_frame_set_shadow_type (GTK_FRAME (frame3), GTK_SHADOW_ETCHED_OUT);

 frame4 = gtk_frame_new (NULL);
 gtk_widget_set_name (frame4, "frame4");
 gtk_widget_ref (frame4);
 gtk_object_set_data_full (GTK_OBJECT (Options), "frame4", frame4,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (frame4);
 gtk_container_add (GTK_CONTAINER (frame3), frame4);
 gtk_frame_set_shadow_type (GTK_FRAME (frame4), GTK_SHADOW_NONE);

 vbox1 = gtk_vbox_new (FALSE, 0);
 gtk_widget_set_name (vbox1, "vbox1");
 gtk_widget_ref (vbox1);
 gtk_object_set_data_full (GTK_OBJECT (Options), "vbox1", vbox1,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (vbox1);
 gtk_container_add (GTK_CONTAINER (frame4), vbox1);

 notebook1 = gtk_notebook_new ();
 gtk_widget_set_name (notebook1, "notebook1");
 gtk_widget_ref (notebook1);
 gtk_object_set_data_full (GTK_OBJECT (Options), "notebook1", notebook1,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (notebook1);
 gtk_box_pack_start (GTK_BOX (vbox1), notebook1, TRUE, TRUE, 0);

 frame5 = gtk_frame_new (NULL);
 gtk_widget_set_name (frame5, "frame5");
 gtk_widget_ref (frame5);
 gtk_object_set_data_full (GTK_OBJECT (Options), "frame5", frame5,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (frame5);
 gtk_container_add (GTK_CONTAINER (notebook1), frame5);

 frame6 = gtk_frame_new (NULL);
 gtk_widget_set_name (frame6, "frame6");
 gtk_widget_ref (frame6);
 gtk_object_set_data_full (GTK_OBJECT (Options), "frame6", frame6,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (frame6);
 gtk_container_add (GTK_CONTAINER (frame5), frame6);
 gtk_frame_set_shadow_type (GTK_FRAME (frame6), GTK_SHADOW_NONE);

 hbox1 = gtk_hbox_new (TRUE, 0);
 gtk_widget_set_name (hbox1, "hbox1");
 gtk_widget_ref (hbox1);
 gtk_object_set_data_full (GTK_OBJECT (Options), "hbox1", hbox1,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (hbox1);
 gtk_container_add (GTK_CONTAINER (frame6), hbox1);

 opAudioFrameLabel = gtk_frame_new ("Audio");
 gtk_widget_set_name (opAudioFrameLabel, "opAudioFrameLabel");
 gtk_widget_ref (opAudioFrameLabel);
 gtk_object_set_data_full (GTK_OBJECT (Options), "opAudioFrameLabel", opAudioFrameLabel,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (opAudioFrameLabel);
 gtk_box_pack_start (GTK_BOX (hbox1), opAudioFrameLabel, FALSE, FALSE, 0);
 gtk_widget_set_usize (opAudioFrameLabel, 212, -2);
 gtk_container_set_border_width (GTK_CONTAINER (opAudioFrameLabel), 3);
 gtk_frame_set_shadow_type (GTK_FRAME (opAudioFrameLabel), GTK_SHADOW_ETCHED_OUT);

 table1 = gtk_table_new (4, 2, FALSE);
 gtk_widget_set_name (table1, "table1");
 gtk_widget_ref (table1);
 gtk_object_set_data_full (GTK_OBJECT (Options), "table1", table1,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (table1);
 gtk_container_add (GTK_CONTAINER (opAudioFrameLabel), table1);

 opAudioLabel = gtk_label_new ("Driver: ");
 gtk_widget_set_name (opAudioLabel, "opAudioLabel");
 gtk_widget_ref (opAudioLabel);
 gtk_object_set_data_full (GTK_OBJECT (Options), "opAudioLabel", opAudioLabel,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (opAudioLabel);
 gtk_table_attach (GTK_TABLE (table1), opAudioLabel, 0, 1, 0, 1,
                   (GtkAttachOptions) (GTK_FILL),
                   (GtkAttachOptions) (0), 0, 0);
 gtk_widget_set_usize (opAudioLabel, 40, -2);
 gtk_misc_set_alignment (GTK_MISC (opAudioLabel), 0, 0.5);

 opAudioDriverMenu = gtk_option_menu_new ();
 gtk_widget_set_name (opAudioDriverMenu, "opAudioDriverMenu");
 gtk_widget_ref (opAudioDriverMenu);
 gtk_object_set_data_full (GTK_OBJECT (Options), "opAudioDriverMenu", opAudioDriverMenu,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (opAudioDriverMenu);
 gtk_table_attach (GTK_TABLE (table1), opAudioDriverMenu, 1, 2, 0, 1,
                   (GtkAttachOptions) (GTK_FILL),
                   (GtkAttachOptions) (0), 0, 0);
 gtk_widget_set_usize (opAudioDriverMenu, 159, 25);
 gtk_tooltips_set_tip (tooltips, opAudioDriverMenu, "Select audio output driver.", NULL);
 opAudioDriverMenu_menu = gtk_menu_new ();
 glade_menuitem = gtk_menu_item_new_with_label ("null");
 gtk_widget_show (glade_menuitem);
 gtk_menu_append (GTK_MENU (opAudioDriverMenu_menu), glade_menuitem);

 #ifdef USE_OSS_AUDIO
  glade_menuitem = gtk_menu_item_new_with_label ("OSS");
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (opAudioDriverMenu_menu), glade_menuitem);
 #endif
 #ifdef HAVE_ALSA5
  glade_menuitem = gtk_menu_item_new_with_label ("ALSA 0.5.x");
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (opAudioDriverMenu_menu), glade_menuitem);
 #endif
 #ifdef HAVE_ALSA9
  glade_menuitem = gtk_menu_item_new_with_label ("ALSA 0.9.x");
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (opAudioDriverMenu_menu), glade_menuitem);
 #endif
 #ifdef HAVE_ESD
  glade_menuitem = gtk_menu_item_new_with_label ("ESD");
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (opAudioDriverMenu_menu), glade_menuitem);
 #endif

 gtk_option_menu_set_menu (GTK_OPTION_MENU (opAudioDriverMenu), opAudioDriverMenu_menu);

 opDelayLabel = gtk_label_new ("Delay:");
 gtk_widget_set_name (opDelayLabel, "opDelayLabel");
 gtk_widget_ref (opDelayLabel);
 gtk_object_set_data_full (GTK_OBJECT (Options), "opDelayLabel", opDelayLabel,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (opDelayLabel);
 gtk_table_attach (GTK_TABLE (table1), opDelayLabel, 0, 1, 1, 2,
                   (GtkAttachOptions) (GTK_FILL),
                   (GtkAttachOptions) (0), 0, 0);
 gtk_widget_set_usize (opDelayLabel, 35, -2);
 gtk_misc_set_alignment (GTK_MISC (opDelayLabel), 0, 0.5);

 opAudioDelaySkinButton_adj = gtk_adjustment_new (0, -500, 500, 0.01, 10, 10);
 opAudioDelaySkinButton = gtk_spin_button_new (GTK_ADJUSTMENT (opAudioDelaySkinButton_adj), 1, 2);
 gtk_widget_set_name (opAudioDelaySkinButton, "opAudioDelaySkinButton");
 gtk_widget_ref (opAudioDelaySkinButton);
 gtk_object_set_data_full (GTK_OBJECT (Options), "opAudioDelaySkinButton", opAudioDelaySkinButton,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (opAudioDelaySkinButton);
 gtk_table_attach (GTK_TABLE (table1), opAudioDelaySkinButton, 1, 2, 1, 2,
                   (GtkAttachOptions) (GTK_FILL), //GTK_EXPAND |
                   (GtkAttachOptions) (0), 0, 0);
 gtk_widget_set_usize (opAudioDelaySkinButton, 160, 25);
 gtk_tooltips_set_tip (tooltips, opAudioDelaySkinButton, "Set audio delay.", NULL);
 gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (opAudioDelaySkinButton), TRUE);

 hbuttonbox2 = gtk_hbutton_box_new ();
 gtk_widget_set_name (hbuttonbox2, "hbuttonbox2");
 gtk_widget_ref (hbuttonbox2);
 gtk_object_set_data_full (GTK_OBJECT (Options), "hbuttonbox2", hbuttonbox2,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (hbuttonbox2);
 gtk_table_attach (GTK_TABLE (table1), hbuttonbox2, 1, 2, 3, 4,
                   (GtkAttachOptions) (GTK_FILL),
                   (GtkAttachOptions) (GTK_FILL), 0, 0);
 gtk_widget_set_usize (hbuttonbox2, -2, 31);
 gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox2), GTK_BUTTONBOX_END);

 opAudioConfigureButton = gtk_button_new_with_label ("Configure");
 gtk_widget_set_name (opAudioConfigureButton, "opAudioConfigureButton");
 gtk_widget_ref (opAudioConfigureButton);
 gtk_object_set_data_full (GTK_OBJECT (Options), "opAudioConfigureButton", opAudioConfigureButton,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (opAudioConfigureButton);
 gtk_container_add (GTK_CONTAINER (hbuttonbox2), opAudioConfigureButton);
 gtk_widget_set_usize (opAudioConfigureButton, -2, 31);
 GTK_WIDGET_SET_FLAGS (opAudioConfigureButton, GTK_CAN_DEFAULT);
 gtk_tooltips_set_tip (tooltips, opAudioConfigureButton, "Configure selected audio driver.", NULL);

 frame16 = gtk_frame_new (NULL);
 gtk_widget_set_name (frame16, "frame16");
 gtk_widget_ref (frame16);
 gtk_object_set_data_full (GTK_OBJECT (Options), "frame16", frame16,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (frame16);
 gtk_table_attach (GTK_TABLE (table1), frame16, 1, 2, 2, 3,
                   (GtkAttachOptions) (GTK_FILL),
                   (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
 gtk_frame_set_shadow_type (GTK_FRAME (frame16), GTK_SHADOW_NONE);

 OpVideoFrameLabel = gtk_frame_new ("Video");
 gtk_widget_set_name (OpVideoFrameLabel, "OpVideoFrameLabel");
 gtk_widget_ref (OpVideoFrameLabel);
 gtk_object_set_data_full (GTK_OBJECT (Options), "OpVideoFrameLabel", OpVideoFrameLabel,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (OpVideoFrameLabel);
 gtk_box_pack_start (GTK_BOX (hbox1), OpVideoFrameLabel, FALSE, FALSE, 0);
 gtk_widget_set_usize (OpVideoFrameLabel, 212, -2);
 gtk_container_set_border_width (GTK_CONTAINER (OpVideoFrameLabel), 3);
 gtk_frame_set_shadow_type (GTK_FRAME (OpVideoFrameLabel), GTK_SHADOW_ETCHED_OUT);

 table2 = gtk_table_new (5, 2, FALSE);
 gtk_widget_set_name (table2, "table2");
 gtk_widget_ref (table2);
 gtk_object_set_data_full (GTK_OBJECT (Options), "table2", table2,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (table2);
 gtk_container_add (GTK_CONTAINER (OpVideoFrameLabel), table2);

 opVideoLabel = gtk_label_new ("Driver:");
 gtk_widget_set_name (opVideoLabel, "opVideoLabel");
 gtk_widget_ref (opVideoLabel);
 gtk_object_set_data_full (GTK_OBJECT (Options), "opVideoLabel", opVideoLabel,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (opVideoLabel);
 gtk_table_attach (GTK_TABLE (table2), opVideoLabel, 0, 1, 0, 1,
                   (GtkAttachOptions) (GTK_FILL),
                   (GtkAttachOptions) (0), 0, 0);
 gtk_widget_set_usize (opVideoLabel, 35, -2);
 gtk_misc_set_alignment (GTK_MISC (opVideoLabel), 0, 0.5);

 opVideoMenu = gtk_option_menu_new ();
 gtk_widget_set_name (opVideoMenu, "opVideoMenu");
 gtk_widget_ref (opVideoMenu);
 gtk_object_set_data_full (GTK_OBJECT (Options), "opVideoMenu", opVideoMenu,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (opVideoMenu);
 gtk_table_attach (GTK_TABLE (table2), opVideoMenu, 1, 2, 0, 1,
                   (GtkAttachOptions) (GTK_FILL),
                   (GtkAttachOptions) (0), 0, 0);
 gtk_widget_set_usize (opVideoMenu, 137, 25);
 gtk_tooltips_set_tip (tooltips, opVideoMenu, "Select video output driver.", NULL);
 opVideoMenu_menu = gtk_menu_new ();
 #if defined( HAVE_X11 ) && defined( HAVE_MGA )
  glade_menuitem = gtk_menu_item_new_with_label ("xmga");
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (opVideoMenu_menu), glade_menuitem);
 #endif
 #ifdef HAVE_XV
  glade_menuitem = gtk_menu_item_new_with_label ("xv");
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (opVideoMenu_menu), glade_menuitem);
 #endif
 #ifdef HAVE_X11
  glade_menuitem = gtk_menu_item_new_with_label ("x11");
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (opVideoMenu_menu), glade_menuitem);
 #endif
 #ifdef HAVE_PNG
  glade_menuitem = gtk_menu_item_new_with_label ("png");
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (opVideoMenu_menu), glade_menuitem);
 #endif
 glade_menuitem = gtk_menu_item_new_with_label ("null");
 gtk_widget_show (glade_menuitem);
 gtk_menu_append (GTK_MENU (opVideoMenu_menu), glade_menuitem);
 gtk_option_menu_set_menu (GTK_OPTION_MENU (opVideoMenu), opVideoMenu_menu);

 opOsdCheckButton = gtk_check_button_new_with_label ("");
 gtk_widget_set_name (opOsdCheckButton, "opOsdCheckButton");
 gtk_widget_ref (opOsdCheckButton);
 gtk_object_set_data_full (GTK_OBJECT (Options), "opOsdCheckButton", opOsdCheckButton,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (opOsdCheckButton);
 gtk_table_attach (GTK_TABLE (table2), opOsdCheckButton, 1, 2, 1, 2,
                   (GtkAttachOptions) (GTK_FILL),
                   (GtkAttachOptions) (0), 0, 0);
 gtk_widget_set_usize (opOsdCheckButton, -2, 24);
 gtk_tooltips_set_tip (tooltips, opOsdCheckButton, "On/off OSD.", NULL);

 hbuttonbox3 = gtk_hbutton_box_new ();
 gtk_widget_set_name (hbuttonbox3, "hbuttonbox3");
 gtk_widget_ref (hbuttonbox3);
 gtk_object_set_data_full (GTK_OBJECT (Options), "hbuttonbox3", hbuttonbox3,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (hbuttonbox3);
 gtk_table_attach (GTK_TABLE (table2), hbuttonbox3, 1, 2, 4, 5,
                   (GtkAttachOptions) (GTK_FILL),
                   (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
 gtk_widget_set_usize (hbuttonbox3, -2, 31);
 gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox3), GTK_BUTTONBOX_END);

 opVideoConfButton = gtk_button_new_with_label ("Configure");
 gtk_widget_set_name (opVideoConfButton, "opVideoConfButton");
 gtk_widget_ref (opVideoConfButton);
 gtk_object_set_data_full (GTK_OBJECT (Options), "opVideoConfButton", opVideoConfButton,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (opVideoConfButton);
 gtk_container_add (GTK_CONTAINER (hbuttonbox3), opVideoConfButton);
 gtk_widget_set_usize (opVideoConfButton, -2, 31);
 GTK_WIDGET_SET_FLAGS (opVideoConfButton, GTK_CAN_DEFAULT);
 gtk_tooltips_set_tip (tooltips, opVideoConfButton, "Configure selected video driver.", NULL);

 opOSDLabel = gtk_label_new ("OSD:");
 gtk_widget_set_name (opOSDLabel, "opOSDLabel");
 gtk_widget_ref (opOSDLabel);
 gtk_object_set_data_full (GTK_OBJECT (Options), "opOSDLabel", opOSDLabel,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (opOSDLabel);
 gtk_table_attach (GTK_TABLE (table2), opOSDLabel, 0, 1, 1, 2,
                   (GtkAttachOptions) (GTK_FILL),
                   (GtkAttachOptions) (0), 0, 0);
 gtk_widget_set_usize (opOSDLabel, 35, -2);
 gtk_misc_set_alignment (GTK_MISC (opOSDLabel), 0, 0.5);

 label5 = gtk_label_new ("OSD level:");
 gtk_widget_set_name (label5, "label5");
 gtk_widget_ref (label5);
 gtk_object_set_data_full (GTK_OBJECT (Options), "label5", label5,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (label5);
 gtk_table_attach (GTK_TABLE (table2), label5, 0, 1, 2, 3,
                   (GtkAttachOptions) (GTK_FILL),
                   (GtkAttachOptions) (0), 0, 0);
 gtk_widget_set_usize (label5, 63, -2);
 gtk_misc_set_alignment (GTK_MISC (label5), 0, 0.5);

 opOSDLevelSpinButton_adj = gtk_adjustment_new (0, 0, 2, 1, 10, 10);
 opOSDLevelSpinButton = gtk_spin_button_new (GTK_ADJUSTMENT (opOSDLevelSpinButton_adj), 1, 0);
 gtk_widget_set_name (opOSDLevelSpinButton, "opOSDLevelSpinButton");
 gtk_widget_ref (opOSDLevelSpinButton);
 gtk_object_set_data_full (GTK_OBJECT (Options), "opOSDLevelSpinButton", opOSDLevelSpinButton,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (opOSDLevelSpinButton);
 gtk_table_attach (GTK_TABLE (table2), opOSDLevelSpinButton, 1, 2, 2, 3,
                   (GtkAttachOptions) (GTK_FILL),
                   (GtkAttachOptions) (0), 0, 0);
 gtk_widget_set_usize (opOSDLevelSpinButton, 136, 25);
 gtk_tooltips_set_tip (tooltips, opOSDLevelSpinButton, "Set OSD level.", NULL);
 gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (opOSDLevelSpinButton), TRUE);

 frame17 = gtk_frame_new (NULL);
 gtk_widget_set_name (frame17, "frame17");
 gtk_widget_ref (frame17);
 gtk_object_set_data_full (GTK_OBJECT (Options), "frame17", frame17,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (frame17);
 gtk_table_attach (GTK_TABLE (table2), frame17, 1, 2, 3, 4,
                   (GtkAttachOptions) (GTK_FILL),
                   (GtkAttachOptions) (GTK_FILL), 0, 0);
 gtk_widget_set_usize (frame17, -2, 40);
 gtk_frame_set_shadow_type (GTK_FRAME (frame17), GTK_SHADOW_NONE);

 opAudio = gtk_label_new ("Audio & Video");
 gtk_widget_set_name (opAudio, "opAudio");
 gtk_widget_ref (opAudio);
 gtk_object_set_data_full (GTK_OBJECT (Options), "opAudio", opAudio,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (opAudio);
 gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 0), opAudio);
 gtk_widget_set_usize (opAudio, 80, -2);

 frame11 = gtk_frame_new (NULL);
 gtk_widget_set_name (frame11, "frame11");
 gtk_widget_ref (frame11);
 gtk_object_set_data_full (GTK_OBJECT (Options), "frame11", frame11,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (frame11);
 gtk_container_add (GTK_CONTAINER (notebook1), frame11);

 frame12 = gtk_frame_new (NULL);
 gtk_widget_set_name (frame12, "frame12");
 gtk_widget_ref (frame12);
 gtk_object_set_data_full (GTK_OBJECT (Options), "frame12", frame12,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (frame12);
 gtk_container_add (GTK_CONTAINER (frame11), frame12);
 gtk_frame_set_shadow_type (GTK_FRAME (frame12), GTK_SHADOW_NONE);

 hbox2 = gtk_hbox_new (FALSE, 0);
 gtk_widget_set_name (hbox2, "hbox2");
 gtk_widget_ref (hbox2);
 gtk_object_set_data_full (GTK_OBJECT (Options), "hbox2", hbox2,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (hbox2);
 gtk_container_add (GTK_CONTAINER (frame12), hbox2);

 frame13 = gtk_frame_new (NULL);
 gtk_widget_set_name (frame13, "frame13");
 gtk_widget_ref (frame13);
 gtk_object_set_data_full (GTK_OBJECT (Options), "frame13", frame13,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (frame13);
 gtk_box_pack_start (GTK_BOX (hbox2), frame13, TRUE, TRUE, 0);
 gtk_widget_set_usize (frame13, 212, -2);
 gtk_frame_set_shadow_type (GTK_FRAME (frame13), GTK_SHADOW_NONE);

 table4 = gtk_table_new (2, 1, FALSE);
 gtk_widget_set_name (table4, "table4");
 gtk_widget_ref (table4);
 gtk_object_set_data_full (GTK_OBJECT (Options), "table4", table4,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (table4);
 gtk_container_add (GTK_CONTAINER (frame13), table4);

 opAutoFullscreenCheckBox = gtk_check_button_new_with_label ("Always switch fullscreen on play");
 gtk_widget_set_name (opAutoFullscreenCheckBox, "opAutoFullscreenCheckBox");
 gtk_widget_ref (opAutoFullscreenCheckBox);
 gtk_object_set_data_full (GTK_OBJECT (Options), "opAutoFullscreenCheckBox", opAutoFullscreenCheckBox,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (opAutoFullscreenCheckBox);
 gtk_table_attach (GTK_TABLE (table4), opAutoFullscreenCheckBox, 0, 1, 0, 1,
                   (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                   (GtkAttachOptions) (0), 0, 0);
 gtk_widget_set_usize (opAutoFullscreenCheckBox, -2, 25);
 gtk_tooltips_set_tip (tooltips, opAutoFullscreenCheckBox, "Switch player window to fullscreen on all play.", NULL);

 vseparator1 = gtk_vseparator_new ();
 gtk_widget_set_name (vseparator1, "vseparator1");
 gtk_widget_ref (vseparator1);
 gtk_object_set_data_full (GTK_OBJECT (Options), "vseparator1", vseparator1,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (vseparator1);
 gtk_box_pack_start (GTK_BOX (hbox2), vseparator1, FALSE, FALSE, 0);
 gtk_widget_set_usize (vseparator1, 3, -2);

 frame14 = gtk_frame_new (NULL);
 gtk_widget_set_name (frame14, "frame14");
 gtk_widget_ref (frame14);
 gtk_object_set_data_full (GTK_OBJECT (Options), "frame14", frame14,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (frame14);
 gtk_box_pack_start (GTK_BOX (hbox2), frame14, TRUE, TRUE, 0);
 gtk_widget_set_usize (frame14, 212, -2);
 gtk_frame_set_shadow_type (GTK_FRAME (frame14), GTK_SHADOW_NONE);

 opMisc = gtk_label_new ("Misc");
 gtk_widget_set_name (opMisc, "opMisc");
 gtk_widget_ref (opMisc);
 gtk_object_set_data_full (GTK_OBJECT (Options), "opMisc", opMisc,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (opMisc);
 gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 1), opMisc);
 gtk_widget_set_usize (opMisc, 75, -2);

 hseparator1 = gtk_hseparator_new ();
 gtk_widget_set_name (hseparator1, "hseparator1");
 gtk_widget_ref (hseparator1);
 gtk_object_set_data_full (GTK_OBJECT (Options), "hseparator1", hseparator1,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (hseparator1);
 gtk_box_pack_start (GTK_BOX (vbox1), hseparator1, FALSE, FALSE, 0);
 gtk_widget_set_usize (hseparator1, -2, 5);

 hbuttonbox1 = gtk_hbutton_box_new ();
 gtk_widget_set_name (hbuttonbox1, "hbuttonbox1");
 gtk_widget_ref (hbuttonbox1);
 gtk_object_set_data_full (GTK_OBJECT (Options), "hbuttonbox1", hbuttonbox1,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (hbuttonbox1);
 gtk_box_pack_start (GTK_BOX (vbox1), hbuttonbox1, TRUE, TRUE, 0);
 gtk_widget_set_usize (hbuttonbox1, -2, 27);
 gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox1), GTK_BUTTONBOX_END);
 gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbuttonbox1), 0);
 gtk_button_box_set_child_size (GTK_BUTTON_BOX (hbuttonbox1), 90, 30);

 opOk = gtk_button_new_with_label ("Ok");
 gtk_widget_set_name (opOk, "opOk");
 gtk_widget_ref (opOk);
 gtk_object_set_data_full (GTK_OBJECT (Options), "opOk", opOk,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (opOk);
 gtk_container_add (GTK_CONTAINER (hbuttonbox1), opOk);
 GTK_WIDGET_SET_FLAGS (opOk, GTK_CAN_DEFAULT);

 opCancel = gtk_button_new_with_label ("Cancel");
 gtk_widget_set_name (opCancel, "opCancel");
 gtk_widget_ref (opCancel);
 gtk_object_set_data_full (GTK_OBJECT (Options), "opCancel", opCancel,
                           (GtkDestroyNotify) gtk_widget_unref);
 gtk_widget_show (opCancel);
 gtk_container_add (GTK_CONTAINER (hbuttonbox1), opCancel);
 GTK_WIDGET_SET_FLAGS (opCancel, GTK_CAN_DEFAULT);

 gtk_signal_connect (GTK_OBJECT (Options), "key_press_event",
                     GTK_SIGNAL_FUNC (on_window2_key_press_event),
                     NULL);
 gtk_signal_connect (GTK_OBJECT (Options), "key_release_event",
                     GTK_SIGNAL_FUNC (on_window2_key_release_event),
                     NULL);
 gtk_signal_connect (GTK_OBJECT (Options), "destroy_event",
                     GTK_SIGNAL_FUNC (on_window2_destroy_event),
                     NULL);
 gtk_signal_connect (GTK_OBJECT (opAudioDriverMenu), "released",
                     GTK_SIGNAL_FUNC (on_opAudioDriverMenu_released),
                     NULL);
 gtk_signal_connect (GTK_OBJECT (opAudioDelaySkinButton), "changed",
                     GTK_SIGNAL_FUNC (on_opAudioDelaySkinButton_changed),
                     NULL);
 gtk_signal_connect (GTK_OBJECT (opAudioDelaySkinButton), "move_to_column",
                     GTK_SIGNAL_FUNC (on_opAudioDelaySkinButton_move_to_column),
                     NULL);
 gtk_signal_connect (GTK_OBJECT (opAudioDelaySkinButton), "move_to_row",
                     GTK_SIGNAL_FUNC (on_opAudioDelaySkinButton_move_to_row),
                     NULL);
 gtk_signal_connect (GTK_OBJECT (opAudioConfigureButton), "released",
                     GTK_SIGNAL_FUNC (on_opAudioConfigureButton_released),
                     NULL);

 gtk_signal_connect (GTK_OBJECT (opVideoMenu), "released",
                     GTK_SIGNAL_FUNC (on_opVideoMenu_released),
                     NULL);
 gtk_signal_connect (GTK_OBJECT (opVideoMenu), "clicked",
                     GTK_SIGNAL_FUNC (on_opVideoMenu_clicked),
                     NULL);
 gtk_signal_connect (GTK_OBJECT (opVideoMenu), "button_release_event",
                     GTK_SIGNAL_FUNC (on_opVideoMenu_button_release_event),
                     NULL);
 gtk_signal_connect (GTK_OBJECT (opVideoMenu), "pressed",
                     GTK_SIGNAL_FUNC (on_opVideoMenu_pressed),
                     NULL);

 gtk_signal_connect (GTK_OBJECT (opOsdCheckButton), "toggled",
                     GTK_SIGNAL_FUNC (on_opOsdCheckButton_toggled),
                     NULL);
 gtk_signal_connect (GTK_OBJECT (opVideoConfButton), "released",
                     GTK_SIGNAL_FUNC (on_opVideoConfButton_released),
                     NULL);
 gtk_signal_connect (GTK_OBJECT (opOSDLevelSpinButton), "changed",
                     GTK_SIGNAL_FUNC (on_opOSDLevelSpinButton_changed),
                     NULL);
 gtk_signal_connect (GTK_OBJECT (opOSDLevelSpinButton), "move_to_column",
                     GTK_SIGNAL_FUNC (on_opOSDLevelSpinButton_move_to_column),
                     NULL);
 gtk_signal_connect (GTK_OBJECT (opOSDLevelSpinButton), "move_to_row",
                     GTK_SIGNAL_FUNC (on_opOSDLevelSpinButton_move_to_row),
                     NULL);
 gtk_signal_connect (GTK_OBJECT (opAutoFullscreenCheckBox), "toggled",
                     GTK_SIGNAL_FUNC (on_opAutoFullscreenCheckBox_toggled),
                     NULL);
 gtk_signal_connect (GTK_OBJECT (opOk), "released",
                     GTK_SIGNAL_FUNC (on_opOk_released),
                     NULL);
 gtk_signal_connect (GTK_OBJECT (opCancel), "released",
                     GTK_SIGNAL_FUNC (on_opCancel_released),
                     NULL);

 gtk_object_set_data (GTK_OBJECT (Options), "tooltips", tooltips);

 return Options;
}
