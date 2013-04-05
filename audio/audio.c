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

#include <assert.h>

#include "audio.h"

void mp_audio_set_format(struct mp_audio *mpa, int format)
{
    mpa->format = format;
    mpa->bps = af_fmt2bits(format) / 8;
}

void mp_audio_set_num_channels(struct mp_audio *mpa, int num_channels)
{
    mpa->nch = num_channels;
}

void mp_audio_copy_config(struct mp_audio *dst, const struct mp_audio *src)
{
    mp_audio_set_format(dst, src->format);
    mp_audio_set_num_channels(dst, src->nch);
    dst->rate = src->rate;
}
