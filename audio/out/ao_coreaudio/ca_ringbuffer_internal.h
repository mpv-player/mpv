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

#ifndef MPV_AUDIO_OUT_CA_RINGBUFFER_H
#define MPV_AUDIO_OUT_CA_RINGBUFFER_H

struct ca_ringbuffer;

struct ca_ringbuffer *ca_ringbuffer_new(void *talloc_ctx, int chunks, int chunk_size);
struct ca_ringbuffer *ca_ringbuffer_new2(void *talloc_ctx, int bps, int chunk_size);
int ca_ringbuffer_read(struct ca_ringbuffer *buffer, unsigned char *data, int len);
int ca_ringbuffer_write(struct ca_ringbuffer *buffer, unsigned char *data, int len);

void ca_ringbuffer_reset(struct ca_ringbuffer *buffer);

int ca_ringbuffer_available(struct ca_ringbuffer *buffer);
int ca_ringbuffer_size(struct ca_ringbuffer *buffer);
int ca_ringbuffer_buffered(struct ca_ringbuffer *buffer);
int ca_ringbuffer_chunk_size(struct ca_ringbuffer *buffer);

char *ca_ringbuffer_repr(struct ca_ringbuffer *buffer, void *talloc_ctx);

#endif
