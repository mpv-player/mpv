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

#include <errno.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_mode.h>

#include "common/common.h"
#include "common/msg.h"
#include "drm_common.h"
#include "drm_prime.h"

int drm_prime_create_framebuffer(struct mp_log *log, int fd,
                                 AVDRMFrameDescriptor *descriptor, int width,
                                 int height, struct drm_prime_framebuffer *framebuffer,
                                 struct drm_prime_handle_refs *handle_refs)
{
    AVDRMLayerDescriptor *layer = NULL;
    uint32_t pitches[4] = { 0 };
    uint32_t offsets[4] = { 0 };
    uint32_t handles[4] = { 0 };
    uint64_t modifiers[4] = { 0 };
    int ret, layer_fd;

    if (descriptor && descriptor->nb_layers) {
        *framebuffer = (struct drm_prime_framebuffer){0};

        for (int object = 0; object < descriptor->nb_objects; object++) {
            ret = drmPrimeFDToHandle(fd, descriptor->objects[object].fd,
                                     &framebuffer->gem_handles[object]);
            if (ret < 0) {
                mp_err(log, "Failed to retrieve the Prime Handle from handle %d (%d).\n",
                       object, descriptor->objects[object].fd);
                goto fail;
            }
            modifiers[object] = descriptor->objects[object].format_modifier;
        }

        layer = &descriptor->layers[0];

        for (int plane = 0; plane < AV_DRM_MAX_PLANES; plane++) {
            layer_fd = framebuffer->gem_handles[layer->planes[plane].object_index];
            if (layer_fd && layer->planes[plane].pitch) {
                pitches[plane] = layer->planes[plane].pitch;
                offsets[plane] = layer->planes[plane].offset;
                handles[plane] = layer_fd;
            } else {
                pitches[plane] = 0;
                offsets[plane] = 0;
                handles[plane] = 0;
                modifiers[plane] = 0;
            }
        }

        ret = drmModeAddFB2WithModifiers(fd, width, height, layer->format,
                                         handles, pitches, offsets,
                                         modifiers, &framebuffer->fb_id,
                                         DRM_MODE_FB_MODIFIERS);
        if (ret < 0) {
            ret = drmModeAddFB2(fd, width, height, layer->format,
                                handles, pitches, offsets,
                                &framebuffer->fb_id, 0);
            if (ret < 0) {
                mp_err(log, "Failed to create framebuffer with drmModeAddFB2 on layer %d: %s\n",
                        0, mp_strerror(errno));
                goto fail;
            }
        }

        for (int plane = 0; plane < AV_DRM_MAX_PLANES; plane++) {
            drm_prime_add_handle_ref(handle_refs, framebuffer->gem_handles[plane]);
        }
   }

   return 0;

fail:
   memset(framebuffer, 0, sizeof(*framebuffer));
   return -1;
}

void drm_prime_destroy_framebuffer(struct mp_log *log, int fd,
                                   struct drm_prime_framebuffer *framebuffer,
                                   struct drm_prime_handle_refs *handle_refs)
{
    if (framebuffer->fb_id)
        drmModeRmFB(fd, framebuffer->fb_id);

    for (int i = 0; i < AV_DRM_MAX_PLANES; i++) {
        if (framebuffer->gem_handles[i]) {
            drm_prime_remove_handle_ref(handle_refs,
                                        framebuffer->gem_handles[i]);
            if (!drm_prime_get_handle_ref_count(handle_refs,
                                                framebuffer->gem_handles[i])) {
                drmIoctl(fd, DRM_IOCTL_GEM_CLOSE, &framebuffer->gem_handles[i]);
            }
        }
    }

    memset(framebuffer, 0, sizeof(*framebuffer));
}

void drm_prime_init_handle_ref_count(void *talloc_parent,
    struct drm_prime_handle_refs *handle_refs)
{
    handle_refs->handle_ref_count = talloc_zero(talloc_parent, uint32_t);
    handle_refs->size = 1;
    handle_refs->ctx = talloc_parent;
}

void drm_prime_add_handle_ref(struct drm_prime_handle_refs *handle_refs,
                              uint32_t handle)
{
    if (handle) {
        if (handle > handle_refs->size) {
            handle_refs->size = handle;
            MP_TARRAY_GROW(handle_refs->ctx, handle_refs->handle_ref_count,
                           handle_refs->size);
        }
        handle_refs->handle_ref_count[handle - 1]++;
    }
}

void drm_prime_remove_handle_ref(struct drm_prime_handle_refs *handle_refs,
                                 uint32_t handle)
{
    if (handle) {
        if (handle <= handle_refs->size &&
             handle_refs->handle_ref_count[handle - 1])
        {
             handle_refs->handle_ref_count[handle - 1]--;
        }
    }
}

uint32_t drm_prime_get_handle_ref_count(struct drm_prime_handle_refs *handle_refs,
                                        uint32_t handle)
{
    if (handle) {
        if (handle <= handle_refs->size)
            return handle_refs->handle_ref_count[handle - 1];
    }
    return 0;
}
