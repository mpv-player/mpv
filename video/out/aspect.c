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

/* Stuff for correct aspect scaling. */
#include "aspect.h"
#include "geometry.h"
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
    struct MPOpts *opts = vo->opts;
    if (scrw <= 0 && scrh <= 0)
        scrw = 1024;
    if (scrh <= 0)
        scrh = (scrw * 3 + 3) / 4;
    if (scrw <= 0)
        scrw = (scrh * 4 + 2) / 3;
    vo->aspdat.scrw = scrw;
    vo->aspdat.scrh = scrh;
    if (opts->force_monitor_aspect)
        vo->monitor_par = opts->force_monitor_aspect * scrh / scrw;
    else
        vo->monitor_par = 1.0 / opts->monitor_pixel_aspect;
}

/* aspect is called with the source resolution and the
 * resolution, that the scaled image should fit into
 */

void aspect_fit(struct vo *vo, int *srcw, int *srch, int fitw, int fith)
{
    struct aspect_data *aspdat = &vo->aspdat;
    float pixelaspect = vo->monitor_par;

    mp_msg(MSGT_VO, MSGL_DBG2, "aspect(0) fitin: %dx%d monitor_par: %.2f\n",
           fitw, fith, vo->monitor_par);
    *srcw = fitw;
    *srch = (float)fitw / aspdat->prew * aspdat->preh / pixelaspect;
    *srch += *srch % 2; // round
    mp_msg(MSGT_VO, MSGL_DBG2, "aspect(1) wh: %dx%d (org: %dx%d)\n",
           *srcw, *srch, aspdat->prew, aspdat->preh);
    if (*srch > fith || *srch < aspdat->orgh) {
        int tmpw = (float)fith / aspdat->preh * aspdat->prew * pixelaspect;
        tmpw += tmpw % 2; // round
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

static void get_max_dims(struct vo *vo, int *w, int *h, int zoom)
{
    struct aspect_data *aspdat = &vo->aspdat;
    *w = zoom ? aspdat->scrw : aspdat->prew;
    *h = zoom ? aspdat->scrh : aspdat->preh;
    if (zoom && WinID >= 0)
        zoom = A_WINZOOM;
    if (zoom == A_WINZOOM) {
        *w = vo->dwidth;
        *h = vo->dheight;
    }
}

void aspect(struct vo *vo, int *srcw, int *srch, int zoom)
{
    int fitw;
    int fith;
    get_max_dims(vo, &fitw, &fith, zoom);
    if (!zoom && geometry_wh_changed) {
        mp_msg(MSGT_VO, MSGL_DBG2, "aspect(0) no aspect forced!\n");
        return; // the user doesn't want to fix aspect
    }
    aspect_fit(vo, srcw, srch, fitw, fith);
}

void panscan_init(struct vo *vo)
{
    vo->panscan_x = 0;
    vo->panscan_y = 0;
    vo->panscan_amount = 0.0f;
}

static void panscan_calc_internal(struct vo *vo, int zoom)
{
    int fwidth, fheight;
    int vo_panscan_area;
    int max_w, max_h;
    get_max_dims(vo, &max_w, &max_h, zoom);
    struct MPOpts *opts = vo->opts;

    if (opts->vo_panscanrange > 0) {
        aspect(vo, &fwidth, &fheight, zoom);
        vo_panscan_area = max_h - fheight;
        if (!vo_panscan_area)
            vo_panscan_area = max_w - fwidth;
        vo_panscan_area *= opts->vo_panscanrange;
    } else
        vo_panscan_area = -opts->vo_panscanrange * max_h;

    vo->panscan_amount = vo_fs || zoom == A_WINZOOM ? vo_panscan : 0;
    vo->panscan_x = vo_panscan_area * vo->panscan_amount * vo->aspdat.asp;
    vo->panscan_y = vo_panscan_area * vo->panscan_amount;
}

/**
 * vos that set vo_dwidth and v_dheight correctly should call this to update
 * vo_panscan_x and vo_panscan_y
 */
void panscan_calc_windowed(struct vo *vo)
{
    panscan_calc_internal(vo, A_WINZOOM);
}
