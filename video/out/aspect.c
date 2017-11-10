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

/* Stuff for correct aspect scaling. */
#include "aspect.h"
#include "math.h"
#include "vo.h"
#include "common/msg.h"
#include "options/options.h"
#include "video/mp_image.h"

#include "vo.h"
#include "sub/osd.h"

static void aspect_calc_panscan(struct mp_vo_opts *opts,
                                int w, int h, int d_w, int d_h, int unscaled,
                                int window_w, int window_h, double monitor_par,
                                int *out_w, int *out_h)
{
    w *= monitor_par;

    int fwidth = window_w;
    int fheight = (float)window_w / d_w * d_h / monitor_par;
    if (fheight > window_h || fheight < h) {
        int tmpw = (float)window_h / d_h * d_w * monitor_par;
        if (tmpw <= window_w) {
            fheight = window_h;
            fwidth = tmpw;
        }
    }

    int vo_panscan_area = window_h - fheight;
    double f_w = fwidth / (double)fheight;
    double f_h = 1;
    if (vo_panscan_area == 0) {
        vo_panscan_area = window_w - fwidth;
        f_w = 1;
        f_h = fheight / (double)fwidth;
    }

    if (unscaled) {
        vo_panscan_area = 0;
        if (unscaled != 2 || (d_w <= window_w && d_h <= window_h)) {
            fwidth = d_w * monitor_par;
            fheight = d_h;
        }
    }

    *out_w = fwidth + vo_panscan_area * opts->panscan * f_w;
    *out_h = fheight + vo_panscan_area * opts->panscan * f_h;
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

static void src_dst_split_scaling(int src_size, int dst_size,
                                  int scaled_src_size,
                                  float zoom, float align, float pan,
                                  int *src_start, int *src_end,
                                  int *dst_start, int *dst_end,
                                  int *osd_margin_a, int *osd_margin_b)
{
    scaled_src_size *= powf(2, zoom);
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
        *src_start += border;
        *dst_start = 0;
    }
    if (*dst_end > dst_size) {
        int border = (*dst_end - dst_size) * s_src / s_dst;
        *src_end -= border;
        *dst_end = dst_size;
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
    int src_dw, src_dh;
    mp_image_params_get_dsize(video, &src_dw, &src_dh);
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
        aspect_calc_panscan(opts, src_w, src_h, src_dw, src_dh, opts->unscaled,
                            window_w, window_h, monitor_par,
                            &scaled_width, &scaled_height);
        src_dst_split_scaling(src_w, window_w, scaled_width,
                              opts->zoom, opts->align_x, opts->pan_x,
                              &src.x0, &src.x1, &dst.x0, &dst.x1,
                              &osd.ml, &osd.mr);
        src_dst_split_scaling(src_h, window_h, scaled_height,
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
    mp_verbose(log, "Video source: %dx%d (%d:%d)\n",
               video->w, video->h, video->p_w, video->p_h);
    mp_verbose(log, "Video display: (%d, %d) %dx%d -> (%d, %d) %dx%d\n",
               src.x0, src.y0, sw, sh, dst.x0, dst.y0, dw, dh);
    mp_verbose(log, "Video scale: %f/%f\n",
               (double)dw / sw, (double)dh / sh);
    mp_verbose(log, "OSD borders: l=%d t=%d r=%d b=%d\n",
               osd.ml, osd.mt, osd.mr, osd.mb);
    mp_verbose(log, "Video borders: l=%d t=%d r=%d b=%d\n",
               dst.x0, dst.y0, window_w - dst.x1, window_h - dst.y1);
}
