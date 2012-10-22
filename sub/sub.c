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

#include "config.h"

#include "stream/stream.h"

#include "osdep/timer.h"

#include "talloc.h"
#include "options.h"
#include "mplayer.h"
#include "mp_msg.h"
#include "libvo/video_out.h"
#include "sub.h"
#include "dec_sub.h"
#include "img_convert.h"
#include "draw_bmp.h"
#include "spudec.h"
#include "subreader.h"


char * const sub_osd_names[]={
    _("Seekbar"),
    _("Play"),
    _("Pause"),
    _("Stop"),
    _("Rewind"),
    _("Forward"),
    _("Clock"),
    _("Contrast"),
    _("Saturation"),
    _("Volume"),
    _("Brightness"),
    _("Hue"),
    _("Balance")
};
char * const sub_osd_names_short[] ={ "", "|>", "||", "[]", "<<" , ">>", "", "", "", "", "", "", "" };

int sub_utf8=0;
int sub_pos=100;
int sub_visibility=1;

subtitle* vo_sub=NULL;
float text_font_scale_factor = 3.5;
// 0 = no autoscale
// 1 = video height
// 2 = video width
// 3 = diagonal
int subtitle_autoscale = 3;

char *font_name = NULL;
char *sub_font_name = NULL;
float font_factor = 0.75;
float sub_delay = 0;
float sub_fps = 0;

void *vo_spudec=NULL;
void *vo_vobsub=NULL;

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
                          const bool formats[SUBBITMAP_COUNT],
                          struct sub_bitmaps *out_imgs)
{
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

    if (formats[SUBBITMAP_RGBA] && out_imgs->format == SUBBITMAP_INDEXED)
        cached |= osd_conv_idx_to_rgba(obj->cache[0], out_imgs);

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
    struct mp_csp_details *dest_csp;
    bool changed;
};

static void draw_on_image(void *ctx, struct sub_bitmaps *imgs)
{
    struct draw_on_image_closure *closure = ctx;
    struct osd_state *osd = closure->osd;
    mp_draw_sub_bitmaps(&osd->draw_cache, closure->dest, imgs,
                        closure->dest_csp);
    talloc_steal(osd, osd->draw_cache);
    closure->changed = true;
}

// Returns whether anything was drawn.
bool osd_draw_on_image(struct osd_state *osd, struct mp_osd_res res,
                       double video_pts, int draw_flags, struct mp_image *dest,
                       struct mp_csp_details *dest_csp)
{
    struct draw_on_image_closure closure = {osd, dest, dest_csp};
    osd_draw(osd, res, video_pts, draw_flags, mp_draw_sub_formats,
             &draw_on_image, &closure);
    return closure.changed;
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

bool sub_bitmaps_bb(struct sub_bitmaps *imgs, int *x1, int *y1,
                    int *x2, int *y2)
{
    *x1 = *y1 = INT_MAX;
    *x2 = *y2 = INT_MIN;
    for (int n = 0; n < imgs->num_parts; n++) {
        struct sub_bitmap *p = &imgs->parts[n];
        *x1 = FFMIN(*x1, p->x);
        *y1 = FFMIN(*y1, p->y);
        *x2 = FFMAX(*x2, p->x + p->dw);
        *y2 = FFMAX(*y2, p->y + p->dh);
    }

    // avoid degenerate bounding box if empty
    *x1 = FFMIN(*x1, *x2);
    *y1 = FFMIN(*y1, *y2);

    return *x1 < *x2 && *y1 < *y2;
}
