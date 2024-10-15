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

#include "video/mp_image.h"
#include "vo.h"

enum {
    DRM_OPTS_FORMAT_XRGB8888,
    DRM_OPTS_FORMAT_XRGB2101010,
    DRM_OPTS_FORMAT_XBGR8888,
    DRM_OPTS_FORMAT_XBGR2101010,
    DRM_OPTS_FORMAT_YUYV,
};

// Enum values based on include/linux/hdmi.h
// and https://docs.kernel.org/gpu/drm-uapi.html
// for interoperability with drm API.
enum drm_metadata_type {
    HDMI_STATIC_METADATA_TYPE1 = 0,
};

// Enum values based on https://docs.kernel.org/gpu/drm-kms.html
// for interoperability with drm API.
enum drm_colorspace {
    DRM_MODE_COLORIMETRY_DEFAULT = 0,
    DRM_MODE_COLORIMETRY_NO_DATA = 0,
    DRM_MODE_COLORIMETRY_SMPTE_170M_YCC = 1,
    DRM_MODE_COLORIMETRY_BT709_YCC = 2,
    DRM_MODE_COLORIMETRY_XVYCC_601 = 3,
    DRM_MODE_COLORIMETRY_XVYCC_709 = 4,
    DRM_MODE_COLORIMETRY_SYCC_601 = 5,
    DRM_MODE_COLORIMETRY_OPYCC_601 = 6,
    DRM_MODE_COLORIMETRY_OPRGB = 7,
    DRM_MODE_COLORIMETRY_BT2020_CYCC = 8,
    DRM_MODE_COLORIMETRY_BT2020_RGB = 9,
    DRM_MODE_COLORIMETRY_BT2020_YCC = 10,
    DRM_MODE_COLORIMETRY_DCI_P3_RGB_D65 = 11,
    DRM_MODE_COLORIMETRY_DCI_P3_RGB_THEATER = 12,
    DRM_MODE_COLORIMETRY_RGB_WIDE_FIXED = 13,
    DRM_MODE_COLORIMETRY_RGB_WIDE_FLOAT = 14,
    DRM_MODE_COLORIMETRY_BT601_YCC = 15,
    DRM_MODE_COLORIMETRY_COUNT,
};

// Enum values based on include/linux/hdmi.h
// and https://docs.kernel.org/gpu/drm-uapi.html
// for interoperability with drm API.
enum drm_eotf {
    HDMI_EOTF_TRADITIONAL_GAMMA_SDR,
    HDMI_EOTF_TRADITIONAL_GAMMA_HDR,
    HDMI_EOTF_SMPTE_ST2084,
    HDMI_EOTF_BT_2100_HLG,
};

struct framebuffer {
    int fd;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t size;
    uint32_t handle;
    uint8_t *map;
    uint32_t id;
};

struct drm_hdr {
    struct hdr_output_metadata metadata;
    uint32_t blob_id;
};

struct drm_mode {
    drmModeModeInfo mode;
    uint32_t blob_id;
};

struct drm_opts {
    char *device_path;
    char *connector_spec;
    char *mode_spec;
    int drm_atomic;
    int draw_plane;
    int drmprime_video_plane;
    int drm_format;
    struct m_geometry draw_surface_size;
    int vrr_enabled;
};

struct vt_switcher {
    int tty_fd;
    struct mp_log *log;
    void (*handlers[2])(void*);
    void *handler_data[2];
};

struct vo_drm_state {
    drmModeConnector *connector;
    drmModeEncoder *encoder;
    drmEventContext ev;

    struct drm_atomic_context *atomic_context;
    struct drm_hdr hdr;
    struct drm_mode mode;
    struct drm_opts *opts;
    struct framebuffer *fb;
    struct mp_image_params target_params;
    struct mp_log *log;
    struct mp_present *present;
    struct vo *vo;
    struct vt_switcher vt_switcher;

    bool active;
    bool paused;
    bool still;
    bool vt_switcher_active;
    bool waiting_for_flip;

    char *card_path;
    int card_no;
    int fd;

    uint32_t crtc_id;
    uint32_t height;
    uint32_t width;
};

bool vo_drm_init(struct vo *vo);
int vo_drm_control(struct vo *vo, int *events, int request, void *arg);

double vo_drm_get_display_fps(struct vo_drm_state *drm);
bool vo_drm_set_hdr_metadata(struct vo *vo, bool force_sdr);
void vo_drm_set_monitor_par(struct vo *vo);
void vo_drm_uninit(struct vo *vo);
void vo_drm_wait_events(struct vo *vo, int64_t until_time_ns);
void vo_drm_wait_on_flip(struct vo_drm_state *drm);
void vo_drm_wakeup(struct vo *vo);

bool vo_drm_acquire_crtc(struct vo_drm_state *drm);
void vo_drm_release_crtc(struct vo_drm_state *drm);

#endif
