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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <libavutil/common.h>

#include "common/common.h"

#include "stream/stream.h"

#include "osdep/timer.h"

#include "mpv_talloc.h"
#include "options/m_config.h"
#include "options/options.h"
#include "common/global.h"
#include "common/msg.h"
#include "player/client.h"
#include "player/command.h"
#include "osd.h"
#include "osd_state.h"
#include "dec_sub.h"
#include "img_convert.h"
#include "draw_bmp.h"
#include "video/mp_image.h"
#include "video/mp_image_pool.h"

#define OPT_BASE_STRUCT struct osd_style_opts
static const m_option_t style_opts[] = {
    OPT_STRING("font", font, 0),
    OPT_FLOATRANGE("font-size", font_size, 0, 1, 9000),
    OPT_COLOR("color", color, 0),
    OPT_COLOR("border-color", border_color, 0),
    OPT_COLOR("shadow-color", shadow_color, 0),
    OPT_COLOR("back-color", back_color, 0),
    OPT_FLOAT("border-size", border_size, 0),
    OPT_FLOAT("shadow-offset", shadow_offset, 0),
    OPT_FLOATRANGE("spacing", spacing, 0, -10, 10),
    OPT_INTRANGE("margin-x", margin_x, 0, 0, 300),
    OPT_INTRANGE("margin-y", margin_y, 0, 0, 600),
    OPT_CHOICE("align-x", align_x, 0,
               ({"left", -1}, {"center", 0}, {"right", +1})),
    OPT_CHOICE("align-y", align_y, 0,
               ({"top", -1}, {"center", 0}, {"bottom", +1})),
    OPT_FLOATRANGE("blur", blur, 0, 0, 20),
    OPT_FLAG("bold", bold, 0),
    OPT_FLAG("italic", italic, 0),
    OPT_CHOICE("justify", justify, 0,
               ({"auto", 0}, {"left", 1}, {"center", 2}, {"right", 3})),
    {0}
};

const struct m_sub_options osd_style_conf = {
    .opts = style_opts,
    .size = sizeof(struct osd_style_opts),
    .defaults = &(const struct osd_style_opts){
        .font = "sans-serif",
        .font_size = 55,
        .color = {255, 255, 255, 255},
        .border_color = {0, 0, 0, 255},
        .shadow_color = {240, 240, 240, 128},
        .border_size = 3,
        .shadow_offset = 0,
        .margin_x = 25,
        .margin_y = 22,
        .align_x = -1,
        .align_y = -1,
    },
    .change_flags = UPDATE_OSD,
};

const struct m_sub_options sub_style_conf = {
    .opts = style_opts,
    .size = sizeof(struct osd_style_opts),
    .defaults = &(const struct osd_style_opts){
        .font = "sans-serif",
        .font_size = 55,
        .color = {255, 255, 255, 255},
        .border_color = {0, 0, 0, 255},
        .shadow_color = {240, 240, 240, 128},
        .border_size = 3,
        .shadow_offset = 0,
        .margin_x = 25,
        .margin_y = 22,
        .align_x = 0,
        .align_y = 1,
    },
    .change_flags = UPDATE_OSD,
};

bool osd_res_equals(struct mp_osd_res a, struct mp_osd_res b)
{
    return a.w == b.w && a.h == b.h && a.ml == b.ml && a.mt == b.mt
        && a.mr == b.mr && a.mb == b.mb
        && a.display_par == b.display_par;
}

struct osd_state *osd_create(struct mpv_global *global)
{
    assert(MAX_OSD_PARTS >= OSDTYPE_COUNT);

    struct osd_state *osd = talloc_zero(NULL, struct osd_state);
    *osd = (struct osd_state) {
        .opts_cache = m_config_cache_alloc(osd, global, &mp_osd_render_sub_opts),
        .global = global,
        .log = mp_log_new(osd, global->log, "osd"),
        .force_video_pts = MP_NOPTS_VALUE,
    };
    pthread_mutex_init(&osd->lock, NULL);
    osd->opts = osd->opts_cache->opts;

    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct osd_object *obj = talloc(osd, struct osd_object);
        *obj = (struct osd_object) {
            .type = n,
            .text = talloc_strdup(obj, ""),
            .progbar_state = {.type = -1},
        };
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
    pthread_mutex_destroy(&osd->lock);
    talloc_free(osd);
}

void osd_set_text(struct osd_state *osd, const char *text)
{
    pthread_mutex_lock(&osd->lock);
    struct osd_object *osd_obj = osd->objs[OSDTYPE_OSD];
    if (!text)
        text = "";
    if (strcmp(osd_obj->text, text) != 0) {
        talloc_free(osd_obj->text);
        osd_obj->text = talloc_strdup(osd_obj, text);
        osd_obj->osd_changed = true;
        osd->want_redraw_notification = true;
    }
    pthread_mutex_unlock(&osd->lock);
}

