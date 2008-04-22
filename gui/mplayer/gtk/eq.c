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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "config.h"
#include "gui/app.h"
#include "gui/cfg.h"
#include "help_mp.h"
#include "libaf/equalizer.h"
#include "libvo/video_out.h"
#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "libmpcodecs/dec_video.h"
#include "gui/mplayer/widgets.h"

#include "eq.h"
#include "gtk_common.h"

#define eqRange 15

GtkWidget * Equalizer = NULL;
static GtkWidget * EquConfig;

static GtkWidget * Notebook;
static GtkWidget * ChannelsList;
static GtkWidget * VContrast, * VBrightness, * VHue, * VSaturation;
static GtkAdjustment * VContrastadj, * VBrightnessadj, * VHueadj, * VSaturationadj;
static GtkWidget * Ok, * Clear, * Config;
static GtkWidget * A3125,  * A125, * A6250, * A250, * A500, * A1000, * A2000, * A4000, * A8000, * A16000;
static GtkAdjustment * A3125adj, * A125adj, * A6250adj, * A250adj, * A500adj, * A1000adj, * A2000adj, * A4000adj, * A8000adj, * A16000adj;

static int Channel = -1;

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

 if ( guiIntfStruct.sh_video )
  {
   get_video_colors( guiIntfStruct.sh_video,"brightness",&vo_gamma_brightness );
   get_video_colors( guiIntfStruct.sh_video,"contrast",&vo_gamma_contrast );
   get_video_colors( guiIntfStruct.sh_video,"hue",&vo_gamma_hue );
   get_video_colors( guiIntfStruct.sh_video,"saturation",&vo_gamma_saturation );
  }
										    
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
 if ( Equalizer ) gtkActive( Equalizer );
    else Equalizer=create_Equalizer();

 if ( !gtkEquChannel1 ) gtkEquChannel1=strdup( MSGTR_EQU_Front_Right );
 if ( !gtkEquChannel2 ) gtkEquChannel2=strdup( MSGTR_EQU_Front_Left );
 if ( !gtkEquChannel3 ) gtkEquChannel3=strdup( MSGTR_EQU_Back_Right );
 if ( !gtkEquChannel4 ) gtkEquChannel4=strdup( MSGTR_EQU_Back_Left );
 if ( !gtkEquChannel5 ) gtkEquChannel5=strdup( MSGTR_EQU_Center );
 if ( !gtkEquChannel6 ) gtkEquChannel6=strdup( MSGTR_EQU_Bass );

 eqSetChannelNames();

 if ( !guiIntfStruct.Playing || !guiIntfStruct.sh_video )
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
}

void HideEqualizer( void )
{
 if ( !Equalizer ) return;
 gtk_widget_hide( Equalizer );
 gtk_widget_destroy( Equalizer );
 Equalizer=NULL;
 if ( EquConfig ) HideEquConfig();
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
	   if ( !guiIntfStruct.Playing ) break;
	   gtkSet( gtkSetContrast,0.0f,NULL );
	   gtkSet( gtkSetBrightness,0.0f,NULL );
	   gtkSet( gtkSetHue,0.0f,NULL );
	   gtkSet( gtkSetSaturation,0.0f,NULL );
	   eqSetBands( Channel );
	  }
	break;
   case 2:
	ShowEquConfig();
	break;
  }
}

static void eqFocus( GtkWindow * window,GtkWidget * widget,gpointer user_data )
{ eqSetBands( Channel ); }

