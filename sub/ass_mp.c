/*
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
 *
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
 * with libass; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#include <ass/ass.h>
#include <ass/ass_types.h>

#include <libavutil/common.h>

#include "config.h"
#include "core/mp_msg.h"
#include "core/path.h"
#include "ass_mp.h"
#include "subreader.h"
#include "sub/sub.h"
#include "stream/stream.h"
#include "core/options.h"

void mp_ass_set_style(ASS_Style *style, struct osd_style_opts *opts)
{
    if (opts->font) {
        free(style->FontName);
        style->FontName = strdup(opts->font);
        style->treat_fontname_as_pattern = 1;
    }

    // libass_font_size = FontSize * (window_height / MP_ASS_FONT_PLAYRESY)
    // scale translates parameters from PlayResY=720 to MP_ASS_FONT_PLAYRESY
    double scale = MP_ASS_FONT_PLAYRESY / 720.0;

    style->FontSize = opts->font_size * scale;
    style->PrimaryColour = MP_ASS_COLOR(opts->color);
    style->SecondaryColour = style->PrimaryColour;
    if (opts->back_color.a) {
        style->OutlineColour = MP_ASS_COLOR(opts->back_color);
        style->BorderStyle = 3; // opaque box
    } else {
        style->OutlineColour = MP_ASS_COLOR(opts->border_color);
        style->BorderStyle = 1; // outline
    }
    style->BackColour = MP_ASS_COLOR(opts->shadow_color);
    style->Outline = opts->border_size * scale;
    style->Shadow = opts->shadow_offset * scale;
    style->Spacing = opts->spacing * scale;
    style->MarginL = opts->margin_x * scale;
    style->MarginR = style->MarginL;
    style->MarginV = opts->margin_y * scale;
    style->ScaleX = 1.;
    style->ScaleY = 1.;
#if LIBASS_VERSION >= 0x01020000
    style->Blur = opts->blur;
#endif
}

ASS_Track *mp_ass_default_track(ASS_Library *library, struct MPOpts *opts)
{
    ASS_Track *track = ass_new_track(library);

    track->track_type = TRACK_TYPE_ASS;
    track->Timer = 100.;
    track->PlayResY = MP_ASS_FONT_PLAYRESY;
    track->WrapStyle = 0;

    if (opts->ass_styles_file && opts->ass_style_override)
        ass_read_styles(track, opts->ass_styles_file, sub_cp);

    if (track->n_styles == 0) {
        track->Kerning = true;
        int sid = ass_alloc_style(track);
        track->default_style = sid;
        ASS_Style *style = track->styles + sid;
        style->Name = strdup("Default");
        style->Alignment = 2;
        mp_ass_set_style(style, opts->sub_text_style);
    }

    if (opts->ass_style_override)
        ass_process_force_style(track);

    return track;
}

/**
 * \brief Convert subtitle to ASS_Events for the given track
 * \param track track
 * \param sub subtitle to convert
 * \return event id
 * note: assumes that subtitle is _not_ fps-based; caller must manually correct
 *   Start and Duration in other case.
 **/
static int ass_process_subtitle(ASS_Track *track, subtitle *sub)
{
    int eid;
    ASS_Event *event;
    int len = 0, j;
    char *p;
    char *end;

    eid = ass_alloc_event(track);
    event = track->events + eid;

    event->Start = sub->start * 10;
    event->Duration = (sub->end - sub->start) * 10;
    event->Style = track->default_style;

    for (j = 0; j < sub->lines; ++j)
        len += sub->text[j] ? strlen(sub->text[j]) : 0;

    len += 2 * sub->lines;      // '\N', including the one after the last line
    len += 6;                   // {\anX}
    len += 1;                   // '\0'

    event->Text = malloc(len);
    end = event->Text + len;
    p = event->Text;

    if (sub->alignment)
        p += snprintf(p, end - p, "{\\an%d}", sub->alignment);

    for (j = 0; j < sub->lines; ++j)
        p += snprintf(p, end - p, "%s\\N", sub->text[j]);

    if (sub->lines > 0)
        p -= 2;                 // remove last "\N"
    *p = 0;

    mp_msg(MSGT_ASS, MSGL_V,
           "plaintext event at %" PRId64 ", +%" PRId64 ": %s  \n",
           (int64_t) event->Start, (int64_t) event->Duration, event->Text);

    return eid;
}


/**
 * \brief Convert subdata to ASS_Track
 * \param subdata subtitles struct from subreader
 * \param fps video framerate
 * \return newly allocated ASS_Track, filled with subtitles from subdata
 */
ASS_Track *mp_ass_read_subdata(ASS_Library *library, struct MPOpts *opts,
                               sub_data *subdata, double fps)
{
    ASS_Track *track;
    int i;

    track = mp_ass_default_track(library, opts);
    track->name = subdata->filename ? strdup(subdata->filename) : 0;

    for (i = 0; i < subdata->sub_num; ++i) {
        int eid = ass_process_subtitle(track, subdata->subtitles + i);
        if (eid < 0)
            continue;
        if (!subdata->sub_uses_time) {
            track->events[eid].Start *= 100. / fps;
            track->events[eid].Duration *= 100. / fps;
        }
    }
    return track;
}

