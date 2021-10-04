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
#include <stdbool.h>

#if HAVE_POSIX
#   include <sys/ioctl.h>
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
#define ESC_CLEAR_COLORS_LEN 5
#define ESC_GOTOXY "\033[%d;%df"
#define ESC_GOTOXY_LEN 11
#define ESC_COLOR_BG "\033[48;2"
#define ESC_COLOR_FG "\033[38;2"
#define ESC_COLOR256_BG "\033[48;5"
#define ESC_COLOR256_FG "\033[38;5"
#define ESC_COLOR_LEN 6

#define DEFAULT_WIDTH 80
#define DEFAULT_HEIGHT 25

typedef struct lut_item {
    char str[4];
    int width;
} lut_item_t;

struct priv {
    // User specified options
    int opt_algo;
    int opt_width;   // 0 -> default
    int opt_height;  // 0 -> default
    int opt_term256;  // 0 -> true color
    
    // Internal data
    size_t buffer_size;
    int    swidth;
    int    sheight;
    
    struct mp_rect    src;
    struct mp_rect    dst;
    struct mp_image  *frame;
    struct mp_sws_context *sws;
    
    // int -> str lookup table for faster conversion
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

static inline int snprint_seq3(char* buff, lut_item_t *lut, size_t prefix_len,
                         const char* prefix, uint8_t r, uint8_t g, uint8_t b)
{
    memcpy(buff, prefix,     prefix_len);   buff += prefix_len;
    memcpy(buff, lut[r].str, lut[r].width); buff += lut[r].width;
    memcpy(buff, lut[g].str, lut[g].width); buff += lut[g].width;
    memcpy(buff, lut[b].str, lut[b].width); buff += lut[b].width;
    *buff = 'm';
    
    return prefix_len + lut[r].width + lut[g].width + lut[b].width + 1;
}

static inline int snprint_seq1(char* buff, lut_item_t *lut, size_t prefix_len,
                         const char* prefix, uint8_t c)
{
    memcpy(buff, prefix,     prefix_len);   buff += prefix_len;
    memcpy(buff, lut[c].str, lut[c].width); buff += lut[c].width;
    *buff = 'm';
    
    return prefix_len + lut[c].width + 1;
}

// Used for generalizing the code between true-color and xterm-256 options
// (removes the need to check for it on every pixel). The dummy function is used
// with ALGO_PLAIN. No inline keyword, because they are made to be pointed at,
// and not called directly.
static int __x256_sprint_bg(char* buff, lut_item_t *lut, uint8_t r, uint8_t g, uint8_t b)
{ return snprint_seq1(buff, lut, ESC_COLOR_LEN, ESC_COLOR256_BG, rgb_to_x256(r, g, b));}

static int __x256_sprint_fg(char* buff, lut_item_t *lut, uint8_t r, uint8_t g, uint8_t b)
{ return snprint_seq1(buff, lut, ESC_COLOR_LEN, ESC_COLOR256_FG, rgb_to_x256(r, g, b));}

static int __tc_sprint_bg(char* buff, lut_item_t *lut, uint8_t r, uint8_t g, uint8_t b)
{ return snprint_seq3(buff, lut, ESC_COLOR_LEN, ESC_COLOR_BG, r, g, b); }

static int __tc_sprint_fg(char* buff, lut_item_t *lut, uint8_t r, uint8_t g, uint8_t b)
{ return snprint_seq3(buff, lut, ESC_COLOR_LEN, ESC_COLOR_FG, r, g, b); }

static int __dummy_sprintf(char* buff, lut_item_t *lut, uint8_t r, uint8_t g, uint8_t b)
{ return 0; }

// The fwrite implementation is about 25% faster than the printf code
// (even if we use *.s with the lut values), however,
// on windows we need to use printf in order to translate escape sequences and
// UTF8 output for the console.
static void print_buff(char* buff, int size)
{
#   ifndef _WIN32
#       ifdef __linux__
            fwrite_unlocked(buff, size, 1, stdout);
#       else
            fwrite(buff, size, 1, stdout);
#       endif
#   else
        printf("%.*s", size, buff);
#   endif
}

static void draw_frame(struct vo *vo, struct vo_frame *frame)
{
    assert(IMGFMT == IMGFMT_BGR24); // or else the init macro breaks.
    assert(frame->current->planes[0]);
    
    struct priv *p = vo->priv;
    mp_sws_scale(p->sws, p->frame, frame->current);
    
    uint8_t *src   = p->frame->planes[0];
    int src_stride = p->frame->stride[0];
    
    int term_x = (vo->dwidth - p->swidth) / 2;
    int term_y = (vo->dheight - p->sheight) / 2;
    
    int max_seq_size  = (p->opt_term256 ? 11 : 19);
    int max_buff_size = (max_seq_size * 2 + 3) * (p->sheight * p->swidth)
                      + (10 + 4) * p->sheight // ESC_GOTOXY + ESC_CLEAR_COLORS
                      + 2; // "\n\0"
    
    char *framebuff = malloc(max_buff_size);
    char *buffptr = framebuff;
    
    // Requires a semi-hack (color_rgb_args) to make compatible with other
    // functions, but doesn't pollute the global fucking namespace with
    // a generic name.
    // We are using 16 bit signed ints, because it makes it easier to check
    // for an unset value.
    typedef struct color {
        int16_t b;
        int16_t g;
        int16_t r;
    } color_t;

#   define color_rgb_args(x) (uint8_t)(x).r, (uint8_t)(x).g, (uint8_t)(x).b
#   define color_buff_inc(x) {.b = *(x)++, .g = *(x)++, .r = *(x)++}
#   define color_eq(x, y) (((x).b == (y).b) && ((x).g == (y).g) \
                                            && ((x).r == (y).r))
    
