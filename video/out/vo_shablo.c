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

#include <stdio.h>
#include <unistd.h>
#include <config.h>

#if HAVE_POSIX
#include <sys/ioctl.h>
#endif

#include <libswscale/swscale.h>

#include "options/m_config.h"
#include "config.h"
#include "vo.h"
#include "sub/osd.h"
#include "video/sws_utils.h"
#include "video/mp_image.h"

#define IMGFMT IMGFMT_BGR24

#define COLOR_PALETTE_PRESET_VGA 0
#define COLOR_PALETTE_PRESET_CMD 1
#define COLOR_PALETTE_PRESET_TERMINALAPP 2
#define COLOR_PALETTE_PRESET_PUTTY 3
#define COLOR_PALETTE_PRESET_MIRC 4
#define COLOR_PALETTE_PRESET_XTERM 5
#define COLOR_PALETTE_PRESET_HUMAN 6
#define COLOR_PALETTE_PRESETS 7

#define DITHERING_NONE 0
#define DITHERING_FS 1
#define DITHERING_FS_WIDTH_EXT 1
#define DITHERING_FS_HEIGHT_EXT 1
#define DITHERING_JJN 2
#define DITHERING_JJN_WIDTH_EXT 2
#define DITHERING_JJN_HEIGHT_EXT 2

#define ESC_HIDE_CURSOR "\e[?25l"
#define ESC_RESTORE_CURSOR "\e[?25h"
#define ESC_CLEAR_SCREEN "\e[2J"
#define ESC_CLEAR_COLORS "\e[0m"
#define ESC_GOTOXY "\e[%d;%df"
#define ESC_COLOR8_FG "\e[3%cm"
#define ESC_COLOR8_BG "\e[4%cm"
#define ESC_COLOREXT8_FG "\e[9%cm"
#define ESC_COLOREXT8_BG "\e[10%cm"

#define DEFAULT_WIDTH 80
#define DEFAULT_HEIGHT 25

#define DEFAULT_BLOCK_WIDTH 8
#define DEFAULT_BLOCK_HEIGHT 16

#define SHADES 5
#define MAX_SHADE_INDEX (SHADES - 1)
#define SHADE_ADDEND (MAX_SHADE_INDEX >> 1)

#define CH_R 0
#define CH_G 1
#define CH_B 2
#define COLOR_CHANNELS 3

#define BASE_PALETTE_SIZE 8
#define EXT_PALETTE_SIZE 16

#define COLOR_DEPTH 8
#define COLOR_DEPTH_SIZE (1 << COLOR_DEPTH)
#define COLOR_DEPTH_MASK (COLOR_DEPTH_SIZE - 1)

struct vo_shablo_opts {
    int fg_ext;
    int bg_ext;
    int dithering;
    int color_palette_preset;
    int lazy;
    int block_width;
    int block_height;
    int width;   // 0 -> default
    int height;  // 0 -> default
};

#define OPT_BASE_STRUCT struct vo_shablo_opts
static const struct m_sub_options vo_shablo_conf = {
    .opts = (const m_option_t[]) {
        OPT_FLAG("vo-shablo-lazy", lazy, 0),
        OPT_CHOICE("vo-shablo-color-palette-preset", color_palette_preset, 0,
                   ({"vga", COLOR_PALETTE_PRESET_VGA},
                    {"cmd", COLOR_PALETTE_PRESET_CMD},
                    {"termapp", COLOR_PALETTE_PRESET_TERMINALAPP},
                    {"putty", COLOR_PALETTE_PRESET_PUTTY},
                    {"mirc", COLOR_PALETTE_PRESET_MIRC},
                    {"xterm", COLOR_PALETTE_PRESET_XTERM},
                    {"human", COLOR_PALETTE_PRESET_HUMAN})),
        OPT_FLAG("vo-shablo-fg-ext", fg_ext, 0),
        OPT_FLAG("vo-shablo-bg-ext", bg_ext, 0),
        OPT_CHOICE("vo-shablo-dithering", dithering, 0,
                   ({"", DITHERING_NONE},
                    {"none", DITHERING_NONE},
                    {"fs", DITHERING_FS},
                    {"jjn", DITHERING_JJN})),
        OPT_INT("vo-shablo-block-width", block_width, 0),
        OPT_INT("vo-shablo-block-height", block_height, 0),
        OPT_INT("vo-shablo-width", width, 0),
        OPT_INT("vo-shablo-height", height, 0),
        {0}
    },
    .defaults = &(const struct vo_shablo_opts) {
        .fg_ext = false,
        .bg_ext = false,
        .dithering = DITHERING_NONE,
        .color_palette_preset = COLOR_PALETTE_PRESET_VGA,
        .lazy = true,
        .block_width = DEFAULT_BLOCK_WIDTH,
        .block_height = DEFAULT_BLOCK_HEIGHT,
    },
    .size = sizeof(struct vo_shablo_opts),
};

struct priv {
    struct vo_shablo_opts *opts;
    size_t buffer_size;
    char *buffer;
    int swidth;
    int sheight;
    struct mp_image *frame;
    struct mp_rect src;
    struct mp_rect dst;
    struct mp_sws_context *sws;
};

/* shade characters */
static char* SHADE_CHARS[SHADES] =
{
    " ", "\xe2\x96\x91", "\xe2\x96\x92", "\xe2\x96\x93", "\xe2\x96\x88"
};

