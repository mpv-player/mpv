
#ifndef __COMMON_H
#define __COMMON_H

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

extern GtkWidget * AddFrame( char * title,int type,GtkWidget * parent,int add );
extern GtkWidget * AddLabel( char * title,GtkWidget * parent );
extern GtkWidget * AddVBox( GtkWidget * parent );
extern GtkWidget * AddHBox( GtkWidget * parent,int type );
extern GtkWidget * AddCheckButton( char * title, GtkWidget * parent );
extern GtkWidget * AddRadioButton( char * title,GSList ** group,GtkWidget * parent );
extern GtkWidget * AddButton( char * title,GtkWidget * parent );
extern GtkWidget * AddHSeparator( GtkWidget * parent );
extern GtkWidget * AddHButtonBox( GtkWidget * parent );
extern GtkWidget * AddHScaler( GtkAdjustment * adj,GtkWidget * parent,int digit );

#endif
