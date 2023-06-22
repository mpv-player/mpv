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

#include "common/global.h"
#include "gpu/hwdec.h"
#include "gpu/video.h"
#include "mpv_talloc.h"
#include "present_sync.h"
#include "video/mp_image.h"
#include "vo.h"
#include "wayland_common.h"
#include "wldmabuf/ra_wldmabuf.h"

#if HAVE_VAAPI
#include "video/vaapi.h"
#endif

// Generated from wayland-protocols
#include "generated/wayland/linux-dmabuf-unstable-v1.h"
#include "generated/wayland/viewporter.h"

#if HAVE_WAYLAND_PROTOCOLS_1_27
#include "generated/wayland/single-pixel-buffer-v1.h"
#endif

// We need at least enough buffers to avoid a
// flickering artifact in certain formats.
#define WL_BUFFERS_WANTED 15

enum hwdec_type {
    HWDEC_NONE,
    HWDEC_VAAPI,
    HWDEC_DRMPRIME,
};

struct buffer {
    struct vo *vo;
    struct wl_buffer *buffer;
    struct wl_list link;
    struct mp_image *image;

    uint32_t drm_format;
    uintptr_t id;
};

struct priv {
    struct mp_log *log;
    struct mp_rect src;
    struct mpv_global *global;

    struct ra_ctx *ctx;
    struct ra_hwdec_ctx hwdec_ctx;

    struct wl_shm_pool *solid_buffer_pool;
    struct wl_buffer *solid_buffer;
    struct wl_list buffer_list;

    bool destroy_buffers;
    enum hwdec_type hwdec_type;
};

static void buffer_handle_release(void *data, struct wl_buffer *wl_buffer)
{
    struct buffer *buf = data;
    if (buf->image) {
        mp_image_unrefp(&buf->image);
        buf->image = NULL;
    }
}

static const struct wl_buffer_listener buffer_listener = {
    buffer_handle_release,
};

#if HAVE_VAAPI
static void close_file_descriptors(VADRMPRIMESurfaceDescriptor desc)
{
    for (int i = 0; i < desc.num_objects; i++)
        close(desc.objects[i].fd);
}
#endif

static uintptr_t vaapi_surface_id(struct mp_image *src)
{
    uintptr_t id = 0;
#if HAVE_VAAPI
    id = (uintptr_t)va_surface_id(src);
#endif
    return id;
}

static void vaapi_dmabuf_importer(struct buffer *buf, struct mp_image *src,
                                  struct zwp_linux_buffer_params_v1 *params)
{
#if HAVE_VAAPI
    struct vo *vo = buf->vo;
    struct priv *p = vo->priv;
    VADRMPRIMESurfaceDescriptor desc = {0};
    VADisplay display = ra_get_native_resource(p->ctx->ra, "VADisplay");

    /* composed has single layer */
    int layer_no = 0;
    buf->id = vaapi_surface_id(src);
    VAStatus status = vaExportSurfaceHandle(display, buf->id, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                            VA_EXPORT_SURFACE_COMPOSED_LAYERS | VA_EXPORT_SURFACE_READ_ONLY, &desc);

    if (!CHECK_VA_STATUS(vo, "vaExportSurfaceHandle()")) {
        /* invalid surface warning => composed layers not supported */
        if (status == VA_STATUS_ERROR_INVALID_SURFACE)
            MP_VERBOSE(vo, "vaExportSurfaceHandle: composed layers not supported.\n");
        goto done;
    }
    buf->drm_format = desc.layers[layer_no].drm_format;
    if (!ra_compatible_format(p->ctx->ra, buf->drm_format, desc.objects[0].drm_format_modifier)) {
        MP_VERBOSE(vo, "%s(%016lx) is not supported.\n",
                   mp_tag_str(buf->drm_format), desc.objects[0].drm_format_modifier);
        buf->drm_format = 0;
        goto done;
    }
    for (int plane_no = 0; plane_no < desc.layers[layer_no].num_planes; ++plane_no) {
        int object = desc.layers[layer_no].object_index[plane_no];
        uint64_t modifier = desc.objects[object].drm_format_modifier;
        zwp_linux_buffer_params_v1_add(params, desc.objects[object].fd, plane_no, desc.layers[layer_no].offset[plane_no],
                                       desc.layers[layer_no].pitch[plane_no], modifier >> 32, modifier & 0xffffffff);
    }

done:
    close_file_descriptors(desc);
#endif
}

static uintptr_t drmprime_surface_id(struct mp_image *src)
{
    uintptr_t id = 0;
#if HAVE_DRM
    struct AVDRMFrameDescriptor *desc = (AVDRMFrameDescriptor *)src->planes[0];

    AVDRMObjectDescriptor object = desc->objects[0];
    id = (uintptr_t)object.fd;
#endif
    return id;
}

