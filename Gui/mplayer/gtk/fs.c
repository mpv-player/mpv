
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <glob.h>
#include <unistd.h>

#include "../mplayer.h"

#include "mplayer/pixmaps/up.xpm"
#include "mplayer/pixmaps/dir.xpm"
#include "mplayer/pixmaps/file.xpm"

#include "../../app.h"
#include "../../interface.h"
#include "../../../config.h"
#include "../../../help_mp.h"
#include "../../../libmpdemux/stream.h"

#include "../widgets.h"
#include "fs.h"
#include "opts.h"
#include "common.h"

#ifndef __linux__
#define get_current_dir_name()  getcwd(NULL, PATH_MAX)
#endif

#ifndef get_current_dir_name
 extern char * get_current_dir_name( void );
#endif

gchar         * fsSelectedFile = NULL;
gchar         * fsSelectedDirectory = NULL;
unsigned char * fsThatDir = ".";
gchar         * fsFilter = "*";

int             fsPressed = 0;
int             fsType    = 0;

char * fsVideoFilterNames[][2] =
         { { "MPEG files (*.mpg,*.mpeg,*.m1v)",                         "*.mpg,*.mpeg,*.m1v" },
           { "VOB files (*.vob)",  				  	"*.vob" },
           { "AVI files (*.avi)",  				  	"*.avi" },
	   { "DiVX files (*.divx)",					"*.divx" },
           { "QuickTime files (*.mov,*.qt)",			  	"*.mov,*.qt" },
           { "ASF files (*.asf)",  				  	"*.asf" },
           { "VIVO files (*.viv)", 				  	"*.viv" },
	   { "RealVideo files (*.rm)",					"*.rm"  },
	   { "Windows Media Video (*.wmv)",			  	"*.wmv" },
	   { "OGG Media files (*.ogm)",			  		"*.ogm" },
	   { "Autodesk animations (*.fli,*.flc)",			"*.fli,*.flc" },
	   { "NuppelVideo files (*.nuv)",				"*.nuv" },
	   { "MP3 files (*.mp3,mp2)",					"*.mp3,*.mp2" },
	   { "Wave files (*.wav)",					"*.wav" },
	   { "WMA files (*.wma)",					"*.wma" },
	   { "Audio files",						"*.wav,*.ogg,*.mp2,*.mp3,*.wma" },
	   { "Video files", 						"*.asf,*.avi,*.divx,*.fli,*.flc,*.ogm,*.mpg,*.mpeg,*.m1v,*.mov,*.nuv,*.qt,*.rm,*.vob,*.viv,*.wmv" },
           { "All files",	      					"*" },
	   { NULL,NULL }
	 };

char * fsSubtitleFilterNames[][2] =
         { { "UTF (*.utf)",  						   "*.utf" },
           { "SUB (*.sub)",   						   "*.sub" },
           { "SRT (*.srt)",   						   "*.str" },
           { "SMI (*.smi)",   						   "*.smi" },
           { "RT  (*.rt) ",   						   "*.rt"  },
           { "TXT (*.txt)",   						   "*.txt" },
           { "SSA (*.ssa)",   						   "*.ssa" },
           { "AQT (*.aqt)",   						   "*.aqt" },
	   { "Subtitles",						   "*.utf,*.sub,*.srt,*.smi,*.rt,*.txt,*.ssa,*.aqt" },
           { "All files",	 					   "*" },
	   { NULL,NULL }
	 };

char * fsOtherFilterNames[][2] =
         { 
	   { "All files", "*"     },
	   { NULL,NULL }
	 };
	 
char * fsAudioFileNames[][2] =
	 { 
	   { "WAV files (*.wav)",					   "*.wav" },
	   { "MP3 files (*.mp2, *.mp3)",				   "*.mp2,*.mp3" },
	   { "OGG Vorbis files (*.ogg)",				   "*.ogg" },
	   { "Audio files",						   "*.ogg,*.mp2,*.mp3,*.wav" },
	   { "All files",						   "*" },
	   { NULL, NULL }
	 };

