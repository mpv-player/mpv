/*
 * This file is part of mplayer.
 *
 * mplayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <sys/stat.h>
#include <dirent.h>
#include "config.h"
#include "playlist.h"
#include "mpvcore/mp_common.h"
#include "talloc.h"
#include "mpvcore/path.h"
#include "osdep/io.h"

struct playlist_entry *playlist_entry_new(const char *filename)
{
    struct playlist_entry *e = talloc_zero(NULL, struct playlist_entry);
    e->filename = talloc_strdup(e, filename);
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

void playlist_remove(struct playlist *pl, struct playlist_entry *entry)
{
    playlist_unlink(pl, entry);
    talloc_free(entry);
}

void playlist_clear(struct playlist *pl)
{
    while (pl->first)
        playlist_remove(pl, pl->first);
    assert(!pl->current);
    pl->current_was_replaced = false;
}

// Moves entry such that entry->prev = at (even if at is NULL)
void playlist_move(struct playlist *pl, struct playlist_entry *entry,
                   struct playlist_entry *at)
{
    struct playlist_entry *save_current = pl->current;
    bool save_replaced = pl->current_was_replaced;

    playlist_unlink(pl, entry);
    playlist_insert(pl, at ? at->prev : pl->last, entry);

    pl->current = save_current;
    pl->current_was_replaced = save_replaced;
}

struct ino_elem {
    ino_t ino;
    dev_t dev;
    struct ino_elem *next;
};

void playlist_add_filepath_wrk(struct playlist *pl, const char *filepath,
            struct ino_elem *ino_list);

int entry_compare(const struct playlist_entry **e1,
        const struct playlist_entry **e2);

void playlist_add_filepath(struct playlist *pl, const char *filepath)
{
    void *ctx = talloc_new(NULL);
    struct playlist *new_pl = talloc_zero(ctx, struct playlist);
    if (!mp_path_isdir(filepath)) {
        playlist_add_file(pl, filepath);
        talloc_free(ctx);
        return;
    }
    struct ino_elem *ino_list = talloc_zero(ctx, struct ino_elem);
    ino_list->ino = -1;
    ino_list->dev = -1;
    ino_list->next = NULL;
    playlist_add_filepath_wrk(new_pl, filepath, ino_list);
    playlist_sort(new_pl, entry_compare);
    playlist_transfer_entries(pl, new_pl);
    talloc_free(ctx);
}

void playlist_add_filepath_wrk(struct playlist *pl, const char *filepath,
    struct ino_elem *ino_list)
{
    void *ctx = talloc_new(NULL);
    struct playlist *new_pl = talloc_zero(ctx, struct playlist);
    DIR *dir;
    if (!mp_path_isdir(filepath) || (dir = opendir(filepath)) == NULL ) {
        playlist_add_file(new_pl, filepath);
        talloc_free(ctx);
        return;
    }
    
    // List of dir inodes, to detect symlink loops. List survives recursion.
    struct ino_elem *new_ino_list;
    if (ino_list == NULL) {
        new_ino_list = talloc_zero(ctx, struct ino_elem);
        new_ino_list->ino = -1;
        new_ino_list->dev = -1;
        new_ino_list->next = NULL;
    } else {
        new_ino_list = ino_list;
    }
    // If we aren't on windows, check and append to inode list.
    #ifndef __MINGW32__
    struct stat *buf = talloc_zero(ctx, struct stat);
    if (stat(filepath, buf) == -1) {
        closedir(dir);
        talloc_free(ctx);
        return;
    }
    struct ino_elem *ino_ptr = new_ino_list;
    while ((ino_ptr = ino_ptr->next) != NULL) {
        if (ino_ptr->ino == buf->st_ino && ino_ptr->dev == buf->st_dev) {
            closedir(dir);
            talloc_free(ctx);
            return;
        }
    }
    // Attach new inode_elem to ino_list ctx, since it's persistent.
    ino_ptr = talloc_zero(new_ino_list, struct ino_elem);
    ino_ptr->ino = buf->st_ino;
    ino_ptr->dev = buf->st_dev;
    ino_ptr->next = new_ino_list->next;
    new_ino_list->next = ino_ptr;
    #endif

    struct dirent *entry;
    bstr base_path = bstrdup(ctx,bstr0(filepath));
    while ((entry = readdir(dir)) != NULL) {
        bstr dname = bstrdup(ctx,bstr0(entry->d_name));
        bstr new_path = bstr0(mp_path_join(ctx, base_path, dname));
        if (bstr_startswith0(dname, ".") || bstrcmp0(dname, "") == 0) {
            continue;
        }
        if (mp_path_isdir(new_path.start)) {
            playlist_add_filepath_wrk(new_pl, new_path.start, new_ino_list);
        } else {
            playlist_add_file(new_pl, new_path.start);
        }
    }
    closedir(dir);
    playlist_transfer_entries(pl, new_pl);
    talloc_free(ctx);
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
    for (int n = 0; n < count; n++) {
        int other = (int)((double)(count) * rand() / (RAND_MAX + 1.0));
        struct playlist_entry *tmp = arr[n];
        arr[n] = arr[other];
        arr[other] = tmp;
    }
    for (int n = 0; n < count; n++)
        playlist_add(pl, arr[n]);
    talloc_free(arr);
    pl->current = save_current;
    pl->current_was_replaced = save_replaced;
}

// Compare function for playlist entries, based on directory, then filename.
int entry_compare(const struct playlist_entry **e1,
        const struct playlist_entry **e2)
{
    if (e1 == NULL || *e1 == NULL)
        return -1;
    if (e2 == NULL || *e2 == NULL)
        return 1;
    int compare = bstrcmp(mp_dirname((*e1)->filename),
            mp_dirname((*e2)->filename));
    if (compare != 0) {
        return compare;
    }
    return strcmp(mp_basename((*e1)->filename),
            mp_basename((*e2)->filename));
}

void playlist_sort(struct playlist *pl,
        int (*compar)(const struct playlist_entry **e1,
            const struct playlist_entry **e2))
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
    qsort(arr, count, sizeof(struct playlist_entry *),
            (int (*)(const void *, const void *)) compar);
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
            char *new_file = mp_path_join(e, base_path, bstr0(e->filename));
            talloc_free(e->filename);
            e->filename = new_file;
        }
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