/* predefined color palettes */
static uint32_t BASE_COLORS[COLOR_PALETTE_PRESETS][EXT_PALETTE_SIZE] =
{
    /* VGA */
    {
        0x000000, 0xaa0000, 0x00aa00, 0xaa5500,
        0x0000aa, 0xaa00aa, 0x00aaaa, 0xaaaaaa,
        0x555555, 0xff5555, 0x55ff55, 0xffff55,
        0x5555ff, 0xff55ff, 0x55ffff, 0xffffff
    },
    /* CMD */
    {
        0x000000, 0x800000, 0x008000, 0x808000,
        0x000080, 0x800080, 0x008080, 0xc0c0c0,
        0x808080, 0xff0000, 0x00ff00, 0xffff00,
        0x0000ff, 0xff00ff, 0x00ffff, 0xffffff
    },
    /* Terminal.app */
    {
        0x000000, 0xc23621, 0x25bc26, 0xadad27,
        0x492ee1, 0xd338d3, 0x33bbc8, 0xcbcccd,
        0x818383, 0xfc391f, 0x31e722, 0xeaec23,
        0x5833ff, 0xf935f8, 0x14f0f0, 0xe9ebeb
    },
    /* PuTTY */
    {
        0x000000, 0xbb0000, 0x00bb00, 0xbbbb00,
        0x0000bb, 0xbb00bb, 0x00bbbb, 0xbbbbbb,
        0x555555, 0xff5555, 0x55ff55, 0xffff55,
        0x5555ff, 0xff55ff, 0x55ffff, 0xffffff
    },
    /* mIRC */
    {
        0x000000, 0x7f0000, 0x009300, 0xfc7f00,
        0x00007f, 0x9c009c, 0x009393, 0xd2d2d2,
        0x7f7f7f, 0xff0000, 0x00fc00, 0xffff00,
        0x0000fc, 0xff00ff, 0x00ffff, 0xffffff
    },
    /* xterm */
    {
        0x000000, 0xcd0000, 0x00cd00, 0xcdcd00,
        0x0000ee, 0xcd00cd, 0x00cdcd, 0xe5e5e5,
        0x7f7f7f, 0xff0000, 0x00ff00, 0xffff00,
        0x5c5cff, 0xff00ff, 0x00ffff, 0xffffff
    },
    /* human */
    {
        0x010101, 0xde382b, 0x39b54a, 0xffc706,
        0x006fb8, 0x762671, 0x2cb5e9, 0xcccccc,
        0x808080, 0xff0000, 0x00ff00, 0xffff00,
        0x0000ff, 0xff00ff, 0x00ffff, 0xffffff
    }
};

/* used colors */

// number of available foreground colors
static size_t fg_colors;

// number of available background colors
static size_t bg_colors;

// index of preset color palette
static size_t color_palette_preset;

/* maps between true colors and emulated colors */

// what emulated colors look like in RGB24
static uint8_t* fg_bg_sh_ch_2_intensity_map = NULL;

// the number of ((fg, bg, sh) -> *) entries in that map
static size_t fg_bg_sh_ch_2_intensity_map_size;

// how RGB24 colors can be emulated via (fg, bg, sh)
static uint16_t* r_g_b_2_shfgbg_map = NULL;

/* reduced palette for optimization */

// (fg, bg, sh) entries that are sufficient
// to represent the whole fg_bg_sh_ch_2_intensity_map
static uint16_t* reduced_fg_bg_sh_palette_indices = NULL;

// the number N of entries in that palette;
// 0 < N < fg_bg_sh_ch_2_intensity_map_size
static size_t reduced_fg_bg_sh_palette_indices_size;

// the lab values (L*, a*, b*) of the reduced palette
static double* reduced_fg_bg_sh_palette_lab = NULL;

static double min_double(double a, double b)
{
    return a < b ? a : b;
}

static double max_double(double a, double b)
{
    return a > b ? a : b;
}

static uint8_t* lookup_fg_bg_sh_2_ch_intensity_map(
    uint8_t fg, uint8_t bg, uint8_t sh)
{
    return fg_bg_sh_ch_2_intensity_map + (COLOR_CHANNELS * (sh + SHADES *
        (bg + bg_colors * (fg))));
}

static uint16_t fg_bg_sh_2_shfgbg(uint8_t fg, uint8_t bg, uint8_t sh)
{
    return ((uint16_t) sh << 8) | ((uint16_t) fg << 4) | ((uint16_t) bg);
}

static void shfgbg_2_fg_bg_sh(uint16_t shfgbg,
    uint8_t* fg, uint8_t* bg, uint8_t* sh)
{
    *bg = shfgbg & 0xf;
    *fg = (shfgbg >> 4) & 0xf;
    *sh = (shfgbg >> 8) & 0x7;
}

static uint16_t lookup_r_g_b_2_shfgbg_map(
    uint8_t r, uint8_t g, uint8_t b)
{
    size_t ir = (size_t) r;
    size_t ig = (size_t) g;
    size_t ib = (size_t) b;
    return r_g_b_2_shfgbg_map[ib + COLOR_DEPTH_SIZE * (ig + COLOR_DEPTH_SIZE * (ir))];
}

static void set_r_g_b_2_shfgbg(uint8_t r, uint8_t g, uint8_t b,
    uint16_t shfgbg)
{
    size_t ir = (size_t) r;
    size_t ig = (size_t) g;
    size_t ib = (size_t) b;
    r_g_b_2_shfgbg_map[ib + COLOR_DEPTH_SIZE * (ig + COLOR_DEPTH_SIZE * (ir))] = shfgbg;
}

