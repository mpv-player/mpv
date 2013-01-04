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

#include "config.h"

#include "talloc.h"
#include "core/mp_msg.h"
#include "sub.h"
#include "libavutil/common.h"

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
    if (osd->osd_render)
        ass_renderer_done(osd->osd_render);
    osd->osd_render = NULL;
    ass_library_done(osd->osd_ass_library);
    osd->osd_ass_library = NULL;
}

static ASS_Track *create_osd_ass_track(struct osd_state *osd)
{
    ASS_Track *track = ass_new_track(osd->osd_ass_library);

    track->track_type = TRACK_TYPE_ASS;
    track->Timer = 100.;
    track->PlayResY = MP_ASS_FONT_PLAYRESY;
    track->PlayResX = track->PlayResY * 1.33333;
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

    return track;
}

static ASS_Event *get_osd_ass_event(ASS_Track *track)
{
    ass_flush_events(track);
    ass_alloc_event(track);
    ASS_Event *event = track->events + 0;
    event->Start = 0;
    event->Duration = 100;
    event->Style = track->default_style;
    assert(event->Text == NULL);
    return event;
}

static void clear_obj(struct osd_object *obj)
{
    if (obj->osd_track)
        ass_flush_events(obj->osd_track);
}

static char *append_utf8_buffer(char *buffer, uint32_t codepoint)
{
    char data[8];
    uint8_t tmp;
    char *output = data;
    PUT_UTF8(codepoint, tmp, *output++ = tmp;);
    return talloc_strndup_append_buffer(buffer, data, output - data);
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
            res = append_utf8_buffer(res, OSD_CODEPOINTS + in[1]);
            res = talloc_strdup_append_buffer(res, "{\\r}");
            in += 2;
            continue;
        }
        if (*in == '{')
            res = talloc_strdup_append_buffer(res, "\\");
        res = talloc_strndup_append_buffer(res, in, 1);
        // Break ASS escapes with U+2060 WORD JOINER
        if (*in == '\\')
            res = append_utf8_buffer(res, 0x2060);
        in++;
    }
    return res;
}

static void update_osd(struct osd_state *osd, struct osd_object *obj)
{
    if (!osd->osd_text[0]) {
        clear_obj(obj);
        return;
    }

    if (!obj->osd_track)
        obj->osd_track = create_osd_ass_track(osd);
    ASS_Event *event = get_osd_ass_event(obj->osd_track);
    char *text = mangle_ass(osd->osd_text);
    event->Text = strdup(text);
    talloc_free(text);
}

#define OSDBAR_ELEMS 46

static void update_progbar(struct osd_state *osd, struct osd_object *obj)
{
    if (osd->progbar_type < 0) {
        clear_obj(obj);
        return;
    }

    if (!obj->osd_track)
        obj->osd_track = create_osd_ass_track(osd);

    ASS_Style *style = obj->osd_track->styles + obj->osd_track->default_style;

    style->Alignment = 10; // all centered
    style->MarginL = style->MarginR = style->MarginV = 0;

    // We need a fixed font size with respect to the OSD width.
    // Assume the OSD bar takes 2/3 of the OSD width at PlayResY=288 and
    // FontSize=22 with an OSD aspect ratio of 16:9. Rescale as needed.
    // xxx can fail when unknown fonts are involved
    double asp = (double)obj->vo_res.w / obj->vo_res.h;
    double scale = (asp / 1.77777) * (obj->osd_track->PlayResY / 288.0);
    style->ScaleX = style->ScaleY = scale;
    style->FontSize = 22.0;
    style->Outline = style->FontSize / 16 * scale;

    int active = (osd->progbar_value * OSDBAR_ELEMS + 255) / 256;
    active = FFMIN(OSDBAR_ELEMS, FFMAX(active, 0));

    char *text = talloc_strdup(NULL, "{\\q2}");

    if (osd->progbar_type >= 32) {
        text = append_utf8_buffer(text, osd->progbar_type);
    } else if (osd->progbar_type > 0) {
        text = talloc_strdup_append_buffer(text, ASS_USE_OSD_FONT);
        text = append_utf8_buffer(text, OSD_CODEPOINTS + osd->progbar_type);
        text = talloc_strdup_append_buffer(text, "{\\r}");
    }

    //xxx space in normal font, because OSD font doesn't have a space
    text = talloc_strdup_append_buffer(text, "\\h");
    text = talloc_strdup_append_buffer(text, ASS_USE_OSD_FONT);

    text = append_utf8_buffer(text, OSD_CODEPOINTS + OSD_PB_START);
    for (int n = 0; n < active; n++)
        text = append_utf8_buffer(text, OSD_CODEPOINTS + OSD_PB_0);
    for (int n = 0; n < OSDBAR_ELEMS - active; n++)
        text = append_utf8_buffer(text, OSD_CODEPOINTS + OSD_PB_1);
    text = append_utf8_buffer(text, OSD_CODEPOINTS + OSD_PB_END);

    ASS_Event *event = get_osd_ass_event(obj->osd_track);
    event->Text = strdup(text);
    talloc_free(text);
}

static void update_sub(struct osd_state *osd, struct osd_object *obj)
{
    struct MPOpts *opts = osd->opts;

    if (!(vo_sub && opts->sub_visibility)) {
        clear_obj(obj);
        return;
    }

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

    ASS_Event *event = get_osd_ass_event(obj->osd_track);
    char *escaped_text = mangle_ass(text);
    event->Text = strdup(escaped_text);
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
