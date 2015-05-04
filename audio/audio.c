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

#include <libavutil/buffer.h>
#include <libavutil/frame.h>
#include <libavutil/version.h>

#include "talloc.h"
#include "common/common.h"
#include "fmt-conversion.h"
#include "audio.h"

static void update_redundant_info(struct mp_audio *mpa)
{
    assert(mp_chmap_is_empty(&mpa->channels) ||
           mp_chmap_is_valid(&mpa->channels));
    mpa->nch = mpa->channels.num;
    mpa->bps = af_fmt2bps(mpa->format);
    if (AF_FORMAT_IS_PLANAR(mpa->format)) {
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

char *mp_audio_config_to_str_buf(char *buf, size_t buf_sz, struct mp_audio *mpa)
{
    snprintf(buf, buf_sz, "%dHz %s %dch %s", mpa->rate,
             mp_chmap_to_str(&mpa->channels), mpa->channels.num,
             af_fmt_to_str(mpa->format));
    return buf;
}

void mp_audio_force_interleaved_format(struct mp_audio *mpa)
{
    if (AF_FORMAT_IS_PLANAR(mpa->format))
        mp_audio_set_format(mpa, af_fmt_from_planar(mpa->format));
}

// Return used size of a plane. (The size is the same for all planes.)
int mp_audio_psize(struct mp_audio *mpa)
{
    return mpa->samples * mpa->sstride;
}

void mp_audio_set_null_data(struct mp_audio *mpa)
{
    for (int n = 0; n < MP_NUM_CHANNELS; n++) {
        mpa->planes[n] = NULL;
        mpa->allocated[n] = NULL;
    }
    mpa->samples = 0;
    mpa->readonly = false;
}

static int get_plane_size(const struct mp_audio *mpa, int samples)
{
    if (samples < 0 || !mp_audio_config_valid(mpa))
        return -1;
    if (samples >= INT_MAX / mpa->sstride)
        return -1;
    return MPMAX(samples * mpa->sstride, 1);
}

static void mp_audio_destructor(void *ptr)
{
    struct mp_audio *mpa = ptr;
    for (int n = 0; n < MP_NUM_CHANNELS; n++)
        av_buffer_unref(&mpa->allocated[n]);
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
    int size = get_plane_size(mpa, samples);
    if (size < 0)
        abort(); // oom or invalid parameters
    for (int n = 0; n < mpa->num_planes; n++) {
        if (!mpa->allocated[n] || size != mpa->allocated[n]->size) {
            if (av_buffer_realloc(&mpa->allocated[n], size) < 0)
                abort(); // OOM
            mpa->planes[n] = mpa->allocated[n]->data;
        }
    }
    for (int n = mpa->num_planes; n < MP_NUM_CHANNELS; n++) {
        av_buffer_unref(&mpa->allocated[n]);
        mpa->planes[n] = NULL;
    }
    talloc_set_destructor(mpa, mp_audio_destructor);
}

// Like mp_audio_realloc(), but only reallocate if the audio grows in size.
// If the buffer is reallocated, also preallocate.
void mp_audio_realloc_min(struct mp_audio *mpa, int samples)
{
    if (samples > mp_audio_get_allocated_size(mpa)) {
        size_t alloc = ta_calc_prealloc_elems(samples);
        if (alloc > INT_MAX)
            abort(); // oom
        mp_audio_realloc(mpa, alloc);
    }
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
        int s = mpa->allocated[n] ? mpa->allocated[n]->size / mpa->sstride : 0;
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

// Copy fields that describe characteristics of the audio frame, but which are
// not part of the core format (format/channels/rate), and not part of the
// data (samples).
void mp_audio_copy_attributes(struct mp_audio *dst, struct mp_audio *src)
{
    // nothing yet
}

// Set data to the audio after the given number of samples (i.e. slice it).
void mp_audio_skip_samples(struct mp_audio *data, int samples)
{
    assert(samples >= 0 && samples <= data->samples);

    for (int n = 0; n < data->num_planes; n++)
        data->planes[n] = (uint8_t *)data->planes[n] + samples * data->sstride;

    data->samples -= samples;
}

// Return false if the frame data is shared, true otherwise.
// Will return true for non-refcounted frames.
bool mp_audio_is_writeable(struct mp_audio *data)
{
    bool ok = !data->readonly;
    for (int n = 0; n < MP_NUM_CHANNELS; n++) {
        if (data->allocated[n])
            ok &= av_buffer_is_writable(data->allocated[n]);
    }
    return ok;
}

static void mp_audio_steal_data(struct mp_audio *dst, struct mp_audio *src)
{
    talloc_set_destructor(dst, mp_audio_destructor);
    mp_audio_destructor(dst);
    *dst = *src;
    talloc_set_destructor(src, NULL);
    talloc_free(src);
}

// Make sure the frame owns the audio data, and if not, copy the data.
// Return negative value on failure (which means it can't be made writeable).
// Non-refcounted frames are always considered writeable.
int mp_audio_make_writeable(struct mp_audio *data)
{
    if (!mp_audio_is_writeable(data)) {
        struct mp_audio *new = talloc(NULL, struct mp_audio);
        *new = *data;
        mp_audio_set_null_data(new); // use format only
        mp_audio_realloc(new, data->samples);
        new->samples = data->samples;
        mp_audio_copy(new, 0, data, 0, data->samples);
        mp_audio_steal_data(data, new);
    }
    return 0;
}

struct mp_audio *mp_audio_from_avframe(struct AVFrame *avframe)
{
    AVFrame *tmp = NULL;
    struct mp_audio *new = talloc_zero(NULL, struct mp_audio);
    talloc_set_destructor(new, mp_audio_destructor);

    mp_audio_set_format(new, af_from_avformat(avframe->format));

    struct mp_chmap lavc_chmap;
    mp_chmap_from_lavc(&lavc_chmap, avframe->channel_layout);

#if LIBAVUTIL_VERSION_MICRO >= 100
    // FFmpeg being special again
    if (lavc_chmap.num != avframe->channels)
        mp_chmap_from_channels(&lavc_chmap, avframe->channels);
#endif

    new->rate = avframe->sample_rate;

    mp_audio_set_channels(new, &lavc_chmap);

    // Force refcounted frame.
    if (!avframe->buf[0]) {
        tmp = av_frame_alloc();
        if (!tmp)
            goto fail;
        if (av_frame_ref(tmp, avframe) < 0)
            goto fail;
        avframe = tmp;
    }

    // If we can't handle the format (e.g. too many channels), bail out.
    if (!mp_audio_config_valid(new) || avframe->nb_extended_buf)
        goto fail;

    for (int n = 0; n < AV_NUM_DATA_POINTERS; n++) {
        if (!avframe->buf[n])
            break;
        if (n >= MP_NUM_CHANNELS)
            goto fail;
        new->allocated[n] = av_buffer_ref(avframe->buf[n]);
        if (!new->allocated[n])
            goto fail;
    }

    for (int n = 0; n < new->num_planes; n++)
        new->planes[n] = avframe->data[n];
    new->samples = avframe->nb_samples;

    return new;

fail:
    talloc_free(new);
    av_frame_free(&tmp);
    return NULL;
}

struct mp_audio_pool {
    AVBufferPool *avpool;
    int element_size;
};

struct mp_audio_pool *mp_audio_pool_create(void *ta_parent)
{
    return talloc_zero(ta_parent, struct mp_audio_pool);
}

static void mp_audio_pool_destructor(void *p)
{
    struct mp_audio_pool *pool = p;
    av_buffer_pool_uninit(&pool->avpool);
}

// Allocate data using the given format and number of samples.
// Returns NULL on error.
struct mp_audio *mp_audio_pool_get(struct mp_audio_pool *pool,
                                   const struct mp_audio *fmt, int samples)
{
    int size = get_plane_size(fmt, samples);
    if (size < 0)
        return NULL;
    if (!pool->avpool || size > pool->element_size) {
        size_t alloc = ta_calc_prealloc_elems(size);
        if (alloc >= INT_MAX)
            return NULL;
        av_buffer_pool_uninit(&pool->avpool);
        pool->element_size = alloc;
        pool->avpool = av_buffer_pool_init(pool->element_size, NULL);
        if (!pool->avpool)
            return NULL;
        talloc_set_destructor(pool, mp_audio_pool_destructor);
    }
    struct mp_audio *new = talloc_ptrtype(NULL, new);
    talloc_set_destructor(new, mp_audio_destructor);
    *new = *fmt;
    mp_audio_set_null_data(new);
    new->samples = samples;
    for (int n = 0; n < new->num_planes; n++) {
        new->allocated[n] = av_buffer_pool_get(pool->avpool);
        if (!new->allocated[n]) {
            talloc_free(new);
            return NULL;
        }
        new->planes[n] = new->allocated[n]->data;
    }
    return new;
}

// Return a copy of the given frame.
// Returns NULL on error.
struct mp_audio *mp_audio_pool_new_copy(struct mp_audio_pool *pool,
                                        struct mp_audio *frame)
{
    struct mp_audio *new = mp_audio_pool_get(pool, frame, frame->samples);
    if (new) {
        mp_audio_copy(new, 0, frame, 0, new->samples);
        mp_audio_copy_attributes(new, frame);
    }
    return new;
}

// Exactly like mp_audio_make_writeable(), but get the data from the pool.
int mp_audio_pool_make_writeable(struct mp_audio_pool *pool,
                                 struct mp_audio *data)
{
    if (mp_audio_is_writeable(data))
        return 0;
    struct mp_audio *new = mp_audio_pool_get(pool, data, data->samples);
    if (!new)
        return -1;
    mp_audio_copy(new, 0, data, 0, data->samples);
    mp_audio_copy_attributes(new, data);
    mp_audio_steal_data(data, new);
    return 0;
}
