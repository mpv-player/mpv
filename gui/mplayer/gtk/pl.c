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
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "config.h"
#include "help_mp.h"
#include "stream/stream.h"

#include "gui/interface.h"
#include "gui/mplayer/widgets.h"
#include "pl.h"
#include "gtk_common.h"

static char * book_open_xpm[] = {
	"16 16 4 1",
	"       c None s None",
	".      c black",
	"X      c #808080",
	"o      c white",
	"                ",
	"  ..            ",
	" .Xo.    ...    ",
	" .Xoo. ..oo.    ",
	" .Xooo.Xooo...  ",
	" .Xooo.oooo.X.  ",
	" .Xooo.Xooo.X.  ",
	" .Xooo.oooo.X.  ",
	" .Xooo.Xooo.X.  ",
	" .Xooo.oooo.X.  ",
	"  .Xoo.Xoo..X.  ",
	"   .Xo.o..ooX.  ",
	"    .X..XXXXX.  ",
	"    ..X.......  ",
	"     ..         ",
	"                "};

static char * book_closed_xpm[] = {
	"16 16 6 1",
	"       c None s None",
	".      c black",
	"X      c blue",
	"o      c yellow",
	"O      c #007FEA",
	"#      c white",
	"                ",
	"       ..       ",
	"     ..XX.      ",
	"   ..XXXXX.     ",
	" ..XXXXXXXX.    ",
	".ooXXXXXXXXX.   ",
	"..ooXXXXXXXXX.  ",
	".X.ooXXXXXXXXX. ",
	".XX.ooXXXXXX..  ",
	" .XX.ooXXX..#O  ",
	"  .XX.oo..##OO. ",
	"   .XX..##OO..  ",
	"    .X.#OO..    ",
	"     ..O..      ",
	"      ..        ",
	"                "};

       GtkWidget * PlayList = NULL;
static GtkWidget * CTDirTree;
static GtkWidget * CLFiles;
static GtkWidget * CLSelected;
static GtkWidget * Add;
static GtkWidget * Remove;
static GtkWidget * Ok;
static GtkWidget * Cancel;
static GdkPixmap * pxOpenedBook;
static GdkPixmap * pxClosedBook;
static GdkBitmap * msOpenedBook;
static GdkBitmap * msClosedBook;

static int   NrOfEntrys = 0;
static int   NrOfSelected = 0;
static int * CLFileSelected = NULL;
static int * CLListSelected = NULL;

static int sigSel;
static int sigUnsel;

typedef struct
{
 int    scaned;
 char * path;
} DirNodeType;

static GtkCTreeNode * sibling;
static GtkCTreeNode * parent;
static gchar        * current_path;
static gchar        * old_path = NULL;

static int compare_func(const void *a, const void *b)
{
 char * tmp;
 int    i;
 if ( !a || !b || !( (DirNodeType *)a )->path ) return -1;
 tmp=strdup( (char *)b ); tmp[strlen( tmp )-1]=0;
 i=strcmp( ( (DirNodeType *)a )->path,tmp );
 free( tmp );
 return i;
}

static void scan_dir( char * path );

