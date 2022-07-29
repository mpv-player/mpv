/*
 * This file is part of mpv video player.
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

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <va/va_wayland.h>
#include <va/va_drmcommon.h>

#include "present_sync.h"
#include "sub/osd.h"
#include "video/vaapi.h"
#include "wayland_common.h"

#include "generated/wayland/linux-dmabuf-unstable-v1.h"
#include "generated/wayland/viewporter.h"

/********* wl_buffer pool *************************/

#define WLBUF_POOL_NUM_ALLOCATED_INIT 30

struct wlbuf_pool_entry {
    uintptr_t key;
    struct wl_buffer *buffer;
    struct zwp_linux_buffer_params_v1 *params;
    uint32_t drm_format;
};

typedef uintptr_t (*wlbuf_pool_key_provider)(struct mp_image *src);
typedef bool (*wlbuf_pool_dmabuf_importer)(struct vo *vo, struct mp_image *src, struct wlbuf_pool_entry* entry);

struct wlbuf_pool {
    struct vo *vo;
    struct wlbuf_pool_entry **entries;
    int num_entries;
    int num_allocated;
    wlbuf_pool_key_provider key_provider;
    wlbuf_pool_dmabuf_importer dmabuf_importer;
};
static void wlbuf_pool_free_entry(struct wlbuf_pool_entry *entry)
{
    if (!entry)
        return;
    if (entry->buffer)
        wl_buffer_destroy(entry->buffer);
    if (entry->params)
        zwp_linux_buffer_params_v1_destroy(entry->params);
    talloc_free(entry);
}

static void wlbuf_pool_clean(struct wlbuf_pool *pool)
{
    if (!pool)
        return;

    for (int i = 0; i < pool->num_entries; ++i)
        wlbuf_pool_free_entry(pool->entries[i]);
    pool->num_entries = 0;
}

static void wlbuf_pool_free(struct wlbuf_pool *pool)
{
    if (!pool)
        return;

    wlbuf_pool_clean(pool);
    talloc_free(pool);
}

static struct wlbuf_pool *wlbuf_pool_alloc(struct vo *vo, wlbuf_pool_key_provider key_provider, wlbuf_pool_dmabuf_importer dmabuf_importer)
{
    struct wlbuf_pool *pool = talloc(NULL, struct wlbuf_pool);
    memset(pool, 0, sizeof(struct wlbuf_pool));
    pool->num_allocated = WLBUF_POOL_NUM_ALLOCATED_INIT;
    pool->entries = talloc_array(pool, struct wlbuf_pool_entry *, pool->num_allocated);
    memset(pool->entries, 0, pool->num_allocated * sizeof(struct wlbuf_pool_entry *));
    pool->vo = vo;
    pool->key_provider = key_provider;
    pool->dmabuf_importer = dmabuf_importer;

    return pool;
}

static struct wlbuf_pool_entry *wlbuf_pool_alloc_entry(struct vo *vo, struct wlbuf_pool *pool,
                                                 struct mp_image *src)
{
    uintptr_t key;
    struct wlbuf_pool_entry *entry;
    struct vo_wayland_state *wl = vo->wl;

    if (!pool || !src)
        return NULL;

    /* 1. try to find existing entry in pool */
    key = pool->key_provider(src);
    for (int i = 0; i < pool->num_entries; ++i) {
        struct wlbuf_pool_entry *item = pool->entries[i];
        if (item->key == key)
            return pool->entries[i];
    }

    /* 2. otherwise allocate new entry and buffer */
    entry = talloc(NULL, struct wlbuf_pool_entry);
    memset(entry, 0, sizeof(struct wlbuf_pool_entry));
    entry->params = zwp_linux_dmabuf_v1_create_params(wl->dmabuf);
    entry->key = pool->key_provider(src);
    if (!pool->dmabuf_importer(pool->vo, src,entry)) {
        wlbuf_pool_free_entry(entry);
        return NULL;
    }
    entry->buffer = zwp_linux_buffer_params_v1_create_immed(entry->params,
                                                            src->params.w,
                                                            src->params.h,
                                                            entry->drm_format, 0);

