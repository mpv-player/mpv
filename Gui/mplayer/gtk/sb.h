
// skin browser

#ifndef __MYSKINBROWSER
#define __MYSKINBROWSER

#include <sys/stat.h>
#include <glob.h>
#include <unistd.h>

#include "../app.h"
#include "../../language.h"

GtkWidget * SkinList;
GtkWidget * sbOk;
char      * sbSelectedSkin=NULL;
char      * sbNotEnoughMemory="SkinBrowser: not enough memory.";
char      * sbMPlayerDirInHome=NULL;
char      * sbMPlayerPrefixDir=NULL;

GtkWidget * sbItemsList[3];
int         sbItemsListCounter = 0;

void HideSkinBrowser( void )
{
 gtk_widget_hide( SkinBrowser );
 gtkVisibleSkinBrowser=0;
 gtkShMem->vs.window=evSkinBrowser;
 gtkSendMessage( evHideWindow );
}

int gtkFillSkinList( gchar * mdir )
{
 gchar         * str[2];
 gchar         * tmp;
 int             i;
 glob_t          gg;
 struct stat     fs;

 if ( ( str[0]=(char *)calloc( 1,7 ) ) == NULL )
  {
   gtkMessageBox( sbNotEnoughMemory );
   return 0;
  }
 str[1]="";
 strcpy( str[0],"default" );
 if ( gtkFindCList( SkinList,str[0] ) == -1 ) gtk_clist_append( GTK_CLIST( SkinList ),str );
 free( str[0] );

 glob( mdir,GLOB_NOSORT,NULL,&gg );
 for( i=0;i<gg.gl_pathc;i++ )
  {
   if ( !strcmp( gg.gl_pathv[i],"." ) || !strcmp( gg.gl_pathv[i],".." ) ) continue;
   stat( gg.gl_pathv[i],&fs );
   if ( S_ISDIR( fs.st_mode ) )
    {
     tmp=strrchr( gg.gl_pathv[i],'/' ); tmp++;
     if ( !strcmp( tmp,"default" ) ) continue;
     if ( ( str[0]=(char *)malloc( strlen( tmp ) + 1 ) ) == NULL ) { gtkMessageBox( sbNotEnoughMemory ); return 0; }
     strcpy( str[0],tmp );
     if ( gtkFindCList( SkinList,str[0] ) == -1 ) gtk_clist_append( GTK_CLIST( SkinList ),str );
     free( str[0] );
    }
  }
 globfree( &gg );
 return 1;
}

void on_SkinBrowser_destroy( GtkObject * object,gpointer user_data )
{ HideSkinBrowser(); }

void on_SkinList_select_row( GtkCList * clist,gint row,gint column,GdkEvent * bevent,gpointer user_data )
{
 gtk_clist_get_text( clist,row,0,&sbSelectedSkin );
 strcpy( gtkShMem->sb.name,sbSelectedSkin );
 gtkSendMessage( evSkinBrowser );
 if( !bevent ) return;
 if( bevent->type == GDK_2BUTTON_PRESS ) HideSkinBrowser();
}

int sbShift = False;

gboolean on_SkinBrowser_key_release_event( GtkWidget * widget,GdkEventKey * event,gpointer user_data )
{
 switch ( event->keyval )
  {
   case GDK_Escape:
   case GDK_Return:
        if ( !sbShift ) HideSkinBrowser();
        break;
   case GDK_Tab:
        if ( sbShift )
         { if ( (--sbItemsListCounter) < 0 ) sbItemsListCounter=2; }
         else
          { if ( (++sbItemsListCounter) > 2 ) sbItemsListCounter=0; }
        gtk_widget_grab_focus( sbItemsList[sbItemsListCounter] );
        break;
   case GDK_Shift_L:
   case GDK_Shift_R:
        sbShift=False;
        break;
  }
// if ( ( event->keyval == GDK_Escape )|| ( event->keyval == GDK_Return ) ) HideSkinBrowser();
 return FALSE;
}

gboolean on_SkinBrowser_key_press_event( GtkWidget * widget,GdkEventKey * event,gpointer user_data )
{
 switch ( event->keyval )
  {
   case GDK_Shift_L:
   case GDK_Shift_R:
        sbShift=True;
        break;
  }
 return FALSE;
}

