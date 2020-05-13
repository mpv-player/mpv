/*
 * VA API output module
 *
 * Copyright (C) 2008-2009 Splitted-Desktop Systems
 * Gwenole Beauchesne <gbeauchesne@splitted-desktop.com>
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <stdarg.h>
#include <limits.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <va/va_x11.h>

#include "config.h"
#include "common/msg.h"
#include "video/out/vo.h"
#include "video/mp_image_pool.h"
#include "video/sws_utils.h"
#include "sub/draw_bmp.h"
#include "sub/img_convert.h"
#include "sub/osd.h"
#include "x11_common.h"

#include "video/mp_image.h"
#include "video/vaapi.h"
#include "video/hwdec.h"

struct vaapi_osd_image {
    int            w, h;
    VAImage        image;
    VASubpictureID subpic_id;
    bool           is_used;
};

struct vaapi_subpic {
    VASubpictureID id;
    int src_x, src_y, src_w, src_h;
    int dst_x, dst_y, dst_w, dst_h;
};

struct vaapi_osd_part {
    bool active;
    int change_id;
    struct vaapi_osd_image image;
    struct vaapi_subpic subpic;
};

#define MAX_OUTPUT_SURFACES 2

struct priv {
    struct mp_log           *log;
    struct vo               *vo;
    VADisplay                display;
    struct mp_vaapi_ctx     *mpvaapi;

    struct mp_image_params   image_params;
    struct mp_rect           src_rect;
    struct mp_rect           dst_rect;
    struct mp_osd_res        screen_osd_res;

    struct mp_image         *output_surfaces[MAX_OUTPUT_SURFACES];
    struct mp_image         *swdec_surfaces[MAX_OUTPUT_SURFACES];

    int                      output_surface;
    int                      visible_surface;
    int                      scaling;
    int                      force_scaled_osd;

    VAImageFormat            osd_format; // corresponds to OSD_VA_FORMAT
    struct vaapi_osd_part    osd_part;
    bool                     osd_screen;
    struct mp_draw_sub_cache *osd_cache;

    struct mp_image_pool    *pool;

    struct mp_image         *black_surface;

    VAImageFormat           *va_subpic_formats;
    unsigned int            *va_subpic_flags;
    int                      va_num_subpic_formats;
    VADisplayAttribute      *va_display_attrs;
    int                     *mp_display_attr;
    int                      va_num_display_attrs;

    struct va_image_formats *image_formats;
};

#define OSD_VA_FORMAT VA_FOURCC_BGRA

static void draw_osd(struct vo *vo);


struct fmtentry {
    uint32_t va;
    enum mp_imgfmt mp;
};

static const struct fmtentry va_to_imgfmt[] = {
    {VA_FOURCC_NV12, IMGFMT_NV12},
    {VA_FOURCC_YV12, IMGFMT_420P},
    {VA_FOURCC_IYUV, IMGFMT_420P},
    {VA_FOURCC_UYVY, IMGFMT_UYVY},
    // Note: not sure about endian issues (the mp formats are byte-addressed)
    {VA_FOURCC_RGBA, IMGFMT_RGBA},
    {VA_FOURCC_RGBX, IMGFMT_RGBA},
    {VA_FOURCC_BGRA, IMGFMT_BGRA},
    {VA_FOURCC_BGRX, IMGFMT_BGRA},
    {0             , IMGFMT_NONE}
};

static enum mp_imgfmt va_fourcc_to_imgfmt(uint32_t fourcc)
{
    for (const struct fmtentry *entry = va_to_imgfmt; entry->va; ++entry) {
        if (entry->va == fourcc)
            return entry->mp;
    }
    return IMGFMT_NONE;
}

static uint32_t va_fourcc_from_imgfmt(int imgfmt)
{
    for (const struct fmtentry *entry = va_to_imgfmt; entry->va; ++entry) {
        if (entry->mp == imgfmt)
            return entry->va;
    }
    return 0;
}

struct va_image_formats {
    VAImageFormat *entries;
    int num;
};

static void va_get_formats(struct priv *ctx)
{
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

static VAImageFormat *va_image_format_from_imgfmt(struct priv *ctx,
                                                  int imgfmt)
{
    struct va_image_formats *formats = ctx->image_formats;
    const int fourcc = va_fourcc_from_imgfmt(imgfmt);
    if (!formats || !formats->num || !fourcc)
        return NULL;
    for (int i = 0; i < formats->num; i++) {
        if (formats->entries[i].fourcc == fourcc)
            return &formats->entries[i];
    }
    return NULL;
}

struct va_surface {
    struct mp_vaapi_ctx *ctx;
    VADisplay display;

    VASurfaceID id;
    int rt_format;

    // The actually allocated surface size (needed for cropping).
    // mp_images can have a smaller size than this, which means they are
    // cropped down to a smaller size by removing right/bottom pixels.
    int w, h;

    VAImage image;       // used for software decoding case
    bool is_derived;     // is image derived by vaDeriveImage()?
};

static struct va_surface *va_surface_in_mp_image(struct mp_image *mpi)
{
    return mpi && mpi->imgfmt == IMGFMT_VAAPI ?
        (struct va_surface*)mpi->planes[0] : NULL;
}

static void release_va_surface(void *arg)
{
    struct va_surface *surface = arg;

    if (surface->id != VA_INVALID_ID) {
        if (surface->image.image_id != VA_INVALID_ID)
            vaDestroyImage(surface->display, surface->image.image_id);
        vaDestroySurfaces(surface->display, &surface->id, 1);
    }

    talloc_free(surface);
}

static struct mp_image *alloc_surface(struct mp_vaapi_ctx *ctx, int rt_format,
                                      int w, int h)
{
    VASurfaceID id = VA_INVALID_ID;
    VAStatus status;
    status = vaCreateSurfaces(ctx->display, rt_format, w, h, &id, 1, NULL, 0);
    if (!CHECK_VA_STATUS(ctx, "vaCreateSurfaces()"))
        return NULL;

    struct va_surface *surface = talloc_ptrtype(NULL, surface);
    if (!surface)
        return NULL;

    *surface = (struct va_surface){
        .ctx = ctx,
        .id = id,
        .rt_format = rt_format,
        .w = w,
        .h = h,
        .display = ctx->display,
        .image = { .image_id = VA_INVALID_ID, .buf = VA_INVALID_ID },
    };

    struct mp_image img = {0};
    mp_image_setfmt(&img, IMGFMT_VAAPI);
    mp_image_set_size(&img, w, h);
    img.planes[0] = (uint8_t*)surface;
    img.planes[3] = (uint8_t*)(uintptr_t)surface->id;
    return mp_image_new_custom_ref(&img, surface, release_va_surface);
}

static void va_surface_image_destroy(struct va_surface *surface)
{
    if (!surface || surface->image.image_id == VA_INVALID_ID)
        return;
    vaDestroyImage(surface->display, surface->image.image_id);
    surface->image.image_id = VA_INVALID_ID;
    surface->is_derived = false;
}

static int va_surface_image_alloc(struct va_surface *p, VAImageFormat *format)
{
    VADisplay *display = p->display;

    if (p->image.image_id != VA_INVALID_ID &&
        p->image.format.fourcc == format->fourcc)
        return 0;

    int r = 0;

    va_surface_image_destroy(p);

    VAStatus status = vaDeriveImage(display, p->id, &p->image);
    if (status == VA_STATUS_SUCCESS) {
        /* vaDeriveImage() is supported, check format */
        if (p->image.format.fourcc == format->fourcc &&
            p->image.width == p->w && p->image.height == p->h)
        {
            p->is_derived = true;
            MP_TRACE(p->ctx, "Using vaDeriveImage()\n");
        } else {
            vaDestroyImage(p->display, p->image.image_id);
            status = VA_STATUS_ERROR_OPERATION_FAILED;
        }
    }
    if (status != VA_STATUS_SUCCESS) {
        p->image.image_id = VA_INVALID_ID;
        status = vaCreateImage(p->display, format, p->w, p->h, &p->image);
        if (!CHECK_VA_STATUS(p->ctx, "vaCreateImage()")) {
            p->image.image_id = VA_INVALID_ID;
            r = -1;
        }
    }

    return r;
}

