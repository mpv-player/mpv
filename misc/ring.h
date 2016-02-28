/*
 * This file is part of mpv.
 * Copyright (c) 2012 wm4
 * Copyright (c) 2013 Stefano Pigozzi <stefano.pigozzi@gmail.com>
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

#ifndef MPV_MP_RING_H
#define MPV_MP_RING_H

/**
 * A simple non-blocking SPSC (single producer, single consumer) ringbuffer
 * implementation. Thread safety is accomplished through atomic operations.
 */

struct mp_ring;

/**
 * Instantiate a new ringbuffer
 *
 * talloc_ctx: talloc context of the newly created object
 * size:       total size in bytes
 * return:     the newly created ringbuffer
 */
struct mp_ring *mp_ring_new(void *talloc_ctx, int size);

/**
 * Read data from the ringbuffer
 *
 * buffer: target ringbuffer instance
 * dest:   destination buffer for the read data. If NULL read data is discarded.
 * len:    maximum number of bytes to read
 * return: number of bytes read
 */
int mp_ring_read(struct mp_ring *buffer, unsigned char *dest, int len);

/**
 * Write data to the ringbuffer
 *
 * buffer: target ringbuffer instance
 * src:    source buffer for the write data
 * len:    maximum number of bytes to write
 * return: number of bytes written
 */
int mp_ring_write(struct mp_ring *buffer, unsigned char *src, int len);

/**
 * Drain data from the ringbuffer
 *
 * buffer: target ringbuffer instance
 * len:    maximum number of bytes to drain
 * return: number of bytes drained
 */
int mp_ring_drain(struct mp_ring *buffer, int len);

/**
 * Reset the ringbuffer discarding any content
 *
 * buffer: target ringbuffer instance
 */
void mp_ring_reset(struct mp_ring *buffer);

/**
 * Get the available size for writing
 *
 * buffer: target ringbuffer instance
 * return: number of bytes that can be written
 */
int mp_ring_available(struct mp_ring *buffer);

/**
 * Get the total size
 *
 * buffer: target ringbuffer instance
 * return: total ringbuffer size in bytes
 */
int mp_ring_size(struct mp_ring *buffer);

/**
 * Get the available size for reading
 *
 * buffer: target ringbuffer instance
 * return: number of bytes ready for reading
 */
int mp_ring_buffered(struct mp_ring *buffer);

/**
 * Get a string representation of the ringbuffer
 *
 * buffer:     target ringbuffer instance
 * talloc_ctx: talloc context of the newly created string
 * return:     string representing the ringbuffer
 */
char *mp_ring_repr(struct mp_ring *buffer, void *talloc_ctx);

#endif