char * fsFontFileNames[][2] =
         {
#ifdef HAVE_FREETYPE
	   { "True Type fonts (*.ttf)",					   "*.ttf" },
	   { "Type1 fonts (*.pfb)",					   "*.pfb" },
	   { "All fonts",						   "*.ttf,*.pfb" },
#else
	   { "font files (*.desc)",					   "*.desc" },
#endif
	   { "All files",						   "*" },
	   { NULL,NULL }
	 };

GtkWidget   * fsFileNamesList;
GtkWidget   * fsFNameList;
GtkWidget   * fsFileSelect = NULL;
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

static char * Filter( char * name )
{
 static char tmp[32];
 int  i,c;
 for ( i=0,c=0;i < strlen( name );i++ )
  {
   if ( ( name[i] >='a' )&&( name[i] <= 'z' ) ) { tmp[c++]='['; tmp[c++]=name[i]; tmp[c++]=name[i] - 32; tmp[c++]=']'; }
    else tmp[c++]=name[i];
  }
 tmp[c]=0;
 return tmp;
}

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
 str[0][0]=NULL;
 str[0][1]=".";  gtk_clist_append( GTK_CLIST( list ),str[0] ); gtk_clist_set_pixmap( GTK_CLIST( list ),0,0,pixmap,mask );
 str[0][1]=".."; gtk_clist_append( GTK_CLIST( list ),str[0] ); gtk_clist_set_pixmap( GTK_CLIST( list ),1,0,pixmap,mask );

 glob( "*",0,NULL,&gg );
// glob( ".*",GLOB_NOSORT | GLOB_APPEND,NULL,&gg );
 for(  i=0;(unsigned)i<gg.gl_pathc;i++ )
  {
   stat( gg.gl_pathv[i],&fs );
   if( !S_ISDIR( fs.st_mode ) ) continue;

   str[0][1]=gg.gl_pathv[i];
   pixmap=dpixmap; mask=dmask;
   gtk_clist_append( GTK_CLIST( list ),str[0] );
   gtk_clist_set_pixmap( GTK_CLIST( list ),c++,0,pixmap,mask );
  }
 globfree( &gg );

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
       glob( Filter( tmp ),glob_param,NULL,&gg ); 
       glob_param=GLOB_APPEND;
      }
    }
  } else glob( Filter( fsFilter ),0,NULL,&gg );

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
 for(  i=0;(unsigned)i<gg.gl_pathc;i++ )
  {
   stat( gg.gl_pathv[i],&fs );
   if(  S_ISDIR( fs.st_mode ) ) continue;

   str[0][1]=gg.gl_pathv[i];
   gtk_clist_append( GTK_CLIST( list ),str[0] );
   gtk_clist_set_pixmap( GTK_CLIST( list ),c++,0,pixmap,mask );
  }
 globfree( &gg );

 gtk_clist_set_column_width( GTK_CLIST( list ),0,17 );
 gtk_clist_select_row( GTK_CLIST( list ),0,1 );
 gtk_widget_show( list );
}

void fs_PersistantHistory( char *subject ); /* forward declaration */

