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

#include "config.h"

#if HAVE_VAAPI
#include <va/va_drmcommon.h>
#endif
#if HAVE_DRM
#include <libavutil/hwcontext_drm.h>
#endif

#include "mpv_talloc.h"
#include "common/global.h"
#include "vo.h"
#include "video/mp_image.h"

#include "gpu/hwdec.h"
#include "gpu/video.h"

#if HAVE_VAAPI
#include "video/vaapi.h"
#endif
#include "present_sync.h"
#include "wayland_common.h"
#include "wlbuf_pool.h"

// Generated from wayland-protocols
#include "generated/wayland/linux-dmabuf-unstable-v1.h"
#include "generated/wayland/viewporter.h"

#if HAVE_WAYLAND_PROTOCOLS_1_27
#include "generated/wayland/single-pixel-buffer-v1.h"
#endif

struct priv {
    struct mp_log *log;
    struct ra_ctx *ctx;
    struct mpv_global *global;
    struct ra_hwdec_ctx hwdec_ctx;
    int events;

    struct wl_shm_pool *solid_buffer_pool;
    struct wl_buffer *solid_buffer;
    struct wlbuf_pool *wlbuf_pool;
    bool want_reset;
    bool want_resize;
    struct mp_rect src;
    bool resized;

#if HAVE_VAAPI
    VADisplay display;
#endif
};

#if HAVE_VAAPI
static uintptr_t vaapi_key_provider(struct mp_image *src)
{
    return va_surface_id(src);
}

static void close_file_descriptors(VADRMPRIMESurfaceDescriptor desc)
{
    for (int i = 0; i < desc.num_objects; i++)
        close(desc.objects[i].fd);
}

/* va-api dmabuf importer */
static bool vaapi_dmabuf_importer(struct mp_image *src, struct wlbuf_pool_entry* entry,
                                  struct zwp_linux_buffer_params_v1 *params)
{
    struct priv *p = entry->vo->priv;
    VADRMPRIMESurfaceDescriptor desc = { 0 };
    /* composed has single layer */
    int layer_no = 0;
    VAStatus status = vaExportSurfaceHandle(p->display, entry->key, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                            VA_EXPORT_SURFACE_COMPOSED_LAYERS | VA_EXPORT_SURFACE_READ_ONLY, &desc);

    if (!CHECK_VA_STATUS(entry->vo, "vaExportSurfaceHandle()")) {
        /* invalid surface warning => composed layers not supported */
        if (status == VA_STATUS_ERROR_INVALID_SURFACE)
            MP_VERBOSE(entry->vo, "vaExportSurfaceHandle: composed layers not supported.\n");
        close_file_descriptors(desc);

        return false;
    }
    bool success = false;
    uint32_t drm_format = desc.layers[layer_no].drm_format;
    if (!vo_wayland_supported_format(entry->vo, drm_format, desc.objects[0].drm_format_modifier)) {
        MP_VERBOSE(entry->vo, "%s(%016lx) is not supported.\n",
                   mp_tag_str(drm_format), desc.objects[0].drm_format_modifier);
        goto done;
    }
    entry->drm_format = drm_format;
    for (int plane_no = 0; plane_no < desc.layers[layer_no].num_planes; ++plane_no) {
        int object = desc.layers[layer_no].object_index[plane_no];
        uint64_t modifier = desc.objects[object].drm_format_modifier;
        zwp_linux_buffer_params_v1_add(params, desc.objects[object].fd, plane_no, desc.layers[layer_no].offset[plane_no],
                                       desc.layers[layer_no].pitch[plane_no], modifier >> 32, modifier & 0xffffffff);
    }
    success = true;

done:
    close_file_descriptors(desc);

    return success;
}
#endif

#if HAVE_DRM

static uintptr_t drmprime_key_provider(struct mp_image *src)
{
    struct AVDRMFrameDescriptor *desc = (AVDRMFrameDescriptor *)src->planes[0];

    AVDRMObjectDescriptor object = desc->objects[0];
    return (uintptr_t)object.fd;
}

static bool drmprime_dmabuf_importer(struct mp_image *src, struct wlbuf_pool_entry *entry,
                                     struct zwp_linux_buffer_params_v1 *params)
{
    int layer_no, plane_no;
    const AVDRMFrameDescriptor *avdesc = (AVDRMFrameDescriptor *)src->planes[0];

    for (layer_no = 0; layer_no < avdesc->nb_layers; layer_no++) {
        AVDRMLayerDescriptor layer = avdesc->layers[layer_no];

        entry->drm_format = layer.format;
        for (plane_no = 0; plane_no < layer.nb_planes; ++plane_no) {
            AVDRMPlaneDescriptor plane = layer.planes[plane_no];
            int object_index = plane.object_index;
            AVDRMObjectDescriptor object = avdesc->objects[object_index];
            uint64_t modifier = object.format_modifier;

            zwp_linux_buffer_params_v1_add(params, object.fd, plane_no, plane.offset,
                                           plane.pitch, modifier >> 32, modifier & 0xffffffff);
        }
    }

