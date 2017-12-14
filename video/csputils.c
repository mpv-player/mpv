/*
 * Common code related to colorspaces and conversion
 *
 * Copyleft (C) 2009 Reimar Döffinger <Reimar.Doeffinger@gmx.de>
 *
 * mp_invert_cmat based on DarkPlaces engine (relicensed from GPL to LGPL)
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

#include "config.h"

#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <libavutil/common.h>
#include <libavcodec/avcodec.h>

#include "mp_image.h"
#include "csputils.h"
#include "options/m_config.h"
#include "options/m_option.h"

const struct m_opt_choice_alternatives mp_csp_names[] = {
    {"auto",        MP_CSP_AUTO},
    {"bt.601",      MP_CSP_BT_601},
    {"bt.709",      MP_CSP_BT_709},
    {"smpte-240m",  MP_CSP_SMPTE_240M},
    {"bt.2020-ncl", MP_CSP_BT_2020_NC},
    {"bt.2020-cl",  MP_CSP_BT_2020_C},
    {"rgb",         MP_CSP_RGB},
    {"xyz",         MP_CSP_XYZ},
    {"ycgco",       MP_CSP_YCGCO},
    {0}
};

const struct m_opt_choice_alternatives mp_csp_levels_names[] = {
    {"auto",        MP_CSP_LEVELS_AUTO},
    {"limited",     MP_CSP_LEVELS_TV},
    {"full",        MP_CSP_LEVELS_PC},
    {0}
};

const struct m_opt_choice_alternatives mp_csp_prim_names[] = {
    {"auto",        MP_CSP_PRIM_AUTO},
    {"bt.601-525",  MP_CSP_PRIM_BT_601_525},
    {"bt.601-625",  MP_CSP_PRIM_BT_601_625},
    {"bt.709",      MP_CSP_PRIM_BT_709},
    {"bt.2020",     MP_CSP_PRIM_BT_2020},
    {"bt.470m",     MP_CSP_PRIM_BT_470M},
    {"apple",       MP_CSP_PRIM_APPLE},
    {"adobe",       MP_CSP_PRIM_ADOBE},
    {"prophoto",    MP_CSP_PRIM_PRO_PHOTO},
    {"cie1931",     MP_CSP_PRIM_CIE_1931},
    {"dci-p3",      MP_CSP_PRIM_DCI_P3},
    {"display-p3",  MP_CSP_PRIM_DISPLAY_P3},
    {"v-gamut",     MP_CSP_PRIM_V_GAMUT},
    {"s-gamut",     MP_CSP_PRIM_S_GAMUT},
    {0}
};

const struct m_opt_choice_alternatives mp_csp_trc_names[] = {
    {"auto",        MP_CSP_TRC_AUTO},
    {"bt.1886",     MP_CSP_TRC_BT_1886},
    {"srgb",        MP_CSP_TRC_SRGB},
    {"linear",      MP_CSP_TRC_LINEAR},
    {"gamma1.8",    MP_CSP_TRC_GAMMA18},
    {"gamma2.2",    MP_CSP_TRC_GAMMA22},
    {"gamma2.8",    MP_CSP_TRC_GAMMA28},
    {"prophoto",    MP_CSP_TRC_PRO_PHOTO},
    {"pq",          MP_CSP_TRC_PQ},
    {"hlg",         MP_CSP_TRC_HLG},
    {"v-log",       MP_CSP_TRC_V_LOG},
    {"s-log1",      MP_CSP_TRC_S_LOG1},
    {"s-log2",      MP_CSP_TRC_S_LOG2},
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

const struct m_opt_choice_alternatives mp_chroma_names[] = {
    {"unknown",     MP_CHROMA_AUTO},
    {"mpeg2/4/h264",MP_CHROMA_LEFT},
    {"mpeg1/jpeg",  MP_CHROMA_CENTER},
    {0}
};

void mp_colorspace_merge(struct mp_colorspace *orig, struct mp_colorspace *new)
{
    if (!orig->space)
        orig->space = new->space;
    if (!orig->levels)
        orig->levels = new->levels;
    if (!orig->primaries)
        orig->primaries = new->primaries;
    if (!orig->gamma)
        orig->gamma = new->gamma;
    if (!orig->sig_peak)
        orig->sig_peak = new->sig_peak;
    if (!orig->light)
        orig->light = new->light;
}

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

enum mp_csp avcol_spc_to_mp_csp(int avcolorspace)
{
    switch (avcolorspace) {
    case AVCOL_SPC_BT709:       return MP_CSP_BT_709;
    case AVCOL_SPC_BT470BG:     return MP_CSP_BT_601;
    case AVCOL_SPC_BT2020_NCL:  return MP_CSP_BT_2020_NC;
    case AVCOL_SPC_BT2020_CL:   return MP_CSP_BT_2020_C;
    case AVCOL_SPC_SMPTE170M:   return MP_CSP_BT_601;
    case AVCOL_SPC_SMPTE240M:   return MP_CSP_SMPTE_240M;
    case AVCOL_SPC_RGB:         return MP_CSP_RGB;
    case AVCOL_SPC_YCOCG:       return MP_CSP_YCGCO;
    default:                    return MP_CSP_AUTO;
    }
}

enum mp_csp_levels avcol_range_to_mp_csp_levels(int avrange)
{
    switch (avrange) {
    case AVCOL_RANGE_MPEG:      return MP_CSP_LEVELS_TV;
    case AVCOL_RANGE_JPEG:      return MP_CSP_LEVELS_PC;
    default:                    return MP_CSP_LEVELS_AUTO;
    }
}

enum mp_csp_prim avcol_pri_to_mp_csp_prim(int avpri)
{
    switch (avpri) {
    case AVCOL_PRI_SMPTE240M:   // Same as below
    case AVCOL_PRI_SMPTE170M:   return MP_CSP_PRIM_BT_601_525;
    case AVCOL_PRI_BT470BG:     return MP_CSP_PRIM_BT_601_625;
    case AVCOL_PRI_BT709:       return MP_CSP_PRIM_BT_709;
    case AVCOL_PRI_BT2020:      return MP_CSP_PRIM_BT_2020;
    case AVCOL_PRI_BT470M:      return MP_CSP_PRIM_BT_470M;
    default:                    return MP_CSP_PRIM_AUTO;
    }
}

enum mp_csp_trc avcol_trc_to_mp_csp_trc(int avtrc)
{
    switch (avtrc) {
    case AVCOL_TRC_BT709:
    case AVCOL_TRC_SMPTE170M:
    case AVCOL_TRC_SMPTE240M:
    case AVCOL_TRC_BT1361_ECG:
    case AVCOL_TRC_BT2020_10:
    case AVCOL_TRC_BT2020_12:    return MP_CSP_TRC_BT_1886;
    case AVCOL_TRC_IEC61966_2_1: return MP_CSP_TRC_SRGB;
    case AVCOL_TRC_LINEAR:       return MP_CSP_TRC_LINEAR;
    case AVCOL_TRC_GAMMA22:      return MP_CSP_TRC_GAMMA22;
    case AVCOL_TRC_GAMMA28:      return MP_CSP_TRC_GAMMA28;
    case AVCOL_TRC_SMPTEST2084:  return MP_CSP_TRC_PQ;
    case AVCOL_TRC_ARIB_STD_B67: return MP_CSP_TRC_HLG;
    default:                     return MP_CSP_TRC_AUTO;
    }
}

int mp_csp_to_avcol_spc(enum mp_csp colorspace)
{
    switch (colorspace) {
    case MP_CSP_BT_709:         return AVCOL_SPC_BT709;
    case MP_CSP_BT_601:         return AVCOL_SPC_BT470BG;
    case MP_CSP_BT_2020_NC:     return AVCOL_SPC_BT2020_NCL;
    case MP_CSP_BT_2020_C:      return AVCOL_SPC_BT2020_CL;
    case MP_CSP_SMPTE_240M:     return AVCOL_SPC_SMPTE240M;
    case MP_CSP_RGB:            return AVCOL_SPC_RGB;
    case MP_CSP_YCGCO:          return AVCOL_SPC_YCOCG;
    default:                    return AVCOL_SPC_UNSPECIFIED;
    }
}

int mp_csp_levels_to_avcol_range(enum mp_csp_levels range)
{
    switch (range) {
    case MP_CSP_LEVELS_TV:      return AVCOL_RANGE_MPEG;
    case MP_CSP_LEVELS_PC:      return AVCOL_RANGE_JPEG;
    default:                    return AVCOL_RANGE_UNSPECIFIED;
    }
}

int mp_csp_prim_to_avcol_pri(enum mp_csp_prim prim)
{
    switch (prim) {
    case MP_CSP_PRIM_BT_601_525: return AVCOL_PRI_SMPTE170M;
    case MP_CSP_PRIM_BT_601_625: return AVCOL_PRI_BT470BG;
    case MP_CSP_PRIM_BT_709:     return AVCOL_PRI_BT709;
    case MP_CSP_PRIM_BT_2020:    return AVCOL_PRI_BT2020;
    case MP_CSP_PRIM_BT_470M:    return AVCOL_PRI_BT470M;
    default:                     return AVCOL_PRI_UNSPECIFIED;
    }
}

int mp_csp_trc_to_avcol_trc(enum mp_csp_trc trc)
{
    switch (trc) {
    // We just call it BT.1886 since we're decoding, but it's still BT.709
    case MP_CSP_TRC_BT_1886:      return AVCOL_TRC_BT709;
    case MP_CSP_TRC_SRGB:         return AVCOL_TRC_IEC61966_2_1;
    case MP_CSP_TRC_LINEAR:       return AVCOL_TRC_LINEAR;
    case MP_CSP_TRC_GAMMA22:      return AVCOL_TRC_GAMMA22;
    case MP_CSP_TRC_GAMMA28:      return AVCOL_TRC_GAMMA28;
    case MP_CSP_TRC_PQ:           return AVCOL_TRC_SMPTEST2084;
    case MP_CSP_TRC_HLG:          return AVCOL_TRC_ARIB_STD_B67;
    default:                      return AVCOL_TRC_UNSPECIFIED;
    }
}

enum mp_csp mp_csp_guess_colorspace(int width, int height)
{
    return width >= 1280 || height > 576 ? MP_CSP_BT_709 : MP_CSP_BT_601;
}

enum mp_csp_prim mp_csp_guess_primaries(int width, int height)
{
    // HD content
    if (width >= 1280 || height > 576)
        return MP_CSP_PRIM_BT_709;

    switch (height) {
    case 576: // Typical PAL content, including anamorphic/squared
        return MP_CSP_PRIM_BT_601_625;

    case 480: // Typical NTSC content, including squared
    case 486: // NTSC Pro or anamorphic NTSC
        return MP_CSP_PRIM_BT_601_525;

    default: // No good metric, just pick BT.709 to minimize damage
        return MP_CSP_PRIM_BT_709;
    }
}

enum mp_chroma_location avchroma_location_to_mp(int avloc)
{
    switch (avloc) {
    case AVCHROMA_LOC_LEFT:             return MP_CHROMA_LEFT;
    case AVCHROMA_LOC_CENTER:           return MP_CHROMA_CENTER;
    default:                            return MP_CHROMA_AUTO;
    }
}

int mp_chroma_location_to_av(enum mp_chroma_location mploc)
{
    switch (mploc) {
    case MP_CHROMA_LEFT:                return AVCHROMA_LOC_LEFT;
    case MP_CHROMA_CENTER:              return AVCHROMA_LOC_CENTER;
    default:                            return AVCHROMA_LOC_UNSPECIFIED;
    }
}

// Return location of chroma samples relative to luma samples. 0/0 means
// centered. Other possible values are -1 (top/left) and +1 (right/bottom).
void mp_get_chroma_location(enum mp_chroma_location loc, int *x, int *y)
{
    *x = 0;
    *y = 0;
    if (loc == MP_CHROMA_LEFT)
        *x = -1;
}

void mp_invert_matrix3x3(float m[3][3])
{
    float m00 = m[0][0], m01 = m[0][1], m02 = m[0][2],
          m10 = m[1][0], m11 = m[1][1], m12 = m[1][2],
          m20 = m[2][0], m21 = m[2][1], m22 = m[2][2];

    // calculate the adjoint
    m[0][0] =  (m11 * m22 - m21 * m12);
    m[0][1] = -(m01 * m22 - m21 * m02);
    m[0][2] =  (m01 * m12 - m11 * m02);
    m[1][0] = -(m10 * m22 - m20 * m12);
    m[1][1] =  (m00 * m22 - m20 * m02);
    m[1][2] = -(m00 * m12 - m10 * m02);
    m[2][0] =  (m10 * m21 - m20 * m11);
    m[2][1] = -(m00 * m21 - m20 * m01);
    m[2][2] =  (m00 * m11 - m10 * m01);

    // calculate the determinant (as inverse == 1/det * adjoint,
    // adjoint * m == identity * det, so this calculates the det)
    float det = m00 * m[0][0] + m10 * m[0][1] + m20 * m[0][2];
    det = 1.0f / det;

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++)
            m[i][j] *= det;
    }
}

// A := A * B
static void mp_mul_matrix3x3(float a[3][3], float b[3][3])
{
    float a00 = a[0][0], a01 = a[0][1], a02 = a[0][2],
          a10 = a[1][0], a11 = a[1][1], a12 = a[1][2],
          a20 = a[2][0], a21 = a[2][1], a22 = a[2][2];

    for (int i = 0; i < 3; i++) {
        a[0][i] = a00 * b[0][i] + a01 * b[1][i] + a02 * b[2][i];
        a[1][i] = a10 * b[0][i] + a11 * b[1][i] + a12 * b[2][i];
        a[2][i] = a20 * b[0][i] + a21 * b[1][i] + a22 * b[2][i];
    }
}

// return the primaries associated with a certain mp_csp_primaries val
struct mp_csp_primaries mp_get_csp_primaries(enum mp_csp_prim spc)
{
    /*
    Values from: ITU-R Recommendations BT.470-6, BT.601-7, BT.709-5, BT.2020-0

    https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.470-6-199811-S!!PDF-E.pdf
    https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.601-7-201103-I!!PDF-E.pdf
    https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.709-5-200204-I!!PDF-E.pdf
    https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.2020-0-201208-I!!PDF-E.pdf

    Other colorspaces from https://en.wikipedia.org/wiki/RGB_color_space#Specifications
    */

    // CIE standard illuminant series
    static const struct mp_csp_col_xy
        d50 = {0.34577, 0.35850},
        d65 = {0.31271, 0.32902},
        c   = {0.31006, 0.31616},
        dci = {0.31400, 0.35100},
        e   = {1.0/3.0, 1.0/3.0};

    switch (spc) {
    case MP_CSP_PRIM_BT_470M:
        return (struct mp_csp_primaries) {
            .red   = {0.670, 0.330},
            .green = {0.210, 0.710},
            .blue  = {0.140, 0.080},
            .white = c
        };
    case MP_CSP_PRIM_BT_601_525:
        return (struct mp_csp_primaries) {
            .red   = {0.630, 0.340},
            .green = {0.310, 0.595},
            .blue  = {0.155, 0.070},
            .white = d65
        };
    case MP_CSP_PRIM_BT_601_625:
        return (struct mp_csp_primaries) {
            .red   = {0.640, 0.330},
            .green = {0.290, 0.600},
            .blue  = {0.150, 0.060},
            .white = d65
        };
    // This is the default assumption if no colorspace information could
    // be determined, eg. for files which have no video channel.
    case MP_CSP_PRIM_AUTO:
    case MP_CSP_PRIM_BT_709:
        return (struct mp_csp_primaries) {
            .red   = {0.640, 0.330},
            .green = {0.300, 0.600},
            .blue  = {0.150, 0.060},
            .white = d65
        };
    case MP_CSP_PRIM_BT_2020:
        return (struct mp_csp_primaries) {
            .red   = {0.708, 0.292},
            .green = {0.170, 0.797},
            .blue  = {0.131, 0.046},
            .white = d65
        };
    case MP_CSP_PRIM_APPLE:
        return (struct mp_csp_primaries) {
            .red   = {0.625, 0.340},
            .green = {0.280, 0.595},
            .blue  = {0.115, 0.070},
            .white = d65
        };
    case MP_CSP_PRIM_ADOBE:
        return (struct mp_csp_primaries) {
            .red   = {0.640, 0.330},
            .green = {0.210, 0.710},
            .blue  = {0.150, 0.060},
            .white = d65
        };
    case MP_CSP_PRIM_PRO_PHOTO:
        return (struct mp_csp_primaries) {
            .red   = {0.7347, 0.2653},
            .green = {0.1596, 0.8404},
            .blue  = {0.0366, 0.0001},
            .white = d50
        };
    case MP_CSP_PRIM_CIE_1931:
        return (struct mp_csp_primaries) {
            .red   = {0.7347, 0.2653},
            .green = {0.2738, 0.7174},
            .blue  = {0.1666, 0.0089},
            .white = e
        };
    // From SMPTE RP 431-2 and 432-1
    case MP_CSP_PRIM_DCI_P3:
    case MP_CSP_PRIM_DISPLAY_P3:
        return (struct mp_csp_primaries) {
            .red   = {0.680, 0.320},
            .green = {0.265, 0.690},
            .blue  = {0.150, 0.060},
            .white = spc == MP_CSP_PRIM_DCI_P3 ? dci : d65
        };
    // From Panasonic VARICAM reference manual
    case MP_CSP_PRIM_V_GAMUT:
        return (struct mp_csp_primaries) {
            .red   = {0.730, 0.280},
            .green = {0.165, 0.840},
            .blue  = {0.100, -0.03},
            .white = d65
        };
    // From Sony S-Log reference manual
    case MP_CSP_PRIM_S_GAMUT:
        return (struct mp_csp_primaries) {
            .red   = {0.730, 0.280},
            .green = {0.140, 0.855},
            .blue  = {0.100, -0.05},
            .white = d65
        };
    default:
        return (struct mp_csp_primaries) {{0}};
    }
}

