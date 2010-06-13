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
 *
 * You can alternatively redistribute this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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
 *
 * Note: contrast, hue and saturation will only work as expected with YUV formats,
 * not with e.g. MP_CSP_XYZ
 */
void mp_get_yuv2rgb_coeffs(struct mp_csp_params *params, float yuv2rgb[3][4]) {
  float uvcos = params->saturation * cos(params->hue);
  float uvsin = params->saturation * sin(params->hue);
  int format = params->format;
  int levelconv = params->levelconv;
  int i;
  const float (*uv_coeffs)[3];
  const float *level_adjust;
  static const float yuv_level_adjust[MP_CSP_LEVELCONV_COUNT][4] = {
    {-16 / 255.0, -128 / 255.0, -128 / 255.0, 1.164},
    { 16 / 255.0 * 1.164, -128 / 255.0, -128 / 255.0, 1.0/1.164},
    { 0, -128 / 255.0, -128 / 255.0, 1},
  };
  static const float xyz_level_adjust[4] = {0, 0, 0, 0};
  static const float uv_coeffs_table[MP_CSP_COUNT][3][3] = {
    [MP_CSP_DEFAULT] = {
      {1,  0.000,  1.596},
      {1, -0.391, -0.813},
      {1,  2.018,  0.000}
    },
    [MP_CSP_BT_601] = {
      {1,  0.000,  1.403},
      {1, -0.344, -0.714},
      {1,  1.773,  0.000}
    },
    [MP_CSP_BT_709] = {
      {1,  0.0000,  1.5701},
      {1, -0.1870, -0.4664},
      {1,  1.8556,  0.0000}
    },
    [MP_CSP_SMPTE_240M] = {
      {1,  0.0000,  1.5756},
      {1, -0.2253, -0.5000},
      {1,  1.8270,  0.0000}
    },
    [MP_CSP_EBU] = {
      {1,  0.000,  1.140},
      {1, -0.396, -0.581},
      {1,  2.029,  0.000}
    },
    [MP_CSP_XYZ] = {
      { 3.2404542, -1.5371385, -0.4985314},
      {-0.9692660,  1.8760108,  0.0415560},
      { 0.0556434, -0.2040259,  1.0572252}
    },
  };

  if (format < 0 || format >= MP_CSP_COUNT)
    format = MP_CSP_DEFAULT;
  uv_coeffs = uv_coeffs_table[format];
  if (levelconv < 0 || levelconv >= MP_CSP_LEVELCONV_COUNT)
    levelconv = MP_CSP_LEVELCONV_TV_TO_PC;
  level_adjust = yuv_level_adjust[levelconv];
  if (format == MP_CSP_XYZ)
    level_adjust = xyz_level_adjust;

  for (i = 0; i < 3; i++) {
    yuv2rgb[i][COL_C]  = params->brightness;
    yuv2rgb[i][COL_Y]  = uv_coeffs[i][COL_Y] * level_adjust[COL_C] * params->contrast;
    yuv2rgb[i][COL_C] += level_adjust[COL_Y] * yuv2rgb[i][COL_Y];
    yuv2rgb[i][COL_U]  = uv_coeffs[i][COL_U] * uvcos + uv_coeffs[i][COL_V] * uvsin;
    yuv2rgb[i][COL_C] += level_adjust[COL_U] * yuv2rgb[i][COL_U];
    yuv2rgb[i][COL_V]  = uv_coeffs[i][COL_U] * uvsin + uv_coeffs[i][COL_V] * uvcos;
    yuv2rgb[i][COL_C] += level_adjust[COL_V] * yuv2rgb[i][COL_V];
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
