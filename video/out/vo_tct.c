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
#include "osdep/terminal.h"
#include "osdep/io.h"
#include "vo.h"
#include "sub/osd.h"
#include "video/sws_utils.h"
#include "video/mp_image.h"

#define IMGFMT IMGFMT_BGR24

#define ALGO_PLAIN 1
#define ALGO_HALF_BLOCKS 2
#define ESC_HIDE_CURSOR "\033[?25l"
#define ESC_RESTORE_CURSOR "\033[?25h"
#define ESC_CLEAR_SCREEN "\033[2J"
#define ESC_CLEAR_COLORS "\033[0m"
#define ESC_GOTOXY "\033[%d;%df"
#define ESC_COLOR_BG "\033[48;2"
#define ESC_COLOR_FG "\033[38;2"
#define ESC_COLOR256_BG "\033[48;5"
#define ESC_COLOR256_FG "\033[38;5"
#define DEFAULT_WIDTH 80
#define DEFAULT_HEIGHT 25

struct vo_tct_opts {
    int algo;
    int width;   // 0 -> default
    int height;  // 0 -> default
    int term256;  // 0 -> true color
};

#define OPT_BASE_STRUCT struct vo_tct_opts
static const struct m_sub_options vo_tct_conf = {
    .opts = (const m_option_t[]) {
        {"vo-tct-algo", OPT_CHOICE(algo,
            {"plain", ALGO_PLAIN},
            {"half-blocks", ALGO_HALF_BLOCKS})},
        {"vo-tct-width", OPT_INT(width)},
        {"vo-tct-height", OPT_INT(height)},
        {"vo-tct-256", OPT_FLAG(term256)},
        {0}
    },
    .defaults = &(const struct vo_tct_opts) {
        .algo = ALGO_HALF_BLOCKS,
    },
    .size = sizeof(struct vo_tct_opts),
};

struct lut_item {
    char str[4];
    int width;
};

struct priv {
    struct vo_tct_opts *opts;
    size_t buffer_size;
    int swidth;
    int sheight;
    struct mp_image *frame;
    struct mp_rect src;
    struct mp_rect dst;
    struct mp_sws_context *sws;
    struct lut_item lut[256];
};

// Convert RGB24 to xterm-256 8-bit value
// For simplicity, assume RGB space is perceptually uniform.
// There are 5 places where one of two outputs needs to be chosen when the
// input is the exact middle:
// - The r/g/b channels and the gray value: the higher value output is chosen.
// - If the gray and color have same distance from the input - color is chosen.
static int rgb_to_x256(uint8_t r, uint8_t g, uint8_t b)
{
    // Calculate the nearest 0-based color index at 16 .. 231
#   define v2ci(v) (v < 48 ? 0 : v < 115 ? 1 : (v - 35) / 40)
    int ir = v2ci(r), ig = v2ci(g), ib = v2ci(b);   // 0..5 each
#   define color_index() (36 * ir + 6 * ig + ib)  /* 0..215, lazy evaluation */

    // Calculate the nearest 0-based gray index at 232 .. 255
    int average = (r + g + b) / 3;
    int gray_index = average > 238 ? 23 : (average - 3) / 10;  // 0..23

    // Calculate the represented colors back from the index
    static const int i2cv[6] = {0, 0x5f, 0x87, 0xaf, 0xd7, 0xff};
    int cr = i2cv[ir], cg = i2cv[ig], cb = i2cv[ib];  // r/g/b, 0..255 each
    int gv = 8 + 10 * gray_index;  // same value for r/g/b, 0..255

    // Return the one which is nearer to the original input rgb value
#   define dist_square(A,B,C, a,b,c) ((A-a)*(A-a) + (B-b)*(B-b) + (C-c)*(C-c))
    int color_err = dist_square(cr, cg, cb, r, g, b);
    int gray_err  = dist_square(gv, gv, gv, r, g, b);
    return color_err <= gray_err ? 16 + color_index() : 232 + gray_index;
}

static void print_seq3(struct lut_item *lut, const char* prefix,
                       uint8_t r, uint8_t g, uint8_t b)
{
// The fwrite implementation is about 25% faster than the printf code
// (even if we use *.s with the lut values), however,
// on windows we need to use printf in order to translate escape sequences and
// UTF8 output for the console.
#ifndef _WIN32
    fputs(prefix, stdout);
    fwrite(lut[r].str, lut[r].width, 1, stdout);
    fwrite(lut[g].str, lut[g].width, 1, stdout);
    fwrite(lut[b].str, lut[b].width, 1, stdout);
    fputc('m', stdout);
#else
    printf("%s;%d;%d;%dm", prefix, (int)r, (int)g, (int)b);
#endif
}

static void print_seq1(struct lut_item *lut, const char* prefix, uint8_t c)
{
#ifndef _WIN32
    fputs(prefix, stdout);
    fwrite(lut[c].str, lut[c].width, 1, stdout);
    fputc('m', stdout);
#else
    printf("%s;%dm", prefix, (int)c);
#endif
}


