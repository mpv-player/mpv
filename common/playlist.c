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

#include <assert.h>
#include "playlist.h"
#include "common/common.h"
#include "common/global.h"
#include "common/msg.h"
#include "misc/random.h"
#include "mpv_talloc.h"
#include "options/path.h"

#include "demux/demux.h"
#include "stream/stream.h"

struct playlist_entry *playlist_entry_new(const char *filename)
{
    struct playlist_entry *e = talloc_zero(NULL, struct playlist_entry);
    char *local_filename = mp_file_url_to_filename(e, bstr0(filename));
    e->filename = local_filename ? local_filename : talloc_strdup(e, filename);
    e->stream_flags = STREAM_ORIGIN_DIRECT;
    e->original_index = -1;
    return e;
}

void playlist_entry_add_param(struct playlist_entry *e, bstr name, bstr value)
{
    struct playlist_param p = {bstrdup(e, name), bstrdup(e, value)};
    MP_TARRAY_APPEND(e, e->params, e->num_params, p);
}

void playlist_entry_add_params(struct playlist_entry *e,
                               struct playlist_param *params,
                               int num_params)
{
    for (int n = 0; n < num_params; n++)
        playlist_entry_add_param(e, params[n].name, params[n].value);
}

static void playlist_update_indexes(struct playlist *pl, int start, int end)
{
    start = MPMAX(start, 0);
    end = end < 0 ? pl->num_entries : MPMIN(end, pl->num_entries);

    for (int n = start; n < end; n++)
        pl->entries[n]->pl_index = n;
}

void playlist_add(struct playlist *pl, struct playlist_entry *add)
{
    assert(add->filename);
    MP_TARRAY_APPEND(pl, pl->entries, pl->num_entries, add);
    add->pl = pl;
    add->pl_index = pl->num_entries - 1;
    add->id = ++pl->id_alloc;
    talloc_steal(pl, add);
}

// Inserts the entry so that it comes directly after "at" (or move to end, if at==NULL).
void playlist_insert_next(struct playlist *pl, struct playlist_entry *add,
                          struct playlist_entry *at)
{
    assert(add->filename);
    assert(!at || at->pl == pl);

    int index = at ? at->pl_index + 1 : pl->num_entries;
    MP_TARRAY_INSERT_AT(pl, pl->entries, pl->num_entries, index, add);

    add->pl = pl;
    add->pl_index = index;
    add->id = ++pl->id_alloc;

    playlist_update_indexes(pl, index, pl->num_entries);

    talloc_steal(pl, add);
}

void playlist_entry_unref(struct playlist_entry *e)
{
    e->reserved--;
    if (e->reserved < 0) {
        assert(!e->pl);
        talloc_free(e);
    }
}

void playlist_remove(struct playlist *pl, struct playlist_entry *entry)
{
    assert(pl && entry->pl == pl);

    if (pl->current == entry) {
        pl->current = playlist_entry_get_rel(entry, 1);
        pl->current_was_replaced = true;
    }

    MP_TARRAY_REMOVE_AT(pl->entries, pl->num_entries, entry->pl_index);
    playlist_update_indexes(pl, entry->pl_index, -1);

    entry->pl = NULL;
    entry->pl_index = -1;
    ta_set_parent(entry, NULL);

    entry->removed = true;
    playlist_entry_unref(entry);
}

void playlist_clear(struct playlist *pl)
{
    for (int n = pl->num_entries - 1; n >= 0; n--)
        playlist_remove(pl, pl->entries[n]);
    assert(!pl->current);
    pl->current_was_replaced = false;
}

void playlist_clear_except_current(struct playlist *pl)
{
    for (int n = pl->num_entries - 1; n >= 0; n--) {
        if (pl->entries[n] != pl->current)
            playlist_remove(pl, pl->entries[n]);
    }
}

// Moves the entry so that it takes "at"'s place (or move to end, if at==NULL).
void playlist_move(struct playlist *pl, struct playlist_entry *entry,
                   struct playlist_entry *at)
{
    if (entry == at)
        return;

    assert(entry && entry->pl == pl);
    assert(!at || at->pl == pl);

    int index = at ? at->pl_index : pl->num_entries;
    MP_TARRAY_INSERT_AT(pl, pl->entries, pl->num_entries, index, entry);

    int old_index = entry->pl_index;
    if (old_index >= index)
        old_index += 1;
    MP_TARRAY_REMOVE_AT(pl->entries, pl->num_entries, old_index);

    playlist_update_indexes(pl, MPMIN(index - 1, old_index - 1),
                                MPMAX(index + 1, old_index + 1));
}

void playlist_add_file(struct playlist *pl, const char *filename)
{
    playlist_add(pl, playlist_entry_new(filename));
}

