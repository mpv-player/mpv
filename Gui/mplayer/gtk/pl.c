
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "../../../config.h"
#include "../../../help_mp.h"

#include "../../interface.h"
#include "../widgets.h"
#include "pl.h"

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
	"X      c red",
	"o      c yellow",
	"O      c #808080",
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

       GtkWidget * PlayList;
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

static int   gtkVPlaylist = 0;
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
 if ( gtkVPlaylist ) gtkActive( PlayList );
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
 if ( !gtkVPlaylist ) return;
 gtkVPlaylist=NrOfSelected=NrOfEntrys=0;
 if ( CLListSelected ) free( CLListSelected ); CLListSelected=NULL;
 if ( CLFileSelected ) free( CLFileSelected ); CLFileSelected=NULL;
 if ( old_path ) free( old_path ); old_path=strdup( current_path );
 gtk_widget_hide( PlayList );
 gtk_widget_destroy( PlayList );
}

static void plDestroy( GtkObject * object,gpointer user_data )
{ HidePlayList(); }

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
	  guiSetDF( guiIntfStruct.Filename,plCurrent->path,plCurrent->name );
	  guiIntfStruct.FilenameChanged=1;
	  guiIntfStruct.StreamType=STREAMTYPE_FILE;
	 }
       }
  case 0: // cancel
       HidePlayList(); 
       break;
  case 2: // remove
       {
	int i; int c=0;

	gtk_signal_handler_block( GTK_OBJECT( CLSelected ),sigSel );
	gtk_signal_handler_block( GTK_OBJECT( CLSelected ),sigUnsel );

        gtk_clist_freeze( GTK_CLIST( CLSelected ) );
        for ( i=0;i<NrOfSelected;i++ )
  	 if ( CLListSelected[i] ) 
	  {
	   gtk_clist_remove( GTK_CLIST( CLSelected ),i - c );
	   c++;
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
	    gtk_clist_get_text( GTK_CLIST( CLFiles ),i,0,&itext );
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

static void plShow( GtkWidget * widget,gpointer user_data )
{ gtkVPlaylist=(int)user_data; }

GtkWidget * create_PlayList( void )
{
  GtkWidget 	* frame1;
  GtkWidget 	* frame2;
  GtkWidget 	* frame3;
  GtkWidget 	* frame4;
  GtkWidget 	* vbox1;
  GtkWidget 	* hbox1;
  GtkWidget 	* frame5;
  GtkWidget 	* scrolledwindow1;
  GtkWidget 	* label2;
  GtkWidget 	* frame6;
  GtkWidget 	* vbox2;
  GtkWidget 	* scrolledwindow2;
  GtkWidget 	* label3;
  GtkWidget 	* hseparator2;
  GtkWidget 	* scrolledwindow3;
  GtkWidget 	* label5;
  GtkWidget 	* hseparator1;
  GtkWidget 	* hbuttonbox1;
  GtkAccelGroup * accel_group;
  GdkColor 		  transparent = { 0 };
  gchar 		* root = "/";
  gchar 		* dummy = "dummy";
  DirNodeType 	* DirNode;

  accel_group=gtk_accel_group_new();

  PlayList=gtk_window_new( GTK_WINDOW_DIALOG );
  gtk_object_set_data( GTK_OBJECT( PlayList ),"PlayList",PlayList );
  gtk_widget_set_usize( PlayList,512,300 );
  gtk_window_set_title( GTK_WINDOW( PlayList ),MSGTR_PlayList );
  gtk_window_set_position( GTK_WINDOW( PlayList ),GTK_WIN_POS_CENTER );
//  gtk_window_set_policy( GTK_WINDOW( PlayList ),FALSE,FALSE,FALSE );
  gtk_window_set_wmclass( GTK_WINDOW( PlayList ),"Playlist","MPlayer" );

  gtk_widget_realize( PlayList );
  gtkAddIcon( PlayList );

  frame1=gtk_frame_new( NULL );
  gtk_widget_ref( frame1 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"frame1",frame1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( frame1 );
  gtk_container_add( GTK_CONTAINER( PlayList ),frame1 );
  gtk_container_set_border_width( GTK_CONTAINER( frame1 ),1 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame1 ),GTK_SHADOW_IN );

  frame2=gtk_frame_new( NULL );
  gtk_widget_ref( frame2 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"frame2",frame2,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( frame2 );
  gtk_container_add( GTK_CONTAINER( frame1 ),frame2 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame2 ),GTK_SHADOW_NONE );

  frame3=gtk_frame_new( NULL );
  gtk_widget_ref( frame3 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"frame3",frame3,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( frame3 );
  gtk_container_add( GTK_CONTAINER( frame2 ),frame3 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame3 ),GTK_SHADOW_ETCHED_OUT );

  frame4=gtk_frame_new( NULL );
  gtk_widget_ref( frame4 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"frame4",frame4,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( frame4 );
  gtk_container_add( GTK_CONTAINER( frame3 ),frame4 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame4 ),GTK_SHADOW_NONE );

  vbox1=gtk_vbox_new( FALSE,0 );
  gtk_widget_ref( vbox1 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"vbox1",vbox1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( vbox1 );
  gtk_container_add( GTK_CONTAINER( frame4 ),vbox1 );

  hbox1=gtk_hbox_new( FALSE,0 );
  gtk_widget_ref( hbox1 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"hbox1",hbox1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( hbox1 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),hbox1,TRUE,TRUE,0 );

  frame5=gtk_frame_new( NULL );
  gtk_widget_ref( frame5 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"frame5",frame5,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( frame5 );
  gtk_box_pack_start( GTK_BOX( hbox1 ),frame5,TRUE,TRUE,0 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame5 ),GTK_SHADOW_ETCHED_OUT );

  scrolledwindow1=gtk_scrolled_window_new( NULL,NULL );
  gtk_widget_ref( scrolledwindow1 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"scrolledwindow1",scrolledwindow1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( scrolledwindow1 );
  gtk_container_add( GTK_CONTAINER( frame5 ),scrolledwindow1 );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolledwindow1 ),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC );

  CTDirTree=gtk_ctree_new( 1,0 );
  gtk_widget_ref( CTDirTree );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"CTDirTree",CTDirTree,(GtkDestroyNotify)gtk_widget_unref );
  gtk_signal_connect( GTK_OBJECT( CTDirTree ),"tree_expand",GTK_SIGNAL_FUNC( plCTree ),(void*)0 );
  gtk_signal_connect( GTK_OBJECT( CTDirTree ),"select_row",GTK_SIGNAL_FUNC( plCTRow ),(void *)0 );
  gtk_container_add( GTK_CONTAINER( scrolledwindow1 ),CTDirTree );
  gtk_clist_set_column_auto_resize( GTK_CLIST( CTDirTree ),0,TRUE );
  gtk_clist_set_column_width( GTK_CLIST( CTDirTree ),0,80 );
  gtk_clist_set_selection_mode( GTK_CLIST( CTDirTree ),GTK_SELECTION_SINGLE );
  gtk_ctree_set_line_style( GTK_CTREE( CTDirTree ),GTK_CTREE_LINES_SOLID );
  gtk_clist_column_titles_show( GTK_CLIST( CTDirTree ) );
  gtk_clist_set_shadow_type( GTK_CLIST( CTDirTree ),GTK_SHADOW_NONE );

  gtk_widget_realize( PlayList );

  if ( !pxOpenedBook ) pxOpenedBook=gdk_pixmap_create_from_xpm_d( PlayList->window,&msOpenedBook,&transparent,book_closed_xpm );
  if ( !pxClosedBook ) pxClosedBook=gdk_pixmap_create_from_xpm_d( PlayList->window,&msClosedBook,&transparent,book_open_xpm );

  parent=gtk_ctree_insert_node( GTK_CTREE( CTDirTree ),NULL,NULL,&root,4,pxOpenedBook,msOpenedBook,pxClosedBook,msClosedBook,FALSE,FALSE );
  DirNode=malloc( sizeof( DirNodeType ) );
  DirNode->scaned=0; DirNode->path=strdup( root );
  gtk_ctree_node_set_row_data_full(GTK_CTREE( CTDirTree ),parent,DirNode,NULL );
  sibling=gtk_ctree_insert_node( GTK_CTREE( CTDirTree ),parent,NULL,&dummy,4,NULL,NULL,NULL,NULL,TRUE,TRUE );
  gtk_ctree_expand( GTK_CTREE( CTDirTree ),parent );
  gtk_widget_show( CTDirTree );
  
  label2=gtk_label_new( MSGTR_PLAYLIST_DirectoryTree );
  gtk_widget_ref( label2 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"label2",label2,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label2 );
  gtk_clist_set_column_widget( GTK_CLIST( CTDirTree ),0,label2 );
  gtk_misc_set_alignment( GTK_MISC( label2 ),0.02,0.5 );

  frame6=gtk_frame_new( NULL );
  gtk_widget_ref( frame6 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"frame6",frame6,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( frame6 );
  gtk_box_pack_start( GTK_BOX( hbox1 ),frame6,TRUE,TRUE,0 );
  gtk_widget_set_usize( frame6,170,-2 );
  gtk_frame_set_shadow_type( GTK_FRAME( frame6 ),GTK_SHADOW_ETCHED_OUT );

  vbox2=gtk_vbox_new( FALSE,0 );
  gtk_widget_ref( vbox2 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"vbox2",vbox2,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( vbox2 );
  gtk_container_add( GTK_CONTAINER( frame6 ),vbox2 );

  scrolledwindow2=gtk_scrolled_window_new( NULL,NULL );
  gtk_widget_ref( scrolledwindow2 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"scrolledwindow2",scrolledwindow2,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( scrolledwindow2 );
  gtk_box_pack_start( GTK_BOX( vbox2 ),scrolledwindow2,TRUE,TRUE,0 );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolledwindow2 ),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC );

  CLFiles=gtk_clist_new( 1 );
  gtk_widget_ref( CLFiles );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"CLFiles",CLFiles,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CLFiles );
  gtk_container_add( GTK_CONTAINER( scrolledwindow2 ),CLFiles );
  gtk_clist_set_column_width( GTK_CLIST( CLFiles ),0,80 );
  gtk_clist_set_selection_mode( GTK_CLIST( CLFiles ),GTK_SELECTION_EXTENDED );
  gtk_clist_column_titles_show( GTK_CLIST( CLFiles ) );
  gtk_clist_set_shadow_type( GTK_CLIST( CLFiles ),GTK_SHADOW_NONE );

  label3=gtk_label_new( MSGTR_PLAYLIST_Files );
  gtk_widget_ref( label3 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"label3",label3,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label3 );
  gtk_clist_set_column_widget( GTK_CLIST( CLFiles ),0,label3 );
  gtk_misc_set_alignment( GTK_MISC( label3 ),0.02,0.5 );

  hseparator2=gtk_hseparator_new();
  gtk_widget_ref( hseparator2 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"hseparator2",hseparator2,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( hseparator2 );
  gtk_box_pack_start( GTK_BOX( vbox2 ),hseparator2,FALSE,FALSE,0 );
  gtk_widget_set_usize( hseparator2,-2,3 );

  scrolledwindow3=gtk_scrolled_window_new( NULL,NULL );
  gtk_widget_ref( scrolledwindow3 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"scrolledwindow3",scrolledwindow3,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( scrolledwindow3 );
  gtk_box_pack_start( GTK_BOX( vbox2 ),scrolledwindow3,TRUE,TRUE,0 );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolledwindow3 ),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC );

  CLSelected=gtk_clist_new( 2 );
  gtk_widget_ref( CLSelected );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"CLSelected",CLSelected,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CLSelected );
  gtk_container_add( GTK_CONTAINER( scrolledwindow3 ),CLSelected );
  gtk_clist_set_column_width( GTK_CLIST( CLSelected ),0,295 );
  gtk_clist_set_column_width( GTK_CLIST( CLSelected ),1,295 );
  gtk_clist_set_selection_mode( GTK_CLIST( CLSelected ),GTK_SELECTION_MULTIPLE );
  gtk_clist_column_titles_show( GTK_CLIST( CLSelected ) );
  gtk_clist_set_shadow_type( GTK_CLIST( CLSelected ),GTK_SHADOW_NONE );

  label5=gtk_label_new( MSGTR_PLAYLIST_Selected );
  gtk_widget_ref( label5 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"label5",label5,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label5 );
  gtk_clist_set_column_widget( GTK_CLIST( CLSelected ),0,label5 );
  gtk_misc_set_alignment( GTK_MISC( label5 ),0.02,0.5 );

  label5=gtk_label_new( MSGTR_PLAYLIST_Path );
  gtk_widget_ref( label5 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"label5",label5,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( label5 );
  gtk_clist_set_column_widget( GTK_CLIST( CLSelected ),1,label5 );
  gtk_misc_set_alignment( GTK_MISC( label5 ),0.02,0.5 );

  hseparator1=gtk_hseparator_new();
  gtk_widget_ref( hseparator1 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"hseparator1",hseparator1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( hseparator1 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),hseparator1,FALSE,FALSE,0 );
  gtk_widget_set_usize( hseparator1,-2,6 );

  hbuttonbox1=gtk_hbutton_box_new();
  gtk_widget_ref( hbuttonbox1 );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"hbuttonbox1",hbuttonbox1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( hbuttonbox1 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),hbuttonbox1,FALSE,FALSE,0 );
  gtk_button_box_set_layout( GTK_BUTTON_BOX( hbuttonbox1 ),GTK_BUTTONBOX_END );
  gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox1 ),10 );
  gtk_button_box_set_child_size( GTK_BUTTON_BOX( hbuttonbox1 ),-1,20 );
  gtk_button_box_set_child_ipadding( GTK_BUTTON_BOX( hbuttonbox1 ),0,-1 );

  Add=gtk_button_new_with_label( MSGTR_Add );
  gtk_widget_ref( Add );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"Add",Add,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( Add );
  gtk_container_add( GTK_CONTAINER( hbuttonbox1 ),Add );
  GTK_WIDGET_UNSET_FLAGS( Add,GTK_CAN_FOCUS );

  Remove=gtk_button_new_with_label( MSGTR_Remove );
  gtk_widget_ref( Remove );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"Remove",Remove,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( Remove );
  gtk_container_add( GTK_CONTAINER( hbuttonbox1 ),Remove );
  GTK_WIDGET_UNSET_FLAGS( Remove,GTK_CAN_FOCUS );

  Ok=gtk_button_new_with_label( MSGTR_Ok );
  gtk_widget_ref( Ok );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"Ok",Ok,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( Ok );
  gtk_container_add( GTK_CONTAINER( hbuttonbox1 ),Ok );
  GTK_WIDGET_UNSET_FLAGS( Ok,GTK_CAN_FOCUS );
