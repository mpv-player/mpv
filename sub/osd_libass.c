/*
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <libavutil/common.h>

#include "config.h"

#include "talloc.h"
#include "misc/bstr.h"
#include "common/common.h"
#include "common/msg.h"
#include "osd.h"
#include "osd_state.h"

static const char osd_font_pfb[] =
#include "sub/osd_font.h"
;

#include "sub/ass_mp.h"
#include "options/options.h"


#define ASS_USE_OSD_FONT "{\\fnmpv-osd-symbols}"

void osd_init_backend(struct osd_state *osd)
{
}

static void create_ass_renderer(struct osd_state *osd, struct osd_object *obj)
{
    if (obj->osd_render)
        return;

    struct mp_log *ass_log = mp_log_new(obj, osd->log, "libass");
    obj->osd_ass_library = mp_ass_init(osd->global, ass_log);
    ass_add_font(obj->osd_ass_library, "mpv-osd-symbols", (void *)osd_font_pfb,
                 sizeof(osd_font_pfb) - 1);

    obj->osd_render = ass_renderer_init(obj->osd_ass_library);
    if (!obj->osd_render)
        abort();

    mp_ass_configure_fonts(obj->osd_render, osd->opts->osd_style,
                           osd->global, ass_log);
    ass_set_aspect_ratio(obj->osd_render, 1.0, 1.0);
}

void osd_destroy_backend(struct osd_state *osd)
{
    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct osd_object *obj = osd->objs[n];
        if (obj->osd_track)
            ass_free_track(obj->osd_track);
        obj->osd_track = NULL;
        if (obj->osd_render)
            ass_renderer_done(obj->osd_render);
        obj->osd_render = NULL;
        if (obj->osd_ass_library)
            ass_library_done(obj->osd_ass_library);
        obj->osd_ass_library = NULL;
    }
}

static void create_ass_track(struct osd_state *osd, struct osd_object *obj,
                             int res_x, int res_y)
{
    create_ass_renderer(osd, obj);

    ASS_Track *track = obj->osd_track;
    if (!track)
        track = ass_new_track(obj->osd_ass_library);

    int old_res_x = track->PlayResX;
    int old_res_y = track->PlayResY;

    double aspect = 1.0 * obj->vo_res.w / FFMAX(obj->vo_res.h, 1) /
                    obj->vo_res.display_par;

    track->track_type = TRACK_TYPE_ASS;
    track->Timer = 100.;
    track->PlayResY = res_y ? res_y : MP_ASS_FONT_PLAYRESY;
    track->PlayResX = res_x ? res_x : track->PlayResY * aspect;
    track->WrapStyle = 1; // end-of-line wrapping instead of smart wrapping
    track->Kerning = true;

    // Force libass to clear its internal cache - it doesn't check for
    // PlayRes changes itself.
    if (old_res_x != track->PlayResX || old_res_y != track->PlayResY)
        ass_set_frame_size(obj->osd_render, 1, 1);

    if (track->n_styles < 2) {
        int sid = ass_alloc_style(track);
        track->default_style = sid;
        ASS_Style *style = track->styles + sid;
        style->Name = strdup("OSD");
        // Set to neutral base direction, as opposed to VSFilter LTR default
        style->Encoding = -1;

        sid = ass_alloc_style(track);
        assert(sid == track->default_style + 1);
        style = track->styles + sid;
        style->Name = strdup("Default");
        style->Encoding = -1;
    }

    ASS_Style *s_osd = track->styles + track->default_style;
    mp_ass_set_style(s_osd, track->PlayResY, osd->opts->osd_style);

    ASS_Style *s_def = track->styles + track->default_style + 1;
    const struct osd_style_opts *def = osd_style_conf.defaults;
    mp_ass_set_style(s_def, track->PlayResY, def);

    obj->osd_track = track;
}

static ASS_Event *add_osd_ass_event(ASS_Track *track, const char *text)
{
    int n = ass_alloc_event(track);
    ASS_Event *event = track->events + n;
    event->Start = 0;
    event->Duration = 100;
    event->Style = track->default_style;
    event->ReadOrder = n;
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

// Same trick as above: never valid UTF-8, so we expect it's free for use.
const char *const osd_ass_0 = "\xFD";
const char *const osd_ass_1 = "\xFE";

static void mangle_ass(bstr *dst, const char *in)
{
    bool escape_ass = true;
    while (*in) {
        // As used by osd_get_function_sym().
        if (in[0] == '\xFF' && in[1]) {
            bstr_xappend(NULL, dst, bstr0(ASS_USE_OSD_FONT));
            mp_append_utf8_bstr(NULL, dst, OSD_CODEPOINTS + in[1]);
            bstr_xappend(NULL, dst, bstr0("{\\r}"));
            in += 2;
            continue;
        }
        if (*in == '\xFD' || *in == '\xFE') {
            escape_ass = *in == '\xFE';
            in += 1;
            continue;
        }
        if (escape_ass && *in == '{')
            bstr_xappend(NULL, dst, bstr0("\\"));
        bstr_xappend(NULL, dst, (bstr){(char *)in, 1});
        // Break ASS escapes with U+2060 WORD JOINER
        if (escape_ass && *in == '\\')
            mp_append_utf8_bstr(NULL, dst, 0x2060);
        in++;
    }
}

static void add_osd_ass_event_escaped(ASS_Track *track, const char *text)
{
    bstr buf = {0};
    mangle_ass(&buf, text);
    add_osd_ass_event(track, buf.start);
    talloc_free(buf.start);
}

static void update_osd(struct osd_state *osd, struct osd_object *obj)
{
    struct MPOpts *opts = osd->opts;

    create_ass_track(osd, obj, 0, 0);
    clear_obj(obj);
    if (!obj->text[0])
        return;

    struct osd_style_opts font = *opts->osd_style;
    font.font_size *= opts->osd_scale;

    double playresy = obj->osd_track->PlayResY;
    // Compensate for libass and mp_ass_set_style scaling the font etc.
    if (!opts->osd_scale_by_window)
        playresy *= 720.0 / obj->vo_res.h;

    ASS_Style *style = obj->osd_track->styles + obj->osd_track->default_style;
    mp_ass_set_style(style, playresy, &font);

    add_osd_ass_event_escaped(obj->osd_track, obj->text);
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

    create_ass_track(osd, obj, 0, 0);
    ASS_Track *track = obj->osd_track;
    ASS_Style *style = track->styles + track->default_style;

    *o_w = track->PlayResX * (opts->osd_bar_w / 100.0);
    *o_h = track->PlayResY * (opts->osd_bar_h / 100.0);

    float base_size = 0.03125;
    style->Outline *= *o_h / track->PlayResY / base_size;
    // So that the chapter marks have space between them
    style->Outline = FFMIN(style->Outline, *o_h / 5.2);
    // So that the border is not 0
    style->Outline = FFMAX(style->Outline, *o_h / 32.0);
    // Rendering with shadow is broken (because there's more than one shape)
    style->Shadow = 0;

    style->Alignment = 5;

    *o_border = style->Outline;

    *o_x = get_align(opts->osd_bar_align_x, track->PlayResX, *o_w, *o_border);
    *o_y = get_align(opts->osd_bar_align_y, track->PlayResY, *o_h, *o_border);
}

static void update_progbar(struct osd_state *osd, struct osd_object *obj)
{
    float px, py, width, height, border;
    get_osd_bar_box(osd, obj, &px, &py, &width, &height, &border);

    clear_obj(obj);

    if (obj->progbar_state.type < 0)
        return;

    float sx = px - border * 2 - height / 4; // includes additional spacing
    float sy = py + height / 2;

    bstr buf = bstr0(talloc_asprintf(NULL, "{\\an6\\pos(%f,%f)}", sx, sy));

    if (obj->progbar_state.type == 0 || obj->progbar_state.type >= 256) {
        // no sym
    } else if (obj->progbar_state.type >= 32) {
        mp_append_utf8_bstr(NULL, &buf, obj->progbar_state.type);
    } else {
        bstr_xappend(NULL, &buf, bstr0(ASS_USE_OSD_FONT));
        mp_append_utf8_bstr(NULL, &buf, OSD_CODEPOINTS + obj->progbar_state.type);
        bstr_xappend(NULL, &buf, bstr0("{\\r}"));
    }

    add_osd_ass_event(obj->osd_track, buf.start);
    talloc_free(buf.start);

    struct ass_draw *d = &(struct ass_draw) { .scale = 4 };
    // filled area
    d->text = talloc_asprintf_append(d->text, "{\\bord0\\pos(%f,%f)}", px, py);
    ass_draw_start(d);
    float pos = obj->progbar_state.value * width - border / 2;
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
    for (int n = 0; n < obj->progbar_state.num_stops; n++) {
        float s = obj->progbar_state.stops[n] * width;
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

static void update_external(struct osd_state *osd, struct osd_object *obj)
{
    create_ass_track(osd, obj, obj->external_res_x, obj->external_res_y);
    clear_obj(obj);

    bstr t = bstr0(obj->text);
    while (t.len) {
        bstr line;
        bstr_split_tok(t, "\n", &line, &t);
        if (line.len) {
            char *tmp = bstrdup0(NULL, line);
            add_osd_ass_event(obj->osd_track, tmp);
            talloc_free(tmp);
        }
    }
}

static void update_sub(struct osd_state *osd, struct osd_object *obj)
{
    struct MPOpts *opts = osd->opts;

    clear_obj(obj);

    if (!obj->text || !obj->text[0] || obj->sub_state.render_bitmap_subs)
        return;

    create_ass_renderer(osd, obj);
    if (!obj->osd_track)
        obj->osd_track = mp_ass_default_track(obj->osd_ass_library, osd->opts);

    struct osd_style_opts font = *opts->sub_text_style;
    font.font_size *= opts->sub_scale;

    ASS_Style *style = obj->osd_track->styles + obj->osd_track->default_style;
    mp_ass_set_style(style, obj->osd_track->PlayResY, &font);
    if (obj->type == OSDTYPE_SUB2)
        style->Alignment = 6;

    ass_set_line_position(obj->osd_render, 100 - opts->sub_pos);

    add_osd_ass_event_escaped(obj->osd_track, obj->text);
}

static void update_object(struct osd_state *osd, struct osd_object *obj)
{
    switch (obj->type) {
    case OSDTYPE_OSD:
        update_osd(osd, obj);
        break;
    case OSDTYPE_SUB:
    case OSDTYPE_SUB2:
        update_sub(osd, obj);
        break;
    case OSDTYPE_PROGBAR:
        update_progbar(osd, obj);
        break;
    case OSDTYPE_EXTERNAL:
        update_external(osd, obj);
        break;
    }
}

void osd_object_get_bitmaps(struct osd_state *osd, struct osd_object *obj,
                            struct sub_bitmaps *out_imgs)
{
    if (!osd->opts->use_text_osd)
        return;

    if (obj->force_redraw)
        update_object(osd, obj);

    *out_imgs = (struct sub_bitmaps) {0};
    if (!obj->osd_track)
        return;

    ass_set_frame_size(obj->osd_render, obj->vo_res.w, obj->vo_res.h);
    ass_set_aspect_ratio(obj->osd_render, obj->vo_res.display_par, 1.0);
    mp_ass_render_frame(obj->osd_render, obj->osd_track, 0,
                        &obj->parts_cache, out_imgs);
    talloc_steal(obj, obj->parts_cache);
}

void osd_object_get_resolution(struct osd_state *osd, int obj,
                               int *out_w, int *out_h)
{
    pthread_mutex_lock(&osd->lock);
    struct osd_object *osd_obj = osd->objs[obj];
    *out_w = osd_obj->osd_track ? osd_obj->osd_track->PlayResX : 0;
    *out_h = osd_obj->osd_track ? osd_obj->osd_track->PlayResY : 0;
    pthread_mutex_unlock(&osd->lock);
}