// img must be a VAAPI surface; make sure its internal VAImage is allocated
// to a format corresponding to imgfmt (or return an error).
static int va_surface_alloc_imgfmt(struct priv *priv, struct mp_image *img,
                                   int imgfmt)
{
    struct va_surface *p = va_surface_in_mp_image(img);
    if (!p)
        return -1;
    // Multiple FourCCs can refer to the same imgfmt, so check by doing the
    // surjective conversion first.
    if (p->image.image_id != VA_INVALID_ID &&
        va_fourcc_to_imgfmt(p->image.format.fourcc) == imgfmt)
        return 0;
    VAImageFormat *format = va_image_format_from_imgfmt(priv, imgfmt);
    if (!format)
        return -1;
    if (va_surface_image_alloc(p, format) < 0)
        return -1;
    return 0;
}

static bool va_image_map(struct mp_vaapi_ctx *ctx, VAImage *image,
                         struct mp_image *mpi)
{
    int imgfmt = va_fourcc_to_imgfmt(image->format.fourcc);
    if (imgfmt == IMGFMT_NONE)
        return false;
    void *data = NULL;
    const VAStatus status = vaMapBuffer(ctx->display, image->buf, &data);
    if (!CHECK_VA_STATUS(ctx, "vaMapBuffer()"))
        return false;

