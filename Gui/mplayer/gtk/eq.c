
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "../../events.h"
#include "../../cfg.h"
#include "../../help_mp.h"
#include "../../../config.h"
#include "../../../help_mp.h"
#include "../../../mplayer.h"
#include "../../../libao2/eq.h"
#include "../../../libvo/video_out.h"
#include "../widgets.h"
#include "../mplayer.h"

#include "eq.h"

#define eqRange 5

GtkWidget * Equalizer;

static GtkWidget * Notebook;
static GtkWidget * ChannelsList;
static GtkWidget * VContrast, * VBrightness, * VHue, * VSaturation;
static GtkAdjustment * VContrastadj, * VBrightnessadj, * VHueadj, * VSaturationadj;
static GtkWidget * Ok, * Clear, * Config;
static GtkWidget * A3125,  * A125, * A6250, * A250, * A500, * A1000, * A2000, * A4000, * A8000, * A16000;
static GtkAdjustment * A3125adj, * A125adj, * A6250adj, * A250adj, * A500adj, * A1000adj, * A2000adj, * A4000adj, * A8000adj, * A16000adj;

static int Channel = -1;
static int gtkVEqualizer = 0;
static int gtkVEquConfig = 0;

// ---

char * gtkEquChannel1 = NULL;
char * gtkEquChannel2 = NULL;
char * gtkEquChannel3 = NULL;
char * gtkEquChannel4 = NULL;
char * gtkEquChannel5 = NULL;
char * gtkEquChannel6 = NULL;

// ---

void ShowEquConfig( void );
void HideEquConfig( void );

static void eqSetBands( int channel )
{
 if ( channel < 0 ) channel=0;
 gtk_adjustment_set_value( A3125adj,0.0f - gtkEquChannels[channel][0] );
 gtk_adjustment_set_value( A6250adj,0.0f - gtkEquChannels[channel][1] );
 gtk_adjustment_set_value( A125adj,0.0f - gtkEquChannels[channel][2] );
 gtk_adjustment_set_value( A250adj,0.0f - gtkEquChannels[channel][3] );
 gtk_adjustment_set_value( A500adj,0.0f - gtkEquChannels[channel][4] );
 gtk_adjustment_set_value( A1000adj,0.0f - gtkEquChannels[channel][5] );
 gtk_adjustment_set_value( A2000adj,0.0f - gtkEquChannels[channel][6] );
 gtk_adjustment_set_value( A4000adj,0.0f - gtkEquChannels[channel][7] );
 gtk_adjustment_set_value( A8000adj,0.0f - gtkEquChannels[channel][8] );
 gtk_adjustment_set_value( A16000adj,0.0f - gtkEquChannels[channel][9] );

 gtk_adjustment_set_value( VContrastadj,(float)vo_gamma_contrast );
 gtk_adjustment_set_value( VBrightnessadj,(float)vo_gamma_brightness );
 gtk_adjustment_set_value( VHueadj,(float)vo_gamma_hue );
 gtk_adjustment_set_value( VSaturationadj,(float)vo_gamma_saturation );
}

static void eqSetChannelNames( void )
{
 gchar * str[2];
 gtk_clist_clear( GTK_CLIST( ChannelsList ) );
 str[1]="";
 str[0]=MSGTR_EQU_All;
 gtk_clist_append( GTK_CLIST( ChannelsList ) ,str);
 if ( guiIntfStruct.AudioType > 1 )
  {
   str[0]=gtkEquChannel1; gtk_clist_append( GTK_CLIST( ChannelsList ) ,str);
   str[0]=gtkEquChannel2; gtk_clist_append( GTK_CLIST( ChannelsList ) ,str);
  }
 if ( guiIntfStruct.AudioType > 2 )
  {
   str[0]=gtkEquChannel3; gtk_clist_append( GTK_CLIST( ChannelsList ) ,str);
   str[0]=gtkEquChannel4; gtk_clist_append( GTK_CLIST( ChannelsList ) ,str);
  }
 if ( guiIntfStruct.AudioType > 4 )
  {
   str[0]=gtkEquChannel5; gtk_clist_append( GTK_CLIST( ChannelsList ) ,str);
   str[0]=gtkEquChannel6; gtk_clist_append( GTK_CLIST( ChannelsList ) ,str);
  }
 gtk_clist_select_row( GTK_CLIST( ChannelsList ),0,0 );
}

void ShowEqualizer( void )
{
 if ( gtkVEqualizer ) gtkActive( Equalizer );
    else Equalizer=create_Equalizer();

 if ( !gtkEquChannel1 ) gtkEquChannel1=strdup( MSGTR_EQU_Front_Right );
 if ( !gtkEquChannel2 ) gtkEquChannel2=strdup( MSGTR_EQU_Front_Left );
 if ( !gtkEquChannel3 ) gtkEquChannel3=strdup( MSGTR_EQU_Back_Right );
 if ( !gtkEquChannel4 ) gtkEquChannel4=strdup( MSGTR_EQU_Back_Left );
 if ( !gtkEquChannel5 ) gtkEquChannel5=strdup( MSGTR_EQU_Center );
 if ( !gtkEquChannel6 ) gtkEquChannel6=strdup( MSGTR_EQU_Bass );

 eqSetChannelNames();

 VContrastadj->value=(float)vo_gamma_contrast;
 VBrightnessadj->value=(float)vo_gamma_brightness;
 VHueadj->value=(float)vo_gamma_hue;
 VSaturationadj->value=(float)vo_gamma_saturation;

 if ( !guiIntfStruct.Playing || !gtkEnableVideoEqualizer )
  {
   gtk_widget_set_sensitive( VContrast,FALSE );
   gtk_widget_set_sensitive( VBrightness,FALSE );
   gtk_widget_set_sensitive( VHue,FALSE );
   gtk_widget_set_sensitive( VSaturation,FALSE );
  }
 Channel=-1;
 eqSetBands( 0 );
 if ( !guiIntfStruct.Playing || !gtkEnableAudioEqualizer )
  {
   gtk_widget_set_sensitive( ChannelsList,FALSE );
   gtk_widget_set_sensitive( A3125,FALSE );
   gtk_widget_set_sensitive( A125,FALSE );
   gtk_widget_set_sensitive( A6250,FALSE );
   gtk_widget_set_sensitive( A250,FALSE );
   gtk_widget_set_sensitive( A500,FALSE );
   gtk_widget_set_sensitive( A1000,FALSE );
   gtk_widget_set_sensitive( A2000,FALSE );
   gtk_widget_set_sensitive( A4000,FALSE );
   gtk_widget_set_sensitive( A8000,FALSE );
   gtk_widget_set_sensitive( A16000,FALSE );
  }

 if ( gtk_notebook_get_current_page( GTK_NOTEBOOK( Notebook ) ) == 0 ) gtk_widget_show( Config );
 gtk_widget_show( Equalizer );
 gtkVisible++;
}