void ShowPlayList( void )
{
 if ( PlayList ) gtkActive( PlayList );
  else PlayList=create_PlayList();

 if ( old_path && *old_path )
  {
   char         * currentdir = strdup( old_path );
   char         * tpath,* pos;
   GtkCTreeNode * node,* nextnode;
   gboolean       leaf;
   tpath=strdup( "/" );
   pos=strtok( currentdir,"/" );
   node=gtk_ctree_find_by_row_data_custom( GTK_CTREE( CTDirTree ),NULL,"/",compare_func );
   do
    {
     char * tpathnew = g_strconcat( tpath,pos,"/",NULL );
     free( tpath ); tpath=tpathnew;
     nextnode=gtk_ctree_find_by_row_data_custom( GTK_CTREE( CTDirTree ),node,tpath,compare_func );
     if ( !nextnode ) break;
     node=nextnode;
     pos=strtok( NULL,"/" );
     gtk_ctree_get_node_info( GTK_CTREE( CTDirTree ),node,NULL,NULL,NULL,NULL,NULL,NULL,&leaf,NULL );
     if ( !leaf && pos ) gtk_ctree_expand( GTK_CTREE( CTDirTree ),node );
      else
       {
        DirNodeType * DirNode;
        gtk_ctree_select( GTK_CTREE( CTDirTree ),node );
	DirNode=gtk_ctree_node_get_row_data( GTK_CTREE( CTDirTree ),node );
	current_path=DirNode->path;
        scan_dir( DirNode->path );
        if ( CLFileSelected ) free( CLFileSelected ); CLFileSelected=calloc( 1,NrOfEntrys * sizeof( int ) );
	break;
       }
    } while( pos );
   free( tpath );
   free( currentdir );
  }
  else gtk_ctree_select( GTK_CTREE( CTDirTree ),parent );

 gtk_clist_freeze( GTK_CLIST( CLSelected ) );
 gtk_clist_clear( GTK_CLIST( CLSelected ) );
 if ( plList )
  {
   plItem * next = plList;
   while ( next || next->next )
    {
     char * text[1][3]; text[0][2]="";
     text[0][0]=next->name;
     text[0][1]=next->path;
     gtk_clist_append( GTK_CLIST( CLSelected ),text[0] );
     NrOfSelected++;
     if ( next->next ) next=next->next; else break;
    }
   CLListSelected=calloc( 1,NrOfSelected * sizeof( int ) );
  }
 gtk_clist_thaw( GTK_CLIST( CLSelected ) );

 gtk_widget_show( PlayList );
}

void HidePlayList( void )
{
 if ( !PlayList ) return;
 NrOfSelected=NrOfEntrys=0;
 gfree( (void **)&CLListSelected ); gfree( (void **)&CLFileSelected );
 if ( old_path ) free( old_path ); old_path=strdup( current_path );
 gtk_widget_hide( PlayList );
 gtk_widget_destroy( PlayList );
 PlayList=NULL;
}

static void plRowSelect( GtkCList * clist,gint row,gint column,GdkEvent * event,gpointer user_data )
{
 switch ( (int) user_data )
  {
   case 0: CLFileSelected[row]=1; break;
   case 1: CLListSelected[row]=1; break;
  }
}

static void plUnRowSelect( GtkCList * clist,gint row,gint column,GdkEvent * event,gpointer user_data )
{
 switch ( (int) user_data )
  {
   case 0: CLFileSelected[row]=0; break;
   case 1: CLListSelected[row]=0; break;
  }
}