// Get the nominal peak for a given colorspace, relative to the reference white
// level. In other words, this returns the brightest encodable value that can
// be represented by a given transfer curve.
float mp_trc_nom_peak(enum mp_csp_trc trc)
{
    switch (trc) {
    case MP_CSP_TRC_PQ:           return 10000.0 / MP_REF_WHITE;
    case MP_CSP_TRC_HLG:          return 12.0;
    case MP_CSP_TRC_V_LOG:        return 46.0855;
    case MP_CSP_TRC_S_LOG1:       return 6.52;
    case MP_CSP_TRC_S_LOG2:       return 9.212;
    }

    return 1.0;
}

bool mp_trc_is_hdr(enum mp_csp_trc trc)
{
    return mp_trc_nom_peak(trc) > 1.0;
}

// Compute the RGB/XYZ matrix as described here:
// http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
void mp_get_rgb2xyz_matrix(struct mp_csp_primaries space, float m[3][3])
{
    float S[3], X[4], Z[4];

    // Convert from CIE xyY to XYZ. Note that Y=1 holds true for all primaries
    X[0] = space.red.x   / space.red.y;
    X[1] = space.green.x / space.green.y;
    X[2] = space.blue.x  / space.blue.y;
    X[3] = space.white.x / space.white.y;

    Z[0] = (1 - space.red.x   - space.red.y)   / space.red.y;
    Z[1] = (1 - space.green.x - space.green.y) / space.green.y;
    Z[2] = (1 - space.blue.x  - space.blue.y)  / space.blue.y;
    Z[3] = (1 - space.white.x - space.white.y) / space.white.y;

    // S = XYZ^-1 * W
    for (int i = 0; i < 3; i++) {
        m[0][i] = X[i];
        m[1][i] = 1;
        m[2][i] = Z[i];
    }

    mp_invert_matrix3x3(m);

    for (int i = 0; i < 3; i++)
        S[i] = m[i][0] * X[3] + m[i][1] * 1 + m[i][2] * Z[3];

    // M = [Sc * XYZc]
    for (int i = 0; i < 3; i++) {
        m[0][i] = S[i] * X[i];
        m[1][i] = S[i] * 1;
        m[2][i] = S[i] * Z[i];
    }
}

