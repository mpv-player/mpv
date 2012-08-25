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
#include "mp_msg.h"
#include "sub.h"
#include "libavutil/common.h"

static const char osd_font_pfb[] =
#include "sub/osd_font.h"
;

#include "sub/ass_mp.h"
#include "mp_core.h"


// Map OSD symbols (e.g. OSD_PLAY) to the glyphs in osd_font_pfb[].
#define OSD_CODEPOINTS 0xE000

// NOTE: \fs-5 to reduce the size of the symbols in relation to normal text.
//       Done because libass doesn't center characters that are too high.
#define ASS_USE_OSD_FONT "{\\fnOSD\\fs-5}"

void osd_init_backend(struct osd_state *osd)
{
    osd->osd_ass_library = mp_ass_init(osd->opts);
    ass_add_font(osd->osd_ass_library, "OSD", (void *)osd_font_pfb,
                 sizeof(osd_font_pfb) - 1);

    osd->osd_render = ass_renderer_init(osd->osd_ass_library);
    mp_ass_configure_fonts(osd->osd_render);
    ass_set_aspect_ratio(osd->osd_render, 1.0, 1.0);
}

void osd_destroy_backend(struct osd_state *osd)
{
    if (osd) {
        if (osd->osd_render)
            ass_renderer_done(osd->osd_render);
        osd->osd_render = NULL;
        ass_library_done(osd->osd_ass_library);
        osd->osd_ass_library = NULL;
    }
}

static void eosd_draw_alpha_a8i8(unsigned char *src,
                                 int src_w, int src_h,
                                 int src_stride,
                                 unsigned char *dst_a,
                                 unsigned char *dst_i,
                                 size_t dst_stride,
                                 int dst_x, int dst_y,
                                 uint32_t color)
{
    const unsigned int r = (color >> 24) & 0xff;
    const unsigned int g = (color >> 16) & 0xff;
    const unsigned int b = (color >>  8) & 0xff;
    const unsigned int a = 0xff - (color & 0xff);

    int gray = (r + g + b) / 3; // not correct

    dst_a += dst_y * dst_stride + dst_x;
    dst_i += dst_y * dst_stride + dst_x;

    int src_skip = src_stride - src_w;
    int dst_skip = dst_stride - src_w;

    for (int y = 0; y < src_h; y++) {
        for (int x = 0; x < src_w; x++) {
            unsigned char as = (*src * a) >> 8;
            unsigned char bs = (gray * as) >> 8;
            // to mplayer scale
            as = -as;

            unsigned char *a = dst_a;
            unsigned char *b = dst_i;

            // NOTE: many special cases, because alpha=0 means transparency,
            //       while alpha=1..255 is opaque..transparent
            if (as) {
                *b = ((*b * as) >> 8) + bs;
                if (*a) {
                    *a = (*a * as) >> 8;
                    if (*a < 1)
                        *a = 1;
                } else {
                    *a = as;
                }
            }

            dst_a++;
            dst_i++;
            src++;
        }
        dst_a += dst_skip;
        dst_i += dst_skip;
        src += src_skip;
    }
}

static void eosd_render_a8i8(unsigned char *a, unsigned char *i, size_t stride,
                             int x, int y, ASS_Image *imgs)
{
    for (ASS_Image *p = imgs; p; p = p->next) {
        eosd_draw_alpha_a8i8(p->bitmap, p->w, p->h, p->stride, a, i, stride,
                             x + p->dst_x, y + p->dst_y, p->color);
    }
}

static bool ass_bb(ASS_Image *imgs, int *x1, int *y1, int *x2, int *y2)
{
    *x1 = *y1 = INT_MAX;
    *x2 = *y2 = INT_MIN;
    for (ASS_Image *p = imgs; p; p = p->next) {
        *x1 = FFMIN(*x1, p->dst_x);
        *y1 = FFMIN(*y1, p->dst_y);
        *x2 = FFMAX(*x2, p->dst_x + p->w);
        *y2 = FFMAX(*y2, p->dst_y + p->h);
    }
    return *x1 < *x2 && *y1 < *y2;
}

static void draw_ass_osd(struct osd_state *osd, mp_osd_obj_t *obj)
{
    ass_set_frame_size(osd->osd_render, osd->w, osd->h);

    ASS_Image *imgs = ass_render_frame(osd->osd_render, obj->osd_track, 0,
                                       NULL);

    int x1, y1, x2, y2;
    if (!ass_bb(imgs, &x1, &y1, &x2, &y2)) {
        obj->flags &= ~OSDFLAG_VISIBLE;
        return;
    }

    obj->bbox.x1 = x1;
    obj->bbox.y1 = y1;
    obj->bbox.x2 = x2;
    obj->bbox.y2 = y2;
    obj->flags |= OSDFLAG_BBOX;
    osd_alloc_buf(obj);

    eosd_render_a8i8(obj->alpha_buffer, obj->bitmap_buffer, obj->stride,
                     -x1, -y1, imgs);
}


static void update_font_scale(ASS_Track *track, ASS_Style *style, double factor)
{
    // duplicated from ass_mp.c
    double fs = track->PlayResY * factor / 100.;
    /* The font size is always proportional to video height only;
    * real -subfont-autoscale behavior is not implemented.
    * Apply a correction that corresponds to about 4:3 aspect ratio
    * video to get a size somewhat closer to what non-libass rendering
    * would produce with the same text_font_scale_factor
    * and subtitle_autoscale.
    */
    if (subtitle_autoscale == 2)
        fs *= 1.3;
    else if (subtitle_autoscale == 3)
        fs *= 1.7;
    style->FontSize = fs;
    style->Outline = style->FontSize / 16;
}


