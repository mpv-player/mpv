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

#define ALGO_PLAIN 1
#define ALGO_HALF_BLOCKS 2
#define ESC_HIDE_CURSOR "\e[?25l"
#define ESC_RESTORE_CURSOR "\e[?25h"
#define ESC_CLEAR_SCREEN "\e[2J"
#define ESC_CLEAR_COLORS "\e[0m"
#define ESC_GOTOXY "\e[%d;%df"
#define ESC_COLOR_BACKGROUND "\e[48;2;%d;%d;%dm"
#define ESC_COLOR_FOREGROUND "\e[38;2;%d;%d;%dm"
#define DEFAULT_WIDTH 80
#define DEFAULT_HEIGHT 25

struct vo_tct_opts {
    int algo;
    int width;   // 0 -> default
    int height;  // 0 -> default
};

#define OPT_BASE_STRUCT struct vo_tct_opts
static const struct m_sub_options vo_tct_conf = {
    .opts = (const m_option_t[]) {
        OPT_CHOICE("vo-tct-algo", algo, 0,
                   ({"plain", ALGO_PLAIN},
                    {"half-blocks", ALGO_HALF_BLOCKS})),
        OPT_INT("vo-tct-width", width, 0),
        OPT_INT("vo-tct-height", height, 0),
        {0}
    },
    .defaults = &(const struct vo_tct_opts) {
        .algo = ALGO_HALF_BLOCKS,
    },
    .size = sizeof(struct vo_tct_opts),
};

struct priv {
    struct vo_tct_opts *opts;
    size_t buffer_size;
    char *buffer;
    int swidth;
    int sheight;
    struct mp_image *frame;
    struct mp_rect src;
    struct mp_rect dst;
    struct mp_sws_context *sws;
};

static void write_plain(
    const int dwidth, const int dheight,
    const int swidth, const int sheight,
    const unsigned char *source, const int source_stride)
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
            printf(ESC_COLOR_BACKGROUND, r, g, b);
            printf(" ");
        }
        printf(ESC_CLEAR_COLORS);
    }
    printf("\n");
}

static void write_half_blocks(
    const int dwidth, const int dheight,
    const int swidth, const int sheight,
    unsigned char *source, int source_stride)
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
            printf(ESC_COLOR_BACKGROUND, r_up, g_up, b_up);
            printf(ESC_COLOR_FOREGROUND, r_down, g_down, b_down);
            printf("\xe2\x96\x84");  // UTF8 bytes of U+2584 (lower half block)
        }
        printf(ESC_CLEAR_COLORS);
    }
    printf("\n");
}

static void get_win_size(struct vo *vo, int *out_width, int *out_height) {
    struct priv *p = vo->priv;
#if HAVE_POSIX
    struct winsize winsize;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsize);
    *out_width = winsize.ws_col;
    *out_height = winsize.ws_row;
#else
    *out_width = DEFAULT_WIDTH;
    *out_height = DEFAULT_HEIGHT;
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

    const int mul = (p->opts->algo == ALGO_PLAIN ? 1 : 2);
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
    if (p->opts->algo == ALGO_PLAIN) {
        write_plain(
            vo->dwidth, vo->dheight, p->swidth, p->sheight,
            p->frame->planes[0], p->frame->stride[0]);
    } else {
        write_half_blocks(
            vo->dwidth, vo->dheight, p->swidth, p->sheight,
            p->frame->planes[0], p->frame->stride[0]);
    }
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
    // most terminal characters aren't 1:1, so we default to 2:1.
    // if user passes their own value of choice, it'll be scaled accordingly.
    vo->monitor_par = vo->opts->monitor_pixel_aspect * 2;

    struct priv *p = vo->priv;
    p->opts = mp_get_config_group(vo, vo->global, &vo_tct_conf);
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
