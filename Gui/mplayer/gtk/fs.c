
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <glob.h>
#include <unistd.h>

#include "./mplayer.h"
#include "psignal.h"
#include "../error.h"

#include "pixmaps/up.xpm"
#include "pixmaps/dir.xpm"
#include "pixmaps/file.xpm"

#include "../../events.h"
#include "../../../config.h"
#include "../../../help_mp.h"

#include "../widgets.h"
#include "fs.h"

#ifndef __linux__
#define get_current_dir_name()  getcwd(NULL, PATH_MAX)
#endif

#ifndef get_current_dir_name
 extern char * get_current_dir_name( void );
#endif

gchar         * fsSelectedFile = NULL;
gchar         * fsSelectedDirectory = NULL;
unsigned char * fsThatDir = ".";
gchar         * fsFilter = NULL;

int             fsPressed = 0;
int             fsLastFilterNames = 2;
unsigned char * fsFilterNames[3][2] = { { "MPEG files( *.mpg )", "*.mpg" },
                                        { "AVI files( *.avi )",  "*.avi" },
                                        { "All files( *)",       "*"     } };

GtkWidget   * fsFileNamesList;
GtkWidget   * fsFNameList;
GtkWidget   * fsFileSelect;
GdkColormap * fsColorMap;
GtkWidget   * fsOk;
GtkWidget   * fsUp;
GtkWidget   * fsCancel;
GtkWidget   * fsCombo4;
GtkWidget   * fsComboEntry2;
GList       * fsList_items = NULL;
GList       * fsTopList_items = NULL;

void CheckDir( GtkWidget * list,unsigned char * directory )
{
 struct stat     fs;
 int             i,c=2;
 gchar         * str[1][2];
 GdkPixmap     * dpixmap,*fpixmap,*pixmap;
 GdkBitmap     * dmask,*fmask,*mask;
 GtkStyle      * style;
 glob_t          gg;

 gtk_widget_hide( list );
 str[0][0]=NULL;
 style=gtk_widget_get_style( fsFileSelect );
 dpixmap=gdk_pixmap_colormap_create_from_xpm_d( fsFileSelect->window,fsColorMap,&dmask,&style->bg[GTK_STATE_NORMAL],(gchar **)dir_xpm );
 fpixmap=gdk_pixmap_colormap_create_from_xpm_d( fsFileSelect->window,fsColorMap,&fmask,&style->bg[GTK_STATE_NORMAL],(gchar **)file_xpm );
 pixmap=dpixmap; mask=dmask;
 str[0][0]=NULL; str[0][1]=(gchar *)malloc( 3 );
 strcpy( str[0][1],"." );
 gtk_clist_append( GTK_CLIST( list ),str[0] ); gtk_clist_set_pixmap( GTK_CLIST( list ),0,0,pixmap,mask );
 strcpy( str[0][1],".." );
 gtk_clist_append( GTK_CLIST( list ),str[0] ); gtk_clist_set_pixmap( GTK_CLIST( list ),1,0,pixmap,mask );
 free( str[0][0] );

 glob( "*",GLOB_NOSORT,NULL,&gg );
// glob( ".*",GLOB_NOSORT | GLOB_APPEND,NULL,&gg );
 for(  i=0;i<gg.gl_pathc;i++ )
  {
   if(  !strcmp( gg.gl_pathv[i],"." ) || !strcmp( gg.gl_pathv[i],".." ) ) continue;
   stat( gg.gl_pathv[i],&fs );
   if(  S_ISDIR( fs.st_mode ) )
    {
     str[0][1]=(gchar *)malloc( strlen( gg.gl_pathv[i] ) + 2 );
     strcpy( str[0][1],"" );
     strcat( str[0][1],gg.gl_pathv[i] );
     pixmap=dpixmap; mask=dmask;
     gtk_clist_append( GTK_CLIST( list ),str[0] );
     gtk_clist_set_pixmap( GTK_CLIST( list ),c,0,pixmap,mask );
     free( str[0][1] );
     c++;
    }
  }
 globfree( &gg );
 glob( fsFilter,GLOB_NOSORT,NULL,&gg );
// glob( ".*",GLOB_NOSORT | GLOB_APPEND,NULL,&gg );
 pixmap=fpixmap; mask=fmask;
 for(  i=0;i<gg.gl_pathc;i++ )
  {
   if(  !strcmp( gg.gl_pathv[i],"." ) || !strcmp( gg.gl_pathv[i],".." ) ) continue;
   stat( gg.gl_pathv[i],&fs );
   if(  S_ISDIR( fs.st_mode ) ) continue;
   str[0][1]=(gchar *)malloc( strlen( gg.gl_pathv[i] ) + 2 );
   strcpy( str[0][1],"" ); strcat( str[0][1],gg.gl_pathv[i] );
   gtk_clist_append( GTK_CLIST( list ),str[0] );
   gtk_clist_set_pixmap( GTK_CLIST( list ),c,0,pixmap,mask );
   free( str[0][1] );
   c++;
  }
 globfree( &gg );

 gtk_clist_set_sort_type( GTK_CLIST( list ),GTK_SORT_ASCENDING );
 gtk_clist_set_compare_func( GTK_CLIST( list ),NULL );
 gtk_clist_set_sort_column( GTK_CLIST( list ),1 );
 gtk_clist_sort( GTK_CLIST( list ) );
 gtk_clist_set_column_width( GTK_CLIST( list ),0,17 );
 gtk_clist_select_row( GTK_CLIST( list ),0,1 );
 gtk_widget_show( list );
}

