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

#ifndef MPLAYER_WAVEFORM_OPTS_H
#define MPLAYER_WAVEFORM_OPTS_H

#include <stdbool.h>
#include <stdint.h>
#include "options/m_option.h"  // struct m_color

// Waveform overlay configuration options.
// Accessed via: mp_get_config_group(ctx, global, &waveform_conf)
struct waveform_opts {
    // ── Phase 1: core functionality ──────────────────────────────────────────
    bool enable;                // --waveform: show overlay
    bool cache_enabled;         // --waveform-cache: persist to disk
    int  sample_fps;            // --waveform-sample-fps: samples/sec [8-128]
    char *sample_fps_values;    // --waveform-sample-fps-values: comma-separated presets
    bool background_scan;       // --waveform-background-scan
    int  max_concurrent_scans;  // --waveform-max-scans [1-8]
    double refresh_interval;    // --waveform-refresh-interval: min time between redraws [0.001-1.0]

    // ── Phase 3+: rendering (defined now, used later) ─────────────────────
    double window_half;         // --waveform-window: seconds each side
    int bar_x, bar_y;           // --waveform-bar-x/y: position (% canvas)
    int bar_w, bar_h;           // --waveform-bar-w/h: size (% canvas)
    struct m_color color_bg;
    struct m_color color_bar;
    struct m_color color_past;
    struct m_color color_head;
    struct m_color color_time;
    struct m_color color_chapter;
};

extern const struct m_sub_options waveform_conf;

#endif /* MPLAYER_WAVEFORM_OPTS_H */
