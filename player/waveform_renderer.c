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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "waveform_renderer.h"
#include "mpv_talloc.h"
#include "common/msg.h"
#include "sub/osd.h"
#include "video/mp_image.h"
#include "video/fmt-conversion.h"
#include "video/img_format.h"

struct waveform_renderer {
    struct mp_log *log;
    struct waveform_opts *opts;

    // Cached waveform data
    struct waveform_sample *samples;
    int sample_count;
    double duration;

    // Playback state
    double time_pos;
    double video_duration;

    // Video dimensions
    int video_w, video_h;

    // Bar geometry (pixels)
    int bar_x, bar_y;
    int bar_w, bar_h;

    // Rendering state
    uint32_t *texture_data;  // RGBA pixel buffer (B,G,R,A in little-endian)
    int texture_w, texture_h;
    bool needs_redraw;
    int change_id;

    // OSD integration
    struct mp_image *packed_img;   // Packed mp_image for OSD system
    struct sub_bitmap bitmap_part;
};

// ── Color utilities ──────────────────────────────────────────────────────────

// Convert m_color to BGRA32 (B=LSB, A=MSB)
static inline uint32_t color_to_bgra(struct m_color c)
{
    return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) |
           ((uint32_t)c.g << 8) | (uint32_t)c.b;
}

// Blend two colors with alpha
static inline uint32_t blend_colors(uint32_t bg, uint32_t fg)
{
    uint8_t fg_a = (fg >> 24) & 0xFF;
    if (fg_a == 0xFF)
        return fg;
    if (fg_a == 0)
        return bg;

    uint8_t bg_b = bg & 0xFF;
    uint8_t bg_g = (bg >> 8) & 0xFF;
    uint8_t bg_r = (bg >> 16) & 0xFF;
    uint8_t bg_a = (bg >> 24) & 0xFF;

    uint8_t fg_b = fg & 0xFF;
    uint8_t fg_g = (fg >> 8) & 0xFF;
    uint8_t fg_r = (fg >> 16) & 0xFF;

    uint8_t out_a = fg_a + (bg_a * (255 - fg_a) / 255);
    uint8_t out_r = (fg_r * fg_a + bg_r * bg_a * (255 - fg_a) / 255) / (out_a ? out_a : 1);
    uint8_t out_g = (fg_g * fg_a + bg_g * bg_a * (255 - fg_a) / 255) / (out_a ? out_a : 1);
    uint8_t out_b = (fg_b * fg_a + bg_b * bg_a * (255 - fg_a) / 255) / (out_a ? out_a : 1);

    return ((uint32_t)out_a << 24) | ((uint32_t)out_r << 16) |
           ((uint32_t)out_g << 8) | (uint32_t)out_b;
}

// ── Geometry calculation ─────────────────────────────────────────────────────

static void update_bar_geometry(struct waveform_renderer *r)
{
    if (r->video_w <= 0 || r->video_h <= 0) {
        r->bar_w = 0;
        r->bar_h = 0;
        return;
    }

    // Calculate bar position and size from percentages
    r->bar_w = (r->video_w * r->opts->bar_w) / 100;
    r->bar_h = (r->video_h * r->opts->bar_h) / 100;
    r->bar_x = (r->video_w * r->opts->bar_x) / 100;
    r->bar_y = (r->video_h * r->opts->bar_y) / 100;

    // Clamp to reasonable values
    if (r->bar_w < 100)
        r->bar_w = 100;
    if (r->bar_h < 20)
        r->bar_h = 20;
    if (r->bar_w > r->video_w)
        r->bar_w = r->video_w;
    if (r->bar_h > r->video_h / 3)
        r->bar_h = r->video_h / 3;

    mp_verbose(r->log, "Bar geometry: %dx%d at (%d,%d)\n",
               r->bar_w, r->bar_h, r->bar_x, r->bar_y);
}

// ── Texture rendering ────────────────────────────────────────────────────────

