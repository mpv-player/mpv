/*
 * This file is part of mpv video player.
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

#ifndef MPLAYER_WAYLAND_COMMON_H
#define MPLAYER_WAYLAND_COMMON_H

#include <wayland-client.h>
#include "input/event.h"
#include "vo.h"

struct wayland_opts {
    int disable_vsync;
    int edge_pixels_pointer;
    int edge_pixels_touch;
};

struct vo_wayland_state {
    struct m_config_cache *vo_opts_cache;
    struct mp_log         *log;
    struct mp_vo_opts     *vo_opts;
    struct vo             *vo;
    struct wayland_opts   *opts;
    struct wl_callback    *frame_callback;
    struct wl_compositor  *compositor;
    struct wl_display     *display;
    struct wl_registry    *registry;
    struct wl_shm         *shm;
    struct wl_surface     *surface;

    /* Geometry */
    struct mp_rect geometry;
    struct mp_rect vdparams;
    struct mp_rect window_size;
    struct wl_list output_list;
    struct vo_wayland_output *current_output;
    int gcd;
    int reduced_height;
    int reduced_width;
    int toplevel_width;
    int toplevel_height;

    /* State */
    bool activated;
    bool has_keyboard_input;
    bool focused;
    bool frame_wait;
    bool hidden;
    bool scale_change;
    bool state_change;
    bool toplevel_configured;
    int display_fd;
    int mouse_unscaled_x;
    int mouse_unscaled_y;
    int mouse_x;
    int mouse_y;
    int pending_vo_events;
    int scaling;
    int timeout_count;
    int wakeup_pipe[2];

    /* idle-inhibit */
    struct zwp_idle_inhibit_manager_v1 *idle_inhibit_manager;
    struct zwp_idle_inhibitor_v1 *idle_inhibitor;

    /* xdg-decoration */
    struct zxdg_decoration_manager_v1 *xdg_decoration_manager;
    struct zxdg_toplevel_decoration_v1 *xdg_toplevel_decoration;
    int requested_decoration;

    /* xdg-shell */
    struct xdg_wm_base      *wm_base;
    struct xdg_surface      *xdg_surface;
    struct xdg_toplevel     *xdg_toplevel;

    /* presentation-time */
    struct wp_presentation  *presentation;
    struct wp_presentation_feedback *feedback;
    struct vo_wayland_sync *sync;
    int sync_size;
    int64_t last_ust;
    int64_t last_msc;
    int64_t last_skipped_vsyncs;
    int64_t last_queue_display_time;
    int64_t refresh_interval;
    int64_t vsync_duration;

    /* Input */
    struct wl_keyboard *keyboard;
    struct wl_pointer  *pointer;
    struct wl_seat     *seat;
    struct wl_touch    *touch;
    struct xkb_context *xkb_context;
    struct xkb_keymap  *xkb_keymap;
    struct xkb_state   *xkb_state;
    uint32_t keyboard_code;

    /* DND */
    struct wl_data_device *dnd_ddev;
    struct wl_data_device_manager *dnd_devman;
    struct wl_data_offer *dnd_offer;
    enum mp_dnd_action dnd_action;
    char *dnd_mime_type;
    int dnd_fd;
    int dnd_mime_score;

    /* Cursor */
    struct wl_cursor_theme *cursor_theme;
    struct wl_cursor       *default_cursor;
    struct wl_surface      *cursor_surface;
    bool                    cursor_visible;
    int                     allocated_cursor_scale;
    uint32_t                pointer_id;
};

int vo_wayland_control(struct vo *vo, int *events, int request, void *arg);
int vo_wayland_init(struct vo *vo);
int vo_wayland_reconfig(struct vo *vo);

void vo_wayland_set_opaque_region(struct vo_wayland_state *wl, int alpha);
void vo_wayland_sync_swap(struct vo_wayland_state *wl);
void vo_wayland_uninit(struct vo *vo);
void vo_wayland_wait_events(struct vo *vo, int64_t until_time_us);
void vo_wayland_wait_frame(struct vo_wayland_state *wl);
void vo_wayland_wakeup(struct vo *vo);

#endif /* MPLAYER_WAYLAND_COMMON_H */
