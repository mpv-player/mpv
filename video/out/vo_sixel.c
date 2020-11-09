/*
 * Sixel mpv output device implementation based on ffmpeg libavdevice implementation
 * by Hayaki Saito
 * https://github.com/saitoha/FFmpeg-SIXEL/blob/sixel/libavdevice/sixel.c
 *
 * Copyright (c) 2014 Hayaki Saito
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libswscale/swscale.h>
#include <sixel.h>

#include "config.h"
#include "options/m_config.h"
#include "osdep/terminal.h"
#include "sub/osd.h"
#include "vo.h"
#include "video/sws_utils.h"
#include "video/mp_image.h"

#define IMGFMT IMGFMT_RGB24

#define ESC_HIDE_CURSOR "\033[?25l"
#define ESC_RESTORE_CURSOR "\033[?25h"
#define ESC_CLEAR_SCREEN "\033[2J"
#define ESC_GOTOXY "\033[%d;%df"
#define ESC_USE_GLOBAL_COLOR_REG "\033[?1070l"

struct priv {

    // User specified options
    int diffuse;
    int width;
    int height;
    int reqcolors;
    int fixedpal;
    int threshold;
    int top;
    int left;

    // Internal data
    sixel_output_t *output;
    sixel_dither_t *dither;
    sixel_dither_t *testdither;
    uint8_t        *buffer;

    int image_height;
    int image_width;
    int image_format;

    unsigned int average_r;
    unsigned int average_g;
    unsigned int average_b;
    int previous_histgram_colors;

    struct mp_image *frame;
    struct mp_sws_context *sws;
};

static const unsigned int depth = 3;

static void validate_offset_values(struct vo* vo)
{
    struct priv* priv = vo->priv;
    int top = priv->top;
    int left = priv->left;
    int terminal_width = 0;
    int terminal_height = 0;

    terminal_get_size(&terminal_width, &terminal_height);

    // Make sure that the user specified top offset
    // lies in the range 1 to TERMINAL_HEIGHT
    // Otherwise default to the topmost row
    if (top <= 0 || top > terminal_height)
        priv->top = 1;

    // Make sure that the user specified left offset
    // lies in the range 1 to TERMINAL_WIDTH
    // Otherwise default to the leftmost column
    if (left <= 0 || left > terminal_width)
        priv->left = 1;
}

static int detect_scene_change(struct vo* vo)
{
    struct priv* priv = vo->priv;
    int score;
    int i;
    unsigned int r = 0;
    unsigned int g = 0;
    unsigned int b = 0;

    unsigned int average_r = priv->average_r;
    unsigned int average_g = priv->average_g;
    unsigned int average_b = priv->average_b;
    int previous_histgram_colors = priv->previous_histgram_colors;

    int histgram_colors = 0;
    int palette_colors = 0;
    unsigned char const* palette;

    histgram_colors = sixel_dither_get_num_of_histogram_colors(priv->testdither);

    if (priv->dither == NULL)
        goto detected;

    /* detect scene change if number of colors increses 20% */
    if (previous_histgram_colors * 6 < histgram_colors * 5)
        goto detected;

    /* detect scene change if number of colors decreses 20% */
    if (previous_histgram_colors * 4 > histgram_colors * 5)
        goto detected;

    palette_colors = sixel_dither_get_num_of_palette_colors(priv->testdither);
    palette = sixel_dither_get_palette(priv->testdither);

    /* compare color difference between current
     * palette and previous one */
    for (i = 0; i < palette_colors; i++) {
        r += palette[i * 3 + 0];
        g += palette[i * 3 + 1];
        b += palette[i * 3 + 2];
    }
    score = (r - average_r) * (r - average_r)
          + (g - average_g) * (g - average_g)
          + (b - average_b) * (b - average_b);
    if (score > priv->threshold * palette_colors
                             * palette_colors)
        goto detected;

    return 0;

detected:
    priv->previous_histgram_colors = histgram_colors;
    priv->average_r = r;
    priv->average_g = g;
    priv->average_b = b;
    return 1;
}

static void dealloc_dithers_and_buffer(struct vo* vo)
{
    struct priv* priv = vo->priv;

    if (priv->buffer) {
        talloc_free(priv->buffer);
        priv->buffer = NULL;
    }

    if (priv->dither) {
        sixel_dither_unref(priv->dither);
        priv->dither = NULL;
    }

    if (priv->testdither) {
        sixel_dither_unref(priv->testdither);
        priv->testdither = NULL;
    }
}

static SIXELSTATUS prepare_static_palette(struct vo* vo)
{
    struct priv* priv = vo->priv;

    if (priv->dither)
        sixel_dither_set_body_only(priv->dither, 1);
    else {
        priv->dither = sixel_dither_get(BUILTIN_XTERM256);
        if (priv->dither == NULL)
            return SIXEL_FALSE;
        sixel_dither_set_diffusion_type(priv->dither, priv->diffuse);
    }
    return SIXEL_OK;
}

static SIXELSTATUS prepare_dynamic_palette(struct vo *vo)
{
    SIXELSTATUS status = SIXEL_FALSE;
    struct priv *priv = vo->priv;

    /* create histgram and construct color palette
     * with median cut algorithm. */
    status = sixel_dither_initialize(priv->testdither, priv->buffer,
                                     priv->width, priv->height, 3,
                                     LARGE_NORM, REP_CENTER_BOX,
                                     QUALITY_LOW);
    if (SIXEL_FAILED(status))
        return status;

    if (detect_scene_change(vo)) {
        if (priv->dither)
            sixel_dither_unref(priv->dither);

        priv->dither = priv->testdither;
        status = sixel_dither_new(&priv->testdither, priv->reqcolors, NULL);

        if (SIXEL_FAILED(status))
            return status;

        sixel_dither_set_diffusion_type(priv->dither, priv->diffuse);
    } else
        sixel_dither_set_body_only(priv->dither, 1);

    return status;
}

