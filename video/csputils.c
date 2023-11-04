/*
 * Common code related to colorspaces and conversion
 *
 * Copyleft (C) 2009 Reimar Döffinger <Reimar.Doeffinger@gmx.de>
 *
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

#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <libavutil/common.h>
#include <libavcodec/avcodec.h>

#include "mp_image.h"
#include "csputils.h"
#include "options/m_config.h"
#include "options/m_option.h"

const struct m_opt_choice_alternatives pl_csp_names[] = {
    {"auto",        PL_COLOR_SYSTEM_UNKNOWN},
    {"bt.601",      PL_COLOR_SYSTEM_BT_601},
    {"bt.709",      PL_COLOR_SYSTEM_BT_709},
    {"smpte-240m",  PL_COLOR_SYSTEM_SMPTE_240M},
    {"bt.2020-ncl", PL_COLOR_SYSTEM_BT_2020_NC},
    {"bt.2020-cl",  PL_COLOR_SYSTEM_BT_2020_C},
    {"rgb",         PL_COLOR_SYSTEM_RGB},
    {"xyz",         PL_COLOR_SYSTEM_XYZ},
    {"ycgco",       PL_COLOR_SYSTEM_YCGCO},
    {0}
};

const struct m_opt_choice_alternatives pl_csp_levels_names[] = {
    {"auto",        PL_COLOR_LEVELS_UNKNOWN},
    {"limited",     PL_COLOR_LEVELS_LIMITED},
    {"full",        PL_COLOR_LEVELS_FULL},
    {0}
};

const struct m_opt_choice_alternatives pl_csp_prim_names[] = {
    {"auto",        PL_COLOR_PRIM_UNKNOWN},
    {"bt.601-525",  PL_COLOR_PRIM_BT_601_525},
    {"bt.601-625",  PL_COLOR_PRIM_BT_601_625},
    {"bt.709",      PL_COLOR_PRIM_BT_709},
    {"bt.2020",     PL_COLOR_PRIM_BT_2020},
    {"bt.470m",     PL_COLOR_PRIM_BT_470M},
    {"apple",       PL_COLOR_PRIM_APPLE},
    {"adobe",       PL_COLOR_PRIM_ADOBE},
    {"prophoto",    PL_COLOR_PRIM_PRO_PHOTO},
    {"cie1931",     PL_COLOR_PRIM_CIE_1931},
    {"dci-p3",      PL_COLOR_PRIM_DCI_P3},
    {"display-p3",  PL_COLOR_PRIM_DISPLAY_P3},
    {"v-gamut",     PL_COLOR_PRIM_V_GAMUT},
    {"s-gamut",     PL_COLOR_PRIM_S_GAMUT},
    {"ebu3213",     PL_COLOR_PRIM_EBU_3213},
    {"film-c",      PL_COLOR_PRIM_FILM_C},
    {"aces-ap0",    PL_COLOR_PRIM_ACES_AP0},
    {"aces-ap1",    PL_COLOR_PRIM_ACES_AP1},
    {0}
};

const struct m_opt_choice_alternatives pl_csp_trc_names[] = {
    {"auto",        PL_COLOR_TRC_UNKNOWN},
    {"bt.1886",     PL_COLOR_TRC_BT_1886},
    {"srgb",        PL_COLOR_TRC_SRGB},
    {"linear",      PL_COLOR_TRC_LINEAR},
    {"gamma1.8",    PL_COLOR_TRC_GAMMA18},
    {"gamma2.0",    PL_COLOR_TRC_GAMMA20},
    {"gamma2.2",    PL_COLOR_TRC_GAMMA22},
    {"gamma2.4",    PL_COLOR_TRC_GAMMA24},
    {"gamma2.6",    PL_COLOR_TRC_GAMMA26},
    {"gamma2.8",    PL_COLOR_TRC_GAMMA28},
    {"prophoto",    PL_COLOR_TRC_PRO_PHOTO},
    {"pq",          PL_COLOR_TRC_PQ},
    {"hlg",         PL_COLOR_TRC_HLG},
    {"v-log",       PL_COLOR_TRC_V_LOG},
    {"s-log1",      PL_COLOR_TRC_S_LOG1},
    {"s-log2",      PL_COLOR_TRC_S_LOG2},
    {"st428",       PL_COLOR_TRC_ST428},
    {0}
};

const struct m_opt_choice_alternatives mp_csp_light_names[] = {
    {"auto",        MP_CSP_LIGHT_AUTO},
    {"display",     MP_CSP_LIGHT_DISPLAY},
    {"hlg",         MP_CSP_LIGHT_SCENE_HLG},
    {"709-1886",    MP_CSP_LIGHT_SCENE_709_1886},
    {"gamma1.2",    MP_CSP_LIGHT_SCENE_1_2},
    {0}
};

const struct m_opt_choice_alternatives pl_chroma_names[] = {
    {"unknown",     PL_CHROMA_UNKNOWN},
    {"uhd",         PL_CHROMA_TOP_LEFT},
    {"mpeg2/4/h264",PL_CHROMA_LEFT},
    {"mpeg1/jpeg",  PL_CHROMA_CENTER},
    {"top",         PL_CHROMA_TOP_CENTER},
    {"bottom left", PL_CHROMA_BOTTOM_LEFT},
    {"bottom",      PL_CHROMA_BOTTOM_CENTER},
    {0}
};

const struct m_opt_choice_alternatives pl_alpha_names[] = {
    {"auto",        PL_ALPHA_UNKNOWN},
    {"straight",    PL_ALPHA_INDEPENDENT},
    {"premul",      PL_ALPHA_PREMULTIPLIED},
    {0}
};

// The short name _must_ match with what vf_stereo3d accepts (if supported).
// The long name in comments is closer to the Matroska spec (StereoMode element).
// The numeric index matches the Matroska StereoMode value. If you add entries
// that don't match Matroska, make sure demux_mkv.c rejects them properly.
const struct m_opt_choice_alternatives mp_stereo3d_names[] = {
    {"no",     -1}, // disable/invalid
    {"mono",    0},
    {"sbs2l",   1}, // "side_by_side_left"
    {"ab2r",    2}, // "top_bottom_right"
    {"ab2l",    3}, // "top_bottom_left"
    {"checkr",  4}, // "checkboard_right" (unsupported by vf_stereo3d)
    {"checkl",  5}, // "checkboard_left"  (unsupported by vf_stereo3d)
    {"irr",     6}, // "row_interleaved_right"
    {"irl",     7}, // "row_interleaved_left"
    {"icr",     8}, // "column_interleaved_right" (unsupported by vf_stereo3d)
    {"icl",     9}, // "column_interleaved_left" (unsupported by vf_stereo3d)
    {"arcc",   10}, // "anaglyph_cyan_red" (Matroska: unclear which mode)
    {"sbs2r",  11}, // "side_by_side_right"
    {"agmc",   12}, // "anaglyph_green_magenta" (Matroska: unclear which mode)
    {"al",     13}, // "alternating frames left first"
    {"ar",     14}, // "alternating frames right first"
    {0}
};

enum pl_color_system mp_csp_guess_colorspace(int width, int height)
{
    return width >= 1280 || height > 576 ? PL_COLOR_SYSTEM_BT_709 : PL_COLOR_SYSTEM_BT_601;
}

enum pl_color_primaries mp_csp_guess_primaries(int width, int height)
{
    // HD content
    if (width >= 1280 || height > 576)
        return PL_COLOR_PRIM_BT_709;

    switch (height) {
    case 576: // Typical PAL content, including anamorphic/squared
        return PL_COLOR_PRIM_BT_601_625;

    case 480: // Typical NTSC content, including squared
    case 486: // NTSC Pro or anamorphic NTSC
        return PL_COLOR_PRIM_BT_601_525;

    default: // No good metric, just pick BT.709 to minimize damage
        return PL_COLOR_PRIM_BT_709;
    }
}

// LMS<-XYZ revised matrix from CIECAM97, based on a linear transform and
// normalized for equal energy on monochrome inputs
static const pl_matrix3x3 m_cat97 = {{
    {  0.8562,  0.3372, -0.1934 },
    { -0.8360,  1.8327,  0.0033 },
    {  0.0357, -0.0469,  1.0112 },
}};

// M := M * XYZd<-XYZs
static void apply_chromatic_adaptation(struct pl_cie_xy src,
                                       struct pl_cie_xy dest, pl_matrix3x3 *mat)
{
    // If the white points are nearly identical, this is a wasteful identity
    // operation.
    if (fabs(src.x - dest.x) < 1e-6 && fabs(src.y - dest.y) < 1e-6)
        return;

    // XYZd<-XYZs = Ma^-1 * (I*[Cd/Cs]) * Ma
    // http://www.brucelindbloom.com/index.html?Eqn_ChromAdapt.html
    // For Ma, we use the CIECAM97 revised (linear) matrix
    float C[3][2];

    for (int i = 0; i < 3; i++) {
        // source cone
        C[i][0] = m_cat97.m[i][0] * pl_cie_X(src)
                + m_cat97.m[i][1] * 1
                + m_cat97.m[i][2] * pl_cie_Z(src);

        // dest cone
        C[i][1] = m_cat97.m[i][0] * pl_cie_X(dest)
                + m_cat97.m[i][1] * 1
                + m_cat97.m[i][2] * pl_cie_Z(dest);
    }

    // tmp := I * [Cd/Cs] * Ma
    pl_matrix3x3 tmp = {0};
    for (int i = 0; i < 3; i++)
        tmp.m[i][i] = C[i][1] / C[i][0];

    pl_matrix3x3_mul(&tmp, &m_cat97);

    // M := M * Ma^-1 * tmp
    pl_matrix3x3 ma_inv = m_cat97;
    pl_matrix3x3_invert(&ma_inv);
    pl_matrix3x3_mul(mat, &ma_inv);
    pl_matrix3x3_mul(mat, &tmp);
}

// Get multiplication factor required if image data is fit within the LSBs of a
// higher smaller bit depth fixed-point texture data.
// This is broken. Use mp_get_csp_uint_mul().
double mp_get_csp_mul(enum pl_color_system csp, int input_bits, int texture_bits)
{
    assert(texture_bits >= input_bits);

    // Convenience for some irrelevant cases, e.g. rgb565 or disabling expansion.
    if (!input_bits)
        return 1;

    // RGB always uses the full range available.
    if (csp == PL_COLOR_SYSTEM_RGB)
        return ((1LL << input_bits) - 1.) / ((1LL << texture_bits) - 1.);

    if (csp == PL_COLOR_SYSTEM_XYZ)
        return 1;

    // High bit depth YUV uses a range shifted from 8 bit.
    return (1LL << input_bits) / ((1LL << texture_bits) - 1.) * 255 / 256;
}

// Return information about color fixed point representation.his is needed for
// converting color from integer formats to or from float. Use as follows:
//      float_val = uint_val * m + o
//      uint_val = clamp(round((float_val - o) / m))
// See H.264/5 Annex E.
//  csp: colorspace
//  levels: full range flag
//  component: ID of the channel, as in mp_regular_imgfmt:
//             1 is red/luminance/gray, 2 is green/Cb, 3 is blue/Cr, 4 is alpha.
//  bits: number of significant bits, e.g. 10 for yuv420p10, 16 for p010
//  out_m: returns factor to multiply the uint number with
//  out_o: returns offset to add after multiplication
void mp_get_csp_uint_mul(enum pl_color_system csp, enum pl_color_levels levels,
                         int bits, int component, double *out_m, double *out_o)
{
    uint16_t i_min = 0;
    uint16_t i_max = (1u << bits) - 1;
    double f_min = 0; // min. float value

    if (csp != PL_COLOR_SYSTEM_RGB && component != 4) {
        if (component == 2 || component == 3) {
            f_min = (1u << (bits - 1)) / -(double)i_max; // force center => 0

            if (levels != PL_COLOR_LEVELS_FULL && bits >= 8) {
                i_min = 16  << (bits - 8); // => -0.5
                i_max = 240 << (bits - 8); // =>  0.5
                f_min = -0.5;
            }
        } else {
            if (levels != PL_COLOR_LEVELS_FULL && bits >= 8) {
                i_min = 16  << (bits - 8); // => 0
                i_max = 235 << (bits - 8); // => 1
            }
        }
    }

    *out_m = 1.0 / (i_max - i_min);
    *out_o = (1 + f_min) - i_max * *out_m;
}

/* Fill in the Y, U, V vectors of a yuv-to-rgb conversion matrix
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
static void luma_coeffs(struct pl_transform3x3 *mat, float lr, float lg, float lb)
{
    assert(fabs(lr+lg+lb - 1) < 1e-6);
    *mat = (struct pl_transform3x3) {
        { {{1, 0,                    2 * (1-lr)          },
           {1, -2 * (1-lb) * lb/lg, -2 * (1-lr) * lr/lg  },
           {1,  2 * (1-lb),          0                   }} },
        // Constant coefficients (mat->c) not set here
    };
}

// get the coefficients of the yuv -> rgb conversion matrix
void mp_get_csp_matrix(struct mp_csp_params *params, struct pl_transform3x3 *m)
{
    enum pl_color_system colorspace = params->repr.sys;
    if (colorspace <= PL_COLOR_SYSTEM_UNKNOWN || colorspace >= PL_COLOR_SYSTEM_COUNT)
        colorspace = PL_COLOR_SYSTEM_BT_601;
    enum pl_color_levels levels_in = params->repr.levels;
    if (levels_in <= PL_COLOR_LEVELS_UNKNOWN || levels_in >= PL_COLOR_LEVELS_COUNT)
        levels_in = PL_COLOR_LEVELS_LIMITED;

    switch (colorspace) {
    case PL_COLOR_SYSTEM_BT_601:     luma_coeffs(m, 0.299,  0.587,  0.114 ); break;
    case PL_COLOR_SYSTEM_BT_709:     luma_coeffs(m, 0.2126, 0.7152, 0.0722); break;
    case PL_COLOR_SYSTEM_SMPTE_240M: luma_coeffs(m, 0.2122, 0.7013, 0.0865); break;
    case PL_COLOR_SYSTEM_BT_2020_NC: luma_coeffs(m, 0.2627, 0.6780, 0.0593); break;
    case PL_COLOR_SYSTEM_BT_2020_C: {
        // Note: This outputs into the [-0.5,0.5] range for chroma information.
        // If this clips on any VO, a constant 0.5 coefficient can be added
        // to the chroma channels to normalize them into [0,1]. This is not
        // currently needed by anything, though.
        *m = (struct pl_transform3x3){{{{0, 0, 1}, {1, 0, 0}, {0, 1, 0}}}};
        break;
    }
    case PL_COLOR_SYSTEM_RGB: {
        *m = (struct pl_transform3x3){{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}}};
        levels_in = -1;
        break;
    }
    case PL_COLOR_SYSTEM_XYZ: {
        // For lack of anything saner to do, just assume the caller wants
        // DCI-P3 primaries, which is a reasonable assumption.
        const struct pl_raw_primaries *dst = pl_raw_primaries_get(PL_COLOR_PRIM_DCI_P3);
        pl_matrix3x3 mat = pl_get_xyz2rgb_matrix(dst);
        // DCDM X'Y'Z' is expected to have equal energy white point (EG 432-1 Annex H)
        apply_chromatic_adaptation((struct pl_cie_xy){1.0/3.0, 1.0/3.0}, dst->white, &mat);
        *m = (struct pl_transform3x3) { .mat = mat };
        levels_in = -1;
        break;
    }
    case PL_COLOR_SYSTEM_YCGCO: {
        *m = (struct pl_transform3x3) {
            {{{1,  -1,  1},
              {1,   1,  0},
              {1,  -1, -1}}},
        };
        break;
    }
    default:
        MP_ASSERT_UNREACHABLE();
    };

    if (params->is_float)
        levels_in = -1;

    if ((colorspace == PL_COLOR_SYSTEM_BT_601 || colorspace == PL_COLOR_SYSTEM_BT_709 ||
         colorspace == PL_COLOR_SYSTEM_SMPTE_240M || colorspace == PL_COLOR_SYSTEM_BT_2020_NC))
    {
        // Hue is equivalent to rotating input [U, V] subvector around the origin.
        // Saturation scales [U, V].
        float huecos = params->gray ? 0 : params->saturation * cos(params->hue);
        float huesin = params->gray ? 0 : params->saturation * sin(params->hue);
        for (int i = 0; i < 3; i++) {
            float u = m->mat.m[i][1], v = m->mat.m[i][2];
            m->mat.m[i][1] = huecos * u - huesin * v;
            m->mat.m[i][2] = huesin * u + huecos * v;
        }
    }

    // The values below are written in 0-255 scale - thus bring s into range.
    double s =
        mp_get_csp_mul(colorspace, params->input_bits, params->texture_bits) / 255;
    // NOTE: The yuvfull ranges as presented here are arguably ambiguous,
    // and conflict with at least the full-range YCbCr/ICtCp values as defined
    // by ITU-R BT.2100. If somebody ever complains about full-range YUV looking
    // different from their reference display, this comment is probably why.
    struct yuvlevels { double ymin, ymax, cmax, cmid; }
        yuvlim =  { 16*s, 235*s, 240*s, 128*s },
        yuvfull = {  0*s, 255*s, 255*s, 128*s },
        anyfull = {  0*s, 255*s, 255*s/2, 0 }, // cmax picked to make cmul=ymul
        yuvlev;
    switch (levels_in) {
    case PL_COLOR_LEVELS_LIMITED: yuvlev = yuvlim; break;
    case PL_COLOR_LEVELS_FULL: yuvlev = yuvfull; break;
    case -1: yuvlev = anyfull; break;
    default:
        MP_ASSERT_UNREACHABLE();
    }

    int levels_out = params->levels_out;
    if (levels_out <= PL_COLOR_LEVELS_UNKNOWN || levels_out >= PL_COLOR_LEVELS_COUNT)
        levels_out = PL_COLOR_LEVELS_FULL;
    struct rgblevels { double min, max; }
        rgblim =  { 16/255., 235/255. },
        rgbfull = {      0,        1  },
        rgblev;
    switch (levels_out) {
    case PL_COLOR_LEVELS_LIMITED: rgblev = rgblim; break;
    case PL_COLOR_LEVELS_FULL: rgblev = rgbfull; break;
    default:
        MP_ASSERT_UNREACHABLE();
    }

    double ymul = (rgblev.max - rgblev.min) / (yuvlev.ymax - yuvlev.ymin);
    double cmul = (rgblev.max - rgblev.min) / (yuvlev.cmax - yuvlev.cmid) / 2;

    // Contrast scales the output value range (gain)
    ymul *= params->contrast;
    cmul *= params->contrast;

    for (int i = 0; i < 3; i++) {
        m->mat.m[i][0] *= ymul;
        m->mat.m[i][1] *= cmul;
        m->mat.m[i][2] *= cmul;
        // Set c so that Y=umin,UV=cmid maps to RGB=min (black to black),
        // also add brightness offset (black lift)
        m->c[i] = rgblev.min - m->mat.m[i][0] * yuvlev.ymin
                  - (m->mat.m[i][1] + m->mat.m[i][2]) * yuvlev.cmid
                  + params->brightness;
    }
}

// Set colorspace related fields in p from f. Don't touch other fields.
void mp_csp_set_image_params(struct mp_csp_params *params,
                             const struct mp_image_params *imgparams)
{
    struct mp_image_params p = *imgparams;
    mp_image_params_guess_csp(&p); // ensure consistency
    params->color = p.color;
}

enum mp_csp_equalizer_param {
    MP_CSP_EQ_BRIGHTNESS,
    MP_CSP_EQ_CONTRAST,
    MP_CSP_EQ_HUE,
    MP_CSP_EQ_SATURATION,
    MP_CSP_EQ_GAMMA,
    MP_CSP_EQ_COUNT,
};

// Default initialization with 0 is enough, except for the capabilities field
struct mp_csp_equalizer_opts {
    // Value for each property is in the range [-100.0, 100.0].
    // 0.0 is default, meaning neutral or no change.
    float values[MP_CSP_EQ_COUNT];
    int output_levels;
};

#define OPT_BASE_STRUCT struct mp_csp_equalizer_opts

const struct m_sub_options mp_csp_equalizer_conf = {
    .opts = (const m_option_t[]) {
        {"brightness", OPT_FLOAT(values[MP_CSP_EQ_BRIGHTNESS]),
            M_RANGE(-100, 100)},
        {"saturation", OPT_FLOAT(values[MP_CSP_EQ_SATURATION]),
            M_RANGE(-100, 100)},
        {"contrast", OPT_FLOAT(values[MP_CSP_EQ_CONTRAST]),
            M_RANGE(-100, 100)},
        {"hue", OPT_FLOAT(values[MP_CSP_EQ_HUE]),
            M_RANGE(-100, 100)},
        {"gamma", OPT_FLOAT(values[MP_CSP_EQ_GAMMA]),
            M_RANGE(-100, 100)},
        {"video-output-levels",
            OPT_CHOICE_C(output_levels, pl_csp_levels_names)},
        {0}
    },
    .size = sizeof(struct mp_csp_equalizer_opts),
};

// Copy settings from eq into params.
static void mp_csp_copy_equalizer_values(struct mp_csp_params *params,
                                         const struct mp_csp_equalizer_opts *eq)
{
    params->brightness = eq->values[MP_CSP_EQ_BRIGHTNESS] / 100.0;
    params->contrast = (eq->values[MP_CSP_EQ_CONTRAST] + 100) / 100.0;
    params->hue = eq->values[MP_CSP_EQ_HUE] / 100.0 * M_PI;
    params->saturation = (eq->values[MP_CSP_EQ_SATURATION] + 100) / 100.0;
    params->gamma = exp(log(8.0) * eq->values[MP_CSP_EQ_GAMMA] / 100.0);
    params->levels_out = eq->output_levels;
}

struct mp_csp_equalizer_state *mp_csp_equalizer_create(void *ta_parent,
                                                    struct mpv_global *global)
{
    struct m_config_cache *c = m_config_cache_alloc(ta_parent, global,
                                                    &mp_csp_equalizer_conf);
    // The terrible, terrible truth.
    return (struct mp_csp_equalizer_state *)c;
}

bool mp_csp_equalizer_state_changed(struct mp_csp_equalizer_state *state)
{
    struct m_config_cache *c = (struct m_config_cache *)state;
    return m_config_cache_update(c);
}

void mp_csp_equalizer_state_get(struct mp_csp_equalizer_state *state,
                                struct mp_csp_params *params)
{
    struct m_config_cache *c = (struct m_config_cache *)state;
    m_config_cache_update(c);
    struct mp_csp_equalizer_opts *opts = c->opts;
    mp_csp_copy_equalizer_values(params, opts);
}

// Multiply the color in c with the given matrix.
// i/o is {R, G, B} or {Y, U, V} (depending on input/output and matrix), using
// a fixed point representation with the given number of bits (so for bits==8,
// [0,255] maps to [0,1]). The output is clipped to the range as needed.
void mp_map_fixp_color(struct pl_transform3x3 *matrix, int ibits, int in[3],
                                               int obits, int out[3])
{
    for (int i = 0; i < 3; i++) {
        double val = matrix->c[i];
        for (int x = 0; x < 3; x++)
            val += matrix->mat.m[i][x] * in[x] / ((1 << ibits) - 1);
        int ival = lrint(val * ((1 << obits) - 1));
        out[i] = av_clip(ival, 0, (1 << obits) - 1);
    }
}
