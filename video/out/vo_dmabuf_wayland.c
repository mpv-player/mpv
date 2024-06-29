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

#include <libavutil/hwcontext_drm.h>
#include <sys/mman.h>
#include <unistd.h>
#include "config.h"

#if HAVE_VAAPI
#include <va/va_drmcommon.h>
#endif

#include "common/global.h"
#include "gpu/hwdec.h"
#include "gpu/video.h"
#include "mpv_talloc.h"
#include "present_sync.h"
#include "sub/draw_bmp.h"
#include "video/fmt-conversion.h"
#include "video/mp_image.h"
#include "vo.h"
#include "wayland_common.h"
#include "wldmabuf/ra_wldmabuf.h"

#if HAVE_VAAPI
#include "video/vaapi.h"
#endif

// Generated from wayland-protocols
#include "linux-dmabuf-v1.h"
#include "viewporter.h"
#include "single-pixel-buffer-v1.h"

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
    struct vo_frame *frame;

    uint32_t drm_format;
    uintptr_t id;
};

struct osd_buffer {
    struct vo *vo;
    struct wl_buffer *buffer;
    struct wl_list link;
    struct mp_image image;
    size_t size;
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
    struct wl_list osd_buffer_list;

    struct wl_shm_pool *osd_shm_pool;
    uint8_t *osd_shm_data;
    int osd_shm_width;
    int osd_shm_stride;
    int osd_shm_height;
    bool osd_surface_is_mapped;
    bool osd_surface_has_contents;

    struct osd_buffer *osd_buffer;
    struct mp_draw_sub_cache *osd_cache;
    struct mp_osd_res screen_osd_res;

    bool destroy_buffers;
    bool force_window;
    enum hwdec_type hwdec_type;
    uint32_t drm_format;
    uint64_t drm_modifier;
};

static void buffer_handle_release(void *data, struct wl_buffer *wl_buffer)
{
    struct buffer *buf = data;
    if (buf->frame) {
        talloc_free(buf->frame);
        buf->frame = NULL;
    }
}

static const struct wl_buffer_listener buffer_listener = {
    buffer_handle_release,
};

static void osd_buffer_handle_release(void *data, struct wl_buffer *wl_buffer)
{
    struct osd_buffer *osd_buf = data;
    wl_list_remove(&osd_buf->link);
    if (osd_buf->buffer) {
        wl_buffer_destroy(osd_buf->buffer);
        osd_buf->buffer = NULL;
    }
    talloc_free(osd_buf);
}

static const struct wl_buffer_listener osd_buffer_listener = {
    osd_buffer_handle_release,
};

