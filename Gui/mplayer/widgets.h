
#ifndef __MY_WIDGET
#define __MY_WIDGET

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "../../linux/shmem.h"

typedef struct
{
 char dir[ 1024 ];
 char filename[ 1024 ];
} gtkFileSelectorStruct;

typedef struct
{
 int  sx;
 int  sy;
 int  tsx;
 int  tsy;
 char str[512];
} gtkMessageBoxStruct;

typedef struct
{
 char name[128];
} gtkSkinStruct;

typedef struct
{
 int window;
} gtkVisibleStruct;

typedef struct
{
 int i;
} gtkOptionsStruct;

typedef struct
{
 int                   message;
 gtkFileSelectorStruct fs;
 gtkMessageBoxStruct   mb;
 gtkSkinStruct         sb;
 gtkVisibleStruct      vs;
 gtkOptionsStruct      op;
} gtkCommStruct;

#define ShMemSize sizeof( gtkCommStruct )

extern gtkCommStruct * gtkShMem;

extern GtkWidget     * SkinBrowser;
extern GtkWidget     * PlayList;
extern GtkWidget     * FileSelect;
extern GtkWidget     * MessageBox;
extern GtkWidget     * AboutBox;
extern GtkWidget     * Options;

extern GtkWidget     * SkinList;
extern GtkWidget     * gtkMessageBoxText;

extern int             gtkVisibleSkinBrowser;
extern int             gtkVisiblePlayList;
extern int             gtkVisibleFileSelect;
extern int             gtkVisibleMessageBox;
extern int             gtkVisibleAboutBox;
extern int             gtkVisibleOptions;

extern char          * sbMPlayerDirInHome;
extern char          * sbMPlayerPrefixDir;

extern void widgetsCreate( void );

extern void gtkInit( int argc,char* argv[], char *envp[] );
extern void gtkDone( void );
extern void gtkMessageBox( gchar * str );
extern int  gtkFillSkinList( gchar * dir );
extern void gtkClearList( GtkWidget * list );
extern void gtkSetDefaultToCList( GtkWidget * list,char * item );
extern int  gtkFindCList( GtkWidget * list,char * item );

#endif