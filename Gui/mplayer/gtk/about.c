
#include "../../app.h"
#include "../../../config.h"
#include "../../../help_mp.h"

#include "mplayer/pixmaps/about.xpm"
#include "../widgets.h"
#include "about.h"
#include "common.h"

GtkWidget * About = NULL;

void ShowAboutBox( void )
{
 if ( About ) gtkActive( About );
   else About=create_About();
 gtk_widget_show( About );
}

void abWidgetDestroy( GtkWidget * widget,GtkWidget ** widget_pointer )
{ WidgetDestroy( NULL,&About ); }

GtkWidget * create_About( void )
{
  GtkWidget     * vbox;
  GtkWidget     * pixmap1;
  GtkWidget     * scrolledwindow1;
  GtkWidget     * AboutText;
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
  gtk_window_set_wmclass( GTK_WINDOW( About ),"About","MPlayer" );

  gtk_widget_realize( About );
  gtkAddIcon( About );
  
  vbox=AddVBox( AddDialogFrame( About ),0 );

  pixmapstyle=gtk_widget_get_style( About );
  pixmapwid=gdk_pixmap_colormap_create_from_xpm_d( About->window,gdk_colormap_get_system(),&mask,&pixmapstyle->bg[GTK_STATE_NORMAL],about_xpm );
  pixmap1=gtk_pixmap_new( pixmapwid,mask );

  gtk_widget_set_name( pixmap1,"pixmap1" );
  gtk_widget_show( pixmap1 );
  gtk_box_pack_start( GTK_BOX( vbox ),pixmap1,FALSE,FALSE,0 );
  gtk_widget_set_usize( pixmap1,-2,174 );

  AddHSeparator( vbox );

  scrolledwindow1=gtk_scrolled_window_new( NULL,NULL );
  gtk_widget_set_name( scrolledwindow1,"scrolledwindow1" );
  gtk_widget_show( scrolledwindow1 );
  gtk_box_pack_start( GTK_BOX( vbox ),scrolledwindow1,TRUE,TRUE,0 );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolledwindow1 ),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC );

  AboutText=gtk_text_new( NULL,NULL );
  gtk_widget_set_name( AboutText,"AboutText" );
  gtk_widget_show( AboutText );
  gtk_container_add( GTK_CONTAINER( scrolledwindow1 ),AboutText );
  gtk_text_insert( GTK_TEXT( AboutText ),NULL,NULL,NULL,
  	"\n" \
  	"GUI development sponsored by UHU Linux\n" \
	"             (http://www.uhulinux.hu/)\n" \
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
	"     * Juergen Keil\n" \
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

  AddHSeparator( vbox );
  Ok=AddButton( MSGTR_Ok,AddHButtonBox( vbox ) );

  gtk_signal_connect( GTK_OBJECT( About ),"destroy",GTK_SIGNAL_FUNC( WidgetDestroy ),&About );
  gtk_signal_connect_object( GTK_OBJECT( Ok ),"clicked",GTK_SIGNAL_FUNC( abWidgetDestroy ),NULL );

  gtk_widget_add_accelerator( Ok,"clicked",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );
  gtk_widget_add_accelerator( Ok,"clicked",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE );
  gtk_window_add_accel_group( GTK_WINDOW( About ),accel_group );

  return About;
}