    return true;
}
#endif

static void set_viewport_source(struct vo *vo, struct mp_rect src) {
    struct priv *p = vo->priv;
    struct vo_wayland_state *wl = vo->wl;

    if (wl->video_viewport && !mp_rect_equals(&p->src, &src)) {
        // 1. update viewport source
        wp_viewport_set_source(wl->video_viewport, src.x0 << 8,
                               src.y0 << 8, mp_rect_w(src) << 8,
                               mp_rect_h(src) << 8);
        // 2. reset buffer pool
        p->want_reset = true;

        // 3. update to new src dimensions
        p->src = src;
    }
}

static void resize(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct vo_wayland_state *wl = vo->wl;
    struct mp_rect src;
    struct mp_rect dst;
    struct mp_osd_res osd;
    struct mp_vo_opts *vo_opts = wl->vo_opts;
    const int width = mp_rect_w(wl->geometry);
    const int height = mp_rect_h(wl->geometry);
    
    vo_wayland_set_opaque_region(wl, 0);
    vo->dwidth = width;
    vo->dheight = height;

    // top level viewport is calculated with pan set to zero
    vo->opts->pan_x = 0;
    vo->opts->pan_y = 0;
    vo_get_src_dst_rects(vo, &src, &dst, &osd);
    if (wl->viewport)
        wp_viewport_set_destination(wl->viewport, 2 * dst.x0 + mp_rect_w(dst), 2 * dst.y0 + mp_rect_h(dst));

    //now we restore pan for video viewport caculation
    vo->opts->pan_x = vo_opts->pan_x;
    vo->opts->pan_y = vo_opts->pan_y;
    vo_get_src_dst_rects(vo, &src, &dst, &osd);
    if (wl->video_viewport)
        wp_viewport_set_destination(wl->video_viewport, mp_rect_w(dst), mp_rect_h(dst));
    wl_subsurface_set_position(wl->video_subsurface, dst.x0, dst.y0);
    set_viewport_source(vo, src);

    vo->want_redraw = true;
    p->resized = true;
    p->want_reset = true;
    p->want_resize = false;
}

static void draw_frame(struct vo *vo, struct vo_frame *frame)
{
    struct priv *p = vo->priv;
    struct vo_wayland_state *wl = vo->wl;
    struct wlbuf_pool_entry *entry;
    
    if (!vo_wayland_check_visible(vo))
        return;

    /* lazy initialization of buffer pool */
    if (!p->wlbuf_pool) {
#if HAVE_VAAPI
        p->display = (VADisplay)ra_get_native_resource(p->ctx->ra, "VADisplay");
        if (p->display)
            p->wlbuf_pool = wlbuf_pool_alloc(vo, wl, vaapi_key_provider, vaapi_dmabuf_importer);
#endif
#if HAVE_DRM
        if (!p->wlbuf_pool)
            p->wlbuf_pool = wlbuf_pool_alloc(vo, wl, drmprime_key_provider, drmprime_dmabuf_importer);
#endif
    }
    entry = wlbuf_pool_get_entry(p->wlbuf_pool, frame->current);
    if (!entry)
        return;

    // ensure the pool is reset after hwdec seek,
    // to avoid stutter artifact
    if (p->want_reset)
        wlbuf_pool_clean(p->wlbuf_pool);
    if (p->want_resize)
        resize(vo);

    MP_VERBOSE(entry->vo, "Schedule buffer pool entry : %lu\n",entry->key );
    wl_surface_attach(wl->video_surface, entry->buffer, 0, 0);
    wl_surface_damage_buffer(wl->video_surface, 0, 0, INT32_MAX, INT32_MAX);
}

static void flip_page(struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wl;
    struct priv *p = vo->priv;

    wl_surface_commit(wl->video_surface);
    wl_surface_commit(wl->surface);
    if (!wl->opts->disable_vsync)
        vo_wayland_wait_frame(wl);
    if (wl->use_present)
       present_sync_swap(wl->present);
    if (p->want_reset) {
        wlbuf_pool_clean(p->wlbuf_pool);
        p->want_reset = false;
    }
}

static void get_vsync(struct vo *vo, struct vo_vsync_info *info)
{
    struct vo_wayland_state *wl = vo->wl;

    if (wl->use_present)
        present_sync_get_info(wl->present, info);
}

static bool is_supported_fmt(int fmt)
{
    return  (fmt == IMGFMT_DRMPRIME || fmt == IMGFMT_VAAPI);
}

static int query_format(struct vo *vo, int format)
{
    return  is_supported_fmt(format);
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    if (!vo_wayland_reconfig(vo))
        return VO_ERROR;

    return 0;
}