void playlist_populate_playlist_path(struct playlist *pl, const char *path)
{
    for (int n = 0; n < pl->num_entries; n++) {
        struct playlist_entry *e = pl->entries[n];
        e->playlist_path = talloc_strdup(e, path);
    }
}

void playlist_shuffle(struct playlist *pl)
{
    for (int n = 0; n < pl->num_entries; n++)
        pl->entries[n]->original_index = n;
    for (int n = 0; n < pl->num_entries - 1; n++) {
        size_t j = (size_t)((pl->num_entries - n) * mp_rand_next_double());
        MPSWAP(struct playlist_entry *, pl->entries[n], pl->entries[n + j]);
    }
    playlist_update_indexes(pl, 0, -1);
}

#define CMP_INT(a, b) ((a) == (b) ? 0 : ((a) > (b) ? 1 : -1))

static int cmp_unshuffle(const void *a, const void *b)
{
    struct playlist_entry *ea = *(struct playlist_entry **)a;
    struct playlist_entry *eb = *(struct playlist_entry **)b;

    if (ea->original_index >= 0 && ea->original_index != eb->original_index)
        return CMP_INT(ea->original_index, eb->original_index);
    return CMP_INT(ea->pl_index, eb->pl_index);
}

void playlist_unshuffle(struct playlist *pl)
{
    if (pl->num_entries)
        qsort(pl->entries, pl->num_entries, sizeof(pl->entries[0]), cmp_unshuffle);
    playlist_update_indexes(pl, 0, -1);
}

// (Explicitly ignores current_was_replaced.)
struct playlist_entry *playlist_get_first(struct playlist *pl)
{
    return pl->num_entries ? pl->entries[0] : NULL;
}

// (Explicitly ignores current_was_replaced.)
struct playlist_entry *playlist_get_last(struct playlist *pl)
{
    return pl->num_entries ? pl->entries[pl->num_entries - 1] : NULL;
}

struct playlist_entry *playlist_get_next(struct playlist *pl, int direction)
{
    assert(direction == -1 || direction == +1);
    if (!pl->current)
        return NULL;
    assert(pl->current->pl == pl);
    if (direction < 0)
        return playlist_entry_get_rel(pl->current, -1);
    return pl->current_was_replaced ? pl->current :
           playlist_entry_get_rel(pl->current, 1);
}

// (Explicitly ignores current_was_replaced.)
struct playlist_entry *playlist_entry_get_rel(struct playlist_entry *e,
                                              int direction)
{
    assert(direction == -1 || direction == +1);
    if (!e->pl)
        return NULL;
    return playlist_entry_from_index(e->pl, e->pl_index + direction);
}

struct playlist_entry *playlist_get_first_in_next_playlist(struct playlist *pl,
                                                           int direction)
{
    struct playlist_entry *entry = playlist_get_next(pl, direction);
    if (!entry)
        return NULL;

    while (entry && entry->playlist_path && pl->current->playlist_path &&
           strcmp(entry->playlist_path, pl->current->playlist_path) == 0)
        entry = playlist_entry_get_rel(entry, direction);

    if (direction < 0)
        entry = playlist_get_first_in_same_playlist(entry,
                                                    pl->current->playlist_path);

    return entry;
}

struct playlist_entry *playlist_get_first_in_same_playlist(
    struct playlist_entry *entry, char *current_playlist_path)
{
    void *tmp = talloc_new(NULL);

    if (!entry || !entry->playlist_path)
        goto exit;

    // Don't go to the beginning of the playlist when the current playlist-path
    // starts with the previous playlist-path, e.g. with mpv --loop-playlist
    // archive_dir/, which expands to archive_dir/{1..9}.zip, the current
    // playlist path "archive_dir/1.zip" begins with the playlist-path
    // "archive_dir/" of {2..9}.zip, so go to 9.zip instead of 2.zip. But
    // playlist-prev-playlist from e.g. the directory "foobar" to the directory
    // "foo" should still go to the first entry in "foo/", and this should all
    // work whether mpv's arguments have trailing slashes or not, e.g. in the
    // first example:
    // mpv archive_dir results in the playlist-paths "archive_dir/1.zip" and
    // "archive_dir"
    // mpv archive_dir/ in "archive_dir/1.zip" and "archive_dir/"
    // mpv archive_dir// in "archive_dir//1.zip" and "archive_dir//"
    // Always adding a separator to entry->playlist_path to fix the foobar foo
    // case would break the previous 2 cases instead. Stripping the separator
    // from entry->playlist_path if present and appending it again makes this
    // work in all cases.
    char* playlist_path = talloc_strdup(tmp, entry->playlist_path);
    mp_path_strip_trailing_separator(playlist_path);
    if (bstr_startswith(bstr0(current_playlist_path),
                        bstr0(talloc_strdup_append(playlist_path, "/")))
#if HAVE_DOS_PATHS
        ||
        bstr_startswith(bstr0(current_playlist_path),
                        bstr0(talloc_strdup_append(playlist_path, "\\")))
#endif
       )
        goto exit;