static void render_waveform_texture(struct waveform_renderer *r)
{
    if (!r->samples || r->sample_count == 0 || r->duration <= 0) {
        mp_verbose(r->log, "No waveform data to render\n");
        return;
    }

    if (r->bar_w <= 0 || r->bar_h <= 0) {
        mp_verbose(r->log, "Invalid bar dimensions\n");
        return;
    }

    // Allocate texture if needed
    int required_size = r->bar_w * r->bar_h;
    if (!r->texture_data || r->texture_w != r->bar_w || r->texture_h != r->bar_h) {
        r->texture_data = talloc_realloc(r, r->texture_data, uint32_t, required_size);
        r->texture_w = r->bar_w;
        r->texture_h = r->bar_h;
        mp_verbose(r->log, "Allocated texture: %dx%d\n", r->texture_w, r->texture_h);
    }

    // Pre-convert colors
    uint32_t color_bg = color_to_bgra(r->opts->color_bg);
    uint32_t color_bar = color_to_bgra(r->opts->color_bar);
    uint32_t color_past = color_to_bgra(r->opts->color_past);
    uint32_t color_head = color_to_bgra(r->opts->color_head);

    // Clear to background
    for (int i = 0; i < required_size; i++) {
        r->texture_data[i] = color_bg;
    }

    // Calculate viewport window
    double window_start = r->time_pos - r->opts->window_half;
    double window_end = r->time_pos + r->opts->window_half;
    double window_duration = window_end - window_start;

    if (window_duration <= 0) {
        mp_warn(r->log, "Invalid window duration\n");
        return;
    }

    // Clamp window to file boundaries
    if (window_start < 0) {
        window_end -= window_start;
        window_start = 0;
    }
    if (window_end > r->duration) {
        window_start -= (window_end - r->duration);
        window_end = r->duration;
        if (window_start < 0)
            window_start = 0;
    }

    int channel_h = r->bar_h / 2;  // Height per channel

    // Render waveform bars
    for (int x = 0; x < r->bar_w; x++) {
        // Map pixel X to time
        double t = window_start + (x * window_duration / r->bar_w);

        // Map time to sample index
        int sample_idx = (int)((t / r->duration) * r->sample_count);
        if (sample_idx < 0)
            sample_idx = 0;
        if (sample_idx >= r->sample_count)
            sample_idx = r->sample_count - 1;

        struct waveform_sample sample = r->samples[sample_idx];

        // Choose color based on playback position
        uint32_t bar_color = (t <= r->time_pos) ? color_past : color_bar;

        // Convert mono RMS to bar height (use same value for both channels)
        // RMS is normalized [0.0, 1.0]
        int bar_h = (int)(sample.rms * channel_h);

        // Top channel (mirrored upward from center)
        for (int y = channel_h - bar_h; y < channel_h; y++) {
            if (y >= 0 && y < r->bar_h) {
                int idx = y * r->bar_w + x;
                r->texture_data[idx] = blend_colors(r->texture_data[idx], bar_color);
            }
        }

        // Bottom channel (mirrored downward from center)
        for (int y = channel_h; y < channel_h + bar_h; y++) {
            if (y >= 0 && y < r->bar_h) {
                int idx = y * r->bar_w + x;
                r->texture_data[idx] = blend_colors(r->texture_data[idx], bar_color);
            }
        }
    }

    // Draw playback head (vertical line at current position)
    int head_x = (int)(((r->time_pos - window_start) / window_duration) * r->bar_w);
    if (head_x >= 0 && head_x < r->bar_w) {
        for (int y = 0; y < r->bar_h; y++) {
            int idx = y * r->bar_w + head_x;
            r->texture_data[idx] = blend_colors(r->texture_data[idx], color_head);
        }
    }

    mp_verbose(r->log, "Rendered waveform: time=%.2f window=[%.2f, %.2f] head_x=%d\n",
               r->time_pos, window_start, window_end, head_x);
}

// ── Public API ───────────────────────────────────────────────────────────────

struct waveform_renderer *waveform_renderer_create(struct mp_log *log,
                                                   struct waveform_opts *opts)
{
    struct waveform_renderer *r = talloc_zero(NULL, struct waveform_renderer);
    r->log = log;
    r->opts = opts;
    r->needs_redraw = true;
    r->change_id = 1;

    mp_info(log, "Waveform renderer created\n");
    return r;
}

void waveform_renderer_destroy(struct waveform_renderer *r)
{
    if (!r)
        return;

    mp_info(r->log, "Waveform renderer destroyed\n");
    talloc_free(r);
}

void waveform_renderer_set_data(struct waveform_renderer *r,
                                struct waveform_sample *samples,
                                int count,
                                double duration)
{
    if (!r)
        return;

    // Free old data
    talloc_free(r->samples);

    // Copy new data
    r->samples = talloc_array(r, struct waveform_sample, count);
    memcpy(r->samples, samples, count * sizeof(struct waveform_sample));
    r->sample_count = count;
    r->duration = duration;

    r->needs_redraw = true;
    r->change_id++;

    mp_info(r->log, "Waveform data updated: %d samples, %.2f seconds\n",
            count, duration);
}

