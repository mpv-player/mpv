
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <glob.h>
#include <unistd.h>

#include "./mplayer.h"

#include "pixmaps/up.xpm"
#include "pixmaps/dir.xpm"
#include "pixmaps/file.xpm"

#include "../../events.h"
#include "../../interface.h"
#include "../../../config.h"
#include "../../../help_mp.h"
#include "../../../libmpdemux/stream.h"

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
gchar           fsFilter[64] = "*";

int             fsPressed = 0;
int             fsMessage = -1;
int             fsType    = 0;

int gtkVFileSelect = 0;

#define fsNumberOfVideoFilterNames 9
char * fsVideoFilterNames[fsNumberOfVideoFilterNames+1][2] =
         { { "MPEG files (*.mpg,*.mpeg)",                               "*.mpg,*.mpeg" },
           { "VOB files (*.vob)",  				  	"*.vob" },
           { "AVI files (*.avi)",  				  	"*.avi" },
           { "QT files (*.mov)",   				  	"*.mov" },
           { "ASF files (*.asf)",  				  	"*.asf" },
           { "VIVO files (*.viv)", 				  	"*.viv" },
	   { "Windows Media Video (*.wmv)",			  	"*.wmv" },
	   { "Audio files (*.mp2,*.mp3,*.wma)",			  	"*.mp2,*.mp3,*.wma" },
	   { "Video files (*.mpg,*.mpeg,*.vob,*.avi,*.mov,*.asf,*.viv,*.wmv)", "*.mpg,*.mpeg,*.vob,*.avi,*.mov,*.asf,*.viv,*.wmv" },
           { "All files (*)",      "*"     } };

#define fsNumberOfSubtitleFilterNames 9
char * fsSubtitleFilterNames[fsNumberOfSubtitleFilterNames+1][2] =
         { { "UTF (*.utf)",  						   "*.utf" },
           { "SUB (*.sub)",   						   "*.sub" },
           { "SRT (*.srt)",   						   "*.str" },
           { "SMI (*.smi)",   						   "*.smi" },
           { "RT  (*.rt) ",   						   "*.rt"  },
           { "TXT (*.txt)",   						   "*.txt" },
           { "SSA (*.ssa)",   						   "*.ssa" },
           { "AQT (*.aqt)",   						   "*.aqt" },
	   { "Subtitles (*.utf,*.sub,*.srt,*.smi,*.rt,*.txt,*.ssa,*.aqt)", "*.utf,*.sub,*.srt,*.smi,*.rt,*.txt,*.ssa,*.aqt" },
           { "All files ( * )", "*"     } };

#define fsNumberOfOtherFilterNames 0
char * fsOtherFilterNames[fsNumberOfOtherFilterNames+1][2] =
         { { "All files ( * )", "*"     } };

GtkWidget   * fsFileNamesList;
GtkWidget   * fsFNameList;
GtkWidget   * fsFileSelect;
GdkColormap * fsColorMap;
GtkWidget   * fsOk;
GtkWidget   * fsUp;
GtkWidget   * fsCancel;
GtkWidget   * fsCombo4;
GtkWidget   * fsPathCombo;
GList       * fsList_items = NULL;
GList       * fsTopList_items = NULL;
GtkWidget   * List;
GtkWidget   * fsFilterCombo;


GtkStyle    * style;
GdkPixmap   * dpixmap;
GdkPixmap   * fpixmap;
GdkBitmap   * dmask;
GdkBitmap   * fmask;

