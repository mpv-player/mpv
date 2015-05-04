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

#ifndef MP_AUDIO_H
#define MP_AUDIO_H

#include "format.h"
#include "chmap.h"

// Audio data chunk
struct mp_audio {
    int samples;        // number of samples in data (per channel)
    void *planes[MP_NUM_CHANNELS]; // data buffer (one per plane)
    int rate;           // sample rate
    struct mp_chmap channels; // channel layout, use mp_audio_set_*() to set
    int format; // format (AF_FORMAT_...), use mp_audio_set_format() to set
    // Redundant fields, for convenience
    int sstride;        // distance between 2 samples in bytes on a plane
                        //  interleaved: bps * nch
                        //  planar:      bps
    int nch;            // number of channels (redundant with chmap)
    int spf;            // sub-samples per sample on each plane
    int num_planes;     // number of planes
    int bps;            // size of sub-samples (af_fmt2bps(format))

    // private
    bool readonly;
    struct AVBufferRef *allocated[MP_NUM_CHANNELS];
};

void mp_audio_set_format(struct mp_audio *mpa, int format);
void mp_audio_set_num_channels(struct mp_audio *mpa, int num_channels);
void mp_audio_set_channels_old(struct mp_audio *mpa, int num_channels);
void mp_audio_set_channels(struct mp_audio *mpa, const struct mp_chmap *chmap);
void mp_audio_copy_config(struct mp_audio *dst, const struct mp_audio *src);
bool mp_audio_config_equals(const struct mp_audio *a, const struct mp_audio *b);
bool mp_audio_config_valid(const struct mp_audio *mpa);

char *mp_audio_config_to_str_buf(char *buf, size_t buf_sz, struct mp_audio *mpa);
#define mp_audio_config_to_str(m) mp_audio_config_to_str_buf((char[64]){0}, 64, (m))

void mp_audio_force_interleaved_format(struct mp_audio *mpa);

int mp_audio_psize(struct mp_audio *mpa);

void mp_audio_set_null_data(struct mp_audio *mpa);

void mp_audio_realloc(struct mp_audio *mpa, int samples);
void mp_audio_realloc_min(struct mp_audio *mpa, int samples);
int mp_audio_get_allocated_size(struct mp_audio *mpa);

void mp_audio_fill_silence(struct mp_audio *mpa, int start, int length);

void mp_audio_copy(struct mp_audio *dst, int dst_offset,
                   struct mp_audio *src, int src_offset, int length);
void mp_audio_copy_attributes(struct mp_audio *dst, struct mp_audio *src);
void mp_audio_skip_samples(struct mp_audio *data, int samples);

bool mp_audio_is_writeable(struct mp_audio *data);
int mp_audio_make_writeable(struct mp_audio *data);

struct AVFrame;
struct mp_audio *mp_audio_from_avframe(struct AVFrame *avframe);

struct mp_audio_pool;
struct mp_audio_pool *mp_audio_pool_create(void *ta_parent);
struct mp_audio *mp_audio_pool_get(struct mp_audio_pool *pool,
                                   const struct mp_audio *fmt, int samples);
struct mp_audio *mp_audio_pool_new_copy(struct mp_audio_pool *pool,
                                        struct mp_audio *frame);
int mp_audio_pool_make_writeable(struct mp_audio_pool *pool,
                                 struct mp_audio *frame);

#endif