void ShowFileSelect( int type,int modal )
{
 int i;
 char * tmp = NULL;

 if ( fsFileSelect ) gtkActive( fsFileSelect );
  else fsFileSelect=create_FileSelect();
 
 fsType=type;
 switch ( type )
  {
   case fsVideoSelector:
        gtk_window_set_title( GTK_WINDOW( fsFileSelect ),MSGTR_FileSelect );
        fsList_items=NULL;
        for( i=0;fsVideoFilterNames[i][0];i++ )
          fsList_items=g_list_append( fsList_items,fsVideoFilterNames[i][0] );
        gtk_combo_set_popdown_strings( GTK_COMBO( List ),fsList_items );
        g_list_free( fsList_items );
        gtk_entry_set_text( GTK_ENTRY( fsFilterCombo ),fsVideoFilterNames[i-2][0] );
	tmp=guiIntfStruct.Filename;
        break;
   case fsSubtitleSelector:
        gtk_window_set_title( GTK_WINDOW( fsFileSelect ),MSGTR_SubtitleSelect );
        fsList_items=NULL;
        for( i=0;fsSubtitleFilterNames[i][0];i++ )
          fsList_items=g_list_append( fsList_items,fsSubtitleFilterNames[i][0] );
        gtk_combo_set_popdown_strings( GTK_COMBO( List ),fsList_items );
        g_list_free( fsList_items );
        gtk_entry_set_text( GTK_ENTRY( fsFilterCombo ),fsSubtitleFilterNames[i-2][0] );
	tmp=guiIntfStruct.Subtitlename;
        break;
   case fsOtherSelector:
        gtk_window_set_title( GTK_WINDOW( fsFileSelect ),MSGTR_OtherSelect );
        fsList_items=NULL;
        for( i=0;fsOtherFilterNames[i][0];i++ )
          fsList_items=g_list_append( fsList_items,fsOtherFilterNames[i][0] );
        gtk_combo_set_popdown_strings( GTK_COMBO( List ),fsList_items );
        g_list_free( fsList_items );
        gtk_entry_set_text( GTK_ENTRY( fsFilterCombo ),fsOtherFilterNames[0][0] );
	tmp=guiIntfStruct.Othername;
        break;
   case fsAudioSelector:
	gtk_window_set_title( GTK_WINDOW( fsFileSelect ),MSGTR_AudioFileSelect );
	fsList_items=NULL;
	for( i=0;fsAudioFileNames[i][0];i++ )
	  fsList_items=g_list_append( fsList_items,fsAudioFileNames[i][0] );
	gtk_combo_set_popdown_strings( GTK_COMBO( List ),fsList_items );
	g_list_free( fsList_items );
	gtk_entry_set_text( GTK_ENTRY( fsFilterCombo ),fsAudioFileNames[i-2][0] );
	tmp=guiIntfStruct.AudioFile;
	break;
   case fsFontSelector:
        gtk_window_set_title( GTK_WINDOW( fsFileSelect ),MSGTR_FontSelect );
	fsList_items=NULL;
	for( i=0;fsFontFileNames[i][0];i++ )
	  fsList_items=g_list_append( fsList_items,fsFontFileNames[i][0] );
	gtk_combo_set_popdown_strings( GTK_COMBO( List ),fsList_items );
	g_list_free( fsList_items );
	gtk_entry_set_text( GTK_ENTRY( fsFilterCombo ),fsFontFileNames[i-2][0] );
	tmp=font_name;
	break;
  }

 if ( tmp && tmp[0] )
  {
   struct stat f;
   char * dir = strdup( tmp );

   do 
    {
     char * c = strrchr( dir,'/' );
     stat( dir,&f );
     if ( S_ISDIR( f.st_mode ) ) break;
     if ( c ) *c=0;
    } while ( strrchr( dir,'/' ) );

   if ( dir[0] ) chdir( dir );
   
   free( dir );
  }
 
 if ( fsTopList_items ) g_list_free( fsTopList_items ); fsTopList_items=NULL;
 {
  char * hist;
  int  i, c = 1;
  
  if ( fsType == fsVideoSelector )
   {
    for ( i=0;i < fsPersistant_MaxPos;i++ )
     if ( fsHistory[i] ) { fsTopList_items=g_list_append( fsTopList_items,fsHistory[i] ); c=0; }
   }
  if ( c ) fsTopList_items=g_list_append( fsTopList_items,(gchar *)get_current_dir_name() );
 }
 if ( getenv( "HOME" ) ) fsTopList_items=g_list_append( fsTopList_items,getenv( "HOME" ) );
 fsTopList_items=g_list_append( fsTopList_items,"/home" );
 fsTopList_items=g_list_append( fsTopList_items,"/mnt" );
 fsTopList_items=g_list_append( fsTopList_items,"/" );
 gtk_combo_set_popdown_strings( GTK_COMBO( fsCombo4 ),fsTopList_items );
  
 gtk_window_set_modal( GTK_WINDOW( fsFileSelect ),modal );

 gtk_widget_show( fsFileSelect );
}

void HideFileSelect( void )
{
 if ( !fsFileSelect ) return;
 gtk_widget_hide( fsFileSelect );
 gtk_widget_destroy( fsFileSelect );
 fsFileSelect=NULL;
}

