/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_GUI_GTK_COMMON_H
#define MPLAYER_GUI_GTK_COMMON_H

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

GtkWidget * AddDialogFrame( GtkWidget * parent );
GtkWidget * AddFrame( const char * title, int type, GtkWidget * parent, int add );
GtkWidget * AddLabel( const char * title, GtkWidget * parent );
GtkWidget * AddVBox( GtkWidget * parent, int type );
GtkWidget * AddHBox( GtkWidget * parent, int type );
GtkWidget * AddCheckButton( const char * title, GtkWidget * parent );
GtkWidget * AddRadioButton( const char * title, GSList ** group, GtkWidget * parent );
GtkWidget * AddSpinButton( const char * title, GtkAdjustment * adj, GtkWidget * parent );
GtkWidget * AddButton( const char * title, GtkWidget * parent );
GtkWidget * AddHSeparator( GtkWidget * parent );
GtkWidget * AddHButtonBox( GtkWidget * parent );
GtkWidget * AddHScaler( GtkAdjustment * adj, GtkWidget * parent, int digit );
GtkWidget * AddVScaler( GtkAdjustment * adj, GtkWidget * parent, int digit );
GtkWidget * AddComboBox( GtkWidget * parent );
void WidgetDestroy( GtkWidget * widget, GtkWidget ** widget_pointer );

#endif /* MPLAYER_GUI_GTK_COMMON_H */
