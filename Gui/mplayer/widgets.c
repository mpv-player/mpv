
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
#include "../../help_mp.h"
#include "../error.h"

GtkWidget     * SkinBrowser;
GtkWidget     * PlayList;
GtkWidget     * FileSelect;
GtkWidget     * AboutBox;
GtkWidget     * Options;
GtkWidget     * PopUpMenu;

GtkWidget     * MessageBox;

GtkWidget     * WarningPixmap;
GtkWidget     * ErrorPixmap;

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
#include "gtk/menu.h"

void widgetsCreate( void )
{
 AboutBox=create_About();
 SkinBrowser=create_SkinBrowser();
 PlayList=create_PlayList();
 FileSelect=create_FileSelect();
 MessageBox=create_MessageBox(0);
 Options=create_Options();
// PopUpMenu=create_PopUpMenu();
}

// --- forked function

static void gtkThreadProc( int argc,char * argv[] )
{
 struct sigaction sa;

 gtk_set_locale();
 gtk_init( &argc,&argv );
 gdk_set_use_xshm( TRUE );

 widgetsCreate();

 gtkPID=getppid();

 memset(&sa, 0, sizeof(sa));
 sa.sa_handler = gtkSigHandler;
 sigaction( SIGTYPE, &sa, NULL );

 gtkIsOk=True;
 gtkSendMessage( evGtkIsOk );

 gtk_main();
 printf( "[gtk] exit.\n" );
 exit( 0 );
}

// --- init & close gtk

void gtkInit( int argc,char* argv[], char *envp[] )
{
 gtkShMem=shmem_alloc( sizeof( gtkCommStruct ) );
 if ( ( gtkPID = fork() ) == 0 ) gtkThreadProc( argc,argv );
}

void gtkDone( void ){
 int status;
 gtkSendMessage(evExit);
 usleep(50000); // 50ms should be enough!
 printf("gtk killed...\n");
 kill( gtkPID,SIGKILL );
}

void gtkMessageBox( int type,gchar * str )
{
 gtkShMem->mb.type=type;
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

