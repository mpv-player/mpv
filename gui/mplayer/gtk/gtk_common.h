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

#endif /* MPLAYER_GUI_GTK_COMMON_H */