void HideEqualizer( void )
{
 if ( !gtkVEqualizer ) return;
 gtkVEqualizer=0; gtkVisible--;
 gtk_widget_hide( Equalizer );
 gtk_widget_destroy( Equalizer );
 if ( gtkVEquConfig ) HideEquConfig();
}

static gboolean eqHScaleMotion( GtkWidget * widget,GdkEventMotion  * event,gpointer user_data )
{
 equalizer_t eq;
 switch ( (int)user_data ) 
  {
   case 0: eq.gain=A3125adj->value; break;
   case 1: eq.gain=A6250adj->value; break;
   case 2: eq.gain=A125adj->value; break;
   case 3: eq.gain=A250adj->value; break;
   case 4: eq.gain=A500adj->value; break;
   case 5: eq.gain=A1000adj->value; break;
   case 6: eq.gain=A2000adj->value; break;
   case 7: eq.gain=A4000adj->value; break;
   case 8: eq.gain=A8000adj->value; break;
   case 9: eq.gain=A16000adj->value; break;
   default: return FALSE;
  }
 eq.gain=0.0f - eq.gain;
 eq.band=(int)user_data;
 if ( Channel == -1 ) 
  {
   int i;
   for ( i=0;i<6;i++ )
    { eq.channel=i; gtkSet( gtkSetEqualizer,0,&eq ); }
  } else { eq.channel=Channel; gtkSet( gtkSetEqualizer,0,&eq ); }
  
 return FALSE;
}

static gboolean eqVScaleMotion( GtkWidget * widget,GdkEventMotion  * event,gpointer user_data )
{

 switch( (int)user_data )
  {
   case 1: gtkSet( gtkSetContrast,VContrastadj->value,NULL );      break;
   case 2: gtkSet( gtkSetBrightness,VBrightnessadj->value,NULL );  break;
   case 3: gtkSet( gtkSetHue,VHueadj->value,NULL );	           break;
   case 4: gtkSet( gtkSetSaturation,VSaturationadj->value,NULL );  break;
  }

 return FALSE;
}

static void eqButtonReleased( GtkButton * button,gpointer user_data )
{ 
 switch( (int)user_data )
  {
   case 0: HideEqualizer(); break;
   case 1: 
	if ( gtk_notebook_get_current_page( GTK_NOTEBOOK( Notebook ) ) == 0 )
	 { 
	  if ( !guiIntfStruct.Playing || !gtkEnableAudioEqualizer ) break;
	  gtkSet( gtkSetEqualizer,0,NULL ); 
	  eqSetBands( Channel ); 
	 }
	 else
	  {
	   if ( !guiIntfStruct.Playing || !gtkEnableVideoEqualizer ) break;
	   gtkSet( gtkSetContrast,0.0f,NULL );
	   gtkSet( gtkSetBrightness,0.0f,NULL );
	   gtkSet( gtkSetHue,0.0f,NULL );
	   gtkSet( gtkSetSaturation,0.0f,NULL );
	   vo_gamma_brightness=vo_gamma_contrast=vo_gamma_hue=vo_gamma_saturation=0;
	   eqSetBands( Channel );
	  }
	break;
   case 2:
	ShowEquConfig();
	break;
  }
}

gboolean eqDestroy( GtkWidget * widget,GdkEvent * event,gpointer user_data )
{ HideEqualizer(); return FALSE; }

static void eqShow( GtkWidget * widget,gpointer user_data )
{ gtkVEqualizer=(int)user_data; }

static void eqSelectChannelsListRow( GtkCList * clist,gint row,gint column,GdkEvent * event,gpointer user_data )
{
 char * tmp;
 Channel=row - 1;
 eqSetBands( Channel );
 if ( Channel == -1 )
  {
   int i,j; equalizer_t eq;
   for ( i=1;i<6;i++ )
    for ( j=0;j<10;j++ )
     { eq.band=j; eq.channel=i; eq.gain=gtkEquChannels[0][j]; gtkSet( gtkSetEqualizer,0,&eq ); }
  }
}

void eqNotebook( GtkNotebook * notebook,GtkNotebookPage * page,gint page_num,gpointer user_data )
{
 if ( page_num ) gtk_widget_hide( Config );
   else gtk_widget_show( Config );
}

