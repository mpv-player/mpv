
#include <sys/stat.h>
#include <glob.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "sb.h"
#include "../../events.h"
#include "../../../config.h"
#include "../../../help_mp.h"

#include "../widgets.h"
#include "../app.h"

GtkWidget * SkinList;
char      * sbSelectedSkin=NULL;
char      * sbMPlayerDirInHome=NULL;
char      * sbMPlayerPrefixDir=NULL;

char * gtkOldSkin;
static char * prev;

int         gtkVSkinBrowser = 0;
GtkWidget * SkinBrowser;

void ShowSkinBrowser( void )
{
 if ( gtkVSkinBrowser ) gtkActive( SkinBrowser );
   else SkinBrowser=create_SkinBrowser();
}

void HideSkinBrowser( void )
{
 gtkVSkinBrowser=0;
 gtk_widget_destroy( SkinBrowser );
}

int gtkFillSkinList( gchar * mdir )
{
 gchar         * str[2];
 gchar         * tmp;
 int             i;
 glob_t          gg;
 struct stat     fs;

 gtkOldSkin=strdup( skinName );
 prev=gtkOldSkin;
 if ( ( str[0]=(char *)calloc( 1,7 ) ) == NULL )
  {
   gtkMessageBox( GTK_MB_FATAL,MSGTR_SKINBROWSER_NotEnoughMemory );
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
     if ( ( str[0]=(char *)malloc( strlen( tmp ) + 1 ) ) == NULL ) { gtkMessageBox( GTK_MB_FATAL,MSGTR_SKINBROWSER_NotEnoughMemory ); return 0; }
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

void on_SkinBrowser_show( GtkObject * object,gpointer user_data )
{ gtkVSkinBrowser=(int)user_data; }

void on_SkinBrowser_Cancel( GtkObject * object,gpointer user_data )
{
 if ( strcmp( sbSelectedSkin,gtkOldSkin ) ) ChangeSkin( gtkOldSkin );
 HideSkinBrowser();
}

void on_SkinBrowser_Ok( GtkObject * object,gpointer user_data )
{
 ChangeSkin( sbSelectedSkin );
 if ( skinName ) free( skinName );
 skinName=strdup( sbSelectedSkin );
 HideSkinBrowser();
}

void on_SkinList_select_row( GtkCList * clist,gint row,gint column,GdkEvent * bevent,gpointer user_data )
{
 gtk_clist_get_text( clist,row,0,&sbSelectedSkin );
 if ( strcmp( prev,sbSelectedSkin ) )
  {
   prev=sbSelectedSkin;
   ChangeSkin( sbSelectedSkin );
   gtkActive( SkinBrowser );
  }
 if( !bevent ) return;
 if( bevent->type == GDK_2BUTTON_PRESS )
  {
   if ( skinName ) free( skinName );
   skinName=strdup( sbSelectedSkin );
   HideSkinBrowser();
  }
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
 GtkWidget     * Ok;
 GtkAccelGroup * accel_group;

 accel_group = gtk_accel_group_new ();

 SkinBrowser=gtk_window_new( GTK_WINDOW_DIALOG );
 gtk_widget_set_name( SkinBrowser,MSGTR_SkinBrowser );
 gtk_object_set_data( GTK_OBJECT( SkinBrowser ),MSGTR_SkinBrowser,SkinBrowser );
 gtk_widget_set_usize( SkinBrowser,256,320 );
 gtk_container_set_border_width( GTK_CONTAINER( SkinBrowser ),1 );
// GTK_WIDGET_SET_FLAGS( SkinBrowser,GTK_CAN_FOCUS );
 GTK_WIDGET_SET_FLAGS( SkinBrowser,GTK_CAN_DEFAULT );
 gtk_widget_set_events( SkinBrowser,GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_FOCUS_CHANGE_MASK | GDK_STRUCTURE_MASK | GDK_PROPERTY_CHANGE_MASK | GDK_VISIBILITY_NOTIFY_MASK );
 gtk_window_set_title( GTK_WINDOW( SkinBrowser ),MSGTR_SkinBrowser );
 gtk_window_set_position( GTK_WINDOW( SkinBrowser ),GTK_WIN_POS_CENTER );
 gtk_window_set_policy( GTK_WINDOW( SkinBrowser ),FALSE,FALSE,TRUE );

 frame5=gtk_frame_new( NULL );
 gtk_widget_set_name( frame5,"frame5" );
 gtk_widget_ref( frame5 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"frame5",frame5,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( frame5 );
 gtk_container_add( GTK_CONTAINER( SkinBrowser ),frame5 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame5 ),GTK_SHADOW_IN );

 frame6=gtk_frame_new( NULL );
 gtk_widget_set_name( frame6,"frame6" );
 gtk_widget_ref( frame6 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"frame6",frame6,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( frame6 );
 gtk_container_add( GTK_CONTAINER( frame5 ),frame6 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame6 ),GTK_SHADOW_NONE );

 frame7=gtk_frame_new( NULL );
 gtk_widget_set_name( frame7,"frame7" );
 gtk_widget_ref( frame7 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"frame7",frame7,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( frame7 );
 gtk_container_add( GTK_CONTAINER( frame6 ),frame7 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame7 ),GTK_SHADOW_ETCHED_OUT );

 frame8=gtk_frame_new( NULL );
 gtk_widget_set_name( frame8,"frame8" );
 gtk_widget_ref( frame8 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"frame8",frame8,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( frame8 );
 gtk_container_add( GTK_CONTAINER( frame7 ),frame8 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame8 ),GTK_SHADOW_NONE );

 vbox5=gtk_vbox_new( FALSE,0 );
 gtk_widget_set_name( vbox5,"vbox5" );
 gtk_widget_ref( vbox5 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"vbox5",vbox5,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( vbox5 );
 gtk_container_add( GTK_CONTAINER( frame8 ),vbox5 );

 label=gtk_label_new( MSGTR_SKIN_LABEL );
 gtk_widget_set_name( label,"label" );
 gtk_widget_ref( label );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"label",label,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( label );
 gtk_box_pack_start( GTK_BOX( vbox5 ),label,FALSE,FALSE,0 );
 gtk_label_set_justify( GTK_LABEL( label ),GTK_JUSTIFY_RIGHT );
 gtk_misc_set_alignment( GTK_MISC( label ),7.45058e-09,7.45058e-09 );

 hseparator4=gtk_hseparator_new();
 gtk_widget_set_name( hseparator4,"hseparator4" );
 gtk_widget_ref( hseparator4 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"hseparator4",hseparator4,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( hseparator4 );
 gtk_box_pack_start( GTK_BOX( vbox5 ),hseparator4,FALSE,TRUE,0 );
 gtk_widget_set_usize( hseparator4,-2,5 );

 scrolledwindow1=gtk_scrolled_window_new( NULL,NULL );
 gtk_widget_set_name( scrolledwindow1,"scrolledwindow1" );
 gtk_widget_ref( scrolledwindow1 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"scrolledwindow1",scrolledwindow1,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( scrolledwindow1 );
 gtk_box_pack_start( GTK_BOX( vbox5 ),scrolledwindow1,TRUE,TRUE,0 );
 gtk_container_set_border_width( GTK_CONTAINER( scrolledwindow1 ),2 );
 gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolledwindow1 ),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC );

 SkinList=gtk_clist_new( 1 );
 gtk_widget_set_name( SkinList,"SkinList" );
 gtk_widget_ref( SkinList );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"SkinList",SkinList,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( SkinList );
 gtk_container_add( GTK_CONTAINER( scrolledwindow1 ),SkinList );
 gtk_clist_set_column_width( GTK_CLIST( SkinList ),0,80 );
 gtk_clist_set_selection_mode( GTK_CLIST( SkinList ),GTK_SELECTION_SINGLE );
 gtk_clist_column_titles_hide( GTK_CLIST( SkinList ) );
 gtk_clist_set_shadow_type( GTK_CLIST( SkinList ),GTK_SHADOW_ETCHED_OUT );

 label2=gtk_label_new( "label2" );
 gtk_widget_set_name( label2,"label2" );
 gtk_widget_ref( label2 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"label2",label2,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( label2 );
 gtk_clist_set_column_widget( GTK_CLIST( SkinList ),0,label2 );

 hseparator5=gtk_hseparator_new();
 gtk_widget_set_name( hseparator5,"hseparator5" );
 gtk_widget_ref( hseparator5 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"hseparator5",hseparator5,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( hseparator5 );
 gtk_box_pack_start( GTK_BOX( vbox5 ),hseparator5,FALSE,TRUE,0 );
 gtk_widget_set_usize( hseparator5,-2,9 );

 hbuttonbox4=gtk_hbutton_box_new();
 gtk_widget_set_name( hbuttonbox4,"hbuttonbox4" );
 gtk_widget_ref( hbuttonbox4 );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),"hbuttonbox4",hbuttonbox4,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_set_usize( hbuttonbox4,-2,25 );
 gtk_widget_show( hbuttonbox4 );
 gtk_box_pack_start( GTK_BOX( vbox5 ),hbuttonbox4,FALSE,TRUE,0 );
 gtk_button_box_set_layout( GTK_BUTTON_BOX( hbuttonbox4 ),GTK_BUTTONBOX_END );
 gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox4 ),5 );
 gtk_button_box_set_child_size( GTK_BUTTON_BOX( hbuttonbox4 ),75,0 );

 Ok=gtk_button_new_with_label( MSGTR_Ok );
 gtk_widget_set_name( Ok,MSGTR_Ok );
 gtk_widget_ref( Ok );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),MSGTR_Ok,Ok,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( Ok );
 gtk_container_add( GTK_CONTAINER( hbuttonbox4 ),Ok );
 gtk_widget_set_usize( Ok,-2,22 );
 gtk_widget_add_accelerator( Ok,"released",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE );

 Cancel=gtk_button_new_with_label( MSGTR_Cancel );
 gtk_widget_set_name( Cancel,MSGTR_Cancel );
 gtk_widget_ref( Cancel );
 gtk_object_set_data_full( GTK_OBJECT( SkinBrowser ),MSGTR_Cancel,Cancel,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( Cancel );
 gtk_container_add( GTK_CONTAINER( hbuttonbox4 ),Cancel );
 gtk_widget_set_usize( Cancel,-2,22 );
 gtk_widget_add_accelerator( Cancel,"released",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );

 gtk_signal_connect( GTK_OBJECT( SkinBrowser ),"destroy",GTK_SIGNAL_FUNC( on_SkinBrowser_destroy ),0 );
 gtk_signal_connect( GTK_OBJECT( SkinBrowser ),"show",GTK_SIGNAL_FUNC( on_SkinBrowser_show ),1 );
 gtk_signal_connect( GTK_OBJECT( SkinBrowser ),"hide",GTK_SIGNAL_FUNC( on_SkinBrowser_show ),0 );
 
 gtk_signal_connect( GTK_OBJECT( SkinList ),"select_row",GTK_SIGNAL_FUNC( on_SkinList_select_row ),NULL );
 gtk_signal_connect( GTK_OBJECT( Ok ),"released",GTK_SIGNAL_FUNC( on_SkinBrowser_Ok ),NULL );
 gtk_signal_connect( GTK_OBJECT( Cancel ),"released",GTK_SIGNAL_FUNC( on_SkinBrowser_Cancel ),NULL );

 if ( ( sbMPlayerDirInHome=(char *)calloc( 1,strlen( skinDirInHome ) + 4 ) ) != NULL )
  { strcpy( sbMPlayerDirInHome,skinDirInHome ); strcat( sbMPlayerDirInHome,"/*" ); }
 if ( ( sbMPlayerPrefixDir=(char *)calloc( 1,strlen( skinMPlayerDir ) + 4 ) ) != NULL )
  { strcpy( sbMPlayerPrefixDir,skinMPlayerDir ); strcat( sbMPlayerPrefixDir,"/*" ); }

 gtk_window_add_accel_group( GTK_WINDOW( SkinBrowser ),accel_group );
 gtk_widget_grab_focus( SkinList );

 return SkinBrowser;
}
