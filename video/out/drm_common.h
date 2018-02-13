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
    int fd;
    drmModeConnector *connector;
    drmModeEncoder *encoder;
    drmModeModeInfo mode;
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
    char *drm_connector_spec;
    int drm_mode_id;
    int drm_overlay_id;
    int drm_format;
};

bool vt_switcher_init(struct vt_switcher *s, struct mp_log *log);
void vt_switcher_destroy(struct vt_switcher *s);
void vt_switcher_poll(struct vt_switcher *s, int timeout_ms);
void vt_switcher_interrupt_poll(struct vt_switcher *s);

void vt_switcher_acquire(struct vt_switcher *s, void (*handler)(void*),
                         void *user_data);
void vt_switcher_release(struct vt_switcher *s, void (*handler)(void*),
                         void *user_data);

struct kms *kms_create(struct mp_log *log, const char *connector_spec,
                       int mode_id, int overlay_id);
void kms_destroy(struct kms *kms);
double kms_get_display_fps(const struct kms *kms);

void kms_show_available_connectors(struct mp_log *log, int card_no);
void kms_show_available_modes(struct mp_log *log,
                              const drmModeConnector *connector);
void kms_show_available_cards_and_connectors(struct mp_log *log);

int drm_validate_connector_opt(struct mp_log *log, const struct m_option *opt,
                               struct bstr name, struct bstr param);

#endif
