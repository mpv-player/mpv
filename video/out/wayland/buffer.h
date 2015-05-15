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

#ifndef MPLAYER_WAYLAND_BUFFER_H
#define MPLAYER_WAYLAND_BUFFER_H

#include <libavutil/common.h>
#include "video/sws_utils.h"
#include "video/img_format.h"
#include "video/out/wayland_common.h"

#define SHM_BUFFER_STRIDE(width, bytes) \
    FFALIGN((width) * (bytes), SWS_MIN_BYTE_ALIGN)

typedef struct format {
    enum wl_shm_format wl_format;
    enum mp_imgfmt     mp_format;
} format_t;

int8_t format_get_bytes(const format_t *fmt);

typedef enum shm_buffer_flags {
    SHM_BUFFER_BUSY         = 1 << 0, // in use by the compositor
    SHM_BUFFER_DIRTY        = 1 << 1, // buffer contains new content
    SHM_BUFFER_ONESHOT      = 1 << 2, // free after release
    SHM_BUFFER_RESIZE_LATER = 1 << 3, // free after release
} shm_buffer_flags_t;

#define SHM_BUFFER_IS_BUSY(b)        (!!((b)->flags & SHM_BUFFER_BUSY))
#define SHM_BUFFER_IS_DIRTY(b)       (!!((b)->flags & SHM_BUFFER_DIRTY))
#define SHM_BUFFER_IS_ONESHOT(b)     (!!((b)->flags & SHM_BUFFER_ONESHOT))
#define SHM_BUFFER_PENDING_RESIZE(b) (!!((b)->flags & SHM_BUFFER_RESIZE_LATER))

#define SHM_BUFFER_SET_BUSY(b)      (b)->flags |= SHM_BUFFER_BUSY
#define SHM_BUFFER_SET_DIRTY(b)     (b)->flags |= SHM_BUFFER_DIRTY
#define SHM_BUFFER_SET_ONESHOT(b)   (b)->flags |= SHM_BUFFER_ONESHOT
#define SHM_BUFFER_SET_PNDNG_RSZ(b) (b)->flags |= SHM_BUFFER_RESIZE_LATER

#define SHM_BUFFER_CLEAR_BUSY(b)      (b)->flags &= ~SHM_BUFFER_BUSY
#define SHM_BUFFER_CLEAR_DIRTY(b)     (b)->flags &= ~SHM_BUFFER_DIRTY
#define SHM_BUFFER_CLEAR_ONESHOT(b)   (b)->flags &= ~SHM_BUFFER_ONESHOT
#define SHM_BUFFER_CLEAR_PNDNG_RSZ(b) (b)->flags &= ~SHM_BUFFER_RESIZE_LATER

typedef struct buffer {
    struct wl_buffer *buffer;

    int flags;

    uint32_t height;
    uint32_t stride;
    uint32_t bytes; // bytes per pixel
    // width = stride / bytes per pixel
    // size = stride * height

    struct wl_shm_pool *shm_pool; // for growing buffers;

    int fd;
    void *data;
    uint32_t pool_size; // size of pool and data XXX
                        // pool_size can be far bigger than the buffer size

    format_t format;

    uint32_t pending_height;
    uint32_t pending_width;
} shm_buffer_t;

shm_buffer_t* shm_buffer_create(uint32_t width,
                                uint32_t height,
                                format_t fmt,
                                struct wl_shm *shm,
                                const struct wl_buffer_listener *listener);

// shm pool is only able to grow and won't shrink
// returns 0 on success or buffer flags indicating the buffer status which
// prevent it from resizing
int shm_buffer_resize(shm_buffer_t *buffer, uint32_t width, uint32_t height);

// if shm_buffer_resize returns SHM_BUFFER_BUSY this function can be called
// after the buffer is released to resize it afterwards
// returns 0 if no pending resize flag was set and -1 on errors
int shm_buffer_pending_resize(shm_buffer_t *buffer);

// buffer is freed, don't use the buffer after calling this function on it
void shm_buffer_destroy(shm_buffer_t *buffer);

#endif /* MPLAYER_WAYLAND_BUFFER_H */