static void drmprime_dmabuf_importer(struct buffer *buf, struct mp_image *src,
                                     struct zwp_linux_buffer_params_v1 *params)
{
#if HAVE_DRM
    int layer_no, plane_no;
    int max_planes = 0;
    const AVDRMFrameDescriptor *desc = (AVDRMFrameDescriptor *)src->planes[0];
    if (!desc)
        return;

    buf->id = drmprime_surface_id(src);
    for (layer_no = 0; layer_no < desc->nb_layers; layer_no++) {
        AVDRMLayerDescriptor layer = desc->layers[layer_no];

        buf->drm_format = layer.format;
        max_planes = MPMAX(max_planes, layer.nb_planes);
        for (plane_no = 0; plane_no < layer.nb_planes; ++plane_no) {
            AVDRMPlaneDescriptor plane = layer.planes[plane_no];
            int object_index = plane.object_index;
            AVDRMObjectDescriptor object = desc->objects[object_index];
            uint64_t modifier = object.format_modifier;

            zwp_linux_buffer_params_v1_add(params, object.fd, plane_no, plane.offset,
                                           plane.pitch, modifier >> 32, modifier & 0xffffffff);
        }
    }
#endif
}

static intptr_t surface_id(struct vo *vo, struct mp_image *src)
{
    struct priv *p = vo->priv;
    switch(p->hwdec_type) {
    case HWDEC_VAAPI:
        return vaapi_surface_id(src);
    case HWDEC_DRMPRIME:
        return drmprime_surface_id(src);
    default:
        return 0;
    }
}

static struct buffer *buffer_check(struct vo *vo, struct mp_image *src)
{
    struct priv *p = vo->priv;

    /* Make more buffers if we're not at the desired amount yet. */
    if (wl_list_length(&p->buffer_list) < WL_BUFFERS_WANTED)
        goto done;

    uintptr_t id = surface_id(vo, src);
    struct buffer *buf;
    wl_list_for_each(buf, &p->buffer_list, link) {
        if (buf->id == id) {
            if (buf->image)
                mp_image_unrefp(&buf->image);
            buf->image = src;
            return buf;
        }
    }

done:
    return NULL;
}

static struct buffer *buffer_create(struct vo *vo, struct mp_image *src)
{
    struct vo_wayland_state *wl = vo->wl;
    struct priv *p = vo->priv;

    struct buffer *buf = talloc_zero(vo, struct buffer);
    buf->vo = vo;
    buf->image = src;

    struct zwp_linux_buffer_params_v1 *params = zwp_linux_dmabuf_v1_create_params(wl->dmabuf);

    switch(p->hwdec_type) {
    case HWDEC_VAAPI:
        vaapi_dmabuf_importer(buf, src, params);
        break;
    case HWDEC_DRMPRIME:
        drmprime_dmabuf_importer(buf, src, params);
        break;
    }

    if (!buf->drm_format) {
        mp_image_unrefp(&buf->image);
        talloc_free(buf);
        zwp_linux_buffer_params_v1_destroy(params);
        return NULL;
    }

    buf->buffer = zwp_linux_buffer_params_v1_create_immed(params, src->params.w, src->params.h,
                                                          buf->drm_format, 0);
    zwp_linux_buffer_params_v1_destroy(params);
    wl_buffer_add_listener(buf->buffer, &buffer_listener, buf);
    wl_list_insert(&p->buffer_list, &buf->link);
    return buf;
}

static struct buffer *buffer_get(struct vo *vo, struct mp_image *src)
{
    /* Reuse existing buffer if possible. */
    struct buffer *buf = buffer_check(vo, src);
    if (buf) {
        return buf;
    } else {
        return buffer_create(vo, src);
    }
}

static void destroy_buffers(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct buffer *buf, *tmp;
    p->destroy_buffers = false;
    wl_list_for_each_safe(buf, tmp, &p->buffer_list, link) {
        wl_list_remove(&buf->link);
        if (buf->image) {
            mp_image_unrefp(&buf->image);
            buf->image = NULL;
        }
        if (buf->buffer) {
            wl_buffer_destroy(buf->buffer);
            buf->buffer = NULL;
        }
        talloc_free(buf);
    }
}

static void set_viewport_source(struct vo *vo, struct mp_rect src) {
    struct priv *p = vo->priv;
    struct vo_wayland_state *wl = vo->wl;

    if (wl->video_viewport && !mp_rect_equals(&p->src, &src)) {
        wp_viewport_set_source(wl->video_viewport, src.x0 << 8,
                               src.y0 << 8, mp_rect_w(src) << 8,
                               mp_rect_h(src) << 8);
        p->src = src;
    }
}

