/*
 * This file is part of mplayer.
 *
 * mplayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_GL_OSD_H
#define MPLAYER_GL_OSD_H

#include <stdbool.h>
#include <inttypes.h>

#include "gl_utils.h"
#include "sub/osd.h"

struct mpgl_osd *mpgl_osd_init(GL *gl, struct mp_log *log, struct osd_state *osd);
void mpgl_osd_destroy(struct mpgl_osd *ctx);

void mpgl_osd_set_options(struct mpgl_osd *ctx, bool pbo);

void mpgl_osd_generate(struct mpgl_osd *ctx, struct mp_osd_res res, double pts,
                       int stereo_mode, int draw_flags);
enum sub_bitmap_format mpgl_osd_get_part_format(struct mpgl_osd *ctx, int index);
struct gl_vao *mpgl_osd_get_vao(struct mpgl_osd *ctx);
void mpgl_osd_draw_part(struct mpgl_osd *ctx, int vp_w, int vp_h, int index);

#endif
