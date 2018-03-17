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

#ifndef MP_DRMATOMIC_H
#define MP_DRMATOMIC_H

#include <stdlib.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "common/msg.h"

struct drm_object {
    int fd;
    uint32_t id;
    uint32_t type;
    drmModeObjectProperties *props;
    drmModePropertyRes **props_info;
};

struct drm_atomic_context {
    int fd;

    struct drm_object *crtc;
    struct drm_object *connector;
    struct drm_object *primary_plane;
    struct drm_object *overlay_plane;

    drmModeAtomicReq *request;
};


int drm_object_create_properties(struct mp_log *log, int fd, struct drm_object *object);
void drm_object_free_properties(struct drm_object *object);
int drm_object_get_property(struct drm_object *object, char *name, uint64_t *value);
int drm_object_set_property(drmModeAtomicReq *request, struct drm_object *object, char *name, uint64_t value);
drmModePropertyBlobPtr drm_object_get_property_blob(struct drm_object *object, char *name);
struct drm_object * drm_object_create(struct mp_log *log, int fd, uint32_t object_id, uint32_t type);
void drm_object_free(struct drm_object *object);
void drm_object_print_info(struct mp_log *log, struct drm_object *object);
struct drm_atomic_context *drm_atomic_create_context(struct mp_log *log, int fd, int crtc_id, int connector_id, int overlay_id);
void drm_atomic_destroy_context(struct drm_atomic_context *ctx);

#endif // MP_DRMATOMIC_H