static void eqSelectChannelsListRow( GtkCList * clist,gint row,gint column,GdkEvent * event,gpointer user_data )
{
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
  GtkWidget * vbox1;
  GtkWidget * hbox1;
  GtkWidget * scrolledwindow1;
  GtkWidget * table1;
  GtkWidget * hbuttonbox1;
  GtkAccelGroup * accel_group;

  accel_group=gtk_accel_group_new();

  Equalizer=gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_widget_set_name( Equalizer,MSGTR_Equalizer );
  gtk_object_set_data( GTK_OBJECT( Equalizer ),MSGTR_Equalizer,Equalizer );
  gtk_widget_set_usize( Equalizer,-1,256 );
  gtk_window_set_title( GTK_WINDOW( Equalizer ),MSGTR_Equalizer );
  gtk_window_set_position( GTK_WINDOW( Equalizer ),GTK_WIN_POS_CENTER );
  gtk_window_set_policy( GTK_WINDOW( Equalizer ),FALSE,FALSE,FALSE );
  gtk_window_set_wmclass( GTK_WINDOW( Equalizer ),"Equalizer","MPlayer" );

  gtk_widget_realize( Equalizer );
  gtkAddIcon( Equalizer );

  vbox1=AddVBox( AddDialogFrame( Equalizer ),0 );

  Notebook=gtk_notebook_new();
  gtk_widget_set_name( Notebook,"Notebook" );
  gtk_widget_show( Notebook );
  gtk_box_pack_start( GTK_BOX( vbox1 ),Notebook,TRUE,TRUE,0 );
  gtk_container_set_border_width( GTK_CONTAINER( Notebook ),1 );

  hbox1=AddHBox( Notebook,0 );

  scrolledwindow1=gtk_scrolled_window_new( NULL,NULL );
  gtk_widget_set_name( scrolledwindow1,"scrolledwindow1" );
  gtk_widget_show( scrolledwindow1 );
  gtk_box_pack_start( GTK_BOX( hbox1 ),scrolledwindow1,FALSE,FALSE,0 );
  gtk_widget_set_usize( scrolledwindow1,106,-2 );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolledwindow1 ),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC );

  ChannelsList=gtk_clist_new( 1 );
  gtk_widget_set_name( ChannelsList,"ChannelsList" );
  gtk_widget_show( ChannelsList );
  gtk_container_add( GTK_CONTAINER( scrolledwindow1 ),ChannelsList );
  gtk_clist_set_column_width( GTK_CLIST( ChannelsList ),0,80 );
  gtk_clist_column_titles_hide( GTK_CLIST( ChannelsList ) );

  table1=gtk_table_new( 2,10,FALSE );
  gtk_widget_set_name( table1,"table1" );
  gtk_widget_show( table1 );
  gtk_box_pack_start( GTK_BOX( hbox1 ),table1,FALSE,FALSE,0 );
  gtk_table_set_row_spacings( GTK_TABLE( table1 ),4 );
  gtk_table_set_col_spacings( GTK_TABLE( table1 ),9 );

  A3125adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A3125=AddVScaler( A3125adj,NULL,-1 );
    gtk_table_attach( GTK_TABLE( table1 ),A3125,0,1,0,1,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),0,0 );
  
  A6250adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A6250=AddVScaler( A6250adj,NULL,-1 );
    gtk_table_attach( GTK_TABLE( table1 ),A6250,1,2,0,1,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),0,0 );

  A125adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A125=AddVScaler( A125adj,NULL,-1 );
    gtk_table_attach( GTK_TABLE( table1 ),A125,2,3,0,1,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),0,0 );

  A250adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A250=AddVScaler( A250adj,NULL,-1 );
    gtk_table_attach( GTK_TABLE( table1 ),A250,3,4,0,1,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),0,0 );

  A500adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A500=AddVScaler( A500adj,NULL,-1 );
    gtk_table_attach( GTK_TABLE( table1 ),A500,4,5,0,1,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),0,0 );

  A1000adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A1000=AddVScaler( A1000adj,NULL,-1 );
    gtk_table_attach( GTK_TABLE( table1 ),A1000,5,6,0,1,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),0,0 );

  A2000adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A2000=AddVScaler( A2000adj,NULL,-1 );
    gtk_table_attach( GTK_TABLE( table1 ),A2000,6,7,0,1,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),0,0 );

  A4000adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A4000=AddVScaler( A4000adj,NULL,-1 );
    gtk_table_attach( GTK_TABLE( table1 ),A4000,7,8,0,1,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),0,0 );

  A8000adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A8000=AddVScaler( A8000adj,NULL,-1 );
    gtk_table_attach( GTK_TABLE( table1 ),A8000,8,9,0,1,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),0,0 );

  A16000adj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-eqRange,eqRange,0.5,0,0 ) );
  A16000=AddVScaler( A16000adj,NULL,-1 );
    gtk_table_attach( GTK_TABLE( table1 ),A16000,9,10,0,1,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),0,0 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( "31.25",NULL ),
    0,1,1,2,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( "62.50",NULL ),
    1,2,1,2,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( "125",NULL ),
    2,3,1,2,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( "250",NULL ),
    3,4,1,2,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( "500",NULL ),
    4,5,1,2,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( "1000",NULL ),
    5,6,1,2,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( "2000",NULL ),
    6,7,1,2,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( "4000",NULL ),
    7,8,1,2,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( "8000",NULL ),
    8,9,1,2,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( "16000",NULL ),
    9,10,1,2,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  gtk_notebook_set_tab_label( GTK_NOTEBOOK( Notebook ),gtk_notebook_get_nth_page( GTK_NOTEBOOK( Notebook ),0 ),
    AddLabel( MSGTR_EQU_Audio,NULL ) );

  table1=gtk_table_new( 4,2,FALSE );
  gtk_widget_set_name( table1,"table1" );
  gtk_widget_show( table1 );
  gtk_container_add( GTK_CONTAINER( Notebook ),table1 );
  
  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( MSGTR_EQU_Contrast,NULL ),
    0,1,0,1,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( MSGTR_EQU_Brightness,NULL ),
    0,1,1,2,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( MSGTR_EQU_Hue,NULL ),
    0,1,2,3,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( MSGTR_EQU_Saturation,NULL ),
    0,1,3,4,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  VContrastadj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-100,100,1,0,0 ) );
  VContrast=AddHScaler( VContrastadj,NULL,1 );
    gtk_table_attach( GTK_TABLE( table1 ),VContrast,1,2,0,1,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
    gtk_widget_set_usize( VContrast,-1,45 );

  VBrightnessadj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-100,100,1,0,0 ) );
  VBrightness=AddHScaler( VBrightnessadj,NULL,1 );
    gtk_table_attach( GTK_TABLE( table1 ),VBrightness,1,2,1,2,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
    gtk_widget_set_usize( VBrightness,-1,45 );

  VHueadj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-100,100,1,0,0 ) );
  VHue=AddHScaler( VHueadj,NULL,1 );
    gtk_table_attach( GTK_TABLE( table1 ),VHue,1,2,2,3,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
    gtk_widget_set_usize( VHue,-1,45 );

  VSaturationadj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-100,100,1,0,0 ) );
  VSaturation=AddHScaler( VSaturationadj,NULL,1 );
    gtk_table_attach( GTK_TABLE( table1 ),VSaturation,1,2,3,4,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
    gtk_widget_set_usize( VSaturation,-1,45 );

  gtk_notebook_set_tab_label( GTK_NOTEBOOK( Notebook ),gtk_notebook_get_nth_page( GTK_NOTEBOOK( Notebook ),1 ),
    AddLabel( MSGTR_EQU_Video,NULL ) );

  AddHSeparator( vbox1 );

  hbuttonbox1=AddHButtonBox( vbox1 );
    gtk_button_box_set_layout( GTK_BUTTON_BOX( hbuttonbox1 ),GTK_BUTTONBOX_END );
    gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox1 ),10 );

  Config=AddButton( MSGTR_Config,hbuttonbox1 );
  Clear=AddButton( MSGTR_Clear,hbuttonbox1 );
  Ok=AddButton( MSGTR_Ok,hbuttonbox1 );
  
  gtk_widget_add_accelerator( Ok,"clicked",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );
  gtk_widget_add_accelerator( Ok,"clicked",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE );

  gtk_signal_connect( GTK_OBJECT( Equalizer ),"destroy",GTK_SIGNAL_FUNC( WidgetDestroy ),&Equalizer );
  gtk_signal_connect( GTK_OBJECT( Equalizer ),"focus_in_event",GTK_SIGNAL_FUNC( eqFocus ),(void *)2 );

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
  
  gtk_signal_connect( GTK_OBJECT( Ok ),"clicked",GTK_SIGNAL_FUNC( eqButtonReleased ),(void *)0 );
  gtk_signal_connect( GTK_OBJECT( Clear ),"clicked",GTK_SIGNAL_FUNC( eqButtonReleased ),(void *)1 );
  gtk_signal_connect( GTK_OBJECT( Config ),"clicked",GTK_SIGNAL_FUNC( eqButtonReleased ),(void *)2 );

  gtk_signal_connect( GTK_OBJECT( Notebook ),"switch_page",GTK_SIGNAL_FUNC( eqNotebook ),NULL );

  gtk_window_add_accel_group( GTK_WINDOW( Equalizer ),accel_group );

  return Equalizer;
}

