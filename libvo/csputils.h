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

#ifndef MPLAYER_CSPUTILS_H
#define MPLAYER_CSPUTILS_H

#include <stdint.h>

struct mp_csp_params {
  float brightness;
  float contrast;
  float hue;
  float saturation;
  float rgamma;
  float ggamma;
  float bgamma;
};

void mp_gen_gamma_map(unsigned char *map, int size, float gamma);
#define ROW_R 0
#define ROW_G 1
#define ROW_B 2
#define COL_Y 0
#define COL_U 1
#define COL_V 2
#define COL_C 3
void mp_get_yuv2rgb_coeffs(struct mp_csp_params *params, float yuv2rgb[3][4]);
void mp_gen_yuv2rgb_map(struct mp_csp_params *params, uint8_t *map, int size);

#endif /* MPLAYER_CSPUTILS_H */
