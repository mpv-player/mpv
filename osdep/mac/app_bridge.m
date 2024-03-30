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

#include "config.h"

#import "osdep/mac/app_bridge_objc.h"

#if HAVE_SWIFT
#include "osdep/mac/swift.h"
#endif

#define OPT_BASE_STRUCT struct macos_opts
const struct m_sub_options macos_conf = {
    .opts = (const struct m_option[]) {
        {"macos-title-bar-appearance", OPT_CHOICE(macos_title_bar_appearance,
            {"auto", 0}, {"aqua", 1}, {"darkAqua", 2},
            {"vibrantLight", 3}, {"vibrantDark", 4},
            {"aquaHighContrast", 5}, {"darkAquaHighContrast", 6},
            {"vibrantLightHighContrast", 7},
            {"vibrantDarkHighContrast", 8})},
        {"macos-title-bar-material", OPT_CHOICE(macos_title_bar_material,
            {"titlebar", 0}, {"selection", 1}, {"menu", 2},
            {"popover", 3}, {"sidebar", 4}, {"headerView", 5},
            {"sheet", 6}, {"windowBackground", 7}, {"hudWindow", 8},
            {"fullScreen", 9}, {"toolTip", 10}, {"contentBackground", 11},
            {"underWindowBackground", 12}, {"underPageBackground", 13},
            {"dark", 14}, {"light", 15}, {"mediumLight", 16},
            {"ultraDark", 17})},
        {"macos-title-bar-color", OPT_COLOR(macos_title_bar_color)},
        {"macos-fs-animation-duration",
            OPT_CHOICE(macos_fs_animation_duration, {"default", -1}),
            M_RANGE(0, 1000)},
        {"macos-force-dedicated-gpu", OPT_BOOL(macos_force_dedicated_gpu)},
        {"macos-app-activation-policy", OPT_CHOICE(macos_app_activation_policy,
            {"regular", 0}, {"accessory", 1}, {"prohibited", 2})},
        {"macos-geometry-calculation", OPT_CHOICE(macos_geometry_calculation,
            {"visible", FRAME_VISIBLE}, {"whole", FRAME_WHOLE})},
        {"macos-render-timer", OPT_CHOICE(macos_render_timer,
            {"callback", RENDER_TIMER_CALLBACK}, {"precise", RENDER_TIMER_PRECISE},
            {"system", RENDER_TIMER_SYSTEM})},
        {"cocoa-cb-sw-renderer", OPT_CHOICE(cocoa_cb_sw_renderer,
            {"auto", -1}, {"no", 0}, {"yes", 1})},
        {"cocoa-cb-10bit-context", OPT_BOOL(cocoa_cb_10bit_context)},
        {0}
    },
    .size = sizeof(struct macos_opts),
    .defaults = &(const struct macos_opts){
        .macos_title_bar_color = {0, 0, 0, 0},
        .macos_fs_animation_duration = -1,
        .cocoa_cb_sw_renderer = -1,
        .cocoa_cb_10bit_context = true
    },
};

static const char app_icon[] =
#include "TOOLS/osxbundle/icon.icns.inc"
;

NSData *app_bridge_icon(void)
{
    return [NSData dataWithBytesNoCopy:(void *)app_icon length:sizeof(app_icon) - 1 freeWhenDone:NO];
}

void app_bridge_tarray_append(void *t, char ***a, int *i, char *s)
{
    MP_TARRAY_APPEND(t, *a, *i, s);
}

const struct m_sub_options *app_bridge_mac_conf(void)
{
    return &macos_conf;
}

const struct m_sub_options *app_bridge_vo_conf(void)
{
    return &vo_sub_opts;
}

void cocoa_init_media_keys(void)
{
    [[AppHub shared] startRemote];
}

void cocoa_uninit_media_keys(void)
{
    [[AppHub shared] stopRemote];
}

void cocoa_set_input_context(struct input_ctx *input_context)
{
    [[AppHub shared] initInput:input_context];
}

void cocoa_set_mpv_handle(struct mpv_handle *ctx)
{
    [[AppHub shared] initMpv:ctx];
}

void cocoa_init_cocoa_cb(void)
{
    [[AppHub shared] initCocoaCb];
}

int cocoa_main(int argc, char *argv[])
{
    return [(Application *)[Application sharedApplication] main:argc :argv];
}