ASS_Track *mp_ass_read_stream(ASS_Library *library, const char *fname,
                              char *charset)
{
    ASS_Track *track;

    struct stream *s = open_stream(fname, NULL, NULL);
    if (!s)
        // Stream code should have printed an error already
        return NULL;
    struct bstr content = stream_read_complete(s, NULL, 100000000, 1);
    if (content.start == NULL)
        mp_tmsg(MSGT_ASS, MSGL_ERR, "Refusing to load subtitle file "
                "larger than 100 MB: %s\n", fname);
    free_stream(s);
    if (content.len == 0) {
        talloc_free(content.start);
        return NULL;
    }
    content.start[content.len] = 0;
    track = ass_read_memory(library, content.start, content.len, charset);
    if (track) {
        free(track->name);
        track->name = strdup(fname);
    }
    talloc_free(content.start);
    return track;
}

void mp_ass_configure(ASS_Renderer *priv, struct MPOpts *opts,
                      struct mp_osd_res *dim)
{
    ass_set_frame_size(priv, dim->w, dim->h);
    ass_set_margins(priv, dim->mt, dim->mb, dim->ml, dim->mr);

    int set_use_margins = 0;
#if LIBASS_VERSION >= 0x01010000
    int set_sub_pos = 0;
#endif
    float set_line_spacing = 0;
    float set_font_scale = 1;
    int set_hinting = 0;
    if (opts->ass_style_override) {
        set_use_margins = opts->ass_use_margins;
#if LIBASS_VERSION >= 0x01010000
        set_sub_pos = 100 - sub_pos;
#endif
        set_line_spacing = opts->ass_line_spacing;
        set_font_scale = opts->sub_scale;
        set_hinting = opts->ass_hinting & 3; // +4 was for no hinting if scaled
    }

    ass_set_use_margins(priv, set_use_margins);
#if LIBASS_VERSION >= 0x01010000
    ass_set_line_position(priv, set_sub_pos);
#endif
    ass_set_font_scale(priv, set_font_scale);
    ass_set_hinting(priv, set_hinting);
    ass_set_line_spacing(priv, set_line_spacing);
}

void mp_ass_configure_fonts(ASS_Renderer *priv, struct osd_style_opts *opts)
{
    char *default_font = mp_find_user_config_file("subfont.ttf");
    char *config       = mp_find_config_file("fonts.conf");

    if (default_font && !mp_path_exists(default_font)) {
        talloc_free(default_font);
        default_font = NULL;
    }

    mp_msg(MSGT_ASS, MSGL_V, "[ass] Setting up fonts...\n");
    ass_set_fonts(priv, default_font, opts->font, 1, config, 1);
    mp_msg(MSGT_ASS, MSGL_V, "[ass] Done.\n");

    talloc_free(default_font);
    talloc_free(config);
}

void mp_ass_render_frame(ASS_Renderer *renderer, ASS_Track *track, double time,
                         struct sub_bitmap **parts, struct sub_bitmaps *res)
{
    int changed;
    ASS_Image *imgs = ass_render_frame(renderer, track, time, &changed);
    if (changed == 2)
        res->bitmap_id = ++res->bitmap_pos_id;
    else if (changed)
        res->bitmap_pos_id++;
    res->format = SUBBITMAP_LIBASS;

    res->parts = *parts;
    res->num_parts = 0;
    int num_parts_alloc = MP_TALLOC_ELEMS(res->parts);
    for (struct ass_image *img = imgs; img; img = img->next) {
        if (img->w == 0 || img->h == 0)
            continue;
        if (res->num_parts >= num_parts_alloc) {
            num_parts_alloc = FFMAX(num_parts_alloc * 2, 32);
            res->parts = talloc_realloc(NULL, res->parts, struct sub_bitmap,
                                        num_parts_alloc);
        }
        struct sub_bitmap *p = &res->parts[res->num_parts];
        p->bitmap = img->bitmap;
        p->stride = img->stride;
        p->libass.color = img->color;
        p->dw = p->w = img->w;
        p->dh = p->h = img->h;
        p->x = img->dst_x;
        p->y = img->dst_y;
        res->num_parts++;
    }
    *parts = res->parts;
}

static int map_ass_level[] = {
    MSGL_ERR,           // 0 "FATAL errors"
    MSGL_WARN,
    MSGL_INFO,
    MSGL_V,
    MSGL_V,
    MSGL_V,             // 5 application recommended level
    MSGL_DBG2,
    MSGL_DBG3,          // 7 "verbose DEBUG"
};

static void message_callback(int level, const char *format, va_list va, void *ctx)
{
    level = map_ass_level[level];
    mp_msg(MSGT_ASS, level, "[ass] ");
    mp_msg_va(MSGT_ASS, level, format, va);
    // libass messages lack trailing \n
    mp_msg(MSGT_ASS, level, "\n");
}

ASS_Library *mp_ass_init(struct MPOpts *opts)
{
    ASS_Library *priv;
    char *path = mp_find_user_config_file("fonts");
    priv = ass_library_init();
    ass_set_message_cb(priv, message_callback, NULL);
    if (path)
        ass_set_fonts_dir(priv, path);
    ass_set_extract_fonts(priv, opts->use_embedded_fonts);
    talloc_free(path);
    return priv;
}
