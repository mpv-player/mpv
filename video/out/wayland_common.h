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

    /* State */
    struct mp_rect geometry;
    struct mp_rect window_size;
    float aspect_ratio;
    bool fullscreen;
    bool configured;
    int wakeup_pipe[2];
    int pending_vo_events;
    int mouse_x;
    int mouse_y;
    int scaling;
    int touch_entries;
    uint32_t pointer_id;
    int display_fd;
    struct wl_callback       *frame_callback;
    struct wl_list            output_list;
    struct vo_wayland_output *current_output;

    /* Shell */
    struct wl_surface       *surface;
    struct zxdg_shell_v6    *shell;
    struct zxdg_toplevel_v6 *xdg_toplevel;
    struct zxdg_surface_v6  *xdg_surface;
    struct org_kde_kwin_server_decoration_manager *server_decoration_manager;
    struct org_kde_kwin_server_decoration *server_decoration;
    struct zwp_idle_inhibit_manager_v1 *idle_inhibit_manager;
    struct zwp_idle_inhibitor_v1 *idle_inhibitor;

    /* Input */
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
};

int vo_wayland_init(struct vo *vo);
int vo_wayland_reconfig(struct vo *vo);
int vo_wayland_control(struct vo *vo, int *events, int request, void *arg);
void vo_wayland_check_events(struct vo *vo);
void vo_wayland_uninit(struct vo *vo);
void vo_wayland_wakeup(struct vo *vo);
void vo_wayland_wait_events(struct vo *vo, int64_t until_time_us);

#endif /* MPLAYER_WAYLAND_COMMON_H */