// M := M * XYZd<-XYZs
static void mp_apply_chromatic_adaptation(struct mp_csp_col_xy src,
                                          struct mp_csp_col_xy dest, float m[3][3])
{
    // If the white points are nearly identical, this is a wasteful identity
    // operation.
    if (fabs(src.x - dest.x) < 1e-6 && fabs(src.y - dest.y) < 1e-6)
        return;

    // XYZd<-XYZs = Ma^-1 * (I*[Cd/Cs]) * Ma
    // http://www.brucelindbloom.com/index.html?Eqn_ChromAdapt.html
    float C[3][2], tmp[3][3] = {{0}};

    // Ma = Bradford matrix, arguably most popular method in use today.
    // This is derived experimentally and thus hard-coded.
    float bradford[3][3] = {
        {  0.8951,  0.2664, -0.1614 },
        { -0.7502,  1.7135,  0.0367 },
        {  0.0389, -0.0685,  1.0296 },
    };

    for (int i = 0; i < 3; i++) {
        // source cone
        C[i][0] = bradford[i][0] * mp_xy_X(src)
                + bradford[i][1] * 1
                + bradford[i][2] * mp_xy_Z(src);

        // dest cone
        C[i][1] = bradford[i][0] * mp_xy_X(dest)
                + bradford[i][1] * 1
                + bradford[i][2] * mp_xy_Z(dest);
    }