static void write_plain(
    const int dwidth, const int dheight,
    const int swidth, const int sheight,
    const unsigned char *source, const int source_stride,
    bool term256, struct lut_item *lut)
{
    assert(source);
    const int tx = (dwidth - swidth) / 2;
    const int ty = (dheight - sheight) / 2;
    for (int y = 0; y < sheight; y++) {
        const unsigned char *row = source + y * source_stride;
        printf(ESC_GOTOXY, ty + y, tx);
        for (int x = 0; x < swidth; x++) {
            unsigned char b = *row++;
            unsigned char g = *row++;
            unsigned char r = *row++;
            if (term256) {
                print_seq1(lut, ESC_COLOR256_BG, rgb_to_x256(r, g, b));
            } else {
                print_seq3(lut, ESC_COLOR_BG, r, g, b);
            }
            printf(" ");
        }
        printf(ESC_CLEAR_COLORS);
    }
    printf("\n");
}

static void write_half_blocks(
    const int dwidth, const int dheight,
    const int swidth, const int sheight,
    unsigned char *source, int source_stride,
    bool term256, struct lut_item *lut)
{
    assert(source);
    const int tx = (dwidth - swidth) / 2;
    const int ty = (dheight - sheight) / 2;
    for (int y = 0; y < sheight * 2; y += 2) {
        const unsigned char *row_up = source + y * source_stride;
        const unsigned char *row_down = source + (y + 1) * source_stride;
        printf(ESC_GOTOXY, ty + y / 2, tx);
        for (int x = 0; x < swidth; x++) {
            unsigned char b_up = *row_up++;
            unsigned char g_up = *row_up++;
            unsigned char r_up = *row_up++;
            unsigned char b_down = *row_down++;
            unsigned char g_down = *row_down++;
            unsigned char r_down = *row_down++;
            if (term256) {
                print_seq1(lut, ESC_COLOR256_BG, rgb_to_x256(r_up, g_up, b_up));
                print_seq1(lut, ESC_COLOR256_FG, rgb_to_x256(r_down, g_down, b_down));
            } else {
                print_seq3(lut, ESC_COLOR_BG, r_up, g_up, b_up);
                print_seq3(lut, ESC_COLOR_FG, r_down, g_down, b_down);
            }
            printf("\xe2\x96\x84");  // UTF8 bytes of U+2584 (lower half block)
        }
        printf(ESC_CLEAR_COLORS);
    }
    printf("\n");
}

static void get_win_size(struct vo *vo, int *out_width, int *out_height) {
    struct priv *p = vo->priv;
    *out_width = DEFAULT_WIDTH;
    *out_height = DEFAULT_HEIGHT;

    terminal_get_size(out_width, out_height);

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

    p->sws->src = *params;
    p->sws->dst = (struct mp_image_params) {
        .imgfmt = IMGFMT,
        .w = p->swidth,
        .h = p->sheight,
        .p_w = 1,
        .p_h = 1,
    };

    const int mul = (p->opts->algo == ALGO_PLAIN ? 1 : 2);
    if (p->frame)
        talloc_free(p->frame);
    p->frame = mp_image_alloc(IMGFMT, p->swidth, p->sheight * mul);
    if (!p->frame)
        return -1;

    if (mp_sws_reinit(p->sws) < 0)
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

    int width, height;
    get_win_size(vo, &width, &height);

    if (vo->dwidth != width || vo->dheight != height)
        reconfig(vo, vo->params);

    if (p->opts->algo == ALGO_PLAIN) {
        write_plain(
            vo->dwidth, vo->dheight, p->swidth, p->sheight,
            p->frame->planes[0], p->frame->stride[0],
            p->opts->term256, p->lut);
    } else {
        write_half_blocks(
            vo->dwidth, vo->dheight, p->swidth, p->sheight,
            p->frame->planes[0], p->frame->stride[0],
            p->opts->term256, p->lut);
    }
    fflush(stdout);
}

static void uninit(struct vo *vo)
{
    printf(ESC_RESTORE_CURSOR);
    printf(ESC_CLEAR_SCREEN);
    printf(ESC_GOTOXY, 0, 0);
    struct priv *p = vo->priv;
    if (p->frame)
        talloc_free(p->frame);
}

static int preinit(struct vo *vo)
{
    // most terminal characters aren't 1:1, so we default to 2:1.
    // if user passes their own value of choice, it'll be scaled accordingly.
    vo->monitor_par = vo->opts->monitor_pixel_aspect * 2;

    struct priv *p = vo->priv;
    p->opts = mp_get_config_group(vo, vo->global, &vo_tct_conf);
    p->sws = mp_sws_alloc(vo);
    p->sws->log = vo->log;
    mp_sws_enable_cmdline_opts(p->sws, vo->global);

    for (int i = 0; i < 256; ++i) {
        char buff[8];
        p->lut[i].width = sprintf(buff, ";%d", i);
        memcpy(p->lut[i].str, buff, 4); // some strings may not end on a null byte, but that's ok.
    }

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

const struct vo_driver video_out_tct = {
    .name = "tct",
    .description = "true-color terminals",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
    .global_opts = &vo_tct_conf,
};
