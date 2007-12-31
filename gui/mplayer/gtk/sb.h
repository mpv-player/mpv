#ifndef GUI_SB_H
#define GUI_SB_H

#include <gtk/gtk.h>

extern char      * sbSelectedSkin;
extern GtkWidget * SkinBrowser;

extern void ShowSkinBrowser( void );
extern GtkWidget * create_SkinBrowser( void );

#endif /* GUI_SB_H */
