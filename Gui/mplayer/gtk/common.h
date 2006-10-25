
#ifndef __COMMON_H
#define __COMMON_H

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

extern GtkWidget * AddDialogFrame( GtkWidget * parent );
extern GtkWidget * AddFrame( const char * title,int type,GtkWidget * parent,int add );
extern GtkWidget * AddLabel( const char * title,GtkWidget * parent );
extern GtkWidget * AddVBox( GtkWidget * parent,int type );
extern GtkWidget * AddHBox( GtkWidget * parent,int type );
extern GtkWidget * AddCheckButton( const char * title, GtkWidget * parent );
extern GtkWidget * AddRadioButton( const char * title,GSList ** group,GtkWidget * parent );
extern GtkWidget * AddSpinButton( const char * title,GtkAdjustment * adj,GtkWidget * parent );
extern GtkWidget * AddButton( const char * title,GtkWidget * parent );
extern GtkWidget * AddHSeparator( GtkWidget * parent );
extern GtkWidget * AddHButtonBox( GtkWidget * parent );
extern GtkWidget * AddHScaler( GtkAdjustment * adj,GtkWidget * parent,int digit );
extern GtkWidget * AddVScaler( GtkAdjustment * adj,GtkWidget * parent,int digit );
extern GtkWidget * AddComboBox( GtkWidget * parent );
extern void WidgetDestroy( GtkWidget * widget,GtkWidget ** widget_pointer );

#endif
