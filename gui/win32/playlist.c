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

#include <windows.h>
#include <mp_msg.h>
#include "playlist.h"

/* TODO: implement sort_playlist */

BOOL adddirtoplaylist(playlist_t *playlist, const char *path, BOOL recursive)
{
    HANDLE findHandle = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATA finddata;
    char findpath[MAX_PATH], filename[MAX_PATH];
    char *filepart;

    sprintf(findpath, "%s\\*.*", path);

    findHandle = FindFirstFile(findpath, &finddata);

    if (findHandle == INVALID_HANDLE_VALUE) return FALSE;
    do
    {
        if (finddata.cFileName[0] == '.' || strstr(finddata.cFileName, "Thumbs.db")) continue;
        sprintf(findpath, "%s\\%s", path, finddata.cFileName);

        if (GetFileAttributes(findpath) & FILE_ATTRIBUTE_DIRECTORY)
        {
            if(recursive)
                adddirtoplaylist(playlist, findpath, recursive);
        }
        else
        {
            if (GetFullPathName(findpath, MAX_PATH, filename, &filepart))
                playlist->add_track(playlist, filename, NULL, filepart, 0);
        }
    } while (FindNextFile(findHandle, &finddata));
    FindClose(findHandle);
    return TRUE;
}

static void add_track(playlist_t *playlist, const char *filename, const char *artist, const char *title, int duration)
{
    (playlist->trackcount)++;
    playlist->tracks = realloc(playlist->tracks, playlist->trackcount * sizeof(pl_track_t *));
    playlist->tracks[playlist->trackcount - 1] = calloc(1, sizeof(pl_track_t));
    if(filename) playlist->tracks[playlist->trackcount - 1]->filename = strdup(filename);
    if(artist) playlist->tracks[playlist->trackcount - 1]->artist = strdup(artist);
    if(title) playlist->tracks[playlist->trackcount - 1]->title = strdup(title);
    if(duration) playlist->tracks[playlist->trackcount - 1]->duration = duration;
}

static void remove_track(playlist_t *playlist, int number)
{
    pl_track_t **tmp = calloc(1, playlist->trackcount * sizeof(pl_track_t *));
    int i, p = 0;
    memcpy(tmp, playlist->tracks, playlist->trackcount * sizeof(pl_track_t *));
    (playlist->trackcount)--;
    playlist->tracks = realloc(playlist->tracks, playlist->trackcount * sizeof(pl_track_t *));
    for(i=0; i<playlist->trackcount + 1; i++)
    {
        if(i != (number - 1))
        {
            playlist->tracks[p] = tmp[i];
            p++;
        }
        else
        {
            if(tmp[i]->filename) free(tmp[i]->filename);
            if(tmp[i]->artist) free(tmp[i]->artist);
            if(tmp[i]->title) free(tmp[i]->title);
            free(tmp[i]);
        }
    }
    free(tmp);
}

static void moveup_track(playlist_t *playlist, int number)
{
    pl_track_t *tmp;
    if(number == 1) return; /* already first */
    tmp = playlist->tracks[number - 2];
    playlist->tracks[number - 2] = playlist->tracks[number - 1];
    playlist->tracks[number - 1] = tmp;
}

static void movedown_track(playlist_t *playlist, int number)
{
    pl_track_t *tmp;
    if(number == playlist->trackcount) return; /* already latest */
    tmp = playlist->tracks[number];
    playlist->tracks[number] = playlist->tracks[number - 1];
    playlist->tracks[number - 1] = tmp;
}

static void sort_playlist(playlist_t *playlist, int opt) {}

static void clear_playlist(playlist_t *playlist)
{
    while(playlist->trackcount) playlist->remove_track(playlist, 1);
    playlist->tracks = NULL;
    playlist->current = 0;
}

static void free_playlist(playlist_t *playlist)
{
    if(playlist->tracks) playlist->clear_playlist(playlist);
    free(playlist);
}

static void dump_playlist(playlist_t *playlist)
{
    int i;
    for (i=0; i<playlist->trackcount; i++)
    {
        mp_msg(MSGT_GPLAYER, MSGL_V, "track %i %s ", i + 1, playlist->tracks[i]->filename);
        if(playlist->tracks[i]->artist) mp_msg(MSGT_GPLAYER, MSGL_V, "%s ", playlist->tracks[i]->artist);
        if(playlist->tracks[i]->title) mp_msg(MSGT_GPLAYER, MSGL_V, "- %s ", playlist->tracks[i]->title);
        if(playlist->tracks[i]->duration) mp_msg(MSGT_GPLAYER, MSGL_V, "%i ", playlist->tracks[i]->duration);
        mp_msg(MSGT_GPLAYER, MSGL_V, "\n");
    }
}

playlist_t *create_playlist(void)
{
    playlist_t *playlist = calloc(1, sizeof(playlist_t));
    playlist->add_track = add_track;
    playlist->remove_track = remove_track;
    playlist->moveup_track = moveup_track;
    playlist->movedown_track = movedown_track;
    playlist->dump_playlist = dump_playlist;
    playlist->sort_playlist = sort_playlist;
    playlist->clear_playlist = clear_playlist;
    playlist->free_playlist = free_playlist;
    return playlist;
}
