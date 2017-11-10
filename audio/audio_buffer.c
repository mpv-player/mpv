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

#include <stddef.h>
#include <limits.h>
#include <assert.h>

#include "common/common.h"

#include "chmap.h"
#include "audio_buffer.h"
#include "format.h"

struct mp_audio_buffer {
    int format;
    struct mp_chmap channels;
    int srate;
    int sstride;
    int num_planes;
    uint8_t *data[MP_NUM_CHANNELS];
    int allocated;
    int num_samples;
};

struct mp_audio_buffer *mp_audio_buffer_create(void *talloc_ctx)
{
    return talloc_zero(talloc_ctx, struct mp_audio_buffer);
}

// Reinitialize the buffer, set a new format, drop old data.
// The audio data in fmt is not used, only the format.
void mp_audio_buffer_reinit_fmt(struct mp_audio_buffer *ab, int format,
                                const struct mp_chmap *channels, int srate)
{
    for (int n = 0; n < MP_NUM_CHANNELS; n++)
        TA_FREEP(&ab->data[n]);
    ab->format = format;
    ab->channels = *channels;
    ab->srate = srate;
    ab->allocated = 0;
    ab->num_samples = 0;
    ab->sstride = af_fmt_to_bytes(ab->format);
    ab->num_planes = 1;
    if (af_fmt_is_planar(ab->format)) {
        ab->num_planes = ab->channels.num;
    } else {
        ab->sstride *= ab->channels.num;
    }
}

// Make the total size of the internal buffer at least this number of samples.
void mp_audio_buffer_preallocate_min(struct mp_audio_buffer *ab, int samples)
{
    if (samples > ab->allocated) {
        for (int n = 0; n < ab->num_planes; n++) {
            ab->data[n] = talloc_realloc(ab, ab->data[n], char,
                                         ab->sstride * samples);
        }
        ab->allocated = samples;
    }
}

// Get number of samples that can be written without forcing a resize of the
// internal buffer.
int mp_audio_buffer_get_write_available(struct mp_audio_buffer *ab)
{
    return ab->allocated - ab->num_samples;
}

// All integer parameters are in samples.
// dst and src can overlap.
static void copy_planes(struct mp_audio_buffer *ab,
                        uint8_t **dst, int dst_offset,
                        uint8_t **src, int src_offset, int length)
{
    for (int n = 0; n < ab->num_planes; n++) {
        memmove((char *)dst[n] + dst_offset * ab->sstride,
                (char *)src[n] + src_offset * ab->sstride,
                length * ab->sstride);
    }
}

// Append data to the end of the buffer.
// If the buffer is not large enough, it is transparently resized.
void mp_audio_buffer_append(struct mp_audio_buffer *ab, void **ptr, int samples)
{
    mp_audio_buffer_preallocate_min(ab, ab->num_samples + samples);
    copy_planes(ab, ab->data, ab->num_samples, (uint8_t **)ptr, 0, samples);
    ab->num_samples += samples;
}

// Prepend silence to the start of the buffer.
void mp_audio_buffer_prepend_silence(struct mp_audio_buffer *ab, int samples)
{
    assert(samples >= 0);
    mp_audio_buffer_preallocate_min(ab, ab->num_samples + samples);
    copy_planes(ab, ab->data, samples, ab->data, 0, ab->num_samples);
    ab->num_samples += samples;
    for (int n = 0; n < ab->num_planes; n++)
        af_fill_silence(ab->data[n], samples * ab->sstride, ab->format);
}

void mp_audio_buffer_duplicate(struct mp_audio_buffer *ab, int samples)
{
    assert(samples >= 0 && samples <= ab->num_samples);
    mp_audio_buffer_preallocate_min(ab, ab->num_samples + samples);
    copy_planes(ab, ab->data, ab->num_samples,
                ab->data, ab->num_samples - samples, samples);
    ab->num_samples += samples;
}

// Get the start of the current readable buffer.
void mp_audio_buffer_peek(struct mp_audio_buffer *ab, uint8_t ***ptr,
                          int *samples)
{
    *ptr = ab->data;
    *samples = ab->num_samples;
}

// Skip leading samples. (Used with mp_audio_buffer_peek() to read data.)
void mp_audio_buffer_skip(struct mp_audio_buffer *ab, int samples)
{
    assert(samples >= 0 && samples <= ab->num_samples);
    copy_planes(ab, ab->data, 0, ab->data, samples, ab->num_samples - samples);
    ab->num_samples -= samples;
}

void mp_audio_buffer_clear(struct mp_audio_buffer *ab)
{
    ab->num_samples = 0;
}

// Return number of buffered audio samples
int mp_audio_buffer_samples(struct mp_audio_buffer *ab)
{
    return ab->num_samples;
}

// Return amount of buffered audio in seconds.
double mp_audio_buffer_seconds(struct mp_audio_buffer *ab)
{
    return ab->num_samples / (double)ab->srate;
}