    /* 3. add new entry to pool */
    if (pool->num_entries == pool->num_allocated) {
        int current_num_allocated = pool->num_allocated;
        pool->num_allocated *= 2;
        pool->entries = talloc_realloc(pool, pool->entries, struct wlbuf_pool_entry *,
                                       pool->num_allocated);
        for (int i = current_num_allocated; i < pool->num_allocated; ++i)
            pool->entries[i] = NULL;
    }
    pool->entries[pool->num_entries++] = entry;

    return entry;
}
/***********************************************************/


struct priv {
    struct vo *vo;
    struct mp_rect src;
    struct mp_rect dst;
    struct mp_osd_res osd;
    struct mp_log *log;
    struct wl_shm_pool *solid_buffer_pool;
    struct wl_buffer *solid_buffer;
    struct wlbuf_pool *wlbuf_pool;

    /* va-api-specific fields */
    VADisplay display;
    struct mp_vaapi_ctx *mpvaapi;
};

static uintptr_t vaapi_key_provider(struct mp_image *src){
    return va_surface_id(src);
}

/* va-api dmabuf importer */
static bool vaapi_dmabuf_importer(struct vo *vo, struct mp_image *src,
                                    struct wlbuf_pool_entry* entry)
{
    struct priv *p = vo->priv;
    VAStatus status;
    VADRMPRIMESurfaceDescriptor desc;
    bool dmabuf_imported = false;
    /* composed has single layer */
    int layer_no = 0;

    status = vaExportSurfaceHandle(p->display, entry->key,
                                 VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                 VA_EXPORT_SURFACE_COMPOSED_LAYERS |
                                 VA_EXPORT_SURFACE_READ_ONLY,
                                 &desc);
    if (status == VA_STATUS_ERROR_INVALID_SURFACE) {
        MP_VERBOSE(vo, "VA export to composed layers not supported.\n");
    } else if (!vo_wayland_supported_format(vo, desc.layers[layer_no].drm_format)) {
        MP_VERBOSE(vo, "%s is not supported.\n",
                   mp_tag_str(desc.layers[layer_no].drm_format));
    } else if (CHECK_VA_STATUS(vo, "vaExportSurfaceHandle()")) {
        entry->drm_format = desc.layers[layer_no].drm_format;
        for (int plane_no = 0; plane_no < desc.layers[layer_no].num_planes; ++plane_no) {
            int object = desc.layers[layer_no].object_index[plane_no];
            uint64_t modifier = desc.objects[object].drm_format_modifier;
            zwp_linux_buffer_params_v1_add(entry->params,
                                           desc.objects[object].fd, plane_no,
                                           desc.layers[layer_no].offset[plane_no],
                                           desc.layers[layer_no].pitch[plane_no],
                                           modifier >> 32,
                                           modifier & 0xffffffff);
        }
        dmabuf_imported = true;
    }

    /* clean up descriptor */
    for (int i = 0; i < desc.num_objects; i++) {
        close(desc.objects[i].fd);
        desc.objects[i].fd = 0;
    }

    return dmabuf_imported;
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;

    wlbuf_pool_free(p->wlbuf_pool);

    if (p->solid_buffer_pool)
        wl_shm_pool_destroy(p->solid_buffer_pool);
    if (p->solid_buffer)
        wl_buffer_destroy(p->solid_buffer);

    vo_wayland_uninit(vo);

    if (vo->hwdec_devs) {
        hwdec_devices_remove(vo->hwdec_devs, &p->mpvaapi->hwctx);
        hwdec_devices_destroy(vo->hwdec_devs);
    }
    if (p->mpvaapi)
        va_destroy(p->mpvaapi);
}

static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;

    p->vo = vo;
    p->log = vo->log;
    if (!vo_wayland_init(vo))
        return VO_ERROR;

    p->display = vaGetDisplayWl(vo->wl->display);
    if (!p->display) {
        MP_ERR(vo, "Unable to get the VA Display.\n");
        return VO_ERROR;
    }

    p->mpvaapi = va_initialize(p->display, p->log, false);
    if (!p->mpvaapi) {
        vaTerminate(p->display);
        p->display = NULL;
        goto fail;
    }

    vo->hwdec_devs = hwdec_devices_create();
    hwdec_devices_add(vo->hwdec_devs, &p->mpvaapi->hwctx);
    p->wlbuf_pool = wlbuf_pool_alloc(vo, vaapi_key_provider, vaapi_dmabuf_importer);

    return 0;

