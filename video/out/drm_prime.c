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

#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "common/msg.h"
#include "drm_common.h"
#include "drm_prime.h"

int drm_prime_create_framebuffer(struct mp_log *log, int fd, AVDRMFrameDescriptor *descriptor, int width, int height,
                                  struct  drm_prime_framebuffer *framebuffer)
{
    AVDRMLayerDescriptor *layer = NULL;
    uint32_t pitches[4], offsets[4], handles[4];
    int ret, layer_fd;

    if (descriptor && descriptor->nb_layers) {
        *framebuffer = (struct drm_prime_framebuffer){0};

        for (int object = 0; object < descriptor->nb_objects; object++) {
            ret = drmPrimeFDToHandle(fd, descriptor->objects[object].fd, &framebuffer->gem_handles[object]);
            if (ret < 0) {
                mp_err(log, "Failed to retrieve the Prime Handle from handle %d (%d).\n", object, descriptor->objects[object].fd);
                goto fail;
            }
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
            }
        }

        ret = drmModeAddFB2(fd, width, height, layer->format,
                            handles, pitches, offsets, &framebuffer->fb_id, 0);
        if (ret < 0) {
            mp_err(log, "Failed to create framebuffer on layer %d.\n", 0);
            goto fail;
        }
    }

   return 0;

fail:
   memset(framebuffer, 0, sizeof(*framebuffer));
   return -1;
}

void drm_prime_destroy_framebuffer(struct mp_log *log, int fd, struct  drm_prime_framebuffer *framebuffer)
{
    if (framebuffer->fb_id)
        drmModeRmFB(fd, framebuffer->fb_id);

    for (int i = 0; i < AV_DRM_MAX_PLANES; i++) {
        if (framebuffer->gem_handles[i])
            drmIoctl(fd, DRM_IOCTL_GEM_CLOSE, &framebuffer->gem_handles[i]);
    }

    memset(framebuffer, 0, sizeof(*framebuffer));
}