static void rgb_2_r_g_b(uint32_t rgb,
    uint8_t* r, uint8_t* g, uint8_t* b)
{
    *r = (rgb >> 16) & 0xff;
    *g = (rgb >> 8) & 0xff;
    *b = rgb & 0xff;
}

static uint32_t color_idx_2_rgb(uint8_t color_idx)
{
    return BASE_COLORS[color_palette_preset][color_idx];
}

static void color_idx_2_r_g_b(uint8_t color_idx,
    uint8_t* r, uint8_t* g, uint8_t* b)
{
    rgb_2_r_g_b(color_idx_2_rgb(color_idx), r, g, b);
}

#define A_CONST 0.055
#define EXPONENT 2.4
static double exp_2_lin_color(double exp_color) {
    if (exp_color <= 0.04045) {
        return exp_color / 12.92;
    } else {
        return pow((exp_color + A_CONST) / (1.0 + A_CONST), EXPONENT);
    }
}

static double lin_2_exp_color(double lin_color) {
    if (lin_color <= 0.0031308) {
        return 12.92 * lin_color;
    } else {
        return (1.0 + A_CONST) * pow(lin_color, 1.0/EXPONENT) - A_CONST;
    }
}
#undef EXPONENT
#undef A_CONST

static double c8_2_cexp(uint8_t c8) {
    return c8 / 255.0;
}

static uint8_t cexp_2_c8(double cexp) {
    return (uint8_t) max_double(min_double(cexp * 255.0, 255.0), 0.0);
}

// Calculates the map ((fg, bg, sh, ch) -> intensity).
// The question to answer here is: What does (fg, bg, sh) look like,
//     expressed as an RGB24 color?
// Precondition: fg_colors and bg_colors must be initialized
static uint8_t* calc_fg_bg_sh_ch_2_intensity_map(void)
{
    uint8_t *result = calloc(fg_bg_sh_ch_2_intensity_map_size,
        COLOR_CHANNELS * sizeof(uint8_t));
    if (result == NULL) {
        return NULL;
    }

    uint8_t fg_rgb[COLOR_CHANNELS] = {0, 0, 0};
    uint8_t bg_rgb[COLOR_CHANNELS] = {0, 0, 0};

    uint8_t* head = result;
    for (uint8_t fg_idx = 0; fg_idx < fg_colors; ++fg_idx) {
        color_idx_2_r_g_b(fg_idx,
            &fg_rgb[CH_R], &fg_rgb[CH_G], &fg_rgb[CH_B]);
        for (uint8_t bg_idx = 0; bg_idx < bg_colors; ++bg_idx) {
            color_idx_2_r_g_b(bg_idx,
                &bg_rgb[CH_R], &bg_rgb[CH_G], &bg_rgb[CH_B]);
            for (uint8_t sh_idx = 0; sh_idx < SHADES; ++sh_idx) {
                for (uint8_t chan = 0; chan < COLOR_CHANNELS; ++chan) {
                    // add background color and foreground color parts
                    double fg_lin = exp_2_lin_color(c8_2_cexp(fg_rgb[chan]));
                    double bg_lin = exp_2_lin_color(c8_2_cexp(bg_rgb[chan]));
                    double fg_part = (fg_lin * sh_idx);
                    double bg_part = (bg_lin * (MAX_SHADE_INDEX - sh_idx));
                    double val_lin = (fg_part + bg_part) / MAX_SHADE_INDEX;
                    double val_exp = lin_2_exp_color(val_lin);
                    uint8_t value = cexp_2_c8(val_exp);
                    *head++ = value;
                }
            }
        }
    }
    return result;
}

// Returns the squared Euclidean distance between (a1, b1, c1) and (a2, b2, c2).
static double get_squared_distance(double a1, double b1, double c1,
    double a2, double b2, double c2)
{
    double da = a2 - a1;
    double db = b2 - b1;
    double dc = c2 - c1;
    return da * da + db * db + dc * dc;
}

// transforms RGB24 color to linear RGB color
static void r_g_b_2_linear_r_g_b(uint8_t r, uint8_t g, uint8_t b,
    double* r_lin, double* g_lin, double* b_lin)
{
    *r_lin = exp_2_lin_color(c8_2_cexp(r));
    *g_lin = exp_2_lin_color(c8_2_cexp(g));
    *b_lin = exp_2_lin_color(c8_2_cexp(b));
}

// transforms linear RGB color to XYZ color
static void linear_r_g_b_2_x_y_z(double r, double g, double b,
    double* x, double* y, double* z)
{
    double rc = (double) r * 100.0;
    double gc = (double) g * 100.0;
    double bc = (double) b * 100.0;

    // RGB to XYZ
    *x = 0.4124564 * rc + 0.3575761 * gc + 0.1804375 * bc;
    *y = 0.2126729 * rc + 0.7151522 * gc + 0.0721750 * bc;
    *z = 0.0193339 * rc + 0.1191920 * gc + 0.9503041 * bc;
}

#define X_N_D65_2DEG 95.047
#define Y_N_D65_2DEG 100.0
#define Z_N_D65_2DEG 108.883

// helper function to calculate cbrt(p/p_n) for CIELAB
static double lab_cbrt(double p, double p_n)
{
    double ppn = p / p_n;
    return ppn < (216.0 / 24389.0) ?
        ((1.0 / 116.0) * (24389.0 / 27.0 * p / p_n + 16.0)) :
        cbrt(ppn);
}