void CheckDir( GtkWidget * list,char * directory )
{
 struct stat     fs;
 int             i,c=2;
 gchar         * str[1][2];
 GdkPixmap     * pixmap;
 GdkBitmap     * mask;
 glob_t          gg;

 if ( !fsFilter[0] ) return;

 gtk_widget_hide( list );
 gtk_clist_clear( GTK_CLIST( list ) );
 str[0][0]=NULL;

 pixmap=dpixmap; mask=dmask;
 str[0][0]=NULL; str[0][1]=(gchar *)malloc( 3 );
 strcpy( str[0][1],"." );  gtk_clist_append( GTK_CLIST( list ),str[0] ); gtk_clist_set_pixmap( GTK_CLIST( list ),0,0,pixmap,mask );
 strcpy( str[0][1],".." ); gtk_clist_append( GTK_CLIST( list ),str[0] ); gtk_clist_set_pixmap( GTK_CLIST( list ),1,0,pixmap,mask );
 free( str[0][0] );

 glob( "*",0,NULL,&gg );
// glob( ".*",GLOB_NOSORT | GLOB_APPEND,NULL,&gg );
 for(  i=0;i<gg.gl_pathc;i++ )
  {
   stat( gg.gl_pathv[i],&fs );
   if( !S_ISDIR( fs.st_mode ) ) continue;

   str[0][1]=(gchar *)malloc( strlen( gg.gl_pathv[i] ) + 1 );
   strcpy( str[0][1],gg.gl_pathv[i] );
   pixmap=dpixmap; mask=dmask;
   gtk_clist_append( GTK_CLIST( list ),str[0] );
   gtk_clist_set_pixmap( GTK_CLIST( list ),c++,0,pixmap,mask );
   free( str[0][1] );
  }
 globfree( &gg );

//printf( "fsFiler: '%s'\n",fsFilter );
 if ( strchr( fsFilter,',' ) )
  {
   char tmp[8];
   int  i,c,glob_param = 0;
//printf( "sub item detected.\n" );   
   for ( i=0,c=0;i<(int)strlen( fsFilter ) + 1;i++,c++ )
    {
     tmp[c]=fsFilter[i];
     if ( ( tmp[c] == ',' )||( tmp[c] == '\0' ) )
      {
       tmp[c]=0; c=-1;
//       printf( "substr: %s\n",tmp );
       glob( tmp,glob_param,NULL,&gg ); 
       glob_param=GLOB_APPEND;
      }
    }
  } else glob( fsFilter,0,NULL,&gg );

#if 0
 if ( !strcmp( fsFilter,"*" ) )
 {
  char * f = strdup( fsFilter );
  int    i;
  for( i=0;i<strlen( f );i++ )
   if ( ( f[i] >= 'A' )&&( f[i] <= 'Z' ) ) f[i]+=32;
  glob( f,GLOB_APPEND,NULL,&gg );

  for( i=0;i<strlen( f );i++ )
   if ( ( f[i] >= 'a' )&&( f[i] <= 'z' ) ) f[i]-=32;
  glob( f,GLOB_APPEND,NULL,&gg );
  free( f );
 }
#endif

// glob( ".*",GLOB_NOSORT | GLOB_APPEND,NULL,&gg );
 pixmap=fpixmap; mask=fmask;
 for(  i=0;i<gg.gl_pathc;i++ )
  {
   stat( gg.gl_pathv[i],&fs );
   if(  S_ISDIR( fs.st_mode ) ) continue;

   str[0][1]=(gchar *)malloc( strlen( gg.gl_pathv[i] ) + 1 );
   strcpy( str[0][1],gg.gl_pathv[i] );
   gtk_clist_append( GTK_CLIST( list ),str[0] );
   gtk_clist_set_pixmap( GTK_CLIST( list ),c++,0,pixmap,mask );
   free( str[0][1] );
  }
 globfree( &gg );

 gtk_clist_set_column_width( GTK_CLIST( list ),0,17 );
 gtk_clist_select_row( GTK_CLIST( list ),0,1 );
 gtk_widget_show( list );
}

static int FirstInit = 1;

