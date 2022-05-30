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

#include "sub/osd.h"
#include "video/vaapi.h"
#include "wayland_common.h"
#include "generated/wayland/linux-dmabuf-unstable-v1.h"
#include "generated/wayland/viewporter.h"

#define VA_POOL_NUM_ALLOCATED_INIT 30

struct va_pool_entry {
    /* key */
    VASurfaceID surface;

    VADRMPRIMESurfaceDescriptor desc;
    struct wl_buffer *buffer;
    struct zwp_linux_buffer_params_v1 *params;
    uint drm_format;
};
struct va_pool {
    struct vo *vo;
    struct va_pool_entry **entries;
    uint num_entries;
    uint num_allocated;
};

struct priv {
    struct vo *vo;
    struct mp_rect src;
    struct mp_rect dst;
    struct mp_osd_res osd;
    struct mp_log *log;

    VADisplay display;
    struct mp_vaapi_ctx *mpvaapi;
    struct va_image_formats *image_formats;
    struct wl_shm_pool *solid_buffer_pool;
    struct wl_buffer *solid_buffer;
    struct va_pool *va_pool;
};

static void va_close_surface_descriptor(VADRMPRIMESurfaceDescriptor desc) {
    for (uint i = 0; i < desc.num_objects; i++) {
        close(desc.objects[i].fd);
        desc.objects[i].fd = 0;
    }
}

static void va_free_entry(struct va_pool_entry *entry) {
    if (!entry)
        return;
    va_close_surface_descriptor(entry->desc);
    if (entry->buffer)
        wl_buffer_destroy(entry->buffer);
    if (entry->params)
        zwp_linux_buffer_params_v1_destroy(entry->params);
    talloc_free(entry);
}

static VAStatus va_export_surface_handle(VADisplay display, VASurfaceID surface,
        VADRMPRIMESurfaceDescriptor *desc) {
    return vaExportSurfaceHandle(display, surface,
            VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
            VA_EXPORT_SURFACE_COMPOSED_LAYERS | VA_EXPORT_SURFACE_READ_ONLY,
            desc);
}

static struct va_pool_entry* va_alloc_entry(struct vo *vo, struct mp_image *src) {
    struct priv *p = vo->priv;
    struct vo_wayland_state *wl = vo->wl;
    VAStatus status;
    struct va_pool_entry *entry = talloc(NULL, struct va_pool_entry);
    memset(entry, 0, sizeof(struct va_pool_entry));

    /* extract dmabuf surface descriptor */
    entry->surface = va_surface_id(src);
    status = va_export_surface_handle(p->display, entry->surface, &entry->desc);
    if (status == VA_STATUS_ERROR_INVALID_SURFACE) {
        MP_VERBOSE(vo, "VA export to composed layers not supported.\n");
        va_free_entry(entry);
        return NULL;
    } else if (!vo_wayland_supported_format(vo,
            entry->desc.layers[0].drm_format)) {
        MP_VERBOSE(vo, "%s is not supported.\n",
                   mp_tag_str(entry->desc.layers[0].drm_format));
        va_free_entry(entry);
        return NULL;
    } else if (!CHECK_VA_STATUS(vo, "vaExportSurfaceHandle()")) {
        va_free_entry(entry);
        return NULL;
    } else {
        uint i, j, plane = 0;

        entry->params = zwp_linux_dmabuf_v1_create_params(wl->dmabuf);
        for (i = 0; i < entry->desc.num_layers; i++) {
            entry->drm_format = entry->desc.layers[i].drm_format;
            for (j = 0; j < entry->desc.layers[i].num_planes; ++j) {
                int object = entry->desc.layers[i].object_index[j];
                uint64_t modifier =
                        entry->desc.objects[object].drm_format_modifier;
                zwp_linux_buffer_params_v1_add(entry->params,
                        entry->desc.objects[object].fd, plane++,
                        entry->desc.layers[i].offset[j],
                        entry->desc.layers[i].pitch[j], modifier >> 32,
                        modifier & 0xffffffff);
            }
        }
    }
    entry->buffer = zwp_linux_buffer_params_v1_create_immed(entry->params,
            src->params.w, src->params.h, entry->drm_format, 0);

    return entry;
}
static void va_pool_clean(struct va_pool *pool) {
    if (!pool)
        return;

    for (uint i = 0; i < pool->num_entries; ++i)
        va_free_entry(pool->entries[i]);
    pool->num_entries = 0;
}

static void va_pool_free(struct va_pool *pool) {
    if (!pool)
        return;

    va_pool_clean(pool);
    talloc_free(pool->entries);
    talloc_free(pool);
}
static struct va_pool* va_pool_alloc(struct vo *vo) {
    struct va_pool *pool = talloc(NULL, struct va_pool);
    memset(pool, 0, sizeof(struct va_pool));
    pool->num_allocated = VA_POOL_NUM_ALLOCATED_INIT;
    pool->entries = talloc_array(NULL,struct va_pool_entry*, pool->num_allocated);
    memset(pool->entries,0,pool->num_allocated * sizeof(struct va_pool_entry*));
    pool->vo = vo;

    return pool;
}
static struct va_pool_entry* va_pool_alloc_entry(struct vo *vo,
        struct va_pool *pool, struct mp_image *src) {
    VASurfaceID surface;

    if (!pool)
        return NULL;

    surface = va_surface_id(src);
    for (uint i = 0; i < pool->num_entries; ++i) {
        struct va_pool_entry *item = pool->entries[i];
        if (item->surface == surface)
            return pool->entries[i];
    }

    struct va_pool_entry *entry = va_alloc_entry(pool->vo, src);
    if (!entry)
        return NULL;

    if (pool->num_entries == pool->num_allocated) {
        uint current_num_allocated = pool->num_allocated;
        pool->num_allocated *= 2;
        pool->entries = talloc_realloc(NULL,pool->entries,struct va_pool_entry*, pool->num_allocated);
        for (uint i = current_num_allocated; i < pool->num_allocated; ++i)
            pool->entries[i] = NULL;
    }
    pool->entries[pool->num_entries++] = entry;

    return entry;
}

