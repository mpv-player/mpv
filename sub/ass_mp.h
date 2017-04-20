/*
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
 *
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

#ifndef MPLAYER_ASS_MP_H
#define MPLAYER_ASS_MP_H

#include <stdint.h>
#include <stdbool.h>

#include <ass/ass.h>
#include <ass/ass_types.h>

// This is probably arbitrary.
// sd_lavc_conv might indirectly still assume this PlayResY, though.
#define MP_ASS_FONT_PLAYRESY 288

#define MP_ASS_RGBA(r, g, b, a) \
    (((unsigned)(r) << 24) | ((g) << 16) | ((b) << 8) | (0xFF - (a)))

// m_color argument
#define MP_ASS_COLOR(c) MP_ASS_RGBA((c).r, (c).g, (c).b, (c).a)

struct MPOpts;
struct mpv_global;
struct mp_osd_res;
struct osd_style_opts;

void mp_ass_flush_old_events(ASS_Track *track, long long ts);
void mp_ass_set_style(ASS_Style *style, double res_y,
                      const struct osd_style_opts *opts);

void mp_ass_configure_fonts(ASS_Renderer *priv, struct osd_style_opts *opts,
                            struct mpv_global *global, struct mp_log *log);
ASS_Library *mp_ass_init(struct mpv_global *global, struct mp_log *log);

struct sub_bitmaps;
struct mp_ass_packer;
struct mp_ass_packer *mp_ass_packer_alloc(void *ta_parent);
void mp_ass_packer_pack(struct mp_ass_packer *p, ASS_Image **image_lists,
                        int num_image_lists, bool changed,
                        int preferred_osd_format, struct sub_bitmaps *out);

#endif                          /* MPLAYER_ASS_MP_H */
