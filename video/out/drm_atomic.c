/*
 * Helper code for the DRM atomic API.
 *
 * by Tobias Jakobi <tjakobi@math.uni-bielefeld.de>
 *
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

#include "drm_atomic.h"

#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sys/poll.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include "common/msg.h"


struct kms_property {
    uint32_t object_type;
    const char *name;
    uint32_t id;
};

struct kms_atomic {
    struct mp_log *log;

    // File descriptor of the DRM device.
    int fd;

    // IDs for connector, CRTC and mode blob.
    uint32_t connector_id;
    uint32_t crtc_id;
    uint32_t mode_blob_id;

    // Properties of connector and CRTC.
    struct kms_property *connector_properties;
    struct kms_property *crtc_properties;

    // Atomic requests for the initial and the restore modeset.
    drmModeAtomicReq *modeset_request;
    drmModeAtomicReq *restore_request;

    // Event handling used to control page flipping.
    struct pollfd fds;
    drmEventContext evctx;

    // Available planes.
    struct atomic_plane *planes;
    unsigned num_planes;

    // Registered pages.
    struct atomic_page **pages;
    unsigned num_pages;

    // Currently displayed page.
    struct atomic_page *cur_page;

    // Number of pending pageflips.
    unsigned pageflips_pending;

    // Mode dimensions.
    unsigned width, height;
};

// A template of properties that should be available
// for all DRM drivers.
static const struct kms_property prop_template[] = {

    // Property IDs of the connector object.
    { DRM_MODE_OBJECT_CONNECTOR, "CRTC_ID", 0 },

    // Property IDs of the CRTC object.
    { DRM_MODE_OBJECT_CRTC, "ACTIVE", 0 },
    { DRM_MODE_OBJECT_CRTC, "MODE_ID", 0 },

    // Property IDs of the plane object.
    { DRM_MODE_OBJECT_PLANE, "type", 0 },
    { DRM_MODE_OBJECT_PLANE, "FB_ID", 0 },
    { DRM_MODE_OBJECT_PLANE, "CRTC_ID", 0 },
    { DRM_MODE_OBJECT_PLANE, "CRTC_X", 0 },
    { DRM_MODE_OBJECT_PLANE, "CRTC_Y", 0 },
    { DRM_MODE_OBJECT_PLANE, "CRTC_W", 0 },
    { DRM_MODE_OBJECT_PLANE, "CRTC_H", 0 },
    { DRM_MODE_OBJECT_PLANE, "SRC_X", 0 },
    { DRM_MODE_OBJECT_PLANE, "SRC_Y", 0 },
    { DRM_MODE_OBJECT_PLANE, "SRC_W", 0 },
    { DRM_MODE_OBJECT_PLANE, "SRC_H", 0 },
    { DRM_MODE_OBJECT_PLANE, "zpos", 0 },
};

struct property_assign {
    const char *name;
    uint64_t value;
};


// Get the index of a compatible (decided by name) DRM device.
static int get_device_index(const char *name)
{
    char buf[32];
    drmVersionPtr ver;

    int index = 0;
    int fd;
    bool found = false;

    while (!found) {
        snprintf(buf, sizeof(buf), "/dev/dri/card%d", index);

        fd = open(buf, O_RDWR);
        if (fd < 0)
            break;

        ver = drmGetVersion(fd);

        if (strcmp(name, ver->name) == 0)
            found = true;
        else
            ++index;

        drmFreeVersion(ver);
        close(fd);
    }

    return (found ? index : -1);
}

// The main pageflip handler which is used by drmHandleEvent().
// Decreases the pending pageflip count and updates the current page.
static void page_flip_handler(int fd, unsigned frame, unsigned sec,
                              unsigned usec, void *data)
{
    struct atomic_page *p;
    struct kms_atomic *root;

    p = data;
    root = p->root;

    if (root->cur_page)
        root->cur_page->flags &= ~page_used;

    root->pageflips_pending--;
    root->cur_page = p;
}

// Get the ID of an object's property using the property name.
// This queries the kernel DRM driver.
static bool get_propid_by_name(int fd, uint32_t object_id, uint32_t object_type,
                               const char *name, uint32_t *prop_id)
{
    drmModeObjectProperties *properties;
    unsigned i;
    bool found = false;

    properties = drmModeObjectGetProperties(fd, object_id, object_type);

    if (!properties)
        goto out;

    for (i = 0; i < properties->count_props; ++i) {
        drmModePropertyRes *prop;

        prop = drmModeGetProperty(fd, properties->props[i]);
        if (!prop)
            continue;

        if (!strcmp(prop->name, name)) {
            *prop_id = prop->prop_id;
            found = true;
        }

        drmModeFreeProperty(prop);

        if (found)
            break;
    }

    drmModeFreeObjectProperties(properties);

out:
    return found;
}

// Lookup the ID of an object's property using the property name.
// This uses the cached properties in userspace.
static bool lookup_propid_by_name(const struct kms_property *properties,
                                  const char *name, uint32_t *prop_id)
{
    while (properties->name) {
        if (!strcmp(properties->name, name)) {
            *prop_id = properties->id;
            return true;
        }

        ++properties;
    }

    return false;
}

// Get the value of an object's property using the ID.
// This queries the kernel DRM driver.
static bool get_propval_by_id(int fd, uint32_t object_id, uint32_t object_type,
                              uint32_t id, uint64_t *prop_value)
{
    drmModeObjectProperties *properties;
    unsigned i;
    bool found = false;

    properties = drmModeObjectGetProperties(fd, object_id, object_type);

    if (!properties)
        goto out;

    for (i = 0; i < properties->count_props; ++i) {
        drmModePropertyRes *prop;

        prop = drmModeGetProperty(fd, properties->props[i]);
        if (!prop)
            continue;

        if (prop->prop_id == id) {
            *prop_value = properties->prop_values[i];
            found = true;
        }

        drmModeFreeProperty(prop);

        if (found)
            break;
    }

    drmModeFreeObjectProperties(properties);

out:
    return found;
}

static struct kms_property *alloc_property_array(unsigned count)
{
    struct kms_property *p;

    // Allocate and add sentinel.
    p = talloc_array(NULL, struct kms_property, count + 1);
    p[count] = (struct kms_property) {
        .object_type = 0,
        .name = NULL,
        .id = 0,
    };

    return p;
}

static int num_kms_properties(struct kms_atomic *kms, uint32_t object_type)
{
    const unsigned num_prop = sizeof(prop_template) / sizeof(prop_template[0]);

    unsigned i, count;

    count = 0;
    for (i = 0; i < num_prop; ++i) {
        if (prop_template[i].object_type == object_type)
            ++count;
    }

    return count;
}

static int get_kms_properties(int fd, uint32_t object_type, uint32_t object_id,
                              struct kms_property *properties)
{
    const unsigned num_prop = sizeof(prop_template) / sizeof(prop_template[0]);

    unsigned i, j;

    j = 0;
    for (i = 0; i < num_prop; ++i) {
        if (prop_template[i].object_type != object_type)
            continue;

        const char* prop_name = prop_template[i].name;

        uint32_t prop_id;

        if (!get_propid_by_name(fd, object_id, object_type, prop_name, &prop_id))
            return -1;

        properties[j] = (struct kms_property) {
            .object_type = object_type,
            .name = prop_name,
            .id = prop_id
        };

        ++j;
    }

    return 0;
}

static int common_properties_create(struct kms_atomic *kms)
{
    const unsigned num_connector_prop = num_kms_properties(kms, DRM_MODE_OBJECT_CONNECTOR);
    const unsigned num_crtc_prop = num_kms_properties(kms, DRM_MODE_OBJECT_CRTC);

    kms->connector_properties = alloc_property_array(num_connector_prop);

    if (get_kms_properties(kms->fd, DRM_MODE_OBJECT_CONNECTOR,
        kms->connector_id, kms->connector_properties) < 0) {
        MP_ERR(kms, "Failed to get DRM properties for connector object.\n");
        goto fail;
    }

    kms->crtc_properties = alloc_property_array(num_crtc_prop);

    if (get_kms_properties(kms->fd, DRM_MODE_OBJECT_CRTC,
        kms->crtc_id, kms->crtc_properties) < 0) {
        MP_ERR(kms, "Failed to get DRM properties for CRTC object.\n");
        goto fail;
    }

    return 0;

fail:
    talloc_free(kms->connector_properties);
    talloc_free(kms->crtc_properties);

    kms->connector_properties = NULL;
    kms->crtc_properties = NULL;

    return -1;
}

static void common_properties_destroy(struct kms_atomic *kms)
{
    talloc_free(kms->connector_properties);
    talloc_free(kms->crtc_properties);

    kms->connector_properties = NULL;
    kms->crtc_properties = NULL;
}

static int planes_create(struct kms_atomic *kms, unsigned crtc_idx,
                         drmModePlaneRes *res)
{
    unsigned i, j;

    const unsigned num_plane_prop = num_kms_properties(kms, DRM_MODE_OBJECT_PLANE);

    drmModePlane **planes = talloc_array(NULL, drmModePlane*, res->count_planes);

    // Get all available planes.
    for (i = 0; i < res->count_planes; ++i)
        planes[i] = drmModeGetPlane(kms->fd, res->planes[i]);

    // Now filter the ones which we can use with the selected CRTC.
    for (i = 0; i < res->count_planes; ++i) {
        if (!planes[i])
            continue;

        // Make sure that the plane can be used with the selected CRTC.
        if (!(planes[i]->possible_crtcs & (1 << crtc_idx)))
            continue;

        kms->num_planes++;
    }

    kms->planes = talloc_zero_array(NULL, struct atomic_plane, kms->num_planes);

    // Copy relevant data.
    j = 0;
    for (i = 0; i < res->count_planes; ++i) {
        if (!planes[i])
            continue;

        if (!(planes[i]->possible_crtcs & (1 << crtc_idx)))
            continue;

        kms->planes[j].id = planes[i]->plane_id;

        kms->planes[j].num_formats = planes[i]->count_formats;
        kms->planes[j].formats = talloc_array(NULL, uint32_t, planes[i]->count_formats);

        memcpy(kms->planes[j].formats, planes[i]->formats,
            sizeof(uint32_t) * planes[i]->count_formats);

        ++j;
    }

    for (i = 0; i < res->count_planes; ++i)
        drmModeFreePlane(planes[i]);

    talloc_free(planes);

    // Get properties of each plane object and verify the type.
    for (i = 0; i < kms->num_planes; ++i) {
        uint32_t prop_id;
        uint64_t type;

        kms->planes[i].properties = alloc_property_array(num_plane_prop);

        if (get_kms_properties(kms->fd, DRM_MODE_OBJECT_PLANE,
            kms->planes[i].id, kms->planes[i].properties) < 0) {
            MP_ERR(kms, "Failed to get DRM properties for plane object.\n");
            goto fail;
        }

        if (!lookup_propid_by_name(kms->planes[i].properties, "type", &prop_id) ||
            !get_propval_by_id(kms->fd, kms->planes[i].id, DRM_MODE_OBJECT_PLANE,
                               prop_id, &type)) {
            MP_ERR(kms, "Failed to lookup type of plane object.\n");
            goto fail;
        }

        switch (type) {
        case DRM_PLANE_TYPE_OVERLAY:
        case DRM_PLANE_TYPE_PRIMARY:
        case DRM_PLANE_TYPE_CURSOR:
            break;

        default:
            MP_ERR(kms, "Unknown type for plane object.\n");
            goto fail;
        }

        kms->planes[i].type = type;
    }

    return 0;

fail:
    for (i = 0; i < kms->num_planes; ++i) {
        talloc_free(kms->planes[i].properties);
        talloc_free(kms->planes[i].formats);
    }

    talloc_free(kms->planes);
    kms->planes = NULL;
    kms->num_planes = 0;

    return -1;
}

static void planes_destroy(struct kms_atomic *kms)
{
    unsigned i;

    assert(kms);

    for (i = 0; i < kms->num_planes; ++i) {
        talloc_free(kms->planes[i].properties);
        talloc_free(kms->planes[i].formats);
    }

    talloc_free(kms->planes);

    kms->planes = NULL;
    kms->num_planes = 0;
}

static int save_properties(drmModeAtomicReq *req, const struct kms_property *properties,
                           int fd, uint32_t object_type, uint32_t object_id)
{
    while (properties->name) {
        uint64_t prop_value;

        if (!get_propval_by_id(fd, object_id, object_type, properties->id, &prop_value))
            return -1;

        if (drmModeAtomicAddProperty(req, object_id, properties->id, prop_value) < 0)
            return -2;

        ++properties;
    }

    return 0;
}

static int create_restore_request(struct kms_atomic *kms) {
    unsigned i;

    assert(kms);
    assert(!kms->restore_request);

    kms->restore_request = drmModeAtomicAlloc();

    if (save_properties(kms->restore_request, kms->connector_properties, kms->fd,
                        DRM_MODE_OBJECT_CONNECTOR, kms->connector_id) < 0)
        goto fail;

    if (save_properties(kms->restore_request, kms->crtc_properties, kms->fd,
                        DRM_MODE_OBJECT_CRTC, kms->crtc_id) < 0)
        goto fail;

    for (i = 0; i < kms->num_planes; ++i) {
        if (save_properties(kms->restore_request, kms->planes[i].properties, kms->fd,
                            DRM_MODE_OBJECT_PLANE, kms->planes[i].id) < 0)
            goto fail;
    }

    return 0;

fail:
    drmModeAtomicFree(kms->restore_request);
    kms->restore_request = NULL;

    return -1;
}

static int create_modeset_request(struct kms_atomic *kms, struct atomic_page *page)
{
    uint32_t prop_id;
    unsigned i;

    assert(!kms->modeset_request);

    kms->modeset_request = drmModeAtomicAlloc();

    if (!lookup_propid_by_name(kms->connector_properties, "CRTC_ID", &prop_id) ||
        drmModeAtomicAddProperty(kms->modeset_request, kms->connector_id,
                                 prop_id, kms->crtc_id) < 0)
        goto fail;

    if (!lookup_propid_by_name(kms->crtc_properties, "ACTIVE", &prop_id) ||
        drmModeAtomicAddProperty(kms->modeset_request, kms->crtc_id,
                                 prop_id, 1) < 0)
        goto fail;

    if (!lookup_propid_by_name(kms->crtc_properties, "MODE_ID", &prop_id) ||
        drmModeAtomicAddProperty(kms->modeset_request, kms->crtc_id,
                                 prop_id, kms->mode_blob_id) < 0)
        goto fail;

    for (i = 0; i < atomic_plane_num; ++i) {
        unsigned j;
        const struct atomic_plane *p;

        const struct property_assign a[] = {
            { "CRTC_X", page->cfg[i].x },
            { "CRTC_Y", page->cfg[i].y },
            { "CRTC_W", page->cfg[i].w },
            { "CRTC_H", page->cfg[i].h },
            { "SRC_X", 0 },
            { "SRC_Y", 0 },
            { "SRC_W", page->desc[i].width << 16 },
            { "SRC_H", page->desc[i].height << 16 }
        };

        const unsigned num_a = sizeof(a) / sizeof(a[0]);

        p = page->cfg[i].plane;

        for (j = 0; j < num_a; ++j) {
            if (!lookup_propid_by_name(p->properties, a[j].name, &prop_id) ||
                drmModeAtomicAddProperty(kms->modeset_request, p->id,
                                         prop_id, a[j].value) < 0)
                goto fail;
        }
    }

    return 0;

fail:
    drmModeAtomicFree(kms->modeset_request);
    kms->modeset_request = NULL;

    return -1;
}

static int update_page_request(struct atomic_page *page)
{
    unsigned i;

    if (page->atomic_request)
        drmModeAtomicFree(page->atomic_request);

    page->atomic_request = drmModeAtomicAlloc();
    if (!page->atomic_request)
        return -1;

    for (i = 0; i < atomic_plane_num; ++i) {
        uint32_t buf_id, prop_id, crtc_id;

        if (i == atomic_plane_osd && !(page->flags & page_osd_active)) {
            buf_id = 0;
            crtc_id = 0;
        } else {
            buf_id = page->buf_id[i];
            crtc_id = page->root->crtc_id;
        }

        const struct atomic_plane *plane = page->cfg[i].plane;

        if (!lookup_propid_by_name(plane->properties, "FB_ID", &prop_id) ||
            drmModeAtomicAddProperty(page->atomic_request, plane->id,
                                     prop_id, buf_id) < 0)
            goto fail;

        if (!lookup_propid_by_name(plane->properties, "CRTC_ID", &prop_id) ||
            drmModeAtomicAddProperty(page->atomic_request, plane->id,
                                     prop_id, crtc_id) < 0)
            goto fail;
    }

    return 0;

fail:
    drmModeAtomicFree(page->atomic_request);
    page->atomic_request = NULL;

    return -2;
}

static int initial_modeset(struct kms_atomic *kms, struct atomic_page *page)
{
    int ret;
    drmModeAtomicReq *request;

    if (page->flags & page_dirty) {
        ret = update_page_request(page);
        if (ret < 0) {
            MP_ERR(kms, "Failed to update atomic request for dirty page.\n");
            return -1;
        }

        page->flags &= ~page_dirty;
    }

    request = drmModeAtomicDuplicate(kms->modeset_request);
    if (!request) {
        MP_ERR(kms, "Failed to duplicate the atomic modeset request.\n");
        return -2;
    }

    ret = drmModeAtomicMerge(request, page->atomic_request);
    if (ret < 0) {
        MP_ERR(kms, "Failed to merge atomic page request (%d).\n", ret);
        ret = -3;
        goto out;
    }

    ret = drmModeAtomicCommit(kms->fd, request, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
    if (ret < 0) {
        MP_ERR(kms, "Failed to commit atomic request (%d).\n", ret);
        ret = -4;
    }

out:
    drmModeAtomicFree(request);
    return ret;
}


/* Start of the public atomic KMS. */