    // tmp := I * [Cd/Cs] * Ma
    for (int i = 0; i < 3; i++)
        tmp[i][i] = C[i][1] / C[i][0];

    mp_mul_matrix3x3(tmp, bradford);

    // M := M * Ma^-1 * tmp
    mp_invert_matrix3x3(bradford);
    mp_mul_matrix3x3(m, bradford);
    mp_mul_matrix3x3(m, tmp);
}

// get the coefficients of the source -> dest cms matrix
void mp_get_cms_matrix(struct mp_csp_primaries src, struct mp_csp_primaries dest,
                       enum mp_render_intent intent, float m[3][3])
{
    float tmp[3][3];

    // In saturation mapping, we don't care about accuracy and just want
    // primaries to map to primaries, making this an identity transformation.
    if (intent == MP_INTENT_SATURATION) {
        for (int i = 0; i < 3; i++)
            m[i][i] = 1;
        return;
    }

    // RGBd<-RGBs = RGBd<-XYZd * XYZd<-XYZs * XYZs<-RGBs
    // Equations from: http://www.brucelindbloom.com/index.html?Math.html
    // Note: Perceptual is treated like relative colorimetric. There's no
    // definition for perceptual other than "make it look good".

    // RGBd<-XYZd, inverted from XYZd<-RGBd
    mp_get_rgb2xyz_matrix(dest, m);
    mp_invert_matrix3x3(m);