    *mpi = (struct mp_image) {0};
    mp_image_setfmt(mpi, imgfmt);
    mp_image_set_size(mpi, image->width, image->height);

    for (int p = 0; p < image->num_planes; p++) {
        mpi->stride[p] = image->pitches[p];
        mpi->planes[p] = (uint8_t *)data + image->offsets[p];
    }

    if (image->format.fourcc == VA_FOURCC_YV12) {
        MPSWAP(int, mpi->stride[1], mpi->stride[2]);
        MPSWAP(uint8_t *, mpi->planes[1], mpi->planes[2]);
    }

    return true;
}

static bool va_image_unmap(struct mp_vaapi_ctx *ctx, VAImage *image)
{
    const VAStatus status = vaUnmapBuffer(ctx->display, image->buf);
    return CHECK_VA_STATUS(ctx, "vaUnmapBuffer()");
}

// va_dst: copy destination, must be IMGFMT_VAAPI
// sw_src: copy source, must be a software pixel format
static int va_surface_upload(struct priv *priv, struct mp_image *va_dst,
                             struct mp_image *sw_src)
{
    struct va_surface *p = va_surface_in_mp_image(va_dst);
    if (!p)
        return -1;

    if (va_surface_alloc_imgfmt(priv, va_dst, sw_src->imgfmt) < 0)
        return -1;

    struct mp_image img;
    if (!va_image_map(p->ctx, &p->image, &img))
        return -1;
    assert(sw_src->w <= img.w && sw_src->h <= img.h);
    mp_image_set_size(&img, sw_src->w, sw_src->h); // copy only visible part
    mp_image_copy(&img, sw_src);
    va_image_unmap(p->ctx, &p->image);

    if (!p->is_derived) {
        VAStatus status = vaPutImage(p->display, p->id,
                                     p->image.image_id,
                                     0, 0, sw_src->w, sw_src->h,
                                     0, 0, sw_src->w, sw_src->h);
        if (!CHECK_VA_STATUS(p->ctx, "vaPutImage()"))
            return -1;
    }

    if (p->is_derived)
        va_surface_image_destroy(p);
    return 0;
}

struct pool_alloc_ctx {
    struct mp_vaapi_ctx *vaapi;
    int rt_format;
};

static struct mp_image *alloc_pool(void *pctx, int fmt, int w, int h)
{
    struct pool_alloc_ctx *alloc_ctx = pctx;
    if (fmt != IMGFMT_VAAPI)
        return NULL;

