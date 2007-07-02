
#ifndef __GUI_MB_H
#define __GUI_MB_H

#include <gtk/gtk.h>

extern GtkWidget * gtkMessageBoxText;
extern GtkWidget * MessageBox;

extern GtkWidget * create_MessageBox( int type );
extern void ShowMessageBox( const char * msg );

#endif
