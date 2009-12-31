/*
 * Common code related to colorspaces and conversion
 *
 * Copyleft (C) 2009 Reimar Döffinger <Reimar.Doeffinger@gmx.de>
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

#include <stdint.h>
#include <math.h>
#include "libavutil/common.h"
#include "csputils.h"

/**
 * \brief little helper function to create a lookup table for gamma
 * \param map buffer to create map into
 * \param size size of buffer
 * \param gamma gamma value
 */
void mp_gen_gamma_map(uint8_t *map, int size, float gamma) {
  int i;
  if (gamma == 1.0) {
    for (i = 0; i < size; i++)
      map[i] = 255 * i / (size - 1);
    return;
  }
  gamma = 1.0 / gamma;
  for (i = 0; i < size; i++) {
    float tmp = (float)i / (size - 1.0);
    tmp = pow(tmp, gamma);
    if (tmp > 1.0) tmp = 1.0;
    if (tmp < 0.0) tmp = 0.0;
    map[i] = 255 * tmp;
  }
}

/**
 * \brief get the coefficients of the yuv -> rgb conversion matrix
 * \param params struct specifying the properties of the conversion like brightness, ...
 * \param yuv2rgb array to store coefficients into
 */
void mp_get_yuv2rgb_coeffs(struct mp_csp_params *params, float yuv2rgb[3][4]) {
  float uvcos = params->saturation * cos(params->hue);
  float uvsin = params->saturation * sin(params->hue);
  int i;
  float uv_coeffs[3][2] = {
    { 0.000,  1.596},
    {-0.391, -0.813},
    { 2.018,  0.000}
  };
  for (i = 0; i < 3; i++) {
    yuv2rgb[i][COL_C]  = params->brightness;
    yuv2rgb[i][COL_Y]  = 1.164 * params->contrast;
    yuv2rgb[i][COL_C] += (-16 / 255.0) * yuv2rgb[i][COL_Y];
    yuv2rgb[i][COL_U]  = uv_coeffs[i][0] * uvcos + uv_coeffs[i][1] * uvsin;
    yuv2rgb[i][COL_C] += (-128 / 255.0) * yuv2rgb[i][COL_U];
    yuv2rgb[i][COL_V]  = uv_coeffs[i][0] * uvsin + uv_coeffs[i][1] * uvcos;
    yuv2rgb[i][COL_C] += (-128 / 255.0) * yuv2rgb[i][COL_V];
    // this "centers" contrast control so that e.g. a contrast of 0
    // leads to a grey image, not a black one
    yuv2rgb[i][COL_C] += 0.5 - params->contrast / 2.0;
  }
}

//! size of gamma map use to avoid slow exp function in gen_yuv2rgb_map
#define GMAP_SIZE (1024)
/**
 * \brief generate a 3D YUV -> RGB map
 * \param params struct containing parameters like brightness, gamma, ...
 * \param map where to store map. Must provide space for (size + 2)^3 elements
 * \param size size of the map, excluding border
 */
void mp_gen_yuv2rgb_map(struct mp_csp_params *params, unsigned char *map, int size) {
  int i, j, k, l;
  float step = 1.0 / size;
  float y, u, v;
  float yuv2rgb[3][4];
  unsigned char gmaps[3][GMAP_SIZE];
  mp_gen_gamma_map(gmaps[0], GMAP_SIZE, params->rgamma);
  mp_gen_gamma_map(gmaps[1], GMAP_SIZE, params->ggamma);
  mp_gen_gamma_map(gmaps[2], GMAP_SIZE, params->bgamma);
  mp_get_yuv2rgb_coeffs(params, yuv2rgb);
  for (i = 0; i < 3; i++)
    for (j = 0; j < 4; j++)
      yuv2rgb[i][j] *= GMAP_SIZE - 1;
  v = 0;
  for (i = -1; i <= size; i++) {
    u = 0;
    for (j = -1; j <= size; j++) {
      y = 0;
      for (k = -1; k <= size; k++) {
        for (l = 0; l < 3; l++) {
          float rgb = yuv2rgb[l][COL_Y] * y + yuv2rgb[l][COL_U] * u + yuv2rgb[l][COL_V] * v + yuv2rgb[l][COL_C];
          *map++ = gmaps[l][av_clip(rgb, 0, GMAP_SIZE - 1)];
        }
        y += (k == -1 || k == size - 1) ? step / 2 : step;
      }
      u += (j == -1 || j == size - 1) ? step / 2 : step;
    }
    v += (i == -1 || i == size - 1) ? step / 2 : step;
  }
}
