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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gtk_url.h"
#include "gtk_common.h"
#include "gui/interface.h"
#include "gui/app.h"
#include "gui/mplayer/gmplayer.h"
#include "gui/mplayer/widgets.h"
#include "help_mp.h"

GtkWidget * URL = NULL;

static GtkWidget * URLCombo;
static GtkWidget * URLEntry;
static GList     * URLComboEntrys = NULL;

void ShowURLDialogBox( void )
{
 if ( URL ) gtkActive( URL );
   else URL=create_URL();
   
 if ( URLList )
  {
   URLItem * item = URLList;
   g_list_free( URLComboEntrys );
   URLComboEntrys=NULL;
   while( item )
    {
     URLComboEntrys=g_list_append( URLComboEntrys,(gchar *)item->url );
     item=item->next;
    }
  }
   
 if ( URLComboEntrys )
  {
   gtk_entry_set_text( GTK_ENTRY( URLEntry ),URLComboEntrys->data );
   gtk_combo_set_popdown_strings( GTK_COMBO( URLCombo ),URLComboEntrys );
  }
 
 gtk_widget_show( URL );
}

void HideURLDialogBox( void )
{
 if ( !URL ) return;
 gtk_widget_hide( URL );
 gtk_widget_destroy( URL );
 URL=0;
}

static void on_Button_pressed( GtkButton * button,gpointer user_data )
{ 
 URLItem * item;

 if ( (int)user_data )
  {
   gchar * str= strdup( gtk_entry_get_text( GTK_ENTRY( URLEntry ) ) );

   if ( str )
    {
     if ( strncmp( str,"http://",7 )
	&& strncmp( str,"ftp://",6 )
	&& strncmp( str,"mms://",6 )
	&& strncmp( str,"pnm://",6 )
	&& strncmp( str,"rtsp://",7 ) )
      {
       gchar * tmp;
       tmp=malloc( strlen( str ) + 8 );
       sprintf( tmp,"http://%s",str );
       free( str ); str=tmp;
      }
     URLComboEntrys=g_list_prepend( URLComboEntrys,(gchar *)str );
     
     item=calloc( 1,sizeof( URLItem ) );
     item->url=gstrdup( str );
     gtkSet( gtkAddURLItem,0,(void *)item );

     guiSetFilename( guiIntfStruct.Filename,str ); guiIntfStruct.FilenameChanged=1;
     mplEventHandling( evPlayNetwork,0 );
    }
  }
 HideURLDialogBox(); 
}

GtkWidget * create_URL( void )
{
 GtkWidget * vbox1;
 GtkWidget * hbox1;
 GtkWidget * hbuttonbox1;
 GtkWidget * Ok;
 GtkWidget * Cancel;
 GtkAccelGroup * accel_group;

 accel_group=gtk_accel_group_new();

 URL=gtk_window_new( GTK_WINDOW_TOPLEVEL );
 gtk_widget_set_name( URL,"URL" );
 gtk_object_set_data( GTK_OBJECT( URL ),"URL",URL );
 gtk_widget_set_usize( URL,384,70 );
 GTK_WIDGET_SET_FLAGS( URL,GTK_CAN_DEFAULT );
 gtk_window_set_title( GTK_WINDOW( URL ),MSGTR_Network );
 gtk_window_set_position( GTK_WINDOW( URL ),GTK_WIN_POS_CENTER );
 gtk_window_set_policy( GTK_WINDOW( URL ),TRUE,TRUE,FALSE );
 gtk_window_set_wmclass( GTK_WINDOW( URL ),"Network","MPlayer" );
 
 gtk_widget_realize( URL );
 gtkAddIcon( URL );

 vbox1=AddVBox( AddDialogFrame( URL ),0 );
 hbox1=AddHBox( vbox1,1 );
 AddLabel( "URL: ",hbox1 );

 URLCombo=AddComboBox( hbox1 );
/*
 gtk_combo_new();
 gtk_widget_set_name( URLCombo,"URLCombo" );
 gtk_widget_show( URLCombo );
 gtk_box_pack_start( GTK_BOX( hbox1 ),URLCombo,TRUE,TRUE,0 );
*/
 URLEntry=GTK_COMBO( URLCombo )->entry;
 gtk_widget_set_name( URLEntry,"URLEntry" );
 gtk_widget_show( URLEntry );

 AddHSeparator( vbox1 );

 hbuttonbox1=AddHButtonBox( vbox1 );
  gtk_button_box_set_layout( GTK_BUTTON_BOX( hbuttonbox1 ),GTK_BUTTONBOX_END );
  gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox1 ),10 );

 Ok=AddButton( MSGTR_Ok,hbuttonbox1 );
 Cancel=AddButton( MSGTR_Cancel,hbuttonbox1 );
 
 gtk_widget_add_accelerator( Ok,"clicked",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE );
 gtk_widget_add_accelerator( Cancel,"clicked",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );

 gtk_signal_connect( GTK_OBJECT( URL ),"destroy",GTK_SIGNAL_FUNC( WidgetDestroy ),&URL );
 gtk_signal_connect( GTK_OBJECT( Ok ),"clicked",GTK_SIGNAL_FUNC( on_Button_pressed ),(void *)1 );
 gtk_signal_connect( GTK_OBJECT( Cancel ),"clicked",GTK_SIGNAL_FUNC( on_Button_pressed ),NULL );

 gtk_widget_grab_focus( URLEntry );
 gtk_window_add_accel_group( GTK_WINDOW( URL ),accel_group );

 return URL;
}

