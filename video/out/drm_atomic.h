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

#ifndef MP_DRM_ATOMIC_H
#define MP_DRM_ATOMIC_H

#include "options/m_option.h"

// Forward-declarations
struct _drmModeAtomicReq;
typedef struct _drmModeAtomicReq drmModeAtomicReq;
struct kms_atomic;
struct kms_property;


enum atomic_plane_type {
    atomic_plane_video,
    atomic_plane_osd,
    atomic_plane_num
};

enum atomic_page_flags {
    // Page is currently in use.
    page_used = (1 << 0),

    // Page has to be cleared before use.
    page_clear = (1 << 1),

    // Page changed, atomic request has to updated.
    page_dirty = (1 << 2),

    // The OSD plane is active.
    page_osd_active = (1 << 3),
};


struct atomic_plane {
    uint32_t id;
    unsigned type;

    uint32_t *formats;
    unsigned num_formats;

    struct kms_property *properties;
};

struct buffer_desc {
    unsigned width, height;

    uint32_t format;

    uint32_t handles[4];
    uint32_t pitches[4];
    uint32_t offsets[4];

    unsigned flags;
};

struct plane_config {
    // The atomic plane which the configuration
    // applies to.
    const struct atomic_plane *plane;

    unsigned x, y;
    unsigned w, h;
};

struct atomic_page {
    // These have to be filled before passing the page
    // to kms_atomic_register_page().
    struct buffer_desc desc[atomic_plane_num];
    struct plane_config cfg[atomic_plane_num];

    // These are initialized by kms_atomic_register_page().
    uint32_t buf_id[atomic_plane_num];
    drmModeAtomicReq *atomic_request;
    struct kms_atomic *root;
    unsigned flags;
};


// Create and destroy an atomic KMS context.
struct kms_atomic *kms_atomic_create(struct mp_log *log,
    const char *name, int mode_id);
void kms_atomic_destroy(struct kms_atomic *kms);

// Get the underlying DRM fd of the context.
int kms_atomic_get_fd(struct kms_atomic *kms);

// Get the dimensions of the mode the context has selected.
void kms_atomic_mode_dim(struct kms_atomic *kms,
    unsigned *width, unsigned *height);

// Query available planes of the context.
unsigned kms_atomic_num_planes(struct kms_atomic *kms);
const struct atomic_plane *kms_atomic_get_plane(struct kms_atomic *kms,
    unsigned index);

// Register and unregister atomic pages with the context.
int kms_atomic_register_page(struct kms_atomic *kms,
    struct atomic_page *page);
void kms_atomic_unregister_pages(struct kms_atomic *kms);

// Enable and disable the context.
int kms_atomic_enable(struct kms_atomic *kms,
    struct atomic_page *page);
void kms_atomic_disable(struct kms_atomic *kms);

// Check if the context has a pageflip pending.
bool kms_atomic_pageflip_pending(struct kms_atomic *kms);

// Control pageflipping for the context.
void kms_atomic_wait_for_flip(struct kms_atomic *kms);
int kms_atomic_issue_flip(struct kms_atomic *kms,
    struct atomic_page *page);

#endif // MP_DRM_ATOMIC_H