void fs_PersistantHistory( char * subject )
{
 int i;

 if ( fsType != fsVideoSelector ) return;

 for ( i=0;i < fsPersistant_MaxPos;i++ )
  if ( fsHistory[i] && !strcmp( fsHistory[i],subject ) )
   {
    char * tmp = fsHistory[i]; fsHistory[i]=fsHistory[0]; fsHistory[0]=tmp;
    return;
   }
 gfree( (void **)&fsHistory[fsPersistant_MaxPos - 1] );
 for ( i=fsPersistant_MaxPos - 1;i;i-- ) fsHistory[i]=fsHistory[i - 1];
 fsHistory[0]=gstrdup( subject );
}
//-----------------------------------------------

void fs_fsFilterCombo_activate( GtkEditable * editable,gpointer user_data )
{
 fsFilter=gtk_entry_get_text( GTK_ENTRY( user_data ) );
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
          for( i=0;fsVideoFilterNames[i][0];i++ )
           if( !strcmp( str,fsVideoFilterNames[i][0] ) )
            { fsFilter=fsVideoFilterNames[i][1]; break; }
          break;
   case fsSubtitleSelector:
          for( i=0;fsSubtitleFilterNames[i][0];i++ )
           if( !strcmp( str,fsSubtitleFilterNames[i][0] ) )
            { fsFilter=fsSubtitleFilterNames[i][1]; break; }
          break;
   case fsOtherSelector:
          for( i=0;fsOtherFilterNames[i][0];i++ )
           if( !strcmp( str,fsOtherFilterNames[i][0] ) )
            { fsFilter=fsOtherFilterNames[i][1]; break; }
          break;
   case fsAudioSelector:
          for( i=0;fsAudioFileNames[i][0];i++ )
           if( !strcmp( str,fsAudioFileNames[i][0] ) )
            { fsFilter=fsAudioFileNames[i][1]; break; }
	  break;
   case fsFontSelector:
          for( i=0;fsFontFileNames[i][0];i++ )
	    if( !strcmp( str,fsFontFileNames[i][0] ) )
	     { fsFilter=fsFontFileNames[i][1]; break; }
	  break;
   default: return;
  }
 CheckDir( fsFNameList,get_current_dir_name() );
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
          guiIntfStruct.FilenameChanged=1; sub_fps=0;
	  gfree( (void **)&guiIntfStruct.AudioFile );
	  gfree( (void **)&guiIntfStruct.Subtitlename );
          fs_PersistantHistory( fsSelectedDirectory );      //totem, write into history
          break;
#ifdef USE_SUB
   case fsSubtitleSelector:
          guiSetDF( guiIntfStruct.Subtitlename,fsSelectedDirectory,fsSelectedFile );
	  guiLoadSubtitle( guiIntfStruct.Subtitlename );
          break;
#endif
   case fsOtherSelector:
          guiSetDF( guiIntfStruct.Othername,fsSelectedDirectory,fsSelectedFile );
          break;
   case fsAudioSelector:
          guiSetDF( guiIntfStruct.AudioFile,fsSelectedDirectory,fsSelectedFile );
          break;
   case fsFontSelector:
          guiSetDF( font_name,fsSelectedDirectory,fsSelectedFile );
#if defined( USE_OSD ) || defined( USE_SUB )
	  guiLoadFont();
#endif
	  if ( Preferences ) gtk_entry_set_text( GTK_ENTRY( prEFontName ),font_name );
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
 if ( mplMainAutoPlay ) { mplMainAutoPlay=0; mplEventHandling( evPlay,0 ); }
}

void fs_Cancel_released( GtkButton * button,gpointer user_data )
{
 HideFileSelect();
 fs_PersistantHistory( get_current_dir_name() );      //totem, write into history file
}

