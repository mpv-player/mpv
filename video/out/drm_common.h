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

#ifndef MP_VT_SWITCHER_H
#define MP_VT_SWITCHER_H

#include <stdbool.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "options/m_option.h"
#include "drm_atomic.h"

#define DRM_OPTS_FORMAT_XRGB8888    0
#define DRM_OPTS_FORMAT_XRGB2101010 1

struct kms {
    struct mp_log *log;
    char *primary_node_path;
    int fd;
    drmModeConnector *connector;
    drmModeEncoder *encoder;
    struct drm_mode mode;
    uint32_t crtc_id;
    int card_no;
    struct drm_atomic_context *atomic_context;
};

struct vt_switcher {
    int tty_fd;
    struct mp_log *log;
    void (*handlers[2])(void*);
    void *handler_data[2];
};

struct drm_opts {
    char *drm_device_path;
    char *drm_connector_spec;
    char *drm_mode_spec;
    int drm_atomic;
    int drm_draw_plane;
    int drm_drmprime_video_plane;
    int drm_format;
    struct m_geometry drm_draw_surface_size;
};

struct drm_vsync_tuple {
    uint64_t ust;
    unsigned int msc;
    unsigned int sbc;
};

struct drm_pflip_cb_closure {
    struct drm_vsync_tuple *frame_vsync; // vsync tuple when the frame that just flipped was queued
    struct drm_vsync_tuple *vsync; // vsync tuple of the latest page flip. drm_pflip_cb updates this
    struct vo_vsync_info *vsync_info; // where the drm_pflip_cb routine writes its output
    bool *waiting_for_flip; // drm_pflip_cb writes false here before returning
    struct mp_log *log; // Needed to print error messages that shame bad drivers
};

bool vt_switcher_init(struct vt_switcher *s, struct mp_log *log);
void vt_switcher_destroy(struct vt_switcher *s);
void vt_switcher_poll(struct vt_switcher *s, int timeout_ms);
void vt_switcher_interrupt_poll(struct vt_switcher *s);

void vt_switcher_acquire(struct vt_switcher *s, void (*handler)(void*),
                         void *user_data);
void vt_switcher_release(struct vt_switcher *s, void (*handler)(void*),
                         void *user_data);

struct kms *kms_create(struct mp_log *log,
                       const char *drm_device_path,
                       const char *connector_spec,
                       const char *mode_spec,
                       int draw_plane, int drmprime_video_plane,
                       bool use_atomic);
void kms_destroy(struct kms *kms);
double kms_get_display_fps(const struct kms *kms);

// DRM Page Flip callback
void drm_pflip_cb(int fd, unsigned int msc, unsigned int sec,
                  unsigned int usec, void *data);

#endif
