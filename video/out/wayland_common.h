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

#include "sub/osd.h"

#if HAVE_GL_WAYLAND
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

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

#if HAVE_GL_WAYLAND
    struct {
        EGLSurface egl_surface;

        struct wl_egl_window *egl_window;

        struct {
            EGLDisplay dpy;
            EGLContext ctx;
            EGLConfig conf;
        } egl;
    } egl_context;
#endif

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

        struct wl_subcompositor *subcomp;
    } display;

    struct {
        int32_t width;    // current size of the window
        int32_t height;
        int32_t p_width;  // previous sizes for leaving fullscreen
        int32_t p_height;
        int32_t sh_width; // sheduled width for resizing
        int32_t sh_height;
        int32_t sh_x;     // x, y calculated with the drag edges for moving
        int32_t sh_y;
        float aspect;

        bool is_init;       // true if the window has a valid size
        bool is_fullscreen; // don't keep aspect ratio in fullscreen mode
        int32_t fs_width;   // fullscreen sizes
        int32_t fs_height;

        struct wl_surface *video_surface;
        int32_t mouse_x; // mouse position inside the surface
        int32_t mouse_y;
        struct wl_shell_surface *shell_surface;
        int events; /* mplayer events (VO_EVENT_RESIZE) */

        struct wl_surface *osd_surfaces[MAX_OSD_PARTS];
        struct wl_subsurface *osd_subsurfaces[MAX_OSD_PARTS];
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

        struct wl_data_device_manager *devman;
        struct wl_data_device *datadev;
        struct wl_data_offer *offer;
        int dnd_fd;
    } input;
};

int vo_wayland_init(struct vo *vo);
void vo_wayland_uninit(struct vo *vo);
bool vo_wayland_config(struct vo *vo, uint32_t d_width, uint32_t d_height, uint32_t flags);
int vo_wayland_control(struct vo *vo, int *events, int request, void *arg);

#endif /* MPLAYER_WAYLAND_COMMON_H */