    // Use function pointers to generalize the printing functions
    int (*sprint_bg)(char*, lut_item_t*, uint8_t, uint8_t, uint8_t) = NULL;
    int (*sprint_fg)(char*, lut_item_t*, uint8_t, uint8_t, uint8_t) = NULL;
    if (p->opt_term256) {
        sprint_bg = &__x256_sprint_bg;
        sprint_fg = &__x256_sprint_fg;
    } else {
        sprint_bg = &__tc_sprint_bg;
        sprint_fg = &__tc_sprint_fg;
    }
    if (p->opt_algo == ALGO_PLAIN) {
        sprint_fg = &__dummy_sprintf;
    }

    // "\xe2\x96\x84" are the UTF8 bytes of U+2584 (lower half block)
    const char* pixel_char = (p->opt_algo == ALGO_PLAIN ?
                              " " : "\xe2\x96\x84");
    int pixel_char_len = (p->opt_algo == ALGO_PLAIN ? 2 : 4);
    
    for (int y = 0; y < p->sheight * 2; y += 2) {
        uint8_t *row_up   = src + y * src_stride;
        uint8_t *row_down = src + (y + 1) * src_stride;
        
        buffptr += snprintf(buffptr, ESC_GOTOXY_LEN, ESC_GOTOXY,
                            term_y + y / 2, term_x);
        color_t old_up   = {-1, -1, -1};
        color_t old_down = {-1, -1, -1};
        
        for (int x = 0; x < p->swidth; ++x) {
            color_t up =   color_buff_inc(row_up);
            color_t down = color_buff_inc(row_down);
            
            if (!color_eq(old_up, up))
                buffptr += sprint_bg(buffptr, p->lut, color_rgb_args(up));
            if (!color_eq(old_down, down))
                buffptr += sprint_fg(buffptr, p->lut, color_rgb_args(down));
            
            buffptr += snprintf(buffptr, pixel_char_len, "%s", pixel_char);
            old_down = down;
            old_up = up;
        }
        buffptr += snprintf(buffptr, ESC_CLEAR_COLORS_LEN, ESC_CLEAR_COLORS);
    }
    *buffptr++ = '\n';
    print_buff(framebuff, buffptr - framebuff);
    free(framebuff);
}

static void get_win_size(struct vo *vo, int *out_width, int *out_height) {
    struct priv *p = vo->priv;
    *out_width = DEFAULT_WIDTH;
    *out_height = DEFAULT_HEIGHT;

    terminal_get_size(out_width, out_height);

    if (p->opt_width > 0)
        *out_width = p->opt_width;
    if (p->opt_height > 0)
        *out_height = p->opt_height;
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

    // We still have to alloc the same ammount as for ALGO_HALF_BLOCKS,
    // because we just use the upper of 2 rows (cleaner code)
    const int mul = (p->opt_algo == ALGO_PLAIN ? 2 : 2);
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

static void flip_page(struct vo *vo)
{
    int width, height;
    get_win_size(vo, &width, &height);
    
    if (vo->dwidth != width || vo->dheight != height)
        reconfig(vo, vo->params);
    
#   ifdef __linux__
        fflush_unlocked(stdout);
#   else
        fflush(stdout);
#   endif
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
    p->sws = mp_sws_alloc(vo);
    p->sws->log = vo->log;
    mp_sws_enable_cmdline_opts(p->sws, vo->global);

    for (int i = 0; i < 256; ++i) {
        char buff[8];
        p->lut[i].width = sprintf(buff, ";%d", i);
        // some strings may not end on a null byte, but that's ok.
        memcpy(p->lut[i].str, buff, 4);
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

#define OPT_BASE_STRUCT struct priv
const struct vo_driver video_out_tct = {
    .name = "tct",
    .description = "true-color terminals",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_frame = draw_frame,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .opt_algo = ALGO_HALF_BLOCKS,
        .opt_term256 = 0,
        .opt_height = 0,
        .opt_width = 0
    },
    .options = (const m_option_t[]) {
        {"algo", OPT_CHOICE(opt_algo,
            {"plain", ALGO_PLAIN},
            {"half-blocks", ALGO_HALF_BLOCKS})},
        {"width", OPT_INT(opt_width)},
        {"height", OPT_INT(opt_height)},
        {"256", OPT_FLAG(opt_term256)},
        {0}
    },
    .options_prefix = "vo-tct"
};
