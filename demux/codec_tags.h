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

#ifndef MP_CODEC_TAGS_H
#define MP_CODEC_TAGS_H

#include <stdint.h>
#include <stdbool.h>

struct sh_stream;

void mp_set_codec_from_tag(struct sh_stream *sh);

void mp_set_pcm_codec(struct sh_stream *sh, bool sign, bool is_float, int bits,
                      bool is_be);

const char *mp_map_mimetype_to_video_codec(const char *mimetype);

#endif