    struct playlist_entry *prev = playlist_entry_get_rel(entry, -1);

    while (prev && prev->playlist_path &&
           strcmp(prev->playlist_path, entry->playlist_path) == 0) {
        entry = prev;
        prev = playlist_entry_get_rel(entry, -1);
    }

exit:
    talloc_free(tmp);
    return entry;
}

void playlist_add_base_path(struct playlist *pl, bstr base_path)
{
    if (base_path.len == 0 || bstrcmp0(base_path, ".") == 0)
        return;
    for (int n = 0; n < pl->num_entries; n++) {
        struct playlist_entry *e = pl->entries[n];
        if (!mp_is_url(bstr0(e->filename))) {
            char *new_file = mp_path_join_bstr(e, base_path, bstr0(e->filename));
            talloc_free(e->filename);
            e->filename = new_file;
        }
    }
}

void playlist_set_stream_flags(struct playlist *pl, int flags)
{
    for (int n = 0; n < pl->num_entries; n++)
        pl->entries[n]->stream_flags = flags;
}

static int64_t playlist_transfer_entries_to(struct playlist *pl, int dst_index,
                                            struct playlist *source_pl)
{
    assert(pl != source_pl);
    struct playlist_entry *first = playlist_get_first(source_pl);

    int count = source_pl->num_entries;
    MP_TARRAY_INSERT_N_AT(pl, pl->entries, pl->num_entries, dst_index, count);

    for (int n = 0; n < count; n++) {
        struct playlist_entry *e = source_pl->entries[n];
        e->pl = pl;
        e->pl_index = dst_index + n;
        e->id = ++pl->id_alloc;
        pl->entries[e->pl_index] = e;
        talloc_steal(pl, e);
    }

    playlist_update_indexes(pl, dst_index + count, -1);
    source_pl->num_entries = 0;

    return first ? first->id : 0;
}

// Move all entries from source_pl to pl, appending them after the current entry
// of pl. source_pl will be empty, and all entries have changed ownership to pl.
// Return the new ID of the first added entry within pl (0 if source_pl was
// empty). The IDs of all added entries increase by 1 each entry (you can
// predict the ID of the last entry).
int64_t playlist_transfer_entries(struct playlist *pl, struct playlist *source_pl)
{

    int add_at = pl->num_entries;
    if (pl->current) {
        add_at = pl->current->pl_index + 1;
        if (pl->current_was_replaced)
            add_at += 1;
    }
    assert(add_at >= 0);
    assert(add_at <= pl->num_entries);

    return playlist_transfer_entries_to(pl, add_at, source_pl);
}

int64_t playlist_append_entries(struct playlist *pl, struct playlist *source_pl)
{
    return playlist_transfer_entries_to(pl, pl->num_entries, source_pl);
}

// Return number of entries between list start and e.
// Return -1 if e is not on the list, or if e is NULL.
int playlist_entry_to_index(struct playlist *pl, struct playlist_entry *e)
{
    if (!e || e->pl != pl)
        return -1;
    return e->pl_index;
}

int playlist_entry_count(struct playlist *pl)
{
    return pl->num_entries;
}

// Return entry for which playlist_entry_to_index() would return index.
// Return NULL if not found.
struct playlist_entry *playlist_entry_from_index(struct playlist *pl, int index)
{
    return index >= 0 && index < pl->num_entries ? pl->entries[index] : NULL;
}

struct playlist *playlist_parse_file(const char *file, struct mp_cancel *cancel,
                                     struct mpv_global *global)
{
    struct mp_log *log = mp_log_new(NULL, global->log, "!playlist_parser");
    mp_verbose(log, "Parsing playlist file %s...\n", file);

    struct demuxer_params p = {
        .force_format = "playlist",
        .stream_flags = STREAM_ORIGIN_DIRECT,
    };
    struct demuxer *d = demux_open_url(file, &p, cancel, global);
    if (!d) {
        talloc_free(log);
        return NULL;
    }

    struct playlist *ret = NULL;
    if (d && d->playlist) {
        ret = talloc_zero(NULL, struct playlist);
        playlist_populate_playlist_path(d->playlist, file);
        playlist_transfer_entries(ret, d->playlist);
        if (d->filetype && strcmp(d->filetype, "hls") == 0) {
            mp_warn(log, "This might be a HLS stream. For correct operation, "
                         "pass it to the player\ndirectly. Don't use --playlist.\n");
        }
    }
    demux_free(d);

    if (ret) {
        mp_verbose(log, "Playlist successfully parsed\n");
    } else {
        mp_err(log, "Error while parsing playlist\n");
    }

    if (ret && !ret->num_entries)
        mp_warn(log, "Warning: empty playlist\n");

    talloc_free(log);
    return ret;
}