/**
 * kms_atomic_create - create a new atomic KMS context.
 *
 * @log: a pointer to a log object.
 * @name: name of the DRM device to use.
 * @mode_id: ID of the mode to use.
 */
struct kms_atomic *kms_atomic_create(struct mp_log *log, const char *name,
                                     int mode_id)
{
    char buf[32];
    int devidx;

    struct kms_atomic *kms = NULL;
    unsigned i, j;
    int ret;

    drmModeRes *resources = NULL;
    drmModePlaneRes *plane_resources = NULL;
    drmModeConnector *connector = NULL;

    drmModeModeInfo *mode;

    kms = talloc_zero(NULL, struct kms_atomic);
    *kms = (struct kms_atomic) {
        .log = mp_log_new(kms, log, "kms-atomic"),
        .fd = -1
    };

    devidx = get_device_index(name);
    if (devidx < 0) {
        MP_ERR(kms, "create(): No compatible DRM device found.\n");
        goto fail;
    }

    snprintf(buf, sizeof(buf), "/dev/dri/card%d", devidx);

    kms->fd = open(buf, O_RDWR, 0);
    if (kms->fd < 0) {
        MP_ERR(kms, "create(): Failed to open DRM device.\n");
        goto fail_open;
    }

    // Request atomic DRM support. This also enables universal planes.
    ret = drmSetClientCap(kms->fd, DRM_CLIENT_CAP_ATOMIC, 1);
    if (ret < 0) {
        MP_ERR(kms, "create(): Failed to enable atomic support (%d).\n", ret);
        goto fail_atomic;
    }

    resources = drmModeGetResources(kms->fd);
    if (!resources) {
        MP_ERR(kms, "create(): Failed to get DRM resources.\n");
        goto fail_resources;
    }

    plane_resources = drmModeGetPlaneResources(kms->fd);
    if (!plane_resources) {
        MP_ERR(kms, "create(): Failed to get DRM plane resources.\n");
        goto fail_plane_resources;
    }

    for (i = 0; i < resources->count_connectors; ++i) {
        connector = drmModeGetConnector(kms->fd, resources->connectors[i]);
        if (connector == NULL)
            continue;

        if (connector->connection == DRM_MODE_CONNECTED &&
            connector->count_modes > 0)
            break;

        drmModeFreeConnector(connector);
        connector = NULL;
    }

    if (i == resources->count_connectors) {
        MP_ERR(kms, "create(): No currently active connector found.\n");
        goto fail_connector;
    }

    kms->connector_id = connector->connector_id;

    for (i = 0; i < connector->count_encoders; i++) {
        drmModeEncoder *encoder;

        encoder = drmModeGetEncoder(kms->fd, connector->encoders[i]);

        if (!encoder)
            continue;

        // Find a CRTC that is compatible with the encoder.
        for (j = 0; j < resources->count_crtcs; ++j) {
            if (encoder->possible_crtcs & (1 << j))
                break;
        }

        drmModeFreeEncoder(encoder);

        // Stop when a suitable CRTC was found.
        if (j != resources->count_crtcs)
            break;
    }

    if (i == connector->count_encoders) {
        MP_ERR(kms, "create(): No compatible encoder found.\n");
        goto fail_encoder;
    }

    kms->crtc_id = resources->crtcs[j];

    if (mode_id < 0 || mode_id >= connector->count_modes) {
        MP_ERR(kms, "create(): Invalid mode ID %d.\n", mode_id);
        goto fail_mode_id;
    }

    mode = &connector->modes[mode_id];

    if (mode->hdisplay == 0 || mode->vdisplay == 0) {
        MP_ERR(kms, "create(): Connector has bogus resolution.\n");
        goto fail_mode_res;
    }

    if (drmModeCreatePropertyBlob(kms->fd, mode, sizeof(drmModeModeInfo),
                                  &kms->mode_blob_id)) {
        MP_ERR(kms, "create(): Failed to blobify mode info.\n");
        goto fail_mode_blob;
    }

    kms->width = mode->hdisplay;
    kms->height = mode->vdisplay;

    ret = common_properties_create(kms);
    if (ret < 0) {
        MP_ERR(kms, "create(): Failed to create common DRM properties (%d).\n", ret);
        goto fail_props;
    }

    ret = planes_create(kms, j, plane_resources);
    if (ret < 0) {
        MP_ERR(kms, "create(): Failed to create DRM planes (%d).\n", ret);
        goto fail_planes;
    }

    MP_INFO(kms, "create(): Using DRM device \"%s\" with connector ID %u.\n",
            buf, kms->connector_id);

    j = 0;
    for (i = 0; i < kms->num_planes; ++i) {
        if (kms->planes[i].type == DRM_PLANE_TYPE_PRIMARY) {
            j = kms->planes[i].id;
            break;
        }
    }

    MP_INFO(kms, "create(): Primary plane ID = %u.\n", j);
    MP_INFO(kms, "create(): Total number of planes = %u.\n", kms->num_planes);

    if (kms)
        goto out;

fail_planes:
    common_properties_destroy(kms);

fail_props:
    drmModeDestroyPropertyBlob(kms->fd, kms->mode_blob_id);

fail_mode_blob:
fail_mode_res:
fail_mode_id:
fail_encoder:
fail_connector:
fail_plane_resources:
fail_resources:
fail_atomic:
    close(kms->fd);

fail_open:
fail:
    talloc_free(kms);
    kms = NULL;

out:
    drmModeFreeConnector(connector);
    drmModeFreePlaneResources(plane_resources);
    drmModeFreeResources(resources);

    return kms;
}

