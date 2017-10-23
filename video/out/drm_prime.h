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

#ifndef DRM_PRIME_H
#define DRM_PRIME_H

#include <libavutil/hwcontext_drm.h>

#include "common/msg.h"

struct drm_prime_framebuffer {
    uint32_t fb_id;
    uint32_t gem_handles[AV_DRM_MAX_PLANES];
};

int drm_prime_create_framebuffer(struct mp_log *log, int fd, AVDRMFrameDescriptor *descriptor, int width, int height,
                                  struct  drm_prime_framebuffer *framebuffers);
void drm_prime_destroy_framebuffer(struct mp_log *log, int fd, struct  drm_prime_framebuffer *framebuffers);
#endif // DRM_PRIME_H
