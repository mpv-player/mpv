/*
 * This file is part of mpv video player.
 * Copyright © 2013 Alexander Preisinger <alexander.preisinger@gmail.com>
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_WAYLAND_COMMON_H
#define MPLAYER_WAYLAND_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>

#include "config.h"

struct vo;

struct vo_wayland_output {
    uint32_t id; /* unique name */
    struct wl_output *output;
    uint32_t flags;
    int32_t width;
    int32_t height;
    struct wl_list link;
};


struct vo_wayland_state {
    struct vo *vo;
    struct mp_log* log;

    struct {
        int fd;
        struct wl_display *display;
        struct wl_registry *registry;
        struct wl_compositor *compositor;
        struct wl_shell *shell;

        struct wl_list output_list;
        struct wl_output *fs_output; /* fullscreen output */
        int output_mode_received;

        int display_fd;

        struct wl_shm *shm;
    } display;

    struct {
        int32_t width;
        int32_t height;
        int32_t p_width;  // previous sizes for leaving fullscreen
        int32_t p_height;
        int32_t sh_width; // sheduled width for resizing
        int32_t sh_height;
        int32_t sh_x;     // x, y calculated with the drag edges for moving
        int32_t sh_y;
        float aspect;

        bool is_fullscreen; // don't keep aspect ratio in fullscreen mode

        struct wl_surface *surface;
        int32_t surf_x;
        int32_t surf_y;
        struct wl_shell_surface *shell_surface;
        int events; /* mplayer events (VO_EVENT_RESIZE) */
    } window;

    struct {
        struct wl_cursor *default_cursor;
        struct wl_cursor_theme *theme;
        struct wl_surface *surface;

        /* pointer for fading out */
        bool visible;
        struct wl_pointer *pointer;
        uint32_t serial;
    } cursor;

    struct {
        struct wl_seat *seat;
        struct wl_keyboard *keyboard;
        struct wl_pointer *pointer;

        struct {
            struct xkb_context *context;
            struct xkb_keymap *keymap;
            struct xkb_state *state;
        } xkb;
    } input;
};

int vo_wayland_init(struct vo *vo);
void vo_wayland_uninit(struct vo *vo);
bool vo_wayland_config(struct vo *vo, uint32_t d_width, uint32_t d_height, uint32_t flags);
int vo_wayland_control(struct vo *vo, int *events, int request, void *arg);

#endif /* MPLAYER_WAYLAND_COMMON_H */