// transforms color from CIEXYZ (CIE 1931 color space) to CIELAB,
// for (D65, 2 degrees)
static void x_y_z_2_l_a_b(double x, double y, double z,
    double* l, double* a, double* b)
{
    double cbrt_x = lab_cbrt(x, X_N_D65_2DEG);
    double cbrt_y = lab_cbrt(y, Y_N_D65_2DEG);
    double cbrt_z = lab_cbrt(z, Z_N_D65_2DEG);

    *l = 116.0 * cbrt_y - 16.0;
    *a = 500.0 * (cbrt_x - cbrt_y);
    *b = 200.0 * (cbrt_y - cbrt_z);
}

// transforms color from RGB24 to CIELAB, for (D65, 2 degrees)
static void r_g_b_2_l_a_b(uint8_t r, uint8_t g, uint8_t b,
    double* ll, double* aa, double* bb)
{
    double r_lin, g_lin, b_lin;
    r_g_b_2_linear_r_g_b(r, g, b, &r_lin, &g_lin, &b_lin);
    double x, y, z;
    linear_r_g_b_2_x_y_z(r_lin, g_lin, b_lin, &x, &y, &z);
    x_y_z_2_l_a_b(x, y, z, ll, aa, bb);
}

// RGB24 to YUV: Y = 0.299 * R + 0.587 * G + 0.114 * B
static double r_g_b_2_y(uint8_t r, uint8_t g, uint8_t b)
{
    double r_lin, g_lin, b_lin;
    r_g_b_2_linear_r_g_b(r, g, b, &r_lin, &g_lin, &b_lin);
    return 0.299 * r_lin + 0.587 * g_lin + 0.114 * b_lin;
}

static double get_y_dist_between_index_colors(
    uint8_t color_idx1, uint8_t color_idx2)
{
    uint8_t r1, g1, b1;
    color_idx_2_r_g_b(color_idx1, &r1, &g1, &b1);
    double y1 = r_g_b_2_y(r1, g1, b1);

    uint8_t r2, g2, b2;
    color_idx_2_r_g_b(color_idx2, &r2, &g2, &b2);
    double y2 = r_g_b_2_y(r2, g2, b2);

    return fabs(y2 - y1);
}

static bool is_gray(uint8_t r, uint8_t g, uint8_t b)
{
    return r == g && g == b;
}

// Evaluates which one of two color emulations (c1, c2) is nicer to emulate
//     a given color c (true -> c1, false -> c2).
// Roughly said, a color emulation is nicer if foreground color and
// background color are more near to c than the other color emulation.
// Both colors c1, c2 must be given by their (fg, bg, sh) values.
// c must be given in (r, g, b).
// Precondition: c1 and c2 do both emulate c.
static bool is_nicer_than(uint8_t r, uint8_t g, uint8_t b,
    uint8_t fg1, uint8_t bg1, uint8_t sh1,
    uint8_t fg2, uint8_t bg2, uint8_t sh2)
{
    // plain colors
    if (sh1 == 0 && sh2 != 0) {
        return true;
    }
    if (sh2 == 0 && sh1 != 0) {
        return false;
    }
    if (sh2 == MAX_SHADE_INDEX) {
        return false;
    }
    if (sh1 == MAX_SHADE_INDEX) {
        return true;
    }

    // check gray colors
    if (is_gray(r, g, b)) {
        // it's about gray.
        // gray is better than color mix
        uint8_t r1, g1, b1;
        color_idx_2_r_g_b(fg1, &r1, &g1, &b1);
        bool gray1 = is_gray(r1, g1, b1);

        uint8_t r2, g2, b2;
        color_idx_2_r_g_b(fg2, &r2, &g2, &b2);
        bool gray2 = is_gray(r2, g2, b2);

        if (gray1 && !gray2) {
            return true;
        }
        if (gray2 && !gray1) {
            return false;
        }
    }

    double d1 = get_y_dist_between_index_colors(fg1, bg1);
    double d2 = get_y_dist_between_index_colors(fg2, bg2);
    return d1 < d2;
}

// Helper function for calc_reduced_emulated_color_palette
// Check
//    - (1) if this color is in the reduced emulation color palette then
//          take the nicer one and
//    - (2) if it is not in the reduced emulation color palette then add it.
static void calc_reduced_emulated_color_palette_helper(
    uint8_t fg_idx, uint8_t bg_idx, uint8_t sh_idx,
    uint8_t from_r, uint8_t from_g, uint8_t from_b,
    size_t* current_palette_size, uint16_t* current_palette)
{
    bool not_yet_in_the_list = true;

    // go through all colors in the current palette
    for (size_t to_idx = 0;
        to_idx < *current_palette_size;
        ++to_idx, ++current_palette)
    {
        // get color
        uint16_t shfgbg = *current_palette;

        // get color's (fg, bg, sh) and (r, g, b)
        uint8_t fg, bg, sh;
        shfgbg_2_fg_bg_sh(shfgbg, &fg, &bg, &sh);
        uint8_t* rgb = lookup_fg_bg_sh_2_ch_intensity_map(fg, bg, sh);
        uint8_t r = *rgb++;
        uint8_t g = *rgb++;
        uint8_t b = *rgb++;

        // check current color (r, g, b)<=>(fg, bg, sh)
        // against given color (from_r, from_g, from_b)
        //                  <=>(fg_idx, bg_idx, sh_idx)
        if (r == from_r && g == from_g && b == from_b) {
            // they match.
            // check if the given color (fg_idx, bg_idx, sh_idx)
            // is nicer than the current color (fg, bg, sh)
            if (is_nicer_than(r, g, b, fg_idx, bg_idx, sh_idx, fg, bg, sh)) {
                // the given color (fg_idx, bg_idx, sh_idx) is nicer.
                // replace color
                *current_palette = fg_bg_sh_2_shfgbg(fg_idx, bg_idx, sh_idx);
            }

            // do not add this color since it's already in the palette
            not_yet_in_the_list = false;
            break;
        }
    }
    if (not_yet_in_the_list) {
        // the given color (from_r, from_g, from_b) is not yet in the palette.
        // add that color
        *current_palette = fg_bg_sh_2_shfgbg(fg_idx, bg_idx, sh_idx);
        (*current_palette_size)++;
    }
}

