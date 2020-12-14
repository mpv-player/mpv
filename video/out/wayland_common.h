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
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>

#include "vo.h"
#include "input/event.h"

struct wayland_opts {
    int disable_vsync;
    int edge_pixels_pointer;
    int edge_pixels_touch;
};

struct vo_wayland_sync {
    int64_t ust;
    int64_t msc;
    int64_t sbc;
    bool filled;
};

struct vo_wayland_output {
    struct vo_wayland_state *wl;
    uint32_t id;
    struct wl_output *output;
    struct mp_rect geometry;
    int phys_width;
    int phys_height;
    int scale;
    uint32_t flags;
    double refresh_rate;
    char *make;
    char *model;
    bool has_surface;
    struct wl_list link;
};

struct vo_wayland_state {
    struct mp_log        *log;
    struct vo            *vo;
    struct wl_display    *display;
    struct wl_shm        *shm;
    struct wl_compositor *compositor;
    struct wl_registry   *registry;
    struct wayland_opts  *opts;

    struct m_config_cache *vo_opts_cache;
    struct mp_vo_opts *vo_opts;

    /* State */
    struct mp_rect geometry;
    struct mp_rect window_size;
    struct mp_rect vdparams;
    int gcd;
    int reduced_width;
    int reduced_height;
    bool frame_wait;
    bool state_change;
    bool toplevel_configured;
    bool activated;
    bool has_keyboard_input;
    bool focused;
    bool hidden;
    int timeout_count;
    int wakeup_pipe[2];
    int pending_vo_events;
    int mouse_x;
    int mouse_y;
    int mouse_unscaled_x;
    int mouse_unscaled_y;
    int scaling;
    int touch_entries;
    int toplevel_width;
    int toplevel_height;
    uint32_t pointer_id;
    int display_fd;
    struct wl_callback       *frame_callback;
    struct wl_list            output_list;
    struct vo_wayland_output *current_output;

    /* Shell */
    struct wl_surface       *surface;
    struct xdg_wm_base      *wm_base;
    struct xdg_toplevel     *xdg_toplevel;
    struct xdg_surface      *xdg_surface;
    struct wp_presentation  *presentation;
    struct wp_presentation_feedback *feedback;
    struct zxdg_decoration_manager_v1 *xdg_decoration_manager;
    struct zxdg_toplevel_decoration_v1 *xdg_toplevel_decoration;
    struct zwp_idle_inhibit_manager_v1 *idle_inhibit_manager;
    struct zwp_idle_inhibitor_v1 *idle_inhibitor;

    /* Presentation Feedback */
    struct vo_wayland_sync *sync;
    int sync_size;
    int64_t last_ust;
    int64_t last_msc;
    int64_t last_skipped_vsyncs;
    int64_t last_queue_display_time;
    int64_t vsync_duration;

    /* Input */
    uint32_t keyboard_code;
    struct wl_seat     *seat;
    struct wl_pointer  *pointer;
    struct wl_touch    *touch;
    struct wl_keyboard *keyboard;
    struct xkb_context *xkb_context;
    struct xkb_keymap  *xkb_keymap;
    struct xkb_state   *xkb_state;

    /* DND */
    struct wl_data_device_manager *dnd_devman;
    struct wl_data_device *dnd_ddev;
    struct wl_data_offer *dnd_offer;
    enum mp_dnd_action dnd_action;
    char *dnd_mime_type;
    int dnd_mime_score;
    int dnd_fd;

    /* Cursor */
    struct wl_cursor_theme *cursor_theme;
    struct wl_cursor       *default_cursor;
    struct wl_surface      *cursor_surface;
    int                     allocated_cursor_scale;
    bool                    cursor_visible;
};

int vo_wayland_init(struct vo *vo);
int vo_wayland_reconfig(struct vo *vo);
int vo_wayland_control(struct vo *vo, int *events, int request, void *arg);
int last_available_sync(struct vo_wayland_state *wl);
void vo_wayland_uninit(struct vo *vo);
void vo_wayland_wakeup(struct vo *vo);
void vo_wayland_wait_events(struct vo *vo, int64_t until_time_us);
void vo_wayland_wait_frame(struct vo_wayland_state *wl);
void vo_wayland_set_opaque_region(struct vo_wayland_state *wl, int alpha);
void vo_wayland_sync_clear(struct vo_wayland_state *wl);
void wayland_sync_swap(struct vo_wayland_state *wl);
void vo_wayland_sync_shift(struct vo_wayland_state *wl);
void queue_new_sync(struct vo_wayland_state *wl);

#endif /* MPLAYER_WAYLAND_COMMON_H */