static void plButtonReleased( GtkButton * button,gpointer user_data )
{
 switch ( (int) user_data )
 {
  case 1: // ok
       {
        int i;
	if ( plList ) gtkSet( gtkDelPl,0,NULL );
	for ( i=0;i<NrOfSelected;i++ )
	 {
	  plItem * item;
	  char * text[3];
	  item=calloc( 1,sizeof( plItem ) );
	  gtk_clist_get_text( GTK_CLIST( CLSelected ),i,0,&text[0] );
	  gtk_clist_get_text( GTK_CLIST( CLSelected ),i,1,&text[1] );
	  item->name=strdup( text[0] );
	  item->path=strdup( text[1] );
	  gtkSet( gtkAddPlItem,0,(void*)item );
	 }
	if ( plCurrent )
	 {
	  mplSetFileName( plCurrent->path,plCurrent->name,STREAMTYPE_FILE );
//	  guiSetDF( guiIntfStruct.Filename,plCurrent->path,plCurrent->name );
//	  guiIntfStruct.FilenameChanged=1;
//	  guiIntfStruct.StreamType=STREAMTYPE_FILE;
	 }
       }
  case 0: // cancel
       HidePlayList();
       break;
  case 2: // remove
       {
	int i; int j; int c=0;

	gtk_signal_handler_block( GTK_OBJECT( CLSelected ),sigSel );
	gtk_signal_handler_block( GTK_OBJECT( CLSelected ),sigUnsel );

        gtk_clist_freeze( GTK_CLIST( CLSelected ) );
        for ( i=0;i<NrOfSelected-c;i++ )
  	 if ( CLListSelected[i] )
	  {
	   gtk_clist_remove( GTK_CLIST( CLSelected ),i - c );
	   c++;
	   for ( j=i;j<NrOfSelected-c;j++ )
		CLListSelected[i] = CLListSelected[i+1];
	  }
	NrOfSelected-=c;
	gtk_clist_thaw( GTK_CLIST( CLSelected ) );

	gtk_signal_handler_unblock( GTK_OBJECT( CLSelected ),sigSel );
	gtk_signal_handler_unblock( GTK_OBJECT( CLSelected ),sigUnsel );

       }
       break;
  case 3: // add
       {
        int i;
        char * itext[1][2];
        char * text[1][3]; text[0][2]="";
        gtk_clist_freeze( GTK_CLIST( CLSelected ) );
        for ( i=0;i<NrOfEntrys;i++ )
         {
          if ( CLFileSelected[i] )
           {
	    gtk_clist_get_text( GTK_CLIST( CLFiles ),i,0,(char **)&itext );
	    text[0][0]=itext[0][0]; text[0][1]=current_path;
	    gtk_clist_append( GTK_CLIST( CLSelected ),text[0] );
	    NrOfSelected++;
	    CLListSelected=realloc( CLListSelected,NrOfSelected * sizeof( int ) );
	    CLListSelected[NrOfSelected - 1]=0;
	   }
	 }
	gtk_clist_thaw( GTK_CLIST( CLSelected ) );
       }
       break;
 }
}

static int check_for_subdir( gchar * path )
{
 DIR 	       * dir;
 struct dirent * dirent;
 struct stat     statbuf;
 gchar 	       * npath;

 if ( (dir=opendir( path )) )
  {
   while ( (dirent=readdir( dir )) )
    {
     if ( dirent->d_name[0] != '.' )
      {
       npath=calloc( 1,strlen( path ) + strlen( dirent->d_name ) + 3 );
       sprintf( npath,"%s/%s",path,dirent->d_name );
       if ( stat( npath,&statbuf ) != -1 && S_ISDIR( statbuf.st_mode ) )
        { free( npath ); closedir( dir ); return 1; }
       free( npath );
      }
    }
   closedir( dir );
  }
 return 0;
}

static void plCTree( GtkCTree * ctree,GtkCTreeNode * parent_node,gpointer user_data )
{
 GtkCTreeNode  * node;
 DirNodeType   * DirNode;
 gchar 		   * text;
 gchar 		   * dummy = "dummy";
 int     	 	 subdir = 1;
 DIR   		   * dir = NULL;
 struct dirent * dirent;
 gchar  	   * path;
 struct 		 stat statbuf;

 DirNode=gtk_ctree_node_get_row_data( ctree,parent_node );
 if ( !DirNode->scaned )
  {
   DirNode->scaned=1; current_path=DirNode->path;
   gtk_clist_freeze( GTK_CLIST( ctree ) );
   node=gtk_ctree_find_by_row_data( ctree,parent_node,NULL );
   gtk_ctree_remove_node( ctree,node );

   if ( (dir=opendir( DirNode->path ) ) )
    {
     while( (dirent=readdir( dir )) )
      {
       path=calloc( 1,strlen( DirNode->path ) + strlen( dirent->d_name ) + 2 );
       if ( !strcmp( current_path,"/" ) ) sprintf( path,"/%s",dirent->d_name );
	else sprintf( path,"%s/%s",current_path,dirent->d_name );
       text=dirent->d_name;

       if ( stat( path,&statbuf ) != -1 && S_ISDIR( statbuf.st_mode ) && dirent->d_name[0] != '.' )
	{
	 DirNode=malloc( sizeof( DirNodeType ) ); DirNode->scaned=0; DirNode->path=strdup( path );
	 subdir=check_for_subdir( path );
	 node=gtk_ctree_insert_node( ctree,parent_node,NULL,&text,4,pxOpenedBook,msOpenedBook,pxClosedBook,msClosedBook,!subdir,FALSE );
	 gtk_ctree_node_set_row_data_full( ctree,node,DirNode,NULL );
	 if ( subdir ) node=gtk_ctree_insert_node( ctree,node,NULL,&dummy,4,NULL,NULL,NULL,NULL,FALSE,FALSE );
	}
       free( path ); path=NULL;
      }
     closedir( dir );
    }

   gtk_ctree_sort_node( ctree,parent_node );
   gtk_clist_thaw( GTK_CLIST( ctree ) );
  }
}

