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

#include <libavutil/fifo.h>
#include "talloc.h"

#include "ca_ringbuffer_internal.h"

struct ca_ringbuffer {
    AVFifoBuffer *fifo;
    int len;
    int chunks;
    int chunk_size;
};

struct ca_ringbuffer *ca_ringbuffer_new(void *talloc_ctx, int chunks, int chunk_size)
{
    struct ca_ringbuffer *buffer =
        talloc_zero(talloc_ctx, struct ca_ringbuffer);

    *buffer = (struct ca_ringbuffer) {
        .fifo       = av_fifo_alloc(chunks * chunk_size),
        .len        = chunks * chunk_size,
        .chunks     = chunks,
        .chunk_size = chunk_size,
    };

    return buffer;
}

struct ca_ringbuffer *ca_ringbuffer_new2(void *talloc_ctx, int bps, int chunk_size)
{
    int chunks = (bps + chunk_size - 1) / chunk_size;
    return ca_ringbuffer_new(talloc_ctx, chunks, chunk_size);
}

int ca_ringbuffer_read(struct ca_ringbuffer *buffer,
                       unsigned char *data, int len)
{
    int buffered = ca_ringbuffer_buffered(buffer);
    if (len > buffered)
        len = buffered;
    if (data)
        av_fifo_generic_read(buffer->fifo, data, len, NULL);
    else
        av_fifo_drain(buffer->fifo, len);
    return len;
}

int ca_ringbuffer_write(struct ca_ringbuffer *buffer,
                        unsigned char *data, int len)
{
    int free = buffer->len - av_fifo_size(buffer->fifo);
    if (len > free)
        len = free;
    return av_fifo_generic_write(buffer->fifo, data, len, NULL);
}

void ca_ringbuffer_reset(struct ca_ringbuffer *buffer)
{
    av_fifo_reset(buffer->fifo);
}

int ca_ringbuffer_available(struct ca_ringbuffer *buffer)
{
    return ca_ringbuffer_size(buffer) - ca_ringbuffer_buffered(buffer);
}

int ca_ringbuffer_size(struct ca_ringbuffer *buffer)
{
    return buffer->len;
}

int ca_ringbuffer_buffered(struct ca_ringbuffer *buffer)
{
    return av_fifo_size(buffer->fifo);
}

int ca_ringbuffer_chunk_size(struct ca_ringbuffer *buffer)
{
    return buffer->chunk_size;
}

char *ca_ringbuffer_repr(struct ca_ringbuffer *buffer, void *talloc_ctx)
{
    return talloc_asprintf(
        talloc_ctx,
        "Ringbuffer { .chunks = %d bytes, .chunk_size = %d bytes, .size = %d bytes }",
        buffer->chunks,
        ca_ringbuffer_chunk_size(buffer),
        ca_ringbuffer_size(buffer));
}