void waveform_renderer_set_position(struct waveform_renderer *r,
                                    double time_pos,
                                    double duration)
{
    if (!r)
        return;

    // Check if position changed significantly based on configured refresh interval
    // Use refresh_interval as the threshold for updates
    double threshold = r->opts ? r->opts->refresh_interval : 0.016;
    if (fabs(time_pos - r->time_pos) < threshold && duration == r->video_duration)
        return;

    r->time_pos = time_pos;
    r->video_duration = duration;
    r->needs_redraw = true;
    r->change_id++;
}

void waveform_renderer_set_dimensions(struct waveform_renderer *r,
                                      int video_w,
                                      int video_h)
{
    if (!r)
        return;

    if (r->video_w == video_w && r->video_h == video_h)
        return;

    r->video_w = video_w;
    r->video_h = video_h;

    update_bar_geometry(r);
    r->needs_redraw = true;
    r->change_id++;
}

void waveform_renderer_draw(struct waveform_renderer *r,
                            struct sub_bitmaps *out,
                            struct mp_osd_res video_res,
                            double pts)
{
    if (!r || !out)
        return;

    // Check if waveform is enabled
    if (!r->opts || !r->opts->enable) {
        out->format = SUBBITMAP_EMPTY;
        out->num_parts = 0;
        return;
    }

    // Update dimensions if video size changed
    if (video_res.w != r->video_w || video_res.h != r->video_h) {
        waveform_renderer_set_dimensions(r, video_res.w, video_res.h);
    }

    // Render to texture if needed
    if (r->needs_redraw) {
        render_waveform_texture(r);
        r->needs_redraw = false;
    }

    // Skip if no texture
    if (!r->texture_data || r->bar_w <= 0 || r->bar_h <= 0) {
        out->format = SUBBITMAP_EMPTY;
        out->num_parts = 0;
        return;
    }

    // Create or recreate packed mp_image if needed
    if (!r->packed_img || r->packed_img->w != r->bar_w || r->packed_img->h != r->bar_h) {
        talloc_free(r->packed_img);
        r->packed_img = mp_image_alloc(IMGFMT_BGRA, r->bar_w, r->bar_h);
        if (!r->packed_img) {
            mp_err(r->log, "Failed to allocate mp_image for waveform\n");
            out->format = SUBBITMAP_EMPTY;
            out->num_parts = 0;
            return;
        }
        talloc_steal(r, r->packed_img);
        mp_verbose(r->log, "Allocated packed mp_image: %dx%d\n", r->bar_w, r->bar_h);
    }

    // Make image writable
    if (!mp_image_make_writeable(r->packed_img)) {
        mp_err(r->log, "Failed to make mp_image writable\n");
        out->format = SUBBITMAP_EMPTY;
        out->num_parts = 0;
        return;
    }

    // Copy texture data to mp_image
    // BGRA format: 4 bytes per pixel, packed
    uint8_t *dst = r->packed_img->planes[0];
    int dst_stride = r->packed_img->stride[0];
    uint32_t *src = r->texture_data;
    int src_stride = r->bar_w * 4;  // bytes per row

    for (int y = 0; y < r->bar_h; y++) {
        memcpy(dst + y * dst_stride, src + y * r->bar_w, src_stride);
    }

    // Fill sub_bitmaps structure with packed mp_image
    out->format = SUBBITMAP_BGRA;
    out->num_parts = 1;
    out->parts = &r->bitmap_part;
    out->change_id = r->change_id++;
    out->packed = r->packed_img;
    out->packed_w = r->bar_w;
    out->packed_h = r->bar_h;

    // Set bitmap properties to point into the packed image
    r->bitmap_part.bitmap = r->packed_img->planes[0];
    r->bitmap_part.stride = r->packed_img->stride[0];
    r->bitmap_part.w = r->bar_w;
    r->bitmap_part.h = r->bar_h;
    r->bitmap_part.dw = r->bar_w;
    r->bitmap_part.dh = r->bar_h;
    r->bitmap_part.x = r->bar_x;
    r->bitmap_part.y = r->bar_y;
    r->bitmap_part.src_x = 0;
    r->bitmap_part.src_y = 0;

    mp_verbose(r->log, "OSD overlay: %dx%d at (%d,%d) time=%.2f\n",
               r->bar_w, r->bar_h, r->bar_x, r->bar_y, r->time_pos);
}
