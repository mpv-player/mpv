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

#include "wayland_common.h"
#include "generated/wayland/linux-dmabuf-unstable-v1.h"

struct wlbuf_pool_entry;

typedef uintptr_t (*wlbuf_pool_key_provider)(struct mp_image *src);
typedef bool (*wlbuf_pool_dmabuf_importer)(struct mp_image *src, struct wlbuf_pool_entry* entry,
                                           struct zwp_linux_buffer_params_v1 *params);

struct wlbuf_pool {
    struct vo *vo;
    struct vo_wayland_state *wl;
    struct wlbuf_pool_entry **entries;
    int num_entries;
    int num_allocated;
    wlbuf_pool_key_provider key_provider;
    wlbuf_pool_dmabuf_importer dmabuf_importer;
    pthread_mutex_t lock;
    bool final_clean;
};

struct wlbuf_pool_entry {
    uintptr_t key;
    struct vo *vo;
    struct wl_buffer *buffer;
    uint32_t drm_format;
    struct mp_image *frame;
    bool pending_delete;
    pthread_mutex_t *pool_lock;
};

/**
 * Allocate pool
 */
struct wlbuf_pool *wlbuf_pool_alloc(struct vo *vo, struct vo_wayland_state *wl, wlbuf_pool_key_provider key_provider,
                                    wlbuf_pool_dmabuf_importer dmabuf_importer);

/**
 * Free pool entries but leave pool itself intact
 */
void wlbuf_pool_clean(struct wlbuf_pool *pool);

/**
 * Free pool
 */
void wlbuf_pool_free(struct wlbuf_pool *pool);

/**
 * Get pool entry - will allocate entry if not present in pool.
 */
struct wlbuf_pool_entry *wlbuf_pool_get_entry(struct wlbuf_pool *pool, struct mp_image *src);