// Calculates a reduced color emulation palette.
// A color emulation is a combination of (fg, bg, sh) which emulates a color c.
// A color emulation duplicate c' is a color emulation (fg', bg', sh') which
//     emulates c, too.
// Color emulation duplicates will be eliminated whereas the nicer one will be
//     in the result.
//
// For example, the color emulation (fg=0, bg=1, sh=2) equals (fg=1, bg=0, sh=2)
//     and thus, only one of them is needed.
//
// Precondition:
//    fg_bg_sh_ch_2_intensity_map_size,
//    fg_bg_sh_ch_2_intensity_map,
//    fg_colors and
//    bg_colors must be initialized.
static void calc_reduced_emulated_color_palette(
    size_t* result_size, uint16_t** result)
{
    uint16_t* res = calloc(fg_bg_sh_ch_2_intensity_map_size, sizeof(uint16_t));
    if (res == NULL) {
        result = NULL;
        return;
    }
    size_t res_size = 0;

    // go through all emulated colors in the full palette
    uint8_t* from_head = fg_bg_sh_ch_2_intensity_map;
    for (uint8_t fg = 0; fg < fg_colors; ++fg) {
        for (uint8_t bg = 0; bg < bg_colors; ++bg) {
            for (uint8_t sh = 0; sh < SHADES; ++sh) {
                // get emulated color's (r, g, b)
                uint8_t from_r = *from_head++;
                uint8_t from_g = *from_head++;
                uint8_t from_b = *from_head++;

                // add color if new to the reduced palette
                calc_reduced_emulated_color_palette_helper(fg, bg, sh,
                    from_r, from_g, from_b, &res_size, res);
            }
        }
    }
    *result = res;
    *result_size = res_size;
}

// Calculates the CIELAB color values of the emulated color palette.
static double* reduced_emulated_color_palette_to_lab(void)
{
    double* res = calloc(reduced_fg_bg_sh_palette_indices_size,
        sizeof(double) * COLOR_CHANNELS);
    if (res == NULL) {
        return NULL;
    }

    double* to_head = res;
    uint16_t* from_head = reduced_fg_bg_sh_palette_indices;
    for (size_t i = 0; i < reduced_fg_bg_sh_palette_indices_size; ++i) {
        uint16_t sh_fg_bg = *from_head++;
        uint8_t fg, bg, sh;
        shfgbg_2_fg_bg_sh(sh_fg_bg, &fg, &bg, &sh);
        uint8_t* rgb = lookup_fg_bg_sh_2_ch_intensity_map(fg, bg, sh);
        uint8_t r = *rgb++;
        uint8_t g = *rgb++;
        uint8_t b = *rgb++;
        double ll, aa, bb;
        r_g_b_2_l_a_b(r, g, b, &ll, &aa, &bb);
        *to_head++ = ll;
        *to_head++ = aa;
        *to_head++ = bb;
    }

    return res;
}

// Calculates (rgb -> shfgbg) by approximating the RGB24 color
// using nearest neighbour applied in CIELAB color space.
static void r_g_b_2_nearest_fg_bg_sh(uint8_t r, uint8_t g, uint8_t b,
    uint8_t* best_fg, uint8_t* best_bg, uint8_t* best_sh)
{
    // calc result by brute force
    *best_fg = 0;
    *best_bg = 0;
    *best_sh = 0;
    double ll1, aa1, bb1;
    r_g_b_2_l_a_b(r, g, b, &ll1, &aa1, &bb1);
    double best_dist = 1e50;
    bool is_first = true;
    uint16_t* from = reduced_fg_bg_sh_palette_indices;
    double* from_lab = reduced_fg_bg_sh_palette_lab;
    // check all available colors for *best match
    for (size_t sh_fg_bg_idx = 0;
        sh_fg_bg_idx < reduced_fg_bg_sh_palette_indices_size;
        ++sh_fg_bg_idx)
    {
        double ll2 = *from_lab++;
        double aa2 = *from_lab++;
        double bb2 = *from_lab++;
        uint16_t sh_fg_bg = *from++;
        double dist = get_squared_distance(ll1, aa1, bb1,
            ll2, aa2, bb2);
        // check if better color found
        if (best_dist > dist || is_first) {
            uint8_t fg, bg, sh;
            shfgbg_2_fg_bg_sh(sh_fg_bg, &fg, &bg, &sh);
            best_dist = dist;
            *best_fg = fg;
            *best_bg = bg;
            *best_sh = sh;
            is_first = false;
        }
    }
}

