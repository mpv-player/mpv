/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <libavutil/common.h>

#include "common/common.h"

#include "stream/stream.h"

#include "osdep/timer.h"

#include "talloc.h"
#include "options/options.h"
#include "common/global.h"
#include "common/msg.h"
#include "osd.h"
#include "dec_sub.h"
#include "img_convert.h"
#include "draw_bmp.h"
#include "video/mp_image.h"
#include "video/mp_image_pool.h"

static const struct osd_style_opts osd_style_opts_def = {
    .font = "sans-serif",
    .font_size = 45,
    .color = {255, 255, 255, 255},
    .border_color = {0, 0, 0, 255},
    .shadow_color = {240, 240, 240, 128},
    .border_size = 2.5,
    .shadow_offset = 0,
    .margin_x = 25,
    .margin_y = 10,
};

#define OPT_BASE_STRUCT struct osd_style_opts
const struct m_sub_options osd_style_conf = {
    .opts = (m_option_t[]) {
        OPT_STRING("font", font, 0),
        OPT_FLOATRANGE("font-size", font_size, 0, 1, 9000),
        OPT_COLOR("color", color, 0),
        OPT_COLOR("border-color", border_color, 0),
        OPT_COLOR("shadow-color", shadow_color, 0),
        OPT_COLOR("back-color", back_color, 0),
        OPT_FLOATRANGE("border-size", border_size, 0, 0, 10),
        OPT_FLOATRANGE("shadow-offset", shadow_offset, 0, 0, 10),
        OPT_FLOATRANGE("spacing", spacing, 0, -10, 10),
        OPT_INTRANGE("margin-x", margin_x, 0, 0, 300),
        OPT_INTRANGE("margin-y", margin_y, 0, 0, 600),
        OPT_FLOATRANGE("blur", blur, 0, 0, 20),
        {0}
    },
    .size = sizeof(struct osd_style_opts),
    .defaults = &osd_style_opts_def,
};

static bool osd_res_equals(struct mp_osd_res a, struct mp_osd_res b)
{
    return a.w == b.w && a.h == b.h && a.ml == b.ml && a.mt == b.mt
        && a.mr == b.mr && a.mb == b.mb
        && a.display_par == b.display_par;
}

struct osd_state *osd_create(struct mpv_global *global)
{
    struct osd_state *osd = talloc_zero(NULL, struct osd_state);
    *osd = (struct osd_state) {
        .opts = global->opts,
        .global = global,
        .log = mp_log_new(osd, global->log, "osd"),
        .osd_text = talloc_strdup(osd, ""),
        .progbar_type = -1,
    };

    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct osd_object *obj = talloc(osd, struct osd_object);
        *obj = (struct osd_object) {
            .type = n,
            .sub_text = talloc_strdup(obj, ""),
        };
        for (int i = 0; i < OSD_CONV_CACHE_MAX; i++)
            obj->cache[i] = talloc_steal(obj, osd_conv_cache_new());
        osd->objs[n] = obj;
    }

    osd->objs[OSDTYPE_SUB]->is_sub = true;
    osd->objs[OSDTYPE_SUB2]->is_sub = true;

    osd_init_backend(osd);
    return osd;
}

void osd_free(struct osd_state *osd)
{
    if (!osd)
        return;
    osd_destroy_backend(osd);
    talloc_free(osd);
}

static bool set_text(void *talloc_ctx, char **var, const char *text)
{
    if (!text)
        text = "";
    if (strcmp(*var, text) == 0)
        return true;
    talloc_free(*var);
    *var = talloc_strdup(talloc_ctx, text);
    return false;
}

void osd_set_text(struct osd_state *osd, const char *text)
{
    if (!set_text(osd, &osd->osd_text, text))
        osd_changed(osd, OSDTYPE_OSD);
}

void osd_set_sub(struct osd_state *osd, struct osd_object *obj, const char *text)
{
    if (!set_text(obj, &obj->sub_text, text))
        osd_changed(osd, obj->type);
}

static void render_object(struct osd_state *osd, struct osd_object *obj,
                          struct mp_osd_res res, double video_pts,
                          const bool sub_formats[SUBBITMAP_COUNT],
                          struct sub_bitmaps *out_imgs)
{
    struct MPOpts *opts = osd->opts;

    bool formats[SUBBITMAP_COUNT];
    memcpy(formats, sub_formats, sizeof(formats));
    if (opts->force_rgba_osd)
        formats[SUBBITMAP_LIBASS] = false;

    *out_imgs = (struct sub_bitmaps) {0};

    if (!osd_res_equals(res, obj->vo_res))
        obj->force_redraw = true;
    obj->vo_res = res;

