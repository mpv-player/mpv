/*
 * This file is part of MPlayer.
 * Copyright © 2012-2013 Scott Moreau <oreaus@gmail.com>
 * Copyright © 2012-2013 Alexander Preisinger <alexander.preisinger@gmail.com>
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_WAYLAND_COMMON_H
#define MPLAYER_WAYLAND_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>

#include "config.h"

enum vo_wayland_window_type {
    TYPE_TOPLEVEL,
    TYPE_FULLSCREEN
};

struct vo;
struct vo_wayland_state;


struct vo_wayland_output {
    uint32_t id; /* unique name */
    struct wl_output *output;
    uint32_t flags;
    int32_t width;
    int32_t height;
    struct wl_list link;
};

struct vo_wayland_display {
    struct wl_output *output;
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shell *shell;

    struct {
        struct wl_shm *shm;
        struct wl_cursor *default_cursor;
        struct wl_cursor_theme *theme;
        struct wl_surface *surface;

        /* save timer and pointer for fading out */
        int mouse_timer;
        bool mouse_waiting_hide;
        struct wl_pointer *pointer;
        uint32_t serial;
    } cursor;

    int display_fd;

    struct wl_list output_list;
    struct wl_output *fs_output; /* fullscreen output */
    int output_mode_received;

    uint32_t formats;
    uint32_t mask;
};

struct vo_wayland_window {
    int32_t width;
    int32_t height;
    int32_t p_width;
    int32_t p_height;

    struct wl_surface *surface;
    struct wl_shell_surface *shell_surface;

    int events; /* mplayer events (VO_EVENT_RESIZE) */

    enum vo_wayland_window_type type; /* is fullscreen */

    /* Because the egl windows have a special resize windw function we have to
     * register it first before doing any resizing.
     * This makes us independet from the output driver */
    void (*resize_func) (struct vo_wayland_state *wl,
                         uint32_t edges,
                         int32_t width,
                         int32_t height,
                         void *user_data);

    void *resize_func_data;
};

struct vo_wayland_input {
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct wl_pointer *pointer;

    struct {
        struct xkb_context *context;
        struct xkb_keymap *keymap;
        struct xkb_state *state;
    } xkb;
};

struct vo_wayland_state {
    struct vo *vo;

    struct vo_wayland_display *display;
    struct vo_wayland_window *window;
    struct vo_wayland_input *input;
};

int vo_wayland_init(struct vo *vo);
void vo_wayland_uninit(struct vo *vo);
void vo_wayland_ontop(struct vo *vo);
void vo_wayland_border(struct vo *vo);
void vo_wayland_fullscreen(struct vo *vo);
void vo_wayland_update_screeninfo(struct vo *vo);
int vo_wayland_check_events(struct vo *vo);
void vo_wayland_update_window_title(struct vo *vo);

#endif /* MPLAYER_WAYLAND_COMMON_H */

