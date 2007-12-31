#ifndef GUI_MENU_H
#define GUI_MENU_H

#include <gtk/gtk.h>

extern GtkWidget * DVDSubMenu;

extern GtkWidget * AddMenuItem( GtkWidget *window1, const char * immagine_xpm,  GtkWidget * SubMenu,const char * label,int Number );
extern GtkWidget * AddSubMenu( GtkWidget *window1, const char * immagine_xpm, GtkWidget * Menu,const char * label );
extern GtkWidget * AddSeparator( GtkWidget * Menu );
extern GtkWidget * create_PopUpMenu( void );

#endif /* GUI_MENU_H */