struct va_image_formats {
    VAImageFormat *entries;
    int num;
};

static void va_get_formats(struct priv *ctx) {
    struct va_image_formats *formats = talloc_ptrtype(ctx, formats);

    formats->num = vaMaxNumImageFormats(ctx->display);
    formats->entries = talloc_array(formats, VAImageFormat, formats->num);
    VAStatus status = vaQueryImageFormats(ctx->display, formats->entries,
            &formats->num);
    if (!CHECK_VA_STATUS(ctx, "vaQueryImageFormats()"))
        return;
    MP_VERBOSE(ctx, "%d image formats available:\n", formats->num);
    for (int i = 0; i < formats->num; i++)
        MP_VERBOSE(ctx, "  %s\n", mp_tag_str(formats->entries[i].fourcc));
    ctx->image_formats = formats;
}

static void uninit(struct vo *vo) {
    struct priv *p = vo->priv;

    va_pool_free(p->va_pool);
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
static int allocate_memfd(size_t size) {
    int fd = memfd_create("mpv", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0)
        return VO_ERROR;

    fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_SEAL);

    if (posix_fallocate(fd, 0, size) == 0)
        return fd;

    close(fd);
    return VO_ERROR;
}

static int preinit(struct vo *vo) {
    struct priv *p = vo->priv;

    p->vo = vo;
    p->log = vo->log;
    if (!vo_wayland_init(vo))
        return VO_ERROR;
    p->display = vaGetDisplayWl(vo->wl->display);
    if (!p->display)
        return VO_ERROR;
    p->mpvaapi = va_initialize(p->display, p->log, false);
    if (!p->mpvaapi) {
        vaTerminate(p->display);
        p->display = NULL;
        goto fail;
    }
    va_get_formats(p);
    if (!p->image_formats)
        goto fail;

    vo->hwdec_devs = hwdec_devices_create();
    hwdec_devices_add(vo->hwdec_devs, &p->mpvaapi->hwctx);
    p->va_pool = va_pool_alloc(vo);

    return 0;

fail:
    uninit(vo);

    return VO_ERROR;
}

static int query_format(struct vo *vo, int format) {
    return format == IMGFMT_VAAPI;
}

static int reconfig(struct vo *vo, struct mp_image_params *params) {
    struct priv *p = vo->priv;
    struct vo_wayland_state *wl = vo->wl;

    if (!p->solid_buffer_pool) {
        int width = 1;
        int height = 1;
        int stride = MP_ALIGN_UP(width * 4, 16);
        int fd = allocate_memfd(stride);
        if (fd < 0)
            return VO_ERROR;
        p->solid_buffer_pool = wl_shm_create_pool(wl->shm, fd, height * stride);
        if (!p->solid_buffer_pool)
            return VO_ERROR;
        p->solid_buffer = wl_shm_pool_create_buffer(p->solid_buffer_pool, 0,
                width, height, stride, WL_SHM_FORMAT_XRGB8888);
        if (!p->solid_buffer)
            return VO_ERROR;
    }
    if (!vo_wayland_reconfig(vo))
        return VO_ERROR;

    return 0;
}

static int resize(struct vo *vo) {
    struct vo_wayland_state *wl = vo->wl;
    struct priv *p = vo->priv;

    wl_subsurface_set_sync(wl->video_subsurface);
    vo_wayland_set_opaque_region(wl, 0);
    const int32_t width = wl->scaling * mp_rect_w(wl->geometry);
    const int32_t height = wl->scaling * mp_rect_h(wl->geometry);
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
    wl_subsurface_set_desync(wl->video_subsurface);

    return VO_TRUE;
}

static int control(struct vo *vo, uint32_t request, void *data) {
    struct priv *p = vo->priv;
    int events = 0;
    int ret;

    switch (request) {
    /* need to clean pool after seek to avoid artifacts */
    case VOCTRL_RESET:
        va_pool_clean(p->va_pool);
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

static void draw_frame(struct vo *vo, struct vo_frame *frame) {
    struct priv *p = vo->priv;
    struct vo_wayland_state *wl = vo->wl;

    if (!vo_wayland_check_visible(vo))
        return;

    struct va_pool_entry *entry = va_pool_alloc_entry(vo, p->va_pool,
            frame->current);
    if (!entry)
        return;

    wl_surface_attach(wl->surface, p->solid_buffer, 0, 0);
    wl_surface_attach(wl->video_surface, entry->buffer, 0, 0);
    wl_surface_damage_buffer(wl->video_surface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(wl->video_surface);
    wl_surface_commit(wl->surface);

    if (!wl->opts->disable_vsync)
        vo_wayland_wait_frame(wl);
    if (wl->presentation)
        vo_wayland_sync_swap(wl);
}
static void flip_page(struct vo *vo) {
    /* no-op */
}

static void get_vsync(struct vo *vo, struct vo_vsync_info *info) {
    struct vo_wayland_state *wl = vo->wl;
    if (wl->presentation) {
        info->vsync_duration = wl->vsync_duration;
        info->skipped_vsyncs = wl->last_skipped_vsyncs;
        info->last_queue_display_time = wl->last_queue_display_time;
    }
}

const struct vo_driver video_out_vaapi_wayland = { .description =
        "VA API with Wayland video output",
        .name = "vaapi-wayland",
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