    if (obj->type == OSDTYPE_SUB || obj->type == OSDTYPE_SUB2) {
        if (obj->render_bitmap_subs && obj->dec_sub) {
            double sub_pts = video_pts;
            if (sub_pts != MP_NOPTS_VALUE)
                sub_pts -= obj->video_offset + opts->sub_delay;
            sub_get_bitmaps(obj->dec_sub, obj->vo_res, sub_pts, out_imgs);
        } else {
            osd_object_get_bitmaps(osd, obj, out_imgs);
        }
    } else if (obj->type == OSDTYPE_EXTERNAL2) {
        if (osd->external2.format) {
            *out_imgs = osd->external2;
            osd->external2.bitmap_id = osd->external2.bitmap_pos_id = 0;
        }
    } else if (obj->type == OSDTYPE_NAV_HIGHLIGHT) {
        mp_nav_get_highlight(osd, obj->vo_res, out_imgs);
    } else {
        osd_object_get_bitmaps(osd, obj, out_imgs);
    }

    if (obj->force_redraw) {
        out_imgs->bitmap_id++;
        out_imgs->bitmap_pos_id++;
    }

    obj->force_redraw = false;
    obj->vo_bitmap_id += out_imgs->bitmap_id;
    obj->vo_bitmap_pos_id += out_imgs->bitmap_pos_id;

    if (out_imgs->num_parts == 0)
        return;

    if (obj->cached.bitmap_id == obj->vo_bitmap_id
        && obj->cached.bitmap_pos_id == obj->vo_bitmap_pos_id
        && formats[obj->cached.format])
    {
        *out_imgs = obj->cached;
        return;
    }

    out_imgs->render_index = obj->type;
    out_imgs->bitmap_id = obj->vo_bitmap_id;
    out_imgs->bitmap_pos_id = obj->vo_bitmap_pos_id;

    if (formats[out_imgs->format])
        return;

    bool cached = false; // do we have a copy of all the image data?

    if (out_imgs->format == SUBBITMAP_INDEXED && opts->sub_gray)
        cached |= osd_conv_idx_to_gray(obj->cache[0], out_imgs);

    if (formats[SUBBITMAP_RGBA] && out_imgs->format == SUBBITMAP_INDEXED)
        cached |= osd_conv_idx_to_rgba(obj->cache[1], out_imgs);

    if (out_imgs->format == SUBBITMAP_RGBA && opts->sub_gauss != 0.0f)
        cached |= osd_conv_blur_rgba(obj->cache[2], out_imgs, opts->sub_gauss);

    // Do this conversion last to not trigger gauss blurring for ASS
    if (formats[SUBBITMAP_RGBA] && out_imgs->format == SUBBITMAP_LIBASS)
        cached |= osd_conv_ass_to_rgba(obj->cache[3], out_imgs);

    if (cached)
        obj->cached = *out_imgs;
}

// draw_flags is a bit field of OSD_DRAW_* constants
void osd_draw(struct osd_state *osd, struct mp_osd_res res,
              double video_pts, int draw_flags,
              const bool formats[SUBBITMAP_COUNT],
              void (*cb)(void *ctx, struct sub_bitmaps *imgs), void *cb_ctx)
{
    if (draw_flags & OSD_DRAW_SUB_FILTER)
        draw_flags |= OSD_DRAW_SUB_ONLY;

    if (!(draw_flags & OSD_DRAW_SUB_ONLY))
        osd->last_vo_res = res;

    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct osd_object *obj = osd->objs[n];

        // Object is drawn into the video frame itself; don't draw twice
        if (osd->render_subs_in_filter && obj->is_sub &&
            !(draw_flags & OSD_DRAW_SUB_FILTER))
            continue;
        if ((draw_flags & OSD_DRAW_SUB_ONLY) && !obj->is_sub)
            continue;

        if (obj->dec_sub)
            sub_lock(obj->dec_sub);

        struct sub_bitmaps imgs;
        render_object(osd, obj, res, video_pts, formats, &imgs);
        if (imgs.num_parts > 0) {
            if (formats[imgs.format]) {
                cb(cb_ctx, &imgs);
            } else {
                MP_ERR(osd, "Can't render OSD part %d (format %d).\n",
                       obj->type, imgs.format);
            }
        }

        if (obj->dec_sub)
            sub_unlock(obj->dec_sub);
    }
}

struct draw_on_image_closure {
    struct osd_state *osd;
    struct mp_image *dest;
    struct mp_image_pool *pool;
    bool changed;
};

