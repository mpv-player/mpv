/*
 * generic alpha renderers for all YUV modes and RGB depths
 * These are "reference implementations", should be optimized later (MMX, etc).
 *
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

#ifndef MPLAYER_OSD_H
#define MPLAYER_OSD_H

void vo_draw_alpha_init(void); // build tables

void vo_draw_alpha_yv12(int w,  int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase, int dststride);
void vo_draw_alpha_yuy2(int w,  int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase, int dststride);
void vo_draw_alpha_uyvy(int w,  int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase, int dststride);
void vo_draw_alpha_rgb24(int w, int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase, int dststride);
void vo_draw_alpha_rgb32(int w, int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase, int dststride);
void vo_draw_alpha_rgb12(int w, int h, unsigned char* src, unsigned char *srca,
                         int srcstride, unsigned char* dstbase, int dststride);
void vo_draw_alpha_rgb15(int w, int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase, int dststride);
void vo_draw_alpha_rgb16(int w, int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase, int dststride);

#endif /* MPLAYER_OSD_H */
