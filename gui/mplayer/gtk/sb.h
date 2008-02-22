#ifndef MPLAYER_GUI_SB_H
#define MPLAYER_GUI_SB_H

#include <gtk/gtk.h>

extern char      * sbSelectedSkin;
extern GtkWidget * SkinBrowser;

extern void ShowSkinBrowser( void );
extern GtkWidget * create_SkinBrowser( void );

#endif /* MPLAYER_GUI_SB_H */