// Calculates the main lookup table (rgb -> shfgbg) by approximating
// the rgb color using nearest neighbour applied in CIELAB color space.
static uint16_t* calc_lookup_table(bool lazy)
{
    uint16_t* result = calloc(COLOR_DEPTH_SIZE * COLOR_DEPTH_SIZE * COLOR_DEPTH_SIZE,
        sizeof(uint16_t));
    if (result == NULL) {
        return NULL;
    }

    if (!lazy) {
        uint16_t* to_head = result;
        for (size_t r_idx = 0; r_idx < COLOR_DEPTH_SIZE; ++r_idx) {
            uint8_t r = (uint8_t) r_idx;
            for (size_t g_idx = 0; g_idx < COLOR_DEPTH_SIZE; ++g_idx) {
                uint8_t g = (uint8_t) g_idx;
                for (size_t b_idx = 0; b_idx < COLOR_DEPTH_SIZE; ++b_idx) {
                    uint8_t b = (uint8_t) b_idx;
                    // calc lookup table cell values by brute force
                    uint8_t best_fg = 0;
                    uint8_t best_bg = 0;
                    uint8_t best_sh = 0;
                    r_g_b_2_nearest_fg_bg_sh(r, g, b, &best_fg, &best_bg, &best_sh);
                    *to_head++ = 0x8000 | fg_bg_sh_2_shfgbg(best_fg, best_bg, best_sh);
                }
            }
        }
    }
    return result;
}

static void r_g_b_2_fg_bg_sh(uint8_t r, uint8_t g, uint8_t b,
    uint8_t* fg, uint8_t* bg, uint8_t* sh)
{
    uint16_t shfgbg = lookup_r_g_b_2_shfgbg_map(r, g, b);
    if ((shfgbg & 0x8000) == 0) {
        // lazy lookup cell calculation
        r_g_b_2_nearest_fg_bg_sh(r, g, b, fg, bg, sh);
        shfgbg = 0x8000 | fg_bg_sh_2_shfgbg(*fg, *bg, *sh);
        set_r_g_b_2_shfgbg(r, g, b, shfgbg);
    }
    shfgbg_2_fg_bg_sh(shfgbg, fg, bg, sh);
}

// Initializes color palettes.
static bool init(int new_color_palette_preset, bool lazy,
    bool light_fg_allowed, bool light_bg_allowed)
{
    if (r_g_b_2_shfgbg_map != NULL) {
        return true;
    }

    color_palette_preset = new_color_palette_preset;
    fg_colors = light_fg_allowed ? EXT_PALETTE_SIZE : BASE_PALETTE_SIZE;
    bg_colors = light_bg_allowed ? EXT_PALETTE_SIZE : BASE_PALETTE_SIZE;
    fg_bg_sh_ch_2_intensity_map_size = fg_colors * bg_colors * SHADES;

    uint8_t* temp = calc_fg_bg_sh_ch_2_intensity_map();
    if (temp == NULL) {
        fprintf(stderr, "[shablo]: Out of memory error.");
        return false;
    }
    fg_bg_sh_ch_2_intensity_map = temp;

    uint16_t* temp2 = NULL;
    size_t temp2_size = 0;
    calc_reduced_emulated_color_palette(&temp2_size, &temp2);
    if (temp2 == NULL) {
        fprintf(stderr, "[shablo]: Out of memory error.");
        return false;
    }
    reduced_fg_bg_sh_palette_indices = temp2;
    reduced_fg_bg_sh_palette_indices_size = temp2_size;

    double* temp3 = reduced_emulated_color_palette_to_lab();
    if (temp3 == NULL) {
        fprintf(stderr, "[shablo]: Out of memory error.");
        return false;
    }
    reduced_fg_bg_sh_palette_lab = temp3;

    uint16_t* temp4 = calc_lookup_table(lazy);
    if (temp4 == NULL) {
        fprintf(stderr, "[shablo]: Out of memory error.");
        return false;
    }
    r_g_b_2_shfgbg_map = temp4;

    return true;
}

// variables for error diffusion dithering
static double* color_error = NULL;
static size_t color_error_width = 0;
static size_t color_error_height = 0;
static size_t color_error_width_ext = 0;
static size_t color_error_height_ext = 0;
static double* color_error_head = NULL;
static size_t color_error_head_x = 0;
static size_t color_error_head_y = 0;

// Creates and/or resets the matrix containing the errors
//     used in error diffusion dithering.
static bool prepare_color_error_array(size_t width, size_t height,
    size_t width_ext, size_t height_ext)
{
    size_t size = COLOR_CHANNELS * (width + width_ext) * (height + height_ext);

    if (color_error != NULL && width == color_error_width &&
        height == color_error_height && color_error_width_ext == width_ext &&
        color_error_height_ext == height_ext)
    {
        memset(color_error, 0, size * sizeof(double));
        color_error_head = color_error;
        color_error_head_x = 0;
        color_error_head_y = 0;
        return true;
    }

    if (color_error != NULL) {
         free(color_error);
    }

    double* result = calloc(size, sizeof(double));
    if (result == NULL) {
        return false;
    }

    color_error_width = width;
    color_error_height = height;
    color_error_width_ext = width_ext;
    color_error_height_ext = height_ext;

    color_error = result;

    color_error_head = color_error;
    color_error_head_x = 0;
    color_error_head_y = 0;

    return true;
}

// Goes to the next entity for error diffusion dithering.
static void dithering_advance_head(void)
{
    double* advanced_head = color_error_head + COLOR_CHANNELS;

    // check edges
    if (++color_error_head_x >= color_error_width) {
        color_error_head_x = 0;
        advanced_head += color_error_width_ext * COLOR_CHANNELS;
        if (++color_error_head_y >= color_error_height) {
            color_error_head_y = 0;
            advanced_head = color_error;
        }
    }

    color_error_head = advanced_head;
}

static uint8_t dithering_pull_error_helper(uint8_t c, double error) {
    return cexp_2_c8(lin_2_exp_color(exp_2_lin_color(c8_2_cexp(c)) + error));
}