    return alloc_surface(alloc_ctx->vaapi, alloc_ctx->rt_format, w, h);
}

// The allocator of the given image pool to allocate VAAPI surfaces, using
// the given rt_format.
static void va_pool_set_allocator(struct mp_image_pool *pool,
                                  struct mp_vaapi_ctx *ctx, int rt_format)
{
    struct pool_alloc_ctx *alloc_ctx = talloc_ptrtype(pool, alloc_ctx);
    *alloc_ctx = (struct pool_alloc_ctx){
        .vaapi = ctx,
        .rt_format = rt_format,
    };
    mp_image_pool_set_allocator(pool, alloc_pool, alloc_ctx);
    mp_image_pool_set_lru(pool);
}

static void flush_output_surfaces(struct priv *p)
{
    for (int n = 0; n < MAX_OUTPUT_SURFACES; n++)
        mp_image_unrefp(&p->output_surfaces[n]);
    p->output_surface = 0;
    p->visible_surface = 0;
}

// See flush_surfaces() remarks - the same applies.
static void free_video_specific(struct priv *p)
{
    flush_output_surfaces(p);

    mp_image_unrefp(&p->black_surface);

    for (int n = 0; n < MAX_OUTPUT_SURFACES; n++)
        mp_image_unrefp(&p->swdec_surfaces[n]);

    if (p->pool)
        mp_image_pool_clear(p->pool);
}

static bool alloc_swdec_surfaces(struct priv *p, int w, int h, int imgfmt)
{
    free_video_specific(p);
    for (int i = 0; i < MAX_OUTPUT_SURFACES; i++) {
        p->swdec_surfaces[i] = mp_image_pool_get(p->pool, IMGFMT_VAAPI, w, h);
        if (va_surface_alloc_imgfmt(p, p->swdec_surfaces[i], imgfmt) < 0)
            return false;
    }
    return true;
}

static void resize(struct priv *p)
{
    vo_get_src_dst_rects(p->vo, &p->src_rect, &p->dst_rect, &p->screen_osd_res);

    // It's not clear whether this is needed; maybe not.
    //vo_x11_clearwindow(p->vo, p->vo->x11->window);

    p->vo->want_redraw = true;
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *p = vo->priv;

    free_video_specific(p);

    vo_x11_config_vo_window(vo);

    if (params->imgfmt != IMGFMT_VAAPI) {
        if (!alloc_swdec_surfaces(p, params->w, params->h, params->imgfmt))
            return -1;
    }

    p->image_params = *params;
    resize(p);
    return 0;
}

static int query_format(struct vo *vo, int imgfmt)
{
    struct priv *p = vo->priv;
    if (imgfmt == IMGFMT_VAAPI || va_image_format_from_imgfmt(p, imgfmt))
        return 1;

    return 0;
}