//  gtk_widget_add_accelerator( Ok,"released",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE );

  Cancel=gtk_button_new_with_label( "Cancel" );
  gtk_widget_ref( Cancel );
  gtk_object_set_data_full( GTK_OBJECT( PlayList ),"Cancel",Cancel,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( Cancel );
  gtk_container_add( GTK_CONTAINER( hbuttonbox1 ),Cancel );
  GTK_WIDGET_UNSET_FLAGS( Cancel,GTK_CAN_FOCUS );
  gtk_widget_add_accelerator( Cancel,"released",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );

  gtk_signal_connect( GTK_OBJECT( PlayList ),"destroy",GTK_SIGNAL_FUNC( plDestroy ),NULL );
  gtk_signal_connect( GTK_OBJECT( PlayList ),"show",GTK_SIGNAL_FUNC( plShow ),(void *)1 );
  gtk_signal_connect( GTK_OBJECT( PlayList ),"hide",GTK_SIGNAL_FUNC( plShow ),(void *)0 );

  gtk_signal_connect( GTK_OBJECT( CLFiles ),"select_row",GTK_SIGNAL_FUNC( plRowSelect ),(void *)0 );
  gtk_signal_connect( GTK_OBJECT( CLFiles ),"unselect_row",GTK_SIGNAL_FUNC( plUnRowSelect ),(void *)0 );
  sigSel=gtk_signal_connect( GTK_OBJECT( CLSelected ),"select_row",GTK_SIGNAL_FUNC( plRowSelect ),(void*)1 );
  sigUnsel=gtk_signal_connect( GTK_OBJECT( CLSelected ),"unselect_row",GTK_SIGNAL_FUNC( plUnRowSelect ),(void*)1 );

  gtk_signal_connect( GTK_OBJECT( Add ),"released",GTK_SIGNAL_FUNC( plButtonReleased ),(void*)3 );
  gtk_signal_connect( GTK_OBJECT( Remove ),"released",GTK_SIGNAL_FUNC( plButtonReleased ),(void*)2 );
  gtk_signal_connect( GTK_OBJECT( Ok ),"released",GTK_SIGNAL_FUNC( plButtonReleased ),(void*)1 );
  gtk_signal_connect( GTK_OBJECT( Cancel ),"released",GTK_SIGNAL_FUNC( plButtonReleased ),(void*)0 );

  gtk_window_add_accel_group( GTK_WINDOW( PlayList ),accel_group );

  return PlayList;
}

