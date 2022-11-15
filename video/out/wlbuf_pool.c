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
#include <unistd.h>

#include "mpv_talloc.h"
#include "common/global.h"
#include "vo.h"
#include "video/mp_image.h"

#include "wayland_common.h"
#include "generated/wayland/linux-dmabuf-unstable-v1.h"
#include "pthread.h"
#include "wlbuf_pool.h"

#define WLBUF_POOL_NUM_ALLOCATED_INIT 30

static void wlbuf_pool_entry_free(struct wlbuf_pool_entry *entry);

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
    pthread_mutex_init(&pool->lock, NULL);
    pool->wl = wl;

    return pool;
}

void wlbuf_pool_clean(struct wlbuf_pool *pool)
{
    int i;
    if (!pool)
        return;
    pthread_mutex_lock(&pool->lock);
    MP_VERBOSE(pool->vo, "Begin clean pool\n");
    for (i = 0; i < pool->num_allocated; ++i) {
        struct wlbuf_pool_entry *entry = pool->entries[i];
        if (!entry)
            continue;
        // force frame unref
        if (pool->final_clean && entry->frame){
            mp_image_unrefp(&entry->frame);
            entry->frame = NULL;
        }
        wlbuf_pool_entry_free(entry);
        pool->entries[i] = NULL;
    }
    pool->num_entries = 0;
    MP_VERBOSE(pool->vo, "End clean pool\n");
    pthread_mutex_unlock(&pool->lock);
}

void wlbuf_pool_free(struct wlbuf_pool *pool)
{
    if (!pool)
        return;
    pool->final_clean = true;
    wlbuf_pool_clean(pool);
    pthread_mutex_destroy(&pool->lock);
    talloc_free(pool);
}

static void wlbuf_pool_entry_free(struct wlbuf_pool_entry *entry)
{
    if (!entry)
        return;
    if (entry->frame) {
        MP_VERBOSE(entry->vo, "Pending free buffer pool entry : %lu\n",entry->key );
        entry->pending_delete = true;
    }
    else {
        MP_VERBOSE(entry->vo, "Free buffer pool entry : %lu\n",entry->key );
        if (entry->buffer)
            wl_buffer_destroy(entry->buffer);
        entry->buffer = NULL;
        talloc_free(entry);
    }
}

static void wlbuf_pool_entry_release(void *data, struct wl_buffer *wl_buffer)
{
    struct wlbuf_pool_entry *entry = (struct wlbuf_pool_entry*)data;
    struct mp_image *frame;
    pthread_mutex_t *lock = entry->pool_lock;

    MP_VERBOSE(entry->vo, "Release buffer pool entry : %lu\n",entry->key );
    pthread_mutex_lock(lock);
    frame = entry->frame;
    entry->frame = NULL;
    if (entry->pending_delete)
        wlbuf_pool_entry_free(entry);
    if (frame)
        mp_image_unrefp(&frame);
    pthread_mutex_unlock(lock);
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
    pthread_mutex_lock(&pool->lock);
    for (int i = 0; i < pool->num_entries; ++i) {
        struct wlbuf_pool_entry *item = pool->entries[i];
        if (item->key == key) {
            pthread_mutex_unlock(&pool->lock);
            if (item->frame){
                mp_image_unrefp(&src);
                return NULL;
            } else {
                item->frame = src;
                return item;
            }
        }
    }
    pthread_mutex_unlock(&pool->lock);
    /* 2. otherwise allocate new entry and buffer */
    entry = talloc(pool, struct wlbuf_pool_entry);
    memset(entry, 0, sizeof(struct wlbuf_pool_entry));
    entry->vo = pool->vo;
    entry->key = pool->key_provider(src);
    entry->pool_lock = &pool->lock;
    MP_VERBOSE(entry->vo, "Allocate buffer pool entry : %lu\n",entry->key );
    params = zwp_linux_dmabuf_v1_create_params(wl->dmabuf);
    import_successful = pool->dmabuf_importer(src,entry,params);
    if (!import_successful) {
        MP_VERBOSE(entry->vo, "Failed to import\n");
        wlbuf_pool_entry_free(entry);
    } else {
        entry->buffer = zwp_linux_buffer_params_v1_create_immed(params, src->params.w, src->params.h,
                                                                entry->drm_format, 0);
    }
    zwp_linux_buffer_params_v1_destroy(params);
    if (!import_successful){
         mp_image_unrefp(&src);
         return NULL;
    }

    /* 3. add new entry to pool */
    if (pool->num_entries == pool->num_allocated) {
        int current_num_allocated = pool->num_allocated;
        pool->num_allocated *= 2;
        pthread_mutex_lock(&pool->lock);
        pool->entries = talloc_realloc(pool, pool->entries, struct wlbuf_pool_entry *, pool->num_allocated);
        for (int i = current_num_allocated; i < pool->num_allocated; ++i)
            pool->entries[i] = NULL;
        pthread_mutex_unlock(&pool->lock);
    }
    wl_buffer_add_listener(entry->buffer, &wlbuf_pool_listener, entry);
    entry->frame = src;
    pthread_mutex_lock(&pool->lock);
    pool->entries[pool->num_entries++] = entry;
    pthread_mutex_unlock(&pool->lock);

    return entry;
}
