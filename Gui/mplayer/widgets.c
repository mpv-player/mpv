
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "widgets.h"

#include "./mplayer.h"
#include "psignal.h"
#include "../events.h"

#include "../../config.h"
#include "../error.h"

#include "pixmaps/up.xpm"
#include "pixmaps/dir.xpm"
#include "pixmaps/file.xpm"
#include "pixmaps/logo.xpm"

GtkWidget     * SkinBrowser;
GtkWidget     * PlayList;
GtkWidget     * FileSelect;
GtkWidget     * MessageBox;
GtkWidget     * AboutBox;
GtkWidget     * Options;

int             gtkVisibleSkinBrowser = 0;
int             gtkVisiblePlayList = 0;
int             gtkVisibleFileSelect = 0;
int             gtkVisibleMessageBox = 0;
int             gtkVisibleAboutBox = 0;
int             gtkVisibleOptions = 0;

gtkCommStruct * gtkShMem;

#include "gtk/sb.h"
#include "gtk/pl.h"
#include "gtk/fs.h"
#include "gtk/mb.h"
#include "gtk/about.h"
#include "gtk/opts.h"

void widgetsCreate( void )
{
 AboutBox=create_About();
 SkinBrowser=create_SkinBrowser();
 PlayList=create_PlayList();
 FileSelect=create_FileSelect();
 MessageBox=create_MessageBox();
 Options=create_Options();
}

// --- forked function

static void gtkThreadProc( int argc,char * argv[] )
{
 gtk_set_locale();
 gtk_init( &argc,&argv );
 gdk_set_use_xshm( TRUE );

 widgetsCreate();

 gtkPID=getppid();

 signal( SIGTYPE,gtkSigHandler );

 gtkIsOk=True;
 gtkSendMessage( evGtkIsOk );

 gtk_main();
 printf( "[gtk] exit.\n" );
 exit( 0 );
}

// --- init & close gtk

void gtkInit( int argc,char* argv[], char *envp[] )
{
 gtkShMem=shmem_alloc( ShMemSize );
 if ( ( gtkPID = fork() ) == 0 ) gtkThreadProc( argc,argv );
}

void gtkDone( void ){
 int status;
 gtkSendMessage(evExit);
 usleep(50000); // 50ms should be enough!
 printf("gtk killed...\n");
 kill( gtkPID,SIGKILL );
}

void gtkMessageBox( gchar * str )
{
 gtkShMem->mb.sx=420; gtkShMem->mb.sy=128;
 gtkShMem->mb.tsx=384; gtkShMem->mb.tsy=77;
 if ( strlen( str ) > 200 )
  {
   gtkShMem->mb.sx=512;
   gtkShMem->mb.sy=128;
   gtkShMem->mb.tsx=476;
   gtkShMem->mb.tsy=77;
  }
 strcpy( gtkShMem->mb.str,str );
 gtkSendMessage( evMessageBox );
}

void gtkClearList( GtkWidget * list )
{ gtk_clist_clear( GTK_CLIST( list ) ); }

int gtkFindCList( GtkWidget * list,char * item )
{
 gint    j,t;
 gchar * tmpstr;
 for( t=0,j=0;j<GTK_CLIST( list )->rows;j++ )
  {
   gtk_clist_get_text( GTK_CLIST( list ),j,0,&tmpstr );
   if ( !strcmp( tmpstr,item ) ) return j;
  }
 return -1;
}

void gtkSetDefaultToCList( GtkWidget * list,char * item )
{
 gint    i;
 if ( ( i=gtkFindCList( list,item ) ) > -1 ) gtk_clist_select_row( GTK_CLIST( list ),i,0 );
}

