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
#include "spudec.h"


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

int sub_unicode=0;
int sub_utf8=0;
int sub_pos=100;
int sub_width_p=100;
int sub_visibility=1;
int sub_bg_color=0; /* subtitles background color */
int sub_bg_alpha=0;
int sub_justify=0;

subtitle* vo_sub=NULL;
char *subtitle_font_encoding = NULL;
float text_font_scale_factor = 3.5;
float osd_font_scale_factor = 4.0;
float subtitle_font_radius = 2.0;
float subtitle_font_thickness = 2.0;
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


static void osd_update_ext(struct osd_state *osd, struct mp_eosd_res res)
{
    struct mp_eosd_res old = osd->res;
    if (old.w != res.w || old.h != res.h || old.ml != res.ml || old.mt != res.mt
        || old.mr != res.mr || old.mb != res.mb)
    {
        osd->res = res;
        for (int n = 0; n < MAX_OSD_PARTS; n++)
            osd->objs[n]->force_redraw = true;
    }
}

void osd_update(struct osd_state *osd, int w, int h)
{
    osd_update_ext(osd, (struct mp_eosd_res) {.w = w, .h = h});
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

    // OSDTYPE_SPU is an odd case, because vf_ass.c can't render it.
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

// Return true if *out_imgs has been filled with valid values.
// Return false on format mismatch, or if nothing to be renderer.
static bool render_object(struct osd_state *osd, struct osd_object *obj,
                          struct sub_bitmaps *out_imgs,
                          struct sub_render_params *sub_params,
                          const bool formats[SUBBITMAP_COUNT])
{
    memset(out_imgs, 0x55, sizeof(*out_imgs));

    if (obj->type == OSDTYPE_SPU) {
        *out_imgs = (struct sub_bitmaps) {0};
        if (spu_visible(osd, obj))
            spudec_get_bitmap(vo_spudec, osd->res.w, osd->res.h, out_imgs);
        // Normal change-detection (sub. dec. calls vo_osd_changed(OSDTYPE_SPU))
        if (obj->force_redraw) {
            out_imgs->bitmap_id++;
            out_imgs->bitmap_pos_id++;
        }
    } else if (obj->type == OSDTYPE_SUB) {
        struct sub_render_params p = *sub_params;
        if (p.pts != MP_NOPTS_VALUE)
            p.pts += sub_delay - osd->sub_offset;

        p.support_rgba = formats[SUBBITMAP_RGBA];

        sub_get_bitmaps(osd, &p, out_imgs);
    } else {
        osd_object_get_bitmaps(osd, obj, out_imgs);
    }

    obj->force_redraw = false;
    obj->vo_bitmap_id += out_imgs->bitmap_id;
    obj->vo_bitmap_pos_id += out_imgs->bitmap_pos_id;

    if (out_imgs->num_parts == 0)
        return false;

    if (out_imgs->bitmap_id == 0 && out_imgs->bitmap_pos_id == 0
        && obj->cached.bitmap_id == obj->vo_bitmap_id
        && obj->cached.bitmap_pos_id == obj->vo_bitmap_pos_id
        && formats[obj->cached.format])
    {
        *out_imgs = obj->cached;
        return true;
    }

    out_imgs->render_index = obj->type;
    out_imgs->bitmap_id = obj->vo_bitmap_id;
    out_imgs->bitmap_pos_id = obj->vo_bitmap_pos_id;

    if (formats[out_imgs->format])
        return true;

    bool cached = false; // do we have a copy of all the image data?

    if ((formats[SUBBITMAP_OLD_PLANAR] || formats[SUBBITMAP_OLD])
        && out_imgs->format == SUBBITMAP_LIBASS)
    {
        cached |= osd_conv_ass_to_old_p(obj->cache[0], out_imgs);
    }

    if (formats[SUBBITMAP_OLD] && out_imgs->format == SUBBITMAP_OLD_PLANAR) {
        cached |= osd_conv_old_p_to_old(obj->cache[1], out_imgs);
    }

    if (formats[SUBBITMAP_RGBA] && out_imgs->format == SUBBITMAP_OLD_PLANAR) {
        cached |= osd_conv_old_p_to_rgba(obj->cache[2], out_imgs);
    }

    if (cached)
        obj->cached = *out_imgs;

    if (!formats[out_imgs->format]) {
        mp_msg(MSGT_OSD, MSGL_ERR, "Can't render OSD part %d (format %d).\n",
               obj->type, out_imgs->format);
        return false;
    }
    return true;
}

// This is a hack to render the first subtitle OSD object, which is not empty.
// It's a hack because it's really only useful for subtitles: normal OSD can
// have multiple objects, and rendering one object may invalidate data of a
// previously rendered, different object (that's how osd_libass.c works).
// Also, it assumes this is called from a filter: it disables VO rendering of
// subtitles, because we don't want to render both.
bool osd_draw_sub(struct osd_state *osd, struct sub_bitmaps *out_imgs,
                  struct sub_render_params *sub_params,
                  const bool formats[SUBBITMAP_COUNT])
{
    *out_imgs = (struct sub_bitmaps) {0};
    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct osd_object *obj = osd->objs[n];
        if (obj->is_sub) {
            // hack to allow rendering non-ASS subs with vf_ass inserted
            // (vf_ass is auto-inserted if VOs don't support EOSD)
#ifdef CONFIG_ASS
            osd->render_subs_in_filter = !!sub_get_ass_track(osd);
            if (!osd->render_subs_in_filter)
                return false;
#endif
            if (render_object(osd, obj, out_imgs, sub_params, formats))
                return true;
        }
    }
    return false;
}

