
#ifndef __MY_WIDGET
#define __MY_WIDGET

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "../../config.h"
#include "../../linux/shmem.h"
#include "play.h"

#define GTK_MB_SIMPLE 0
#define GTK_MB_MODAL 1
#define GTK_MB_FATAL 2
#define GTK_MB_ERROR 4
#define GTK_MB_WARNING 8

typedef struct
{
 char dir[ 2048 ];
 char filename[ 2048 ];
} gtkFileSelectorStruct;

typedef struct
{
 int  sx;
 int  sy;
 int  tsx;
 int  tsy;
 int  type;
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

#ifdef USE_DVDREAD 
 mplDVDStruct          DVD;
#endif
 
 int		       popupmenu;
 int		       popupmenuparam;
 int		       visiblepopupmenu;
} gtkCommStruct;

extern gtkCommStruct * gtkShMem;

extern GtkWidget     * SkinBrowser;
extern GtkWidget     * PlayList;
extern GtkWidget     * FileSelect;
extern GtkWidget     * AboutBox;
extern GtkWidget     * Options;
extern GtkWidget     * PopUpMenu;

extern GtkWidget     * MessageBox;

extern GtkWidget     * WarningPixmap;
extern GtkWidget     * ErrorPixmap;

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
extern void gtkMessageBox( int type,gchar * str );
extern int  gtkFillSkinList( gchar * dir );
extern void gtkClearList( GtkWidget * list );
extern void gtkSetDefaultToCList( GtkWidget * list,char * item );
extern int  gtkFindCList( GtkWidget * list,char * item );

#endif