// --- equalizer config dialog box

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

 if ( EquConfig ) gtkActive( EquConfig );
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
{
 if ( !EquConfig ) return;
 gtk_widget_hide( EquConfig );
 gtk_widget_destroy( EquConfig ); 
 EquConfig=NULL;
}

static void ecButtonReleased( GtkButton * button,gpointer user_data )
{
 if ( (int)user_data )
 { // if you pressed Ok
  gfree( (void **)&gtkEquChannel1 ); gtkEquChannel1=gstrdup( gtk_entry_get_text( GTK_ENTRY( CEChannel1 ) ) );
  gfree( (void **)&gtkEquChannel2 ); gtkEquChannel2=gstrdup( gtk_entry_get_text( GTK_ENTRY( CEChannel2 ) ) );
  gfree( (void **)&gtkEquChannel3 ); gtkEquChannel3=gstrdup( gtk_entry_get_text( GTK_ENTRY( CEChannel3 ) ) );
  gfree( (void **)&gtkEquChannel4 ); gtkEquChannel4=gstrdup( gtk_entry_get_text( GTK_ENTRY( CEChannel4 ) ) );
  gfree( (void **)&gtkEquChannel5 ); gtkEquChannel5=gstrdup( gtk_entry_get_text( GTK_ENTRY( CEChannel5 ) ) );
  gfree( (void **)&gtkEquChannel6 ); gtkEquChannel6=gstrdup( gtk_entry_get_text( GTK_ENTRY( CEChannel6 ) ) );
  eqSetChannelNames();
 }
 HideEquConfig();
}

