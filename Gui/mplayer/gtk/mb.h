
#ifndef __GUI_MESSAGEBOX_H
#define __GUI_MESSAGEBOX_H

#include <gtk/gtk.h>

extern GtkWidget * gtkMessageBoxText;
extern GtkWidget * MessageBox;

extern GtkWidget * create_MessageBox( int type );
extern void ShowMessageBox( char * msg );

#endif
