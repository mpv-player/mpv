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

struct drm_prime_handle_refs {
    uint32_t *handle_ref_count;
    size_t size;
    void *ctx;
};

int drm_prime_create_framebuffer(struct mp_log *log, int fd, AVDRMFrameDescriptor *descriptor, int width, int height,
                                 struct  drm_prime_framebuffer *framebuffers,
                                 struct drm_prime_handle_refs *handle_refs);
void drm_prime_destroy_framebuffer(struct mp_log *log, int fd, struct  drm_prime_framebuffer *framebuffers,
                                   struct  drm_prime_handle_refs *handle_refs);
void drm_prime_init_handle_ref_count(void *talloc_parent, struct drm_prime_handle_refs *handle_refs);
void drm_prime_add_handle_ref(struct drm_prime_handle_refs *handle_refs, uint32_t handle);
void drm_prime_remove_handle_ref(struct drm_prime_handle_refs *handle_refs, uint32_t handle);
uint32_t drm_prime_get_handle_ref_count(struct drm_prime_handle_refs *handle_refs, uint32_t handle);
#endif // DRM_PRIME_H