/**
 * kms_atomic_destroy - destroy an atomic KMS context.
 *
 * @kms: a pointer to an atomic KMS context.
 *
 * Counterpart to kms_atomic_create().
 */
void kms_atomic_destroy(struct kms_atomic *kms)
{
    planes_destroy(kms);
    common_properties_destroy(kms);

    drmModeDestroyPropertyBlob(kms->fd, kms->mode_blob_id);

    close(kms->fd);
    kms->fd = -1;

    talloc_free(kms);
}

/**
 * kms_atomic_get_fd - return the underlying file descriptor of the context.
 *
 * @kms: a pointer to an atomic KMS context.
 */
int kms_atomic_get_fd(struct kms_atomic *kms)
{
    assert(kms);

    return kms->fd;
}

/**
 * kms_atomic_mode_dim - return the dimensions of the context's mode.
 *
 * @kms: a pointer to an atomic KMS context.
 * @width: a pointer to an unsigned integer.
 * @height: same as 'width'.
 */
void kms_atomic_mode_dim(struct kms_atomic *kms,
                         unsigned *width, unsigned *height)
{
    assert(kms);

    *width = kms->width;
    *height = kms->height;
}

/**
 * kms_atomic_num_planes - return the number of available planes.
 *
 * @kms: a pointer to an atomic KMS context.
 */