#if HAVE_VAAPI
static void close_file_descriptors(const VADRMPRIMESurfaceDescriptor *desc)
{
    for (int i = 0; i < desc->num_objects; i++)
        close(desc->objects[i].fd);
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

static bool vaapi_drm_format(struct vo *vo, struct mp_image *src)
{
    bool format = false;
#if HAVE_VAAPI
    struct priv *p = vo->priv;
    VADRMPRIMESurfaceDescriptor desc = {0};

    uintptr_t id = vaapi_surface_id(src);
    VADisplay display = ra_get_native_resource(p->ctx->ra, "VADisplay");
    VAStatus status = vaExportSurfaceHandle(display, id, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                            VA_EXPORT_SURFACE_COMPOSED_LAYERS | VA_EXPORT_SURFACE_READ_ONLY, &desc);

    if (!CHECK_VA_STATUS(vo, "vaExportSurfaceHandle()")) {
        /* invalid surface warning => composed layers not supported */
        if (status == VA_STATUS_ERROR_INVALID_SURFACE)
            MP_VERBOSE(vo, "vaExportSurfaceHandle: composed layers not supported.\n");
        goto done;
    }
    p->drm_format = desc.layers[0].drm_format;
    p->drm_modifier = desc.objects[0].drm_format_modifier;
    format = true;
done:
    close_file_descriptors(&desc);
#endif
    return format;
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
        MP_VERBOSE(vo, "%s(%016" PRIx64 ") is not supported.\n",
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
    close_file_descriptors(&desc);
#endif
}

static uintptr_t drmprime_surface_id(struct mp_image *src)
{
    uintptr_t id = 0;
    struct AVDRMFrameDescriptor *desc = (AVDRMFrameDescriptor *)src->planes[0];

    AVDRMObjectDescriptor object = desc->objects[0];
    id = (uintptr_t)object.fd;
    return id;
}

static bool drmprime_drm_format(struct vo *vo, struct mp_image *src)
{
    struct priv *p = vo->priv;
    struct AVDRMFrameDescriptor *desc = (AVDRMFrameDescriptor *)src->planes[0];
    if (!desc)
        return false;

    // Just check the very first layer/plane.
    p->drm_format = desc->layers[0].format;
    int object_index = desc->layers[0].planes[0].object_index;
    p->drm_modifier = desc->objects[object_index].format_modifier;
    return true;
}

static void drmprime_dmabuf_importer(struct buffer *buf, struct mp_image *src,
                                     struct zwp_linux_buffer_params_v1 *params)
{
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

static bool drm_format_check(struct vo *vo, struct mp_image *src)
{
    struct priv *p = vo->priv;
    switch(p->hwdec_type) {
    case HWDEC_VAAPI:
        return vaapi_drm_format(vo, src);
    case HWDEC_DRMPRIME:
        return drmprime_drm_format(vo, src);
    }
    return false;
}

static struct buffer *buffer_check(struct vo *vo, struct vo_frame *frame)
{
    struct priv *p = vo->priv;

    /* Make more buffers if we're not at the desired amount yet. */
    if (wl_list_length(&p->buffer_list) < WL_BUFFERS_WANTED)
        goto done;

    uintptr_t id = surface_id(vo, frame->current);
    struct buffer *buf;
    wl_list_for_each(buf, &p->buffer_list, link) {
        if (buf->id == id) {
            if (buf->frame)
                talloc_free(buf->frame);
            buf->frame = frame;
            return buf;
        }
    }

done:
    return NULL;
}

static struct buffer *buffer_create(struct vo *vo, struct vo_frame *frame)
{
    struct vo_wayland_state *wl = vo->wl;
    struct priv *p = vo->priv;

    struct buffer *buf = talloc_zero(vo, struct buffer);
    buf->vo = vo;
    buf->frame = frame;

    struct mp_image *image = buf->frame->current;
    struct zwp_linux_buffer_params_v1 *params = zwp_linux_dmabuf_v1_create_params(wl->dmabuf);

    switch(p->hwdec_type) {
    case HWDEC_VAAPI:
        vaapi_dmabuf_importer(buf, image, params);
        break;
    case HWDEC_DRMPRIME:
        drmprime_dmabuf_importer(buf, image, params);
        break;
    }

    if (!buf->drm_format) {
        talloc_free(buf->frame);
        talloc_free(buf);
        zwp_linux_buffer_params_v1_destroy(params);
        return NULL;
    }

    buf->buffer = zwp_linux_buffer_params_v1_create_immed(params, image->params.w, image->params.h,
                                                          buf->drm_format, 0);
    zwp_linux_buffer_params_v1_destroy(params);
    wl_buffer_add_listener(buf->buffer, &buffer_listener, buf);
    wl_list_insert(&p->buffer_list, &buf->link);
    return buf;
}

static struct buffer *buffer_get(struct vo *vo, struct vo_frame *frame)
{
    /* Reuse existing buffer if possible. */
    struct buffer *buf = buffer_check(vo, frame);
    if (buf) {
        return buf;
    } else {
        return buffer_create(vo, frame);
    }
}

static void destroy_buffers(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct buffer *buf, *tmp;
    p->destroy_buffers = false;
    wl_list_for_each_safe(buf, tmp, &p->buffer_list, link) {
        wl_list_remove(&buf->link);
        if (buf->frame) {
            talloc_free(buf->frame);
            buf->frame = NULL;
        }
        if (buf->buffer) {
            wl_buffer_destroy(buf->buffer);
            buf->buffer = NULL;
        }
        talloc_free(buf);
    }
}

static void destroy_osd_buffers(struct vo *vo)
{
    if (!vo->wl)
        return;

    // Remove any existing buffer before we destroy them.
    wl_surface_attach(vo->wl->osd_surface, NULL, 0, 0);
    wl_surface_commit(vo->wl->osd_surface);

    struct priv *p = vo->priv;
    struct osd_buffer *osd_buf, *tmp;
    wl_list_for_each_safe(osd_buf, tmp, &p->osd_buffer_list, link) {
        wl_list_remove(&osd_buf->link);
        munmap(osd_buf->image.planes[0], osd_buf->size);
        if (osd_buf->buffer) {
            wl_buffer_destroy(osd_buf->buffer);
            osd_buf->buffer = NULL;
        }
    }
}

static struct osd_buffer *osd_buffer_check(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct osd_buffer *osd_buf;
    wl_list_for_each(osd_buf, &p->osd_buffer_list, link) {
        return osd_buf;
    }
    return NULL;
}

static struct osd_buffer *osd_buffer_create(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct osd_buffer *osd_buf = talloc_zero(vo, struct osd_buffer);

    osd_buf->vo = vo;
    osd_buf->size = p->osd_shm_height * p->osd_shm_stride;
    mp_image_set_size(&osd_buf->image, p->osd_shm_width, p->osd_shm_height);
    osd_buf->image.planes[0] = p->osd_shm_data;
    osd_buf->image.stride[0] = p->osd_shm_stride;
    osd_buf->buffer = wl_shm_pool_create_buffer(p->osd_shm_pool, 0,
                                                p->osd_shm_width, p->osd_shm_height,
                                                p->osd_shm_stride, WL_SHM_FORMAT_ARGB8888);

    if (!osd_buf->buffer) {
        talloc_free(osd_buf);
        return NULL;
    }

    wl_list_insert(&p->osd_buffer_list, &osd_buf->link);
    wl_buffer_add_listener(osd_buf->buffer, &osd_buffer_listener, osd_buf);
    return osd_buf;
}

static struct osd_buffer *osd_buffer_get(struct vo *vo)
{
    struct osd_buffer *osd_buf = osd_buffer_check(vo);
    if (osd_buf) {
        return osd_buf;
    } else {
        return osd_buffer_create(vo);
    }
}

static void create_shm_pool(struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wl;
    struct priv *p = vo->priv;

    int stride = MP_ALIGN_UP(vo->dwidth * 4, 16);
    size_t size = vo->dheight * stride;
    int fd = vo_wayland_allocate_memfd(vo, size);
    if (fd < 0)
        return;
    uint8_t *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED)
        goto error1;
    struct wl_shm_pool *pool = wl_shm_create_pool(wl->shm, fd, size);
    if (!pool)
        goto error2;
    close(fd);

    destroy_osd_buffers(vo);

    if (p->osd_shm_pool)
        wl_shm_pool_destroy(p->osd_shm_pool);
    p->osd_shm_pool = pool;
    p->osd_shm_width = vo->dwidth;
    p->osd_shm_height = vo->dheight;
    p->osd_shm_stride = stride;
    p->osd_shm_data = data;
    return;

error2:
    munmap(data, size);
error1:
    close(fd);
}

static void set_viewport_source(struct vo *vo, struct mp_rect src)
{
    struct priv *p = vo->priv;
    struct vo_wayland_state *wl = vo->wl;

    if (p->force_window)
        return;

    if (!mp_rect_equals(&p->src, &src)) {
        wp_viewport_set_source(wl->video_viewport, wl_fixed_from_int(src.x0),
                               wl_fixed_from_int(src.y0), wl_fixed_from_int(mp_rect_w(src)),
                               wl_fixed_from_int(mp_rect_h(src)));
        p->src = src;
    }
}

static void resize(struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wl;
    struct priv *p = vo->priv;

    struct mp_rect src;
    struct mp_rect dst;
    struct mp_vo_opts *vo_opts = wl->vo_opts;

    const int width = mp_rect_w(wl->geometry);
    const int height = mp_rect_h(wl->geometry);

    if (width == 0 || height == 0)
        return;

    vo_wayland_set_opaque_region(wl, false);
    vo->dwidth = width;
    vo->dheight = height;

    create_shm_pool(vo);

    // top level viewport is calculated with pan set to zero
    vo->opts->pan_x = 0;
    vo->opts->pan_y = 0;
    vo_get_src_dst_rects(vo, &src, &dst, &p->screen_osd_res);
    int window_w = p->screen_osd_res.ml + p->screen_osd_res.mr + mp_rect_w(dst);
    int window_h = p->screen_osd_res.mt + p->screen_osd_res.mb + mp_rect_h(dst);
    wp_viewport_set_destination(wl->viewport, lround(window_w / wl->scaling),
                                lround(window_h / wl->scaling));

    //now we restore pan for video viewport calculation
    vo->opts->pan_x = vo_opts->pan_x;
    vo->opts->pan_y = vo_opts->pan_y;
    vo_get_src_dst_rects(vo, &src, &dst, &p->screen_osd_res);
    wp_viewport_set_destination(wl->video_viewport, lround(mp_rect_w(dst) / wl->scaling),
                                                    lround(mp_rect_h(dst) / wl->scaling));
    wl_subsurface_set_position(wl->video_subsurface, lround(dst.x0 / wl->scaling), lround(dst.y0 / wl->scaling));
    wp_viewport_set_destination(wl->osd_viewport, lround(vo->dwidth / wl->scaling),
                                                  lround(vo->dheight / wl->scaling));
    wl_subsurface_set_position(wl->osd_subsurface, lround((0 - dst.x0) / wl->scaling), lround((0 - dst.y0) / wl->scaling));
    set_viewport_source(vo, src);
}

static bool draw_osd(struct vo *vo, struct mp_image *cur, double pts)
{
    struct priv *p = vo->priv;
    struct mp_osd_res *res = &p->screen_osd_res;
    bool draw = false;

    struct sub_bitmap_list *sbs = osd_render(vo->osd, *res, pts, 0, mp_draw_sub_formats);

    if (!sbs)
        return draw;

    struct mp_rect act_rc[1], mod_rc[64];
    int num_act_rc = 0, num_mod_rc = 0;

    if (!p->osd_cache)
        p->osd_cache = mp_draw_sub_alloc(p, vo->global);

    struct mp_image *osd = mp_draw_sub_overlay(p->osd_cache, sbs, act_rc,
                                               MP_ARRAY_SIZE(act_rc), &num_act_rc,
                                               mod_rc, MP_ARRAY_SIZE(mod_rc), &num_mod_rc);

    p->osd_surface_has_contents = num_act_rc > 0;

    if (!osd || !num_mod_rc)
        goto done;

    for (int n = 0; n < num_mod_rc; n++) {
        struct mp_rect rc = mod_rc[n];

        int rw = mp_rect_w(rc);
        int rh = mp_rect_h(rc);

        void *src = mp_image_pixel_ptr(osd, 0, rc.x0, rc.y0);
        void *dst = cur->planes[0] + rc.x0 * 4 + rc.y0 * cur->stride[0];

        memcpy_pic(dst, src, rw * 4, rh, cur->stride[0], osd->stride[0]);
    }

    draw = true;
done:
    talloc_free(sbs);
    return draw;
}

static void draw_frame(struct vo *vo, struct vo_frame *frame)
{
    struct priv *p = vo->priv;
    struct vo_wayland_state *wl = vo->wl;
    struct buffer *buf;
    struct osd_buffer *osd_buf;
    double pts;

    if (!vo_wayland_check_visible(vo)) {
        if (frame->current)
            talloc_free(frame);
        return;
    }

    if (p->destroy_buffers)
        destroy_buffers(vo);

    // Reuse the solid buffer so the osd can be visible
    if (p->force_window) {
        wl_surface_attach(wl->video_surface, p->solid_buffer, 0, 0);
        wl_surface_damage_buffer(wl->video_surface, 0, 0, 1, 1);
    }

    pts = frame->current ? frame->current->pts : 0;
    if (frame->current) {
        buf = buffer_get(vo, frame);

        if (buf && buf->frame) {
            struct mp_image *image = buf->frame->current;
            wl_surface_attach(wl->video_surface, buf->buffer, 0, 0);
            wl_surface_damage_buffer(wl->video_surface, 0, 0, image->w,
                                     image->h);

        }
    }

    osd_buf = osd_buffer_get(vo);
    if (osd_buf && osd_buf->buffer) {
        if (draw_osd(vo, &osd_buf->image, pts) && p->osd_surface_has_contents) {
            wl_surface_attach(wl->osd_surface, osd_buf->buffer, 0, 0);
            wl_surface_damage_buffer(wl->osd_surface, 0, 0, osd_buf->image.w,
                                     osd_buf->image.h);
            p->osd_surface_is_mapped = true;
        } else if (!p->osd_surface_has_contents && p->osd_surface_is_mapped) {
            wl_surface_attach(wl->osd_surface, NULL, 0, 0);
            p->osd_surface_is_mapped = false;
        }
    }
}

static void flip_page(struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wl;

    wl_surface_commit(wl->video_surface);
    wl_surface_commit(wl->osd_surface);
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

static int reconfig(struct vo *vo, struct mp_image *img)
{
    struct priv *p = vo->priv;

    if (img->params.force_window) {
        p->force_window = true;
        goto done;
    }

    if (!drm_format_check(vo, img)) {
        MP_ERR(vo, "Unable to get drm format from hardware decoding!\n");
        return VO_ERROR;
    }

    if (!ra_compatible_format(p->ctx->ra, p->drm_format, p->drm_modifier)) {
        MP_ERR(vo, "Format '%s' with modifier '(%016" PRIx64 ")' is not supported by"
               " the compositor.\n", mp_tag_str(p->drm_format), p->drm_modifier);
        return VO_ERROR;
    }

    p->force_window = false;
done:
    if (!vo_wayland_reconfig(vo))
        return VO_ERROR;

    wl_surface_set_buffer_transform(vo->wl->video_surface, img->params.rotate / 90);

    // Immediately destroy all buffers if params change.
    destroy_buffers(vo);
    return 0;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;
    int events = 0;
    int ret;

    switch (request) {
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
    destroy_osd_buffers(vo);
    if (p->osd_shm_pool)
        wl_shm_pool_destroy(p->osd_shm_pool);
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
    wl_list_init(&p->buffer_list);
    wl_list_init(&p->osd_buffer_list);
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

    if (vo->wl->single_pixel_manager) {
        p->solid_buffer = wp_single_pixel_buffer_manager_v1_create_u32_rgba_buffer(
            vo->wl->single_pixel_manager, 0, 0, 0, UINT32_MAX); /* R, G, B, A */
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
    p->hwdec_ctx = (struct ra_hwdec_ctx) {
        .log = p->log,
        .global = p->global,
        .ra_ctx = p->ctx,
    };
    ra_hwdec_ctx_init(&p->hwdec_ctx, vo->hwdec_devs, NULL, true);

    // Loop through hardware accelerated formats and only request known
    // supported formats.
    for (int i = IMGFMT_VDPAU_OUTPUT; i < IMGFMT_AVPIXFMT_START; ++i) {
        if (is_supported_fmt(i)) {
            struct hwdec_imgfmt_request params = {
                .imgfmt = i,
                .probing = false,
            };
            ra_hwdec_ctx_load_fmt(&p->hwdec_ctx, vo->hwdec_devs, &params);
        }
    }

    for (int i = 0; i < p->hwdec_ctx.num_hwdecs; i++) {
        struct ra_hwdec *hw = p->hwdec_ctx.hwdecs[i];
        if (ra_get_native_resource(p->ctx->ra, "VADisplay")) {
            p->hwdec_type = HWDEC_VAAPI;
        } else if (strcmp(hw->driver->name, "drmprime") == 0) {
            p->hwdec_type = HWDEC_DRMPRIME;
        }
    }

    if (p->hwdec_type == HWDEC_NONE) {
        MP_ERR(vo, "No valid hardware decoding driver could be loaded!\n");
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
    .caps = VO_CAP_ROTATE90,
    .frame_owner = true,
    .preinit = preinit,
    .query_format = query_format,
    .reconfig2 = reconfig,
    .control = control,
    .draw_frame = draw_frame,
    .flip_page = flip_page,
    .get_vsync = get_vsync,
    .wakeup = vo_wayland_wakeup,
    .wait_events = vo_wayland_wait_events,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
};
