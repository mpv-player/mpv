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

#ifndef MPV_MACOSX_APPLICATION
#define MPV_MACOSX_APPLICATION

#include "osdep/macosx_menubar.h"
#include "options/m_option.h"

enum {
    FRAME_VISIBLE = 0,
    FRAME_WHOLE,
};

enum {
    MAC_CSP_AUTO = -1,
    MAC_CSP_DISPLAY_P3, //macOS 10.11.2+
    MAC_CSP_DISPLAY_P3_HLG, //macOS 10.14.6+
    MAC_CSP_DISPLAY_P3_PQ_EOTF, //macOS 10.14.6–10.15.4
    MAC_CSP_DCIP3, //macOS 10.11+
    MAC_CSP_ITUR_2020, //macOS 10.11+
    MAC_CSP_ITUR_709, //macOS 10.11+

    MAC_CSP_SRGB, //macOS 10.5+
    MAC_CSP_LINEAR_SRGB, //macOS 10.12+
    MAC_CSP_GENERIC_RGB_LINEAR, //macOS 10.5+
    MAC_CSP_ADOBE_RGB1998, //macOS 10.5+

    // no documentation?
    MAC_CSP_DISPLAY_P3_PQ, //macOS 10.15.4+


    // extended formats with values below 0.0 and above 1.0, useless?
    MAC_CSP_EXTENDED_LINEAR_DISPLAY_P3, //macOS 10.14.3+
    MAC_CSP_EXTENDED_SRGB, //macOS 10.12+
    MAC_CSP_EXTENDED_LINEAR_SRGB, //macOS 10.12+
    MAC_CSP_EXTENDED_LINEAR_ITUR_2020, //macOS 10.14.3+
    // pixel values between 0.0 and 12.0
    MAC_CSP_ITUR_2020_HLG, //macOS 10.15.6–11.0
    // pixel value of 1.0 is assumed to be 100 nits
    MAC_CSP_ITUR_2020_PQ_EOTF, //macOS 10.14.6–10.15.4

    // no documentation?
    MAC_CSP_EXTENDED_DISPLAY_P3, //macOS 11.0+
    MAC_CSP_EXTENDED_ITUR_2020, //macOS 11.0+
};

struct macos_opts {
    int macos_title_bar_style;
    int macos_title_bar_appearance;
    int macos_title_bar_material;
    struct m_color macos_title_bar_color;
    int macos_fs_animation_duration;
    int macos_force_dedicated_gpu;
    int macos_app_activation_policy;
    int macos_geometry_calculation;
    int macos_output_csp;
    int cocoa_cb_sw_renderer;
    int cocoa_cb_10bit_context;
};

// multithreaded wrapper for mpv_main
int cocoa_main(int argc, char *argv[]);
void cocoa_register_menu_item_action(MPMenuKey key, void* action);

extern const struct m_sub_options macos_conf;

#endif /* MPV_MACOSX_APPLICATION */