void HideFileSelect( void )
{
 gtk_widget_hide( fsFileSelect );
 gtkVisibleFileSelect=0;
 gtkShMem->vs.window=evLoad;
 gtkSendMessage( evHideWindow );
}

void fs_fsFileSelect_destroy( GtkObject * object,gpointer user_data )
{ HideFileSelect(); }

void fs_combo_entry1_activate( GtkEditable * editable,gpointer user_data )
{
 unsigned char * str;

 str=gtk_entry_get_text( GTK_ENTRY(user_data ) );
 gtk_clist_clear(  GTK_CLIST( fsFNameList ) );
 if(  fsFilter ) free( fsFilter );
 if(  (  fsFilter=(unsigned char *)malloc( strlen( str ) + 1 ) )  == NULL )
  {
   dbprintf( 0,"[gtk] not enough memory.\n" );
   exit( 0 );
  }
 strcpy( fsFilter,str );
 CheckDir( fsFNameList,(unsigned char *)get_current_dir_name() );
}

void fs_combo_entry1_changed( GtkEditable * editable,gpointer user_data )
{
 unsigned char * str;
 int             i;

 str=gtk_entry_get_text( GTK_ENTRY(user_data ) );

 for( i=0;i<fsLastFilterNames+1;i++ )
  {
   if(  !strcmp( str,fsFilterNames[i][0] ) )
    {
     if(  fsFilter ) free( fsFilter );
     if( (  fsFilter=(unsigned char *)malloc( 6 ) ) == NULL )
      {
       dbprintf( 0,"[gtk] not enough memory.\n" );
       exit( 0 );
      }
     strcpy( fsFilter,fsFilterNames[i][1] );
    }
  }
 gtk_clist_clear( GTK_CLIST( fsFNameList ) );
 CheckDir( fsFNameList,(unsigned char *)get_current_dir_name() );
}

void fs_fsComboEntry2_activate( GtkEditable * editable,gpointer user_data )
{
 unsigned char * str;

 str=gtk_entry_get_text( GTK_ENTRY( user_data ) );
 if ( chdir( str ) != -1 )
  {
   gtk_clist_clear(  GTK_CLIST( fsFNameList ) );
   CheckDir( fsFNameList,(unsigned char *)get_current_dir_name() );
  }
}

void fs_fsComboEntry2_changed( GtkEditable * editable,gpointer user_data )
{
 unsigned char * str;

 str=gtk_entry_get_text( GTK_ENTRY( user_data ) );
 fsPressed=2;
// if (
// tmp=(unsigned char *)malloc( strlen( fsSelectedDirectory ) + 5 );
// strcpy( tmp,fsSelectedDirectory ); strcat( tmp,"/*" );
// fprintf( stderr,"str: %s\n",tmp );
// free( tmp );
 if ( chdir( str ) != -1 )
  {
   gtk_clist_clear(  GTK_CLIST( fsFNameList ) );
   CheckDir( fsFNameList,(unsigned char *)get_current_dir_name() );
  }
}

