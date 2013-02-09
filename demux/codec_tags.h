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

uint32_t mp_video_fourcc_alias(uint32_t fourcc);

struct sh_video;
struct sh_audio;

void mp_set_audio_codec_from_tag(struct sh_audio *sh);
void mp_set_video_codec_from_tag(struct sh_video *sh);

#endif
