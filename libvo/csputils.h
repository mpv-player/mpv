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
 *
 * You can alternatively redistribute this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef MPLAYER_CSPUTILS_H
#define MPLAYER_CSPUTILS_H

#include <stdint.h>


/* NOTE: the csp and levels AUTO values are converted to specific ones
 * above vf/vo level. At least vf_scale relies on all valid settings being
 * nonzero at vf/vo level.
 */

enum mp_csp {
    MP_CSP_AUTO,
    MP_CSP_BT_601,
    MP_CSP_BT_709,
    MP_CSP_SMPTE_240M,
    MP_CSP_COUNT
};

// Any enum mp_csp value is a valid index (except MP_CSP_COUNT)
extern char * const mp_csp_names[MP_CSP_COUNT];

enum mp_csp_levels {
    MP_CSP_LEVELS_AUTO,
    MP_CSP_LEVELS_TV,
    MP_CSP_LEVELS_PC,
    MP_CSP_LEVELS_COUNT,
};

struct mp_csp_details {
    enum mp_csp format;
    enum mp_csp_levels levels_in;      // encoded video
    enum mp_csp_levels levels_out;     // output device
};

// initializer for struct mp_csp_details that contains reasonable defaults
#define MP_CSP_DETAILS_DEFAULTS {MP_CSP_BT_601, MP_CSP_LEVELS_TV, MP_CSP_LEVELS_PC}

struct mp_csp_params {
    struct mp_csp_details colorspace;
    float brightness;
    float contrast;
    float hue;
    float saturation;
    float rgamma;
    float ggamma;
    float bgamma;
    int input_shift;
};

enum mp_csp_equalizer_param {
    MP_CSP_EQ_BRIGHTNESS,
    MP_CSP_EQ_CONTRAST,
    MP_CSP_EQ_HUE,
    MP_CSP_EQ_SATURATION,
    MP_CSP_EQ_GAMMA,
    MP_CSP_EQ_COUNT,
};

#define MP_CSP_EQ_CAPS_COLORMATRIX \
    ( (1 << MP_CSP_EQ_BRIGHTNESS) \
    | (1 << MP_CSP_EQ_CONTRAST) \
    | (1 << MP_CSP_EQ_HUE) \
    | (1 << MP_CSP_EQ_SATURATION) )

#define MP_CSP_EQ_CAPS_GAMMA (1 << MP_CSP_EQ_GAMMA)

extern char * const mp_csp_equalizer_names[MP_CSP_EQ_COUNT];

// Default initialization with 0 is enough, except for the capabilities field
struct mp_csp_equalizer {
    // Bit field of capabilities. For example (1 << MP_CSP_EQ_HUE) means hue
    // support is available.
    int capabilities;
    // Value for each property is in the range [-100, 100].
    // 0 is default, meaning neutral or no change.
    int values[MP_CSP_EQ_COUNT];
};


void mp_csp_copy_equalizer_values(struct mp_csp_params *params,
                                  const struct mp_csp_equalizer *eq);

int mp_csp_equalizer_set(struct mp_csp_equalizer *eq, const char *property,
                         int value);

int mp_csp_equalizer_get(struct mp_csp_equalizer *eq, const char *property,
                         int *out_value);

enum mp_csp mp_csp_guess_colorspace(int width, int height);

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