void ShowFileSelect( int type,int modal )
{
 int i;

 if ( gtkVFileSelect ) gtkActive( fsFileSelect );
  else fsFileSelect=create_FileSelect();
 
 if ( FirstInit )
  {
   fsTopList_items=g_list_append( fsTopList_items,(gchar *)get_current_dir_name() );
   if ( getenv( "HOME" ) ) fsTopList_items=g_list_append( fsTopList_items,getenv( "HOME" ) );
   fsTopList_items=g_list_append( fsTopList_items,"/home" );
   fsTopList_items=g_list_append( fsTopList_items,"/mnt" );
   fsTopList_items=g_list_append( fsTopList_items,"/" );
   FirstInit=0;
  }
 gtk_combo_set_popdown_strings( GTK_COMBO( fsCombo4 ),fsTopList_items );
  
 fsType=type;
 switch ( type )
  {
   case fsVideoSelector:
        fsMessage=evFileLoaded;
        gtk_window_set_title( GTK_WINDOW( fsFileSelect ),MSGTR_FileSelect );
        fsList_items=NULL;
        for( i=0;i<fsNumberOfVideoFilterNames + 1;i++ )
          fsList_items=g_list_append( fsList_items,fsVideoFilterNames[i][0] );
        gtk_combo_set_popdown_strings( GTK_COMBO( List ),fsList_items );
        g_list_free( fsList_items );
        gtk_entry_set_text( GTK_ENTRY( fsFilterCombo ),fsVideoFilterNames[fsNumberOfVideoFilterNames - 1][0] );
        break;
   case fsSubtitleSelector:
        gtk_window_set_title( GTK_WINDOW( fsFileSelect ),MSGTR_SubtitleSelect );
        fsList_items=NULL;
        for( i=0;i<fsNumberOfSubtitleFilterNames + 1;i++ )
          fsList_items=g_list_append( fsList_items,fsSubtitleFilterNames[i][0] );
        gtk_combo_set_popdown_strings( GTK_COMBO( List ),fsList_items );
        g_list_free( fsList_items );
        gtk_entry_set_text( GTK_ENTRY( fsFilterCombo ),fsSubtitleFilterNames[fsNumberOfSubtitleFilterNames - 1][0] );
        break;
   case fsOtherSelector:
        gtk_window_set_title( GTK_WINDOW( fsFileSelect ),MSGTR_OtherSelect );
        fsList_items=NULL;
        for( i=0;i<fsNumberOfSubtitleFilterNames + 1;i++ )
          fsList_items=g_list_append( fsList_items,fsOtherFilterNames[i][0] );
        gtk_combo_set_popdown_strings( GTK_COMBO( List ),fsList_items );
        g_list_free( fsList_items );
        gtk_entry_set_text( GTK_ENTRY( fsFilterCombo ),fsOtherFilterNames[fsNumberOfOtherFilterNames][0] );
        break;
  }
 
 gtk_window_set_modal( GTK_WINDOW( fsFileSelect ),modal );

 gtk_widget_show( fsFileSelect );
}

void HideFileSelect( void )
{
 gtk_widget_hide( fsFileSelect );
 gtk_widget_destroy( fsFileSelect );
 gtkVFileSelect=0;
}

void fs_fsFileSelect_destroy( GtkObject * object,gpointer user_data )
{ HideFileSelect(); }

void fs_fsFilterCombo_activate( GtkEditable * editable,gpointer user_data )
{
 strcpy( fsFilter,gtk_entry_get_text( GTK_ENTRY( user_data ) ) );
 CheckDir( fsFNameList,get_current_dir_name() );
}

void fs_fsFilterCombo_changed( GtkEditable * editable,gpointer user_data )
{
 char * str;
 int    i;

 str=gtk_entry_get_text( GTK_ENTRY(user_data ) );

 switch ( fsType )
  {
   case fsVideoSelector:
          for( i=0;i<fsNumberOfVideoFilterNames+1;i++ )
           if( !strcmp( str,fsVideoFilterNames[i][0] ) )
            {
             strcpy( fsFilter,fsVideoFilterNames[i][1] );
             CheckDir( fsFNameList,get_current_dir_name() );
             break;
            }
          break;
   case fsSubtitleSelector:
          for( i=0;i<fsNumberOfSubtitleFilterNames+1;i++ )
           if( !strcmp( str,fsSubtitleFilterNames[i][0] ) )
            {
             strcpy( fsFilter,fsSubtitleFilterNames[i][1] );
             CheckDir( fsFNameList,get_current_dir_name() );
             break;
            }
          break;
   case fsOtherSelector:
          for( i=0;i<fsNumberOfOtherFilterNames+1;i++ )
           if( !strcmp( str,fsOtherFilterNames[i][0] ) )
            {
             strcpy( fsFilter,fsOtherFilterNames[i][1] );
             CheckDir( fsFNameList,get_current_dir_name() );
             break;
            }
          break;
  }
}

