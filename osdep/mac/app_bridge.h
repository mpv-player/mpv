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

#include <stdbool.h>
#include "options/m_option.h"

struct input_ctx;
struct mpv_handle;

enum {
    FRAME_VISIBLE = 0,
    FRAME_WHOLE,
};

enum {
    RENDER_TIMER_PRESENTATION_FEEDBACK = -1,
    RENDER_TIMER_SYSTEM,
    RENDER_TIMER_CALLBACK,
    RENDER_TIMER_PRECISE,
};

enum {
    MAC_CSP_AUTO = -1,
    MAC_CSP_DISPLAY_P3,
    MAC_CSP_DISPLAY_P3_HLG,
    MAC_CSP_DISPLAY_P3_PQ,
    MAC_CSP_DISPLAY_P3_LINEAR,
    MAC_CSP_DCI_P3,
    MAC_CSP_BT_2020,
    MAC_CSP_BT_2020_LINEAR,
    MAC_CSP_BT_2100_HLG,
    MAC_CSP_BT_2100_PQ,
    MAC_CSP_BT_709,
    MAC_CSP_SRGB,
    MAC_CSP_SRGB_LINEAR,
    MAC_CSP_RGB_LINEAR,
    MAC_CSP_ADOBE,
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
    bool macos_menu_shortcuts;
    char **macos_bundle_path;
    int cocoa_cb_sw_renderer;
    bool cocoa_cb_10bit_context;
    int cocoa_cb_output_csp;
};

void cocoa_init_media_keys(void);
void cocoa_uninit_media_keys(void);
void cocoa_set_input_context(struct input_ctx *input_context);
void cocoa_set_mpv_handle(struct mpv_handle *ctx);
void cocoa_init_cocoa_cb(void);
// multithreaded wrapper for mpv_main
int cocoa_main(int argc, char *argv[]);

extern const struct m_sub_options macos_conf;
