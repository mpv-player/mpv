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
#include "sub/osd.h"
#include "sub/img_convert.h"
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
    struct osd_conv_cache *conv_cache;
};

#define MAX_OUTPUT_SURFACES 2

struct priv {
    struct mp_log           *log;
    struct vo               *vo;
    VADisplay                display;
    struct mp_vaapi_ctx     *mpvaapi;
    struct mp_hwdec_info     hwdec_info;

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
    // with old libva versions only
    int                      deint;
    int                      deint_type;

    VAImageFormat            osd_format; // corresponds to OSD_VA_FORMAT
    struct vaapi_osd_part    osd_parts[MAX_OSD_PARTS];
    bool                     osd_screen;

    struct mp_image_pool    *pool;
    struct va_image_formats *va_image_formats;

    struct mp_image         *black_surface;

    VAImageFormat           *va_subpic_formats;
    unsigned int            *va_subpic_flags;
    int                      va_num_subpic_formats;
    VADisplayAttribute      *va_display_attrs;
    int                     *mp_display_attr;
    int                      va_num_display_attrs;
};

#define OSD_VA_FORMAT VA_FOURCC_BGRA

static const bool osd_formats[SUBBITMAP_COUNT] = {
    // Actually BGRA, but only on little endian.
    // This will break on big endian, I think.
    [SUBBITMAP_RGBA] = true,
};

static void draw_osd(struct vo *vo);

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
        if (va_surface_alloc_imgfmt(p->swdec_surfaces[i], imgfmt) < 0)
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