void fs_fsPathCombo_activate( GtkEditable * editable,gpointer user_data )
{
 unsigned char * str;

 str=gtk_entry_get_text( GTK_ENTRY( user_data ) );
 if ( chdir( str ) != -1 ) CheckDir( fsFNameList,get_current_dir_name() );
}

void fs_fsPathCombo_changed( GtkEditable * editable,gpointer user_data )
{
 unsigned char * str;

 str=gtk_entry_get_text( GTK_ENTRY( user_data ) );
 fsPressed=2;
 if ( chdir( str ) != -1 ) CheckDir( fsFNameList,get_current_dir_name() );
}

void fs_Up_released( GtkButton * button,gpointer user_data )
{
 chdir( ".." );
 fsSelectedFile=fsThatDir;
 CheckDir( fsFNameList,get_current_dir_name() );
 gtk_entry_set_text( GTK_ENTRY( fsPathCombo ),(unsigned char *)get_current_dir_name() );
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
   CheckDir( fsFNameList,get_current_dir_name() );
   gtk_entry_set_text( GTK_ENTRY( fsPathCombo ),(unsigned char *)get_current_dir_name() );
   return;
  }

 switch( fsPressed )
  {
   case 1:
        fsSelectedDirectory=(unsigned char *)get_current_dir_name();
        break;
   case 2:
        str=gtk_entry_get_text( GTK_ENTRY( fsPathCombo ) );
        fsSelectedFile=str;
        if ( !fsFileExist( fsSelectedFile ) ) { HideFileSelect(); return; }
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
        break;
  }
 switch ( fsType )
  {
   case fsVideoSelector:
          guiSetDF( guiIntfStruct.Filename,fsSelectedDirectory,fsSelectedFile );
          guiIntfStruct.StreamType=STREAMTYPE_FILE;
          guiIntfStruct.FilenameChanged=1;
          break;
   case fsSubtitleSelector:
          guiSetDF( guiIntfStruct.Subtitlename,fsSelectedDirectory,fsSelectedFile );
          guiIntfStruct.SubtitleChanged=1;
          break;
   case fsOtherSelector:
          guiSetDF( guiIntfStruct.Othername,fsSelectedDirectory,fsSelectedFile );
          break;
  }

 HideFileSelect();

 item=fsTopList_items;
 while( item )
  {
   if ( !strcmp( item->data,fsSelectedDirectory ) ) i=0;
   item=item->next;
  }
 if ( i ) fsTopList_items=g_list_prepend( fsTopList_items,(gchar *)get_current_dir_name() );
 if (  mplMainAutoPlay ) mplEventHandling( evPlay,0 );
}

void fs_Cancel_released( GtkButton * button,gpointer user_data )
{ HideFileSelect(); }

