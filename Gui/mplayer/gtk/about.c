
#include "../../events.h"
#include "../../../config.h"
#include "../../../help_mp.h"

#include "mplayer/pixmaps/about.xpm"
#include "../widgets.h"
#include "about.h"

int gtkVAboutBox = 0;
GtkWidget * AboutBox;

void ShowAboutBox( void )
{
 if ( gtkVAboutBox ) gtkActive( AboutBox );
   else AboutBox=create_About();
 gtk_widget_show( AboutBox );
}

void ab_AboutBox_show( GtkButton * button,gpointer user_data )
{ gtkVAboutBox=(int)user_data; }

void ab_Ok_released( GtkButton * button,gpointer user_data )
{
 gtkVAboutBox=(int)user_data; 
 gtk_widget_destroy( AboutBox );
}

GtkWidget * create_About( void )
{
  GtkWidget     * About;
  GtkWidget     * frame1;
  GtkWidget     * frame2;
  GtkWidget     * frame3;
  GtkWidget     * frame4;
  GtkWidget     * vbox1;
  GtkWidget     * pixmap1;
  GtkWidget     * hseparator2;
  GtkWidget     * scrolledwindow1;
  GtkWidget     * AboutText;
  GtkWidget     * hseparator1;
  GtkWidget     * hbuttonbox1;
  GtkWidget     * Ok;

  GtkStyle      * pixmapstyle;
  GdkPixmap     * pixmapwid;
  GdkBitmap     * mask;

  GtkAccelGroup * accel_group;

  accel_group=gtk_accel_group_new();

  About=gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_widget_set_name( About,MSGTR_About );
  gtk_object_set_data( GTK_OBJECT( About ),MSGTR_About,About );
  gtk_widget_set_usize( About,340,415 );
  gtk_window_set_title( GTK_WINDOW( About ),MSGTR_About );
  gtk_window_set_position( GTK_WINDOW( About ),GTK_WIN_POS_CENTER );
  gtk_window_set_policy( GTK_WINDOW( About ),TRUE,FALSE,FALSE );
  gtk_window_set_wmclass( GTK_WINDOW( About ),MSGTR_About,"MPlayer" );

  gtk_widget_realize( About );
  gtkAddIcon( About );

  frame1=gtk_frame_new( NULL );
  gtk_widget_set_name( frame1,"frame1" );
  gtk_widget_ref( frame1 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"frame1",frame1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( frame1 );
  gtk_container_add( GTK_CONTAINER( About ),frame1 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame1 ),GTK_SHADOW_IN );

  frame2=gtk_frame_new( NULL );
  gtk_widget_set_name( frame2,"frame2" );
  gtk_widget_ref( frame2 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"frame2",frame2,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( frame2 );
  gtk_container_add( GTK_CONTAINER( frame1 ),frame2 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame2 ),GTK_SHADOW_NONE );

  frame3=gtk_frame_new( NULL );
  gtk_widget_set_name( frame3,"frame3" );
  gtk_widget_ref( frame3 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"frame3",frame3,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( frame3 );
  gtk_container_add( GTK_CONTAINER( frame2 ),frame3 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame3 ),GTK_SHADOW_ETCHED_OUT );

  frame4=gtk_frame_new( NULL );
  gtk_widget_set_name( frame4,"frame4" );
  gtk_widget_ref( frame4 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"frame4",frame4,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( frame4 );
  gtk_container_add( GTK_CONTAINER( frame3 ),frame4 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame4 ),GTK_SHADOW_NONE );

  vbox1=gtk_vbox_new( FALSE,0 );
  gtk_widget_set_name( vbox1,"vbox1" );
  gtk_widget_ref( vbox1 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"vbox1",vbox1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( vbox1 );
  gtk_container_add( GTK_CONTAINER( frame4 ),vbox1 );

  pixmapstyle=gtk_widget_get_style( About );
  pixmapwid=gdk_pixmap_colormap_create_from_xpm_d( About->window,gdk_colormap_get_system(),&mask,&pixmapstyle->bg[GTK_STATE_NORMAL],about_xpm );
  pixmap1=gtk_pixmap_new( pixmapwid,mask );

  gtk_widget_set_name( pixmap1,"pixmap1" );
  gtk_widget_ref( pixmap1 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"pixmap1",pixmap1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( pixmap1 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),pixmap1,FALSE,FALSE,0 );
  gtk_widget_set_usize( pixmap1,-2,174 );

  hseparator2=gtk_hseparator_new( );
  gtk_widget_set_name( hseparator2,"hseparator2" );
  gtk_widget_ref( hseparator2 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"hseparator2",hseparator2,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( hseparator2 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),hseparator2,FALSE,FALSE,0 );
  gtk_widget_set_usize( hseparator2,-2,7 );

  scrolledwindow1=gtk_scrolled_window_new( NULL,NULL );
  gtk_widget_set_name( scrolledwindow1,"scrolledwindow1" );
  gtk_widget_ref( scrolledwindow1 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"scrolledwindow1",scrolledwindow1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( scrolledwindow1 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),scrolledwindow1,TRUE,TRUE,0 );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolledwindow1 ),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC );

  AboutText=gtk_text_new( NULL,NULL );
  gtk_widget_set_name( AboutText,"AboutText" );
  gtk_widget_ref( AboutText );
  gtk_object_set_data_full( GTK_OBJECT( About ),"AboutText",AboutText,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( AboutText );
  gtk_container_add( GTK_CONTAINER( scrolledwindow1 ),AboutText );
  gtk_text_insert( GTK_TEXT( AboutText ),NULL,NULL,NULL,
	"\n" \
	"   MPlayer core team:\n" \
	"\n" \
	"     * Arpad Gereoffy (A'rpi/ESP-team)\n" \
	"     * Zoltan Ponekker (Pontscho/Fresh!mindworkz)\n" \
	"     * Gabor Berczi (Gabucino)\n" \
	"     * Alex Beregszaszi (al3x)\n" \
	"     * Gabor Lenart (LGB)\n" \
	"     * Felix Buenemann (Atmos)\n" \
	"     * Alban Bedel (Albeu)\n" \
	"     * pl\n" \
	"     * Michael Niedermayer\n" \
	"\n" \
	"   Additional codes:\n" \
	"\n" \
	"     * Szabolcs Berecz (Szabi)\n" \
	"     * Laszlo Megyer (Lez, Laaz)\n" \
	"     * Gyula Laszlo (Chass, Tegla)\n" \
	"     * Zoltan Mark Vician (Se7en)\n" \
	"     * Andreas Ackermann (Acki)\n" \
	"     * TeLeNiEkO\n" \
	"     * Michael Graffam\n" \
	"     * Jens Hoffmann\n" \
	"     * German Gomez Garcia\n" \
	"     * Dariusz Pietrzak (Eyck)\n" \
	"     * Marcus Comstedt\n" \
	"     * Jurgen Keil\n" \
	"     * Vladimir Kushnir\n" \
	"     * Bertrand BAUDET\n" \
	"     * Derek J Witt\n" \
	"     * Artur Zaprzala\n" \
	"     * lanzz@lanzz.org\n" \
	"     * Adam Tla/lka\n" \
	"     * Folke Ashberg\n" \
	"     * Kamil Toman\n" \
	"     * Ivan Kalvatchev\n" \
	"     * Sven Goethel\n" \
	"     * joy_ping\n" \
	"     * Eric Anholt\n" \
	"     * Jiri Svoboda\n" \
	"     * Oliver Schoenbrunner\n" \
	"     * Jeroen Dobbelaere\n" \
	"     * David Holm\n" \
	"     * Panagiotis Issaris\n" \
	"     * Mike Melanson\n" \
	"     * Tobias Diedrich\n" \
	"     * Kilian A. Foth\n" \
	"     * Tim Ferguson\n" \
	"     * Sam Lin\n" \
	"     * Johannes Feigl\n" \
	"     * Kim Minh Kaplan\n" \
	"     * Brian Kuschak\n" \
	"     * Stephen Davies\n" \
	"     * Rik Snel\n" \
	"     * Anders Johansson\n" \
	"     * Roberto Togni\n" \
	"     * Wojtek Kaniewski\n" \
	"     * Fredrik Kuivinen\n" \
	"\n" \
	"   Main testers:\n" \
	"\n" \
	"     * Tibor Balazs (Tibcu)\n" \
	"     * Peter Sasi (SaPe)\n" \
	"     * Christoph H. Lampert\n" \
	"     * Attila Kinali\n" \
	"     * Dirk Vornheder\n" \
	"     * Bohdan Horst (Nexus)\n" \
	"\n",1481 );

  hseparator1=gtk_hseparator_new();
  gtk_widget_set_name( hseparator1,"hseparator1" );
  gtk_widget_ref( hseparator1 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"hseparator1",hseparator1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( hseparator1 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),hseparator1,FALSE,FALSE,0 );
  gtk_widget_set_usize( hseparator1,-2,10 );

  hbuttonbox1=gtk_hbutton_box_new( );
  gtk_widget_set_name( hbuttonbox1,"hbuttonbox1" );
  gtk_widget_ref( hbuttonbox1 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"hbuttonbox1",hbuttonbox1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_set_usize( hbuttonbox1,-2,25 );
  gtk_button_box_set_child_size( GTK_BUTTON_BOX( hbuttonbox1 ),75,0 );
  gtk_widget_show( hbuttonbox1 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),hbuttonbox1,FALSE,FALSE,0 );

  Ok=gtk_button_new_with_label( MSGTR_Ok );
  gtk_widget_set_name( Ok,MSGTR_Ok );
  gtk_widget_ref( Ok );
  gtk_object_set_data_full( GTK_OBJECT( About ),MSGTR_Ok,Ok,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( Ok );
  gtk_container_add( GTK_CONTAINER( hbuttonbox1 ),Ok );

  gtk_signal_connect( GTK_OBJECT( About ),"destroy",GTK_SIGNAL_FUNC( ab_Ok_released ),0 );
  gtk_signal_connect( GTK_OBJECT( About ),"show",GTK_SIGNAL_FUNC( ab_AboutBox_show ),(void *)1 );
  gtk_signal_connect( GTK_OBJECT( About ),"hide",GTK_SIGNAL_FUNC( ab_AboutBox_show ),0 );
  gtk_signal_connect( GTK_OBJECT( Ok ),"released",GTK_SIGNAL_FUNC( ab_Ok_released ),0 );

  gtk_widget_add_accelerator( Ok,"released",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );
  gtk_widget_add_accelerator( Ok,"released",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE );
  gtk_window_add_accel_group( GTK_WINDOW( About ),accel_group );

  return About;
}
