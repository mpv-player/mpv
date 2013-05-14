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

#include "core/mp_common.h"

#include "stream/stream.h"

#include "osdep/timer.h"

#include "talloc.h"
#include "core/options.h"
#include "core/mplayer.h"
#include "core/mp_msg.h"
#include "sub.h"
#include "dec_sub.h"
#include "img_convert.h"
#include "draw_bmp.h"
#include "spudec.h"
#include "subreader.h"
#include "video/mp_image.h"
#include "video/mp_image_pool.h"

int sub_pos=100;
int sub_visibility=1;

subtitle* vo_sub=NULL;

float sub_delay = 0;
float sub_fps = 0;

void *vo_spudec=NULL;
void *vo_vobsub=NULL;

static const struct osd_style_opts osd_style_opts_def = {
    .font = "Sans",
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

static struct osd_state *global_osd;

static bool osd_res_equals(struct mp_osd_res a, struct mp_osd_res b)
{
    return a.w == b.w && a.h == b.h && a.ml == b.ml && a.mt == b.mt
        && a.mr == b.mr && a.mb == b.mb
        && a.display_par == b.display_par
        && a.video_par == b.video_par;
}

struct osd_state *osd_create(struct MPOpts *opts, struct ass_library *asslib)
{
    struct osd_state *osd = talloc_zero(NULL, struct osd_state);
    *osd = (struct osd_state) {
        .opts = opts,
        .ass_library = asslib,
        .osd_text = talloc_strdup(osd, ""),
        .progbar_type = -1,
    };

    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct osd_object *obj = talloc_struct(osd, struct osd_object, {
            .type = n,
        });
        for (int i = 0; i < OSD_CONV_CACHE_MAX; i++)
            obj->cache[i] = talloc_steal(obj, osd_conv_cache_new());
        osd->objs[n] = obj;
    }

    osd->objs[OSDTYPE_SPU]->is_sub = true;      // spudec.c
    osd->objs[OSDTYPE_SUB]->is_sub = true;      // dec_sub.c
    osd->objs[OSDTYPE_SUBTITLE]->is_sub = true; // osd_libass.c

    osd_init_backend(osd);
    global_osd = osd;
    return osd;
}

void osd_free(struct osd_state *osd)
{
    if (!osd)
        return;
    osd_destroy_backend(osd);
    talloc_free(osd);
    global_osd = NULL;
}

void osd_set_text(struct osd_state *osd, const char *text)
{
    if (!text)
        text = "";
    if (strcmp(osd->osd_text, text) == 0)
        return;
    talloc_free(osd->osd_text);
    osd->osd_text = talloc_strdup(osd, text);
    vo_osd_changed(OSDTYPE_OSD);
}

static bool spu_visible(struct osd_state *osd, struct osd_object *obj)
{
    struct MPOpts *opts = osd->opts;
    return opts->sub_visibility && vo_spudec && spudec_visible(vo_spudec);
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

    if (obj->type == OSDTYPE_SPU) {
        if (spu_visible(osd, obj))
            spudec_get_indexed(vo_spudec, &obj->vo_res, out_imgs);
    } else if (obj->type == OSDTYPE_SUB) {
        double sub_pts = video_pts;
        if (sub_pts != MP_NOPTS_VALUE)
            sub_pts += sub_delay - osd->sub_offset;
        sub_get_bitmaps(osd, obj->vo_res, sub_pts, out_imgs);
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

    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct osd_object *obj = osd->objs[n];

        // Object is drawn into the video frame itself; don't draw twice
        if (osd->render_subs_in_filter && obj->is_sub &&
            !(draw_flags & OSD_DRAW_SUB_FILTER))
            continue;
        if ((draw_flags & OSD_DRAW_SUB_ONLY) && !obj->is_sub)
            continue;

        struct sub_bitmaps imgs;
        render_object(osd, obj, res, video_pts, formats, &imgs);
        if (imgs.num_parts > 0) {
            if (formats[imgs.format]) {
                cb(cb_ctx, &imgs);
            } else {
                mp_msg(MSGT_OSD, MSGL_ERR,
                       "Can't render OSD part %d (format %d).\n",
                       obj->type, imgs.format);
            }
        }
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

void vo_osd_changed(int new_value)
{
    struct osd_state *osd = global_osd;
    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        if (osd->objs[n]->type == new_value)
            osd->objs[n]->force_redraw = true;
    }
    osd->want_redraw = true;
}

void osd_changed_all(struct osd_state *osd)
{
    for (int n = 0; n < MAX_OSD_PARTS; n++)
        vo_osd_changed(n);
}
