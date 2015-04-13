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

#ifndef MPLAYER_DRAW_BMP_H
#define MPLAYER_DRAW_BMP_H

#include "osd.h"

struct mp_image;
struct sub_bitmaps;
struct mp_draw_sub_cache;
void mp_draw_sub_bitmaps(struct mp_draw_sub_cache **cache, struct mp_image *dst,
                         struct sub_bitmaps *sbs);

extern const bool mp_draw_sub_formats[SUBBITMAP_COUNT];

#endif /* MPLAYER_DRAW_BMP_H */

// vim: ts=4 sw=4 et tw=80
