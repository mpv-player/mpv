
#ifndef GUI_MB_H
#define GUI_MB_H

#include <gtk/gtk.h>

extern GtkWidget * MessageBox;

extern GtkWidget * create_MessageBox( int type );
extern void ShowMessageBox( const char * msg );

#endif /* GUI_MB_H */
