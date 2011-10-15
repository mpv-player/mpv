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
#include <assert.h>
#include <libavutil/common.h>

#include "csputils.h"

char * const mp_csp_names[MP_CSP_COUNT] = {
    "Autoselect",
    "BT.601 (SD)",
    "BT.709 (HD)",
    "SMPTE-240M",
};

char * const mp_csp_equalizer_names[MP_CSP_EQ_COUNT] = {
    "brightness",
    "contrast",
    "hue",
    "saturation",
    "gamma",
};


enum mp_csp mp_csp_guess_colorspace(int width, int height)
{
    return width >= 1280 || height > 576 ? MP_CSP_BT_709 : MP_CSP_BT_601;
}

/**
 * \brief little helper function to create a lookup table for gamma
 * \param map buffer to create map into
 * \param size size of buffer
 * \param gamma gamma value
 */
void mp_gen_gamma_map(uint8_t *map, int size, float gamma)
{
    if (gamma == 1.0) {
        for (int i = 0; i < size; i++)
            map[i] = 255 * i / (size - 1);
        return;
    }
    gamma = 1.0 / gamma;
    for (int i = 0; i < size; i++) {
        float tmp = (float)i / (size - 1.0);
        tmp = pow(tmp, gamma);
        if (tmp > 1.0)
            tmp = 1.0;
        if (tmp < 0.0)
            tmp = 0.0;
        map[i] = 255 * tmp;
    }
}

/* Fill in the Y, U, V vectors of a yuv2rgb conversion matrix
 * based on the given luma weights of the R, G and B components (lr, lg, lb).
 * lr+lg+lb is assumed to equal 1.
 * This function is meant for colorspaces satisfying the following
 * conditions (which are true for common YUV colorspaces):
 * - The mapping from input [Y, U, V] to output [R, G, B] is linear.
 * - Y is the vector [1, 1, 1].  (meaning input Y component maps to 1R+1G+1B)
 * - U maps to a value with zero R and positive B ([0, x, y], y > 0;
 *   i.e. blue and green only).
 * - V maps to a value with zero B and positive R ([x, y, 0], x > 0;
 *   i.e. red and green only).
 * - U and V are orthogonal to the luma vector [lr, lg, lb].
 * - The magnitudes of the vectors U and V are the minimal ones for which
 *   the image of the set Y=[0...1],U=[-0.5...0.5],V=[-0.5...0.5] under the
 *   conversion function will cover the set R=[0...1],G=[0...1],B=[0...1]
 *   (the resulting matrix can be converted for other input/output ranges
 *   outside this function).
 * Under these conditions the given parameters lr, lg, lb uniquely
 * determine the mapping of Y, U, V to R, G, B.
 */
static void luma_coeffs(float m[3][4], float lr, float lg, float lb)
{
    assert(fabs(lr+lg+lb - 1) < 1e-6);
    m[0][0] = m[1][0] = m[2][0] = 1;
    m[0][1] = 0;
    m[1][1] = -2 * (1-lb) * lb/lg;
    m[2][1] = 2 * (1-lb);
    m[0][2] = 2 * (1-lr);
    m[1][2] = -2 * (1-lr) * lr/lg;
    m[2][2] = 0;
    // Constant coefficients (m[x][3]) not set here
}

/**
 * \brief get the coefficients of the yuv -> rgb conversion matrix
 * \param params struct specifying the properties of the conversion like
 *  brightness, ...
 * \param m array to store coefficients into
 */
