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

#pragma once

#include "input/input.h"

enum {
    FRAME_VISIBLE = 0,
    FRAME_WHOLE,
};

enum {
    RENDER_TIMER_CALLBACK = 0,
    RENDER_TIMER_PRECISE,
    RENDER_TIMER_SYSTEM,
};

struct macos_opts {
    int macos_title_bar_appearance;
    int macos_title_bar_material;
    struct m_color macos_title_bar_color;
    int macos_fs_animation_duration;
    bool macos_force_dedicated_gpu;
    int macos_app_activation_policy;
    int macos_geometry_calculation;
    int macos_render_timer;
    int cocoa_cb_sw_renderer;
    bool cocoa_cb_10bit_context;
};

void cocoa_init_media_keys(void);
void cocoa_uninit_media_keys(void);
void cocoa_set_input_context(struct input_ctx *input_context);
void cocoa_set_mpv_handle(struct mpv_handle *ctx);
void cocoa_init_cocoa_cb(void);

extern const struct m_sub_options macos_conf;