void fs_fsFNameList_select_row( GtkWidget * widget,gint row,gint column,GdkEventButton *bevent,gpointer user_data )
{
 gtk_clist_get_text( GTK_CLIST(widget ),row,1,&fsSelectedFile );
 fsPressed=1;
 if( bevent && bevent->type == GDK_BUTTON_PRESS )  gtk_button_released( GTK_BUTTON( fsOk ) );
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
 GtkWidget     * hseparator3;
 GtkWidget     * hbuttonbox3;

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
 gtk_window_set_wmclass( GTK_WINDOW( fsFileSelect ),"FileSelect","MPlayer" );
 fsColorMap=gdk_colormap_get_system();
 
 gtk_widget_realize( fsFileSelect );
 gtkAddIcon( fsFileSelect );

 style=gtk_widget_get_style( fsFileSelect );
 dpixmap=gdk_pixmap_colormap_create_from_xpm_d( fsFileSelect->window,fsColorMap,&dmask,&style->bg[GTK_STATE_NORMAL],(gchar **)dir_xpm );
 fpixmap=gdk_pixmap_colormap_create_from_xpm_d( fsFileSelect->window,fsColorMap,&fmask,&style->bg[GTK_STATE_NORMAL],(gchar **)file_xpm );

 vbox4=AddVBox( AddDialogFrame( fsFileSelect ),0 );
 hbox4=AddHBox( vbox4,1 );

 fsCombo4=gtk_combo_new();
 gtk_widget_set_name( fsCombo4,"fsCombo4" );
 gtk_widget_show( fsCombo4 );
 gtk_box_pack_start( GTK_BOX( hbox4 ),fsCombo4,TRUE,TRUE,0 );
 gtk_widget_set_usize( fsCombo4,-2,20 );

 fsPathCombo=GTK_COMBO( fsCombo4 )->entry;
 gtk_widget_set_name( fsPathCombo,"fsPathCombo" );
 gtk_widget_show( fsPathCombo );
 gtk_widget_set_usize( fsPathCombo,-2,20 );

 vseparator1=gtk_vseparator_new();
 gtk_widget_set_name( vseparator1,"vseparator1" );
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

 AddHSeparator( vbox4 );

 hbox6=AddHBox( NULL,0 );
   gtk_box_pack_start( GTK_BOX( vbox4 ),hbox6,TRUE,TRUE,0 );

 fsFNameListWindow=gtk_scrolled_window_new( NULL,NULL );
 gtk_widget_set_name( fsFNameListWindow,"fsFNameListWindow" );
 gtk_widget_show( fsFNameListWindow );
 gtk_box_pack_start( GTK_BOX( hbox6 ),fsFNameListWindow,TRUE,TRUE,0 );
 gtk_widget_set_usize( fsFNameListWindow,-2,145 );
 gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( fsFNameListWindow ),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC );

 fsFNameList=gtk_clist_new( 2 );
 gtk_widget_set_name( fsFNameList,"fsFNameList" );
 gtk_container_add( GTK_CONTAINER( fsFNameListWindow ),fsFNameList );
 gtk_clist_set_column_width( GTK_CLIST( fsFNameList ),0,80 );
 gtk_clist_set_selection_mode( GTK_CLIST( fsFNameList ),GTK_SELECTION_BROWSE );
 gtk_clist_column_titles_hide( GTK_CLIST( fsFNameList ) );
 gtk_clist_set_shadow_type( GTK_CLIST( fsFNameList ),GTK_SHADOW_ETCHED_OUT );

 AddHSeparator( vbox4 );

 List=gtk_combo_new();
 gtk_widget_set_name( List,"List" );
 gtk_widget_ref( List );
 gtk_object_set_data_full( GTK_OBJECT( fsFileSelect ),"List",List,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( List );
 gtk_box_pack_start( GTK_BOX( vbox4 ),List,FALSE,FALSE,0 );
 gtk_widget_set_usize( List,-2,20 );

 fsFilterCombo=GTK_COMBO( List )->entry;
 gtk_widget_set_name( fsFilterCombo,"fsFilterCombo" );
 gtk_widget_show( fsFilterCombo );
 gtk_entry_set_editable (GTK_ENTRY( fsFilterCombo ),FALSE );

 AddHSeparator( vbox4 );

 hbuttonbox3=AddHButtonBox( vbox4 );
   gtk_button_box_set_layout( GTK_BUTTON_BOX( hbuttonbox3 ),GTK_BUTTONBOX_END );
   gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox3 ),10 );

 fsOk=AddButton( MSGTR_Ok,hbuttonbox3 );
 fsCancel=AddButton( MSGTR_Cancel,hbuttonbox3 );

 gtk_signal_connect( GTK_OBJECT( fsFileSelect ),"destroy",GTK_SIGNAL_FUNC( WidgetDestroy ),&fsFileSelect );
 gtk_signal_connect( GTK_OBJECT( fsFileSelect ),"key_release_event",GTK_SIGNAL_FUNC( on_FileSelect_key_release_event ),NULL );

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