static ASS_Track *create_osd_ass_track(struct osd_state *osd)
{
    ASS_Track *track = mp_ass_default_track(osd->osd_ass_library, osd->opts);
    ASS_Style *style = track->styles + track->default_style;

    track->PlayResX = track->PlayResY * 1.33333;

    update_font_scale(track, style, text_font_scale_factor);

    style->Alignment = 5;

    free(style->FontName);
    style->FontName = strdup(font_name ? font_name : "Sans");

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
    return event;
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
        if (in[0] == '\\' && strchr("nNh{}", in[1])) {
            // Undo escaping, e.g. \{ -> \\{
            // Note that e.g. \\j still must be emitted as \\j
            // (libass only understands the escapes listed in the strchr args)
            res = talloc_asprintf_append_buffer(res, "\\\\%c", in[1]);
            in += 2;
            continue;
        }
        // As used by osd_get_function_sym().
        if (in[0] == '\xFF') {
            res = talloc_strdup_append_buffer(res, ASS_USE_OSD_FONT);
            res = append_utf8_buffer(res, OSD_CODEPOINTS + in[1]);
            res = talloc_strdup_append_buffer(res, "{\\r}");
            in += 2;
            continue;
        }
        if (*in == '{')
            res = talloc_strdup_append_buffer(res, "\\");
        res = talloc_strndup_append_buffer(res, in, 1);
        in++;
    }
    return res;
}

void vo_update_text_osd(struct osd_state *osd, mp_osd_obj_t* obj)
{
    if (!obj->osd_track)
        obj->osd_track = create_osd_ass_track(osd);
    ASS_Event *event = get_osd_ass_event(obj->osd_track);
    event->Text = mangle_ass(osd->osd_text);
    draw_ass_osd(osd, obj);
    talloc_free(event->Text);
    event->Text = NULL;
}

#define OSDBAR_ELEMS 46

void vo_update_text_progbar(struct osd_state *osd, mp_osd_obj_t* obj)
{
    obj->flags |= OSDFLAG_CHANGED | OSDFLAG_VISIBLE;

    if (vo_osd_progbar_type < 0) {
        obj->flags &= ~OSDFLAG_VISIBLE;
        return;
    }

    if (!obj->osd_track)
        obj->osd_track = create_osd_ass_track(osd);

    ASS_Style *style = obj->osd_track->styles + obj->osd_track->default_style;

    style->Alignment = 10;
    style->MarginL = style->MarginR = style->MarginV = 0;

    // We need a fixed font size with respect to the OSD width.
    // Assume the OSD bar takes 2/3 of the OSD width at PlayResY=288 and
    // FontSize=22 with an OSD aspect ratio of 16:9. Rescale as needed.
    // xxx can fail when unknown fonts are involved
    double asp = (double)osd->w / osd->h;
    double scale = (asp / 1.77777) * (obj->osd_track->PlayResY / 288.0);
    style->ScaleX = style->ScaleY = scale;
    style->FontSize = 22.0;
    style->Outline = style->FontSize / 16 * scale;

    int active = (vo_osd_progbar_value * OSDBAR_ELEMS + 255) / 256;
    active = FFMIN(OSDBAR_ELEMS, FFMAX(active, 0));

    char *text = talloc_strdup(NULL, "{\\q2}");

    if (vo_osd_progbar_type >= 32) {
        text = append_utf8_buffer(text, vo_osd_progbar_type);
    } else if (vo_osd_progbar_type > 0) {
        text = talloc_strdup_append_buffer(text, ASS_USE_OSD_FONT);
        text = append_utf8_buffer(text, OSD_CODEPOINTS + vo_osd_progbar_type);
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
    event->Text = text;
    draw_ass_osd(osd, obj);
    event->Text = NULL;

    talloc_free(text);
}

void vo_update_text_sub(struct osd_state *osd, mp_osd_obj_t* obj)
{
    struct MPOpts *opts = osd->opts;

    obj->flags |= OSDFLAG_CHANGED | OSDFLAG_VISIBLE;

    if (!vo_sub || !opts->sub_visibility) {
        obj->flags &= ~OSDFLAG_VISIBLE;
        return;
    }

    if (!obj->osd_track)
        obj->osd_track = mp_ass_default_track(osd->osd_ass_library, osd->opts);

    ASS_Style *style = obj->osd_track->styles + obj->osd_track->default_style;

    style->MarginV = obj->osd_track->PlayResY * ((100 - sub_pos)/110.0);
    update_font_scale(obj->osd_track, style, text_font_scale_factor);

    char *text = talloc_strdup(NULL, "");

    for (int n = 0; n < vo_sub->lines; n++)
        text = talloc_asprintf_append_buffer(text, "%s\n", vo_sub->text[n]);

    ASS_Event *event = get_osd_ass_event(obj->osd_track);
    event->Text = mangle_ass(text);
    draw_ass_osd(osd, obj);
    talloc_free(event->Text);
    event->Text = NULL;

    talloc_free(text);
}

// unneeded
void osd_font_invalidate(void) {}
void osd_font_load(struct osd_state *osd) {}
