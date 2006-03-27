#ifndef __GUI_SB_H
#define __GUI_SB_H

#include <gtk/gtk.h>

extern GtkWidget * SkinList;
extern char      * sbSelectedSkin;
extern char      * sbMPlayerDirInHome;
extern char      * sbMPlayerDirInHome_obsolete;
extern char      * sbMPlayerPrefixDir;
extern char      * sbMPlayerPrefixDir_obsolete;
extern GtkWidget * SkinBrowser;

extern void ShowSkinBrowser( void );
extern int gtkFillSkinList( gchar * mdir );
extern GtkWidget * create_SkinBrowser( void );

#endif
