
#ifndef __PREFERENCES_H
#define __PREFERENCES_H

#include <gtk/gtk.h>

extern GtkWidget * OSSConfig;
extern GtkWidget * Preferences;
extern int    	   gtkVPreferences;
extern GtkWidget * prEFontName;

extern GtkWidget * create_Preferences( void );
extern GtkWidget * create_OSSConfig( void );

extern void ShowPreferences( void );

#endif
