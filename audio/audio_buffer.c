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

#include <stddef.h>
#include <limits.h>
#include <assert.h>

#include "common/common.h"

#include "audio_buffer.h"
#include "audio.h"
#include "format.h"

struct mp_audio_buffer {
    struct mp_audio *buffer;
};

struct mp_audio_buffer *mp_audio_buffer_create(void *talloc_ctx)
{
    struct mp_audio_buffer *ab = talloc(talloc_ctx, struct mp_audio_buffer);
    *ab = (struct mp_audio_buffer) {
        .buffer = talloc_zero(ab, struct mp_audio),
    };
    return ab;
}

// Reinitialize the buffer, set a new format, drop old data.
// The audio data in fmt is not used, only the format.
void mp_audio_buffer_reinit(struct mp_audio_buffer *ab, struct mp_audio *fmt)
{
    mp_audio_copy_config(ab->buffer, fmt);
    mp_audio_realloc(ab->buffer, 1);
    ab->buffer->samples = 0;
}

void mp_audio_buffer_reinit_fmt(struct mp_audio_buffer *ab, int format,
                                const struct mp_chmap *channels, int srate)
{
    struct mp_audio mpa = {0};
    mp_audio_set_format(&mpa, format);
    mp_audio_set_channels(&mpa, channels);
    mpa.rate = srate;
    mp_audio_buffer_reinit(ab, &mpa);
}

void mp_audio_buffer_get_format(struct mp_audio_buffer *ab,
                                struct mp_audio *out_fmt)
{
    *out_fmt = (struct mp_audio){0};
    mp_audio_copy_config(out_fmt, ab->buffer);
}

// Make the total size of the internal buffer at least this number of samples.
void mp_audio_buffer_preallocate_min(struct mp_audio_buffer *ab, int samples)
{
    mp_audio_realloc_min(ab->buffer, samples);
}

// Get number of samples that can be written without forcing a resize of the
// internal buffer.
int mp_audio_buffer_get_write_available(struct mp_audio_buffer *ab)
{
    return mp_audio_get_allocated_size(ab->buffer) - ab->buffer->samples;
}

// Get a pointer to the end of the buffer (where writing would append). If the
// internal buffer is too small for the given number of samples, it's resized.
// After writing to the buffer, mp_audio_buffer_finish_write() has to be used
// to make the written data part of the readable buffer.
void mp_audio_buffer_get_write_buffer(struct mp_audio_buffer *ab, int samples,
                                      struct mp_audio *out_buffer)
{
    assert(samples >= 0);
    mp_audio_realloc_min(ab->buffer, ab->buffer->samples + samples);
    *out_buffer = *ab->buffer;
    out_buffer->samples = ab->buffer->samples + samples;
    mp_audio_skip_samples(out_buffer, ab->buffer->samples);
}

void mp_audio_buffer_finish_write(struct mp_audio_buffer *ab, int samples)
{
    assert(samples >= 0 && samples <= mp_audio_buffer_get_write_available(ab));
    ab->buffer->samples += samples;
}

// Append data to the end of the buffer.
// If the buffer is not large enough, it is transparently resized.
// For now always copies the data.
void mp_audio_buffer_append(struct mp_audio_buffer *ab, struct mp_audio *mpa)
{
    int offset = ab->buffer->samples;
    ab->buffer->samples += mpa->samples;
    mp_audio_realloc_min(ab->buffer, ab->buffer->samples);
    mp_audio_copy(ab->buffer, offset, mpa, 0, mpa->samples);
}

// Prepend silence to the start of the buffer.
void mp_audio_buffer_prepend_silence(struct mp_audio_buffer *ab, int samples)
{
    assert(samples >= 0);
    int oldlen = ab->buffer->samples;
    ab->buffer->samples += samples;
    mp_audio_realloc_min(ab->buffer, ab->buffer->samples);
    mp_audio_copy(ab->buffer, samples, ab->buffer, 0, oldlen);
    mp_audio_fill_silence(ab->buffer, 0, samples);
}

// Get the start of the current readable buffer.
void mp_audio_buffer_peek(struct mp_audio_buffer *ab, struct mp_audio *out_mpa)
{
    *out_mpa = *ab->buffer;
}

// Skip leading samples. (Used with mp_audio_buffer_peek() to read data.)
void mp_audio_buffer_skip(struct mp_audio_buffer *ab, int samples)
{
    assert(samples >= 0 && samples <= ab->buffer->samples);
    mp_audio_copy(ab->buffer, 0, ab->buffer, samples,
                  ab->buffer->samples - samples);
    ab->buffer->samples -= samples;
}

void mp_audio_buffer_clear(struct mp_audio_buffer *ab)
{
    ab->buffer->samples = 0;
}

// Return number of buffered audio samples
int mp_audio_buffer_samples(struct mp_audio_buffer *ab)
{
    return ab->buffer->samples;
}

// Return amount of buffered audio in seconds.
double mp_audio_buffer_seconds(struct mp_audio_buffer *ab)
{
    return ab->buffer->samples / (double)ab->buffer->rate;
}