GtkWidget * create_SkinBrowser( void )
{
 GtkWidget     * SkinBrowser;
 GtkWidget     * frame5;
 GtkWidget     * frame6;
 GtkWidget     * frame7;
 GtkWidget     * frame8;
 GtkWidget     * vbox5;
 GtkWidget     * label;
 GtkWidget     * hseparator4;
 GtkWidget     * scrolledwindow1;
 GtkWidget     * label2;
 GtkWidget     * hseparator5;
 GtkWidget     * hbuttonbox4;
 GtkWidget     * Cancel;

 SkinBrowser=gtk_window_new( GTK_WINDOW_DIALOG );
 gtk_widget_set_name( SkinBrowser,langSkinBrowser );
 gtk_object_set_data( GTK_OBJECT( SkinBrowser ),langSkinBrowser,SkinBrowser );
 gtk_widget_set_usize( SkinBrowser,256,320 );
 gtk_container_set_border_width( GTK_CONTAINER( SkinBrowser ),1 );
 GTK_WIDGET_SET_FLAGS( SkinBrowser,GTK_CAN_FOCUS );
 GTK_WIDGET_SET_FLAGS( SkinBrowser,GTK_CAN_DEFAULT );
 gtk_widget_set_events( SkinBrowser,GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_FOCUS_CHANGE_MASK | GDK_STRUCTURE_MASK | GDK_PROPERTY_CHANGE_MASK | GDK_VISIBILITY_NOTIFY_MASK );
 gtk_window_set_title( GTK_WINDOW( SkinBrowser ),langSkinBrowser );
 gtk_window_set_position( GTK_WINDOW( SkinBrowser ),GTK_WIN_POS_CENTER );
 gtk_window_set_policy( GTK_WINDOW( SkinBrowser ),FALSE,FALSE,TRUE );

 frame5=gtk_frame_new( NULL );
 gtk_widget_set_name( frame5,"frame5" );
 gtk_widget_ref( frame5 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"frame5",frame5,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( frame5 );
 gtk_container_add( GTK_CONTAINER( SkinBrowser ),frame5 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame5 ),GTK_SHADOW_IN );

 frame6=gtk_frame_new( NULL );
 gtk_widget_set_name( frame6,"frame6" );
 gtk_widget_ref( frame6 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"frame6",frame6,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( frame6 );
 gtk_container_add( GTK_CONTAINER( frame5 ),frame6 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame6 ),GTK_SHADOW_NONE );

 frame7=gtk_frame_new( NULL );
 gtk_widget_set_name( frame7,"frame7" );
 gtk_widget_ref( frame7 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"frame7",frame7,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( frame7 );
 gtk_container_add( GTK_CONTAINER( frame6 ),frame7 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame7 ),GTK_SHADOW_ETCHED_OUT );

 frame8=gtk_frame_new( NULL );
 gtk_widget_set_name( frame8,"frame8" );
 gtk_widget_ref( frame8 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"frame8",frame8,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( frame8 );
 gtk_container_add( GTK_CONTAINER( frame7 ),frame8 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame8 ),GTK_SHADOW_NONE );

 vbox5=gtk_vbox_new( FALSE,0 );
 gtk_widget_set_name( vbox5,"vbox5" );
 gtk_widget_ref( vbox5 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"vbox5",vbox5,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( vbox5 );
 gtk_container_add( GTK_CONTAINER( frame8 ),vbox5 );

 label=gtk_label_new( "Skins:" );
 gtk_widget_set_name( label,"label" );
 gtk_widget_ref( label );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"label",label,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( label );
 gtk_box_pack_start( GTK_BOX( vbox5 ),label,FALSE,FALSE,0 );
 gtk_label_set_justify( GTK_LABEL( label ),GTK_JUSTIFY_RIGHT );
 gtk_misc_set_alignment( GTK_MISC( label ),7.45058e-09,7.45058e-09 );

 hseparator4=gtk_hseparator_new();
 gtk_widget_set_name( hseparator4,"hseparator4" );
 gtk_widget_ref( hseparator4 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"hseparator4",hseparator4,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( hseparator4 );
 gtk_box_pack_start( GTK_BOX( vbox5 ),hseparator4,FALSE,TRUE,0 );
 gtk_widget_set_usize( hseparator4,-2,5 );

 scrolledwindow1=gtk_scrolled_window_new( NULL,NULL );
 gtk_widget_set_name( scrolledwindow1,"scrolledwindow1" );
 gtk_widget_ref( scrolledwindow1 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"scrolledwindow1",scrolledwindow1,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( scrolledwindow1 );
 gtk_box_pack_start( GTK_BOX( vbox5 ),scrolledwindow1,TRUE,TRUE,0 );
 gtk_container_set_border_width( GTK_CONTAINER( scrolledwindow1 ),2 );
 gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolledwindow1 ),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC );

 SkinList=gtk_clist_new( 1 );
 gtk_widget_set_name( SkinList,"SkinList" );
 gtk_widget_ref( SkinList );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"SkinList",SkinList,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( SkinList );
 gtk_container_add( GTK_CONTAINER( scrolledwindow1 ),SkinList );
 gtk_clist_set_column_width( GTK_CLIST( SkinList ),0,80 );
 gtk_clist_set_selection_mode( GTK_CLIST( SkinList ),GTK_SELECTION_SINGLE );
 gtk_clist_column_titles_hide( GTK_CLIST( SkinList ) );
 gtk_clist_set_shadow_type( GTK_CLIST( SkinList ),GTK_SHADOW_ETCHED_OUT );

 label2=gtk_label_new( "label2" );
 gtk_widget_set_name( label2,"label2" );
 gtk_widget_ref( label2 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"label2",label2,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( label2 );
 gtk_clist_set_column_widget( GTK_CLIST( SkinList ),0,label2 );

 hseparator5=gtk_hseparator_new();
 gtk_widget_set_name( hseparator5,"hseparator5" );
 gtk_widget_ref( hseparator5 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"hseparator5",hseparator5,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( hseparator5 );
 gtk_box_pack_start( GTK_BOX( vbox5 ),hseparator5,FALSE,TRUE,0 );
 gtk_widget_set_usize( hseparator5,-2,9 );

 hbuttonbox4=gtk_hbutton_box_new();
 gtk_widget_set_name( hbuttonbox4,"hbuttonbox4" );
 gtk_widget_ref( hbuttonbox4 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"hbuttonbox4",hbuttonbox4,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( hbuttonbox4 );
 gtk_box_pack_start( GTK_BOX( vbox5 ),hbuttonbox4,FALSE,TRUE,0 );
 gtk_button_box_set_layout( GTK_BUTTON_BOX( hbuttonbox4 ),GTK_BUTTONBOX_END );
 gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox4 ),0 );
 gtk_button_box_set_child_size( GTK_BUTTON_BOX( hbuttonbox4 ),80,0 );

 sbOk=gtk_button_new_with_label( langOk );
 gtk_widget_set_name( sbOk,langOk );
 gtk_widget_ref( sbOk );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),langOk,sbOk,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( sbOk );
 gtk_container_add( GTK_CONTAINER( hbuttonbox4 ),sbOk );
 gtk_widget_set_usize( sbOk,-2,33 );
 GTK_WIDGET_SET_FLAGS( sbOk,GTK_CAN_DEFAULT );

 Cancel=gtk_button_new_with_label( langCancel );
 gtk_widget_set_name( Cancel,langCancel );
 gtk_widget_ref( Cancel );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),langCancel,Cancel,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( Cancel );
 gtk_container_add( GTK_CONTAINER( hbuttonbox4 ),Cancel );
 gtk_widget_set_usize( Cancel,-2,33 );
 GTK_WIDGET_SET_FLAGS( Cancel,GTK_CAN_DEFAULT );

 gtk_signal_connect( GTK_OBJECT( SkinBrowser ),"destroy",
                     GTK_SIGNAL_FUNC( on_SkinBrowser_destroy ),
                     NULL );
 gtk_signal_connect( GTK_OBJECT( SkinBrowser ),"key_release_event",
                     GTK_SIGNAL_FUNC( on_SkinBrowser_key_release_event ),
                     NULL );
 gtk_signal_connect( GTK_OBJECT( SkinBrowser ),"key_press_event",
                     GTK_SIGNAL_FUNC( on_SkinBrowser_key_press_event ),
                     NULL );
 gtk_signal_connect( GTK_OBJECT( SkinList ),"select_row",
                     GTK_SIGNAL_FUNC( on_SkinList_select_row ),
                     NULL );
 gtk_signal_connect( GTK_OBJECT( sbOk ),"released",
                     GTK_SIGNAL_FUNC( on_SkinBrowser_destroy ),
                     NULL );
 gtk_signal_connect( GTK_OBJECT( Cancel ),"released",
                     GTK_SIGNAL_FUNC( on_SkinBrowser_destroy ),
                     NULL );

 if ( ( sbMPlayerDirInHome=(char *)calloc( 1,strlen( skinDirInHome ) + 2 ) ) != NULL )
  { strcpy( sbMPlayerDirInHome,skinDirInHome ); strcat( sbMPlayerDirInHome,"/*" ); }
 if ( ( sbMPlayerPrefixDir=(char *)calloc( 1,strlen( skinMPlayerDir ) + 2 ) ) != NULL )
  { strcpy( sbMPlayerPrefixDir,skinMPlayerDir ); strcat( sbMPlayerPrefixDir,"/*" ); }

 gtk_widget_grab_focus( SkinList );

 sbItemsList[0]=SkinList;
 sbItemsList[1]=sbOk;
 sbItemsList[2]=Cancel;

 return SkinBrowser;
}

#endif