GtkWidget * create_Equalizer( void )
{
  GtkWidget * frame1;
  GtkWidget * frame2;
  GtkWidget * frame3;
  GtkWidget * frame4;
  GtkWidget * vbox1;
  GtkWidget * hbox1;
  GtkWidget * scrolledwindow1;
  GtkWidget * table1;
  GtkWidget * label3;
  GtkWidget * label4;
  GtkWidget * label5;
  GtkWidget * label6;
  GtkWidget * label7;
  GtkWidget * label8;
  GtkWidget * label9;
  GtkWidget * label10;
  GtkWidget * label11;
  GtkWidget * label12;
  GtkWidget * label1;
  GtkWidget * hbox2;
  GtkWidget * vbox2;
  GtkWidget * label13;
  GtkWidget * label14;
  GtkWidget * label15;
  GtkWidget * label16;
  GtkWidget * vbox3;
  GtkWidget * label2;
  GtkWidget * hbuttonbox1;
  GtkWidget * hseparator1;
  GtkAccelGroup * accel_group;

  accel_group=gtk_accel_group_new();

  Equalizer=gtk_window_new( GTK_WINDOW_DIALOG );
  gtk_widget_set_name( Equalizer,MSGTR_Equalizer );
  gtk_object_set_data( GTK_OBJECT( Equalizer ),MSGTR_Equalizer,Equalizer );
  gtk_widget_set_usize( Equalizer,550,256 );
  gtk_window_set_title( GTK_WINDOW( Equalizer ),MSGTR_Equalizer );
  gtk_window_set_position( GTK_WINDOW( Equalizer ),GTK_WIN_POS_CENTER );
  gtk_window_set_policy( GTK_WINDOW( Equalizer ),FALSE,FALSE,FALSE );
  gtk_window_set_wmclass( GTK_WINDOW( Equalizer ),MSGTR_Equalizer,"MPlayer" );

  gtk_widget_realize( Equalizer );
  gtkAddIcon( Equalizer );
    
  frame1=gtk_frame_new( NULL );
  gtk_widget_set_name( frame1,"frame1" );
  gtk_widget_ref( frame1 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"frame1",frame1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( frame1 );
  gtk_container_add( GTK_CONTAINER( Equalizer ),frame1 );
  gtk_container_set_border_width( GTK_CONTAINER( frame1 ),1 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame1 ),GTK_SHADOW_IN );

  frame2=gtk_frame_new( NULL );
  gtk_widget_set_name( frame2,"frame2" );
  gtk_widget_ref( frame2 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"frame2",frame2,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( frame2 );
  gtk_container_add( GTK_CONTAINER( frame1 ),frame2 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame2 ),GTK_SHADOW_NONE );

  frame3=gtk_frame_new( NULL );
  gtk_widget_set_name( frame3,"frame3" );
  gtk_widget_ref( frame3 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"frame3",frame3,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( frame3 );
  gtk_container_add( GTK_CONTAINER( frame2 ),frame3 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame3 ),GTK_SHADOW_ETCHED_OUT );

  frame4=gtk_frame_new( NULL );
  gtk_widget_set_name( frame4,"frame4" );
  gtk_widget_ref( frame4 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"frame4",frame4,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( frame4 );
  gtk_container_add( GTK_CONTAINER( frame3 ),frame4 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame4 ),GTK_SHADOW_NONE );

  vbox1=gtk_vbox_new( FALSE,0 );
  gtk_widget_set_name( vbox1,"vbox1" );
  gtk_widget_ref( vbox1 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"vbox1",vbox1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( vbox1 );
  gtk_container_add( GTK_CONTAINER( frame4 ),vbox1 );

  Notebook=gtk_notebook_new();
  gtk_widget_set_name( Notebook,"Notebook" );
  gtk_widget_ref( Notebook );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"Notebook",Notebook,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( Notebook );
  gtk_box_pack_start( GTK_BOX( vbox1 ),Notebook,TRUE,TRUE,0 );
  gtk_container_set_border_width( GTK_CONTAINER( Notebook ),1 );

  hbox1=gtk_hbox_new( FALSE,0 );
  gtk_widget_set_name( hbox1,"hbox1" );
  gtk_widget_ref( hbox1 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"hbox1",hbox1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( hbox1 );
  gtk_container_add( GTK_CONTAINER( Notebook ),hbox1 );

  scrolledwindow1=gtk_scrolled_window_new( NULL,NULL );
  gtk_widget_set_name( scrolledwindow1,"scrolledwindow1" );
  gtk_widget_ref( scrolledwindow1 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"scrolledwindow1",scrolledwindow1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( scrolledwindow1 );
  gtk_box_pack_start( GTK_BOX( hbox1 ),scrolledwindow1,FALSE,FALSE,0 );
  gtk_widget_set_usize( scrolledwindow1,106,-2 );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolledwindow1 ),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC );

  ChannelsList=gtk_clist_new( 1 );
  gtk_widget_set_name( ChannelsList,"ChannelsList" );
  gtk_widget_ref( ChannelsList );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"ChannelsList",ChannelsList,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( ChannelsList );
  gtk_container_add( GTK_CONTAINER( scrolledwindow1 ),ChannelsList );
  gtk_clist_set_column_width( GTK_CLIST( ChannelsList ),0,80 );
  gtk_clist_column_titles_hide( GTK_CLIST( ChannelsList ) );

  table1=gtk_table_new( 2,10,FALSE );
  gtk_widget_set_name( table1,"table1" );
  gtk_widget_ref( table1 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"table1",table1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( table1 );
  gtk_box_pack_start( GTK_BOX( hbox1 ),table1,FALSE,FALSE,0 );
  gtk_table_set_row_spacings( GTK_TABLE( table1 ),4 );
  gtk_table_set_col_spacings( GTK_TABLE( table1 ),9 );

  A3125adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A3125=gtk_vscale_new( A3125adj );
  gtk_widget_set_name( A3125,"A3125" );
  gtk_widget_ref( A3125 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"A3125",A3125,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( A3125 );
  gtk_table_attach( GTK_TABLE( table1 ),A3125,0,1,0,1,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( GTK_EXPAND | GTK_FILL ),0,0 );
  gtk_scale_set_draw_value( GTK_SCALE( A3125 ),FALSE );

  A6250adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A6250=gtk_vscale_new( A6250adj );
  gtk_widget_set_name( A6250,"A6250" );
  gtk_widget_ref( A6250 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"A6250",A6250,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( A6250 );
  gtk_table_attach( GTK_TABLE( table1 ),A6250,1,2,0,1,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( GTK_EXPAND | GTK_FILL ),0,0 );
  gtk_scale_set_draw_value( GTK_SCALE( A6250 ),FALSE );

  A125adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A125=gtk_vscale_new( A125adj );
  gtk_widget_set_name( A125,"A125" );
  gtk_widget_ref( A125 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"A125",A125,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( A125 );
  gtk_table_attach( GTK_TABLE( table1 ),A125,2,3,0,1,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( GTK_EXPAND | GTK_FILL ),0,0 );
  gtk_scale_set_draw_value( GTK_SCALE( A125 ),FALSE );

  A250adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A250=gtk_vscale_new( A250adj );
  gtk_widget_set_name( A250,"A250" );
  gtk_widget_ref( A250 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"A250",A250,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( A250 );
  gtk_table_attach( GTK_TABLE( table1 ),A250,3,4,0,1,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( GTK_EXPAND | GTK_FILL ),0,0 );
  gtk_scale_set_draw_value( GTK_SCALE( A250 ),FALSE );

  A500adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A500=gtk_vscale_new( A500adj );
  gtk_widget_set_name( A500,"A500" );
  gtk_widget_ref( A500 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"A500",A500,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( A500 );
  gtk_table_attach( GTK_TABLE( table1 ),A500,4,5,0,1,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( GTK_EXPAND | GTK_FILL ),0,0 );
  gtk_scale_set_draw_value( GTK_SCALE( A500 ),FALSE );

  A1000adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A1000=gtk_vscale_new( A1000adj );
  gtk_widget_set_name( A1000,"A1000" );
  gtk_widget_ref( A1000 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"A1000",A1000,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( A1000 );
  gtk_table_attach( GTK_TABLE( table1 ),A1000,5,6,0,1,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( GTK_EXPAND | GTK_FILL ),0,0 );
  gtk_scale_set_draw_value( GTK_SCALE( A1000 ),FALSE );

  A2000adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A2000=gtk_vscale_new( A2000adj );
  gtk_widget_set_name( A2000,"A2000" );
  gtk_widget_ref( A2000 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"A2000",A2000,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( A2000 );
  gtk_table_attach( GTK_TABLE( table1 ),A2000,6,7,0,1,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( GTK_EXPAND | GTK_FILL ),0,0 );
  gtk_scale_set_draw_value( GTK_SCALE( A2000 ),FALSE );

  A4000adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A4000=gtk_vscale_new( A4000adj );
  gtk_widget_set_name( A4000,"A4000" );
  gtk_widget_ref( A4000 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"A4000",A4000,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( A4000 );
  gtk_table_attach( GTK_TABLE( table1 ),A4000,7,8,0,1,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( GTK_EXPAND | GTK_FILL ),0,0 );
  gtk_scale_set_draw_value( GTK_SCALE( A4000 ),FALSE );

  A8000adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A8000=gtk_vscale_new( A8000adj );
  gtk_widget_set_name( A8000,"A8000" );
  gtk_widget_ref( A8000 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"A8000",A8000,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( A8000 );
  gtk_table_attach( GTK_TABLE( table1 ),A8000,8,9,0,1,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( GTK_EXPAND | GTK_FILL ),0,0 );
  gtk_scale_set_draw_value( GTK_SCALE( A8000 ),FALSE );

  A16000adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A16000=gtk_vscale_new( A16000adj );
  gtk_widget_set_name( A16000,"A16000" );
  gtk_widget_ref( A16000 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"A16000",A16000,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( A16000 );
  gtk_table_attach( GTK_TABLE( table1 ),A16000,9,10,0,1,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( GTK_EXPAND | GTK_FILL ),0,0 );
  gtk_scale_set_draw_value( GTK_SCALE( A16000 ),FALSE );

  label3=gtk_label_new( "31.25" );
  gtk_widget_set_name( label3,"label3" );
  gtk_widget_ref( label3 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"label3",label3,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label3 );
  gtk_table_attach( GTK_TABLE( table1 ),label3,0,1,1,2,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( 0 ),0,0 );
  gtk_misc_set_alignment( GTK_MISC( label3 ),0,0.5 );
  gtk_misc_set_padding( GTK_MISC( label3 ),2,0 );

  label4=gtk_label_new( "62.50" );
  gtk_widget_set_name( label4,"label4" );
  gtk_widget_ref( label4 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"label4",label4,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label4 );
  gtk_table_attach( GTK_TABLE( table1 ),label4,1,2,1,2,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( 0 ),0,0 );
  gtk_misc_set_alignment( GTK_MISC( label4 ),0,0.5 );
  gtk_misc_set_padding( GTK_MISC( label4 ),1,0 );

  label5=gtk_label_new( "125" );
  gtk_widget_set_name( label5,"label5" );
  gtk_widget_ref( label5 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"label5",label5,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label5 );
  gtk_table_attach( GTK_TABLE( table1 ),label5,2,3,1,2,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( 0 ),0,0 );
  gtk_misc_set_alignment( GTK_MISC( label5 ),0,0.5 );
  gtk_misc_set_padding( GTK_MISC( label5 ),5,0 );

  label6=gtk_label_new( "250" );
  gtk_widget_set_name( label6,"label6" );
  gtk_widget_ref( label6 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"label6",label6,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label6 );
  gtk_table_attach( GTK_TABLE( table1 ),label6,3,4,1,2,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( 0 ),0,0 );
  gtk_misc_set_alignment( GTK_MISC( label6 ),0,0.5 );
  gtk_misc_set_padding( GTK_MISC( label6 ),5,0 );

  label7=gtk_label_new( "500" );
  gtk_widget_set_name( label7,"label7" );
  gtk_widget_ref( label7 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"label7",label7,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label7 );
  gtk_table_attach( GTK_TABLE( table1 ),label7,4,5,1,2,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( 0 ),0,0 );
  gtk_misc_set_alignment( GTK_MISC( label7 ),0,0.5 );
  gtk_misc_set_padding( GTK_MISC( label7 ),7,0 );

  label8=gtk_label_new( "1000" );
  gtk_widget_set_name( label8,"label8" );
  gtk_widget_ref( label8 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"label8",label8,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label8 );
  gtk_table_attach( GTK_TABLE( table1 ),label8,5,6,1,2,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( 0 ),0,0 );
  gtk_misc_set_alignment( GTK_MISC( label8 ),0,0.5 );
  gtk_misc_set_padding( GTK_MISC( label8 ),5,0 );

  label9=gtk_label_new( "2000" );
  gtk_widget_set_name( label9,"label9" );
  gtk_widget_ref( label9 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"label9",label9,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label9 );
  gtk_table_attach( GTK_TABLE( table1 ),label9,6,7,1,2,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( 0 ),0,0 );
  gtk_misc_set_alignment( GTK_MISC( label9 ),0,0.5 );
  gtk_misc_set_padding( GTK_MISC( label9 ),2,0 );

  label10=gtk_label_new( "4000" );
  gtk_widget_set_name( label10,"label10" );
  gtk_widget_ref( label10 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"label10",label10,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label10 );
  gtk_table_attach( GTK_TABLE( table1 ),label10,7,8,1,2,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( 0 ),0,0 );
  gtk_misc_set_alignment( GTK_MISC( label10 ),0,0.5 );
  gtk_misc_set_padding( GTK_MISC( label10 ),3,0 );

  label11=gtk_label_new( "8000" );
  gtk_widget_set_name( label11,"label11" );
  gtk_widget_ref( label11 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"label11",label11,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label11 );
  gtk_table_attach( GTK_TABLE( table1 ),label11,8,9,1,2,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( 0 ),0,0 );
  gtk_misc_set_alignment( GTK_MISC( label11 ),0,0.5 );
  gtk_misc_set_padding( GTK_MISC( label11 ),1,0 );

  label12=gtk_label_new( "16000" );
  gtk_widget_set_name( label12,"label12" );
  gtk_widget_ref( label12 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"label12",label12,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label12 );
  gtk_table_attach( GTK_TABLE( table1 ),label12,9,10,1,2,( GtkAttachOptions )( GTK_FILL ),( GtkAttachOptions )( 0 ),0,0 );
  gtk_misc_set_alignment( GTK_MISC( label12 ),0,0.5 );

  label1=gtk_label_new( MSGTR_EQU_Audio );
  gtk_widget_set_name( label1,"label1" );
  gtk_widget_ref( label1 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"label1",label1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label1 );
  gtk_notebook_set_tab_label( GTK_NOTEBOOK( Notebook ),gtk_notebook_get_nth_page( GTK_NOTEBOOK( Notebook ),0 ),label1 );

  hbox2=gtk_hbox_new( FALSE,0 );
  gtk_widget_set_name( hbox2,"hbox2" );
  gtk_widget_ref( hbox2 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"hbox2",hbox2,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( hbox2 );
  gtk_container_add( GTK_CONTAINER( Notebook ),hbox2 );

  vbox2=gtk_vbox_new( TRUE,0 );
  gtk_widget_set_name( vbox2,"vbox2" );
  gtk_widget_ref( vbox2 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"vbox2",vbox2,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( vbox2 );
  gtk_box_pack_start( GTK_BOX( hbox2 ),vbox2,FALSE,FALSE,0 );

  label13=gtk_label_new( MSGTR_EQU_Contrast );
  gtk_widget_set_name( label13,"label13" );
  gtk_widget_ref( label13 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"label13",label13,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label13 );
  gtk_box_pack_start( GTK_BOX( vbox2 ),label13,FALSE,FALSE,0 );
  gtk_label_set_justify( GTK_LABEL( label13 ),GTK_JUSTIFY_LEFT );
  gtk_misc_set_alignment( GTK_MISC( label13 ),0.02,0.5 );

  label14=gtk_label_new( MSGTR_EQU_Brightness );
  gtk_widget_set_name( label14,"label14" );
  gtk_widget_ref( label14 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"label14",label14,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label14 );
  gtk_box_pack_start( GTK_BOX( vbox2 ),label14,FALSE,FALSE,0 );
  gtk_label_set_justify( GTK_LABEL( label14 ),GTK_JUSTIFY_LEFT );
  gtk_misc_set_alignment( GTK_MISC( label14 ),0.02,0.5 );

  label15=gtk_label_new( MSGTR_EQU_Hue );
  gtk_widget_set_name( label15,"label15" );
  gtk_widget_ref( label15 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"label15",label15,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label15 );
  gtk_box_pack_start( GTK_BOX( vbox2 ),label15,FALSE,FALSE,0 );
  gtk_label_set_justify( GTK_LABEL( label15 ),GTK_JUSTIFY_LEFT );
  gtk_misc_set_alignment( GTK_MISC( label15 ),0.02,0.5 );

  label16=gtk_label_new( MSGTR_EQU_Saturation );
  gtk_widget_set_name( label16,"label16" );
  gtk_widget_ref( label16 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"label16",label16,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label16 );
  gtk_box_pack_start( GTK_BOX( vbox2 ),label16,FALSE,FALSE,0 );
  gtk_label_set_justify( GTK_LABEL( label16 ),GTK_JUSTIFY_LEFT );
  gtk_misc_set_alignment( GTK_MISC( label16 ),0.02,0.5 );

  vbox3=gtk_vbox_new( TRUE,0 );
  gtk_widget_set_name( vbox3,"vbox3" );
  gtk_widget_ref( vbox3 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"vbox3",vbox3,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( vbox3 );
  gtk_box_pack_start( GTK_BOX( hbox2 ),vbox3,TRUE,TRUE,0 );

  VContrastadj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-100,100,1,0,0 ) );
  VContrast=gtk_hscale_new( VContrastadj );
  gtk_widget_set_name( VContrast,"VContrast" );
  gtk_widget_ref( VContrast );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"VContrast",VContrast,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( VContrast );
  gtk_box_pack_start( GTK_BOX( vbox3 ),VContrast,TRUE,TRUE,0 );
  gtk_scale_set_value_pos( GTK_SCALE( VContrast ),GTK_POS_RIGHT );

  VBrightnessadj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-100,100,1,0,0 ) );
  VBrightness=gtk_hscale_new( VBrightnessadj );
  gtk_widget_set_name( VBrightness,"VBrightness" );
  gtk_widget_ref( VBrightness );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"VBrightness",VBrightness,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( VBrightness );
  gtk_box_pack_start( GTK_BOX( vbox3 ),VBrightness,TRUE,TRUE,0 );
  gtk_scale_set_value_pos( GTK_SCALE( VBrightness ),GTK_POS_RIGHT );

  VHueadj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-100,100,1,0,0 ) );
  VHue=gtk_hscale_new( VHueadj );
  gtk_widget_set_name( VHue,"VHue" );
  gtk_widget_ref( VHue );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"VHue",VHue,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( VHue );
  gtk_box_pack_start( GTK_BOX( vbox3 ),VHue,TRUE,TRUE,0 );
  gtk_scale_set_value_pos( GTK_SCALE( VHue ),GTK_POS_RIGHT );

  VSaturationadj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-100,100,1,0,0 ) );
  VSaturation=gtk_hscale_new( VSaturationadj );
  gtk_widget_set_name( VSaturation,"VSaturation" );
  gtk_widget_ref( VSaturation );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"VSaturation",VSaturation,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( VSaturation );
  gtk_box_pack_start( GTK_BOX( vbox3 ),VSaturation,TRUE,TRUE,0 );
  gtk_scale_set_value_pos( GTK_SCALE( VSaturation ),GTK_POS_RIGHT );

  label2=gtk_label_new( MSGTR_EQU_Video );
  gtk_widget_set_name( label2,"label2" );
  gtk_widget_ref( label2 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"label2",label2,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label2 );
  gtk_notebook_set_tab_label( GTK_NOTEBOOK( Notebook ),gtk_notebook_get_nth_page( GTK_NOTEBOOK( Notebook ),1 ),label2 );

  hbuttonbox1=gtk_hbutton_box_new();
  gtk_widget_set_name( hbuttonbox1,"hbuttonbox1" );
  gtk_widget_ref( hbuttonbox1 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"hbuttonbox1",hbuttonbox1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( hbuttonbox1 );
  gtk_box_pack_end( GTK_BOX( vbox1 ),hbuttonbox1,FALSE,TRUE,0 );
  gtk_button_box_set_layout( GTK_BUTTON_BOX( hbuttonbox1 ),GTK_BUTTONBOX_END );
  gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox1 ),10 );
  gtk_button_box_set_child_size( GTK_BUTTON_BOX( hbuttonbox1 ),85,20 );

  Config=gtk_button_new_with_label( MSGTR_Config );
  gtk_widget_set_name( Config,"Config" );
  gtk_widget_ref( Config );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"Config",Config,(GtkDestroyNotify)gtk_widget_unref );
  gtk_container_add( GTK_CONTAINER( hbuttonbox1 ),Config );

  Clear=gtk_button_new_with_label( MSGTR_Clear );
  gtk_widget_set_name( Clear,"Clear" );
  gtk_widget_ref( Clear );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"Clear",Clear,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( Clear );
  gtk_container_add( GTK_CONTAINER( hbuttonbox1 ),Clear );

  Ok=gtk_button_new_with_label( MSGTR_Ok );
  gtk_widget_set_name( Ok,"Ok" );
  gtk_widget_ref( Ok );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"Ok",Ok,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( Ok );
  gtk_container_add( GTK_CONTAINER( hbuttonbox1 ),Ok );
  gtk_widget_add_accelerator( Ok,"released",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );
  gtk_widget_add_accelerator( Ok,"released",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE );

  hseparator1=gtk_hseparator_new();
  gtk_widget_set_name( hseparator1,"hseparator1" );
  gtk_widget_ref( hseparator1 );
  gtk_object_set_data_full( GTK_OBJECT( Equalizer ),"hseparator1",hseparator1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( hseparator1 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),hseparator1,FALSE,FALSE,0 );
  gtk_widget_set_usize( hseparator1,-2,5 );

  gtk_signal_connect( GTK_OBJECT( Equalizer ),"destroy",GTK_SIGNAL_FUNC( eqDestroy ),NULL );
  gtk_signal_connect( GTK_OBJECT( Equalizer ),"show",GTK_SIGNAL_FUNC( eqShow ),(void *)1 );
  gtk_signal_connect( GTK_OBJECT( Equalizer ),"hide",GTK_SIGNAL_FUNC( eqShow ),(void *)0 );
  gtk_signal_connect( GTK_OBJECT( ChannelsList ),"select_row",GTK_SIGNAL_FUNC( eqSelectChannelsListRow ),NULL );

  gtk_signal_connect( GTK_OBJECT( A3125 ),"motion_notify_event",GTK_SIGNAL_FUNC( eqHScaleMotion ),(void*)0 );
  gtk_signal_connect( GTK_OBJECT( A6250 ),"motion_notify_event",GTK_SIGNAL_FUNC( eqHScaleMotion ),(void*)1 );
  gtk_signal_connect( GTK_OBJECT( A125 ),"motion_notify_event",GTK_SIGNAL_FUNC( eqHScaleMotion ),(void*)2 );
  gtk_signal_connect( GTK_OBJECT( A250 ),"motion_notify_event",GTK_SIGNAL_FUNC( eqHScaleMotion ),(void*)3 );
  gtk_signal_connect( GTK_OBJECT( A500 ),"motion_notify_event",GTK_SIGNAL_FUNC( eqHScaleMotion ),(void*)4 );
  gtk_signal_connect( GTK_OBJECT( A1000 ),"motion_notify_event",GTK_SIGNAL_FUNC( eqHScaleMotion ),(void*)5 );
  gtk_signal_connect( GTK_OBJECT( A2000 ),"motion_notify_event",GTK_SIGNAL_FUNC( eqHScaleMotion ),(void*)6 );
  gtk_signal_connect( GTK_OBJECT( A4000 ),"motion_notify_event",GTK_SIGNAL_FUNC( eqHScaleMotion ),(void*)7 );
  gtk_signal_connect( GTK_OBJECT( A8000 ),"motion_notify_event",GTK_SIGNAL_FUNC( eqHScaleMotion ),(void*)8 );
  gtk_signal_connect( GTK_OBJECT( A16000 ),"motion_notify_event",GTK_SIGNAL_FUNC( eqHScaleMotion ),(void*)9 );

  gtk_signal_connect( GTK_OBJECT( VContrast ),"motion_notify_event",GTK_SIGNAL_FUNC( eqVScaleMotion ),(void*)1 );
  gtk_signal_connect( GTK_OBJECT( VBrightness ),"motion_notify_event",GTK_SIGNAL_FUNC( eqVScaleMotion ),(void*)2 );
  gtk_signal_connect( GTK_OBJECT( VHue ),"motion_notify_event",GTK_SIGNAL_FUNC( eqVScaleMotion ),(void*)3 );
  gtk_signal_connect( GTK_OBJECT( VSaturation ),"motion_notify_event",GTK_SIGNAL_FUNC( eqVScaleMotion ),(void *)4 );
  
  gtk_signal_connect( GTK_OBJECT( Ok ),"released",GTK_SIGNAL_FUNC( eqButtonReleased ),(void *)0 );
  gtk_signal_connect( GTK_OBJECT( Clear ),"released",GTK_SIGNAL_FUNC( eqButtonReleased ),(void *)1 );
  gtk_signal_connect( GTK_OBJECT( Config ),"released",GTK_SIGNAL_FUNC( eqButtonReleased ),(void *)2 );

  gtk_signal_connect( GTK_OBJECT( Notebook ),"switch_page",GTK_SIGNAL_FUNC( eqNotebook ),NULL );

  gtk_window_add_accel_group( GTK_WINDOW( Equalizer ),accel_group );

  return Equalizer;
}

// --- equalizer config dialog box

static GtkWidget * EquConfig;
static GtkWidget * CBChannel1;
static GtkWidget * CEChannel1;
static GtkWidget * CBChannel2;
static GtkWidget * CEChannel2;
static GtkWidget * CBChannel3;
static GtkWidget * CEChannel3;
static GtkWidget * CBChannel4;
static GtkWidget * CEChannel4;
static GtkWidget * CBChannel5;
static GtkWidget * CEChannel5;
static GtkWidget * CBChannel6;
static GtkWidget * CEChannel6;
static GtkWidget * ecOk;
static GtkWidget * ecCancel;

GtkWidget * create_EquConfig( void );

void ShowEquConfig( void )
{
 GList * Items = NULL;

 if ( gtkVEquConfig ) gtkActive( EquConfig );
    else EquConfig=create_EquConfig();
	
 Items=g_list_append( Items,(gpointer)MSGTR_EQU_Front_Right  );
 Items=g_list_append( Items,(gpointer)MSGTR_EQU_Front_Left );
 Items=g_list_append( Items,(gpointer)MSGTR_EQU_Back_Right );
 Items=g_list_append( Items,(gpointer)MSGTR_EQU_Back_Left );
 Items=g_list_append( Items,(gpointer)MSGTR_EQU_Center );
 Items=g_list_append( Items,(gpointer)MSGTR_EQU_Bass );
 
 gtk_combo_set_popdown_strings( GTK_COMBO( CBChannel1 ),Items );
 gtk_combo_set_popdown_strings( GTK_COMBO( CBChannel2 ),Items );
 gtk_combo_set_popdown_strings( GTK_COMBO( CBChannel3 ),Items );
 gtk_combo_set_popdown_strings( GTK_COMBO( CBChannel4 ),Items );
 gtk_combo_set_popdown_strings( GTK_COMBO( CBChannel5 ),Items );
 gtk_combo_set_popdown_strings( GTK_COMBO( CBChannel6 ),Items );

 g_list_free( Items );

 gtk_entry_set_text( GTK_ENTRY( CEChannel1 ),gtkEquChannel1 ); gtk_entry_set_editable( GTK_ENTRY( CEChannel1 ),FALSE );
 gtk_entry_set_text( GTK_ENTRY( CEChannel2 ),gtkEquChannel2 ); gtk_entry_set_editable( GTK_ENTRY( CEChannel2 ),FALSE );
 gtk_entry_set_text( GTK_ENTRY( CEChannel3 ),gtkEquChannel3 ); gtk_entry_set_editable( GTK_ENTRY( CEChannel3 ),FALSE );
 gtk_entry_set_text( GTK_ENTRY( CEChannel4 ),gtkEquChannel4 ); gtk_entry_set_editable( GTK_ENTRY( CEChannel4 ),FALSE );
 gtk_entry_set_text( GTK_ENTRY( CEChannel5 ),gtkEquChannel5 ); gtk_entry_set_editable( GTK_ENTRY( CEChannel5 ),FALSE );
 gtk_entry_set_text( GTK_ENTRY( CEChannel6 ),gtkEquChannel6 ); gtk_entry_set_editable( GTK_ENTRY( CEChannel6 ),FALSE );

 gtk_widget_show( EquConfig );
 gtkSetLayer( EquConfig );
}

void HideEquConfig( void )
{ gtk_widget_hide( EquConfig ); gtk_widget_destroy( EquConfig ); gtkVEquConfig=0; }

static void ecHandler( GtkObject * object,gpointer user_data )
{
 switch ( (int)user_data )
 {
   case 0: HideEquConfig(); break;
   case 1: gtkVEquConfig=1; break;
   case 2: gtkVEquConfig=0; break;
 }
}

static void ecButtonReleased( GtkButton * button,gpointer user_data )
{
 if ( (int)user_data )
 { // if you pressed Ok
  if ( gtkEquChannel1) free( gtkEquChannel1 ); gtkEquChannel1=strdup( gtk_entry_get_text( GTK_ENTRY( CEChannel1 ) ) );
  if ( gtkEquChannel2) free( gtkEquChannel2 ); gtkEquChannel2=strdup( gtk_entry_get_text( GTK_ENTRY( CEChannel2 ) ) );
  if ( gtkEquChannel3) free( gtkEquChannel3 ); gtkEquChannel3=strdup( gtk_entry_get_text( GTK_ENTRY( CEChannel3 ) ) );
  if ( gtkEquChannel4) free( gtkEquChannel4 ); gtkEquChannel4=strdup( gtk_entry_get_text( GTK_ENTRY( CEChannel4 ) ) );
  if ( gtkEquChannel5) free( gtkEquChannel5 ); gtkEquChannel5=strdup( gtk_entry_get_text( GTK_ENTRY( CEChannel5 ) ) );
  if ( gtkEquChannel6) free( gtkEquChannel6 ); gtkEquChannel6=strdup( gtk_entry_get_text( GTK_ENTRY( CEChannel6 ) ) );
  eqSetChannelNames();
 }
 HideEquConfig();
}

GtkWidget * create_EquConfig( void )
{
  GtkWidget * frame1;
  GtkWidget * frame2;
  GtkWidget * frame3;
  GtkWidget * frame4;
  GtkWidget * vbox1;
  GtkWidget * table1;
  GtkWidget * label1;
  GtkWidget * label2;
  GtkWidget * label3;
  GtkWidget * label4;
  GtkWidget * label5;
  GtkWidget * label6;
  GtkWidget * hseparator1;
  GtkWidget * hbuttonbox1;
  GtkAccelGroup * accel_group;

  accel_group=gtk_accel_group_new();

  EquConfig=gtk_window_new( GTK_WINDOW_DIALOG );
  gtk_widget_set_name( EquConfig,"EquConfig" );
  gtk_object_set_data( GTK_OBJECT( EquConfig ),"EquConfig",EquConfig );
  gtk_widget_set_usize( EquConfig,350,198 );
  GTK_WIDGET_SET_FLAGS( EquConfig,GTK_CAN_DEFAULT );
  gtk_window_set_title( GTK_WINDOW( EquConfig ),"Configure Equalizer" );
  gtk_window_set_position( GTK_WINDOW( EquConfig ),GTK_WIN_POS_CENTER );
//  gtk_window_set_modal( GTK_WINDOW( EquConfig ),TRUE );
  gtk_window_set_policy( GTK_WINDOW( EquConfig ),FALSE,FALSE,FALSE );
  gtk_window_set_wmclass( GTK_WINDOW( EquConfig ),"EquConfig","MPlayer" );

  gtk_widget_realize( EquConfig );
  gtkAddIcon( EquConfig );

  frame1=gtk_frame_new( NULL );
  gtk_widget_set_name( frame1,"frame1" );
  gtk_widget_ref( frame1 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"frame1",frame1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( frame1 );
  gtk_container_add( GTK_CONTAINER( EquConfig ),frame1 );
  gtk_container_set_border_width( GTK_CONTAINER( frame1 ),1 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame1 ),GTK_SHADOW_IN );

  frame2=gtk_frame_new( NULL );
  gtk_widget_set_name( frame2,"frame2" );
  gtk_widget_ref( frame2 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"frame2",frame2,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( frame2 );
  gtk_container_add( GTK_CONTAINER( frame1 ),frame2 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame2 ),GTK_SHADOW_NONE );

  frame3=gtk_frame_new( NULL );
  gtk_widget_set_name( frame3,"frame3" );
  gtk_widget_ref( frame3 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"frame3",frame3,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( frame3 );
  gtk_container_add( GTK_CONTAINER( frame2 ),frame3 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame3 ),GTK_SHADOW_ETCHED_OUT );

  frame4=gtk_frame_new( NULL );
  gtk_widget_set_name( frame4,"frame4" );
  gtk_widget_ref( frame4 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"frame4",frame4,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( frame4 );
  gtk_container_add( GTK_CONTAINER( frame3 ),frame4 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame4 ),GTK_SHADOW_NONE );

  vbox1=gtk_vbox_new( FALSE,0 );
  gtk_widget_set_name( vbox1,"vbox1" );
  gtk_widget_ref( vbox1 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"vbox1",vbox1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( vbox1 );
  gtk_container_add( GTK_CONTAINER( frame4 ),vbox1 );

  table1=gtk_table_new( 6,2,FALSE );
  gtk_widget_set_name( table1,"table1" );
  gtk_widget_ref( table1 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"table1",table1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( table1 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),table1,TRUE,TRUE,0 );
  gtk_table_set_row_spacings( GTK_TABLE( table1 ),4 );
  gtk_table_set_col_spacings( GTK_TABLE( table1 ),4 );

  label1=gtk_label_new( "Channel 1:" );
  gtk_widget_set_name( label1,"label1" );
  gtk_widget_ref( label1 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"label1",label1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label1 );
  gtk_table_attach( GTK_TABLE( table1 ),label1,0,1,0,1,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  gtk_misc_set_alignment( GTK_MISC( label1 ),0,0.5 );

  label2=gtk_label_new( "Channel 2:" );
  gtk_widget_set_name( label2,"label2" );
  gtk_widget_ref( label2 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"label2",label2,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label2 );
  gtk_table_attach( GTK_TABLE( table1 ),label2,0,1,1,2,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  gtk_misc_set_alignment( GTK_MISC( label2 ),0,0.5 );

  label3=gtk_label_new( "Channel 3:" );
  gtk_widget_set_name( label3,"label3" );
  gtk_widget_ref( label3 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"label3",label3,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label3 );
  gtk_table_attach( GTK_TABLE( table1 ),label3,0,1,2,3,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  gtk_misc_set_alignment( GTK_MISC( label3 ),0,0.5 );

  label4=gtk_label_new( "Channel 4:" );
  gtk_widget_set_name( label4,"label4" );
  gtk_widget_ref( label4 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"label4",label4,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label4 );
  gtk_table_attach( GTK_TABLE( table1 ),label4,0,1,3,4,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  gtk_misc_set_alignment( GTK_MISC( label4 ),0,0.5 );

  label5=gtk_label_new( "Channel 5:" );
  gtk_widget_set_name( label5,"label5" );
  gtk_widget_ref( label5 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"label5",label5,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label5 );
  gtk_table_attach( GTK_TABLE( table1 ),label5,0,1,4,5,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  gtk_misc_set_alignment( GTK_MISC( label5 ),0,0.5 );

  label6=gtk_label_new( "Channel 6:" );
  gtk_widget_set_name( label6,"label6" );
  gtk_widget_ref( label6 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"label6",label6,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label6 );
  gtk_table_attach( GTK_TABLE( table1 ),label6,0,1,5,6,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  gtk_misc_set_alignment( GTK_MISC( label6 ),0,0.5 );

  CBChannel1=gtk_combo_new();
  gtk_widget_set_name( CBChannel1,"CBChannel1" );
  gtk_widget_ref( CBChannel1 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"CBChannel1",CBChannel1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CBChannel1 );
  gtk_table_attach( GTK_TABLE( table1 ),CBChannel1,1,2,0,1,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  CEChannel1=GTK_COMBO( CBChannel1 )->entry;
  gtk_widget_set_name( CEChannel1,"CEChannel1" );
  gtk_widget_ref( CEChannel1 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"CEChannel1",CEChannel1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CEChannel1 );

  CBChannel2=gtk_combo_new();
  gtk_widget_set_name( CBChannel2,"CBChannel2" );
  gtk_widget_ref( CBChannel2 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"CBChannel2",CBChannel2,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CBChannel2 );
  gtk_table_attach( GTK_TABLE( table1 ),CBChannel2,1,2,1,2,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  
  CEChannel2=GTK_COMBO( CBChannel2 )->entry;
  gtk_widget_set_name( CEChannel2,"CEChannel2" );
  gtk_widget_ref( CEChannel2 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"CEChannel2",CEChannel2,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CEChannel2 );

  CBChannel3=gtk_combo_new();
  gtk_widget_set_name( CBChannel3,"CBChannel3" );
  gtk_widget_ref( CBChannel3 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"CBChannel3",CBChannel3,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CBChannel3 );
  gtk_table_attach( GTK_TABLE( table1 ),CBChannel3,1,2,2,3,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  
  CEChannel3=GTK_COMBO( CBChannel3 )->entry;
  gtk_widget_set_name( CEChannel3,"CEChannel3" );
  gtk_widget_ref( CEChannel3 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"CEChannel3",CEChannel3,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CEChannel3 );

  CBChannel4=gtk_combo_new();
  gtk_widget_set_name( CBChannel4,"CBChannel4" );
  gtk_widget_ref( CBChannel4 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"CBChannel4",CBChannel4,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CBChannel4 );
  gtk_table_attach( GTK_TABLE( table1 ),CBChannel4,1,2,3,4,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  
  CEChannel4=GTK_COMBO( CBChannel4 )->entry;
  gtk_widget_set_name( CEChannel4,"CEChannel4" );
  gtk_widget_ref( CEChannel4 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"CEChannel4",CEChannel4,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CEChannel4 );

  CBChannel5=gtk_combo_new();
  gtk_widget_set_name( CBChannel5,"CBChannel5" );
  gtk_widget_ref( CBChannel5 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"CBChannel5",CBChannel5,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CBChannel5 );
  gtk_table_attach( GTK_TABLE( table1 ),CBChannel5,1,2,4,5,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  
  CEChannel5=GTK_COMBO( CBChannel5 )->entry;
  gtk_widget_set_name( CEChannel5,"CEChannel5" );
  gtk_widget_ref( CEChannel5 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"CEChannel5",CEChannel5,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CEChannel5 );

  CBChannel6=gtk_combo_new();
  gtk_widget_set_name( CBChannel6,"CBChannel6" );
  gtk_widget_ref( CBChannel6 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"CBChannel6",CBChannel6,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CBChannel6 );
  gtk_table_attach( GTK_TABLE( table1 ),CBChannel6,1,2,5,6,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  
  CEChannel6=GTK_COMBO( CBChannel6 )->entry;
  gtk_widget_set_name( CEChannel6,"CEChannel6" );
  gtk_widget_ref( CEChannel6 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"CEChannel6",CEChannel6,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CEChannel6 );

  hseparator1=gtk_hseparator_new();
  gtk_widget_set_name( hseparator1,"hseparator1" );
  gtk_widget_ref( hseparator1 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"hseparator1",hseparator1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( hseparator1 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),hseparator1,FALSE,FALSE,0 );
  gtk_widget_set_usize( hseparator1,-2,6 );

  hbuttonbox1=gtk_hbutton_box_new();
  gtk_widget_set_name( hbuttonbox1,"hbuttonbox1" );
  gtk_widget_ref( hbuttonbox1 );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"hbuttonbox1",hbuttonbox1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( hbuttonbox1 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),hbuttonbox1,FALSE,FALSE,0 );
  gtk_button_box_set_layout( GTK_BUTTON_BOX( hbuttonbox1 ),GTK_BUTTONBOX_END );
  gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox1 ),10 );
  gtk_button_box_set_child_size( GTK_BUTTON_BOX( hbuttonbox1 ),-1,20 );
  gtk_button_box_set_child_ipadding( GTK_BUTTON_BOX( hbuttonbox1 ),0,-1 );

  ecOk=gtk_button_new_with_label( "Ok" );
  gtk_widget_set_name( ecOk,"Ok" );
  gtk_widget_ref( ecOk );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"ecOk",ecOk,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( ecOk );
  gtk_container_add( GTK_CONTAINER( hbuttonbox1 ),ecOk );
  GTK_WIDGET_UNSET_FLAGS( ecOk,GTK_CAN_FOCUS );
  gtk_widget_add_accelerator( ecOk,"released",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE );

  ecCancel=gtk_button_new_with_label( "Cancel" );
  gtk_widget_set_name( ecCancel,"Cancel" );
  gtk_widget_ref( ecCancel );
  gtk_object_set_data_full( GTK_OBJECT( EquConfig ),"ecCancel",ecCancel,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( ecCancel );
  gtk_container_add( GTK_CONTAINER( hbuttonbox1 ),ecCancel );
  GTK_WIDGET_UNSET_FLAGS( ecCancel,GTK_CAN_FOCUS );
  gtk_widget_add_accelerator( ecCancel,"released",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );

  gtk_signal_connect( GTK_OBJECT( EquConfig ),"destroy",GTK_SIGNAL_FUNC( ecHandler ),(void *)0 );
  gtk_signal_connect( GTK_OBJECT( EquConfig ),"show",GTK_SIGNAL_FUNC( ecHandler ),(void *)1 );
  gtk_signal_connect( GTK_OBJECT( EquConfig ),"hide",GTK_SIGNAL_FUNC( ecHandler ),(void *)2 );
  
  gtk_signal_connect( GTK_OBJECT( ecOk ),"released",GTK_SIGNAL_FUNC( ecButtonReleased ),(void *)1 );
  gtk_signal_connect( GTK_OBJECT( ecCancel ),"released",GTK_SIGNAL_FUNC( ecButtonReleased ),(void *)0 );

  gtk_window_add_accel_group( GTK_WINDOW( EquConfig ),accel_group );

  return EquConfig;
}

