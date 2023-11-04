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

#ifndef MPLAYER_CSPUTILS_H
#define MPLAYER_CSPUTILS_H

#include <stdbool.h>
#include <stdint.h>

#include <libplacebo/colorspace.h>

#include "options/m_option.h"

/* NOTE: the csp and levels AUTO values are converted to specific ones
 * above vf/vo level. At least vf_scale relies on all valid settings being
 * nonzero at vf/vo level.
 */

extern const struct m_opt_choice_alternatives pl_csp_names[];
extern const struct m_opt_choice_alternatives pl_csp_levels_names[];
extern const struct m_opt_choice_alternatives pl_csp_prim_names[];
extern const struct m_opt_choice_alternatives pl_csp_trc_names[];

enum mp_csp_light {
    MP_CSP_LIGHT_AUTO,
    MP_CSP_LIGHT_DISPLAY,
    MP_CSP_LIGHT_SCENE_HLG,
    MP_CSP_LIGHT_SCENE_709_1886,
    MP_CSP_LIGHT_SCENE_1_2,
    MP_CSP_LIGHT_COUNT
};

extern const struct m_opt_choice_alternatives mp_csp_light_names[];

// These constants are based on the ICC specification (Table 23) and match
// up with the API of LittleCMS, which treats them as integers.
enum mp_render_intent {
    MP_INTENT_PERCEPTUAL = 0,
    MP_INTENT_RELATIVE_COLORIMETRIC = 1,
    MP_INTENT_SATURATION = 2,
    MP_INTENT_ABSOLUTE_COLORIMETRIC = 3
};

// The numeric values (except -1) match the Matroska StereoMode element value.
enum mp_stereo3d_mode {
    MP_STEREO3D_INVALID = -1,
    /* only modes explicitly referenced in the code are listed */
    MP_STEREO3D_MONO = 0,
    MP_STEREO3D_SBS2L = 1,
    MP_STEREO3D_AB2R = 2,
    MP_STEREO3D_AB2L = 3,
    MP_STEREO3D_SBS2R = 11,
    /* no explicit enum entries for most valid values */
    MP_STEREO3D_COUNT = 15, // 14 is last valid mode
};

extern const struct m_opt_choice_alternatives mp_stereo3d_names[];

#define MP_STEREO3D_NAME(x) m_opt_choice_str(mp_stereo3d_names, x)

#define MP_STEREO3D_NAME_DEF(x, def) \
    (MP_STEREO3D_NAME(x) ? MP_STEREO3D_NAME(x) : (def))

// For many colorspace conversions, in particular those involving HDR, an
// implicit reference white level is needed. Since this magic constant shows up
// a lot, give it an explicit name. The value of 203 cd/m² comes from ITU-R
// Report BT.2408, and the value for HLG comes from the cited HLG 75% level
// (transferred to scene space).
#define MP_REF_WHITE 203.0
#define MP_REF_WHITE_HLG 3.17955

struct mp_csp_params {
    struct pl_color_repr repr;
    struct pl_color_space color;
    enum pl_color_levels levels_out; // output device
    float brightness;
    float contrast;
    float hue;
    float saturation;
    float gamma;
    // discard U/V components
    bool gray;
    // input is already centered and range-expanded
    bool is_float;
    // texture_bits/input_bits is for rescaling fixed point input to range [0,1]
    int texture_bits;
    int input_bits;
};

#define MP_CSP_PARAMS_DEFAULTS {                                \
    .repr = pl_color_repr_sdtv,                                 \
    .levels_out = PL_COLOR_LEVELS_FULL,                         \
    .brightness = 0, .contrast = 1, .hue = 0, .saturation = 1,  \
    .gamma = 1, .texture_bits = 8, .input_bits = 8}

struct mp_image_params;
void mp_csp_set_image_params(struct mp_csp_params *params,
                             const struct mp_image_params *imgparams);

extern const struct m_opt_choice_alternatives pl_chroma_names[];
extern const struct m_opt_choice_alternatives pl_alpha_names[];

extern const struct m_sub_options mp_csp_equalizer_conf;

struct mpv_global;
struct mp_csp_equalizer_state *mp_csp_equalizer_create(void *ta_parent,
                                                    struct mpv_global *global);
bool mp_csp_equalizer_state_changed(struct mp_csp_equalizer_state *state);
void mp_csp_equalizer_state_get(struct mp_csp_equalizer_state *state,
                                struct mp_csp_params *params);

struct mp_csp_col_xy {
    float x, y;
};

static inline float mp_xy_X(struct mp_csp_col_xy xy) {
    return xy.x / xy.y;
}

static inline float mp_xy_Z(struct mp_csp_col_xy xy) {
    return (1 - xy.x - xy.y) / xy.y;
}

struct mp_csp_primaries {
    struct mp_csp_col_xy red, green, blue, white;
};

enum pl_color_system mp_csp_guess_colorspace(int width, int height);
enum pl_color_primaries mp_csp_guess_primaries(int width, int height);

struct mp_csp_primaries mp_get_csp_primaries(enum pl_color_primaries csp);
float mp_trc_nom_peak(enum pl_color_transfer trc);
bool mp_trc_is_hdr(enum pl_color_transfer trc);

/* Color conversion matrix: RGB = m * YUV + c
 * m is in row-major matrix, with m[row][col], e.g.:
 *     [ a11 a12 a13 ]     float m[3][3] = { { a11, a12, a13 },
 *     [ a21 a22 a23 ]                       { a21, a22, a23 },
 *     [ a31 a32 a33 ]                       { a31, a32, a33 } };
 * This is accessed as e.g.: m[2-1][1-1] = a21
 * In particular, each row contains all the coefficients for one of R, G, B,
 * while each column contains all the coefficients for one of Y, U, V:
 *     m[r,g,b][y,u,v] = ...
 * The matrix could also be viewed as group of 3 vectors, e.g. the 1st column
 * is the Y vector (1, 1, 1), the 2nd is the U vector, the 3rd the V vector.
 * The matrix might also be used for other conversions and colorspaces.
 */
struct mp_cmat {
    float m[3][3];
    float c[3];
};

void mp_get_rgb2xyz_matrix(struct mp_csp_primaries space, float m[3][3]);
void mp_get_cms_matrix(struct mp_csp_primaries src, struct mp_csp_primaries dest,
                       enum mp_render_intent intent, float cms_matrix[3][3]);

double mp_get_csp_mul(enum pl_color_system csp, int input_bits, int texture_bits);
void mp_get_csp_uint_mul(enum pl_color_system csp, enum pl_color_levels levels,
                         int bits, int component, double *out_m, double *out_o);
void mp_get_csp_matrix(struct mp_csp_params *params, struct mp_cmat *out);

void mp_invert_matrix3x3(float m[3][3]);
void mp_invert_cmat(struct mp_cmat *out, struct mp_cmat *in);
void mp_map_fixp_color(struct mp_cmat *matrix, int ibits, int in[3],
                                               int obits, int out[3]);

#endif /* MPLAYER_CSPUTILS_H */