unsigned kms_atomic_num_planes(struct kms_atomic *kms)
{
    assert(kms);

    return kms->num_planes;
}

/**
 * kms_atomic_get_plane - get a constant pointer to the plane with a given index.
 *
 * @kms: a pointer to an atomic KMS context.
 * @index: index of the plane to retrieve.
 */
const struct atomic_plane *kms_atomic_get_plane(struct kms_atomic *kms, unsigned index)
{
    assert(kms);

    if (index >= kms->num_planes)
        return NULL;
    else
        return &kms->planes[index];
}

/**
 * kms_atomic_register_page - register a page with the atomic KMS context.
 *
 * @kms: a pointer to an atomic KMS context.
 * @page: a pointer to an atomic page.
 */
int kms_atomic_register_page(struct kms_atomic *kms, struct atomic_page *page)
{
    unsigned i;
    int ret;

    for (i = 0; i < atomic_plane_num; ++i) {
        const struct buffer_desc *d;

        d = &page->desc[i];

        ret = drmModeAddFB2(kms->fd, d->width, d->height, d->format, d->handles,
                            d->pitches, d->offsets, &page->buf_id[i], d->flags);
        if (ret < 0) {
            MP_ERR(kms, "Failed to add page buffer as framebuffer (%d).\n", ret);
            return -1;
        }
    }

    // This triggers an update of the atomic request later.
    page->flags = page_dirty;

    page->root = kms;

    kms->pages = talloc_realloc(NULL, kms->pages, struct atomic_page*, kms->num_pages + 1);

    kms->pages[kms->num_pages] = page;
    ++kms->num_pages;

    return 0;
}