void fs_Up_released( GtkButton * button,gpointer user_data )
{
 chdir( ".." );
 fsSelectedFile=fsThatDir;
 gtk_clist_clear(  GTK_CLIST( user_data ) );
 CheckDir( fsFNameList,(unsigned char *)get_current_dir_name() );
 gtk_entry_set_text( GTK_ENTRY( fsComboEntry2 ),(unsigned char *)get_current_dir_name() );
 return;
}

int fsFileExist( unsigned char * fname )
{
 FILE * f = fopen( fname,"r" );
 if ( f == NULL ) return 0;
 fclose( f );
 return 1;
}

void fs_Ok_released( GtkButton * button,gpointer user_data )
{
 unsigned char * str;
 GList         * item;
 int             size,j,i = 1;
 struct stat     fs;

 stat( fsSelectedFile,&fs );
 if(  S_ISDIR(fs.st_mode ) )
  {
   chdir( fsSelectedFile );
   fsSelectedFile=fsThatDir;
   if(  fsFNameList != NULL ) gtk_clist_clear( GTK_CLIST( fsFNameList ) );
   CheckDir( fsFNameList,(unsigned char *)get_current_dir_name() );
   gtk_entry_set_text( GTK_ENTRY( fsComboEntry2 ),(unsigned char *)get_current_dir_name() );
   return;
  }

 HideFileSelect();

 switch( fsPressed )
  {
   case 1:
        fsSelectedDirectory=(unsigned char *)get_current_dir_name();
        printf("[gtk-fs] 1-fsSelectedFile: %s\n",fsSelectedFile);
        #ifdef DEBUG
         dbprintf( 1,"[gtk-fs] fsSelectedFile: %s\n",fsSelectedFile );
        #endif
        break;
   case 2:
        str=gtk_entry_get_text( GTK_ENTRY( fsComboEntry2 ) );
        fsSelectedFile=str;
        printf("[gtk-fs] 2-fsSelectedFile: '%s'  \n",fsSelectedFile);
        #ifdef DEBUG
         dbprintf( 1,"[gtk-fs] fsSelectedFile: %s\n",fsSelectedFile );
        #endif
        if ( !fsFileExist( fsSelectedFile ) ) return;
        fsSelectedDirectory=fsSelectedFile;
        size=strlen( fsSelectedDirectory );
        for ( j=0;j<size;j++ )
         {
          if ( fsSelectedDirectory[ size - j ] == '/' )
           {
            fsSelectedFile+=size - j + 1;
            fsSelectedDirectory[ size - j ]=0;
            break;
           }
         }
        printf("[gtk-fs-xxx] fsSelectedFile: '%s'  \n",fsSelectedFile);
        printf("[gtk-fs-xxx] fsSelectedDirectory: '%s'  \n",fsSelectedDirectory);
        break;
  }
 strcpy( gtkShMem->fs.dir,fsSelectedDirectory );
 strcpy( gtkShMem->fs.filename,fsSelectedFile );
printf( "----gtk---> directory: %s\n",fsSelectedDirectory );
printf( "----gtk---> filename: %s\n",fsSelectedFile );
printf( "----gtksm-> directory: %s\n",gtkShMem->fs.dir );
printf( "----gtksm-> filename: %s\n",gtkShMem->fs.filename );
 item=fsTopList_items;
 while( item )
  {
   if ( !strcmp( item->data,fsSelectedDirectory ) ) i=0;
   item=item->next;
  }
 if ( i )
  {
   fsTopList_items=g_list_prepend( fsTopList_items,(gchar *)get_current_dir_name() );
   gtk_combo_set_popdown_strings( GTK_COMBO( user_data ),fsTopList_items );
  }
 gtkSendMessage( evFileLoaded );
}

void fs_Cancel_released( GtkButton * button,gpointer user_data )
{ HideFileSelect(); }

void fs_fsFNameList_select_row( GtkWidget * widget,gint row,gint column,GdkEventButton *bevent,gpointer user_data )
{
 gtk_clist_get_text( GTK_CLIST(widget ),row,1,&fsSelectedFile ); fsSelectedFile++;
 fsPressed=1;
 if( !bevent ) return;
 if( bevent->type == GDK_2BUTTON_PRESS ) gtk_button_released( GTK_BUTTON( fsOk ) );
}

