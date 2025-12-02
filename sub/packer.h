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

#ifndef MP_SUB_PACKER_H
#define MP_SUB_PACKER_H

#include <stdbool.h>

#include "config.h"

#include <ass/ass.h>
#include <ass/ass_types.h>

struct sub_bitmaps;
struct mp_sub_packer;
struct mp_sub_packer *mp_sub_packer_alloc(void *ta_parent);
void mp_sub_packer_pack_ass(struct mp_sub_packer *p, ASS_Image **image_lists,
                            int num_image_lists, bool changed, bool video_color_space,
                            int preferred_osd_format, struct sub_bitmaps *out);

#if HAVE_SUBRANDR
struct sbr_instanced_raster_pass;
void mp_sub_packer_pack_sbr(struct mp_sub_packer *p, struct sbr_instanced_raster_pass *pass,
                            struct sub_bitmaps *out);
const struct sub_bitmaps *mp_sub_packer_get_cached(struct mp_sub_packer *p);
#endif

#endif