static bool render_to_screen(struct priv *p, struct mp_image *mpi)
{
    VAStatus status;

    VASurfaceID surface = va_surface_id(mpi);
    if (surface == VA_INVALID_ID) {
        if (!p->black_surface) {
            int w = p->image_params.w, h = p->image_params.h;
            // 4:2:0 should work everywhere
            int fmt = IMGFMT_420P;
            p->black_surface = mp_image_pool_get(p->pool, IMGFMT_VAAPI, w, h);
            if (p->black_surface) {
                struct mp_image *img = mp_image_alloc(fmt, w, h);
                if (img) {
                    mp_image_clear(img, 0, 0, w, h);
                    if (va_surface_upload(p, p->black_surface, img) < 0)
                        mp_image_unrefp(&p->black_surface);
                    talloc_free(img);
                }
            }
        }
        surface = va_surface_id(p->black_surface);
    }

    if (surface == VA_INVALID_ID)
        return false;

    struct vaapi_osd_part *part = &p->osd_part;
    if (part->active) {
        struct vaapi_subpic *sp = &part->subpic;
        int flags = 0;
        if (p->osd_screen)
            flags |= VA_SUBPICTURE_DESTINATION_IS_SCREEN_COORD;
        status = vaAssociateSubpicture(p->display,
                                       sp->id, &surface, 1,
                                       sp->src_x, sp->src_y,
                                       sp->src_w, sp->src_h,
                                       sp->dst_x, sp->dst_y,
                                       sp->dst_w, sp->dst_h,
                                       flags);
        CHECK_VA_STATUS(p, "vaAssociateSubpicture()");
    }

    int flags = va_get_colorspace_flag(p->image_params.color.space) |
                p->scaling | VA_FRAME_PICTURE;
    status = vaPutSurface(p->display,
                          surface,
                          p->vo->x11->window,
                          p->src_rect.x0,
                          p->src_rect.y0,
                          p->src_rect.x1 - p->src_rect.x0,
                          p->src_rect.y1 - p->src_rect.y0,
                          p->dst_rect.x0,
                          p->dst_rect.y0,
                          p->dst_rect.x1 - p->dst_rect.x0,
                          p->dst_rect.y1 - p->dst_rect.y0,
                          NULL, 0,
                          flags);
    CHECK_VA_STATUS(p, "vaPutSurface()");

    if (part->active) {
        struct vaapi_subpic *sp = &part->subpic;
        status = vaDeassociateSubpicture(p->display, sp->id,
                                         &surface, 1);
        CHECK_VA_STATUS(p, "vaDeassociateSubpicture()");
    }

    return true;
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;

    p->visible_surface = p->output_surface;
    render_to_screen(p, p->output_surfaces[p->output_surface]);
    p->output_surface = (p->output_surface + 1) % MAX_OUTPUT_SURFACES;
}

static void draw_image(struct vo *vo, struct mp_image *mpi)
{
    struct priv *p = vo->priv;

    if (mpi->imgfmt != IMGFMT_VAAPI) {
        struct mp_image *dst = p->swdec_surfaces[p->output_surface];
        if (!dst || va_surface_upload(p, dst, mpi) < 0) {
            MP_WARN(vo, "Could not upload surface.\n");
            talloc_free(mpi);
            return;
        }
        mp_image_copy_attributes(dst, mpi);
        talloc_free(mpi);
        mpi = mp_image_new_ref(dst);
    }

    talloc_free(p->output_surfaces[p->output_surface]);
    p->output_surfaces[p->output_surface] = mpi;

    draw_osd(vo);
}

static void free_subpicture(struct priv *p, struct vaapi_osd_image *img)
{
    if (img->image.image_id != VA_INVALID_ID)
        vaDestroyImage(p->display, img->image.image_id);
    if (img->subpic_id != VA_INVALID_ID)
        vaDestroySubpicture(p->display, img->subpic_id);
    img->image.image_id = VA_INVALID_ID;
    img->subpic_id = VA_INVALID_ID;
}

static int new_subpicture(struct priv *p, int w, int h,
                          struct vaapi_osd_image *out)
{
    VAStatus status;

    free_subpicture(p, out);

    struct vaapi_osd_image m = {
        .image = {.image_id = VA_INVALID_ID, .buf = VA_INVALID_ID},
        .subpic_id = VA_INVALID_ID,
        .w = w,
        .h = h,
    };

    status = vaCreateImage(p->display, &p->osd_format, w, h, &m.image);
    if (!CHECK_VA_STATUS(p, "vaCreateImage()"))
        goto error;
    status = vaCreateSubpicture(p->display, m.image.image_id, &m.subpic_id);
    if (!CHECK_VA_STATUS(p, "vaCreateSubpicture()"))
        goto error;

    *out = m;
    return 0;

error:
    free_subpicture(p, &m);
    MP_ERR(p, "failed to allocate OSD sub-picture of size %dx%d.\n", w, h);
    return -1;
}