static int reconfig(struct vo *vo, struct mp_image_params *params, int flags)
{
    struct priv *p = vo->priv;

    free_video_specific(p);

    vo_x11_config_vo_window(vo, NULL, flags, "vaapi");

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
    if (imgfmt == IMGFMT_VAAPI || va_image_format_from_imgfmt(p->mpvaapi, imgfmt))
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
                    if (va_surface_upload(p->black_surface, img) < 0)
                        mp_image_unrefp(&p->black_surface);
                    talloc_free(img);
                }
            }
        }
        surface = va_surface_id(p->black_surface);
    }

    int fields = mpi ? mpi->fields : 0;
    if (surface == VA_INVALID_ID)
        return false;

    va_lock(p->mpvaapi);

    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct vaapi_osd_part *part = &p->osd_parts[n];
        if (part->active) {
            struct vaapi_subpic *sp = &part->subpic;
            int flags = 0;
            if (p->osd_screen)
                flags |= VA_SUBPICTURE_DESTINATION_IS_SCREEN_COORD;
            status = vaAssociateSubpicture2(p->display,
                                            sp->id, &surface, 1,
                                            sp->src_x, sp->src_y,
                                            sp->src_w, sp->src_h,
                                            sp->dst_x, sp->dst_y,
                                            sp->dst_w, sp->dst_h,
                                            flags);
            CHECK_VA_STATUS(p, "vaAssociateSubpicture()");
        }
    }

    int flags = va_get_colorspace_flag(p->image_params.colorspace) | p->scaling;
    if (p->deint && (fields & MP_IMGFIELD_INTERLACED)) {
        flags |= (fields & MP_IMGFIELD_TOP_FIRST) ? VA_BOTTOM_FIELD : VA_TOP_FIELD;
    } else {
        flags |= VA_FRAME_PICTURE;
    }
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

    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct vaapi_osd_part *part = &p->osd_parts[n];
        if (part->active) {
            struct vaapi_subpic *sp = &part->subpic;
            status = vaDeassociateSubpicture(p->display, sp->id,
                                             &surface, 1);
            CHECK_VA_STATUS(p, "vaDeassociateSubpicture()");
        }
    }

    va_unlock(p->mpvaapi);

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
        if (!dst || va_surface_upload(dst, mpi) < 0) {
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

static void draw_osd_cb(void *pctx, struct sub_bitmaps *imgs)
{
    struct priv *p = pctx;

    struct vaapi_osd_part *part = &p->osd_parts[imgs->render_index];
    if (imgs->change_id != part->change_id) {
        part->change_id = imgs->change_id;

        osd_scale_rgba(part->conv_cache, imgs);

        struct mp_rect bb;
        if (!mp_sub_bitmaps_bb(imgs, &bb))
            goto error;

        // Prevent filtering artifacts on borders
        int pad = 2;

        int w = bb.x1 - bb.x0;
        int h = bb.y1 - bb.y0;
        if (part->image.w < w + pad || part->image.h < h + pad) {
            int sw = MP_ALIGN_UP(w + pad, 64);
            int sh = MP_ALIGN_UP(h + pad, 64);
            if (new_subpicture(p, sw, sh, &part->image) < 0)
                goto error;
        }

        struct vaapi_osd_image *img = &part->image;
        struct mp_image vaimg;
        if (!va_image_map(p->mpvaapi, &img->image, &vaimg))
            goto error;

        // Clear borders and regions uncovered by sub-bitmaps
        mp_image_clear(&vaimg, 0, 0, w + pad, h + pad);

        for (int n = 0; n < imgs->num_parts; n++) {
            struct sub_bitmap *sub = &imgs->parts[n];

            // Note: nothing guarantees that the sub-bitmaps don't overlap.
            //       But in all currently existing cases, they don't.
            //       We simply hope that this won't change, and nobody will
            //       ever notice our little shortcut here.

            size_t dst = (sub->y - bb.y0) * vaimg.stride[0] +
                         (sub->x - bb.x0) * 4;

            memcpy_pic(vaimg.planes[0] + dst, sub->bitmap, sub->w * 4, sub->h,
                       vaimg.stride[0], sub->stride);
        }

        if (!va_image_unmap(p->mpvaapi, &img->image))
            goto error;

        part->subpic = (struct vaapi_subpic) {
            .id = img->subpic_id,
            .src_x = 0,     .src_y = 0,
            .src_w = w,     .src_h = h,
            .dst_x = bb.x0, .dst_y = bb.y0,
            .dst_w = w,     .dst_h = h,
        };
    }

    part->active = true;

error:
    ;
}

static void draw_osd(struct vo *vo)
{
    struct priv *p = vo->priv;

    struct mp_image *cur = p->output_surfaces[p->output_surface];
    double pts = cur ? cur->pts : 0;

    if (!p->osd_format.fourcc)
        return;

    va_lock(p->mpvaapi);

    struct mp_osd_res vid_res = osd_res_from_image_params(vo->params);

    struct mp_osd_res *res;
    if (p->osd_screen) {
        res = &p->screen_osd_res;
    } else {
        res = &vid_res;
    }

    for (int n = 0; n < MAX_OSD_PARTS; n++)
        p->osd_parts[n].active = false;
    osd_draw(vo->osd, *res, pts, 0, osd_formats, draw_osd_cb, p);

    va_unlock(p->mpvaapi);
}

static int get_displayattribtype(const char *name)
{
    if (!strcmp(name, "brightness"))
        return VADisplayAttribBrightness;
    else if (!strcmp(name, "contrast"))
        return VADisplayAttribContrast;
    else if (!strcmp(name, "saturation"))
        return VADisplayAttribSaturation;
    else if (!strcmp(name, "hue"))
        return VADisplayAttribHue;
    return -1;
}

static int get_display_attribute(struct priv *p, const char *name)
{
    int type = get_displayattribtype(name);
    for (int n = 0; n < p->va_num_display_attrs; n++) {
        VADisplayAttribute *attr = &p->va_display_attrs[n];
        if (attr->type == type)
            return n;
    }
    return -1;
}

static int mp_eq_to_va(VADisplayAttribute * const attr, int mpvalue)
{
    /* normalize to attribute value range */
    int r = attr->max_value - attr->min_value;
    if (r == 0)
        return INT_MIN; // assume INT_MIN is outside allowed min/max range
    return ((mpvalue + 100) * r + 100) / 200 + attr->min_value;
}

static int get_equalizer(struct priv *p, const char *name, int *value)
{
    int index = get_display_attribute(p, name);
    if (index < 0)
        return VO_NOTIMPL;

    VADisplayAttribute *attr = &p->va_display_attrs[index];

    if (!(attr->flags & VA_DISPLAY_ATTRIB_GETTABLE))
        return VO_NOTIMPL;

    /* normalize to -100 .. 100 range */
    int r = attr->max_value - attr->min_value;
    if (r == 0)
        return VO_NOTIMPL;

    *value = ((attr->value - attr->min_value) * 200 + r / 2) / r - 100;
    if (mp_eq_to_va(attr, p->mp_display_attr[index]) == attr->value)
        *value = p->mp_display_attr[index];

    return VO_TRUE;
}

static int set_equalizer(struct priv *p, const char *name, int value)
{
    VAStatus status;
    int index = get_display_attribute(p, name);
    if (index < 0)
        return VO_NOTIMPL;

    VADisplayAttribute *attr = &p->va_display_attrs[index];

    if (!(attr->flags & VA_DISPLAY_ATTRIB_SETTABLE))
        return VO_NOTIMPL;

    int r = mp_eq_to_va(attr, value);
    if (r == INT_MIN)
        return VO_NOTIMPL;

    attr->value = r;
    p->mp_display_attr[index] = value;

    MP_VERBOSE(p, "Changing '%s' (range [%d, %d]) to %d\n", name,
               attr->max_value, attr->min_value, attr->value);

    va_lock(p->mpvaapi);
    status = vaSetDisplayAttributes(p->display, attr, 1);
    va_unlock(p->mpvaapi);
    if (!CHECK_VA_STATUS(p, "vaSetDisplayAttributes()"))
        return VO_FALSE;
    return VO_TRUE;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;

    switch (request) {
    case VOCTRL_GET_DEINTERLACE:
        if (!p->deint_type)
            break;
        *(int*)data = !!p->deint;
        return VO_TRUE;
    case VOCTRL_SET_DEINTERLACE:
        if (!p->deint_type)
            break;
        p->deint = *(int*)data ? p->deint_type : 0;
        return VO_TRUE;
    case VOCTRL_GET_HWDEC_INFO: {
        struct mp_hwdec_info **arg = data;
        *arg = &p->hwdec_info;
        return true;
    }
    case VOCTRL_SET_EQUALIZER: {
        struct voctrl_set_equalizer_args *eq = data;
        return set_equalizer(p, eq->name, eq->value);
    }
    case VOCTRL_GET_EQUALIZER: {
        struct voctrl_get_equalizer_args *eq = data;
        return get_equalizer(p, eq->name, eq->valueptr);
    }
    case VOCTRL_REDRAW_FRAME:
        p->output_surface = p->visible_surface;
        draw_osd(vo);
        return true;
    case VOCTRL_GET_PANSCAN:
        return VO_TRUE;
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

    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct vaapi_osd_part *part = &p->osd_parts[n];
        free_subpicture(p, &part->image);
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

    p->display = vaGetDisplay(vo->x11->display);
    if (!p->display)
        goto fail;

    p->mpvaapi = va_initialize(p->display, p->log);
    if (!p->mpvaapi) {
        vaTerminate(p->display);
        p->display = NULL;
        goto fail;
    }

    p->hwdec_info.hwctx = &p->mpvaapi->hwctx;

    if (va_guess_if_emulated(p->mpvaapi)) {
        MP_WARN(vo, "VA-API is most likely emulated via VDPAU.\n"
                    "It's better to use VDPAU directly with: --vo=vdpau\n");
    }

    p->pool = mp_image_pool_new(MAX_OUTPUT_SURFACES + 3);
    va_pool_set_allocator(p->pool, p->mpvaapi, VA_RT_FORMAT_YUV420);
    p->va_image_formats = p->mpvaapi->image_formats;

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
                   VA_STR_FOURCC(p->va_subpic_formats[i].fourcc),
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

    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct vaapi_osd_part *part = &p->osd_parts[n];
        part->image.image.image_id = VA_INVALID_ID;
        part->image.subpic_id = VA_INVALID_ID;
        part->conv_cache = talloc_steal(vo, osd_conv_cache_new());
    }

    int max_display_attrs = vaMaxNumDisplayAttributes(p->display);
    p->va_display_attrs = talloc_array(vo, VADisplayAttribute, max_display_attrs);
    if (p->va_display_attrs) {
        status = vaQueryDisplayAttributes(p->display, p->va_display_attrs,
                                          &p->va_num_display_attrs);
        if (!CHECK_VA_STATUS(p, "vaQueryDisplayAttributes()"))
            p->va_num_display_attrs = 0;
        p->mp_display_attr = talloc_zero_array(vo, int, p->va_num_display_attrs);
    }
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
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .scaling = VA_FILTER_SCALING_DEFAULT,
        .deint = 0,
#if !HAVE_VAAPI_VPP
        .deint_type = 2,
#endif
    },
    .options = (const struct m_option[]) {
#if USE_VAAPI_SCALING
        OPT_CHOICE("scaling", scaling, 0,
                   ({"default", VA_FILTER_SCALING_DEFAULT},
                    {"fast", VA_FILTER_SCALING_FAST},
                    {"hq", VA_FILTER_SCALING_HQ},
                    {"nla", VA_FILTER_SCALING_NL_ANAMORPHIC})),
#endif
        OPT_CHOICE("deint", deint_type, 0,
                   ({"no", 0},
                    {"first-field", 1},
                    {"bob", 2})),
        OPT_FLAG("scaled-osd", force_scaled_osd, 0),
        {0}
    },
};
