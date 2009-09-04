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

#ifndef MPLAYER_ASPECT_H
#define MPLAYER_ASPECT_H
/* Stuff for correct aspect scaling. */

struct vo;
void panscan_init(struct vo *vo);
void panscan_calc(struct vo *vo);
void panscan_calc_windowed(struct vo *vo);

void aspect_save_orig(struct vo *vo, int orgw, int orgh);

void aspect_save_prescale(struct vo *vo, int prew, int preh);

void aspect_save_screenres(struct vo *vo, int scrw, int scrh);

#define A_WINZOOM 2 ///< zoom to fill window size
#define A_ZOOM 1
#define A_NOZOOM 0

void aspect(struct vo *vo, int *srcw, int *srch, int zoom);
void aspect_fit(struct vo *vo, int *srcw, int *srch, int fitw, int fith);


#ifdef IS_OLD_VO
#define vo_panscan_x global_vo->panscan_x
#define vo_panscan_y global_vo->panscan_y
#define vo_panscan_amount global_vo->panscan_amount
#define monitor_aspect global_vo->monitor_aspect

#define panscan_init() panscan_init(global_vo)
#define panscan_calc() panscan_calc(global_vo)
#define panscan_calc_windowed() panscan_calc_windowed(global_vo)
#define aspect_save_orig(...) aspect_save_orig(global_vo, __VA_ARGS__)
#define aspect_save_prescale(...) aspect_save_prescale(global_vo, __VA_ARGS__)
#define aspect_save_screenres(...) aspect_save_screenres(global_vo, __VA_ARGS__)
#define aspect(...) aspect(global_vo, __VA_ARGS__)
#endif

#endif /* MPLAYER_ASPECT_H */