// Accumulates the error diffusion dithering error onto
//     call-by-reference RGB values.
static void dithering_pull_error(uint8_t* r, uint8_t* g, uint8_t* b)
{
    *r = dithering_pull_error_helper(*r, color_error_head[CH_R]);
    *g = dithering_pull_error_helper(*g, color_error_head[CH_G]);
    *b = dithering_pull_error_helper(*b, color_error_head[CH_B]);
}

static double dithering_calc_error(uint8_t origin, uint8_t effective) {
    return exp_2_lin_color(c8_2_cexp(origin)) - exp_2_lin_color(c8_2_cexp(effective));
}

// Little helper for Floyd&Steinberg dithering.
static double dithering_fs_helper(double e)
{
    return e / 16.0;
}

// Calculates the error for Floyd&Steinberg dithering and stores it
//     into the error matrix.
static void dithering_fs_push_error(
    uint8_t r_origin, uint8_t g_origin, uint8_t b_origin,
    uint8_t r_effective, uint8_t g_effective, uint8_t b_effective)
{
    double error[3] = {0.0, 0.0, 0.0};

    error[CH_R] = dithering_calc_error(r_origin, r_effective);
    error[CH_G] = dithering_calc_error(g_origin, g_effective);
    error[CH_B] = dithering_calc_error(b_origin, b_effective);

    double* next_head = color_error_head + COLOR_CHANNELS;
    // add error
    size_t line_size = (color_error_width - DITHERING_FS_WIDTH_EXT) *
        COLOR_CHANNELS;
    for (int ch = 0; ch < COLOR_CHANNELS; ++ch) {
        double e1 = error[ch];
        double e2 = e1 * 2.0;
        double e3 = e1 + e2;
        double e4 = e2 * 2.0;
        double e5 = e1 + e4;
        double e7 = e5 + e2;

        e1 = dithering_fs_helper(e1);
        e3 = dithering_fs_helper(e3);
        e5 = dithering_fs_helper(e5);
        e7 = dithering_fs_helper(e7);

        double* here = next_head + ch;
        *here += e7;
        here += line_size;
        *here += e3;
        here += COLOR_CHANNELS;
        *here += e5;
        here += COLOR_CHANNELS;
        *here += e1;
    }

    dithering_advance_head();
}

// Little helper for Jarvis&Judice&Ninke dithering.
static double dithering_jjn_helper(double e)
{
    return e / 48.0;
}

// Calculates the error for Jarvis&Judice&Ninke dithering and
// stores it into the error matrix.
static void dithering_jjn_push_error(
    uint8_t r_origin, uint8_t g_origin, uint8_t b_origin,
    uint8_t r_effective, uint8_t g_effective, uint8_t b_effective)
{
    double error[3] = {0.0, 0.0, 0.0};

    error[CH_R] = dithering_calc_error(r_origin, r_effective);
    error[CH_G] = dithering_calc_error(g_origin, g_effective);
    error[CH_B] = dithering_calc_error(b_origin, b_effective);

    double* next_head = color_error_head + COLOR_CHANNELS;
    // add error
    size_t line_size = (color_error_width - DITHERING_JJN_WIDTH_EXT) *
        COLOR_CHANNELS;
    for (int ch = 0; ch < COLOR_CHANNELS; ++ch) {
        double e1 = error[ch];
        double e2 = e1 * 2.0;
        double e3 = e1 + e2;
        double e4 = e2 * 2.0;
        double e5 = e1 + e4;
        double e7 = e5 + e2;

        e1 = dithering_jjn_helper(e1);
        e3 = dithering_jjn_helper(e3);
        e5 = dithering_jjn_helper(e5);
        e7 = dithering_jjn_helper(e7);

        double* here = next_head + ch;
        *here += e7;
        here += COLOR_CHANNELS;
        *here += e5;
        here += line_size;
        *here += e3;
        here += COLOR_CHANNELS;
        *here += e5;
        here += COLOR_CHANNELS;
        *here += e7;
        here += COLOR_CHANNELS;
        *here += e5;
        here += COLOR_CHANNELS;
        *here += e3;
        here += line_size;
        *here += e1;
        here += COLOR_CHANNELS;
        *here += e3;
        here += COLOR_CHANNELS;
        *here += e5;
        here += COLOR_CHANNELS;
        *here += e3;
        here += COLOR_CHANNELS;
        *here += e1;
    }

    dithering_advance_head();
}