static void draw_osd(struct vo *vo)
{
    struct priv *p = vo->priv;

    struct mp_image *cur = p->output_surfaces[p->output_surface];
    double pts = cur ? cur->pts : 0;

    if (!p->osd_format.fourcc)
        return;

    struct mp_osd_res vid_res = osd_res_from_image_params(vo->params);

    struct mp_osd_res *res;
    if (p->osd_screen) {
        res = &p->screen_osd_res;
    } else {
        res = &vid_res;
    }

    p->osd_part.active = false;

    if (!p->osd_cache)
        p->osd_cache = mp_draw_sub_alloc(p, vo->global);

    struct sub_bitmap_list *sbs = osd_render(vo->osd, *res, pts, 0,
                                             mp_draw_sub_formats);

    struct mp_rect act_rc[1], mod_rc[64];
    int num_act_rc = 0, num_mod_rc = 0;

    struct mp_image *osd = mp_draw_sub_overlay(p->osd_cache, sbs,
                    act_rc, MP_ARRAY_SIZE(act_rc), &num_act_rc,
                    mod_rc, MP_ARRAY_SIZE(mod_rc), &num_mod_rc);

    if (!osd)
        goto error;

    struct vaapi_osd_part *part = &p->osd_part;

    part->active = false;

    int w = res->w;
    int h = res->h;
    if (part->image.w != w || part->image.h != h) {
        if (new_subpicture(p, w, h, &part->image) < 0)
            goto error;
    }

    struct vaapi_osd_image *img = &part->image;
    struct mp_image vaimg;
    if (!va_image_map(p->mpvaapi, &img->image, &vaimg))
        goto error;

    for (int n = 0; n < num_mod_rc; n++) {
        struct mp_rect *rc = &mod_rc[n];

        int rw = mp_rect_w(*rc);
        int rh = mp_rect_h(*rc);

        void *src = mp_image_pixel_ptr(osd, 0, rc->x0, rc->y0);
        void *dst = vaimg.planes[0] + rc->y0 * vaimg.stride[0] + rc->x0 * 4;

        memcpy_pic(dst, src, rw * 4, rh, vaimg.stride[0], osd->stride[0]);
    }

    if (!va_image_unmap(p->mpvaapi, &img->image))
        goto error;

    if (num_act_rc) {
        struct mp_rect rc = act_rc[0];
        rc.x0 = rc.y0 = 0; // must be a Mesa bug
        part->subpic = (struct vaapi_subpic) {
            .id = img->subpic_id,
            .src_x = rc.x0,         .src_y = rc.y0,
            .src_w = mp_rect_w(rc), .src_h = mp_rect_h(rc),
            .dst_x = rc.x0,         .dst_y = rc.y0,
            .dst_w = mp_rect_w(rc), .dst_h = mp_rect_h(rc),
        };
        part->active = true;
    }

error:
    talloc_free(sbs);
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;

    switch (request) {
    case VOCTRL_REDRAW_FRAME:
        p->output_surface = p->visible_surface;
        draw_osd(vo);
        return true;
    case VOCTRL_SET_PANSCAN:
        resize(p);
        return VO_TRUE;
    }

    int events = 0;
    int r = vo_x11_control(vo, &events, request, data);
    if (events & VO_EVENT_RESIZE)
        resize(p);
    if (events & VO_EVENT_EXPOSE)
        vo->want_redraw = true;
    vo_event(vo, events);
    return r;
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;

    free_video_specific(p);
    talloc_free(p->pool);

    struct vaapi_osd_part *part = &p->osd_part;
    free_subpicture(p, &part->image);

    if (vo->hwdec_devs) {
        hwdec_devices_remove(vo->hwdec_devs, &p->mpvaapi->hwctx);
        hwdec_devices_destroy(vo->hwdec_devs);
    }

    va_destroy(p->mpvaapi);

    vo_x11_uninit(vo);
}