fail:
    uninit(vo);

    return VO_ERROR;
}

static int query_format(struct vo *vo, int format)
{
    return format == IMGFMT_VAAPI;
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *p = vo->priv;
    struct vo_wayland_state *wl = vo->wl;

    if (!p->solid_buffer_pool) {
        int width = 1;
        int height = 1;
        int stride = MP_ALIGN_UP(width * 4, 16);
        int fd = vo_wayland_allocate_memfd(vo, stride);
        if (fd < 0)
            return VO_ERROR;
        p->solid_buffer_pool = wl_shm_create_pool(wl->shm, fd, height * stride);
        if (!p->solid_buffer_pool)
            return VO_ERROR;
        p->solid_buffer = wl_shm_pool_create_buffer(p->solid_buffer_pool, 0,
                                                    width, height, stride,
                                                    WL_SHM_FORMAT_XRGB8888);
        if (!p->solid_buffer)
            return VO_ERROR;
        wl_surface_attach(wl->surface, p->solid_buffer, 0, 0);
    }

    if (!vo_wayland_reconfig(vo))
        return VO_ERROR;

    return 0;
}

static int resize(struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wl;
    struct priv *p = vo->priv;

    vo_wayland_set_opaque_region(wl, 0);
    const int width = wl->scaling * mp_rect_w(wl->geometry);
    const int height = wl->scaling * mp_rect_h(wl->geometry);
    vo->dwidth = width;
    vo->dheight = height;
    vo_get_src_dst_rects(vo, &p->src, &p->dst, &p->osd);

    wp_viewport_set_destination(wl->viewport,
                                (p->dst.x0 << 1) + mp_rect_w(p->dst),
                                (p->dst.y0 << 1) + mp_rect_h(p->dst));
    wp_viewport_set_destination(wl->video_viewport, mp_rect_w(p->dst),
                                mp_rect_h(p->dst));
    wl_subsurface_set_position(wl->video_subsurface, p->dst.x0, p->dst.y0);

    vo->want_redraw = true;

    return VO_TRUE;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;
    int events = 0;
    int ret;

    switch (request) {
    /* need to clean buffer pool after seek to avoid judder */
    case VOCTRL_RESET:
        wlbuf_pool_clean(p->wlbuf_pool);
        break;
    default:
        break;
    }
    ret = vo_wayland_control(vo, &events, request, data);
    if (events & VO_EVENT_RESIZE)
        ret = resize(vo);
    if (events & VO_EVENT_EXPOSE)
        vo->want_redraw = true;
    vo_event(vo, events);

    return ret;
}

static void draw_frame(struct vo *vo, struct vo_frame *frame)
{
    struct priv *p = vo->priv;
    struct vo_wayland_state *wl = vo->wl;

    if (!vo_wayland_check_visible(vo))
        return;

    struct wlbuf_pool_entry *entry = wlbuf_pool_alloc_entry(vo, p->wlbuf_pool,
                                                      frame->current);
    if (!entry)
        return;

    wl_surface_attach(wl->video_surface, entry->buffer, 0, 0);
    wl_surface_damage_buffer(wl->video_surface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(wl->video_surface);
    wl_surface_commit(wl->surface);

    if (!wl->opts->disable_vsync)
        vo_wayland_wait_frame(wl);
    if (wl->presentation)
        present_sync_swap(wl->present);
}
static void flip_page(struct vo *vo)
{
    /* no-op */
}

static void get_vsync(struct vo *vo, struct vo_vsync_info *info)
{
    struct vo_wayland_state *wl = vo->wl;
    if (wl->presentation)
        present_sync_get_info(wl->present, info);
}

const struct vo_driver video_out_dmabuf_wayland = {
    .description = "Wayland dmabuf video output",
    .name = "dmabuf-wayland",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_frame = draw_frame,
    .flip_page = flip_page,
    .get_vsync = get_vsync,
    .wakeup = vo_wayland_wakeup,
    .wait_events = vo_wayland_wait_events,
    .uninit = uninit,
    .priv_size = sizeof(struct priv)
};