void draw_osd_with_eosd(struct vo *vo, struct osd_state *osd)
{
    struct mp_eosd_res dim = {0};
    if (vo_control(vo, VOCTRL_GET_EOSD_RES, &dim) != VO_TRUE)
        return;

    bool formats[SUBBITMAP_COUNT];
    for (int n = 0; n < SUBBITMAP_COUNT; n++) {
        int data = n;
        formats[n] = vo_control(vo, VOCTRL_QUERY_EOSD_FORMAT, &data) == VO_TRUE;
    }

    osd_update_ext(osd, dim);

    struct aspect_data asp = vo->aspdat;

    struct sub_render_params subparams = {
        .pts = osd->vo_sub_pts,
        .dim = dim,
        .normal_scale = 1,
        .vsfilter_scale = (double) asp.prew / asp.preh * asp.orgh / asp.orgw,
        .support_rgba = formats[SUBBITMAP_RGBA],
    };

    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct osd_object *obj = osd->objs[n];
        if (obj->is_sub && osd->render_subs_in_filter)
            continue;
        struct sub_bitmaps imgs;
        if (render_object(osd, obj, &imgs, &subparams, formats))
            vo_control(vo, VOCTRL_DRAW_EOSD, &imgs);
    }
}

void osd_draw_text_ext(struct osd_state *osd, int w, int h,
                       int ml, int mt, int mr, int mb, int unused0, int unused1,
                       void (*draw_alpha)(void *ctx, int x0, int y0, int w,
                                          int h, unsigned char* src,
                                          unsigned char *srca,
                                          int stride),
                   void *ctx)
{
    struct mp_eosd_res dim =
        {.w = w, .h = h, .ml = ml, .mt = mt, .mr = mr, .mb = mb};
    osd_update_ext(osd, dim);
    struct sub_render_params subparams = {
        .pts = osd->vo_sub_pts,
        .dim = dim,
        .normal_scale = 1,
        .vsfilter_scale = 1, // unknown
    };
    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct osd_object *obj = osd->objs[n];
        if (obj->is_sub && osd->render_subs_in_filter)
            continue;
        struct sub_bitmaps imgs;
        bool formats[SUBBITMAP_COUNT] = {[SUBBITMAP_OLD_PLANAR] = true};
        if (render_object(osd, obj, &imgs, &subparams, formats)) {
            assert(imgs.num_parts == 1);
            struct sub_bitmap *part = &imgs.parts[0];
            struct old_osd_planar *bmp = part->bitmap;
            draw_alpha(ctx, part->x, part->y, part->w, part->h,
                        bmp->bitmap, bmp->alpha, part->stride);
        }
    }
}

void osd_draw_text(struct osd_state *osd, int w, int h,
                   void (*draw_alpha)(void *ctx, int x0, int y0, int w, int h,
                                      unsigned char* src, unsigned char *srca,
                                      int stride),
                   void *ctx)
{
    osd_draw_text_ext(osd, w, h, 0, 0, 0, 0, 0, 0, draw_alpha, ctx);
}

void vo_osd_changed(int new_value)
{
    struct osd_state *osd = global_osd;
    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        if (osd->objs[n]->type == new_value)
            osd->objs[n]->force_redraw = true;
    }
}

bool vo_osd_has_changed(struct osd_state *osd)
{
    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        if (osd->objs[n]->force_redraw)
            return true;
    }
    return false;
}

// Needed for VOs using the old OSD API (osd_draw_text_[ext]).
void vo_osd_reset_changed(void)
{
    struct osd_state *osd = global_osd;
    for (int n = 0; n < MAX_OSD_PARTS; n++)
        osd->objs[n]->force_redraw = false;
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
