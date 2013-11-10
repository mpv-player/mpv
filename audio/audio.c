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

#include "mpvcore/mp_common.h"
#include "mpvcore/mp_talloc.h"
#include "audio.h"

static void update_redundant_info(struct mp_audio *mpa)
{
    assert(mp_chmap_is_empty(&mpa->channels) ||
           mp_chmap_is_valid(&mpa->channels));
    mpa->nch = mpa->channels.num;
    mpa->bps = af_fmt2bits(mpa->format) / 8;
    if (af_fmt_is_planar(mpa->format)) {
        mpa->spf = 1;
        mpa->num_planes = mpa->nch;
        mpa->sstride = mpa->bps;
    } else {
        mpa->spf = mpa->nch;
        mpa->num_planes = 1;
        mpa->sstride = mpa->bps * mpa->nch;
    }
}

void mp_audio_set_format(struct mp_audio *mpa, int format)
{
    mpa->format = format;
    update_redundant_info(mpa);
}

void mp_audio_set_num_channels(struct mp_audio *mpa, int num_channels)
{
    mp_chmap_from_channels(&mpa->channels, num_channels);
    update_redundant_info(mpa);
}

// Use old MPlayer/ALSA channel layout.
void mp_audio_set_channels_old(struct mp_audio *mpa, int num_channels)
{
    mp_chmap_from_channels_alsa(&mpa->channels, num_channels);
    update_redundant_info(mpa);
}

void mp_audio_set_channels(struct mp_audio *mpa, const struct mp_chmap *chmap)
{
    mpa->channels = *chmap;
    update_redundant_info(mpa);
}

void mp_audio_copy_config(struct mp_audio *dst, const struct mp_audio *src)
{
    dst->format = src->format;
    dst->channels = src->channels;
    dst->rate = src->rate;
    update_redundant_info(dst);
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
                                chmap->num, af_fmt_to_str(format));
    talloc_free(chstr);
    return res;
}

char *mp_audio_config_to_str(struct mp_audio *mpa)
{
    return mp_audio_fmt_to_str(mpa->rate, &mpa->channels, mpa->format);
}

void mp_audio_force_interleaved_format(struct mp_audio *mpa)
{
    if (af_fmt_is_planar(mpa->format))
        mp_audio_set_format(mpa, af_fmt_from_planar(mpa->format));
}

// Return used size of a plane. (The size is the same for all planes.)
int mp_audio_psize(struct mp_audio *mpa)
{
    return mpa->samples * mpa->sstride;
}

void mp_audio_set_null_data(struct mp_audio *mpa)
{
    for (int n = 0; n < MP_NUM_CHANNELS; n++)
        mpa->planes[n] = NULL;
    mpa->samples = 0;
}

/* Reallocate the data stored in mpa->planes[n] so that enough samples are
 * available on every plane. The previous data is kept (for the smallest
 * common number of samples before/after resize).
 *
 * mpa->samples is not set or used.
 *
 * This function is flexible enough to handle format and channel layout
 * changes. In these cases, all planes are reallocated as needed. Unused
 * planes are freed.
 *
 * mp_audio_realloc(mpa, 0) will still yield non-NULL for mpa->data[n].
 *
 * Allocated data is implicitly freed on talloc_free(mpa).
 */
void mp_audio_realloc(struct mp_audio *mpa, int samples)
{
    assert(samples >= 0);
    int size = MPMAX(samples * mpa->sstride, 1);
    for (int n = 0; n < mpa->num_planes; n++) {
        mpa->planes[n] = talloc_realloc_size(mpa, mpa->planes[n], size);
    }
    for (int n = mpa->num_planes; n < MP_NUM_CHANNELS; n++) {
        talloc_free(mpa->planes[n]);
        mpa->planes[n] = NULL;
    }
}

// Like mp_audio_realloc(), but only reallocate if the audio grows in size.
void mp_audio_realloc_min(struct mp_audio *mpa, int samples)
{
    if (samples > mp_audio_get_allocated_size(mpa))
        mp_audio_realloc(mpa, samples);
}

/* Get the size allocated for the data, in number of samples. If the allocated
 * size isn't on sample boundaries (e.g. after format changes), the returned
 * sample number is a rounded down value.
 *
 * Note that this only works in situations where mp_audio_realloc() also works!
 */
int mp_audio_get_allocated_size(struct mp_audio *mpa)
{
    int size = 0;
    for (int n = 0; n < mpa->num_planes; n++) {
        int s = talloc_get_size(mpa->planes[n]) / mpa->sstride;
        size = n == 0 ? s : MPMIN(size, s);
    }
    return size;
}

// Clear the samples [start, start + length) with silence.
void mp_audio_fill_silence(struct mp_audio *mpa, int start, int length)
{
    assert(start >= 0 && length >= 0 && start + length <= mpa->samples);
    int offset = start * mpa->sstride;
    int size = length * mpa->sstride;
    int fillbyte = 0;
    if ((mpa->format & AF_FORMAT_SIGN_MASK) == AF_FORMAT_US)
        fillbyte = 0x80;
    for (int n = 0; n < mpa->num_planes; n++) {
        if (n > 0 && mpa->planes[n] == mpa->planes[0])
            continue; // silly optimization for special cases
        memset((char *)mpa->planes[n] + offset, fillbyte, size);
    }
}
