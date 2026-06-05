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

#include "waveform_opts.h"
#include "options/m_config.h"
#include "options/m_option.h"

#define OPT_BASE_STRUCT struct waveform_opts

// Helper: AARRGGBB → struct m_color {r,g,b,a}
#define WAVEFORM_COLOR(aa, rr, gg, bb) \
    { .r = (rr), .g = (gg), .b = (bb), .a = (aa) }

const struct m_sub_options waveform_conf = {
    .opts = (const struct m_option[]){
        // ── Phase 1: core ────────────────────────────────────────────────────
        {"waveform",                OPT_BOOL(enable),               .flags = UPDATE_OSD},
        {"waveform-cache",          OPT_BOOL(cache_enabled)},
        {"waveform-sample-fps",     OPT_INT(sample_fps),            M_RANGE(8, 128)},
        {"waveform-sample-fps-values", OPT_STRING(sample_fps_values)},
        {"waveform-background-scan",OPT_BOOL(background_scan)},
        {"waveform-max-scans",      OPT_INT(max_concurrent_scans),  M_RANGE(1, 8)},
        {"waveform-refresh-interval", OPT_DOUBLE(refresh_interval), M_RANGE(0.001, 1.0)},

        // ── Phase 3+: rendering (parsed, not yet used) ────────────────────
        {"waveform-window",  OPT_DOUBLE(window_half), M_RANGE(0.1, 60.0)},
        {"waveform-bar-x",   OPT_INT(bar_x),          M_RANGE(0, 100)},
        {"waveform-bar-y",   OPT_INT(bar_y),          M_RANGE(0, 100)},
        {"waveform-bar-w",   OPT_INT(bar_w),          M_RANGE(1, 100)},
        {"waveform-bar-h",   OPT_INT(bar_h),          M_RANGE(1, 100)},
        {"waveform-color-bg",      OPT_COLOR(color_bg)},
        {"waveform-color-bar",     OPT_COLOR(color_bar)},
        {"waveform-color-past",    OPT_COLOR(color_past)},
        {"waveform-color-head",    OPT_COLOR(color_head)},
        {"waveform-color-time",    OPT_COLOR(color_time)},
        {"waveform-color-chapter", OPT_COLOR(color_chapter)},

        {0}
    },
    .size = sizeof(struct waveform_opts),
    .defaults = &(const struct waveform_opts){
        // Phase 1 defaults
        .enable              = false,
        .cache_enabled       = true,
        .sample_fps          = 32,
        .sample_fps_values   = "8,16,32,48,64",
        .background_scan     = true,
        .max_concurrent_scans = 2,
        .refresh_interval    = 0.016,  // ~60fps refresh rate (16ms)

        // Phase 3+ defaults (match Lua script values)
        .window_half = 10.0,
        .bar_x = 0,  .bar_y = 72,  // bar_x=0 starts at left edge (X=0)
        .bar_w = 90, .bar_h = 14,
        // Colors: AARRGGBB → {r, g, b, a}
        .color_bg      = WAVEFORM_COLOR(0xCC, 0x00, 0x00, 0x00),
        .color_bar     = WAVEFORM_COLOR(0x88, 0x00, 0xAA, 0xFF),
        .color_past    = WAVEFORM_COLOR(0x88, 0xFF, 0x77, 0x00),
        .color_head    = WAVEFORM_COLOR(0x00, 0xFF, 0xFF, 0xFF),
        .color_time    = WAVEFORM_COLOR(0x00, 0xAA, 0xAA, 0xAA),
        .color_chapter = WAVEFORM_COLOR(0x44, 0xFF, 0xFF, 0xFF),
    },
};
