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

#ifndef MPLAYER_GUI_WIDGETS_H
#define MPLAYER_GUI_WIDGETS_H

#include <stdio.h>
#include <stdlib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "config.h"
#include "osdep/shmem.h"
#include "play.h"
#include "mplayer.h"
#include "gui/interface.h"
#include "gui/wm/ws.h"

#define GTK_MB_SIMPLE 0
#define GTK_MB_MODAL 1
#define GTK_MB_FATAL 2
#define GTK_MB_ERROR 4
#define GTK_MB_WARNING 8

extern GtkWidget     * PlayList;
extern GtkWidget     * Options;
extern GtkWidget     * PopUpMenu;

extern GtkWidget     * WarningPixmap;
extern GtkWidget     * ErrorPixmap;

extern GtkWidget     * SkinList;
extern GtkWidget     * gtkMessageBoxText;

extern int             gtkPopupMenu;
extern int             gtkPopupMenuParam;

extern char          * sbMPlayerDirInHome;
extern char          * sbMPlayerPrefixDir;
extern char          * sbMPlayerDirInHome_obsolete;
extern char          * sbMPlayerPrefixDir_obsolete;

extern GdkPixmap * gtkIcon;
extern GdkBitmap * gtkIconMask;
extern Pixmap      guiIcon;
extern Pixmap	   guiIconMask;

void widgetsCreate( void );

void gtkInit( void );
void gtkAddIcon( GtkWidget * window );

int  gtkFillSkinList( gchar * dir );
void gtkClearList( GtkWidget * list );
void gtkSetDefaultToCList( GtkWidget * list, char * item );
int  gtkFindCList( GtkWidget * list, char * item );

void gtkEventHandling( void );

void gtkShow( int type, char * param );
void gtkMessageBox( int type, const gchar * str );
void gtkSetLayer( GtkWidget * wdg );
void gtkActive( GtkWidget * wdg );

#endif /* MPLAYER_GUI_WIDGETS_H */