static int resize(struct vo *vo)
{
    struct priv *priv = vo->priv;

    dealloc_dithers_and_buffer(vo);

    SIXELSTATUS status = sixel_dither_new(&priv->testdither, priv->reqcolors, NULL);
    if (SIXEL_FAILED(status))
        return status;

    priv->buffer =
        talloc_array(NULL, uint8_t, depth * priv->width * priv->height);

    return 0;
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *priv = vo->priv;
    priv->image_height = params->h;
    priv->image_width  = params->w;
    priv->image_format = params->imgfmt;

    priv->sws->src = *params;
    priv->sws->dst = (struct mp_image_params) {
        .imgfmt = IMGFMT,
        .w = priv->width,
        .h = priv->height,
        .p_w = 1,
        .p_h = 1,
    };

    priv->frame = mp_image_alloc(IMGFMT, priv->width, priv->height);
    if (!priv->frame)
        return -1;

    if (mp_sws_reinit(priv->sws) < 0)
        return -1;

    printf(ESC_HIDE_CURSOR);
    printf(ESC_CLEAR_SCREEN);
    vo->want_redraw = true;

    return resize(vo);
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct priv *priv = vo->priv;
    struct mp_image src = *mpi;

    // Downscale the image
    mp_sws_scale(priv->sws, priv->frame, &src);

    // Copy from mpv to RGB format as required by libsixel
    memcpy_pic(priv->buffer, priv->frame->planes[0], priv->width * depth, priv->height,
               priv->width * depth, priv->frame->stride[0]);

    if (priv->fixedpal)
        prepare_static_palette(vo);
    else
        prepare_dynamic_palette(vo);

    talloc_free(mpi);
}

static int sixel_write(char *data, int size, void *priv)
{
    return fwrite(data, 1, size, (FILE *)priv);
}

static void flip_page(struct vo *vo)
{
    struct priv* priv = vo->priv;

    // Make sure that image and dither are valid before drawing
    if (priv->buffer == NULL || priv->dither == NULL)
        return;

    // Go to the offset row and column, then display the image
    printf(ESC_GOTOXY, priv->top, priv->left);
    sixel_encode(priv->buffer, priv->width, priv->height,
                 PIXELFORMAT_RGB888,
                 priv->dither, priv->output);
    fflush(stdout);
}

static int preinit(struct vo *vo)
{
    struct priv *priv = vo->priv;
    SIXELSTATUS status = SIXEL_FALSE;
    FILE* sixel_output_file = stdout;

    // Parse opts set by CLI or conf
    priv->sws = mp_sws_alloc(vo);
    priv->sws->log = vo->log;
    mp_sws_enable_cmdline_opts(priv->sws, vo->global);

    status = sixel_output_new(&priv->output, sixel_write, sixel_output_file, NULL);
    if (SIXEL_FAILED(status))
        return status;

    sixel_output_set_encode_policy(priv->output, SIXEL_ENCODEPOLICY_FAST);

    printf(ESC_HIDE_CURSOR);

    /* don't use private color registers for each frame. */
    printf(ESC_USE_GLOBAL_COLOR_REG);

    priv->dither = NULL;
    status = sixel_dither_new(&priv->testdither, priv->reqcolors, NULL);

    if (SIXEL_FAILED(status))
        return status;

    priv->buffer =
        talloc_array(NULL, uint8_t, depth * priv->width * priv->height);

    priv->average_r = 0;
    priv->average_g = 0;
    priv->average_b = 0;
    priv->previous_histgram_colors = 0;

    validate_offset_values(vo);

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


static void uninit(struct vo *vo)
{
    struct priv *priv = vo->priv;

    printf(ESC_RESTORE_CURSOR);

    printf(ESC_CLEAR_SCREEN);
    printf(ESC_GOTOXY, 1, 1);
    fflush(stdout);

    if (priv->output) {
        sixel_output_unref(priv->output);
        priv->output = NULL;
    }

    dealloc_dithers_and_buffer(vo);
}

#define OPT_BASE_STRUCT struct priv

const struct vo_driver video_out_sixel = {
    .name = "sixel",
    .description = "libsixel",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .diffuse = DIFFUSE_ATKINSON,
        .width = 320,
        .height = 240,
        .reqcolors = 256,
        .fixedpal = 0,
        .threshold = 0,
        .top = 1,
        .left = 1,
    },
    .options = (const m_option_t[]) {
        {"diffusion", OPT_CHOICE(diffuse,
            {"auto", DIFFUSE_AUTO},
            {"none", DIFFUSE_NONE},
            {"atkinson", DIFFUSE_ATKINSON},
            {"fs", DIFFUSE_FS},
            {"jajuni", DIFFUSE_JAJUNI},
            {"stucki", DIFFUSE_STUCKI},
            {"burkes", DIFFUSE_BURKES},
            {"arithmetic", DIFFUSE_A_DITHER},
            {"xor", DIFFUSE_X_DITHER})},
        {"width", OPT_INT(width)},
        {"height", OPT_INT(height)},
        {"reqcolors", OPT_INT(reqcolors)},
        {"fixedpalette", OPT_INT(fixedpal)},
        {"color-threshold", OPT_INT(threshold)},
        {"offset-top", OPT_INT(top)},
        {"offset-left", OPT_INT(left)},
        {0}
    },
    .options_prefix = "vo-sixel",
};
