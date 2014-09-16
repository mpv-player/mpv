/*
 * This file is part of mpv video player.
 * Copyright © 2014 Alexander Preisinger <alexander.preisinger@gmail.com>
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

#include "buffer.h"
#include "memfile.h"

#include <unistd.h>
#include <sys/mman.h>

int8_t format_get_bytes(const format_t *fmt)
{
    return mp_imgfmt_get_desc(fmt->mp_format).bytes[0];
}

shm_buffer_t* shm_buffer_create(uint32_t width,
                                uint32_t height,
                                format_t fmt,
                                struct wl_shm *shm,
                                const struct wl_buffer_listener *listener)
{
    int8_t bytes = format_get_bytes(&fmt);
    uint32_t stride = SHM_BUFFER_STRIDE(width, bytes);
    uint32_t size = stride * height;

    shm_buffer_t *buffer = calloc(1, sizeof(shm_buffer_t));
    int fd = memfile_create(size);

    if (fd < 0)
        return NULL;

    buffer->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (buffer->data == MAP_FAILED) {
        close(fd);
        return NULL;
    }

    buffer->shm_pool = wl_shm_create_pool(shm, fd, size);
    buffer->buffer = wl_shm_pool_create_buffer(buffer->shm_pool,
                                               0, width, height, stride,
                                               fmt.wl_format);

    wl_buffer_add_listener(buffer->buffer, listener, buffer);

    buffer->fd = fd;
    buffer->height = height;
    buffer->stride = stride;
    buffer->format = fmt;
    buffer->bytes = bytes;
    buffer->pool_size = size;
    buffer->pending_height = 0;
    buffer->pending_width = 0;

    return buffer;
}

int shm_buffer_resize(shm_buffer_t *buffer, uint32_t width, uint32_t height)
{
    uint32_t new_stride = SHM_BUFFER_STRIDE(width, buffer->bytes);
    uint32_t new_size = new_stride * height;

    if (SHM_BUFFER_IS_BUSY(buffer)) {
        SHM_BUFFER_SET_PNDNG_RSZ(buffer);
        buffer->pending_width = width;
        buffer->pending_height = height;
        return SHM_BUFFER_BUSY;
    }

    SHM_BUFFER_CLEAR_PNDNG_RSZ(buffer);

    if (new_size > buffer->pool_size) {
        munmap(buffer->data, buffer->pool_size);
        ftruncate(buffer->fd, new_size);

        buffer->data = mmap(NULL, new_size, PROT_READ | PROT_WRITE,
                            MAP_SHARED, buffer->fd, 0);

        // TODO: the buffer should be destroyed when -1 is return
        if (buffer->data == MAP_FAILED)
            return -1;

        wl_shm_pool_resize(buffer->shm_pool, new_size);
        buffer->pool_size = new_size;
    }

    const void *listener = wl_proxy_get_listener((struct wl_proxy*)buffer->buffer);

    wl_buffer_destroy(buffer->buffer);
    buffer->buffer = wl_shm_pool_create_buffer(buffer->shm_pool,
                                               0, width, height, new_stride,
                                               buffer->format.wl_format);

    wl_buffer_add_listener(buffer->buffer, listener, buffer);

    buffer->height = height;
    buffer->stride = new_stride;

    return 0;
}

int shm_buffer_pending_resize(shm_buffer_t *buffer)
{
    if (SHM_BUFFER_PENDING_RESIZE(buffer)) {
        SHM_BUFFER_CLEAR_PNDNG_RSZ(buffer);
        return shm_buffer_resize(buffer, buffer->pending_width, buffer->pending_height);
    }
    else {
        return 0;
    }
}

void shm_buffer_destroy(shm_buffer_t *buffer)
{
    if (!buffer)
        return;

    wl_buffer_destroy(buffer->buffer);
    wl_shm_pool_destroy(buffer->shm_pool);
    munmap(buffer->data, buffer->pool_size);
    close(buffer->fd);
    free(buffer);
}