GtkWidget * create_EquConfig( void )
{
  GtkWidget * vbox1;
  GtkWidget * table1;
  GtkWidget * hbuttonbox1;
  GtkAccelGroup * accel_group;

  accel_group=gtk_accel_group_new();

  EquConfig=gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_widget_set_name( EquConfig,"EquConfig" );
  gtk_object_set_data( GTK_OBJECT( EquConfig ),"EquConfig",EquConfig );
  gtk_widget_set_usize( EquConfig,350,260 );
  GTK_WIDGET_SET_FLAGS( EquConfig,GTK_CAN_DEFAULT );
  gtk_window_set_title( GTK_WINDOW( EquConfig ),MSGTR_ConfigureEqualizer );
  gtk_window_set_position( GTK_WINDOW( EquConfig ),GTK_WIN_POS_CENTER );
//  gtk_window_set_modal( GTK_WINDOW( EquConfig ),TRUE );
  gtk_window_set_policy( GTK_WINDOW( EquConfig ),FALSE,FALSE,FALSE );
  gtk_window_set_wmclass( GTK_WINDOW( EquConfig ),"EquConfig","MPlayer" );

  gtk_widget_realize( EquConfig );
  gtkAddIcon( EquConfig );

  vbox1=AddVBox( AddDialogFrame( EquConfig ),0 );

  table1=gtk_table_new( 6,2,FALSE );
  gtk_widget_set_name( table1,"table1" );
  gtk_widget_show( table1 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),table1,TRUE,TRUE,0 );
  gtk_table_set_row_spacings( GTK_TABLE( table1 ),4 );
  gtk_table_set_col_spacings( GTK_TABLE( table1 ),4 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( MSGTR_EQU_Channel1,NULL ),
    0,1,0,1,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( MSGTR_EQU_Channel2,NULL ),
    0,1,1,2,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( MSGTR_EQU_Channel3,NULL ),
    0,1,2,3,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( MSGTR_EQU_Channel4,NULL ),
    0,1,3,4,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( MSGTR_EQU_Channel5,NULL ),
    0,1,4,5,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  gtk_table_attach( GTK_TABLE( table1 ),
    AddLabel( MSGTR_EQU_Channel6,NULL ),
    0,1,5,6,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  CBChannel1=AddComboBox( NULL );
    gtk_table_attach( GTK_TABLE( table1 ),CBChannel1,1,2,0,1,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  CEChannel1=GTK_COMBO( CBChannel1 )->entry;
  gtk_widget_set_name( CEChannel1,"CEChannel1" );
  gtk_widget_show( CEChannel1 );

  CBChannel2=AddComboBox( NULL );
    gtk_table_attach( GTK_TABLE( table1 ),CBChannel2,1,2,1,2,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  
  CEChannel2=GTK_COMBO( CBChannel2 )->entry;
  gtk_widget_set_name( CEChannel2,"CEChannel2" );
  gtk_widget_show( CEChannel2 );

  CBChannel3=AddComboBox( NULL );
    gtk_table_attach( GTK_TABLE( table1 ),CBChannel3,1,2,2,3,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  
  CEChannel3=GTK_COMBO( CBChannel3 )->entry;
  gtk_widget_set_name( CEChannel3,"CEChannel3" );
  gtk_widget_show( CEChannel3 );

  CBChannel4=AddComboBox( NULL );
    gtk_table_attach( GTK_TABLE( table1 ),CBChannel4,1,2,3,4,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  
  CEChannel4=GTK_COMBO( CBChannel4 )->entry;
  gtk_widget_set_name( CEChannel4,"CEChannel4" );
  gtk_widget_show( CEChannel4 );

  CBChannel5=AddComboBox( NULL );
    gtk_table_attach( GTK_TABLE( table1 ),CBChannel5,1,2,4,5,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  
  CEChannel5=GTK_COMBO( CBChannel5 )->entry;
  gtk_widget_set_name( CEChannel5,"CEChannel5" );
  gtk_widget_show( CEChannel5 );

  CBChannel6=AddComboBox( NULL );
    gtk_table_attach( GTK_TABLE( table1 ),CBChannel6,1,2,5,6,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  
  CEChannel6=GTK_COMBO( CBChannel6 )->entry;
  gtk_widget_set_name( CEChannel6,"CEChannel6" );
  gtk_widget_show( CEChannel6 );

  AddHSeparator( vbox1 );

  hbuttonbox1=AddHButtonBox( vbox1 );
    gtk_button_box_set_layout( GTK_BUTTON_BOX( hbuttonbox1 ),GTK_BUTTONBOX_END );
    gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox1 ),10 );

  ecOk=AddButton( MSGTR_Ok,hbuttonbox1 );
  ecCancel=AddButton( MSGTR_Cancel,hbuttonbox1 );

  gtk_widget_add_accelerator( ecOk,"clicked",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE );
  gtk_widget_add_accelerator( ecCancel,"clicked",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );

  gtk_signal_connect( GTK_OBJECT( EquConfig ),"destroy",GTK_SIGNAL_FUNC( WidgetDestroy ),&EquConfig );
  
  gtk_signal_connect( GTK_OBJECT( ecOk ),"clicked",GTK_SIGNAL_FUNC( ecButtonReleased ),(void *)1 );
  gtk_signal_connect( GTK_OBJECT( ecCancel ),"clicked",GTK_SIGNAL_FUNC( ecButtonReleased ),(void *)0 );

  gtk_window_add_accel_group( GTK_WINDOW( EquConfig ),accel_group );

  return EquConfig;
}