// Writes the source image to stdout.
static void write_shaded(
    const int dwidth, const int dheight,
    const int swidth, const int sheight,
    const unsigned char *source, const int source_stride,
    const size_t dithering)
{
    assert(source);

    // prepare dithering
    bool ok = false;
    switch (dithering) {
    case DITHERING_NONE:
        ok = true;
        break;
    case DITHERING_FS:
        ok = prepare_color_error_array(swidth, sheight,
            DITHERING_FS_WIDTH_EXT, DITHERING_FS_HEIGHT_EXT);
        break;
    case DITHERING_JJN:
        ok = prepare_color_error_array(swidth, sheight,
            DITHERING_JJN_WIDTH_EXT, DITHERING_JJN_HEIGHT_EXT);
        break;
    }
    if (!ok) {
        return;
    }

    // the big loops
    const int tx = (dwidth - swidth) >> 1;
    const int ty = (dheight - sheight) >> 1;
    for (int y = 0; y < sheight; y++) {
        const unsigned char *row = source + y * source_stride;
        printf(ESC_GOTOXY, ty + y, tx);
        for (int x = 0; x < swidth; x++) {
            printf(ESC_CLEAR_COLORS);
            unsigned char b = *row++;
            unsigned char g = *row++;
            unsigned char r = *row++;

            // dithering.read
            uint8_t r_effective = r;
            uint8_t g_effective = g;
            uint8_t b_effective = b;
            if (dithering != DITHERING_NONE) {
                dithering_pull_error(&r_effective, &g_effective, &b_effective);
            }

            // deduce color emulation
            uint8_t fg_idx;
            uint8_t bg_idx;
            uint8_t sh_idx;
            r_g_b_2_fg_bg_sh(r_effective, g_effective, b_effective,
                &fg_idx, &bg_idx, &sh_idx);

            // dithering.write
            if (dithering != DITHERING_NONE) {
                uint8_t* effective = lookup_fg_bg_sh_2_ch_intensity_map(
                    fg_idx, bg_idx, sh_idx);
                r_effective = *effective++;
                g_effective = *effective++;
                b_effective = *effective++;
                switch (dithering) {
                case DITHERING_NONE:
                    break;
                case DITHERING_FS:
                    dithering_fs_push_error(r, g, b,
                        r_effective, g_effective, b_effective);
                    break;
                case DITHERING_JJN:
                    dithering_jjn_push_error(r, g, b,
                        r_effective, g_effective, b_effective);
                    break;
                }
            }

            // draw
            bool fg_light = (fg_idx & 0x8) != 0;
            bool bg_light = (bg_idx & 0x8) != 0;
            unsigned char fg = '0' | (fg_idx & 0x7);
            unsigned char bg = '0' | (bg_idx & 0x7);
            // draw bg color
            if (sh_idx < MAX_SHADE_INDEX) {
                if (bg_light) {
                    printf(ESC_COLOREXT8_BG, bg);
                } else {
                    printf(ESC_COLOR8_BG, bg);
                }
            }
            // draw fg color
            if (sh_idx > 0) {
                if (fg_light) {
                    printf(ESC_COLOREXT8_FG, fg);
                } else {
                    printf(ESC_COLOR8_FG, fg);
                }
            }
            // draw shade char
            printf("%s", SHADE_CHARS[sh_idx]);
        }
        printf(ESC_CLEAR_COLORS);
    }
    printf("\n");
}

static void get_win_size(struct vo *vo, int *out_width, int *out_height)
{
    struct priv *p = vo->priv;
    *out_width = DEFAULT_WIDTH;
    *out_height = DEFAULT_HEIGHT;
#if HAVE_POSIX
    struct winsize winsize;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsize) >= 0) {
        *out_width = winsize.ws_col;
        *out_height = winsize.ws_row;
    }
#endif

    if (p->opts->width > 0)
        *out_width = p->opts->width;
    if (p->opts->height > 0)
        *out_height = p->opts->height;
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *p = vo->priv;

    get_win_size(vo, &vo->dwidth, &vo->dheight);

    struct mp_osd_res osd;
    vo_get_src_dst_rects(vo, &p->src, &p->dst, &osd);
    p->swidth = p->dst.x1 - p->dst.x0;
    p->sheight = p->dst.y1 - p->dst.y0;

    if (p->buffer)
        free(p->buffer);

    mp_sws_set_from_cmdline(p->sws, vo->opts->sws_opts);
    p->sws->src = *params;
    p->sws->dst = (struct mp_image_params) {
        .imgfmt = IMGFMT,
        .w = p->swidth,
        .h = p->sheight,
        .p_w = 1,
        .p_h = 1,
    };

    p->frame = mp_image_alloc(IMGFMT, p->swidth, p->sheight);
    if (!p->frame)
        return -1;

    if (mp_sws_reinit(p->sws) < 0)
        return -1;

    if (!init(p->opts->color_palette_preset,p->opts->lazy,
            p->opts->fg_ext, p->opts->bg_ext))
        return -1;

    printf(ESC_HIDE_CURSOR);
    printf(ESC_CLEAR_SCREEN);
    vo->want_redraw = true;

    return 0;
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct priv *p = vo->priv;
    struct mp_image src = *mpi;
    // XXX: pan, crop etc.
    mp_sws_scale(p->sws, p->frame, &src);
    talloc_free(mpi);
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    write_shaded(
        vo->dwidth, vo->dheight, p->swidth, p->sheight,
        p->frame->planes[0], p->frame->stride[0],
        p->opts->dithering);
    fflush(stdout);
}

static void uninit(struct vo *vo)
{
    printf(ESC_RESTORE_CURSOR);
    printf(ESC_CLEAR_SCREEN);
    printf(ESC_GOTOXY, 0, 0);
    struct priv *p = vo->priv;
    if (p->buffer)
        talloc_free(p->buffer);
    if (p->sws)
        talloc_free(p->sws);
}

static int preinit(struct vo *vo)
{
    // most terminal characters aren't 1:1, so we default to 16:8.
    // if user passes their own value of choice, it'll be scaled accordingly.

    struct priv *p = vo->priv;
    p->opts = mp_get_config_group(vo, vo->global, &vo_shablo_conf);
    vo->monitor_par = (double) ((double) (vo->opts->monitor_pixel_aspect) *
        (double) p->opts->block_height / (double) p->opts->block_width);
    p->sws = mp_sws_alloc(vo);
    return 0;
}

static int query_format(struct vo *vo, int format)
{
    return format == IMGFMT;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    return VO_NOTIMPL;
}

const struct vo_driver video_out_shablo = {
    .name = "shablo",
    .description = "shaded blocks for ANSI-color terminals (experimental)",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
    .global_opts = &vo_shablo_conf,
};

