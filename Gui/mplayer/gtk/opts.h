
#ifndef __PREFERENCES_H
#define __PREFERENCES_H

#include <gtk/gtk.h>

#ifdef USE_OSS_AUDIO
extern GtkWidget * OSSConfig;
#endif
extern GtkWidget * Preferences;
extern GtkWidget * prEFontName;

extern GtkWidget * create_Preferences( void );
#ifdef USE_OSS_AUDIO
extern GtkWidget * create_OSSConfig( void );
#endif

extern void ShowPreferences( void );

#endif