static void draw_on_image(void *ctx, struct sub_bitmaps *imgs)
{
    struct draw_on_image_closure *closure = ctx;
    struct osd_state *osd = closure->osd;
    if (closure->pool) {
        mp_image_pool_make_writeable(closure->pool, closure->dest);
    } else {
        mp_image_make_writeable(closure->dest);
    }
    mp_draw_sub_bitmaps(&osd->draw_cache, closure->dest, imgs);
    talloc_steal(osd, osd->draw_cache);
    closure->changed = true;
}

// Calls mp_image_make_writeable() on the dest image if something is drawn.
// Returns whether anything was drawn.
bool osd_draw_on_image(struct osd_state *osd, struct mp_osd_res res,
                       double video_pts, int draw_flags, struct mp_image *dest)
{
    struct draw_on_image_closure closure = {osd, dest};
    osd_draw(osd, res, video_pts, draw_flags, mp_draw_sub_formats,
             &draw_on_image, &closure);
    return closure.changed;
}

// Like osd_draw_on_image(), but if dest needs to be copied to make it
// writeable, allocate images from the given pool. (This is a minor
// optimization to reduce "real" image sized memory allocations.)
void osd_draw_on_image_p(struct osd_state *osd, struct mp_osd_res res,
                         double video_pts, int draw_flags,
                         struct mp_image_pool *pool, struct mp_image *dest)
{
    struct draw_on_image_closure closure = {osd, dest, pool};
    osd_draw(osd, res, video_pts, draw_flags, mp_draw_sub_formats,
             &draw_on_image, &closure);
}

void osd_changed(struct osd_state *osd, int new_value)
{
    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        if (osd->objs[n]->type == new_value)
            osd->objs[n]->force_redraw = true;
    }
    osd->want_redraw = true;
}

void osd_changed_all(struct osd_state *osd)
{
    for (int n = 0; n < MAX_OSD_PARTS; n++)
        osd_changed(osd, n);
}

// Scale factor to translate OSD coordinates to what the obj uses internally.
// osd_coordinates * (sw, sh) = obj_coordinates
void osd_object_get_scale_factor(struct osd_state *osd, struct osd_object *obj,
                                 double *sw, double *sh)
{
    int nw, nh;
    osd_object_get_resolution(osd, obj, &nw, &nh);
    *sw = nw / (double)obj->vo_res.w;
    *sh = nh / (double)obj->vo_res.h;
}

// Turn *x and *y, which are given in OSD coordinates, to video coordinates.
// frame_w and frame_h give the dimensions of the original, unscaled video.
// (This gives correct results only after the OSD has been updated after a
//  resize or video reconfig.)
void osd_coords_to_video(struct osd_state *osd, int frame_w, int frame_h,
                         int *x, int *y)
{
    struct mp_osd_res res = osd->objs[OSDTYPE_OSD]->vo_res;
    int vidw = res.w - res.ml - res.mr;
    int vidh = res.h - res.mt - res.mb;
    double xscale = (double)vidw / frame_w;
    double yscale = (double)vidh / frame_h;
    // The OSD size + margins make up the scaled rectangle of the video.
    *x = (*x - res.ml) / xscale;
    *y = (*y - res.mt) / yscale;
}

// Position the subbitmaps in imgs on the screen. Basically, this fits the
// subtitle canvas (of size frame_w x frame_h) onto the screen, such that it
// fills the whole video area (especially if the video is magnified, e.g. on
// fullscreen). If compensate_par is given, adjust the way the subtitles are
// "stretched" on the screen, and letter-box the result.
void osd_rescale_bitmaps(struct sub_bitmaps *imgs, int frame_w, int frame_h,
                         struct mp_osd_res res, double compensate_par)
{
    int vidw = res.w - res.ml - res.mr;
    int vidh = res.h - res.mt - res.mb;
    double xscale = (double)vidw / frame_w;
    double yscale = (double)vidh / frame_h;
    if (compensate_par > 0) {
        if (compensate_par > 1.0) {
            xscale /= compensate_par;
        } else {
            yscale *= compensate_par;
        }
    }
    int cx = vidw / 2 - (int)(frame_w * xscale) / 2;
    int cy = vidh / 2 - (int)(frame_h * yscale) / 2;
    for (int i = 0; i < imgs->num_parts; i++) {
        struct sub_bitmap *bi = &imgs->parts[i];
        bi->x = bi->x * xscale + cx + res.ml;
        bi->y = bi->y * yscale + cy + res.mt;
        bi->dw = bi->w * xscale;
        bi->dh = bi->h * yscale;
    }
    imgs->scaled = xscale != 1 || yscale != 1;
}
