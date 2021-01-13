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
#include "common/stats.h"
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
    {"font", OPT_STRING(font)},
    {"font-size", OPT_FLOAT(font_size), M_RANGE(1, 9000)},
    {"color", OPT_COLOR(color)},
    {"border-color", OPT_COLOR(border_color)},
    {"shadow-color", OPT_COLOR(shadow_color)},
    {"back-color", OPT_COLOR(back_color)},
    {"border-size", OPT_FLOAT(border_size)},
    {"shadow-offset", OPT_FLOAT(shadow_offset)},
    {"spacing", OPT_FLOAT(spacing), M_RANGE(-10, 10)},
    {"margin-x", OPT_INT(margin_x), M_RANGE(0, 300)},
    {"margin-y", OPT_INT(margin_y), M_RANGE(0, 600)},
    {"align-x", OPT_CHOICE(align_x,
        {"left", -1}, {"center", 0}, {"right", +1})},
    {"align-y", OPT_CHOICE(align_y,
        {"top", -1}, {"center", 0}, {"bottom", +1})},
    {"blur", OPT_FLOAT(blur), M_RANGE(0, 20)},
    {"bold", OPT_FLAG(bold)},
    {"italic", OPT_FLAG(italic)},
    {"justify", OPT_CHOICE(justify,
        {"auto", 0}, {"left", 1}, {"center", 2}, {"right", 3})},
    {"font-provider", OPT_CHOICE(font_provider,
        {"auto", 0}, {"none", 1}, {"fontconfig", 2}), .flags = UPDATE_SUB_HARD},
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
        .stats = stats_ctx_create(osd, global, "osd"),
    };
    pthread_mutex_init(&osd->lock, NULL);
    osd->opts = osd->opts_cache->opts;

    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct osd_object *obj = talloc(osd, struct osd_object);
        *obj = (struct osd_object) {
            .type = n,
            .text = talloc_strdup(obj, ""),
            .progbar_state = {.type = -1},
            .vo_change_id = 1,
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
    talloc_free(osd->objs[OSDTYPE_EXTERNAL2]->external2);
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
    if (osd->render_subs_in_filter != s) {
        osd->render_subs_in_filter = s;

        int change_id = 0;
        for (int n = 0; n < MAX_OSD_PARTS; n++)
            change_id = MPMAX(change_id, osd->objs[n]->vo_change_id);
        for (int n = 0; n < MAX_OSD_PARTS; n++)
            osd->objs[n]->vo_change_id = change_id + 1;
    }
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
    if (s->num_stops) {
        memcpy(osd_obj->progbar_state.stops, s->stops,
               sizeof(osd_obj->progbar_state.stops[0]) * s->num_stops);
    }
    osd_obj->osd_changed = true;
    osd->want_redraw_notification = true;
    pthread_mutex_unlock(&osd->lock);
}

void osd_set_external2(struct osd_state *osd, struct sub_bitmaps *imgs)
{
    pthread_mutex_lock(&osd->lock);
    struct osd_object *obj = osd->objs[OSDTYPE_EXTERNAL2];
    talloc_free(obj->external2);
    obj->external2 = sub_bitmaps_copy(NULL, imgs);
    obj->vo_change_id += 1;
    osd->want_redraw_notification = true;
    pthread_mutex_unlock(&osd->lock);
}

