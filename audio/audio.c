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

#include "core/mp_talloc.h"
#include "audio.h"

void mp_audio_set_format(struct mp_audio *mpa, int format)
{
    mpa->format = format;
    mpa->bps = af_fmt2bits(format) / 8;
}

void mp_audio_set_num_channels(struct mp_audio *mpa, int num_channels)
{
    struct mp_chmap map;
    mp_chmap_from_channels(&map, num_channels);
    mp_audio_set_channels(mpa, &map);
}

// Use old MPlayer/ALSA channel layout.
void mp_audio_set_channels_old(struct mp_audio *mpa, int num_channels)
{
    struct mp_chmap map;
    mp_chmap_from_channels_alsa(&map, num_channels);
    mp_audio_set_channels(mpa, &map);
}

void mp_audio_set_channels(struct mp_audio *mpa, const struct mp_chmap *chmap)
{
    assert(mp_chmap_is_empty(chmap) || mp_chmap_is_valid(chmap));
    mpa->channels = *chmap;
    mpa->nch = mpa->channels.num;
}

void mp_audio_copy_config(struct mp_audio *dst, const struct mp_audio *src)
{
    mp_audio_set_format(dst, src->format);
    mp_audio_set_channels(dst, &src->channels);
    dst->rate = src->rate;
}

bool mp_audio_config_equals(const struct mp_audio *a, const struct mp_audio *b)
{
    return a->format == b->format && a->rate == b->rate &&
           mp_chmap_equals(&a->channels, &b->channels);
}

char *mp_audio_fmt_to_str(int srate, const struct mp_chmap *chmap, int format)
{
    char *chstr = mp_chmap_to_str(chmap);
    char *res = talloc_asprintf(NULL, "%dHz %s %dch %s", srate, chstr,
                                chmap->num, af_fmt2str_short(format));
    talloc_free(chstr);
    return res;
}

char *mp_audio_config_to_str(struct mp_audio *mpa)
{
    return mp_audio_fmt_to_str(mpa->rate, &mpa->channels, mpa->format);
}
