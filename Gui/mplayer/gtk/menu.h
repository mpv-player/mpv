#ifndef __GUI_MENU_H
#define __GUI_MENU_H

#include <gtk/gtk.h>

extern GtkWidget * DVDSubMenu;

extern GtkWidget * AddMenuItem( GtkWidget * Menu,char * label,int Number );
extern GtkWidget * AddSubMenu( GtkWidget * Menu,char * label );
extern GtkWidget * AddSeparator( GtkWidget * Menu );
extern GtkWidget * create_PopUpMenu( void );

#endif