static void resize(struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wl;
    struct mp_rect src;
    struct mp_rect dst;
    struct mp_osd_res osd;
    struct mp_vo_opts *vo_opts = wl->vo_opts;
    const int width = mp_rect_w(wl->geometry);
    const int height = mp_rect_h(wl->geometry);

    if (width == 0 || height == 0)
        return;
    
    vo_wayland_set_opaque_region(wl, false);
    vo->dwidth = width;
    vo->dheight = height;

    // top level viewport is calculated with pan set to zero
    vo->opts->pan_x = 0;
    vo->opts->pan_y = 0;
    vo_get_src_dst_rects(vo, &src, &dst, &osd);
    if (wl->viewport)
        wp_viewport_set_destination(wl->viewport, 2 * dst.x0 + mp_rect_w(dst), 2 * dst.y0 + mp_rect_h(dst));

    //now we restore pan for video viewport calculation
    vo->opts->pan_x = vo_opts->pan_x;
    vo->opts->pan_y = vo_opts->pan_y;
    vo_get_src_dst_rects(vo, &src, &dst, &osd);
    if (wl->video_viewport)
        wp_viewport_set_destination(wl->video_viewport, mp_rect_w(dst), mp_rect_h(dst));
    wl_subsurface_set_position(wl->video_subsurface, dst.x0, dst.y0);
    set_viewport_source(vo, src);
}

static void draw_frame(struct vo *vo, struct vo_frame *frame)
{
    struct priv *p = vo->priv;
    struct vo_wayland_state *wl = vo->wl;
    struct buffer *buf;

    if (!vo_wayland_check_visible(vo) || !frame->current)
        return;

    if (p->destroy_buffers)
        destroy_buffers(vo);

    struct mp_image *src = mp_image_new_ref(frame->current);
    buf = buffer_get(vo, src);

    if (buf && buf->image) {
        wl_surface_attach(wl->video_surface, buf->buffer, 0, 0);
        wl_surface_damage_buffer(wl->video_surface, 0, 0, buf->image->params.w,
                                 buf->image->params.h);
    }
}

static void flip_page(struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wl;

    wl_surface_commit(wl->video_surface);
    wl_surface_commit(wl->surface);

    if (!wl->opts->disable_vsync)
        vo_wayland_wait_frame(wl);

    if (wl->use_present)
       present_sync_swap(wl->present);
}

static void get_vsync(struct vo *vo, struct vo_vsync_info *info)
{
    struct vo_wayland_state *wl = vo->wl;
    if (wl->use_present)
        present_sync_get_info(wl->present, info);
}

static bool is_supported_fmt(int fmt)
{
    return (fmt == IMGFMT_DRMPRIME || fmt == IMGFMT_VAAPI);
}

static int query_format(struct vo *vo, int format)
{
    return is_supported_fmt(format);
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    if (!vo_wayland_reconfig(vo))
        return VO_ERROR;

    // Immediately destroy all buffers if params change.
    destroy_buffers(vo);
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
    case VOCTRL_LOAD_HWDEC_API:
        assert(p->hwdec_ctx.ra_ctx);
        struct hwdec_imgfmt_request* req = (struct hwdec_imgfmt_request*)data;
        if (!is_supported_fmt(req->imgfmt))
            return 0;
        ra_hwdec_ctx_load_fmt(&p->hwdec_ctx, vo->hwdec_devs, req);
        return (p->hwdec_ctx.num_hwdecs > 0);
    case VOCTRL_RESET:
        p->destroy_buffers = true;
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
        resize(vo);
        return VO_TRUE;
    }

    ret = vo_wayland_control(vo, &events, request, data);
    if (events & VO_EVENT_RESIZE)
        resize(vo);
    if (events & VO_EVENT_EXPOSE)
        vo->want_redraw = true;
    vo_event(vo, events);

    return ret;
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;

    destroy_buffers(vo);
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

    wl_list_init(&p->buffer_list);

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
    assert(!p->hwdec_ctx.ra_ctx);
    p->hwdec_ctx = (struct ra_hwdec_ctx) {
        .log = p->log,
        .global = p->global,
        .ra_ctx = p->ctx,
    };

    ra_hwdec_ctx_init(&p->hwdec_ctx, vo->hwdec_devs, NULL, true);

    for (int i = 0; i < p->hwdec_ctx.num_hwdecs; i++) {
        struct ra_hwdec *hw = p->hwdec_ctx.hwdecs[i];
        if (ra_get_native_resource(p->ctx->ra, "VADisplay")) {
            p->hwdec_type = HWDEC_VAAPI;
        } else if (strcmp(hw->driver->name, "drmprime") == 0) {
            p->hwdec_type = HWDEC_DRMPRIME;
        }
    }

    if (p->hwdec_type == HWDEC_NONE) {
        MP_ERR(vo, "No valid hardware decoding driver could be loaded!");
        goto err;
    }

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