static void check_obj_resize(struct osd_state *osd, struct mp_osd_res res,
                             struct osd_object *obj)
{
    if (!osd_res_equals(res, obj->vo_res)) {
        obj->vo_res = res;
        mp_client_broadcast_event_external(osd->global->client_api,
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

static struct sub_bitmaps *render_object(struct osd_state *osd,
                                         struct osd_object *obj,
                                         struct mp_osd_res osdres, double video_pts,
                                         const bool sub_formats[SUBBITMAP_COUNT])
{
    int format = SUBBITMAP_LIBASS;
    if (!sub_formats[format] || osd->opts->force_rgba_osd)
        format = SUBBITMAP_RGBA;

    struct sub_bitmaps *res = NULL;

    check_obj_resize(osd, osdres, obj);

    if (obj->type == OSDTYPE_SUB) {
        if (obj->sub)
            res = sub_get_bitmaps(obj->sub, obj->vo_res, format, video_pts);
    } else if (obj->type == OSDTYPE_SUB2) {
        if (obj->sub && sub_is_secondary_visible(obj->sub))
            res = sub_get_bitmaps(obj->sub, obj->vo_res, format, video_pts);
    } else if (obj->type == OSDTYPE_EXTERNAL2) {
        if (obj->external2 && obj->external2->format) {
            res = sub_bitmaps_copy(NULL, obj->external2); // need to be owner
            obj->external2->change_id = 0;
        }
    } else {
        res = osd_object_get_bitmaps(osd, obj, format);
    }

    if (obj->vo_had_output != !!res) {
        obj->vo_had_output = !!res;
        obj->vo_change_id += 1;
    }

    if (res) {
        obj->vo_change_id += res->change_id;

        res->render_index = obj->type;
        res->change_id = obj->vo_change_id;
    }

    return res;
}

// Render OSD to a list of bitmap and return it. The returned object is
// refcounted. Typically you should hold it only for a short time, and then
// release it.
// draw_flags is a bit field of OSD_DRAW_* constants
struct sub_bitmap_list *osd_render(struct osd_state *osd, struct mp_osd_res res,
                                   double video_pts, int draw_flags,
                                   const bool formats[SUBBITMAP_COUNT])
{
    pthread_mutex_lock(&osd->lock);

    struct sub_bitmap_list *list = talloc_zero(NULL, struct sub_bitmap_list);
    list->change_id = 1;
    list->w = res.w;
    list->h = res.h;

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

        char *stat_type_render = obj->is_sub ? "sub-render" : "osd-render";
        stats_time_start(osd->stats, stat_type_render);

        struct sub_bitmaps *imgs =
            render_object(osd, obj, res, video_pts, formats);

        stats_time_end(osd->stats, stat_type_render);

        if (imgs && imgs->num_parts > 0) {
            if (formats[imgs->format]) {
                talloc_steal(list, imgs);
                MP_TARRAY_APPEND(list, list->items, list->num_items, imgs);
                imgs = NULL;
            } else {
                MP_ERR(osd, "Can't render OSD part %d (format %d).\n",
                       obj->type, imgs->format);
            }
        }

        list->change_id += obj->vo_change_id;

        talloc_free(imgs);
    }

    // If this is called with OSD_DRAW_SUB_ONLY or OSD_DRAW_OSD_ONLY set, assume
    // it will always draw the complete OSD by doing multiple osd_draw() calls.
    // OSD_DRAW_SUB_FILTER on the other hand is an evil special-case, and we
    // must not reset the flag when it happens.
    if (!(draw_flags & OSD_DRAW_SUB_FILTER))
        osd->want_redraw_notification = false;

    pthread_mutex_unlock(&osd->lock);
    return list;
}

// Warning: this function should be considered legacy. Use osd_render() instead.
void osd_draw(struct osd_state *osd, struct mp_osd_res res,
              double video_pts, int draw_flags,
              const bool formats[SUBBITMAP_COUNT],
              void (*cb)(void *ctx, struct sub_bitmaps *imgs), void *cb_ctx)
{
    struct sub_bitmap_list *list =
        osd_render(osd, res, video_pts, draw_flags, formats);

    stats_time_start(osd->stats, "draw");

    for (int n = 0; n < list->num_items; n++)
        cb(cb_ctx, list->items[n]);

    stats_time_end(osd->stats, "draw");

    talloc_free(list);
}

// Calls mp_image_make_writeable() on the dest image if something is drawn.
// draw_flags as in osd_render().
void osd_draw_on_image(struct osd_state *osd, struct mp_osd_res res,
                       double video_pts, int draw_flags, struct mp_image *dest)
{
    osd_draw_on_image_p(osd, res, video_pts, draw_flags, NULL, dest);
}

// Like osd_draw_on_image(), but if dest needs to be copied to make it
// writeable, allocate images from the given pool. (This is a minor
// optimization to reduce "real" image sized memory allocations.)
void osd_draw_on_image_p(struct osd_state *osd, struct mp_osd_res res,
                         double video_pts, int draw_flags,
                         struct mp_image_pool *pool, struct mp_image *dest)
{
    struct sub_bitmap_list *list =
        osd_render(osd, res, video_pts, draw_flags, mp_draw_sub_formats);

    if (!list->num_items) {
        talloc_free(list);
        return;
    }

    if (!mp_image_pool_make_writeable(pool, dest))
        return; // on OOM, skip

    // Need to lock for the dumb osd->draw_cache thing.
    pthread_mutex_lock(&osd->lock);

    if (!osd->draw_cache)
        osd->draw_cache = mp_draw_sub_alloc(osd, osd->global);

    stats_time_start(osd->stats, "draw-bmp");

    if (!mp_draw_sub_bitmaps(osd->draw_cache, dest, list))
        MP_WARN(osd, "Failed rendering OSD.\n");
    talloc_steal(osd, osd->draw_cache);

    stats_time_end(osd->stats, "draw-bmp");

    pthread_mutex_unlock(&osd->lock);

    talloc_free(list);
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

// Copy *in and return a new allocation of it. Free with talloc_free(). This
// will contain a refcounted copy of the image data.
//
// in->packed must be set and must be a refcounted image, unless there is no
// data (num_parts==0).
//
//  p_cache: if not NULL, then this points to a struct sub_bitmap_copy_cache*
//           variable. The function may set this to an allocation and may later
//           read it. You have to free it with talloc_free() when done.
//  in: valid struct, or NULL (in this case it also returns NULL)
//  returns: new copy, or NULL if there was no data in the input
struct sub_bitmaps *sub_bitmaps_copy(struct sub_bitmap_copy_cache **p_cache,
                                     struct sub_bitmaps *in)
{
    if (!in || !in->num_parts)
        return NULL;

    struct sub_bitmaps *res = talloc(NULL, struct sub_bitmaps);
    *res = *in;

    // Note: the p_cache thing is a lie and unused.

    // The bitmaps being refcounted is essential for performance, and for
    // not invalidating in->parts[*].bitmap pointers.
    assert(in->packed && in->packed->bufs[0]);

    res->packed = mp_image_new_ref(res->packed);
    MP_HANDLE_OOM(res->packed);
    talloc_steal(res, res->packed);

    res->parts = NULL;
    MP_RESIZE_ARRAY(res, res->parts, res->num_parts);
    memcpy(res->parts, in->parts, sizeof(res->parts[0]) * res->num_parts);

    return res;
}
