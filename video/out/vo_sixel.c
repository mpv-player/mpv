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

#define TERMINAL_FALLBACK_COLS      80
#define TERMINAL_FALLBACK_ROWS      25
#define TERMINAL_FALLBACK_PX_WIDTH  320
#define TERMINAL_FALLBACK_PX_HEIGHT 240

#define ESC_HIDE_CURSOR             "\033[?25l"
#define ESC_RESTORE_CURSOR          "\033[?25h"
#define ESC_CLEAR_SCREEN            "\033[2J"
#define ESC_GOTOXY                  "\033[%d;%df"
#define ESC_USE_GLOBAL_COLOR_REG    "\033[?1070l"

struct priv {

    // User specified options
    int opt_diffuse;
    int opt_width;
    int opt_height;
    int opt_reqcolors;
    int opt_fixedpal;
    int opt_threshold;
    int opt_top;
    int opt_left;
    int opt_pad_y;
    int opt_pad_x;
    int opt_rows;
    int opt_cols;
    int opt_clear;

    // Internal data
    sixel_output_t *output;
    sixel_dither_t *dither;
    sixel_dither_t *testdither;
    uint8_t        *buffer;
    bool            skip_frame_draw;

    int left, top;  // image origin cell (1 based)
    int width, height;  // actual image px size - always reflects dst_rect.
    int num_cols, num_rows;  // terminal size in cells
    int canvas_ok;  // whether canvas vo->dwidth and vo->dheight are positive

    int previous_histgram_colors;

    struct mp_rect src_rect;
    struct mp_rect dst_rect;
    struct mp_osd_res osd;
    struct mp_image *frame;
    struct mp_sws_context *sws;
};

static const unsigned int depth = 3;

static int detect_scene_change(struct vo* vo)
{
    struct priv* priv = vo->priv;
    int previous_histgram_colors = priv->previous_histgram_colors;
    int histgram_colors = 0;

    // If threshold is set negative, then every frame must be a scene change
    if (priv->dither == NULL || priv->opt_threshold < 0)
        return 1;

    histgram_colors = sixel_dither_get_num_of_histogram_colors(priv->testdither);

    int color_difference_count = previous_histgram_colors - histgram_colors;
    color_difference_count = (color_difference_count > 0) ?  // abs value
                              color_difference_count : -color_difference_count;

    if (100 * color_difference_count >
        priv->opt_threshold * previous_histgram_colors)
    {
        priv->previous_histgram_colors = histgram_colors; // update history
        return 1;
    } else {
        return 0;
    }

}