static void scan_dir( char * path )
{
 DIR   		   * dir = NULL;
 char		   * curr;
 struct dirent * dirent;
 struct 		 stat statbuf;
 char 		   * text[1][2]; text[0][1]="";

 gtk_clist_clear( GTK_CLIST( CLFiles ) );
 if ( (dir=opendir( path )) )
  {
   NrOfEntrys=0;
   while( (dirent=readdir( dir )) )
    {
	 curr=calloc( 1,strlen( path ) + strlen( dirent->d_name ) + 3 ); sprintf( curr,"%s/%s",path,dirent->d_name );
	 if ( stat( curr,&statbuf ) != -1 && ( S_ISREG( statbuf.st_mode ) || S_ISLNK( statbuf.st_mode ) ) )
	  {
	   text[0][0]=dirent->d_name;
	   gtk_clist_append( GTK_CLIST( CLFiles ),text[0] );
	   NrOfEntrys++;
	  }
	 free( curr );
	}
   closedir( dir );
   gtk_clist_sort( GTK_CLIST( CLFiles ) );
  }
}

static void plCTRow(GtkWidget * widget, gint row, gint column, GdkEventButton * bevent, gpointer data)
{
 DirNodeType  * DirNode;
 GtkCTreeNode * node;
 node=gtk_ctree_node_nth( GTK_CTREE( widget ),row );
 DirNode=gtk_ctree_node_get_row_data( GTK_CTREE( widget ),node );
 current_path=DirNode->path;
 gtk_ctree_expand( GTK_CTREE( widget ),node );
 scan_dir( DirNode->path );
 if ( CLFileSelected ) free( CLFileSelected ); CLFileSelected=calloc( 1,NrOfEntrys * sizeof( int ) );
}