void mp_get_yuv2rgb_coeffs(struct mp_csp_params *params, float m[3][4])
{
    int format = params->colorspace.format;
    if (format <= MP_CSP_AUTO || format >= MP_CSP_COUNT)
        format = MP_CSP_BT_601;
    switch (format) {
    case MP_CSP_BT_601:     luma_coeffs(m, 0.299,  0.587,  0.114 ); break;
    case MP_CSP_BT_709:     luma_coeffs(m, 0.2126, 0.7152, 0.0722); break;
    case MP_CSP_SMPTE_240M: luma_coeffs(m, 0.2122, 0.7013, 0.0865); break;
    default:
        abort();
    };

    // Hue is equivalent to rotating input [U, V] subvector around the origin.
    // Saturation scales [U, V].
    float huecos = params->saturation * cos(params->hue);
    float huesin = params->saturation * sin(params->hue);
    for (int i = 0; i < 3; i++) {
        float u = m[i][COL_U];
        m[i][COL_U] = huecos * u - huesin * m[i][COL_V];
        m[i][COL_V] = huesin * u + huecos * m[i][COL_V];
    }

    int levels_in = params->colorspace.levels_in;
    if (levels_in <= MP_CSP_LEVELS_AUTO || levels_in >= MP_CSP_LEVELS_COUNT)
        levels_in = MP_CSP_LEVELS_TV;
    // The values below are written in 0-255 scale
    struct yuvlevels { double ymin, ymax, cmin, cmid; }
        yuvlim =  { 16, 235, 16, 128 },
        yuvfull = {  0, 255,  1, 128 },  // '1' to make it symmetric around 128
        yuvlev;
    switch (levels_in) {
    case MP_CSP_LEVELS_TV: yuvlev = yuvlim; break;
    case MP_CSP_LEVELS_PC: yuvlev = yuvfull; break;
    default:
        abort();
    }

    int levels_out = params->colorspace.levels_out;
    if (levels_out <= MP_CSP_LEVELS_AUTO || levels_out >= MP_CSP_LEVELS_COUNT)
        levels_out = MP_CSP_LEVELS_PC;
    struct rgblevels { double min, max; }
        rgblim =  { 16, 235 },
        rgbfull = {  0, 255 },
        rgblev;
    switch (levels_out) {
    case MP_CSP_LEVELS_TV: rgblev = rgblim; break;
    case MP_CSP_LEVELS_PC: rgblev = rgbfull; break;
    default:
        abort();
    }

    double ymul = (rgblev.max - rgblev.min) / (yuvlev.ymax - yuvlev.ymin);
    double cmul = (rgblev.max - rgblev.min) / (yuvlev.cmid - yuvlev.cmin) / 2;
    for (int i = 0; i < 3; i++) {
        m[i][COL_Y] *= ymul;
        m[i][COL_U] *= cmul;
        m[i][COL_V] *= cmul;
        // Set COL_C so that Y=umin,UV=cmid maps to RGB=min (black to black)
        m[i][COL_C] = (rgblev.min - m[i][COL_Y] * yuvlev.ymin
                       -(m[i][COL_U] + m[i][COL_V]) * yuvlev.cmid) / 255;
    }

    // Brightness adds a constant to output R,G,B.
    // Contrast scales Y around 1/2 (not 0 in this implementation).
    for (int i = 0; i < 3; i++) {
        m[i][COL_C] += params->brightness;
        m[i][COL_Y] *= params->contrast;
        m[i][COL_C] += (rgblev.max-rgblev.min)/255 * (1 - params->contrast)/2;
    }

    float depth_multiplier = params->input_shift >= 0 ?
                             (1 << params->input_shift) :
                             (1.0 / (1 << -params->input_shift));
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            m[i][j] *= depth_multiplier;
}

//! size of gamma map use to avoid slow exp function in gen_yuv2rgb_map
#define GMAP_SIZE (1024)
/**
 * \brief generate a 3D YUV -> RGB map
 * \param params struct containing parameters like brightness, gamma, ...
 * \param map where to store map. Must provide space for (size + 2)^3 elements
 * \param size size of the map, excluding border
 */
void mp_gen_yuv2rgb_map(struct mp_csp_params *params, unsigned char *map, int size)
{
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
                    float rgb = yuv2rgb[l][COL_Y] * y + yuv2rgb[l][COL_U] * u +
                                yuv2rgb[l][COL_V] * v + yuv2rgb[l][COL_C];
                    *map++ = gmaps[l][av_clip(rgb, 0, GMAP_SIZE - 1)];
                }
                y += (k == -1 || k == size - 1) ? step / 2 : step;
            }
            u += (j == -1 || j == size - 1) ? step / 2 : step;
        }
        v += (i == -1 || i == size - 1) ? step / 2 : step;
    }
}

// Copy settings from eq into params.
void mp_csp_copy_equalizer_values(struct mp_csp_params *params,
                                  const struct mp_csp_equalizer *eq)
{
    params->brightness = eq->values[MP_CSP_EQ_BRIGHTNESS] / 100.0;
    params->contrast = (eq->values[MP_CSP_EQ_CONTRAST] + 100) / 100.0;
    params->hue = eq->values[MP_CSP_EQ_HUE] / 100.0 * 3.1415927;
    params->saturation = (eq->values[MP_CSP_EQ_SATURATION] + 100) / 100.0;
    float gamma = exp(log(8.0) * eq->values[MP_CSP_EQ_GAMMA] / 100.0);
    params->rgamma = gamma;
    params->ggamma = gamma;
    params->bgamma = gamma;
}

static int find_eq(int capabilities, const char *name)
{
    for (int i = 0; i < MP_CSP_EQ_COUNT; i++) {
        if (strcmp(name, mp_csp_equalizer_names[i]) == 0)
            return ((1 << i) & capabilities) ? i : -1;
    }
    return -1;
}

int mp_csp_equalizer_get(struct mp_csp_equalizer *eq, const char *property,
                         int *out_value)
{
    int index = find_eq(eq->capabilities, property);
    if (index < 0)
        return -1;

    *out_value = eq->values[index];

    return 0;
}

int mp_csp_equalizer_set(struct mp_csp_equalizer *eq, const char *property,
                         int value)
{
    int index = find_eq(eq->capabilities, property);
    if (index < 0)
        return 0;

    eq->values[index] = value;

    return 1;
}
