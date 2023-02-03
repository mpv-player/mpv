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

#include "mpv_talloc.h"
#include "common/global.h"
#include "vo.h"
#include "video/mp_image.h"

#include "wayland_common.h"
#include "generated/wayland/linux-dmabuf-unstable-v1.h"
#include "pthread.h"
#include "wlbuf_pool.h"

#define WLBUF_POOL_NUM_ALLOCATED_INIT 30

static void wlbuf_pool_entry_free(struct wlbuf_pool_entry *entry, bool force);

/**
 * When trying to free pool entries that are being held by Wayland,
 * these entries are set to pending free state. They will be freed
 * when either the frame listener gets called, or the vo is uninitialized
*/
static bool set_pending_free(struct wlbuf_pool_entry *entry) {
    if (entry->image) {
        MP_TRACE(entry->vo, "Pending free for buffer pool entry : %lu\n",
                entry->key);
        entry->pending_free = true;
        // add to purgatory
        struct wlbuf_pool *pool = entry->pool;
        for (int i = 0; i < WLBUF_NUM_PURG_ENTRIES; ++i) {
            if (pool->purg.entries[i] == NULL) {
                pool->purg.entries[i] = entry;
                break;
            }
        }
    }

    return entry->image;
}

/**
 * Complete pending free of entry.
 *
 */
static void complete_pending_free(struct wlbuf_pool_entry *entry) {
    if (entry->pending_free) {
        // 1. remove from purgatory
        struct wlbuf_pool *pool = entry->pool;
        for (int i = 0; i < WLBUF_NUM_PURG_ENTRIES; ++i) {
            if (pool->purg.entries[i] == entry) {
                pool->purg.entries[i] = NULL;
                break;
            }
        }
        // 2. free entry
        wlbuf_pool_entry_free(entry, false);
    }
}

/**
 * Free all purgatory entries
 */
static void free_purgatory(struct wlbuf_pool *pool){
    for (int i = 0; i < WLBUF_NUM_PURG_ENTRIES; ++i)
        wlbuf_pool_entry_free(pool->purg.entries[i], true);
}

struct wlbuf_pool *wlbuf_pool_alloc(struct vo *vo, struct vo_wayland_state *wl, wlbuf_pool_key_provider key_provider,
                                    wlbuf_pool_dmabuf_importer dmabuf_importer)
{
    struct wlbuf_pool *pool = talloc(NULL, struct wlbuf_pool);
    memset(pool, 0, sizeof(struct wlbuf_pool));
    pool->num_allocated = WLBUF_POOL_NUM_ALLOCATED_INIT;
    pool->entries = talloc_array(pool, struct wlbuf_pool_entry *, pool->num_allocated);
    memset(pool->entries, 0, pool->num_allocated * sizeof(struct wlbuf_pool_entry *));
    pool->vo = vo;
    pool->key_provider = key_provider;
    pool->dmabuf_importer = dmabuf_importer;
    pool->wl = wl;
    for (int i = 0; i < WLBUF_NUM_PURG_ENTRIES; ++i)
        pool->purg.entries[i] = NULL;

    return pool;
}

void wlbuf_pool_clean(struct wlbuf_pool *pool, bool final_clean)
{
    int i;
    if (!pool)
        return;
    MP_TRACE(pool->vo, "Begin clean pool\n");
    if (final_clean)
        free_purgatory(pool);
    for (i = 0; i < pool->num_allocated; ++i) {
        struct wlbuf_pool_entry *entry = pool->entries[i];
        if (!entry)
            continue;
        wlbuf_pool_entry_free(entry, final_clean);
        pool->entries[i] = NULL;
    }
    pool->num_entries = 0;
    MP_TRACE(pool->vo, "End clean pool\n");
}

void wlbuf_pool_free(struct wlbuf_pool *pool)
{
    if (!pool)
        return;

    wlbuf_pool_clean(pool, true);
    talloc_free(pool);
}

static void wlbuf_pool_entry_free(struct wlbuf_pool_entry *entry, bool force)
{
    if (!entry)
        return;
    if (force && entry->image) {
        mp_image_unrefp(&entry->image);
        entry->image = NULL;
    }
    if (!set_pending_free(entry)) {
        MP_TRACE(entry->vo, "Free buffer pool entry : %lu\n", entry->key);
        if (entry->buffer)
            wl_buffer_destroy(entry->buffer);
        talloc_free(entry);
    }
}

/**
 * Unref pool entry's image, and also free entry itself if it's set to pending_free
 */
static void wlbuf_pool_entry_release(void *data, struct wl_buffer *wl_buffer)
{
    struct wlbuf_pool_entry *entry = (struct wlbuf_pool_entry*) data;

    MP_TRACE(entry->vo, "Release buffer pool entry : %lu\n", entry->key);

    // 1. always unref image
    if (entry->image)
        mp_image_unrefp(&entry->image);
    entry->image = NULL;

    // 2. complete pending free if needed
    complete_pending_free(entry);
}

static const struct wl_buffer_listener wlbuf_pool_listener = {
    wlbuf_pool_entry_release,
};

struct wlbuf_pool_entry *wlbuf_pool_get_entry(struct wlbuf_pool *pool, struct mp_image *src)
{
    uintptr_t key;
    struct wlbuf_pool_entry *entry;
    struct vo_wayland_state *wl = pool->wl;
    bool import_successful;
    struct zwp_linux_buffer_params_v1 *params;

    if (!pool || !src)
        return NULL;

    /* 1. try to find existing entry in pool */
    src = mp_image_new_ref(src);
    key = pool->key_provider(src);
    for (int i = 0; i < pool->num_entries; ++i) {
        struct wlbuf_pool_entry *item = pool->entries[i];
        if (!item)
            continue;
        if (item->key == key) {
            if (item->image) {
                mp_image_unrefp(&src);
                MP_DBG(item->vo, "Buffer already scheduled - returning NULL.\n");
                return NULL;
            } else {
                item->image = src;
                return item;
            }
        }
    }
    /* 2. otherwise allocate new entry and buffer */
    entry = talloc(pool, struct wlbuf_pool_entry);
    memset(entry, 0, sizeof(struct wlbuf_pool_entry));
    entry->vo = pool->vo;
    entry->key = pool->key_provider(src);
    entry->pool = pool;
    MP_TRACE(entry->vo, "Allocate buffer pool entry : %lu\n", entry->key);
    params = zwp_linux_dmabuf_v1_create_params(wl->dmabuf);
    import_successful = pool->dmabuf_importer(src, entry, params);
    if (!import_successful) {
        MP_DBG(entry->vo, "Failed to import\n");
        wlbuf_pool_entry_free(entry, false);
    } else {
        entry->buffer = zwp_linux_buffer_params_v1_create_immed(params, src->params.w, src->params.h,
                                                                entry->drm_format, 0);
    }
    zwp_linux_buffer_params_v1_destroy(params);
    if (!import_successful) {
        mp_image_unrefp(&src);
        return NULL;
    }
    /* 3. add new entry to pool */
    if (pool->num_entries == pool->num_allocated) {
        int current_num_allocated = pool->num_allocated;
        pool->num_allocated *= 2;
        pool->entries = talloc_realloc(pool, pool->entries, struct wlbuf_pool_entry *, pool->num_allocated);
        for (int i = current_num_allocated; i < pool->num_allocated; ++i)
            pool->entries[i] = NULL;
    }
    wl_buffer_add_listener(entry->buffer, &wlbuf_pool_listener, entry);
    entry->image = src;
    pool->entries[pool->num_entries++] = entry;

    return entry;
}
