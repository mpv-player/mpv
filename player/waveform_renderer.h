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

#ifndef MPLAYER_WAVEFORM_RENDERER_H
#define MPLAYER_WAVEFORM_RENDERER_H

#include <stdbool.h>
#include "waveform_cache.h"
#include "waveform_opts.h"

struct mp_log;
struct mp_osd_res;
struct sub_bitmaps;

// Opaque renderer state
struct waveform_renderer;

// Create waveform renderer
// log: logging context (owned by caller, must outlive renderer)
// opts: configuration options (owned by caller, must outlive renderer)
struct waveform_renderer *waveform_renderer_create(
    struct mp_log *log,
    struct waveform_opts *opts);

// Destroy renderer and free all resources
void waveform_renderer_destroy(struct waveform_renderer *r);

// Update cached waveform data (called from scanner callback)
// samples: array of waveform samples (copied internally)
// count: number of samples
// duration: total audio duration in seconds
void waveform_renderer_set_data(
    struct waveform_renderer *r,
    struct waveform_sample *samples,
    int count,
    double duration);

// Update current playback position (called from playloop)
// time_pos: current playback time in seconds
// duration: total video duration in seconds
void waveform_renderer_set_position(
    struct waveform_renderer *r,
    double time_pos,
    double duration);

// Update video dimensions (called on window resize)
// video_w, video_h: video display dimensions in pixels
void waveform_renderer_set_dimensions(
    struct waveform_renderer *r,
    int video_w,
    int video_h);

// Generate OSD overlay (called from OSD rendering system)
// out: output sub_bitmaps structure (filled by this function)
// video_res: current video resolution and margins
// pts: current presentation timestamp
void waveform_renderer_draw(
    struct waveform_renderer *r,
    struct sub_bitmaps *out,
    struct mp_osd_res video_res,
    double pts);

#endif /* MPLAYER_WAVEFORM_RENDERER_H */
