/*
 * This file is part of mpv.
 * Copyright (c) 2012 wm4
 * Copyright (c) 2013 Stefano Pigozzi <stefano.pigozzi@gmail.com>
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

#include <inttypes.h>
#include <libavutil/common.h>
#include <assert.h>
#include "talloc.h"
#include "mpvcore/mp_memory_barrier.h"
#include "mpvcore/mp_ring.h"

struct mp_ring {
    uint8_t  *buffer;

    /* Positions of thes first readable/writeable chunks. Do not read this
     * fields but use the atomic private accessors `mp_ring_get_wpos`
     * and `mp_ring_get_rpos`. */
    uint32_t rpos, wpos;
};

static uint32_t mp_ring_get_wpos(struct mp_ring *buffer)
{
    mp_memory_barrier();
    return buffer->wpos;
}

static uint32_t mp_ring_get_rpos(struct mp_ring *buffer)
{
    mp_memory_barrier();
    return buffer->rpos;
}

struct mp_ring *mp_ring_new(void *talloc_ctx, int size)
{
    struct mp_ring *ringbuffer =
        talloc_zero(talloc_ctx, struct mp_ring);

    *ringbuffer = (struct mp_ring) {
        .buffer = talloc_size(talloc_ctx, size),
    };

    return ringbuffer;
}

int mp_ring_drain(struct mp_ring *buffer, int len)
{
    int buffered  = mp_ring_buffered(buffer);
    int drain_len = FFMIN(len, buffered);
    mp_atomic_add_and_fetch(&buffer->rpos, drain_len);
    mp_memory_barrier();
    return drain_len;
}

int mp_ring_read(struct mp_ring *buffer, unsigned char *dest, int len)
{
    if (!dest) return mp_ring_drain(buffer, len);

    int size     = mp_ring_size(buffer);
    int buffered = mp_ring_buffered(buffer);
    int read_len = FFMIN(len, buffered);
    int read_ptr = mp_ring_get_rpos(buffer) % size;

    int len1 = FFMIN(size - read_ptr, read_len);
    int len2 = read_len - len1;

    memcpy(dest, buffer->buffer + read_ptr, len1);
    memcpy(dest + len1, buffer->buffer, len2);

    mp_atomic_add_and_fetch(&buffer->rpos, read_len);
    mp_memory_barrier();

    return read_len;
}

int mp_ring_read_cb(struct mp_ring *buffer, void *ctx, int len,
        void (*func)(void*, void*, int))
{
    // The point of this function is defining custom read behaviour, assume
    // it's a programmers error if func is null.
    assert(func);

    int size     = mp_ring_size(buffer);
    int buffered = mp_ring_buffered(buffer);
    int read_len = FFMIN(len, buffered);
    int read_ptr = mp_ring_get_rpos(buffer) % size;

    func(ctx, buffer->buffer + read_ptr, read_len);

    return mp_ring_drain(buffer, read_len);
}

int mp_ring_write(struct mp_ring *buffer, unsigned char *src, int len)
{
    int size      = mp_ring_size(buffer);
    int free      = mp_ring_available(buffer);
    int write_len = FFMIN(len, free);
    int write_ptr = mp_ring_get_wpos(buffer) % size;

    int len1 = FFMIN(size - write_ptr, write_len);
    int len2 = write_len - len1;

    memcpy(buffer->buffer + write_ptr, src, len1);
    memcpy(buffer->buffer, src + len1, len2);

    mp_atomic_add_and_fetch(&buffer->wpos, write_len);
    mp_memory_barrier();

    return write_len;
}

void mp_ring_reset(struct mp_ring *buffer)
{
    buffer->wpos = buffer->rpos = 0;
    mp_memory_barrier();
}

int mp_ring_available(struct mp_ring *buffer)
{
    return mp_ring_size(buffer) - mp_ring_buffered(buffer);
}

int mp_ring_size(struct mp_ring *buffer)
{
    return talloc_get_size(buffer->buffer);
}

int mp_ring_buffered(struct mp_ring *buffer)
{
    return (mp_ring_get_wpos(buffer) - mp_ring_get_rpos(buffer));
}

char *mp_ring_repr(struct mp_ring *buffer, void *talloc_ctx)
{
    return talloc_asprintf(
        talloc_ctx,
        "Ringbuffer { .size = %dB, .buffered = %dB, .available = %dB }",
        mp_ring_size(buffer),
        mp_ring_buffered(buffer),
        mp_ring_available(buffer));
}