GtkWidget * create_PlayList( void )
{
  GtkWidget 	* vbox1;
  GtkWidget 	* hbox1;
  GtkWidget 	* scrolledwindow1;
  GtkWidget 	* vbox2;
  GtkWidget 	* scrolledwindow2;
  GtkWidget 	* scrolledwindow3;
  GtkWidget 	* hbuttonbox1;
  GtkAccelGroup * accel_group;
  GdkColor 	  transparent = { 0,0,0,0 };
  gchar 	* root = "/";
  gchar 	* dummy = "dummy";
  DirNodeType 	* DirNode;

  accel_group=gtk_accel_group_new();

  PlayList=gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_object_set_data( GTK_OBJECT( PlayList ),"PlayList",PlayList );
  gtk_widget_set_usize( PlayList,512,384 );
  gtk_window_set_title( GTK_WINDOW( PlayList ),MSGTR_PlayList );
  gtk_window_set_position( GTK_WINDOW( PlayList ),GTK_WIN_POS_CENTER );
//  gtk_window_set_policy( GTK_WINDOW( PlayList ),FALSE,FALSE,FALSE );
  gtk_window_set_wmclass( GTK_WINDOW( PlayList ),"Playlist","MPlayer" );

  gtk_widget_realize( PlayList );
  gtkAddIcon( PlayList );

  vbox1=AddVBox( AddDialogFrame( PlayList ),0 );
  hbox1=AddHBox( NULL,1 );
   gtk_box_pack_start( GTK_BOX( vbox1 ),hbox1,TRUE,TRUE,0 );

  scrolledwindow1=gtk_scrolled_window_new( NULL,NULL );
  gtk_widget_show( scrolledwindow1 );
  gtk_container_add( GTK_CONTAINER(
    AddFrame( NULL,0,hbox1,1 ) ),scrolledwindow1 );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolledwindow1 ),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC );

  CTDirTree=gtk_ctree_new( 1,0 );
  gtk_signal_connect( GTK_OBJECT( CTDirTree ),"tree_expand",GTK_SIGNAL_FUNC( plCTree ),(void*)0 );
  gtk_signal_connect( GTK_OBJECT( CTDirTree ),"select_row",GTK_SIGNAL_FUNC( plCTRow ),(void *)0 );
  gtk_container_add( GTK_CONTAINER( scrolledwindow1 ),CTDirTree );
  gtk_clist_set_column_auto_resize( GTK_CLIST( CTDirTree ),0,TRUE );
  gtk_clist_set_column_width( GTK_CLIST( CTDirTree ),0,80 );
  gtk_clist_set_selection_mode( GTK_CLIST( CTDirTree ),GTK_SELECTION_SINGLE );
  gtk_ctree_set_line_style( GTK_CTREE( CTDirTree ),GTK_CTREE_LINES_SOLID );
  gtk_clist_column_titles_show( GTK_CLIST( CTDirTree ) );
  gtk_clist_set_shadow_type( GTK_CLIST( CTDirTree ),GTK_SHADOW_NONE );

  if ( !pxOpenedBook ) pxOpenedBook=gdk_pixmap_create_from_xpm_d( PlayList->window,&msOpenedBook,&transparent,book_closed_xpm );
  if ( !pxClosedBook ) pxClosedBook=gdk_pixmap_create_from_xpm_d( PlayList->window,&msClosedBook,&transparent,book_open_xpm );

  parent=gtk_ctree_insert_node( GTK_CTREE( CTDirTree ),NULL,NULL,&root,4,pxOpenedBook,msOpenedBook,pxClosedBook,msClosedBook,FALSE,FALSE );
  DirNode=malloc( sizeof( DirNodeType ) );
  DirNode->scaned=0; DirNode->path=strdup( root );
  gtk_ctree_node_set_row_data_full(GTK_CTREE( CTDirTree ),parent,DirNode,NULL );
  sibling=gtk_ctree_insert_node( GTK_CTREE( CTDirTree ),parent,NULL,&dummy,4,NULL,NULL,NULL,NULL,TRUE,TRUE );
  gtk_ctree_expand( GTK_CTREE( CTDirTree ),parent );
  gtk_widget_show( CTDirTree );


  gtk_clist_set_column_widget( GTK_CLIST( CTDirTree ),0,
    AddLabel( MSGTR_PLAYLIST_DirectoryTree,NULL ) );

  vbox2=AddVBox(
    AddFrame( NULL,1,hbox1,1 ),0 );

  scrolledwindow2=gtk_scrolled_window_new( NULL,NULL );
  gtk_widget_show( scrolledwindow2 );
  gtk_box_pack_start( GTK_BOX( vbox2 ),scrolledwindow2,TRUE,TRUE,0 );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolledwindow2 ),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC );

  CLFiles=gtk_clist_new( 1 );
  gtk_widget_show( CLFiles );
  gtk_container_add( GTK_CONTAINER( scrolledwindow2 ),CLFiles );
  gtk_clist_set_column_width( GTK_CLIST( CLFiles ),0,80 );
  gtk_clist_set_selection_mode( GTK_CLIST( CLFiles ),GTK_SELECTION_EXTENDED );
  gtk_clist_column_titles_show( GTK_CLIST( CLFiles ) );
  gtk_clist_set_shadow_type( GTK_CLIST( CLFiles ),GTK_SHADOW_NONE );

  gtk_clist_set_column_widget( GTK_CLIST( CLFiles ),0,
    AddLabel( MSGTR_PLAYLIST_Files,NULL ) );

  AddHSeparator( vbox2 );

  scrolledwindow3=gtk_scrolled_window_new( NULL,NULL );
  gtk_widget_show( scrolledwindow3 );
  gtk_box_pack_start( GTK_BOX( vbox2 ),scrolledwindow3,TRUE,TRUE,0 );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolledwindow3 ),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC );

  CLSelected=gtk_clist_new( 2 );
  gtk_widget_show( CLSelected );
  gtk_container_add( GTK_CONTAINER( scrolledwindow3 ),CLSelected );
  gtk_clist_set_column_width( GTK_CLIST( CLSelected ),0,295 );
  gtk_clist_set_column_width( GTK_CLIST( CLSelected ),1,295 );
  gtk_clist_set_selection_mode( GTK_CLIST( CLSelected ),GTK_SELECTION_MULTIPLE );
  gtk_clist_column_titles_show( GTK_CLIST( CLSelected ) );
  gtk_clist_set_shadow_type( GTK_CLIST( CLSelected ),GTK_SHADOW_NONE );

  gtk_clist_set_column_widget( GTK_CLIST( CLSelected ),0,
    AddLabel( MSGTR_PLAYLIST_Selected,NULL ) );

  gtk_clist_set_column_widget( GTK_CLIST( CLSelected ),1,
    AddLabel( MSGTR_PLAYLIST_Path,NULL ) );

  AddHSeparator( vbox1 );

  hbuttonbox1=AddHButtonBox( vbox1 );
    gtk_button_box_set_layout( GTK_BUTTON_BOX( hbuttonbox1 ),GTK_BUTTONBOX_END );
    gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox1 ),10 );

  Add=AddButton( MSGTR_Add,hbuttonbox1 );
  Remove=AddButton( MSGTR_Remove,hbuttonbox1 );
  Ok=AddButton( MSGTR_Ok,hbuttonbox1 );
  Cancel=AddButton( MSGTR_Cancel,hbuttonbox1 );

  gtk_widget_add_accelerator( Cancel,"clicked",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );

  gtk_signal_connect( GTK_OBJECT( PlayList ),"destroy",GTK_SIGNAL_FUNC( WidgetDestroy ),&PlayList );

  gtk_signal_connect( GTK_OBJECT( CLFiles ),"select_row",GTK_SIGNAL_FUNC( plRowSelect ),(void *)0 );
  gtk_signal_connect( GTK_OBJECT( CLFiles ),"unselect_row",GTK_SIGNAL_FUNC( plUnRowSelect ),(void *)0 );
  sigSel=gtk_signal_connect( GTK_OBJECT( CLSelected ),"select_row",GTK_SIGNAL_FUNC( plRowSelect ),(void*)1 );
  sigUnsel=gtk_signal_connect( GTK_OBJECT( CLSelected ),"unselect_row",GTK_SIGNAL_FUNC( plUnRowSelect ),(void*)1 );

  gtk_signal_connect( GTK_OBJECT( Add ),"clicked",GTK_SIGNAL_FUNC( plButtonReleased ),(void*)3 );
  gtk_signal_connect( GTK_OBJECT( Remove ),"clicked",GTK_SIGNAL_FUNC( plButtonReleased ),(void*)2 );
  gtk_signal_connect( GTK_OBJECT( Ok ),"clicked",GTK_SIGNAL_FUNC( plButtonReleased ),(void*)1 );
  gtk_signal_connect( GTK_OBJECT( Cancel ),"clicked",GTK_SIGNAL_FUNC( plButtonReleased ),(void*)0 );

  gtk_window_add_accel_group( GTK_WINDOW( PlayList ),accel_group );

  return PlayList;
}
