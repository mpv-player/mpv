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
#include "video/mp_image.h"
#include "vo.h"
#include "xdg-activation-v1.h"

struct compositor_format;
struct vo_wayland_seat;
struct vo_wayland_tranche;
struct vo_wayland_data_offer;

struct drm_format {
    uint32_t format;
    uint64_t modifier;
};

struct vo_wayland_state {
    struct m_config_cache   *opts_cache;
    struct mp_log           *log;
    struct mp_vo_opts       *opts;
    struct vo               *vo;
    struct wl_callback      *frame_callback;
    struct wl_compositor    *compositor;
    struct wl_subcompositor *subcompositor;
    struct wl_display       *display;
    struct wl_registry      *registry;
    struct wl_shm           *shm;
    struct wl_surface       *surface;
    struct wl_surface       *osd_surface;
    struct wl_subsurface    *osd_subsurface;
    struct wl_surface       *video_surface;
    struct wl_surface       *callback_surface;
    struct wl_subsurface    *video_subsurface;

    /* Geometry */
    struct mp_rect geometry;
    struct mp_rect window_size;
    struct wl_list output_list;
    struct vo_wayland_output *current_output;
    int bounded_height;
    int bounded_width;
    int reduced_height;
    int reduced_width;

    /* State */
    bool activated;
    bool focused;
    bool frame_wait;
    bool geometry_configured;
    bool hidden;
    bool initial_size_hint;
    bool locked_size;
    bool need_rescale;
    bool reconfigured;
    bool resizing;
    bool scale_configured;
    bool state_change;
    bool tiled;
    bool toplevel_configured;
    int display_fd;
    int mouse_x;
    int mouse_y;
    int pending_vo_events;
    int pending_scaling;   // base 120
    int scaling;           // base 120
    double scaling_factor; // wl->scaling divided by 120
    int resizing_constraint;
    int timeout_count;
    int wakeup_pipe[2];

    /* color-management */
    struct wp_color_manager_v1 *color_manager;
    struct wp_color_management_surface_v1 *color_surface;
    struct wp_color_management_surface_feedback_v1 *color_surface_feedback;
    struct wp_image_description_creator_icc_v1 *icc_creator;
    struct mp_image_params target_params;
    bool supports_icc;
    bool supports_parametric;
    bool supports_primaries;
    bool supports_tf_power;
    bool supports_luminances;
    bool supports_display_primaries;
    int primaries_map[PL_COLOR_PRIM_COUNT];
    int transfer_map[PL_COLOR_TRC_COUNT];
    void *icc_file;
    uint32_t icc_size;

    /* content-type */
    struct wp_content_type_manager_v1 *content_type_manager;
    struct wp_content_type_v1 *content_type;
    int current_content_type;

    /* cursor-shape */
    struct wp_cursor_shape_manager_v1 *cursor_shape_manager;

    /* fifo */
    bool has_fifo;

    /* fractional-scale */
    struct wp_fractional_scale_manager_v1 *fractional_scale_manager;
    struct wp_fractional_scale_v1 *fractional_scale;

    /* idle-inhibit */
    struct zwp_idle_inhibit_manager_v1 *idle_inhibit_manager;
    struct zwp_idle_inhibitor_v1 *idle_inhibitor;

    /* text-input */
    struct zwp_text_input_manager_v3 *text_input_manager;

    /* linux-dmabuf */
    struct wl_list tranche_list;
    struct vo_wayland_tranche *current_tranche;
    struct zwp_linux_dmabuf_v1 *dmabuf;
    struct zwp_linux_dmabuf_feedback_v1 *dmabuf_feedback;
    struct compositor_format *compositor_format_map;
    uint32_t compositor_format_size;

    /* presentation-time */
    struct wp_presentation  *presentation;
    struct vo_wayland_feedback_pool *fback_pool;
    struct mp_present *present;
    int64_t refresh_interval;
    bool present_clock;
    bool present_v2;
    bool use_present;

    /* single-pixel-buffer */
    struct wp_single_pixel_buffer_manager_v1 *single_pixel_manager;

    /* xdg-activation */
    struct xdg_activation_v1 *xdg_activation;

    /* xdg-decoration */
    struct zxdg_decoration_manager_v1 *xdg_decoration_manager;
    struct zxdg_toplevel_decoration_v1 *xdg_toplevel_decoration;
    int requested_decoration;

    /* xdg-shell */
    struct xdg_wm_base      *wm_base;
    struct xdg_surface      *xdg_surface;
    struct xdg_toplevel     *xdg_toplevel;

    /* viewporter */
    struct wp_viewporter *viewporter;
    struct wp_viewport   *viewport;
    struct wp_viewport   *cursor_viewport;
    struct wp_viewport   *osd_viewport;
    struct wp_viewport   *video_viewport;

    /* Input */
    struct wl_list seat_list;
    struct xkb_context *xkb_context;

    /* Data offer */
    struct wl_data_device_manager *devman;
    bstr selection_text;

    /* Cursor */
    struct wl_cursor_theme *cursor_theme;
    struct wl_cursor       *default_cursor;
    struct wl_surface      *cursor_surface;
    bool                    cursor_visible;
    int                     allocated_cursor_scale;
    struct vo_wayland_seat *last_button_seat;
};

bool vo_wayland_check_visible(struct vo *vo);
bool vo_wayland_valid_format(struct vo_wayland_state *wl, uint32_t drm_format, uint64_t modifier);
bool vo_wayland_init(struct vo *vo);
bool vo_wayland_reconfig(struct vo *vo);

int vo_wayland_allocate_memfd(struct vo *vo, size_t size);
int vo_wayland_control(struct vo *vo, int *events, int request, void *arg);

void vo_wayland_handle_color(struct vo_wayland_state *wl);
void vo_wayland_handle_scale(struct vo_wayland_state *wl);
void vo_wayland_set_opaque_region(struct vo_wayland_state *wl, bool alpha);
void vo_wayland_sync_swap(struct vo_wayland_state *wl);
void vo_wayland_uninit(struct vo *vo);
void vo_wayland_wait_events(struct vo *vo, int64_t until_time_ns);
void vo_wayland_wait_frame(struct vo_wayland_state *wl);
void vo_wayland_wakeup(struct vo *vo);

#endif /* MPLAYER_WAYLAND_COMMON_H */