/**
 * kms_atomic_unregister_pages - unregister all currently registered pages.
 *
 * @kms: a pointer to an atomic KMS context.
 */
void kms_atomic_unregister_pages(struct kms_atomic *kms)
{
    unsigned i;

    for (i = 0; i < kms->num_pages; ++i) {
        unsigned j;
        struct atomic_page *pg;

        pg = kms->pages[i];

        drmModeAtomicFree(pg->atomic_request);

        for (j = 0; j < atomic_plane_num; ++j)
            drmModeRmFB(kms->fd, pg->buf_id[j]);
    }
}

/**
 * kms_atomic_enable - enable the context and display a given page.
 *
 * @kms: a pointer to an atomic KMS context.
 * @page: a pointer to an atomic page.
 */
int kms_atomic_enable(struct kms_atomic *kms, struct atomic_page *page)
{
    int ret;

    assert(kms);
    assert(page);

    if (!kms->num_pages) {
        MP_ERR(kms, "enable(): Context has no registered pages.\n");
        return -1;
    }

    MP_INFO(kms, "enable(): Enabling context with %u pages.\n", kms->num_pages);

    // Setup the flip handler.
    kms->fds.fd = kms->fd;
    kms->fds.events = POLLIN;
    kms->evctx.version = DRM_EVENT_CONTEXT_VERSION;
    kms->evctx.page_flip_handler = page_flip_handler;

    ret = create_restore_request(kms);
    if (ret < 0) {
        MP_ERR(kms, "enable(): Failed to create atomic restore request (%d).\n", ret);
        goto fail_restore;
    }

    ret = create_modeset_request(kms, page);
    if (ret < 0) {
        MP_ERR(kms, "enable(): Failed to create atomic modeset request (%d).\n", ret);
        goto fail_modeset;
    }

    MP_INFO(kms, "enable(): Setting mode with resolution %ux%u.\n",
            kms->width, kms->height);

    ret = initial_modeset(kms, page);
    if (ret < 0) {
        MP_ERR(kms, "enable(): initial atomic modeset failed (%d).\n", ret);
        goto fail_initial;
    }

    return 0;

fail_initial:
    drmModeAtomicFree(kms->modeset_request);
    kms->modeset_request = NULL;

fail_modeset:
    drmModeAtomicFree(kms->restore_request);
    kms->restore_request = NULL;

fail_restore:
    return -1;
}