gboolean on_FileSelect_key_release_event( GtkWidget * widget,GdkEventKey * event,gpointer user_data )
{
 switch ( event->keyval )
  {
   case GDK_Escape:
        gtk_button_released( GTK_BUTTON( fsCancel ) );
        break;
   case GDK_Return:
        gtk_button_released( GTK_BUTTON( fsOk ) );
        break;
   case GDK_BackSpace:
        gtk_button_released( GTK_BUTTON( fsUp ) );
        break;
  }
 return FALSE;
}

GtkWidget * create_FileSelect( void )
{
 GtkWidget     * FSFrame;
 GtkWidget     * frame2;
 GtkWidget     * frame3;
 GtkWidget     * frame4;
 GtkWidget     * vbox4;
 GtkWidget     * hbox4;
 GtkWidget     * vseparator1;
 GtkWidget     * hseparator1;
 GtkWidget     * hbox6;
 GtkWidget     * fsFNameListWindow;
 GtkWidget     * label1;
 GtkWidget     * hseparator2;
 GtkWidget     * List;
 GtkWidget     * combo_entry1;
 GtkWidget     * hseparator3;
 GtkWidget     * hbuttonbox3;
 int             i;

// GtkWidget     * okpixmapwid,*uppixmapwid,*cancelpixmapwid;
// GdkPixmap     * okpixmap,*uppixmap,*cancelpixmap;
// GdkBitmap     * okmask,*upmask,*cancelmask;
// GtkStyle      * okstyle,*upstyle,*cancelstyle;

 GtkWidget     * uppixmapwid;
 GdkPixmap     * uppixmap;
 GdkBitmap     * upmask;
 GtkStyle      * upstyle;

 if( (  fsFilter=(unsigned char *)malloc( 3 ) ) == NULL )
  {
   dbprintf( 0,"[gtk] not enough memory.\n" );
   exit( 0 );
  }
 strcpy( fsFilter,"*" );

 fsFileSelect=gtk_window_new( GTK_WINDOW_DIALOG );
 gtk_widget_set_name( fsFileSelect,"fsFileSelect" );
 gtk_object_set_data( GTK_OBJECT( fsFileSelect ),"fsFileSelect",fsFileSelect );
 gtk_widget_set_usize( fsFileSelect,416,256 );
 GTK_WIDGET_SET_FLAGS( fsFileSelect,GTK_CAN_DEFAULT );
 gtk_widget_set_events( fsFileSelect,GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_FOCUS_CHANGE_MASK | GDK_STRUCTURE_MASK | GDK_PROPERTY_CHANGE_MASK | GDK_VISIBILITY_NOTIFY_MASK );
 gtk_window_set_title( GTK_WINDOW( fsFileSelect ),MSGTR_FileSelect );
 gtk_window_set_position( GTK_WINDOW( fsFileSelect ),GTK_WIN_POS_CENTER );
 gtk_window_set_policy( GTK_WINDOW( fsFileSelect ),FALSE,FALSE,TRUE );
 fsColorMap=gdk_colormap_get_system();

 FSFrame=gtk_frame_new( NULL );
 gtk_widget_set_name( FSFrame,"FSFrame" );
 gtk_widget_ref( FSFrame );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"FSFrame",FSFrame,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( FSFrame );
 gtk_container_add( GTK_CONTAINER( fsFileSelect ),FSFrame );
 gtk_container_set_border_width( GTK_CONTAINER( FSFrame ),1 );
 gtk_frame_set_shadow_type( GTK_FRAME( FSFrame ),GTK_SHADOW_IN );

 frame2=gtk_frame_new( NULL );
 gtk_widget_set_name( frame2,"frame2" );
 gtk_widget_ref( frame2 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"frame2",frame2,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( frame2 );
 gtk_container_add( GTK_CONTAINER( FSFrame ),frame2 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame2 ),GTK_SHADOW_NONE );

 frame3=gtk_frame_new( NULL );
 gtk_widget_set_name( frame3,"frame3" );
 gtk_widget_ref( frame3 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"frame3",frame3,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( frame3 );
 gtk_container_add( GTK_CONTAINER( frame2 ),frame3 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame3 ),GTK_SHADOW_ETCHED_OUT );

 frame4=gtk_frame_new( NULL );
 gtk_widget_set_name( frame4,"frame4" );
 gtk_widget_ref( frame4 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"frame4",frame4,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( frame4 );
 gtk_container_add( GTK_CONTAINER( frame3 ),frame4 );
 gtk_container_set_border_width( GTK_CONTAINER( frame4 ),1 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame4 ),GTK_SHADOW_NONE );

 vbox4=gtk_vbox_new( FALSE,0 );
 gtk_widget_set_name( vbox4,"vbox4" );
 gtk_widget_ref( vbox4 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"vbox4",vbox4,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( vbox4 );
 gtk_container_add( GTK_CONTAINER( frame4 ),vbox4 );

 hbox4=gtk_hbox_new( FALSE,0 );
 gtk_widget_set_name( hbox4,"hbox4" );
 gtk_widget_ref( hbox4 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"hbox4",hbox4,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( hbox4 );
 gtk_box_pack_start( GTK_BOX( vbox4 ),hbox4,TRUE,TRUE,0 );

 fsCombo4=gtk_combo_new();
 gtk_widget_set_name( fsCombo4,"fsCombo4" );
 gtk_widget_ref( fsCombo4 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"fsCombo4",fsCombo4,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( fsCombo4 );
 gtk_box_pack_start( GTK_BOX( hbox4 ),fsCombo4,TRUE,TRUE,0 );
 gtk_widget_set_usize( fsCombo4,-2,20 );

 fsTopList_items=g_list_append( fsTopList_items,(gchar *)get_current_dir_name() );
 if ( getenv( "HOME" ) ) fsTopList_items=g_list_append( fsTopList_items,getenv( "HOME" ) );
 fsTopList_items=g_list_append( fsTopList_items,"/home" );
 fsTopList_items=g_list_append( fsTopList_items,"/mnt" );
 fsTopList_items=g_list_append( fsTopList_items,"/" );
 gtk_combo_set_popdown_strings( GTK_COMBO( fsCombo4 ),fsTopList_items );

 fsComboEntry2=GTK_COMBO( fsCombo4 )->entry;
 gtk_widget_set_name( fsComboEntry2,"fsComboEntry2" );
 gtk_widget_ref( fsComboEntry2 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"fsComboEntry2",fsComboEntry2,( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( fsComboEntry2 );
 gtk_widget_set_usize( fsComboEntry2,-2,20 );

 vseparator1=gtk_vseparator_new();
 gtk_widget_set_name( vseparator1,"vseparator1" );
 gtk_widget_ref( vseparator1 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"vseparator1",vseparator1,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( vseparator1 );
 gtk_box_pack_start( GTK_BOX( hbox4 ),vseparator1,FALSE,TRUE,0 );
 gtk_widget_set_usize( vseparator1,7,20 );

 upstyle=gtk_widget_get_style( fsFileSelect );
 uppixmap=gdk_pixmap_colormap_create_from_xpm_d( fsFileSelect->window,fsColorMap,&upmask,&upstyle->bg[GTK_STATE_NORMAL],(gchar **)up_xpm );
 uppixmapwid=gtk_pixmap_new( uppixmap,upmask );
 gtk_widget_show( uppixmapwid );

 fsUp=gtk_button_new();
 gtk_container_add( GTK_CONTAINER(fsUp ),uppixmapwid );
 gtk_widget_show( fsUp );
 gtk_box_pack_start( GTK_BOX( hbox4 ),fsUp,FALSE,FALSE,0 );
 gtk_widget_set_usize( fsUp,65,15 );

 hseparator1=gtk_hseparator_new();
 gtk_widget_set_name( hseparator1,"hseparator1" );
 gtk_widget_ref( hseparator1 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"hseparator1",hseparator1,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( hseparator1 );
 gtk_box_pack_start( GTK_BOX( vbox4 ),hseparator1,FALSE,TRUE,0 );
 gtk_widget_set_usize( hseparator1,-2,8 );

 hbox6=gtk_hbox_new( FALSE,0 );
 gtk_widget_set_name( hbox6,"hbox6" );
 gtk_widget_ref( hbox6 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"hbox6",hbox6,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( hbox6 );
 gtk_box_pack_start( GTK_BOX( vbox4 ),hbox6,TRUE,TRUE,0 );

 fsFNameListWindow=gtk_scrolled_window_new( NULL,NULL );
 gtk_widget_set_name( fsFNameListWindow,"fsFNameListWindow" );
 gtk_widget_ref( fsFNameListWindow );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"fsFNameListWindow",fsFNameListWindow,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( fsFNameListWindow );
 gtk_box_pack_start( GTK_BOX( hbox6 ),fsFNameListWindow,TRUE,TRUE,0 );
 gtk_widget_set_usize( fsFNameListWindow,-2,145 );
 gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( fsFNameListWindow ),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC );

 fsFNameList=gtk_clist_new( 2 );
 gtk_widget_set_name( fsFNameList,"fsFNameList" );
 gtk_widget_ref( fsFNameList );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"fsFNameList",fsFNameList,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_container_add( GTK_CONTAINER( fsFNameListWindow ),fsFNameList );
 gtk_clist_set_column_width( GTK_CLIST( fsFNameList ),0,80 );
 gtk_clist_set_selection_mode( GTK_CLIST( fsFNameList ),GTK_SELECTION_BROWSE );
 gtk_clist_column_titles_hide( GTK_CLIST( fsFNameList ) );
 gtk_clist_set_shadow_type( GTK_CLIST( fsFNameList ),GTK_SHADOW_ETCHED_OUT );
 CheckDir( fsFNameList,(unsigned char *)get_current_dir_name() );

 label1=gtk_label_new( "label1" );
 gtk_widget_set_name( label1,"label1" );
 gtk_widget_ref( label1 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"label1",label1,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( label1 );
 gtk_clist_set_column_widget( GTK_CLIST( fsFNameList ),0,label1 );

 hseparator2=gtk_hseparator_new();
 gtk_widget_set_name( hseparator2,"hseparator2" );
 gtk_widget_ref( hseparator2 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"hseparator2",hseparator2,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( hseparator2 );
 gtk_box_pack_start( GTK_BOX( vbox4 ),hseparator2,FALSE,TRUE,0 );
 gtk_widget_set_usize( hseparator2,-2,9 );

 List=gtk_combo_new();
 gtk_widget_set_name( List,"List" );
 gtk_widget_ref( List );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"List",List,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( List );
 gtk_box_pack_start( GTK_BOX( vbox4 ),List,FALSE,FALSE,0 );
 gtk_widget_set_usize( List,-2,20 );
 fsList_items=NULL;
 for( i=0;i<fsLastFilterNames + 1;i++ )
   fsList_items=g_list_append( fsList_items,fsFilterNames[i][0] );
 gtk_combo_set_popdown_strings( GTK_COMBO( List ),fsList_items );
 g_list_free( fsList_items );

 combo_entry1=GTK_COMBO( List )->entry;
 gtk_widget_set_name( combo_entry1,"combo_entry1" );
 gtk_widget_ref( combo_entry1 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"combo_entry1",combo_entry1,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( combo_entry1 );
 gtk_entry_set_text( GTK_ENTRY( combo_entry1 ),fsFilterNames[fsLastFilterNames][0] );

 hseparator3=gtk_hseparator_new();
 gtk_widget_set_name( hseparator3,"hseparator3" );
 gtk_widget_ref( hseparator3 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"hseparator3",hseparator3,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( hseparator3 );
 gtk_box_pack_start( GTK_BOX( vbox4 ),hseparator3,FALSE,TRUE,0 );
 gtk_widget_set_usize( hseparator3,-2,7 );

 hbuttonbox3=gtk_hbutton_box_new();
 gtk_widget_set_name( hbuttonbox3,"hbuttonbox3" );
 gtk_widget_ref( hbuttonbox3 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"hbuttonbox3",hbuttonbox3,
                          ( GtkDestroyNotify ) gtk_widget_unref );
 gtk_widget_show( hbuttonbox3 );
 gtk_box_pack_start( GTK_BOX( vbox4 ),hbuttonbox3,FALSE,TRUE,0 );
 gtk_button_box_set_layout( GTK_BUTTON_BOX( hbuttonbox3 ),GTK_BUTTONBOX_END );
 gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox3 ),10 );
 gtk_button_box_set_child_size( GTK_BUTTON_BOX( hbuttonbox3 ),85,20 );
 gtk_button_box_set_child_ipadding( GTK_BUTTON_BOX( hbuttonbox3 ),0,0 );

// okstyle=gtk_widget_get_style( fsFileSelect );
// okpixmap=gdk_pixmap_colormap_create_from_xpm_d( fsFileSelect->window,fsColorMap,&okmask,&okstyle->bg[GTK_STATE_NORMAL],(gchar **)ok_xpm );
// okpixmapwid=gtk_pixmap_new( okpixmap,okmask );
// gtk_widget_show( okpixmapwid );
// fsOk=gtk_button_new();
// gtk_container_add( GTK_CONTAINER(fsOk ),okpixmapwid );
// gtk_container_add( GTK_CONTAINER( hbuttonbox3 ),fsOk );
// gtk_widget_show( fsOk );

 fsOk=gtk_button_new_with_label( MSGTR_Ok );
 gtk_widget_set_name( fsOk,MSGTR_Ok );
 gtk_widget_ref( fsOk );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),MSGTR_Ok,fsOk,( GtkDestroyNotify )gtk_widget_unref );
 gtk_container_add( GTK_CONTAINER( hbuttonbox3 ),fsOk );
 gtk_widget_show( fsOk );

// cancelstyle=gtk_widget_get_style( fsFileSelect );
// cancelpixmap=gdk_pixmap_colormap_create_from_xpm_d( fsFileSelect->window,fsColorMap,&cancelmask,&cancelstyle->bg[GTK_STATE_NORMAL],(gchar **)cancel_xpm );
// cancelpixmapwid=gtk_pixmap_new( cancelpixmap,cancelmask );
// gtk_widget_show( cancelpixmapwid );
// fsCancel=gtk_button_new();
// gtk_widget_show( fsCancel );
// gtk_container_add( GTK_CONTAINER( fsCancel ),cancelpixmapwid );
// gtk_container_add( GTK_CONTAINER( hbuttonbox3 ),fsCancel );
// gtk_widget_show( fsCancel );

 fsCancel=gtk_button_new_with_label( MSGTR_Cancel );
 gtk_widget_set_name( fsCancel,MSGTR_Cancel );
 gtk_widget_ref( fsCancel );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),MSGTR_Cancel,fsCancel,( GtkDestroyNotify )gtk_widget_unref );
 gtk_container_add( GTK_CONTAINER( hbuttonbox3 ),fsCancel );
 gtk_widget_show( fsCancel );

 gtk_signal_connect( GTK_OBJECT( fsFileSelect ),"destroy",
                     GTK_SIGNAL_FUNC( fs_fsFileSelect_destroy ),
                     NULL );
 gtk_signal_connect( GTK_OBJECT( fsFileSelect ),"key_release_event",
                     GTK_SIGNAL_FUNC( on_FileSelect_key_release_event ),
                     NULL );
 gtk_signal_connect( GTK_OBJECT( combo_entry1 ),"changed",
                     GTK_SIGNAL_FUNC( fs_combo_entry1_changed ),
                     combo_entry1 );
 gtk_signal_connect( GTK_OBJECT( combo_entry1 ),"activate",
                     GTK_SIGNAL_FUNC( fs_combo_entry1_activate ),
                     combo_entry1 );
 gtk_signal_connect( GTK_OBJECT( fsComboEntry2 ),"changed",
                     GTK_SIGNAL_FUNC( fs_fsComboEntry2_changed ),
                     fsComboEntry2 );
 gtk_signal_connect( GTK_OBJECT( fsComboEntry2 ),"activate",
                     GTK_SIGNAL_FUNC( fs_fsComboEntry2_activate ),
                     fsComboEntry2 );
 gtk_signal_connect( GTK_OBJECT( fsUp ),"released",
                     GTK_SIGNAL_FUNC( fs_Up_released ),
                     fsFNameList );
 gtk_signal_connect( GTK_OBJECT( fsOk ),"released",
                     GTK_SIGNAL_FUNC( fs_Ok_released ),
                     fsCombo4 );
 gtk_signal_connect( GTK_OBJECT( fsCancel ),"released",
                     GTK_SIGNAL_FUNC( fs_Cancel_released ),
                     NULL );

 gtk_signal_connect( GTK_OBJECT( fsFNameList ),"select_row",
                    ( GtkSignalFunc ) fs_fsFNameList_select_row,
                     NULL );

 gtk_widget_grab_focus( fsFNameList );

 return fsFileSelect;
}

