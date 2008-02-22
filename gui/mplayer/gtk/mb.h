#ifndef MPLAYER_GUI_MB_H
#define MPLAYER_GUI_MB_H

#include <gtk/gtk.h>

extern GtkWidget * MessageBox;

extern GtkWidget * create_MessageBox( int type );
extern void ShowMessageBox( const char * msg );

#endif /* MPLAYER_GUI_MB_H */
