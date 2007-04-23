
#ifndef __PREFERENCES_H
#define __PREFERENCES_H

#include <gtk/gtk.h>

extern GtkWidget * AudioConfig;
extern GtkWidget * Preferences;
extern GtkWidget * prEFontName;

extern GtkWidget * create_Preferences( void );
extern GtkWidget * create_AudioConfig( void );

extern void ShowPreferences( void );

#endif
