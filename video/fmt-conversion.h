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

#ifndef MPLAYER_FMT_CONVERSION_H
#define MPLAYER_FMT_CONVERSION_H

#include <libavutil/pixfmt.h>

enum AVPixelFormat imgfmt2pixfmt(int fmt);
int pixfmt2imgfmt(enum AVPixelFormat pix_fmt);

#endif /* MPLAYER_FMT_CONVERSION_H */
