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

// Add entry "add" after entry "after".
// If "after" is NULL, add as first entry.
// Post condition: add->prev == after
void playlist_insert(struct playlist *pl, struct playlist_entry *after,
                     struct playlist_entry *add)
{
    assert(pl && add->pl == NULL && add->next == NULL && add->prev == NULL);
    if (after) {
        assert(after->pl == pl);
        assert(pl->first && pl->last);
    }
    add->prev = after;
    if (after) {
        add->next = after->next;
        after->next = add;
    } else {
        add->next = pl->first;
        pl->first = add;
    }
    if (add->next) {
        add->next->prev = add;
    } else {
        pl->last = add;
    }
    add->pl = pl;
    talloc_steal(pl, add);
}

void playlist_add(struct playlist *pl, struct playlist_entry *add)
{
    playlist_insert(pl, pl->last, add);
}

static void playlist_unlink(struct playlist *pl, struct playlist_entry *entry)
{
    assert(pl && entry->pl == pl);

    if (pl->current == entry) {
        pl->current = entry->next;
        pl->current_was_replaced = true;
    }

    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        pl->last = entry->prev;
    }
    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        pl->first = entry->next;
    }
    entry->next = entry->prev = NULL;
    // xxx: we'd want to reset the talloc parent of entry
    entry->pl = NULL;
}

void playlist_entry_unref(struct playlist_entry *e)
{
    e->reserved--;
    if (e->reserved < 0)
        talloc_free(e);
}

void playlist_remove(struct playlist *pl, struct playlist_entry *entry)
{
    playlist_unlink(pl, entry);
    entry->removed = true;
    playlist_entry_unref(entry);
}

void playlist_clear(struct playlist *pl)
{
    while (pl->first)
        playlist_remove(pl, pl->first);
    assert(!pl->current);
    pl->current_was_replaced = false;
}

// Moves the entry so that it takes "at"'s place (or move to end, if at==NULL).
void playlist_move(struct playlist *pl, struct playlist_entry *entry,
                   struct playlist_entry *at)
{
    if (entry == at)
        return;

    struct playlist_entry *save_current = pl->current;
    bool save_replaced = pl->current_was_replaced;

    playlist_unlink(pl, entry);
    playlist_insert(pl, at ? at->prev : pl->last, entry);

    pl->current = save_current;
    pl->current_was_replaced = save_replaced;
}

void playlist_add_file(struct playlist *pl, const char *filename)
{
    playlist_add(pl, playlist_entry_new(filename));
}

static int playlist_count(struct playlist *pl)
{
    int c = 0;
    for (struct playlist_entry *e = pl->first; e; e = e->next)
        c++;
    return c;
}

void playlist_shuffle(struct playlist *pl)
{
    struct playlist_entry *save_current = pl->current;
    bool save_replaced = pl->current_was_replaced;
    int count = playlist_count(pl);
    struct playlist_entry **arr = talloc_array(NULL, struct playlist_entry *,
                                               count);
    for (int n = 0; n < count; n++) {
        arr[n] = pl->first;
        playlist_unlink(pl, pl->first);
    }
    for (int n = 0; n < count - 1; n++) {
        int j = (int)((double)(count - n) * rand() / (RAND_MAX + 1.0));
        MPSWAP(struct playlist_entry *, arr[n], arr[n + j]);
    }
    for (int n = 0; n < count; n++)
        playlist_add(pl, arr[n]);
    talloc_free(arr);
    pl->current = save_current;
    pl->current_was_replaced = save_replaced;
}

struct playlist_entry *playlist_get_next(struct playlist *pl, int direction)
{
    assert(direction == -1 || direction == +1);
    if (!pl->current)
        return NULL;
    assert(pl->current->pl == pl);
    if (direction < 0)
        return pl->current->prev;
    return pl->current_was_replaced ? pl->current : pl->current->next;
}

void playlist_add_base_path(struct playlist *pl, bstr base_path)
{
    if (base_path.len == 0 || bstrcmp0(base_path, ".") == 0)
        return;
    for (struct playlist_entry *e = pl->first; e; e = e->next) {
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
    for (struct playlist_entry *e = pl->first; e; e = e->next) {
        if (e->num_redirects >= 10) // arbitrary limit for sanity
            break;
        char *s = talloc_strdup(e, redirected_from);
        if (s)
            MP_TARRAY_APPEND(e, e->redirects, e->num_redirects, s);
    }
}

// Move all entries from source_pl to pl, appending them after the current entry
// of pl. source_pl will be empty, and all entries have changed ownership to pl.
void playlist_transfer_entries(struct playlist *pl, struct playlist *source_pl)
{
    struct playlist_entry *add_after = pl->current;
    if (pl->current && pl->current_was_replaced)
        add_after = pl->current->next;
    if (!add_after)
        add_after = pl->last;

    while (source_pl->first) {
        struct playlist_entry *e = source_pl->first;
        playlist_unlink(source_pl, e);
        playlist_insert(pl, add_after, e);
        add_after = e;
    }
}

void playlist_append_entries(struct playlist *pl, struct playlist *source_pl)
{
    while (source_pl->first) {
        struct playlist_entry *e = source_pl->first;
        playlist_unlink(source_pl, e);
        playlist_add(pl, e);
    }
}

// Return number of entries between list start and e.
// Return -1 if e is not on the list, or if e is NULL.
int playlist_entry_to_index(struct playlist *pl, struct playlist_entry *e)
{
    struct playlist_entry *cur = pl->first;
    int pos = 0;
    if (!e)
        return -1;
    while (cur && cur != e) {
        cur = cur->next;
        pos++;
    }
    return cur == e ? pos : -1;
}

int playlist_entry_count(struct playlist *pl)
{
    return playlist_entry_to_index(pl, pl->last) + 1;
}

// Return entry for which playlist_entry_to_index() would return index.
// Return NULL if not found.
struct playlist_entry *playlist_entry_from_index(struct playlist *pl, int index)
{
    struct playlist_entry *e = pl->first;
    for (int n = 0; ; n++) {
        if (!e || n == index)
            return e;
        e = e->next;
    }
}

struct playlist *playlist_parse_file(const char *file, struct mp_cancel *cancel,
                                     struct mpv_global *global)
{
    struct mp_log *log = mp_log_new(NULL, global->log, "!playlist_parser");
    mp_verbose(log, "Parsing playlist file %s...\n", file);

    struct demuxer_params p = {.force_format = "playlist"};
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
    free_demuxer_and_stream(d);

    if (ret) {
        mp_verbose(log, "Playlist successfully parsed\n");
    } else {
        mp_err(log, "Error while parsing playlist\n");
    }

    if (ret && !ret->first)
        mp_warn(log, "Warning: empty playlist\n");

    talloc_free(log);
    return ret;
}
