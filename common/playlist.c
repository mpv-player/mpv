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
#include "config.h"
#include "playlist.h"
#include "common/common.h"
#include "common/global.h"
#include "common/msg.h"
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

void playlist_shuffle(struct playlist *pl)
{
    for (int n = 0; n < pl->num_entries; n++)
        pl->entries[n]->original_index = n;
    for (int n = 0; n < pl->num_entries - 1; n++) {
        int j = (int)((double)(pl->num_entries - n) * rand() / (RAND_MAX + 1.0));
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

// Add redirected_from as new redirect entry to each item in pl.
void playlist_add_redirect(struct playlist *pl, const char *redirected_from)
{
    for (int n = 0; n < pl->num_entries; n++) {
        struct playlist_entry *e = pl->entries[n];
        if (e->num_redirects >= 10) // arbitrary limit for sanity
            continue;
        char *s = talloc_strdup(e, redirected_from);
        if (s)
            MP_TARRAY_APPEND(e, e->redirects, e->num_redirects, s);
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
