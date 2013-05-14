/*
 * This file is part of mplayer2.
 *
 * mplayer2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer2.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <libavutil/common.h>

#include "config.h"

#include "talloc.h"
#include "core/mp_common.h"
#include "core/mp_msg.h"
#include "sub.h"

static const char osd_font_pfb[] =
#include "sub/osd_font.h"
;

#include "sub/ass_mp.h"
#include "core/mp_core.h"


// NOTE: \fs-5 to reduce the size of the symbols in relation to normal text.
//       Done because libass doesn't center characters that are too high.
#define ASS_USE_OSD_FONT "{\\fnOSD\\fs-5}"

void osd_init_backend(struct osd_state *osd)
{
    osd->osd_ass_library = mp_ass_init(osd->opts);
    ass_add_font(osd->osd_ass_library, "OSD", (void *)osd_font_pfb,
                 sizeof(osd_font_pfb) - 1);

    osd->osd_render = ass_renderer_init(osd->osd_ass_library);
    mp_ass_configure_fonts(osd->osd_render, osd->opts->osd_style);
    ass_set_aspect_ratio(osd->osd_render, 1.0, 1.0);
}

void osd_destroy_backend(struct osd_state *osd)
{
    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct osd_object *obj = osd->objs[n];
        if (obj->osd_track)
            ass_free_track(obj->osd_track);
        obj->osd_track = NULL;
    }
    if (osd->osd_render)
        ass_renderer_done(osd->osd_render);
    osd->osd_render = NULL;
    ass_library_done(osd->osd_ass_library);
    osd->osd_ass_library = NULL;
}

static void create_osd_ass_track(struct osd_state *osd, struct osd_object *obj)
{
    ASS_Track *track = obj->osd_track;
    if (!track)
        track = ass_new_track(osd->osd_ass_library);

    double aspect = 1.0 * obj->vo_res.w / FFMAX(obj->vo_res.h, 1) /
                    obj->vo_res.display_par;

    track->track_type = TRACK_TYPE_ASS;
    track->Timer = 100.;
    track->PlayResY = MP_ASS_FONT_PLAYRESY;
    track->PlayResX = track->PlayResY * aspect;
    track->WrapStyle = 1; // end-of-line wrapping instead of smart wrapping

    if (track->n_styles == 0) {
        track->Kerning = true;
        int sid = ass_alloc_style(track);
        track->default_style = sid;
        ASS_Style *style = track->styles + sid;
        style->Alignment = 5; // top-title, left
        style->Name = strdup("OSD");
        mp_ass_set_style(style, osd->opts->osd_style);
        // Set to neutral base direction, as opposed to VSFilter LTR default
        style->Encoding = -1;
    }

    obj->osd_track = track;
}

static ASS_Event *add_osd_ass_event(ASS_Track *track, const char *text)
{
    int n = ass_alloc_event(track);
    ASS_Event *event = track->events + n;
    event->Start = 0;
    event->Duration = 100;
    event->Style = track->default_style;
    assert(event->Text == NULL);
    if (text)
        event->Text = strdup(text);
    return event;
}

static void clear_obj(struct osd_object *obj)
{
    if (obj->osd_track)
        ass_flush_events(obj->osd_track);
}

void osd_get_function_sym(char *buffer, size_t buffer_size, int osd_function)
{
    // 0xFF is never valid UTF-8, so we can use it to escape OSD symbols.
    snprintf(buffer, buffer_size, "\xFF%c", osd_function);
}

static char *mangle_ass(const char *in)
{
    char *res = talloc_strdup(NULL, "");
    while (*in) {
        // As used by osd_get_function_sym().
        if (in[0] == '\xFF' && in[1]) {
            res = talloc_strdup_append_buffer(res, ASS_USE_OSD_FONT);
            res = mp_append_utf8_buffer(res, OSD_CODEPOINTS + in[1]);
            res = talloc_strdup_append_buffer(res, "{\\r}");
            in += 2;
            continue;
        }
        if (*in == '{')
            res = talloc_strdup_append_buffer(res, "\\");
        res = talloc_strndup_append_buffer(res, in, 1);
        // Break ASS escapes with U+2060 WORD JOINER
        if (*in == '\\')
            res = mp_append_utf8_buffer(res, 0x2060);
        in++;
    }
    return res;
}

static void update_osd(struct osd_state *osd, struct osd_object *obj)
{
    struct MPOpts *opts = osd->opts;

    create_osd_ass_track(osd, obj);
    clear_obj(obj);
    if (!osd->osd_text[0])
        return;

    struct osd_style_opts font = *opts->osd_style;
    font.font_size *= opts->osd_scale;

    ASS_Style *style = obj->osd_track->styles + obj->osd_track->default_style;
    mp_ass_set_style(style, &font);

    char *text = mangle_ass(osd->osd_text);
    add_osd_ass_event(obj->osd_track, text);
    talloc_free(text);
}

// align: -1 .. +1
// frame: size of the containing area
// obj: size of the object that should be positioned inside the area
// margin: min. distance from object to frame (as long as -1 <= align <= +1)
static float get_align(float align, float frame, float obj, float margin)
{
    frame -= margin * 2;
    return margin + frame / 2 - obj / 2 + (frame - obj) / 2 * align;
}

struct ass_draw {
    int scale;
    char *text;
};

static void ass_draw_start(struct ass_draw *d)
{
    d->scale = FFMAX(d->scale, 1);
    d->text = talloc_asprintf_append(d->text, "{\\p%d}", d->scale);
}

static void ass_draw_stop(struct ass_draw *d)
{
    d->text = talloc_strdup_append(d->text, "{\\p0}");
}

static void ass_draw_c(struct ass_draw *d, float x, float y)
{
    int ix = round(x * (1 << (d->scale - 1)));
    int iy = round(y * (1 << (d->scale - 1)));
    d->text = talloc_asprintf_append(d->text, " %d %d", ix, iy);
}

static void ass_draw_append(struct ass_draw *d, const char *t)
{
    d->text = talloc_strdup_append(d->text, t);
}

static void ass_draw_move_to(struct ass_draw *d, float x, float y)
{
    ass_draw_append(d, " m");
    ass_draw_c(d, x, y);
}

static void ass_draw_line_to(struct ass_draw *d, float x, float y)
{
    ass_draw_append(d, " l");
    ass_draw_c(d, x, y);
}

static void ass_draw_rect_ccw(struct ass_draw *d, float x0, float y0,
                              float x1, float y1)
{
    ass_draw_move_to(d, x0, y0);
    ass_draw_line_to(d, x0, y1);
    ass_draw_line_to(d, x1, y1);
    ass_draw_line_to(d, x1, y0);
}

static void ass_draw_rect_cw(struct ass_draw *d, float x0, float y0,
                             float x1, float y1)
{
    ass_draw_move_to(d, x0, y0);
    ass_draw_line_to(d, x1, y0);
    ass_draw_line_to(d, x1, y1);
    ass_draw_line_to(d, x0, y1);
}

static void ass_draw_reset(struct ass_draw *d)
{
    talloc_free(d->text);
    d->text = NULL;
}

static void get_osd_bar_box(struct osd_state *osd, struct osd_object *obj,
                            float *o_x, float *o_y, float *o_w, float *o_h,
                            float *o_border)
{
    struct MPOpts *opts = osd->opts;

    bool new_track = !obj->osd_track;
    create_osd_ass_track(osd, obj);
    ASS_Track *track = obj->osd_track;
    ASS_Style *style = track->styles + track->default_style;

    *o_w = track->PlayResX * (opts->osd_bar_w / 100.0);
    *o_h = track->PlayResY * (opts->osd_bar_h / 100.0);

    if (new_track) {
        float base_size = 0.03125;
        style->Outline *= *o_h / track->PlayResY / base_size;
        // So that the chapter marks have space between them
        style->Outline = FFMIN(style->Outline, *o_h / 5.2);
        // So that the border is not 0
        style->Outline = FFMAX(style->Outline, *o_h / 32.0);
        // Rendering with shadow is broken (because there's more than one shape)
        style->Shadow = 0;
    }

    *o_border = style->Outline;

    *o_x = get_align(opts->osd_bar_align_x, track->PlayResX, *o_w, *o_border);
    *o_y = get_align(opts->osd_bar_align_y, track->PlayResY, *o_h, *o_border);
}

static void update_progbar(struct osd_state *osd, struct osd_object *obj)
{
    float px, py, width, height, border;
    get_osd_bar_box(osd, obj, &px, &py, &width, &height, &border);

    clear_obj(obj);

    if (osd->progbar_type < 0)
        return;

    float sx = px - border * 2 - height / 4; // includes additional spacing
    float sy = py + height / 2;

    char *text = talloc_asprintf(NULL, "{\\an6\\pos(%f,%f)}", sx, sy);

    if (osd->progbar_type == 0 || osd->progbar_type >= 256) {
        // no sym
    } else if (osd->progbar_type >= 32) {
        text = mp_append_utf8_buffer(text, osd->progbar_type);
    } else {
        text = talloc_strdup_append_buffer(text, ASS_USE_OSD_FONT);
        text = mp_append_utf8_buffer(text, OSD_CODEPOINTS + osd->progbar_type);
        text = talloc_strdup_append_buffer(text, "{\\r}");
    }

    add_osd_ass_event(obj->osd_track, text);
    talloc_free(text);

    struct ass_draw *d = &(struct ass_draw) { .scale = 4 };
    // filled area
    d->text = talloc_asprintf_append(d->text, "{\\bord0\\pos(%f,%f)}", px, py);
    ass_draw_start(d);
    float pos = osd->progbar_value * width - border / 2;
    ass_draw_rect_cw(d, 0, 0, pos, height);
    ass_draw_stop(d);
    add_osd_ass_event(obj->osd_track, d->text);
    ass_draw_reset(d);

    // position marker
    d->text = talloc_asprintf_append(d->text, "{\\bord%f\\pos(%f,%f)}",
                                     border / 2, px, py);
    ass_draw_start(d);
    ass_draw_move_to(d, pos + border / 2, 0);
    ass_draw_line_to(d, pos + border / 2, height);
    ass_draw_stop(d);
    add_osd_ass_event(obj->osd_track, d->text);
    ass_draw_reset(d);

    d->text = talloc_asprintf_append(d->text, "{\\pos(%f,%f)}", px, py);
    ass_draw_start(d);

    // the box
    ass_draw_rect_cw(d, -border, -border, width + border, height + border);

    // the "hole"
    ass_draw_rect_ccw(d, 0, 0, width, height);

    // chapter marks
    for (int n = 0; n < osd->progbar_num_stops; n++) {
        float s = osd->progbar_stops[n] * width;
        float dent = border * 1.3;

        if (s > dent && s < width - dent) {
            ass_draw_move_to(d, s + dent, 0);
            ass_draw_line_to(d, s,        dent);
            ass_draw_line_to(d, s - dent, 0);

            ass_draw_move_to(d, s - dent, height);
            ass_draw_line_to(d, s,        height - dent);
            ass_draw_line_to(d, s + dent, height);
        }
    }

    ass_draw_stop(d);
    add_osd_ass_event(obj->osd_track, d->text);
    ass_draw_reset(d);
}

static void update_sub(struct osd_state *osd, struct osd_object *obj)
{
    struct MPOpts *opts = osd->opts;

    clear_obj(obj);

    if (!(vo_sub && opts->sub_visibility))
        return;

    if (!obj->osd_track)
        obj->osd_track = mp_ass_default_track(osd->osd_ass_library, osd->opts);

    struct osd_style_opts font = *opts->sub_text_style;
    font.font_size *= opts->sub_scale;

    ASS_Style *style = obj->osd_track->styles + obj->osd_track->default_style;
    mp_ass_set_style(style, &font);

#if LIBASS_VERSION >= 0x01010000
    ass_set_line_position(osd->osd_render, 100 - sub_pos);
#endif

    char *text = talloc_strdup(NULL, "");

    for (int n = 0; n < vo_sub->lines; n++)
        text = talloc_asprintf_append_buffer(text, "%s\n", vo_sub->text[n]);

    char *escaped_text = mangle_ass(text);
    add_osd_ass_event(obj->osd_track, escaped_text);
    talloc_free(escaped_text);
    talloc_free(text);
}

static void update_object(struct osd_state *osd, struct osd_object *obj)
{
    switch (obj->type) {
    case OSDTYPE_OSD:
        update_osd(osd, obj);
        break;
    case OSDTYPE_SUBTITLE:
        update_sub(osd, obj);
        break;
    case OSDTYPE_PROGBAR:
        update_progbar(osd, obj);
        break;
    }
}

void osd_object_get_bitmaps(struct osd_state *osd, struct osd_object *obj,
                            struct sub_bitmaps *out_imgs)
{
    if (obj->force_redraw)
        update_object(osd, obj);

    *out_imgs = (struct sub_bitmaps) {0};
    if (!obj->osd_track)
        return;

    ass_set_frame_size(osd->osd_render, obj->vo_res.w, obj->vo_res.h);
    ass_set_aspect_ratio(osd->osd_render, obj->vo_res.display_par, 1.0);
    mp_ass_render_frame(osd->osd_render, obj->osd_track, 0,
                        &obj->parts_cache, out_imgs);
    talloc_steal(obj, obj->parts_cache);
}