static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;
    p->vo = vo;
    p->log = vo->log;

    VAStatus status;

    if (!vo_x11_init(vo))
        goto fail;

    if (!vo_x11_create_vo_window(vo, NULL, "vaapi"))
        goto fail;

    p->display = vaGetDisplay(vo->x11->display);
    if (!p->display)
        goto fail;

    p->mpvaapi = va_initialize(p->display, p->log, false);
    if (!p->mpvaapi) {
        vaTerminate(p->display);
        p->display = NULL;
        goto fail;
    }

    if (va_guess_if_emulated(p->mpvaapi)) {
        MP_WARN(vo, "VA-API is most likely emulated via VDPAU.\n"
                    "It's better to use VDPAU directly with: --vo=vdpau\n");
    }

    va_get_formats(p);
    if (!p->image_formats)
        goto fail;

    p->pool = mp_image_pool_new(p);
    va_pool_set_allocator(p->pool, p->mpvaapi, VA_RT_FORMAT_YUV420);

    int max_subpic_formats = vaMaxNumSubpictureFormats(p->display);
    p->va_subpic_formats = talloc_array(vo, VAImageFormat, max_subpic_formats);
    p->va_subpic_flags = talloc_array(vo, unsigned int, max_subpic_formats);
    status = vaQuerySubpictureFormats(p->display,
                                      p->va_subpic_formats,
                                      p->va_subpic_flags,
                                      &p->va_num_subpic_formats);
    if (!CHECK_VA_STATUS(p, "vaQuerySubpictureFormats()"))
        p->va_num_subpic_formats = 0;
    MP_VERBOSE(vo, "%d subpicture formats available:\n",
               p->va_num_subpic_formats);

    for (int i = 0; i < p->va_num_subpic_formats; i++) {
        MP_VERBOSE(vo, "  %s, flags 0x%x\n",
                   mp_tag_str(p->va_subpic_formats[i].fourcc),
                   p->va_subpic_flags[i]);
        if (p->va_subpic_formats[i].fourcc == OSD_VA_FORMAT) {
            p->osd_format = p->va_subpic_formats[i];
            if (!p->force_scaled_osd) {
                p->osd_screen =
                    p->va_subpic_flags[i] & VA_SUBPICTURE_DESTINATION_IS_SCREEN_COORD;
            }
        }
    }

    if (!p->osd_format.fourcc)
        MP_ERR(vo, "OSD format not supported. Disabling OSD.\n");

    struct vaapi_osd_part *part = &p->osd_part;
    part->image.image.image_id = VA_INVALID_ID;
    part->image.subpic_id = VA_INVALID_ID;

    int max_display_attrs = vaMaxNumDisplayAttributes(p->display);
    p->va_display_attrs = talloc_array(vo, VADisplayAttribute, max_display_attrs);
    if (p->va_display_attrs) {
        status = vaQueryDisplayAttributes(p->display, p->va_display_attrs,
                                          &p->va_num_display_attrs);
        if (!CHECK_VA_STATUS(p, "vaQueryDisplayAttributes()"))
            p->va_num_display_attrs = 0;
        p->mp_display_attr = talloc_zero_array(vo, int, p->va_num_display_attrs);
    }

    vo->hwdec_devs = hwdec_devices_create();
    hwdec_devices_add(vo->hwdec_devs, &p->mpvaapi->hwctx);

    MP_WARN(vo, "Warning: this compatibility VO is low quality and may "
                "have issues with OSD, scaling, screenshots and more.\n"
                "vo=gpu is the preferred choice in any case and "
                "includes VA-API support via hwdec=vaapi or vaapi-copy.\n");

    return 0;

fail:
    uninit(vo);
    return -1;
}

#define OPT_BASE_STRUCT struct priv

const struct vo_driver video_out_vaapi = {
    .description = "VA API with X11",
    .name = "vaapi",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .wakeup = vo_x11_wakeup,
    .wait_events = vo_x11_wait_events,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .scaling = VA_FILTER_SCALING_DEFAULT,
    },
    .options = (const struct m_option[]) {
        {"scaling", OPT_CHOICE(scaling,
            {"default", VA_FILTER_SCALING_DEFAULT},
            {"fast", VA_FILTER_SCALING_FAST},
            {"hq", VA_FILTER_SCALING_HQ},
            {"nla", VA_FILTER_SCALING_NL_ANAMORPHIC})},
        {"scaled-osd", OPT_FLAG(force_scaled_osd)},
        {0}
    },
    .options_prefix = "vo-vaapi",
};
