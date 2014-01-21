/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <libavutil/common.h>

/* Stuff for correct aspect scaling. */
#include "aspect.h"
#include "vo.h"
#include "common/msg.h"
#include "options/options.h"

#include "vo.h"
#include "sub/osd.h"

void aspect_save_videores(struct vo *vo, int w, int h, int d_w, int d_h)
{
    vo->aspdat.orgw = w;
    vo->aspdat.orgh = h;
    vo->aspdat.prew = d_w;
    vo->aspdat.preh = d_h;
    vo->aspdat.par = (double)d_w / d_h * h / w;
}

static void aspect_calc(struct vo *vo, int *srcw, int *srch)
{
    struct aspect_data *aspdat = &vo->aspdat;
    float pixelaspect = vo->monitor_par;

    int fitw = FFMAX(1, vo->dwidth);
    int fith = FFMAX(1, vo->dheight);

    MP_DBG(vo, "aspect(0) fitin: %dx%d monitor_par: %.2f\n",
           fitw, fith, pixelaspect);
    *srcw = fitw;
    *srch = (float)fitw / aspdat->prew * aspdat->preh / pixelaspect;
    MP_DBG(vo, "aspect(1) wh: %dx%d (org: %dx%d)\n",
           *srcw, *srch, aspdat->prew, aspdat->preh);
    if (*srch > fith || *srch < aspdat->orgh) {
        int tmpw = (float)fith / aspdat->preh * aspdat->prew * pixelaspect;
        if (tmpw <= fitw) {
            *srch = fith;
            *srcw = tmpw;
        } else if (*srch > fith) {
            MP_WARN(vo, "No suitable new aspect found!\n");
        }
    }
    MP_DBG(vo, "aspect(2) wh: %dx%d (org: %dx%d)\n",
           *srcw, *srch, aspdat->prew, aspdat->preh);
}

void aspect_calc_panscan(struct vo *vo, int *out_w, int *out_h)
{
    struct mp_vo_opts *opts = vo->opts;
    int fwidth, fheight;
    aspect_calc(vo, &fwidth, &fheight);

    int vo_panscan_area = vo->dheight - fheight;
    if (!vo_panscan_area)
        vo_panscan_area = vo->dwidth - fwidth;

    *out_w = fwidth + vo_panscan_area * opts->panscan * fwidth / fheight;
    *out_h = fheight + vo_panscan_area * opts->panscan;
}


static void print_video_rect(struct vo *vo, struct mp_rect src,
                             struct mp_rect dst, struct mp_osd_res osd)
{
    int sw = src.x1 - src.x0, sh = src.y1 - src.y0;
    int dw = dst.x1 - dst.x0, dh = dst.y1 - dst.y0;

    MP_VERBOSE(&vo->vo_log, "Window size: %dx%d\n",
               vo->dwidth, vo->dheight);
    MP_VERBOSE(&vo->vo_log, "Video source: %dx%d (%dx%d)\n",
               vo->aspdat.orgw, vo->aspdat.orgh,
               vo->aspdat.prew, vo->aspdat.preh);
    MP_VERBOSE(&vo->vo_log, "Video display: (%d, %d) %dx%d -> (%d, %d) %dx%d\n",
               src.x0, src.y0, sw, sh, dst.x0, dst.y0, dw, dh);
    MP_VERBOSE(&vo->vo_log, "Video scale: %f/%f\n",
               (double)dw / sw, (double)dh / sh);
    MP_VERBOSE(&vo->vo_log, "OSD borders: l=%d t=%d r=%d b=%d\n",
               osd.ml, osd.mt, osd.mr, osd.mb);
    MP_VERBOSE(&vo->vo_log, "Video borders: l=%d t=%d r=%d b=%d\n",
               dst.x0, dst.y0, vo->dwidth - dst.x1, vo->dheight - dst.y1);
}

// Clamp [start, end) to range [0, size) with various fallbacks.
static void clamp_size(int size, int *start, int *end)
{
    *start = FFMAX(0, *start);
    *end = FFMIN(size, *end);
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

// Calculate the appropriate source and destination rectangle to
// get a correctly scaled picture, including pan-scan.
// out_src: visible part of the video
// out_dst: area of screen covered by the video source rectangle
// out_osd: OSD size, OSD margins, etc.
void vo_get_src_dst_rects(struct vo *vo, struct mp_rect *out_src,
                          struct mp_rect *out_dst, struct mp_osd_res *out_osd)
{
    struct mp_vo_opts *opts = vo->opts;
    int src_w = vo->aspdat.orgw;
    int src_h = vo->aspdat.orgh;
    struct mp_rect dst = {0, 0, vo->dwidth, vo->dheight};
    struct mp_rect src = {0, 0, src_w,      src_h};
    struct mp_osd_res osd = {
        .w = vo->dwidth,
        .h = vo->dheight,
        .display_par = vo->monitor_par,
    };
    if (opts->keepaspect) {
        int scaled_width, scaled_height;
        aspect_calc_panscan(vo, &scaled_width, &scaled_height);
        src_dst_split_scaling(src_w, vo->dwidth, scaled_width, opts->unscaled,
                              opts->zoom, opts->align_x, opts->pan_x,
                              &src.x0, &src.x1, &dst.x0, &dst.x1,
                              &osd.ml, &osd.mr);
        src_dst_split_scaling(src_h, vo->dheight, scaled_height, opts->unscaled,
                              opts->zoom, opts->align_y, opts->pan_y,
                              &src.y0, &src.y1, &dst.y0, &dst.y1,
                              &osd.mt, &osd.mb);
    }

    *out_src = src;
    *out_dst = dst;
    *out_osd = osd;

    print_video_rect(vo, src, dst, osd);
}