void osd_set_sub(struct osd_state *osd, int index, struct dec_sub *dec_sub)
{
    pthread_mutex_lock(&osd->lock);
    if (index >= 0 && index < 2) {
        struct osd_object *obj = osd->objs[OSDTYPE_SUB + index];
        obj->sub = dec_sub;
        obj->vo_change_id += 1;
    }
    osd->want_redraw_notification = true;
    pthread_mutex_unlock(&osd->lock);
}

bool osd_get_render_subs_in_filter(struct osd_state *osd)
{
    pthread_mutex_lock(&osd->lock);
    bool r = osd->render_subs_in_filter;
    pthread_mutex_unlock(&osd->lock);
    return r;
}

void osd_set_render_subs_in_filter(struct osd_state *osd, bool s)
{
    pthread_mutex_lock(&osd->lock);
    osd->render_subs_in_filter = s;
    pthread_mutex_unlock(&osd->lock);
}

void osd_set_force_video_pts(struct osd_state *osd, double video_pts)
{
    pthread_mutex_lock(&osd->lock);
    osd->force_video_pts = video_pts;
    pthread_mutex_unlock(&osd->lock);
}

double osd_get_force_video_pts(struct osd_state *osd)
{
    pthread_mutex_lock(&osd->lock);
    double pts = osd->force_video_pts;
    pthread_mutex_unlock(&osd->lock);
    return pts;
}

void osd_set_progbar(struct osd_state *osd, struct osd_progbar_state *s)
{
    pthread_mutex_lock(&osd->lock);
    struct osd_object *osd_obj = osd->objs[OSDTYPE_OSD];
    osd_obj->progbar_state.type = s->type;
    osd_obj->progbar_state.value = s->value;
    osd_obj->progbar_state.num_stops = s->num_stops;
    MP_TARRAY_GROW(osd_obj, osd_obj->progbar_state.stops, s->num_stops);
    memcpy(osd_obj->progbar_state.stops, s->stops,
           sizeof(osd_obj->progbar_state.stops[0]) * s->num_stops);
    osd_obj->osd_changed = true;
    osd->want_redraw_notification = true;
    pthread_mutex_unlock(&osd->lock);
}

void osd_set_external2(struct osd_state *osd, struct sub_bitmaps *imgs)
{
    pthread_mutex_lock(&osd->lock);
    osd->objs[OSDTYPE_EXTERNAL2]->external2 = imgs;
    osd->objs[OSDTYPE_EXTERNAL2]->vo_change_id += 1;
    osd->want_redraw_notification = true;
    pthread_mutex_unlock(&osd->lock);
}

static void check_obj_resize(struct osd_state *osd, struct mp_osd_res res,
                             struct osd_object *obj)
{
    if (!osd_res_equals(res, obj->vo_res)) {
        obj->vo_res = res;
        mp_client_broadcast_event(mp_client_api_get_core(osd->global->client_api),
                                  MP_EVENT_WIN_RESIZE, NULL);
    }
}

// Optional. Can be called for faster reaction of OSD-generating scripts like
// osc.lua. This can achieve that the resize happens first, so that the OSD is
// generated at the correct resolution the first time the resized frame is
// rendered. Since the OSD doesn't (and can't) wait for the script, this
// increases the time in which the script can react, and also gets rid of the
// unavoidable redraw delay (though it will still be racy).
// Unnecessary for anything else.
void osd_resize(struct osd_state *osd, struct mp_osd_res res)
{
    pthread_mutex_lock(&osd->lock);
    int types[] = {OSDTYPE_OSD, OSDTYPE_EXTERNAL, OSDTYPE_EXTERNAL2, -1};
    for (int n = 0; types[n] >= 0; n++)
        check_obj_resize(osd, res, osd->objs[types[n]]);
    pthread_mutex_unlock(&osd->lock);
}

static void render_object(struct osd_state *osd, struct osd_object *obj,
                          struct mp_osd_res res, double video_pts,
                          const bool sub_formats[SUBBITMAP_COUNT],
                          struct sub_bitmaps *out_imgs)
{
    int format = SUBBITMAP_LIBASS;
    if (!sub_formats[format] || osd->opts->force_rgba_osd)
        format = SUBBITMAP_RGBA;

    *out_imgs = (struct sub_bitmaps) {0};

    check_obj_resize(osd, res, obj);

    if (obj->type == OSDTYPE_SUB || obj->type == OSDTYPE_SUB2) {
        if (obj->sub)
            sub_get_bitmaps(obj->sub, obj->vo_res, format, video_pts, out_imgs);
    } else if (obj->type == OSDTYPE_EXTERNAL2) {
        if (obj->external2 && obj->external2->format) {
            *out_imgs = *obj->external2;
            obj->external2->change_id = 0;
        }
    } else {
        osd_object_get_bitmaps(osd, obj, format, out_imgs);
    }

