
#include "../../events.h"
#include "../../../config.h"
#include "../../../help_mp.h"

#include "pixmaps/about.xpm"
#include "../widgets.h"
#include "about.h"

void ab_Ok_released( GtkButton * button,gpointer user_data )
{ gtk_widget_destroy( AboutBox ); }

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

  accel_group=gtk_accel_group_new( );

  About=gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_widget_set_name( About,MSGTR_About );
  gtk_object_set_data( GTK_OBJECT( About ),MSGTR_About,About );
  gtk_widget_set_usize( About,340,415 );
  gtk_window_set_title( GTK_WINDOW( About ),MSGTR_About );
  gtk_window_set_position( GTK_WINDOW( About ),GTK_WIN_POS_CENTER );
  gtk_window_set_policy( GTK_WINDOW( About ),TRUE,FALSE,FALSE );

  frame1=gtk_frame_new( NULL );
  gtk_widget_set_name( frame1,"frame1" );
  gtk_widget_ref( frame1 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"frame1",frame1,( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( frame1 );
  gtk_container_add( GTK_CONTAINER( About ),frame1 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame1 ),GTK_SHADOW_IN );

  frame2=gtk_frame_new( NULL );
  gtk_widget_set_name( frame2,"frame2" );
  gtk_widget_ref( frame2 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"frame2",frame2,( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( frame2 );
  gtk_container_add( GTK_CONTAINER( frame1 ),frame2 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame2 ),GTK_SHADOW_NONE );

  frame3=gtk_frame_new( NULL );
  gtk_widget_set_name( frame3,"frame3" );
  gtk_widget_ref( frame3 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"frame3",frame3,( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( frame3 );
  gtk_container_add( GTK_CONTAINER( frame2 ),frame3 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame3 ),GTK_SHADOW_ETCHED_OUT );

  frame4=gtk_frame_new( NULL );
  gtk_widget_set_name( frame4,"frame4" );
  gtk_widget_ref( frame4 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"frame4",frame4,( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( frame4 );
  gtk_container_add( GTK_CONTAINER( frame3 ),frame4 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame4 ),GTK_SHADOW_NONE );

  vbox1=gtk_vbox_new( FALSE,0 );
  gtk_widget_set_name( vbox1,"vbox1" );
  gtk_widget_ref( vbox1 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"vbox1",vbox1,( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( vbox1 );
  gtk_container_add( GTK_CONTAINER( frame4 ),vbox1 );

  pixmapstyle=gtk_widget_get_style( About );
  pixmapwid=gdk_pixmap_colormap_create_from_xpm_d( About->window,gdk_colormap_get_system(),&mask,&pixmapstyle->bg[GTK_STATE_NORMAL],about_xpm );
  pixmap1=gtk_pixmap_new( pixmapwid,mask );

  gtk_widget_set_name( pixmap1,"pixmap1" );
  gtk_widget_ref( pixmap1 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"pixmap1",pixmap1,( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( pixmap1 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),pixmap1,FALSE,FALSE,0 );
  gtk_widget_set_usize( pixmap1,-2,174 );

  hseparator2=gtk_hseparator_new( );
  gtk_widget_set_name( hseparator2,"hseparator2" );
  gtk_widget_ref( hseparator2 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"hseparator2",hseparator2,( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( hseparator2 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),hseparator2,FALSE,FALSE,0 );
  gtk_widget_set_usize( hseparator2,-2,7 );

  scrolledwindow1=gtk_scrolled_window_new( NULL,NULL );
  gtk_widget_set_name( scrolledwindow1,"scrolledwindow1" );
  gtk_widget_ref( scrolledwindow1 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"scrolledwindow1",scrolledwindow1,( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( scrolledwindow1 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),scrolledwindow1,TRUE,TRUE,0 );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolledwindow1 ),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC );

  AboutText=gtk_text_new( NULL,NULL );
  gtk_widget_set_name( AboutText,"AboutText" );
  gtk_widget_ref( AboutText );
  gtk_object_set_data_full( GTK_OBJECT( About ),"AboutText",AboutText,( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( AboutText );
  gtk_container_add( GTK_CONTAINER( scrolledwindow1 ),AboutText );
  gtk_text_insert( GTK_TEXT( AboutText ),NULL,NULL,NULL,
                   "\nMPlayer code:\n" \
                   "       fileformat detection,demuxers - A'rpi\n" \
                   "       DVD support - ( alpha version was: LGB ) now: ?\n" \
                   "       network streaming - Bertrand BAUDET\n" \
                   "       A-V sync code - A'rpi\n" \
                   "       subtitles file parser/reader - Lez( most of them )\n" \
                   "       config files & commandline parser - Szabi\n" \
                   "       fastmemcpy - Nick Kurshev\n" \
                   "       LIRC support - Acki\n" \
                   "       SUB/OSD renderer - Adam Tla/lka\n" \
                   "       Gui - Pontscho\n\nlibvo drivers:\n" \
                   "       vo_aa.c - Folke Ashberg\n" \
                   "       vo_dga.c - Acki\n" \
                   "       vo_fbdev.c - Szabi\n" \
                   "       vo_ggi.c - al3x\n" \
                   "       vo_gl.c - A'rpi\n" \
                   "       vo_md5.c - A'rpi\n" \
                   "       vo_mga.c - A'rpi\n" \
                   "       vo_null.c - A'rpi\n" \
                   "       vo_odivx.c - A'rpi\n" \
                   "       vo_pgm.c - A'rpi\n" \
                   "       vo_png.c - Atmos\n" \
                   "       vo_sdl.c - Atmos\n" \
                   "       vo_svga.c - se7en\n" \
                   "       vo_x11.c - Pontscho\n"\
                   "       vo_xmga.c - Pontscho\n"\
                   "       vo_xv.c - Pontscho\n" \
                   "       vo_aa.c - Folke Ashberg\n\n" \
                   "libao2 drivers:\n" \
                   "       ao_alsa5.c - al3x\n" \
                   "       ao_alsa9.c - al3x( BUGGY,use oss )\n" \
                   "       ao_null.c - A'rpi\n" \
                   "       ao_oss.c - A'rpi\n" \
                   "       ao_pcm.c - Atmos\n" \
                   "       ao_sdl.c - Atmos\n" \
                   "       ao_sun.c - Jürgen Keil\n\n" \
                   "Homepage:\n" \
                   "        Design:  Chass\n" \
                   "        Contents: Gabucino\n" \
                   "                  LGB\n\n" \
                   "English documentation:\n" \
                   "        tech-*.txt: A'rpi\n" \
                   "        all the others: Gabucino\n\n" \
                   "Documentation translations:\n" \
                   "       Hungarian - Gabucino\n" \
                   "       Spanish - TeLeNiEkO\n" \
                   "       Russian - Nick Kurshev\n" \
                   "       Polish - Dariush Pietrzak\n" \
                   "       German - Atmosfear\n\n" \
                   "Platforms/ports:\n" \
                   "       DEBIAN packaging - Dariush Pietrzak\n" \
                   "       FreeBSD support - Vladimir Kushnir\n" \
                   "       Solaris 8 support - Jürgen Keil\n",1535 );

  hseparator1=gtk_hseparator_new( );
  gtk_widget_set_name( hseparator1,"hseparator1" );
  gtk_widget_ref( hseparator1 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"hseparator1",hseparator1,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( hseparator1 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),hseparator1,FALSE,FALSE,0 );
  gtk_widget_set_usize( hseparator1,-2,10 );

  hbuttonbox1=gtk_hbutton_box_new( );
  gtk_widget_set_name( hbuttonbox1,"hbuttonbox1" );
  gtk_widget_ref( hbuttonbox1 );
  gtk_object_set_data_full( GTK_OBJECT( About ),"hbuttonbox1",hbuttonbox1,
                           ( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_set_usize( hbuttonbox1,-2,25 );
  gtk_button_box_set_child_size( GTK_BUTTON_BOX( hbuttonbox1 ),75,0 );
  gtk_widget_show( hbuttonbox1 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),hbuttonbox1,FALSE,FALSE,0 );

  Ok=gtk_button_new_with_label( MSGTR_Ok );
  gtk_widget_set_name( Ok,MSGTR_Ok );
  gtk_widget_ref( Ok );
  gtk_object_set_data_full( GTK_OBJECT( About ),MSGTR_Ok,Ok,( GtkDestroyNotify ) gtk_widget_unref );
  gtk_widget_show( Ok );
  gtk_container_add( GTK_CONTAINER( hbuttonbox1 ),Ok );

  gtk_signal_connect( GTK_OBJECT( About ),"destroy",GTK_SIGNAL_FUNC( ab_Ok_released ),NULL );
  gtk_signal_connect( GTK_OBJECT( Ok ),"released",GTK_SIGNAL_FUNC( ab_Ok_released ),NULL );

  gtk_widget_add_accelerator( Ok,"released",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );
  gtk_widget_add_accelerator( Ok,"released",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE );
  gtk_window_add_accel_group( GTK_WINDOW( About ),accel_group );

  return About;
}
