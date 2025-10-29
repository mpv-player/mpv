/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_PLAYLIST_H
#define MPLAYER_PLAYLIST_H

#include <stdbool.h>

#include "misc/bstr.h"

typedef struct kh_playlist_id_map_s playlist_id_map_t;

struct playlist_param {
    bstr name, value;
};

struct playlist_entry {
    // Invariant: (pl && pl->entries[pl_index] == this) || (!pl && pl_index < 0)
    struct playlist *pl;
    int pl_index;

    uint64_t id;

    char *filename;
    char *playlist_path;

    struct playlist_param *params;
    int num_params;

    char *title;

    // Used for unshuffling: the pl_index before it was shuffled. -1 => unknown.
    int original_index;

    // Set to true if this playlist entry was selected while trying to go backwards
    // in the playlist. If this is true and the playlist entry fails to play later,
    // then mpv tries to go to the next previous entry. This flag is always cleared
    // regardless if the attempt was successful or not.
    bool playlist_prev_attempt : 1;

    // Set to true if not at least 1 frame (audio or video) could be played.
    bool init_failed : 1;
    // Entry was removed with playlist_remove (etc.), but not deallocated.
    bool removed : 1;
    // Additional refcount. Normally (reserved==0), the entry is owned by the
    // playlist, and this can be used to keep the entry alive.
    int reserved;
    // Any flags from STREAM_ORIGIN_FLAGS. 0 if unknown.
    // Used to reject loading of unsafe entries from external playlists.
    int stream_flags;
};

struct playlist {
    struct playlist_entry **entries;
    int num_entries;

    // This provides some sort of stable iterator. If this entry is removed from
    // the playlist, current is set to the next element (or NULL), and
    // current_was_replaced is set to true.
    struct playlist_entry *current;
    bool current_was_replaced;
    bool playlist_completed;
    bool playlist_started;
    char *playlist_dir;

    uint64_t id_alloc;
};

void playlist_entry_add_param(struct playlist_entry *e, bstr name, bstr value);
void playlist_entry_add_params(struct playlist_entry *e,
                               struct playlist_param *params,
                               int params_count);

struct playlist_entry *playlist_entry_new(const char *filename);

void playlist_insert_at(struct playlist *pl, struct playlist_entry *entry,
                        struct playlist_entry *at);

void playlist_remove(struct playlist *pl, struct playlist_entry *entry);
void playlist_clear(struct playlist *pl);
void playlist_clear_except_current(struct playlist *pl);

void playlist_move(struct playlist *pl, struct playlist_entry *entry,
                   struct playlist_entry *at);

void playlist_append_file(struct playlist *pl, const char *filename);
void playlist_populate_playlist_path(struct playlist *pl, const char *path);
void playlist_shuffle(struct playlist *pl);
void playlist_unshuffle(struct playlist *pl);
struct playlist_entry *playlist_get_first(struct playlist *pl);
struct playlist_entry *playlist_get_last(struct playlist *pl);
struct playlist_entry *playlist_get_next(struct playlist *pl, int direction);
struct playlist_entry *playlist_entry_get_rel(struct playlist_entry *e,
                                              int direction);
struct playlist_entry *playlist_get_first_in_next_playlist(struct playlist *pl,
                                                           int direction);
struct playlist_entry *playlist_get_first_in_same_playlist(struct playlist_entry *entry,
                                                           char *current_playlist_path);
void playlist_add_base_path(struct playlist *pl, bstr base_path);
void playlist_set_stream_flags(struct playlist *pl, int flags);
int64_t playlist_transfer_entries_to(struct playlist *pl, int dst_index,
                                     struct playlist *source_pl);
int64_t playlist_transfer_entries(struct playlist *pl, struct playlist *source_pl);
int64_t playlist_append_entries(struct playlist *pl, struct playlist *source_pl);

int playlist_entry_to_index(struct playlist *pl, struct playlist_entry *e);
int playlist_entry_count(struct playlist *pl);
struct playlist_entry *playlist_entry_from_index(struct playlist *pl, int index);

struct mp_cancel;
struct mpv_global;
struct playlist *playlist_parse_file(const char *file, struct mp_cancel *cancel,
                                     struct mpv_global *global);

void playlist_entry_unref(struct playlist_entry *e);

void playlist_set_current(struct playlist *pl);

playlist_id_map_t *playlist_build_id_map(const struct playlist *pl);
void playlist_destroy_id_map(playlist_id_map_t *h);
struct playlist_entry *playlist_entry_from_id(struct playlist *pl, const playlist_id_map_t *h, uint64_t id);

void playlist_entry_remove(struct playlist_entry *entry);

#endif