    obj->vo_change_id += out_imgs->change_id;

    if (out_imgs->num_parts == 0)
        return;

    out_imgs->render_index = obj->type;
    out_imgs->change_id = obj->vo_change_id;
}

// draw_flags is a bit field of OSD_DRAW_* constants
void osd_draw(struct osd_state *osd, struct mp_osd_res res,
              double video_pts, int draw_flags,
              const bool formats[SUBBITMAP_COUNT],
              void (*cb)(void *ctx, struct sub_bitmaps *imgs), void *cb_ctx)
{
    pthread_mutex_lock(&osd->lock);

    if (osd->force_video_pts != MP_NOPTS_VALUE)
        video_pts = osd->force_video_pts;

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
        if ((draw_flags & OSD_DRAW_OSD_ONLY) && obj->is_sub)
            continue;

        if (obj->sub)
            sub_lock(obj->sub);

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

        if (obj->sub)
            sub_unlock(obj->sub);
    }

    // If this is called with OSD_DRAW_SUB_ONLY or OSD_DRAW_OSD_ONLY set, assume
    // it will always draw the complete OSD by doing multiple osd_draw() calls.
    // OSD_DRAW_SUB_FILTER on the other hand is an evil special-case, and we
    // must not reset the flag when it happens.
    if (!(draw_flags & OSD_DRAW_SUB_FILTER))
        osd->want_redraw_notification = false;

    pthread_mutex_unlock(&osd->lock);
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
    if (!mp_image_pool_make_writeable(closure->pool, closure->dest))
        return; // on OOM, skip
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

// Setup the OSD resolution to render into an image with the given parameters.
// The interesting part about this is that OSD has to compensate the aspect
// ratio if the image does not have a 1:1 pixel aspect ratio.
struct mp_osd_res osd_res_from_image_params(const struct mp_image_params *p)
{
    return (struct mp_osd_res) {
        .w = p->w,
        .h = p->h,
        .display_par = p->p_h / (double)p->p_w,
    };
}

// Typically called to react to OSD style changes.
void osd_changed(struct osd_state *osd)
{
    pthread_mutex_lock(&osd->lock);
    osd->objs[OSDTYPE_OSD]->osd_changed = true;
    osd->want_redraw_notification = true;
    // Done here for a lack of a better place.
    m_config_cache_update(osd->opts_cache);
    pthread_mutex_unlock(&osd->lock);
}

bool osd_query_and_reset_want_redraw(struct osd_state *osd)
{
    pthread_mutex_lock(&osd->lock);
    bool r = osd->want_redraw_notification;
    osd->want_redraw_notification = false;
    pthread_mutex_unlock(&osd->lock);
    return r;
}

struct mp_osd_res osd_get_vo_res(struct osd_state *osd)
{
    pthread_mutex_lock(&osd->lock);
    // Any OSDTYPE is fine; but it mustn't be a subtitle one (can have lower res.)
    struct mp_osd_res res = osd->objs[OSDTYPE_OSD]->vo_res;
    pthread_mutex_unlock(&osd->lock);
    return res;
}

// Position the subbitmaps in imgs on the screen. Basically, this fits the
// subtitle canvas (of size frame_w x frame_h) onto the screen, such that it
// fills the whole video area (especially if the video is magnified, e.g. on
// fullscreen). If compensate_par is >0, adjust the way the subtitles are
// "stretched" on the screen, and letter-box the result. If compensate_par
// is <0, strictly letter-box the subtitles. If it is 0, stretch them.
void osd_rescale_bitmaps(struct sub_bitmaps *imgs, int frame_w, int frame_h,
                         struct mp_osd_res res, double compensate_par)
{
    int vidw = res.w - res.ml - res.mr;
    int vidh = res.h - res.mt - res.mb;
    double xscale = (double)vidw / frame_w;
    double yscale = (double)vidh / frame_h;
    if (compensate_par < 0)
        compensate_par = xscale / yscale / res.display_par;
    if (compensate_par > 0)
        xscale /= compensate_par;
    int cx = vidw / 2 - (int)(frame_w * xscale) / 2;
    int cy = vidh / 2 - (int)(frame_h * yscale) / 2;
    for (int i = 0; i < imgs->num_parts; i++) {
        struct sub_bitmap *bi = &imgs->parts[i];
        bi->x = (int)(bi->x * xscale) + cx + res.ml;
        bi->y = (int)(bi->y * yscale) + cy + res.mt;
        bi->dw = (int)(bi->w * xscale + 0.5);
        bi->dh = (int)(bi->h * yscale + 0.5);
    }
}