void fs_fsFNameList_select_row( GtkWidget * widget,gint row,gint column,GdkEventButton *bevent,gpointer user_data )
{
 gtk_clist_get_text( GTK_CLIST(widget ),row,1,&fsSelectedFile );
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

void fs_FileSelect_show( GtkWidget * widget,gpointer user_data )
{ gtkVFileSelect=(int)user_data; }

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
 GtkWidget     * hseparator3;
 GtkWidget     * hbuttonbox3;
 int             i;

 GtkWidget     * uppixmapwid;
 GdkPixmap     * uppixmap;
 GdkBitmap     * upmask;
 GtkStyle      * upstyle;

 fsFileSelect=gtk_window_new( GTK_WINDOW_TOPLEVEL );
 gtk_widget_set_name( fsFileSelect,"fsFileSelect" );
 gtk_object_set_data( GTK_OBJECT( fsFileSelect ),"fsFileSelect",fsFileSelect );
 gtk_widget_set_usize( fsFileSelect,512,300 );
 GTK_WIDGET_SET_FLAGS( fsFileSelect,GTK_CAN_DEFAULT );
 gtk_widget_set_events( fsFileSelect,GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_FOCUS_CHANGE_MASK | GDK_STRUCTURE_MASK | GDK_PROPERTY_CHANGE_MASK | GDK_VISIBILITY_NOTIFY_MASK );
 gtk_window_set_title( GTK_WINDOW( fsFileSelect ),MSGTR_FileSelect );
 gtk_window_set_position( GTK_WINDOW( fsFileSelect ),GTK_WIN_POS_CENTER );
 gtk_window_set_policy( GTK_WINDOW( fsFileSelect ),TRUE,TRUE,TRUE );
 fsColorMap=gdk_colormap_get_system();

 style=gtk_widget_get_style( fsFileSelect );
 dpixmap=gdk_pixmap_colormap_create_from_xpm_d( fsFileSelect->window,fsColorMap,&dmask,&style->bg[GTK_STATE_NORMAL],(gchar **)dir_xpm );
 fpixmap=gdk_pixmap_colormap_create_from_xpm_d( fsFileSelect->window,fsColorMap,&fmask,&style->bg[GTK_STATE_NORMAL],(gchar **)file_xpm );

 FSFrame=gtk_frame_new( NULL );
 gtk_widget_set_name( FSFrame,"FSFrame" );
 gtk_widget_ref( FSFrame );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"FSFrame",FSFrame,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( FSFrame );
 gtk_container_add( GTK_CONTAINER( fsFileSelect ),FSFrame );
 gtk_container_set_border_width( GTK_CONTAINER( FSFrame ),1 );
 gtk_frame_set_shadow_type( GTK_FRAME( FSFrame ),GTK_SHADOW_IN );

 frame2=gtk_frame_new( NULL );
 gtk_widget_set_name( frame2,"frame2" );
 gtk_widget_ref( frame2 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"frame2",frame2,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( frame2 );
 gtk_container_add( GTK_CONTAINER( FSFrame ),frame2 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame2 ),GTK_SHADOW_NONE );

 frame3=gtk_frame_new( NULL );
 gtk_widget_set_name( frame3,"frame3" );
 gtk_widget_ref( frame3 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"frame3",frame3,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( frame3 );
 gtk_container_add( GTK_CONTAINER( frame2 ),frame3 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame3 ),GTK_SHADOW_ETCHED_OUT );

 frame4=gtk_frame_new( NULL );
 gtk_widget_set_name( frame4,"frame4" );
 gtk_widget_ref( frame4 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"frame4",frame4,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( frame4 );
 gtk_container_add( GTK_CONTAINER( frame3 ),frame4 );
 gtk_container_set_border_width( GTK_CONTAINER( frame4 ),1 );
 gtk_frame_set_shadow_type( GTK_FRAME( frame4 ),GTK_SHADOW_NONE );

 vbox4=gtk_vbox_new( FALSE,0 );
 gtk_widget_set_name( vbox4,"vbox4" );
 gtk_widget_ref( vbox4 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"vbox4",vbox4,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( vbox4 );
 gtk_container_add( GTK_CONTAINER( frame4 ),vbox4 );

 hbox4=gtk_hbox_new( FALSE,0 );
 gtk_widget_set_name( hbox4,"hbox4" );
 gtk_widget_ref( hbox4 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"hbox4",hbox4,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( hbox4 );
 gtk_box_pack_start( GTK_BOX( vbox4 ),hbox4,FALSE,FALSE,0 );

 fsCombo4=gtk_combo_new();
 gtk_widget_set_name( fsCombo4,"fsCombo4" );
 gtk_widget_ref( fsCombo4 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"fsCombo4",fsCombo4,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( fsCombo4 );
 gtk_box_pack_start( GTK_BOX( hbox4 ),fsCombo4,TRUE,TRUE,0 );
 gtk_widget_set_usize( fsCombo4,-2,20 );

 fsPathCombo=GTK_COMBO( fsCombo4 )->entry;
 gtk_widget_set_name( fsPathCombo,"fsPathCombo" );
 gtk_widget_ref( fsPathCombo );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"fsPathCombo",fsPathCombo,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( fsPathCombo );
 gtk_widget_set_usize( fsPathCombo,-2,20 );

 vseparator1=gtk_vseparator_new();
 gtk_widget_set_name( vseparator1,"vseparator1" );
 gtk_widget_ref( vseparator1 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"vseparator1",vseparator1,(GtkDestroyNotify)gtk_widget_unref );
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
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"hseparator1",hseparator1,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( hseparator1 );
 gtk_box_pack_start( GTK_BOX( vbox4 ),hseparator1,FALSE,TRUE,0 );
 gtk_widget_set_usize( hseparator1,-2,8 );

 hbox6=gtk_hbox_new( FALSE,0 );
 gtk_widget_set_name( hbox6,"hbox6" );
 gtk_widget_ref( hbox6 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"hbox6",hbox6,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( hbox6 );
 gtk_box_pack_start( GTK_BOX( vbox4 ),hbox6,TRUE,TRUE,0 );

 fsFNameListWindow=gtk_scrolled_window_new( NULL,NULL );
 gtk_widget_set_name( fsFNameListWindow,"fsFNameListWindow" );
 gtk_widget_ref( fsFNameListWindow );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"fsFNameListWindow",fsFNameListWindow,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( fsFNameListWindow );
 gtk_box_pack_start( GTK_BOX( hbox6 ),fsFNameListWindow,TRUE,TRUE,0 );
 gtk_widget_set_usize( fsFNameListWindow,-2,145 );
 gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( fsFNameListWindow ),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC );

 fsFNameList=gtk_clist_new( 2 );
 gtk_widget_set_name( fsFNameList,"fsFNameList" );
 gtk_widget_ref( fsFNameList );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"fsFNameList",fsFNameList,(GtkDestroyNotify)gtk_widget_unref );
 gtk_container_add( GTK_CONTAINER( fsFNameListWindow ),fsFNameList );
 gtk_clist_set_column_width( GTK_CLIST( fsFNameList ),0,80 );
 gtk_clist_set_selection_mode( GTK_CLIST( fsFNameList ),GTK_SELECTION_BROWSE );
 gtk_clist_column_titles_hide( GTK_CLIST( fsFNameList ) );
 gtk_clist_set_shadow_type( GTK_CLIST( fsFNameList ),GTK_SHADOW_ETCHED_OUT );
 CheckDir( fsFNameList,get_current_dir_name() );

 label1=gtk_label_new( "label1" );
 gtk_widget_set_name( label1,"label1" );
 gtk_widget_ref( label1 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"label1",label1,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( label1 );
 gtk_clist_set_column_widget( GTK_CLIST( fsFNameList ),0,label1 );

 hseparator2=gtk_hseparator_new();
 gtk_widget_set_name( hseparator2,"hseparator2" );
 gtk_widget_ref( hseparator2 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"hseparator2",hseparator2,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( hseparator2 );
 gtk_box_pack_start( GTK_BOX( vbox4 ),hseparator2,FALSE,TRUE,0 );
 gtk_widget_set_usize( hseparator2,-2,9 );

 List=gtk_combo_new();
 gtk_widget_set_name( List,"List" );
 gtk_widget_ref( List );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"List",List,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( List );
 gtk_box_pack_start( GTK_BOX( vbox4 ),List,FALSE,FALSE,0 );
 gtk_widget_set_usize( List,-2,20 );
 fsList_items=NULL;
 for( i=0;i<fsNumberOfVideoFilterNames + 1;i++ )
   fsList_items=g_list_append( fsList_items,fsVideoFilterNames[i][0] );
 gtk_combo_set_popdown_strings( GTK_COMBO( List ),fsList_items );
 g_list_free( fsList_items );

 fsFilterCombo=GTK_COMBO( List )->entry;
 gtk_widget_set_name( fsFilterCombo,"fsFilterCombo" );
 gtk_widget_ref( fsFilterCombo );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"fsFilterCombo",fsFilterCombo,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( fsFilterCombo );
 gtk_entry_set_editable (GTK_ENTRY( fsFilterCombo ),FALSE );
 gtk_entry_set_text( GTK_ENTRY( fsFilterCombo ),fsVideoFilterNames[fsNumberOfVideoFilterNames - 1][0] );

 hseparator3=gtk_hseparator_new();
 gtk_widget_set_name( hseparator3,"hseparator3" );
 gtk_widget_ref( hseparator3 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"hseparator3",hseparator3,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( hseparator3 );
 gtk_box_pack_start( GTK_BOX( vbox4 ),hseparator3,FALSE,TRUE,0 );
 gtk_widget_set_usize( hseparator3,-2,7 );

 hbuttonbox3=gtk_hbutton_box_new();
 gtk_widget_set_name( hbuttonbox3,"hbuttonbox3" );
 gtk_widget_ref( hbuttonbox3 );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"hbuttonbox3",hbuttonbox3,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( hbuttonbox3 );
 gtk_box_pack_start( GTK_BOX( vbox4 ),hbuttonbox3,FALSE,TRUE,0 );
 gtk_button_box_set_layout( GTK_BUTTON_BOX( hbuttonbox3 ),GTK_BUTTONBOX_END );
 gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox3 ),10 );
 gtk_button_box_set_child_size( GTK_BUTTON_BOX( hbuttonbox3 ),85,20 );
 gtk_button_box_set_child_ipadding( GTK_BUTTON_BOX( hbuttonbox3 ),0,0 );

 fsOk=gtk_button_new_with_label( MSGTR_Ok );
 gtk_widget_set_name( fsOk,MSGTR_Ok );
 gtk_widget_ref( fsOk );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),MSGTR_Ok,fsOk,( GtkDestroyNotify )gtk_widget_unref );
 gtk_container_add( GTK_CONTAINER( hbuttonbox3 ),fsOk );
 gtk_widget_show( fsOk );

 fsCancel=gtk_button_new_with_label( MSGTR_Cancel );
 gtk_widget_set_name( fsCancel,MSGTR_Cancel );
 gtk_widget_ref( fsCancel );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),MSGTR_Cancel,fsCancel,( GtkDestroyNotify )gtk_widget_unref );
 gtk_container_add( GTK_CONTAINER( hbuttonbox3 ),fsCancel );
 gtk_widget_show( fsCancel );

 gtk_signal_connect( GTK_OBJECT( fsFileSelect ),"destroy",GTK_SIGNAL_FUNC( fs_fsFileSelect_destroy ),NULL );
 gtk_signal_connect( GTK_OBJECT( fsFileSelect ),"key_release_event",GTK_SIGNAL_FUNC( on_FileSelect_key_release_event ),NULL );

 gtk_signal_connect( GTK_OBJECT( fsFileSelect ),"show",GTK_SIGNAL_FUNC( fs_FileSelect_show ),1 );
 gtk_signal_connect( GTK_OBJECT( fsFileSelect ),"hide",GTK_SIGNAL_FUNC( fs_FileSelect_show ),0 );

 gtk_signal_connect( GTK_OBJECT( fsFilterCombo ),"changed",GTK_SIGNAL_FUNC( fs_fsFilterCombo_changed ),fsFilterCombo );
 gtk_signal_connect( GTK_OBJECT( fsFilterCombo ),"activate",GTK_SIGNAL_FUNC( fs_fsFilterCombo_activate ),fsFilterCombo );
 gtk_signal_connect( GTK_OBJECT( fsPathCombo ),"changed",GTK_SIGNAL_FUNC( fs_fsPathCombo_changed ),fsPathCombo );
 gtk_signal_connect( GTK_OBJECT( fsPathCombo ),"activate",GTK_SIGNAL_FUNC( fs_fsPathCombo_activate ),fsPathCombo );
 gtk_signal_connect( GTK_OBJECT( fsUp ),"released",GTK_SIGNAL_FUNC( fs_Up_released ),fsFNameList );
 gtk_signal_connect( GTK_OBJECT( fsOk ),"released",GTK_SIGNAL_FUNC( fs_Ok_released ),fsCombo4 );
 gtk_signal_connect( GTK_OBJECT( fsCancel ),"released",GTK_SIGNAL_FUNC( fs_Cancel_released ),NULL );
 gtk_signal_connect( GTK_OBJECT( fsFNameList ),"select_row",(GtkSignalFunc)fs_fsFNameList_select_row,NULL );

 gtk_widget_grab_focus( fsFNameList );

 return fsFileSelect;
}

