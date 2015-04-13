/*
 * This file is part of mpv.
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

/* Stuff for correct aspect scaling. */
#include "aspect.h"
#include "vo.h"
#include "common/msg.h"
#include "options/options.h"
#include "video/mp_image.h"

#include "vo.h"
#include "sub/osd.h"

static void aspect_calc_panscan(struct mp_log *log, struct mp_vo_opts *opts,
                                int w, int h, int d_w, int d_h,
                                int window_w, int window_h, double monitor_par,
                                int *out_w, int *out_h)
{
    mp_dbg(log, "aspect(0) fitin: %dx%d monitor_par: %.2f\n",
           window_w, window_h, monitor_par);
    int fwidth = window_w;
    int fheight = (float)window_w / d_w * d_h / monitor_par;
    mp_dbg(log, "aspect(1) wh: %dx%d (org: %dx%d)\n",
           fwidth, fheight, d_w, d_h);
    if (fheight > window_h || fheight < h) {
        int tmpw = (float)window_h / d_h * d_w * monitor_par;
        if (tmpw <= window_w) {
            fheight = window_h;
            fwidth = tmpw;
        } else if (fheight > window_h) {
            mp_warn(log, "No suitable new aspect found!\n");
        }
    }
    mp_dbg(log, "aspect(2) wh: %dx%d (org: %dx%d)\n",
           fwidth, fheight, d_w, d_h);

    int vo_panscan_area = window_h - fheight;
    if (!vo_panscan_area)
        vo_panscan_area = window_w - fwidth;

    *out_w = fwidth + vo_panscan_area * opts->panscan * fwidth / fheight;
    *out_h = fheight + vo_panscan_area * opts->panscan;
}

// Clamp [start, end) to range [0, size) with various fallbacks.
static void clamp_size(int size, int *start, int *end)
{
    *start = MPMAX(0, *start);
    *end = MPMIN(size, *end);
    if (*start >= *end) {
        *start = 0;
        *end = 1;
    }
}

// Round source to a multiple of 2, this is at least needed for vo_direct3d
// and ATI cards.
#define VID_SRC_ROUND_UP(x) (((x) + 1) & ~1)

static void src_dst_split_scaling(int src_size, int dst_size,
                                  int scaled_src_size, bool unscaled,
                                  float zoom, float align, float pan,
                                  int *src_start, int *src_end,
                                  int *dst_start, int *dst_end,
                                  int *osd_margin_a, int *osd_margin_b)
{
    if (unscaled) {
        scaled_src_size = src_size;
        zoom = 0.0;
    }

    scaled_src_size += zoom * scaled_src_size;
    align = (align + 1) / 2;

    *src_start = 0;
    *src_end = src_size;
    *dst_start = (dst_size - scaled_src_size) * align + pan * scaled_src_size;
    *dst_end = *dst_start + scaled_src_size;

    // Distance of screen frame to video
    *osd_margin_a = *dst_start;
    *osd_margin_b = dst_size - *dst_end;

    // Clip to screen
    int s_src = *src_end - *src_start;
    int s_dst = *dst_end - *dst_start;
    if (*dst_start < 0) {
        int border = -(*dst_start) * s_src / s_dst;
        *src_start += VID_SRC_ROUND_UP(border);
        *dst_start = 0;
    }
    if (*dst_end > dst_size) {
        int border = (*dst_end - dst_size) * s_src / s_dst;
        *src_end -= VID_SRC_ROUND_UP(border);
        *dst_end = dst_size;
    }

    if (unscaled) {
        // Force unscaled by reducing the range for src or dst
        int src_s = *src_end - *src_start;
        int dst_s = *dst_end - *dst_start;
        if (src_s > dst_s) {
            *src_end = *src_start + dst_s;
        } else if (src_s < dst_s) {
            *dst_end = *dst_start + src_s;
        }
    }

    // For sanity: avoid bothering VOs with corner cases
    clamp_size(src_size, src_start, src_end);
    clamp_size(dst_size, dst_start, dst_end);
}

void mp_get_src_dst_rects(struct mp_log *log, struct mp_vo_opts *opts,
                          int vo_caps, struct mp_image_params *video,
                          int window_w, int window_h, double monitor_par,
                          struct mp_rect *out_src,
                          struct mp_rect *out_dst,
                          struct mp_osd_res *out_osd)
{
    int src_w = video->w;
    int src_h = video->h;
    int src_dw = video->d_w;
    int src_dh = video->d_h;
    if (video->rotate % 180 == 90 && (vo_caps & VO_CAP_ROTATE90)) {
        MPSWAP(int, src_w, src_h);
        MPSWAP(int, src_dw, src_dh);
    }
    window_w = MPMAX(1, window_w);
    window_h = MPMAX(1, window_h);
    struct mp_rect dst = {0, 0, window_w, window_h};
    struct mp_rect src = {0, 0, src_w,    src_h};
    struct mp_osd_res osd = {
        .w = window_w,
        .h = window_h,
        .display_par = monitor_par,
    };
    if (opts->keepaspect) {
        int scaled_width, scaled_height;
        aspect_calc_panscan(log, opts, src_w, src_h, src_dw, src_dh,
                            window_w, window_h, monitor_par,
                            &scaled_width, &scaled_height);
        src_dst_split_scaling(src_w, window_w, scaled_width, opts->unscaled,
                              opts->zoom, opts->align_x, opts->pan_x,
                              &src.x0, &src.x1, &dst.x0, &dst.x1,
                              &osd.ml, &osd.mr);
        src_dst_split_scaling(src_h, window_h, scaled_height, opts->unscaled,
                              opts->zoom, opts->align_y, opts->pan_y,
                              &src.y0, &src.y1, &dst.y0, &dst.y1,
                              &osd.mt, &osd.mb);
    }

    *out_src = src;
    *out_dst = dst;
    *out_osd = osd;

    int sw = src.x1 - src.x0, sh = src.y1 - src.y0;
    int dw = dst.x1 - dst.x0, dh = dst.y1 - dst.y0;

    mp_verbose(log, "Window size: %dx%d\n",
               window_w, window_h);
    mp_verbose(log, "Video source: %dx%d (%dx%d)\n",
               video->w, video->h, video->d_w, video->d_h);
    mp_verbose(log, "Video display: (%d, %d) %dx%d -> (%d, %d) %dx%d\n",
               src.x0, src.y0, sw, sh, dst.x0, dst.y0, dw, dh);
    mp_verbose(log, "Video scale: %f/%f\n",
               (double)dw / sw, (double)dh / sh);
    mp_verbose(log, "OSD borders: l=%d t=%d r=%d b=%d\n",
               osd.ml, osd.mt, osd.mr, osd.mb);
    mp_verbose(log, "Video borders: l=%d t=%d r=%d b=%d\n",
               dst.x0, dst.y0, window_w - dst.x1, window_h - dst.y1);
}
