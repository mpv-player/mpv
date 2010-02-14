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

#include "gui/mplayer/pixmaps/about.xpm"
#include "gui/mplayer/widgets.h"
#include "about.h"
#include "gtk_common.h"

GtkWidget * About = NULL;

void ShowAboutBox( void )
{
 if ( About ) gtkActive( About );
   else About=create_About();
 gtk_widget_show( About );
}

static void abWidgetDestroy( GtkWidget * widget, GtkWidget ** widget_pointer )
{ WidgetDestroy( NULL,&About ); }

GtkWidget * create_About( void )
{
  GtkWidget     * vbox;
  GtkWidget     * pixmap1;
  GtkWidget     * scrolledwindow1;
  GtkWidget     * AboutText;
  GtkWidget     * Ok;

#ifdef CONFIG_GTK2
  GtkTextBuffer * AboutTextBuffer;
  GtkTextIter   iter;
#endif /* CONFIG_GTK2 */

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

#ifdef CONFIG_GTK2
  AboutText = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(AboutText), FALSE);
  gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(AboutText), FALSE);
  AboutTextBuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (AboutText));
  gtk_text_buffer_get_iter_at_offset (AboutTextBuffer, &iter, 0);
#else
  AboutText=gtk_text_new( NULL,NULL );
  gtk_text_set_editable(GTK_TEXT(AboutText), FALSE);
#endif
  gtk_widget_set_name( AboutText,"AboutText" );
  gtk_widget_show( AboutText );
  gtk_container_add( GTK_CONTAINER( scrolledwindow1 ),AboutText );