static void call_request_hwdec_api(void *ctx, struct hwdec_imgfmt_request *params)
{
    // Roundabout way to run hwdec loading on the VO thread.
    // Redirects to request_hwdec_api().
    vo_control(ctx, VOCTRL_LOAD_HWDEC_API, params);
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;
    int events = 0;
    int ret;

    switch (request) {
    case VOCTRL_SET_PANSCAN:
        p->want_resize = true;
        return VO_TRUE;
    case VOCTRL_LOAD_HWDEC_API:
        assert(p->hwdec_ctx.ra);
        struct hwdec_imgfmt_request* req = (struct hwdec_imgfmt_request*)data;
        if (!is_supported_fmt(req->imgfmt))
            return 0;
        ra_hwdec_ctx_load_fmt(&p->hwdec_ctx, vo->hwdec_devs, req);
        return (p->hwdec_ctx.num_hwdecs > 0);
        break;
	case VOCTRL_RESET:
        p->want_reset = true;
	    return VO_TRUE;
	    break;
    }

    ret = vo_wayland_control(vo, &events, request, data);
    if (events & VO_EVENT_RESIZE){
        p->want_resize = true;
        if (!p->resized)
            resize(vo);
    }
    if (events & VO_EVENT_EXPOSE)
        vo->want_redraw = true;
    vo_event(vo, events);

    return ret;
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;

    wlbuf_pool_free(p->wlbuf_pool);
    if (p->solid_buffer_pool)
        wl_shm_pool_destroy(p->solid_buffer_pool);
    if (p->solid_buffer)
        wl_buffer_destroy(p->solid_buffer);
    ra_hwdec_ctx_uninit(&p->hwdec_ctx);
    if (vo->hwdec_devs) {
        hwdec_devices_set_loader(vo->hwdec_devs, NULL, NULL);
        hwdec_devices_destroy(vo->hwdec_devs);
    }
    vo_wayland_uninit(vo);
    ra_ctx_destroy(&p->ctx);
}

static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;

    p->log = vo->log;
    p->global = vo->global;
    p->ctx = ra_ctx_create_by_name(vo, "wldmabuf");
    if (!p->ctx)
       goto err;
    assert(p->ctx->ra);

    if (!vo->wl->dmabuf || !vo->wl->dmabuf_feedback) {
        MP_FATAL(vo->wl, "Compositor doesn't support the %s (ver. 4) protocol!\n",
                 zwp_linux_dmabuf_v1_interface.name);
        goto err;
    }

    if (!vo->wl->shm) {
        MP_FATAL(vo->wl, "Compositor doesn't support the %s protocol!\n",
                 wl_shm_interface.name);
        goto err;
    }

    if (!vo->wl->video_subsurface) {
        MP_FATAL(vo->wl, "Compositor doesn't support the %s protocol!\n",
                 wl_subcompositor_interface.name);
        goto err;
    }

    if (!vo->wl->viewport) {
        MP_FATAL(vo->wl, "Compositor doesn't support the %s protocol!\n",
                 wp_viewporter_interface.name);
        goto err;
    }

    if (vo->wl->single_pixel_manager) {
#if HAVE_WAYLAND_PROTOCOLS_1_27
        p->solid_buffer = wp_single_pixel_buffer_manager_v1_create_u32_rgba_buffer(
            vo->wl->single_pixel_manager, 0, 0, 0, UINT32_MAX); /* R, G, B, A */
#endif
    } else {
        int width = 1;
        int height = 1;
        int stride = MP_ALIGN_UP(width * 4, 16);
        int fd = vo_wayland_allocate_memfd(vo, stride);
        if (fd < 0)
            goto err;
        p->solid_buffer_pool = wl_shm_create_pool(vo->wl->shm, fd, height * stride);
        close(fd);
        if (!p->solid_buffer_pool)
            goto err;
        p->solid_buffer = wl_shm_pool_create_buffer(
            p->solid_buffer_pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
    }
    if (!p->solid_buffer)
        goto err;

    wl_surface_attach(vo->wl->surface, p->solid_buffer, 0, 0);

    vo->hwdec_devs = hwdec_devices_create();
    hwdec_devices_set_loader(vo->hwdec_devs, call_request_hwdec_api, vo);
    assert(!p->hwdec_ctx.ra);
    p->hwdec_ctx = (struct ra_hwdec_ctx) {
        .log = p->log,
        .global = p->global,
        .ra = p->ctx->ra,
    };
    ra_hwdec_ctx_init(&p->hwdec_ctx, vo->hwdec_devs, NULL, true);
    p->src = (struct mp_rect){0, 0, 0, 0};

    return 0;

err:
    uninit(vo);
    return -1;
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
    .priv_size = sizeof(struct priv),
};