static void dealloc_dithers_and_buffers(struct vo* vo)
{
    struct priv* priv = vo->priv;

    if (priv->buffer) {
        talloc_free(priv->buffer);
        priv->buffer = NULL;
    }

    if (priv->frame) {
        talloc_free(priv->frame);
        priv->frame = NULL;
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

    if (!priv->dither) {
        priv->dither = sixel_dither_get(BUILTIN_XTERM256);
        if (priv->dither == NULL)
            return SIXEL_FALSE;

        sixel_dither_set_diffusion_type(priv->dither, priv->opt_diffuse);
    }

    sixel_dither_set_body_only(priv->dither, 0);
    return SIXEL_OK;
}

static SIXELSTATUS prepare_dynamic_palette(struct vo *vo)
{
    SIXELSTATUS status = SIXEL_FALSE;
    struct priv *priv = vo->priv;

    /* create histgram and construct color palette
     * with median cut algorithm. */
    status = sixel_dither_initialize(priv->testdither, priv->buffer,
                                     priv->width, priv->height,
                                     SIXEL_PIXELFORMAT_RGB888,
                                     LARGE_NORM, REP_CENTER_BOX,
                                     QUALITY_LOW);
    if (SIXEL_FAILED(status))
        return status;

    if (detect_scene_change(vo)) {
        if (priv->dither) {
            sixel_dither_unref(priv->dither);
            priv->dither = NULL;
        }

        priv->dither = priv->testdither;
        status = sixel_dither_new(&priv->testdither, priv->opt_reqcolors, NULL);

        if (SIXEL_FAILED(status))
            return status;

        sixel_dither_set_diffusion_type(priv->dither, priv->opt_diffuse);
    } else {
        if (priv->dither == NULL)
            return SIXEL_FALSE;
    }

    sixel_dither_set_body_only(priv->dither, 0);
    return status;
}

static void update_canvas_dimensions(struct vo *vo)
{
    // this function sets the vo canvas size in pixels vo->dwidth, vo->dheight,
    // and the number of rows and columns available in priv->num_rows/cols
    struct priv *priv   = vo->priv;
    int num_rows        = TERMINAL_FALLBACK_ROWS;
    int num_cols        = TERMINAL_FALLBACK_COLS;
    int total_px_width  = 0;
    int total_px_height = 0;

    terminal_get_size2(&num_rows, &num_cols, &total_px_width, &total_px_height);

    // If the user has specified rows/cols use them for further calculations
    num_rows = (priv->opt_rows > 0) ? priv->opt_rows : num_rows;
    num_cols = (priv->opt_cols > 0) ? priv->opt_cols : num_cols;

    // If the pad value is set in between 0 and width/2 - 1, then we
    // subtract from the detected width. Otherwise, we assume that the width
    // output must be a integer multiple of num_cols and accordingly set
    // total_width to be an integer multiple of num_cols. So in case the padding
    // added by terminal is less than the number of cells in that axis, then rounding
    // down will take care of correcting the detected width and remove padding.
    if (priv->opt_width > 0) {
        // option - set by the user, hard truth
        total_px_width = priv->opt_width;
    } else {
        if (total_px_width <= 0) {
                // ioctl failed to read terminal width
                total_px_width = TERMINAL_FALLBACK_PX_WIDTH;
        } else {
            if (priv->opt_pad_x >= 0 && priv->opt_pad_x < total_px_width / 2) {
                // explicit padding set by the user
                total_px_width -= (2 * priv->opt_pad_x);
            } else {
                // rounded "auto padding"
                total_px_width = total_px_width / num_cols * num_cols;
            }
        }
    }

    if (priv->opt_height > 0) {
        total_px_height = priv->opt_height;
    } else {
        if (total_px_height <= 0) {
            total_px_height = TERMINAL_FALLBACK_PX_HEIGHT;
        } else {
            if (priv->opt_pad_y >= 0 && priv->opt_pad_y < total_px_height / 2) {
                total_px_height -= (2 * priv->opt_pad_y);
            } else {
                total_px_height = total_px_height / num_rows * num_rows;
            }
        }
    }

    // use n-1 rows for height
    // The last row can't be used for encoding image, because after sixel encode
    // the terminal moves the cursor to next line below the image, causing the
    // last line to be empty instead of displaying image data.
    // TODO: Confirm if the output height must be a multiple of 6, if not, remove
    // the / 6 * 6 part which is setting the height to be a multiple of 6.
    vo->dheight = total_px_height * (num_rows - 1) / num_rows / 6 * 6;
    vo->dwidth  = total_px_width;

    priv->num_rows = num_rows;
    priv->num_cols = num_cols;

    priv->canvas_ok = vo->dwidth > 0 && vo->dheight > 0;
}

static void set_sixel_output_parameters(struct vo *vo)
{
    // This function sets output scaled size in priv->width, priv->height
    // and the scaling rectangles in pixels priv->src_rect, priv->dst_rect
    // as well as image positioning in cells priv->top, priv->left.
    struct priv *priv = vo->priv;

    vo_get_src_dst_rects(vo, &priv->src_rect, &priv->dst_rect, &priv->osd);

    // priv->width and priv->height are the width and height of dst_rect
    // and they are not changed anywhere else outside this function.
    // It is the sixel image output dimension which is output by libsixel.
    priv->width  = priv->dst_rect.x1 - priv->dst_rect.x0;
    priv->height = priv->dst_rect.y1 - priv->dst_rect.y0;

    // top/left values must be greater than 1. If it is set, then
    // the image will be rendered from there and no further centering is done.
    priv->top  = (priv->opt_top  > 0) ?  priv->opt_top :
                  priv->num_rows * priv->dst_rect.y0 / vo->dheight + 1;
    priv->left = (priv->opt_left > 0) ?  priv->opt_left :
                  priv->num_cols * priv->dst_rect.x0 / vo->dwidth  + 1;
}

static int update_sixel_swscaler(struct vo *vo, struct mp_image_params *params)
{
    struct priv *priv = vo->priv;

    priv->sws->src = *params;
    priv->sws->src.w = mp_rect_w(priv->src_rect);
    priv->sws->src.h = mp_rect_h(priv->src_rect);
    priv->sws->dst = (struct mp_image_params) {
        .imgfmt = IMGFMT,
        .w = priv->width,
        .h = priv->height,
        .p_w = 1,
        .p_h = 1,
    };

    dealloc_dithers_and_buffers(vo);

    priv->frame = mp_image_alloc(IMGFMT, priv->width, priv->height);
    if (!priv->frame)
        return -1;

    if (mp_sws_reinit(priv->sws) < 0)
        return -1;

    // create testdither only if dynamic palette mode is set
    if (!priv->opt_fixedpal) {
        SIXELSTATUS status = sixel_dither_new(&priv->testdither,
                                              priv->opt_reqcolors, NULL);
        if (SIXEL_FAILED(status)) {
            MP_ERR(vo, "update_sixel_swscaler: Failed to create new dither: %s\n",
                   sixel_helper_format_error(status));
            return -1;
        }
    }

    priv->buffer =
        talloc_array(NULL, uint8_t, depth * priv->width * priv->height);

    return 0;
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *priv = vo->priv;
    int ret = 0;
    update_canvas_dimensions(vo);
    if (priv->canvas_ok) {  // if too small - succeed but skip the rendering
        set_sixel_output_parameters(vo);
        ret = update_sixel_swscaler(vo, params);
    }

    printf(ESC_CLEAR_SCREEN);
    vo->want_redraw = true;

    return ret;
}

static void draw_frame(struct vo *vo, struct vo_frame *frame)
{
    struct priv *priv = vo->priv;
    SIXELSTATUS status;
    struct mp_image *mpi = NULL;

    int  prev_rows   = priv->num_rows;
    int  prev_cols   = priv->num_cols;
    int  prev_height = vo->dheight;
    int  prev_width  = vo->dwidth;
    bool resized     = false;
    update_canvas_dimensions(vo);
    if (!priv->canvas_ok)
        return;

    if (prev_rows != priv->num_rows || prev_cols != priv->num_cols ||
        prev_width != vo->dwidth || prev_height != vo->dheight)
    {
        set_sixel_output_parameters(vo);
        // Not checking for vo->config_ok because draw_frame is never called
        // with a failed reconfig.
        update_sixel_swscaler(vo, vo->params);

        printf(ESC_CLEAR_SCREEN);
        resized = true;
    }

    if (frame->repeat && !frame->redraw && !resized) {
        // Frame is repeated, and no need to update OSD either
        priv->skip_frame_draw = true;
        return;
    } else {
        // Either frame is new, or OSD has to be redrawn
        priv->skip_frame_draw = false;
    }

    // Normal case where we have to draw the frame and the image is not NULL
    if (frame->current) {
        mpi = mp_image_new_ref(frame->current);
        struct mp_rect src_rc = priv->src_rect;
        src_rc.x0 = MP_ALIGN_DOWN(src_rc.x0, mpi->fmt.align_x);
        src_rc.y0 = MP_ALIGN_DOWN(src_rc.y0, mpi->fmt.align_y);
        mp_image_crop_rc(mpi, src_rc);

        // scale/pan to our dest rect
        mp_sws_scale(priv->sws, priv->frame, mpi);
    } else {
        // Image is NULL, so need to clear image and draw OSD
        mp_image_clear(priv->frame, 0, 0, priv->width, priv->height);
    }

    struct mp_osd_res dim = {
        .w = priv->width,
        .h = priv->height
    };
    osd_draw_on_image(vo->osd, dim, mpi ? mpi->pts : 0, 0, priv->frame);

    // Copy from mpv to RGB format as required by libsixel
    memcpy_pic(priv->buffer, priv->frame->planes[0], priv->width * depth,
               priv->height, priv->width * depth, priv->frame->stride[0]);

    // Even if either of these prepare palette functions fail, on re-running them
    // they should try to re-initialize the dithers, so it shouldn't dereference
    // any NULL pointers. flip_page also has a check to make sure dither is not
    // NULL before drawing, so failure in these functions should still be okay.
    if (priv->opt_fixedpal) {
        status = prepare_static_palette(vo);
    } else {
        status = prepare_dynamic_palette(vo);
    }

    if (SIXEL_FAILED(status)) {
        MP_WARN(vo, "draw_frame: prepare_palette returned error: %s\n",
                sixel_helper_format_error(status));
    }

    if (mpi)
        talloc_free(mpi);
}

static int sixel_write(char *data, int size, void *priv)
{
    return fwrite(data, 1, size, (FILE *)priv);
}

static void flip_page(struct vo *vo)
{
    struct priv* priv = vo->priv;
    if (!priv->canvas_ok)
        return;

    // If frame is repeated and no update required, then we skip encoding
    if (priv->skip_frame_draw)
        return;

    // Make sure that image and dither are valid before drawing
    if (priv->buffer == NULL || priv->dither == NULL)
        return;

    // Go to the offset row and column, then display the image
    printf(ESC_GOTOXY, priv->top, priv->left);
    sixel_encode(priv->buffer, priv->width, priv->height,
                 depth, priv->dither, priv->output);
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
    if (SIXEL_FAILED(status)) {
        MP_ERR(vo, "preinit: Failed to create output file: %s\n",
               sixel_helper_format_error(status));
        return -1;
    }

    sixel_output_set_encode_policy(priv->output, SIXEL_ENCODEPOLICY_FAST);

    printf(ESC_HIDE_CURSOR);

    /* don't use private color registers for each frame. */
    printf(ESC_USE_GLOBAL_COLOR_REG);

    priv->dither = NULL;

    // create testdither only if dynamic palette mode is set
    if (!priv->opt_fixedpal) {
        status = sixel_dither_new(&priv->testdither, priv->opt_reqcolors, NULL);
        if (SIXEL_FAILED(status)) {
            MP_ERR(vo, "preinit: Failed to create new dither: %s\n",
                   sixel_helper_format_error(status));
            return -1;
        }
    }

    priv->previous_histgram_colors = 0;

    return 0;
}

static int query_format(struct vo *vo, int format)
{
    return format == IMGFMT;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    if (request == VOCTRL_SET_PANSCAN)
        return (vo->config_ok && !reconfig(vo, vo->params)) ? VO_TRUE : VO_FALSE;
    return VO_NOTIMPL;
}


static void uninit(struct vo *vo)
{
    struct priv *priv = vo->priv;

    printf(ESC_RESTORE_CURSOR);

    if (priv->opt_clear) {
        printf(ESC_CLEAR_SCREEN);
        printf(ESC_GOTOXY, 1, 1);
    }
    fflush(stdout);

    if (priv->output) {
        sixel_output_unref(priv->output);
        priv->output = NULL;
    }

    dealloc_dithers_and_buffers(vo);
}

#define OPT_BASE_STRUCT struct priv

const struct vo_driver video_out_sixel = {
    .name = "sixel",
    .description = "terminal graphics using sixels",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_frame = draw_frame,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .opt_diffuse = DIFFUSE_AUTO,
        .opt_width = 0,
        .opt_height = 0,
        .opt_reqcolors = 256,
        .opt_threshold = -1,
        .opt_fixedpal = 1,
        .opt_top = 0,
        .opt_left = 0,
        .opt_pad_y = -1,
        .opt_pad_x = -1,
        .opt_rows = 0,
        .opt_cols = 0,
        .opt_clear = 1,
    },
    .options = (const m_option_t[]) {
        {"dither", OPT_CHOICE(opt_diffuse,
            {"auto", DIFFUSE_AUTO},
            {"none", DIFFUSE_NONE},
            {"atkinson", DIFFUSE_ATKINSON},
            {"fs", DIFFUSE_FS},
            {"jajuni", DIFFUSE_JAJUNI},
            {"stucki", DIFFUSE_STUCKI},
            {"burkes", DIFFUSE_BURKES},
            {"arithmetic", DIFFUSE_A_DITHER},
            {"xor", DIFFUSE_X_DITHER})},
        {"width", OPT_INT(opt_width)},
        {"height", OPT_INT(opt_height)},
        {"reqcolors", OPT_INT(opt_reqcolors)},
        {"fixedpalette", OPT_FLAG(opt_fixedpal)},
        {"threshold", OPT_INT(opt_threshold)},
        {"top", OPT_INT(opt_top)},
        {"left", OPT_INT(opt_left)},
        {"pad-y", OPT_INT(opt_pad_y)},
        {"pad-x", OPT_INT(opt_pad_x)},
        {"rows", OPT_INT(opt_rows)},
        {"cols", OPT_INT(opt_cols)},
        {"exit-clear", OPT_FLAG(opt_clear), },
        {0}
    },
    .options_prefix = "vo-sixel",
};