    // Chromatic adaptation, except in absolute colorimetric intent
    if (intent != MP_INTENT_ABSOLUTE_COLORIMETRIC)
        mp_apply_chromatic_adaptation(src.white, dest.white, m);

    // XYZs<-RGBs
    mp_get_rgb2xyz_matrix(src, tmp);
    mp_mul_matrix3x3(m, tmp);
}

// get the coefficients of an SMPTE 428-1 xyz -> rgb conversion matrix
// intent = the rendering intent used to convert to the target primaries
static void mp_get_xyz2rgb_coeffs(struct mp_csp_params *params,
                                  enum mp_render_intent intent, struct mp_cmat *m)
{
    struct mp_csp_primaries prim = mp_get_csp_primaries(params->color.primaries);
    float brightness = params->brightness;
    mp_get_rgb2xyz_matrix(prim, m->m);
    mp_invert_matrix3x3(m->m);

    // All non-absolute mappings want to map source white to target white
    if (intent != MP_INTENT_ABSOLUTE_COLORIMETRIC) {
        // SMPTE 428-1 defines the calibration white point as CIE xy (0.314, 0.351)
        static const struct mp_csp_col_xy smpte428 = {0.314, 0.351};
        mp_apply_chromatic_adaptation(smpte428, prim.white, m->m);
    }

