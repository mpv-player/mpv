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

#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <assert.h>

#include <libavutil/mem.h>

#include "talloc.h"
#include "common/common.h"
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

bool mp_audio_config_valid(const struct mp_audio *mpa)
{
    return mp_chmap_is_valid(&mpa->channels) && af_fmt_is_valid(mpa->format)
        && mpa->rate >= 1 && mpa->rate < 10000000;
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

static void mp_audio_destructor(void *ptr)
{
    struct mp_audio *mpa = ptr;
    for (int n = 0; n < MP_NUM_CHANNELS; n++) {
        // Note: don't free if not allocated by mp_audio_realloc
        if (mpa->allocated[n])
            av_free(mpa->planes[n]);
    }
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
    if (samples >= INT_MAX / mpa->sstride)
        abort(); // oom
    int size = MPMAX(samples * mpa->sstride, 1);
    for (int n = 0; n < mpa->num_planes; n++) {
        if (size != mpa->allocated[n]) {
            // Note: av_realloc() can't be used (see libavutil doxygen)
            void *new = av_malloc(size);
            if (!new)
                abort();
            if (mpa->allocated[n])
                memcpy(new, mpa->planes[n], MPMIN(mpa->allocated[n], size));
            av_free(mpa->planes[n]);
            mpa->planes[n] = new;
            mpa->allocated[n] = size;
        }
    }
    for (int n = mpa->num_planes; n < MP_NUM_CHANNELS; n++) {
        av_free(mpa->planes[n]);
        mpa->planes[n] = NULL;
        mpa->allocated[n] = 0;
    }
    talloc_set_destructor(mpa, mp_audio_destructor);
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
        int s = mpa->allocated[n] / mpa->sstride;
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
    for (int n = 0; n < mpa->num_planes; n++) {
        if (n > 0 && mpa->planes[n] == mpa->planes[0])
            continue; // silly optimization for special cases
        af_fill_silence((char *)mpa->planes[n] + offset, size, mpa->format);
    }
}

// All integer parameters are in samples.
// dst and src can overlap.
void mp_audio_copy(struct mp_audio *dst, int dst_offset,
                   struct mp_audio *src, int src_offset, int length)
{
    assert(mp_audio_config_equals(dst, src));
    assert(length >= 0);
    assert(dst_offset >= 0 && dst_offset + length <= dst->samples);
    assert(src_offset >= 0 && src_offset + length <= src->samples);

    for (int n = 0; n < dst->num_planes; n++) {
        memmove((char *)dst->planes[n] + dst_offset * dst->sstride,
                (char *)src->planes[n] + src_offset * src->sstride,
                length * dst->sstride);
    }
}

// Set data to the audio after the given number of samples (i.e. slice it).
void mp_audio_skip_samples(struct mp_audio *data, int samples)
{
    assert(samples >= 0 && samples <= data->samples);

    for (int n = 0; n < data->num_planes; n++)
        data->planes[n] = (uint8_t *)data->planes[n] + samples * data->sstride;

    data->samples -= samples;
}
