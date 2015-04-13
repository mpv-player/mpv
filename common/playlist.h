/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_PLAYLIST_H
#define MPLAYER_PLAYLIST_H

#include <stdbool.h>
#include "misc/bstr.h"

struct playlist_param {
    bstr name, value;
};

struct playlist_entry {
    struct playlist_entry *prev, *next;
    struct playlist *pl;

    char *filename;

    struct playlist_param *params;
    int num_params;

    // Set to true if playback didn't seem to work, or if the file could be
    // played only for a very short time. This is used to make playlist
    // navigation just work in case the user has unplayable files in the
    // playlist.
    bool playback_short : 1;
    // Set to true if not at least 1 frame (audio or video) could be played.
    bool init_failed : 1;
    // Entry was removed with playlist_remove (etc.), but not deallocated.
    bool removed : 1;
    // Additional refcount. Normally (reserved==0), the entry is owned by the
    // playlist, and this can be used to keep the entry alive.
    int reserved;
    // Used to reject loading of unsafe entries from external playlists.
    // Can have any of the following bit flags set:
    //  STREAM_SAFE_ONLY: only allow streams marked with is_safe
    //  STREAM_NETWORK_ONLY: only allow streams marked with is_network
    // The value 0 allows everything.
    int stream_flags;
};

struct playlist {
    struct playlist_entry *first, *last;

    // This provides some sort of stable iterator. If this entry is removed from
    // the playlist, current is set to the next element (or NULL), and
    // current_was_replaced is set to true.
    struct playlist_entry *current;
    bool current_was_replaced;

    bool disable_safety;
};

void playlist_entry_add_param(struct playlist_entry *e, bstr name, bstr value);
void playlist_entry_add_params(struct playlist_entry *e,
                               struct playlist_param *params,
                               int params_count);

struct playlist_entry *playlist_entry_new(const char *filename);

void playlist_insert(struct playlist *pl, struct playlist_entry *after,
                     struct playlist_entry *add);
void playlist_add(struct playlist *pl, struct playlist_entry *add);
void playlist_remove(struct playlist *pl, struct playlist_entry *entry);
void playlist_clear(struct playlist *pl);

void playlist_move(struct playlist *pl, struct playlist_entry *entry,
                   struct playlist_entry *at);

void playlist_add_file(struct playlist *pl, const char *filename);
void playlist_shuffle(struct playlist *pl);
struct playlist_entry *playlist_get_next(struct playlist *pl, int direction);
void playlist_add_base_path(struct playlist *pl, bstr base_path);
void playlist_transfer_entries(struct playlist *pl, struct playlist *source_pl);
void playlist_append_entries(struct playlist *pl, struct playlist *source_pl);

int playlist_entry_to_index(struct playlist *pl, struct playlist_entry *e);
int playlist_entry_count(struct playlist *pl);
struct playlist_entry *playlist_entry_from_index(struct playlist *pl, int index);

struct mpv_global;
struct playlist *playlist_parse_file(const char *file, struct mpv_global *global);

void playlist_entry_unref(struct playlist_entry *e);

#endif
