/*
  MPlayer Gui for win32
  Copyright (c) 2003 Sascha Sommer <saschasommer@freenet.de>
  Copyright (c) 2006 Erik Augustson <erik_27can@yahoo.com>
  Copyright (c) 2006 Gianluigi Tiesi <sherpya@netfarm.it>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02111-1307 USA
*/

#ifndef _GUI_H
#define _GUI_H
#include <config.h>
#include <playtree.h>
#include <m_config.h>
#include "skinload.h"
#include "playlist.h"

extern char *skinName;
extern float sub_aspect;
extern play_tree_t* playtree;
extern m_config_t* mconfig;

extern NOTIFYICONDATA nid;

typedef struct window_priv_t window_priv_t;
struct window_priv_t
{
    HWND hwnd;
    image img;
    image *background;
    HBITMAP bitmap;
    int type;
};

typedef struct gui_t gui_t;
struct gui_t
{
    /* screenproperties */
    int screenw, screenh, screenbpp;
    /* window related stuff */
    char *classname;
    HICON icon;
    unsigned int window_priv_count;
    window_priv_t **window_priv;

    HWND mainwindow;
    HWND subwindow;

    /* for event handling */
    widget *activewidget;

    int mousewx, mousewy; /* mousepos inside widget */
    int mousex, mousey;

    HMENU menu;
    HMENU diskmenu;
    HMENU traymenu;
    HMENU trayplaymenu;
    HMENU trayplaybackmenu;
    HMENU submenu;
    HMENU subtitlemenu;
    HMENU aspectmenu;
    HMENU dvdmenu;
    HMENU playlistmenu;

    int skinbrowserwindow;
    int playlistwindow;
    int aboutwindow;

    skin_t *skin;
    playlist_t *playlist;

    void (*startplay)(gui_t *gui);
    void (*updatedisplay)(gui_t *gui, HWND hwnd);
    void (*playercontrol)(int event);   /* userdefine call back function */
    void (*uninit)(gui_t *gui);
};

#define     wsShowWindow    8
#define     wsHideWindow    16
#define     wsShowFrame     1
#define     wsMovable       2
#define     wsSizeable      4

extern gui_t *create_gui(char *skindir, char *skinName, void (*playercontrol)(int event));
extern int destroy_window(gui_t *gui);
extern int create_window(gui_t *gui, char *skindir);
extern int create_subwindow(gui_t *gui, char *skindir);
extern int parse_filename(char *file, play_tree_t *playtree, m_config_t *mconfig, int clear);
extern void capitalize(char *filename);
extern int import_playtree_playlist_into_gui(play_tree_t *my_playtree, m_config_t *config);

/* Dialogs */
extern void display_playlistwindow(gui_t *gui);
extern void update_playlistwindow(void);
extern int display_openfilewindow(gui_t *gui, int add);
extern void display_openurlwindow(gui_t *gui, int add);
extern void display_skinbrowser(gui_t *gui);
extern void display_chapterselwindow(gui_t *gui);
extern void display_eqwindow(gui_t *gui);
extern void display_prefswindow(gui_t *gui);
extern void display_opensubtitlewindow(gui_t *gui);

#endif
