
#ifndef __GUI_EQ_H
#define __GUI_EQ_H

#include <gtk/gtk.h>

extern GtkWidget * Equalizer;

extern int gtkEnableAudioEqualizer;
extern int gtkEnableVideoEqualizer;

extern GtkWidget * create_Equalizer( void );
extern void ShowEqualizer( void );

#endif