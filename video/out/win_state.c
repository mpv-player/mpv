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

#include "win_state.h"
#include "vo.h"

#include "video/mp_image.h"

static void calc_monitor_aspect(struct mp_vo_opts *opts, int scr_w, int scr_h,
                                double *pixelaspect, int *w, int *h)
{
    *pixelaspect = 1.0 / opts->monitor_pixel_aspect;

    if (scr_w > 0 && scr_h > 0 && opts->force_monitor_aspect)
        *pixelaspect = 1.0 / (opts->force_monitor_aspect * scr_h / scr_w);

    if (*pixelaspect < 1) {
        *h /= *pixelaspect;
    } else {
        *w *= *pixelaspect;
    }
}

// Fit *w/*h into the size specified by geo.
static void apply_autofit(int *w, int *h, int scr_w, int scr_h,
                          struct m_geometry *geo, bool allow_up, bool allow_down)
{
    if (!geo->wh_valid)
        return;

    int dummy = 0;
    int n_w = *w, n_h = *h;
    m_geometry_apply(&dummy, &dummy, &n_w, &n_h, scr_w, scr_h, geo);

    if (!allow_up && *w <= n_w && *h <= n_h)
        return;
    if (!allow_down && *w >= n_w && *h >= n_h)
        return;

    // If aspect mismatches, always make the window smaller than the fit box
    // (Or larger, if allow_down==false.)
    double asp = (double)*w / *h;
    double n_asp = (double)n_w / n_h;
    if ((n_asp <= asp) == allow_down) {
        *w = n_w;
        *h = n_w / asp;
    } else {
        *w = n_h * asp;
        *h = n_h;
    }
}

// Compute the "suggested" window size and position and return it in *out_geo.
// screen is the bounding box of the current screen within the virtual desktop.
// Does not change *vo.
//  screen: position of the area on virtual desktop on which the video-content
//          should be placed (maybe after excluding decorations, taskbars, etc)
//  monitor: position of the monitor on virtual desktop (used for pixelaspect).
//  dpi_scale: the DPI multiplier to get from virtual to real coordinates
//             (>1 for "hidpi")
// Use vo_apply_window_geometry() to copy the result into the vo.
// NOTE: currently, all windowing backends do their own handling of window
//       geometry additional to this code. This is to deal with initial window
//       placement, fullscreen handling, avoiding resize on reconfig() with no
//       size change, multi-monitor stuff, and possibly more.
void vo_calc_window_geometry3(struct vo *vo, const struct mp_rect *screen,
                              const struct mp_rect *monitor,
                              double dpi_scale, struct vo_win_geometry *out_geo)
{
    struct mp_vo_opts *opts = vo->opts;

    *out_geo = (struct vo_win_geometry){0};

    // The case of calling this function even though no video was configured
    // yet (i.e. vo->params==NULL) happens when vo_gpu creates a hidden window
    // in order to create a rendering context.
    struct mp_image_params params = { .w = 320, .h = 200 };
    if (vo->params)
        params = *vo->params;

    if (!opts->hidpi_window_scale)
        dpi_scale = 1;

    int d_w, d_h;
    mp_image_params_get_dsize(&params, &d_w, &d_h);
    if ((vo->driver->caps & VO_CAP_ROTATE90) && params.rotate % 180 == 90)
        MPSWAP(int, d_w, d_h);
    d_w = MPCLAMP(d_w * opts->window_scale * dpi_scale, 1, 16000);
    d_h = MPCLAMP(d_h * opts->window_scale * dpi_scale, 1, 16000);

    int scr_w = screen->x1 - screen->x0;
    int scr_h = screen->y1 - screen->y0;

    int mon_w = monitor->x1 - monitor->x0;
    int mon_h = monitor->y1 - monitor->y0;

    MP_DBG(vo, "max content size: %dx%d\n", scr_w, scr_h);
    MP_DBG(vo, "monitor size: %dx%d\n", mon_w, mon_h);

    calc_monitor_aspect(opts, mon_w, mon_h, &out_geo->monitor_par, &d_w, &d_h);

    apply_autofit(&d_w, &d_h, scr_w, scr_h, &opts->autofit, true, true);
    apply_autofit(&d_w, &d_h, scr_w, scr_h, &opts->autofit_smaller, true, false);
    apply_autofit(&d_w, &d_h, scr_w, scr_h, &opts->autofit_larger, false, true);

    out_geo->win.x0 = (int)(scr_w - d_w) / 2;
    out_geo->win.y0 = (int)(scr_h - d_h) / 2;
    m_geometry_apply(&out_geo->win.x0, &out_geo->win.y0, &d_w, &d_h,
                     scr_w, scr_h, &opts->geometry);

    out_geo->win.x0 += screen->x0;
    out_geo->win.y0 += screen->y0;
    out_geo->win.x1 = out_geo->win.x0 + d_w;
    out_geo->win.y1 = out_geo->win.y0 + d_h;

    if (opts->geometry.xy_valid || opts->force_window_position)
        out_geo->flags |= VO_WIN_FORCE_POS;
}

// same as vo_calc_window_geometry3 with monitor assumed same as screen
void vo_calc_window_geometry2(struct vo *vo, const struct mp_rect *screen,
                              double dpi_scale, struct vo_win_geometry *out_geo)
{
    vo_calc_window_geometry3(vo, screen, screen, dpi_scale, out_geo);
}

void vo_calc_window_geometry(struct vo *vo, const struct mp_rect *screen,
                             struct vo_win_geometry *out_geo)
{
    vo_calc_window_geometry2(vo, screen, 1.0, out_geo);
}

// Copy the parameters in *geo to the vo fields.
// (Doesn't do anything else - windowing backends should trigger VO_EVENT_RESIZE
//  to ensure that the VO reinitializes rendering properly.)
void vo_apply_window_geometry(struct vo *vo, const struct vo_win_geometry *geo)
{
    vo->dwidth = geo->win.x1 - geo->win.x0;
    vo->dheight = geo->win.y1 - geo->win.y0;
    vo->monitor_par = geo->monitor_par;
}