    // Since this outputs linear RGB rather than companded RGB, we
    // want to linearize any brightness additions. 2 is a reasonable
    // approximation for any sort of gamma function that could be in use.
    // As this is an aesthetic setting only, any exact values do not matter.
    brightness *= fabs(brightness);

    for (int i = 0; i < 3; i++)
        m->c[i] = brightness;
}

// Get multiplication factor required if image data is fit within the LSBs of a
// higher smaller bit depth isfixed-point texture data.
double mp_get_csp_mul(enum mp_csp csp, int input_bits, int texture_bits)
{
    assert(texture_bits >= input_bits);

    // Convenience for some irrelevant cases, e.g. rgb565 or disabling expansion.
    if (!input_bits)
        return 1;

    // RGB always uses the full range available.
    if (csp == MP_CSP_RGB)
        return ((1LL << input_bits) - 1.) / ((1LL << texture_bits) - 1.);

    if (csp == MP_CSP_XYZ)
        return 1;

    // High bit depth YUV uses a range shifted from 8 bit.
    return (1LL << input_bits) / ((1LL << texture_bits) - 1.) * 255 / 256;
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
static void luma_coeffs(struct mp_cmat *mat, float lr, float lg, float lb)
{
    assert(fabs(lr+lg+lb - 1) < 1e-6);
    *mat = (struct mp_cmat) {
        { {1, 0,                    2 * (1-lr)          },
          {1, -2 * (1-lb) * lb/lg, -2 * (1-lr) * lr/lg  },
          {1,  2 * (1-lb),          0                   } },
        // Constant coefficients (mat->c) not set here
    };
}

// get the coefficients of the yuv -> rgb conversion matrix
void mp_get_csp_matrix(struct mp_csp_params *params, struct mp_cmat *m)
{
    enum mp_csp colorspace = params->color.space;
    if (colorspace <= MP_CSP_AUTO || colorspace >= MP_CSP_COUNT)
        colorspace = MP_CSP_BT_601;
    enum mp_csp_levels levels_in = params->color.levels;
    if (levels_in <= MP_CSP_LEVELS_AUTO || levels_in >= MP_CSP_LEVELS_COUNT)
        levels_in = MP_CSP_LEVELS_TV;

    switch (colorspace) {
    case MP_CSP_BT_601:     luma_coeffs(m, 0.299,  0.587,  0.114 ); break;
    case MP_CSP_BT_709:     luma_coeffs(m, 0.2126, 0.7152, 0.0722); break;
    case MP_CSP_SMPTE_240M: luma_coeffs(m, 0.2122, 0.7013, 0.0865); break;
    case MP_CSP_BT_2020_NC: luma_coeffs(m, 0.2627, 0.6780, 0.0593); break;
    case MP_CSP_BT_2020_C: {
        // Note: This outputs into the [-0.5,0.5] range for chroma information.
        // If this clips on any VO, a constant 0.5 coefficient can be added
        // to the chroma channels to normalize them into [0,1]. This is not
        // currently needed by anything, though.
        *m = (struct mp_cmat){{{0, 0, 1}, {1, 0, 0}, {0, 1, 0}}};
        break;
    }
    case MP_CSP_RGB: {
        *m = (struct mp_cmat){{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
        levels_in = -1;
        break;
    }
    case MP_CSP_XYZ: {
        // The vo should probably not be using a matrix generated by this
        // function for XYZ sources, but if it does, let's just assume it
        // wants BT.709 with D65 white point (virtually all other content).
        mp_get_xyz2rgb_coeffs(params, MP_INTENT_RELATIVE_COLORIMETRIC, m);
        levels_in = -1;
        break;
    }
    case MP_CSP_YCGCO: {
        *m = (struct mp_cmat) {
            {{1,  -1,  1},
             {1,   1,  0},
             {1,  -1, -1}},
        };
        break;
    }
    default:
        abort();
    };

    if ((colorspace == MP_CSP_BT_601 || colorspace == MP_CSP_BT_709 ||
         colorspace == MP_CSP_SMPTE_240M || colorspace == MP_CSP_BT_2020_NC))
    {
        // Hue is equivalent to rotating input [U, V] subvector around the origin.
        // Saturation scales [U, V].
        float huecos = params->gray ? 0 : params->saturation * cos(params->hue);
        float huesin = params->gray ? 0 : params->saturation * sin(params->hue);
        for (int i = 0; i < 3; i++) {
            float u = m->m[i][1], v = m->m[i][2];
            m->m[i][1] = huecos * u - huesin * v;
            m->m[i][2] = huesin * u + huecos * v;
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
    case MP_CSP_LEVELS_TV: yuvlev = yuvlim; break;
    case MP_CSP_LEVELS_PC: yuvlev = yuvfull; break;
    case -1: yuvlev = anyfull; break;
    default:
        abort();
    }

    int levels_out = params->levels_out;
    if (levels_out <= MP_CSP_LEVELS_AUTO || levels_out >= MP_CSP_LEVELS_COUNT)
        levels_out = MP_CSP_LEVELS_PC;
    struct rgblevels { double min, max; }
        rgblim =  { 16/255., 235/255. },
        rgbfull = {      0,        1  },
        rgblev;
    switch (levels_out) {
    case MP_CSP_LEVELS_TV: rgblev = rgblim; break;
    case MP_CSP_LEVELS_PC: rgblev = rgbfull; break;
    default:
        abort();
    }

    double ymul = (rgblev.max - rgblev.min) / (yuvlev.ymax - yuvlev.ymin);
    double cmul = (rgblev.max - rgblev.min) / (yuvlev.cmax - yuvlev.cmid) / 2;

    // Contrast scales the output value range (gain)
    ymul *= params->contrast;
    cmul *= params->contrast;

    for (int i = 0; i < 3; i++) {
        m->m[i][0] *= ymul;
        m->m[i][1] *= cmul;
        m->m[i][2] *= cmul;
        // Set c so that Y=umin,UV=cmid maps to RGB=min (black to black),
        // also add brightness offset (black lift)
        m->c[i] = rgblev.min - m->m[i][0] * yuvlev.ymin
                  - (m->m[i][1] + m->m[i][2]) * yuvlev.cmid
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

bool mp_colorspace_equal(struct mp_colorspace c1, struct mp_colorspace c2)
{
    return c1.space == c2.space &&
           c1.levels == c2.levels &&
           c1.primaries == c2.primaries &&
           c1.gamma == c2.gamma &&
           c1.light == c2.light &&
           c1.sig_peak == c2.sig_peak;
}

#define OPT_BASE_STRUCT struct mp_csp_equalizer_opts

const struct m_sub_options mp_csp_equalizer_conf = {
    .opts = (const m_option_t[]) {
        OPT_INTRANGE("brightness", values[MP_CSP_EQ_BRIGHTNESS], 0, -100, 100),
        OPT_INTRANGE("saturation", values[MP_CSP_EQ_SATURATION], 0, -100, 100),
        OPT_INTRANGE("contrast", values[MP_CSP_EQ_CONTRAST], 0, -100, 100),
        OPT_INTRANGE("hue", values[MP_CSP_EQ_HUE], 0, -100, 100),
        OPT_INTRANGE("gamma", values[MP_CSP_EQ_GAMMA], 0, -100, 100),
        OPT_CHOICE_C("video-output-levels", values[MP_CSP_EQ_OUTPUT_LEVELS], 0,
                     mp_csp_levels_names),
        {0}
    },
    .size = sizeof(struct mp_csp_equalizer_opts),
};

// Copy settings from eq into params.
void mp_csp_copy_equalizer_values(struct mp_csp_params *params,
                                  const struct mp_csp_equalizer_opts *eq)
{
    params->brightness = eq->values[MP_CSP_EQ_BRIGHTNESS] / 100.0;
    params->contrast = (eq->values[MP_CSP_EQ_CONTRAST] + 100) / 100.0;
    params->hue = eq->values[MP_CSP_EQ_HUE] / 100.0 * M_PI;
    params->saturation = (eq->values[MP_CSP_EQ_SATURATION] + 100) / 100.0;
    params->gamma = exp(log(8.0) * eq->values[MP_CSP_EQ_GAMMA] / 100.0);
    params->levels_out = eq->values[MP_CSP_EQ_OUTPUT_LEVELS];
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

void mp_invert_cmat(struct mp_cmat *out, struct mp_cmat *in)
{
    *out = *in;
    mp_invert_matrix3x3(out->m);

    // fix the constant coefficient
    // rgb = M * yuv + C
    // M^-1 * rgb = yuv + M^-1 * C
    // yuv = M^-1 * rgb - M^-1 * C
    //                  ^^^^^^^^^^
    out->c[0] = -(out->m[0][0] * in->c[0] + out->m[0][1] * in->c[1] + out->m[0][2] * in->c[2]);
    out->c[1] = -(out->m[1][0] * in->c[0] + out->m[1][1] * in->c[1] + out->m[1][2] * in->c[2]);
    out->c[2] = -(out->m[2][0] * in->c[0] + out->m[2][1] * in->c[1] + out->m[2][2] * in->c[2]);
}

// Multiply the color in c with the given matrix.
// i/o is {R, G, B} or {Y, U, V} (depending on input/output and matrix), using
// a fixed point representation with the given number of bits (so for bits==8,
// [0,255] maps to [0,1]). The output is clipped to the range as needed.
void mp_map_fixp_color(struct mp_cmat *matrix, int ibits, int in[3],
                                               int obits, int out[3])
{
    for (int i = 0; i < 3; i++) {
        double val = matrix->c[i];
        for (int x = 0; x < 3; x++)
            val += matrix->m[i][x] * in[x] / ((1 << ibits) - 1);
        int ival = lrint(val * ((1 << obits) - 1));
        out[i] = av_clip(ival, 0, (1 << obits) - 1);
    }
}