#ifdef CONFIG_GTK2
  gtk_text_buffer_insert (AboutTextBuffer, &iter,
#else
  gtk_text_insert( GTK_TEXT( AboutText ),NULL,NULL,NULL,
#endif
  	"\n"
	MSGTR_ABOUT_UHU
	"             (http://www.uhulinux.hu/)\n"
	"\n"
	MSGTR_ABOUT_Contributors
	"\n"
	"     * Ackermann, Andreas\n"
	"     * adland\n"
	"     * Anholt, Eric\n"
	"     * Ashberg, Folke\n"
	"     * Balatoni, Dénes\n"
	"     * Barat, Zsolt\n"
	"     * Barbato, Luca\n"
	"     * Baryshkov, Dmitry\n"
	"     * Baudet, Bertrand\n"
	"     * Bedel, Alban\n"
	"     * Behrisch, Michael\n"
	"     * Belev, Luchezar\n"
	"     * Bérczi, Gábor\n"
	"     * Berecz, Szabolcs\n"
	"     * Beregszászi, Alex\n"
	"     * Bitterberg, Tilmann\n"
	"     * Biurrun, Diego\n"
	"     * Blomenkamp, Marcsu\n"
	"     * Buehler, Andrew\n"
	"     * Bulgroz, Eviv\n"
	"     * Bünemann, Felix\n"
	"     * Bunkus, Moritz\n"
	"     * Christiansen, Dan Villiom Podlaski\n"
	"     * Clagg, Jeff\n"
	"     * Compn\n"
	"     * Comstedt, Marcus\n"
	"     * Cook, Kees\n"
	"     * Davies, Stephen\n"
	"     * Di Vita, Piero\n"
	"     * Diedrich, Tobias\n"
	"     * Dietrich, Florian\n"
	"     * Dobbelaere, Jeroen\n"
	"     * Döffinger, Reimar\n"
	"     * Dolbeau, Romain\n"
	"     * Dönmez, Ismail\n"
	"     * Edele, Robert\n"
	"     * Egger, Christoph\n"
	"     * Elsinghorst, Paul Wilhelm\n"
	"     * Ernesti, Bernd\n"
	"     * Falco, Salvatore\n"
	"     * Feigl, Johannes\n"
	"     * Felker, D Richard III\n"
	"     * Ferguson, Tim\n"
	"     * Finlayson, Ross\n"
	"     * Forghieri, Daniele\n"
	"     * Foth, Kilian A.\n"
	"     * Franz, Fabian\n"
	"     * Gansser, Martin\n"
	"     * Gereöffy, Árpád\n"
	"     * Giani, Matteo\n"
	"     * Goethel, Sven\n"
	"     * Gomez Garcia, German\n"
	"     * Gottwald, Alexander\n"
	"     * Graffam, Michael\n"
	"     * Gritsenko, Andriy N.\n"
	"     * Guyomarch, Rémi\n"
	"     * Hammelmann, Jürgen\n"
	"     * Hertel, Christopher R.\n"
	"     * Hess, Andreas\n"
	"     * Hickey, Corey\n"
	"     * Hidvégi, Zoltán\n"
	"     * Hoffmann, Jens\n"
	"     * Holm, David\n"
	"     * Horst, Bohdan\n"
	"     * Hug, Hampa\n"
	"     * Hurka, Tomas\n"
	"     * Isani, Sidik\n"
	"     * Issaris, Panagiotis\n"
	"     * Jacobs, Aurelien\n"
	"     * Jelveh, Reza\n"
	"     * Jermann, Jonas\n"
	"     * Johansson, Anders\n"
	"     * Kain, Nicholas\n"
	"     * Kalinski, Filip\n"
	"     * Kalvachev, Ivan\n"
	"     * Kaniewski, Wojtek\n"
	"     * Kaplan, Kim Minh\n"
	"     * Kärkkäinen, Samuli\n"
	"     * Keil, Jürgen\n"
	"     * Kesterson, Robert\n"
	"     * Kinali, Attila\n"
	"     * Kovriga, Gregory\n"
	"     * Kühling, David\n"
	"     * Kuivinen, Fredrik\n"
	"     * Kurshev, Nick\n"
	"     * Kuschak, Brian\n"
	"     * Kushnir, Vladimir\n"
	"     * Lambley, Dave\n"
	"     * László, Gyula\n"
	"     * Le Gaillart, Nicolas\n"
	"     * Lénárt, Gábor\n"
	"     * Leroy, Colin\n"
	"     * Liljeblad, Oskar\n"
	"     * Lin, Sam\n"
	"     * Lombard, Pierre\n"
	"     * Madick, Puk\n"
	"     * Makovicka, Jindrich\n"
	"     * Marek, Rudolf\n"
	"     * Megyer, László\n"
	"     * Melanson, Mike\n"
	"     * von Merkatz, Arwed\n"
	"     * Merritt, Loren\n"
	"     * Mierzejewski, Dominik\n"
	"     * Milushev, Mihail\n"
	"     * Mistry, Nehal\n"
	"     * Mohari, András\n"
	"     * Mueller, Steven\n"
	"     * Neundorf, Alexander\n"
	"     * Niedermayer, Michael\n"
	"     * Noring, Fredrik\n"
	"     * Ohm, Christian\n"
	"     * Parrish, Joey\n"
	"     * Pietrzak, Dariusz\n"
	"     * Plourde, Nicolas\n"
	"     * Poettering, Lennart\n"
	"     * Poirier, Guillaume\n"
	"     * Ponekker, Zoltán\n"
	"     * van Poorten, Ivo\n"
	"     * Ran, Lu\n"
	"     * Reder, Uwe\n"
	"     * rgselk\n"
	"     * Rune Petersen\n"
	"     * Saari, Ville\n"
	"     * Sabbi, Nico\n"
	"     * Sandell, Björn\n"
	"     * Sauerbeck, Tilman\n"
	"     * Scherthan, Frank\n"
	"     * Schneider, Florian\n"
	"     * Schoenbrunner, Oliver\n"
	"     * Shimon, Oded\n"
	"     * Simon, Peter\n"
	"     * Snel, Rik\n"
	"     * Sommer, Sascha\n"
	"     * Strasser, Alexander\n"
	"     * Strzelecki, Kamil\n"
	"     * Svoboda, Jiri\n"
	"     * Swain, Robert\n"
	"     * Syrjälä, Ville\n"
	"     * Szecsi, Gabor\n"
	"     * Tackaberry, Jason\n"
	"     * Tam, Howell\n"
	"     * Tlalka, Adam\n"
	"     * Tiesi, Gianluigi\n"
	"     * Togni, Roberto\n"
	"     * Tropea, Salvador Eduardo\n"
	"     * Vajna, Miklós\n"
	"     * Verdejo Pinochet, Reynaldo H.\n"
	"     * Wigren, Per\n"
	"     * Witt, Derek J\n"
	"     * Young, Alan\n"
	"     * Zaprzala, Artur\n"
	"     * Zealey, Mark\n"
	"     * Ziv-Av, Matan\n"
	"     * Zoltán, Márk Vicián\n"
	"\n"
	MSGTR_ABOUT_Codecs_libs_contributions
	"\n"
	"     * Bellard, Fabrice\n"
	"     * Chappelier, Vivien and Vincent, Damien\n"
	"     * Hipp, Michael\n"
	"     * Holtzman, Aaron\n"
	"     * Janovetz, Jake\n"
	"     * Kabelac, Zdenek\n"
	"     * Kuznetsov, Eugene\n"
	"     * Lespinasse, Michel\n"
	"     * Podlipec, Mark\n"
	"\n"
	MSGTR_ABOUT_Translations
	"\n"
	"     * Biernat, Marcin\n"
	"     * Fargas, Marc\n"
	"     * Heryan, Jiri\n"
	"     * Jarycki, Marek\n"
	"     * Kaplita, Leszek\n"
	"     * Krämer, Sebastian\n"
	"     * López, Juan Martin\n"
	"     * Michniewski, Piotr\n"
	"     * Misiorny, Jakub\n"
	"     * Mizda, Gábor\n"
	"     * Paszta, Maciej\n"
	"     * Proszek, Łukasz\n"
	"     * Schiller, Wacław\n"
	"     * Zubimendi, Andoni\n"
	"\n"
	MSGTR_ABOUT_Skins
	"\n"
	"     * Azrael\n"
	"     * Bekesi, Viktor\n"
	"     * Burt.S.\n"
	"     * Carpenter, Andrew\n"
	"     * Foucault, Charles\n"
	"     * Gyimesi, Attila\n"
	"     * Hertroys, Alban\n"
	"     * Juan Pablo\n"
	"     * Kiss, Balint\n"
	"     * Kuehne, Andre\n"
	"     * Kuhlmann, Rüdiger\n"
	"     * Naumov, Dan\n"
	"     * Northam, Ryan\n"
	"     * Oyarzun Arroyo\n"
	"     * Park, DongCheon\n"
	"     * Pehrson, Jurgen\n"
	"     * Pizurica, Nikola\n"
	"     * Ptak, Oliwier\n"
	"     * Riccio, Pasquale\n"
	"     * Schultz, Jesper\n"
	"     * Szumiela, Marcin\n"
	"     * Tisi, Massimo\n"
	"     * Tyr, Jiri jun.\n"
	"     * Vasilev, Ognian\n"
	"     * Veres, Imre\n"
	"     * Vesko, Radic\n"
	"     * Vigvary, Balasz\n"
	"     * Weber, Andrew\n"
	"     * Whitmore, Gary Jr.\n"
	"     * Wilamowski, Franciszek\n"
	"     * Zeising, Michael\n"
	"\n",-1 );

  AddHSeparator( vbox );
  Ok=AddButton( MSGTR_Ok,AddHButtonBox( vbox ) );

  gtk_signal_connect( GTK_OBJECT( About ),"destroy",GTK_SIGNAL_FUNC( WidgetDestroy ),&About );
  gtk_signal_connect_object( GTK_OBJECT( Ok ),"clicked",GTK_SIGNAL_FUNC( abWidgetDestroy ),NULL );

  gtk_widget_add_accelerator( Ok,"clicked",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );
  gtk_widget_add_accelerator( Ok,"clicked",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE );
  gtk_window_add_accel_group( GTK_WINDOW( About ),accel_group );

  return About;
}
