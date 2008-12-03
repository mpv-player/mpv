/*
 * MPlayer GUI for Win32
 * Copyright (C) 2003 Sascha Sommer <saschasommer@freenet.de>
 * Copyright (C) 2006 Erik Augustson <erik_27can@yahoo.com>
 * Copyright (C) 2006 Gianluigi Tiesi <sherpya@netfarm.it>
 *
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef MPLAYER_GUI_PLAYLIST_H
#define MPLAYER_GUI_PLAYLIST_H

#include <windows.h>

typedef struct
{
    char *filename;
    char *artist;
    char *title;
    int duration;
} pl_track_t;

typedef struct playlist_t playlist_t;
struct playlist_t
{
    int current;                  /* currently used track */
    int trackcount;               /* number of tracknumber */
    pl_track_t **tracks;             /* tracklist */
    void (*add_track)(playlist_t* playlist, const char *filename, const char *artist, const char *title, int duration);
    void (*remove_track)(playlist_t* playlist, int number);
    void (*moveup_track)(playlist_t* playlist, int number);
    void (*movedown_track)(playlist_t* playlist, int number);
    void (*dump_playlist)(playlist_t* playlist);
    void (*sort_playlist)(playlist_t* playlist, int opt);
    void (*clear_playlist)(playlist_t* playlist);
    void (*free_playlist)(playlist_t* playlist);
};

#define SORT_BYFILENAME     1
#define SORT_BYARTIST       2
#define SORT_BYTITLE        3
#define SORT_BYDURATION     4

playlist_t *create_playlist(void);
BOOL adddirtoplaylist(playlist_t *playlist, const char* path, BOOL recursive);

#endif /* MPLAYER_GUI_PLAYLIST_H */
