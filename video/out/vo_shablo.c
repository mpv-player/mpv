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
#define COLOR_PALETTE_PRESETS 1

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
#define MIN_COLOR_DEPTH 1
#define MAX_COLOR_DEPTH 8

struct vo_shablo_opts {
    int fg_ext;
    int bg_ext;
    int color_palette_preset;
    int color_depth; // for each channel
    int block_width;
    int block_height;
    int width;   // 0 -> default
    int height;  // 0 -> default
};

#define OPT_BASE_STRUCT struct vo_shablo_opts
static const struct m_sub_options vo_shablo_conf = {
    .opts = (const m_option_t[]) {
        OPT_INT("vo-shablo-color-depth-per-channel", color_depth, 0),
        OPT_CHOICE("vo-shablo-color-palette-preset", color_palette_preset, 0,
                   ({"vga", COLOR_PALETTE_PRESET_VGA})),
        OPT_FLAG("vo-shablo-fg-ext", fg_ext, 0),
        OPT_FLAG("vo-shablo-bg-ext", bg_ext, 0),
        OPT_INT("vo-shablo-block-width", block_width, 0),
        OPT_INT("vo-shablo-block-height", block_height, 0),
        OPT_INT("vo-shablo-width", width, 0),
        OPT_INT("vo-shablo-height", height, 0),
        {0}
    },
    .defaults = &(const struct vo_shablo_opts) {
        .color_palette_preset = COLOR_PALETTE_PRESET_VGA,
        .color_depth = 6,
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
static char* SHADE_CHARS[SHADES] = {" ", "\xe2\x96\x91", "\xe2\x96\x92", "\xe2\x96\x93", "\xe2\x96\x88"};

/* predefined color palettes */
static uint32_t BASE_COLORS[COLOR_PALETTE_PRESETS][EXT_PALETTE_SIZE] =
{
    /* VGA */
    {
        0x000000, 0xaa0000, 0x00aa00, 0xaa5500, 0x0000aa, 0xaa00aa, 0x00aaaa, 0xaaaaaa,
        0x555555, 0xff5555, 0x55ff55, 0xffff55, 0x5555ff, 0xff55ff, 0x55ffff, 0xffffff
    }
};

/* used colors */
static size_t fg_colors; // number of available foreground colors
static size_t bg_colors; // number of available background colors
static size_t color_palette_preset; // index of preset color palette

/* used for quick calculations during playback */
static uint8_t depth_mask;
static size_t depth_size;
static size_t depth_shift;
static uint8_t round_addend;

/* maps between true colors and emulated colors */
static uint8_t* fg_bg_sh_ch_2_intensity_map = NULL; // what emulated colors look like in RGB
static size_t fg_bg_sh_ch_2_intensity_map_size; // the number of ((fg, bg, sh) -> *) entries in that map

static uint16_t* r_g_b_2_shfgbg_map = NULL; // how RGB colors can be emulated via (fg, bg, sh)

static int min_int(int a, int b) {
    return a < b ? a : b;
}

static double min_double(double a, double b) {
    return a < b ? a : b;
}

static double max_double(double a, double b) {
    return a > b ? a : b;
}

static uint16_t fg_bg_sh_2_shfgbg(uint8_t fg, uint8_t bg, uint8_t sh) {
    return ((uint16_t) sh << 8) | ((uint16_t) fg << 4) | ((uint16_t) bg);
}

static void shfgbg_2_fg_bg_sh(uint16_t sh_fg_bg, uint8_t* fg, uint8_t* bg, uint8_t* sh) {
    *bg = sh_fg_bg & 0xf;
    *fg = (sh_fg_bg >> 4) & 0xf;
    *sh = (sh_fg_bg >> 8) & 0x7;
}

static uint16_t lookup_r_g_b_2_shfgbg_map(uint8_t red, uint8_t green, uint8_t blue) {
    int r = depth_mask & (min_int((int) red + round_addend, 0xff) >> depth_shift);
    int g = depth_mask & (min_int((int) green + round_addend, 0xff) >> depth_shift);
    int b = depth_mask & (min_int((int) blue + round_addend, 0xff) >> depth_shift);
    return r_g_b_2_shfgbg_map[b + depth_size * (g + depth_size * (r))];
}

static void r_g_b_2_fg_bg_sh(uint8_t r, uint8_t g, uint8_t b, uint8_t* fg, uint8_t* bg, uint8_t* sh) {
    uint16_t shfgbg = lookup_r_g_b_2_shfgbg_map(r, g, b);
    shfgbg_2_fg_bg_sh(shfgbg, fg, bg, sh);
}

static void rgb_2_r_g_b(uint32_t rgb, uint8_t* r, uint8_t* g, uint8_t* b) {
    *r = (rgb >> 16) & 0xff;
    *g = (rgb >> 8) & 0xff;
    *b = rgb & 0xff;
}

static uint32_t color_idx_2_rgb(uint8_t color_idx) {
    return BASE_COLORS[color_palette_preset][color_idx];
}

static void color_idx_2_r_g_b(uint8_t color_idx, uint8_t* r, uint8_t* g, uint8_t* b) {
    rgb_2_r_g_b(color_idx_2_rgb(color_idx), r, g, b);
}

// Calculates the map ((fg, bg, sh, ch) -> intensity).
// The question to answer here is: What does (fg, bg, sh) look like, expressed as an RGB color?
// Precondition: fg_colors and bg_colors must be initialized
static uint8_t* calc_fg_bg_sh_ch_2_intensity_map(void) {
    uint8_t *result = calloc(fg_bg_sh_ch_2_intensity_map_size, COLOR_CHANNELS * sizeof(uint8_t));
    if (result == NULL) {
        return NULL;
    }

    uint8_t fg_rgb[COLOR_CHANNELS] = {0, 0, 0};
    uint8_t bg_rgb[COLOR_CHANNELS] = {0, 0, 0};

    uint8_t* head = result;
    for (uint8_t fg_idx = 0; fg_idx < fg_colors; ++fg_idx) {
        color_idx_2_r_g_b(fg_idx, &fg_rgb[CH_R], &fg_rgb[CH_G], &fg_rgb[CH_B]);
        for (uint8_t bg_idx = 0; bg_idx < bg_colors; ++bg_idx) {
            color_idx_2_r_g_b(bg_idx, &bg_rgb[CH_R], &bg_rgb[CH_G], &bg_rgb[CH_B]);
            for (uint8_t sh_idx = 0; sh_idx < SHADES; ++sh_idx) {
                for (uint8_t chan = 0; chan < COLOR_CHANNELS; ++chan) {
                    // add background color and foreground color parts
                    double fg_part = ((double) fg_rgb[chan] * sh_idx);
                    double bg_part = ((double) bg_rgb[chan] * (MAX_SHADE_INDEX - sh_idx));
                    double value = ((fg_part + bg_part + SHADE_ADDEND) / MAX_SHADE_INDEX);
                    value = min_double(0xff, max_double(0x00, value));
                    *head++ = (uint8_t) value;
                }
            }
        }
    }
    return result;
}

// Returns the squared Euclidean distance between (a1, b1, c1) and (a2, b2, c2).
static double get_squared_distance(double a1, double b1, double c1, double a2, double b2, double c2) {
    double da = a2 - a1;
    double db = b2 - b1;
    double dc = c2 - c1;
    return da * da + db * db + dc * dc;
}

// Calculates the main lookup table (rgb -> shfgbg) by approximating the rgb color using nearest neighbour applied in RGB color space.
static uint16_t* calc_lookup_table(void) {
    uint16_t* result = calloc(depth_size * depth_size * depth_size, sizeof(uint16_t));
    if (result == NULL) {
        return NULL;
    }

    uint16_t* to_head = result;
    for (uint16_t r_idx = 0; r_idx < depth_size; ++r_idx) {
        uint8_t red = r_idx << depth_shift;
        for (uint16_t g_idx = 0; g_idx < depth_size; ++g_idx) {
            uint8_t green = g_idx << depth_shift;
            for (uint16_t b_idx = 0; b_idx < depth_size; ++b_idx) {
                uint8_t blue = b_idx << depth_shift;
                // calc lookup table cell values by brute force
                uint8_t best_fg_idx = 0;
                uint8_t best_bg_idx = 0;
                uint8_t best_sh_idx = 0;
                double best_dist = 1e50;
                bool is_first = true;
                uint8_t* from = fg_bg_sh_ch_2_intensity_map;
                // check all available colors for best match
                for (uint8_t fg_idx = 0; fg_idx < fg_colors; ++fg_idx) {
                    for (uint8_t bg_idx = 0; bg_idx < bg_colors; ++bg_idx) {
                        for (uint8_t sh_idx = 0; sh_idx < SHADES; ++sh_idx) {
                            uint8_t r = *from++;
                            uint8_t g = *from++;
                            uint8_t b = *from++;
                            double dist = get_squared_distance(red, green, blue, r, g, b);
                            // check if better color found
                            if (best_dist > dist || is_first) {
                                best_dist = dist;
                                best_fg_idx = fg_idx;
                                best_bg_idx = bg_idx;
                                best_sh_idx = sh_idx;
                                is_first = false;
                            }
                        }
                    }
                }
                *to_head++ = fg_bg_sh_2_shfgbg(best_fg_idx, best_bg_idx, best_sh_idx);
            }
        }
    }
    return result;
}

// Initializes color palettes.
static bool init(int new_color_palette_preset, int depth_per_channel,
        bool light_fg_allowed, bool light_bg_allowed) {
    if (r_g_b_2_shfgbg_map != NULL) {
        return true;
    }

    if (depth_per_channel < MIN_COLOR_DEPTH) {
        fprintf(stderr, "[shablo]: --vo-shablo-color-depth-per-channel=%d: too low (min value: %d)", depth_per_channel, MIN_COLOR_DEPTH);
        return false;
    }
    if (depth_per_channel > MAX_COLOR_DEPTH) {
        fprintf(stderr, "[shablo]: --vo-shablo-color-depth-per-channel=%d: too high (max value: %d)", depth_per_channel, MAX_COLOR_DEPTH);
        return false;
    }

    color_palette_preset = new_color_palette_preset;
    fg_colors = light_fg_allowed ? EXT_PALETTE_SIZE : BASE_PALETTE_SIZE;
    bg_colors = light_bg_allowed ? EXT_PALETTE_SIZE : BASE_PALETTE_SIZE;
    fg_bg_sh_ch_2_intensity_map_size = fg_colors * bg_colors * SHADES;

    depth_shift = MAX_COLOR_DEPTH - depth_per_channel;
    depth_size = 1 << depth_per_channel;
    depth_mask = depth_size - 1;
    round_addend = depth_shift >> 1;

    uint8_t* temp = calc_fg_bg_sh_ch_2_intensity_map();
    if (temp == NULL) {
        fprintf(stderr, "[shablo]: Out of memory error.");
        return false;
    }
    fg_bg_sh_ch_2_intensity_map = temp;

    uint16_t* temp4 = calc_lookup_table();
    if (temp4 == NULL) {
        fprintf(stderr, "[shablo]: Out of memory error.");
        return false;
    }
    r_g_b_2_shfgbg_map = temp4;

    return true;
}

// Writes the source image to stdout.
static void write_shaded(
    const int dwidth, const int dheight,
    const int swidth, const int sheight,
    const unsigned char *source, const int source_stride)
{
    assert(source);

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

            // deduce color emulation
            uint8_t fg_idx;
            uint8_t bg_idx;
            uint8_t sh_idx;
            r_g_b_2_fg_bg_sh(r, g, b, &fg_idx, &bg_idx, &sh_idx);
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

static void get_win_size(struct vo *vo, int *out_width, int *out_height) {
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

    if (!init(p->opts->color_palette_preset,p->opts->color_depth,
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
        p->frame->planes[0], p->frame->stride[0]);
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
    vo->monitor_par = (double) ((double) (vo->opts->monitor_pixel_aspect) * (double) p->opts->block_height / (double) p->opts->block_width);
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