/**
 * kms_atomic_disable - disable the context and restore the display.
 *
 * @kms: a pointer to an atomic KMS context.
 *
 * Counterpart to kms_atomic_enable().
 */
void kms_atomic_disable(struct kms_atomic *kms)
{
    int ret;

    assert(kms);
    assert(kms->restore_request);

    // Restore the display state.
    ret = drmModeAtomicCommit(kms->fd, kms->restore_request,
                              DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);

    if (ret < 0)
        MP_WARN(kms, "disable(): Failed to restore the display (%d).\n", ret);

    drmModeAtomicFree(kms->modeset_request);
    drmModeAtomicFree(kms->restore_request);

    kms->modeset_request = NULL;
    kms->restore_request = NULL;

    kms->fds.fd = -1;
    kms->evctx.page_flip_handler = NULL;
}

/**
 * kms_atomic_pageflip_pending - check if the context has a pending pageflip.
 *
 * @kms: a pointer to an atomic KMS context.
 */
bool kms_atomic_pageflip_pending(struct kms_atomic *kms)
{
    assert(kms);

    return (kms->pageflips_pending != 0);
}

/**
 * kms_atomic_wait_for_flip - wait for a pageflip.
 *
 * @kms: a pointer to an atomic KMS context.
 */
void kms_atomic_wait_for_flip(struct kms_atomic *kms)
{
    const int timeout = -1;

    kms->fds.revents = 0;

    if (poll(&kms->fds, 1, timeout) < 0)
        return;

    if (kms->fds.revents & (POLLHUP | POLLERR))
        return;

    if (kms->fds.revents & POLLIN)
        drmHandleEvent(kms->fds.fd, &kms->evctx);
}

/**
 * kms_atomic_issue_flip - issue a pageflip to a given page.
 *
 * @kms: a pointer to an atomic KMS context.
 * @page: a pointer to an atomic page.
 */
int kms_atomic_issue_flip(struct kms_atomic *kms, struct atomic_page *page)
{
    int ret;

    assert(kms);
    assert(page);

    if (page->flags & page_dirty) {
        ret = update_page_request(page);
        if (ret < 0) {
            MP_ERR(kms, "issue_flip(): Failed to update atomic request for dirty page.\n");
            return ret;
        }

        page->flags &= ~page_dirty;
    }

    // We don't queue multiple page flips.
    if (kms->pageflips_pending > 0)
        kms_atomic_wait_for_flip(kms);

    // Issue a pageflip at the next vblank interval.
    ret = drmModeAtomicCommit(kms->fd, page->atomic_request,
                              DRM_MODE_PAGE_FLIP_EVENT, page);
    if (ret < 0)
        return ret;

    kms->pageflips_pending++;

    // On startup no frame is displayed.
    // We therefore wait for the initial flip to finish.
    if (!kms->cur_page)
        kms_atomic_wait_for_flip(kms);

    return 0;
}
