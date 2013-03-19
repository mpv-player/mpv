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
#include "core/mp_msg.h"
#include "core/options.h"

#include "vo.h"

void aspect_save_videores(struct vo *vo, int w, int h, int d_w, int d_h)
{
    vo->aspdat.orgw = w;
    vo->aspdat.orgh = h;
    vo->aspdat.prew = d_w;
    vo->aspdat.preh = d_h;
    vo->aspdat.par = (double)d_w / d_h * h / w;
}

void aspect_save_screenres(struct vo *vo, int scrw, int scrh)
{
    mp_msg(MSGT_VO, MSGL_DBG2, "aspect_save_screenres %dx%d\n", scrw, scrh);
    struct mp_vo_opts *opts = vo->opts;
    if (scrw <= 0 && scrh <= 0)
        scrw = 1024;
    if (scrh <= 0)
        scrh = (scrw * 3 + 3) / 4;
    if (scrw <= 0)
        scrw = (scrh * 4 + 2) / 3;
    if (opts->force_monitor_aspect)
        vo->aspdat.monitor_par = opts->force_monitor_aspect * scrh / scrw;
    else
        vo->aspdat.monitor_par = 1.0 / opts->monitor_pixel_aspect;
}

void aspect_calc_monitor(struct vo *vo, int *w, int *h)
{
    float pixelaspect = vo->aspdat.monitor_par;

    if (pixelaspect < 1) {
        *h /= pixelaspect;
    } else {
        *w *= pixelaspect;
    }
}

static void aspect_calc(struct vo *vo, int *srcw, int *srch)
{
    struct aspect_data *aspdat = &vo->aspdat;
    float pixelaspect = aspdat->monitor_par;

    int fitw = FFMAX(1, vo->dwidth);
    int fith = FFMAX(1, vo->dheight);

    mp_msg(MSGT_VO, MSGL_DBG2, "aspect(0) fitin: %dx%d monitor_par: %.2f\n",
           fitw, fith, aspdat->monitor_par);
    *srcw = fitw;
    *srch = (float)fitw / aspdat->prew * aspdat->preh / pixelaspect;
    mp_msg(MSGT_VO, MSGL_DBG2, "aspect(1) wh: %dx%d (org: %dx%d)\n",
           *srcw, *srch, aspdat->prew, aspdat->preh);
    if (*srch > fith || *srch < aspdat->orgh) {
        int tmpw = (float)fith / aspdat->preh * aspdat->prew * pixelaspect;
        if (tmpw <= fitw) {
            *srch = fith;
            *srcw = tmpw;
        } else if (*srch > fith) {
            mp_tmsg(MSGT_VO, MSGL_WARN,
                    "[ASPECT] Warning: No suitable new res found!\n");
        }
    }
    aspdat->asp = *srcw / (float)*srch;
    mp_msg(MSGT_VO, MSGL_DBG2, "aspect(2) wh: %dx%d (org: %dx%d)\n",
           *srcw, *srch, aspdat->prew, aspdat->preh);
}

void aspect_calc_panscan(struct vo *vo, int *out_w, int *out_h)
{
    struct mp_vo_opts *opts = vo->opts;
    int fwidth, fheight;
    aspect_calc(vo, &fwidth, &fheight);

    int vo_panscan_area;
    if (opts->panscanrange > 0) {
        vo_panscan_area = vo->dheight - fheight;
        if (!vo_panscan_area)
            vo_panscan_area = vo->dwidth - fwidth;
        vo_panscan_area *= opts->panscanrange;
    } else
        vo_panscan_area = -opts->panscanrange * vo->dheight;

    *out_w = fwidth + vo_panscan_area * opts->panscan * vo->aspdat.asp;
    *out_h = fheight + vo_panscan_area * opts->panscan;
}
