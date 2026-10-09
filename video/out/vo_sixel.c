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

#include <libavutil/cpu.h>

#include "config.h"
#include "misc/bstr.h"
#include "misc/mp_assert.h"
#include "misc/thread_pool.h"
#include "options/m_config.h"
#include "osdep/terminal.h"
#include "osdep/threads.h"
#include "sub/osd.h"
#include "terminal_swapchain.h"
#include "vo.h"
#include "video/sws_utils.h"
#include "video/mp_image.h"

// Exported by every libsixel, but not in public headers.
SIXELAPI sixel_index_t *sixel_dither_apply_palette(sixel_dither_t *dither,
                                                   unsigned char *pixels,
                                                   int width, int height);

#define IMGFMT IMGFMT_RGB24

#define TERM_ESC_USE_GLOBAL_COLOR_REG   "\033[?1070l"

#define TERMINAL_FALLBACK_COLS      80
#define TERMINAL_FALLBACK_ROWS      25
#define TERMINAL_FALLBACK_PX_WIDTH  320
#define TERMINAL_FALLBACK_PX_HEIGHT 240

struct vo_sixel_opts {
    int diffuse;
    int reqcolors;
    bool fixedpal;
    int threshold;
    int width, height, top, left;
    int pad_y, pad_x;
    int rows, cols;
    bool config_clear, alt_screen;
    bool buffered;
};

// Rows above a slice that are dithered with it, so the error diffusion has
// settled when it reaches the slice, and a band below it, so the slice's
// last row diffuses its error like every other row instead of being the
// last row of the image. The indices of both are dropped before the slice
// is encoded.
#define SLICE_WARMUP 12
#define SLICE_TAIL 6

// A horizontal part of the canvas, encoded as a sixel image of its own.
struct slice {
    struct vo *vo;
    int y, h;               // pixel rows in the canvas
    sixel_allocator_t *allocator;
    sixel_output_t *output;
    sixel_dither_t *dither; // same palette as priv->dither, dithers the rows
    sixel_dither_t *indexed; // the same palette, encodes the dithered indices
    int palette_gen;        // priv->palette_gen the dithers were made for
    uint8_t *scratch;       // the slice with its extra rows, dithered here
    bstr raw;               // the encoded image
    SIXELSTATUS status;
};

struct priv {
    // User specified options
    struct vo_sixel_opts opts;

    // Internal data
    sixel_dither_t *dither;
    sixel_dither_t *testdither;
    uint8_t        *buffer;
    bool            skip_frame_draw;

    int left, top;  // image origin cell (1 based)
    int width, height;  // actual image px size - always reflects dst_rect.
    int num_cols, num_rows;  // terminal size in cells
    int cell_h;     // cell height in pixels, 0 if unknown
    int canvas_ok;  // whether canvas vo->dwidth and vo->dheight are positive

    struct mp_thread_pool *pool;
    struct slice *slices;
    int num_slices;
    int slice_rows;     // cell rows per slice
    int palette_gen;    // counts the dithers priv->dither pointed to
    uint8_t *calib;     // the centre of every 5-bit colour bucket
    mp_mutex lock;      // protects pending
    mp_cond cond;
    int pending;        // slices still being encoded
    struct terminal_swapchain *swapchain;

    int previous_histogram_colors;

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
    int previous_histogram_colors = priv->previous_histogram_colors;
    int histogram_colors = 0;

    // If threshold is set negative, then every frame must be a scene change
    if (priv->dither == NULL || priv->opts.threshold < 0)
        return 1;

    histogram_colors = sixel_dither_get_num_of_histogram_colors(priv->testdither);

    int color_difference_count = previous_histogram_colors - histogram_colors;
    color_difference_count = (color_difference_count > 0) ?  // abs value
                              color_difference_count : -color_difference_count;

    if (100 * color_difference_count >
        priv->opts.threshold * previous_histogram_colors)
    {
        priv->previous_histogram_colors = histogram_colors; // update history
        return 1;
    } else {
        return 0;
    }

}

static void free_slices(struct priv *priv)
{
    for (int i = 0; i < priv->num_slices; i++) {
        struct slice *s = &priv->slices[i];
        if (s->output)
            sixel_output_unref(s->output);
        if (s->dither)
            sixel_dither_unref(s->dither);
        if (s->indexed)
            sixel_dither_unref(s->indexed);
        if (s->allocator)
            sixel_allocator_unref(s->allocator);
        talloc_free(s->raw.start);
    }
    talloc_free(priv->slices);
    priv->slices = NULL;
    priv->num_slices = 0;
}

static void dealloc_dithers_and_buffers(struct vo* vo)
{
    struct priv* priv = vo->priv;

    free_slices(priv);

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

        sixel_dither_set_diffusion_type(priv->dither, priv->opts.diffuse);
        priv->palette_gen++;
    }

    sixel_dither_set_body_only(priv->dither, 0);
    return SIXEL_OK;
}

static SIXELSTATUS prepare_dynamic_palette(struct vo *vo)
{
    SIXELSTATUS status = SIXEL_FALSE;
    struct priv *priv = vo->priv;

    /* create histogram and construct color palette
     * with median cut algorithm. */
    status = sixel_dither_initialize(priv->testdither, priv->buffer,
                                     vo->dwidth, vo->dheight,
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
        priv->palette_gen++;
        status = sixel_dither_new(&priv->testdither, priv->opts.reqcolors, NULL);

        if (SIXEL_FAILED(status))
            return status;

        sixel_dither_set_diffusion_type(priv->dither, priv->opts.diffuse);
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
    num_rows = (priv->opts.rows > 0) ? priv->opts.rows : num_rows;
    num_cols = (priv->opts.cols > 0) ? priv->opts.cols : num_cols;

    // If the pad value is set in between 0 and width/2 - 1, then we
    // subtract from the detected width. Otherwise, we assume that the width
    // output must be a integer multiple of num_cols and accordingly set
    // total_width to be an integer multiple of num_cols. So in case the padding
    // added by terminal is less than the number of cells in that axis, then rounding
    // down will take care of correcting the detected width and remove padding.
    if (priv->opts.width > 0) {
        // option - set by the user, hard truth
        total_px_width = priv->opts.width;
    } else {
        if (total_px_width <= 0) {
                // ioctl failed to read terminal width
                total_px_width = TERMINAL_FALLBACK_PX_WIDTH;
        } else {
            if (priv->opts.pad_x >= 0 && priv->opts.pad_x < total_px_width / 2) {
                // explicit padding set by the user
                total_px_width -= (2 * priv->opts.pad_x);
            } else {
                // rounded "auto padding"
                total_px_width = total_px_width / num_cols * num_cols;
            }
        }
    }

    if (priv->opts.height > 0) {
        total_px_height = priv->opts.height;
    } else {
        if (total_px_height <= 0) {
            total_px_height = TERMINAL_FALLBACK_PX_HEIGHT;
        } else {
            if (priv->opts.pad_y >= 0 && priv->opts.pad_y < total_px_height / 2) {
                total_px_height -= (2 * priv->opts.pad_y);
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
    priv->cell_h = total_px_height % num_rows ? 0 : total_px_height / num_rows;

    priv->canvas_ok = vo->dwidth > 0 && vo->dheight > 0;
}

static void set_sixel_output_parameters(struct vo *vo)
{
    // This function sets output scaled size in priv->width, priv->height
    // and the scaling rectangles in pixels priv->src_rect, priv->dst_rect
    // as well as image positioning in cells priv->top, priv->left.
    struct priv *priv = vo->priv;

    vo_get_src_dst_rects(vo, &priv->src_rect, &priv->dst_rect, &priv->osd);

    // priv->width and priv->height are the width and height of dst_rect,
    // the scaled video inside the canvas that libsixel outputs.
    priv->width  = priv->dst_rect.x1 - priv->dst_rect.x0;
    priv->height = priv->dst_rect.y1 - priv->dst_rect.y0;

    // top/left values must be greater than 1. The canvas starts there.
    priv->top  = (priv->opts.top  > 0) ? priv->opts.top  : 1;
    priv->left = (priv->opts.left > 0) ? priv->opts.left : 1;
}

// Collects the encoded image in a slice's buffer, which keeps its memory.
static int sixel_buffer(char *data, int size, void *priv)
{
    bstr_xappend(NULL, priv, (bstr){data, size});
    return size;
}

// Slices start on a cell row, so they can be placed with the cursor, and
// hold whole sixel bands of 6 rows.
static int setup_slices(struct vo *vo)
{
    struct priv *priv = vo->priv;
    free_slices(priv);

    int px = vo->dheight;
    int rows = 0;
    if (priv->cell_h > 0) {
        // Whole sixel bands, at least three times the rows dithered around a slice.
        int unit = 6 / (priv->cell_h % 3 ? 1 : 3) / (priv->cell_h % 2 ? 1 : 2);
        rows = unit;
        while (rows * priv->cell_h < 3 * (SLICE_WARMUP + SLICE_TAIL))
            rows += unit;
        px = rows * priv->cell_h;
    }
    priv->slice_rows = rows;
    priv->num_slices = (vo->dheight + px - 1) / px;
    priv->slices = talloc_zero_array(NULL, struct slice, priv->num_slices);
    for (int i = 0; i < priv->num_slices; i++) {
        struct slice *s = &priv->slices[i];
        s->vo = vo;
        s->y = i * px;
        s->h = MPMIN(px, vo->dheight - s->y);
        s->palette_gen = -1;
        s->scratch = talloc_array(priv->slices, uint8_t,
                                  (SLICE_WARMUP + s->h + SLICE_TAIL) * vo->dwidth * depth);
        // The dithered indices come from the dither's allocator, and the
        // slice frees them with the same one.
        SIXELSTATUS status = sixel_allocator_new(&s->allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_SUCCEEDED(status))
            status = sixel_output_new(&s->output, sixel_buffer, &s->raw, s->allocator);
        if (SIXEL_FAILED(status)) {
            MP_ERR(vo, "Failed to create a sixel output: %s\n",
                   sixel_helper_format_error(status));
            return -1;
        }
        sixel_output_set_encode_policy(s->output, SIXEL_ENCODEPOLICY_FAST);
    }
    return 0;
}

static int sixel_discard(char *data, int size, void *priv)
{
    return size;
}

// libsixel caches the palette entry it chose for the first color it saw
// in each 5-bit colour bucket, so two dithers can quantize the same colour
// differently. Feed every bucket's centre first, so the slices, and all
// frames, choose alike. The dither is left without diffusion.
static SIXELSTATUS prime_cache(struct priv *priv, sixel_dither_t *dither)
{
    sixel_output_t *output = NULL;
    SIXELSTATUS status = sixel_output_new(&output, sixel_discard, NULL, NULL);
    if (SIXEL_FAILED(status))
        return status;
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_NONE);
    status = sixel_encode(priv->calib, 32 * 32, 32, depth, dither, output);
    sixel_output_unref(output);
    return status;
}

// Give the slice its dithers with the palette priv->dither has.
static SIXELSTATUS slice_dither(struct slice *s)
{
    struct vo *vo = s->vo;
    struct priv *priv = vo->priv;
    if (s->dither && s->palette_gen == priv->palette_gen)
        return SIXEL_OK;
    if (s->dither) {
        sixel_dither_unref(s->dither);
        s->dither = NULL;
    }
    if (s->indexed) {
        sixel_dither_unref(s->indexed);
        s->indexed = NULL;
    }

    // Only sixel_dither_initialize() gives a dither the colour cache of its
    // fast lookup, and it takes the palette from an image: one pixel per
    // colour, each in its own bucket of libsixel's 5-bit histogram, is kept
    // as it is, and the frame's palette replaces it afterwards.
    int ncolors = sixel_dither_get_num_of_palette_colors(priv->dither);
    unsigned char *palette = sixel_dither_get_palette(priv->dither);
    unsigned char buckets[SIXEL_PALETTE_MAX * 3];
    for (int i = 0; i < ncolors; i++) {
        buckets[i * 3] = (i >> 5) << 5;
        buckets[i * 3 + 1] = ((i >> 2) & 7) << 5;
        buckets[i * 3 + 2] = (i & 3) << 6;
    }
    SIXELSTATUS status = sixel_dither_new(&s->dither, ncolors, s->allocator);
    if (SIXEL_FAILED(status))
        return status;
    status = sixel_dither_initialize(s->dither, buckets, ncolors, 1,
                                     SIXEL_PIXELFORMAT_RGB888,
                                     LARGE_NORM, REP_CENTER_BOX, QUALITY_LOW);
    if (SIXEL_FAILED(status))
        return status;
    mp_assert(sixel_dither_get_num_of_palette_colors(s->dither) == ncolors);
    sixel_dither_set_palette(s->dither, palette);
    // A dynamic palette is rebuilt on every change, by default every
    // frame, so only the fixed one is worth priming.
    if (priv->opts.fixedpal) {
        status = prime_cache(priv, s->dither);
        if (SIXEL_FAILED(status))
            return status;
    }
    sixel_dither_set_diffusion_type(s->dither, priv->opts.diffuse);

    // The encoder takes the palette and the indices from this one.
    status = sixel_dither_new(&s->indexed, ncolors, s->allocator);
    if (SIXEL_FAILED(status))
        return status;
    sixel_dither_set_pixelformat(s->indexed, SIXEL_PIXELFORMAT_PAL8);
    sixel_dither_set_palette(s->indexed, palette);
    sixel_dither_set_body_only(s->indexed, 0);
    s->palette_gen = priv->palette_gen;
    return status;
}

static void encode_slice(void *ctx)
{
    struct slice *s = ctx;
    struct vo *vo = s->vo;
    struct priv *priv = vo->priv;

    s->raw.len = 0;
    s->status = slice_dither(s);
    if (SIXEL_SUCCEEDED(s->status)) {
        // Dither the slice with the rows around it, from a copy, since the
        // diffusion writes into the image and those rows belong to other
        // slices being encoded at the same time, then encode the slice's
        // own rows of the indices.
        int warm = MPMIN(SLICE_WARMUP, s->y);
        int tail = s->y + s->h < vo->dheight ? SLICE_TAIL : 0;
        size_t stride = vo->dwidth * depth;
        memcpy_pic(s->scratch,
                   priv->frame->planes[0] + (s->y - warm) * priv->frame->stride[0],
                   stride, warm + s->h + tail, stride, priv->frame->stride[0]);
        sixel_index_t *indices = sixel_dither_apply_palette(s->dither, s->scratch,
                                                            vo->dwidth,
                                                            warm + s->h + tail);
        if (indices) {
            s->status = sixel_encode(indices + warm * vo->dwidth, vo->dwidth,
                                     s->h, 1, s->indexed, s->output);
            sixel_allocator_free(s->allocator, indices);
        } else {
            s->status = SIXEL_RUNTIME_ERROR;
        }
    }

    mp_mutex_lock(&priv->lock);
    priv->pending--;
    mp_cond_broadcast(&priv->cond);
    mp_mutex_unlock(&priv->lock);
}

static void encode_slices(struct vo *vo)
{
    struct priv *priv = vo->priv;

    priv->pending = priv->num_slices;
    for (int i = 0; i < priv->num_slices; i++) {
        if (!mp_thread_pool_queue(priv->pool, encode_slice, &priv->slices[i]))
            encode_slice(&priv->slices[i]);
    }
    mp_mutex_lock(&priv->lock);
    while (priv->pending)
        mp_cond_wait(&priv->cond, &priv->lock);
    mp_mutex_unlock(&priv->lock);

    for (int i = 0; i < priv->num_slices; i++) {
        if (SIXEL_FAILED(priv->slices[i].status)) {
            MP_WARN(vo, "Failed to encode the frame: %s\n",
                    sixel_helper_format_error(priv->slices[i].status));
            break;
        }
    }
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

    priv->frame = mp_image_alloc(IMGFMT, vo->dwidth, vo->dheight);
    if (!priv->frame)
        return -1;
    mp_image_clear(priv->frame, 0, 0, priv->frame->w, priv->frame->h);

    if (mp_sws_reinit(priv->sws) < 0)
        return -1;

    // create testdither only if dynamic palette mode is set
    if (!priv->opts.fixedpal) {
        SIXELSTATUS status = sixel_dither_new(&priv->testdither,
                                              priv->opts.reqcolors, NULL);
        if (SIXEL_FAILED(status)) {
            MP_ERR(vo, "update_sixel_swscaler: Failed to create new dither: %s\n",
                   sixel_helper_format_error(status));
            return -1;
        }
    }

    if (!priv->opts.fixedpal) {
        priv->buffer =
            talloc_array(NULL, uint8_t, depth * vo->dwidth * vo->dheight);
    }

    return setup_slices(vo);
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

    if (priv->opts.config_clear) {
        bstr_xappend(NULL, terminal_swapchain_next(priv->swapchain),
                     (bstr)bstr0_lit(TERM_ESC_CLEAR_SCREEN));
    }
    vo->want_redraw = true;

    return ret;
}

static bool draw_frame(struct vo *vo, struct vo_frame *frame)
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
        goto done;

    if (prev_rows != priv->num_rows || prev_cols != priv->num_cols ||
        prev_width != vo->dwidth || prev_height != vo->dheight)
    {
        set_sixel_output_parameters(vo);
        // Not checking for vo->config_ok because draw_frame is never called
        // with a failed reconfig.
        update_sixel_swscaler(vo, vo->params);

        if (priv->opts.config_clear) {
            bstr_xappend(NULL, terminal_swapchain_next(priv->swapchain),
                         (bstr)bstr0_lit(TERM_ESC_CLEAR_SCREEN));
        }
        resized = true;
    }

    if (frame->repeat && !frame->redraw && !resized) {
        // Frame is repeated, and no need to update OSD either
        priv->skip_frame_draw = true;
        goto done;
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

        // scale/pan to our dest rect inside the canvas
        mp_image_clear_rc_inv(priv->frame, priv->dst_rect);
        struct mp_image dst = *priv->frame;
        mp_image_crop_rc(&dst, priv->dst_rect);
        mp_sws_scale(priv->sws, &dst, mpi);
    } else {
        // Image is NULL, so need to clear image and draw OSD
        mp_image_clear(priv->frame, 0, 0, priv->frame->w, priv->frame->h);
    }

    osd_draw_on_image(vo->osd, priv->osd, mpi ? mpi->pts : 0, 0, priv->frame);

    // The slices read the image directly, only the histogram of the dynamic
    // palette needs it as one packed buffer.
    if (!priv->opts.fixedpal) {
        memcpy_pic(priv->buffer, priv->frame->planes[0], priv->frame->w * depth,
                   priv->frame->h, priv->frame->w * depth, priv->frame->stride[0]);
    }

    // Even if either of these prepare palette functions fail, on re-running them
    // they should try to re-initialize the dithers, so it shouldn't dereference
    // any NULL pointers. flip_page also has a check to make sure dither is not
    // NULL before drawing, so failure in these functions should still be okay.
    if (priv->opts.fixedpal) {
        status = prepare_static_palette(vo);
    } else {
        status = prepare_dynamic_palette(vo);
    }

    if (SIXEL_FAILED(status)) {
        MP_WARN(vo, "draw_frame: prepare_palette returned error: %s\n",
                sixel_helper_format_error(status));
    } else {
        encode_slices(vo);
    }

    if (mpi)
        talloc_free(mpi);

done:
    return VO_TRUE;
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
    if (!priv->frame || !priv->dither)
        return;

    // Go to the row and column of each slice, then display it
    bstr *out = terminal_swapchain_acquire(priv->swapchain);
    for (int i = 0; i < priv->num_slices; i++) {
        struct slice *s = &priv->slices[i];
        if (SIXEL_FAILED(s->status) || !s->raw.len)
            continue;
        bstr_xappend_asprintf(NULL, out, TERM_ESC_GOTO_YX,
                              priv->top + i * priv->slice_rows, priv->left);
        bstr_xappend(NULL, out, s->raw);
    }
    terminal_swapchain_present(priv->swapchain, out);
}

static int preinit(struct vo *vo)
{
    struct priv *priv = vo->priv;
    SIXELSTATUS status = SIXEL_FALSE;

    // Parse opts set by CLI or conf
    priv->sws = mp_sws_alloc(vo);
    priv->sws->log = vo->log;
    mp_sws_enable_cmdline_opts(priv->sws, vo->global);

    mp_mutex_init(&priv->lock);
    mp_cond_init(&priv->cond);
    priv->calib = talloc_array(priv, uint8_t, 32 * 32 * 32 * depth);
    for (int i = 0; i < 32 * 32 * 32; i++) {
        priv->calib[i * depth + 0] = (i >> 10) << 3 | 4;
        priv->calib[i * depth + 1] = ((i >> 5) & 31) << 3 | 4;
        priv->calib[i * depth + 2] = (i & 31) << 3 | 4;
    }
    int threads = av_cpu_count() + 1;
    priv->pool = mp_thread_pool_create(priv, 0, 1, MPMAX(threads, 1));
    priv->swapchain = terminal_swapchain_create(vo);

    bstr *out = terminal_swapchain_acquire(priv->swapchain);
    if (priv->opts.alt_screen)
        bstr_xappend(NULL, out, (bstr)bstr0_lit(TERM_ESC_ALT_SCREEN));
    bstr_xappend(NULL, out, (bstr)bstr0_lit(TERM_ESC_HIDE_CURSOR));
    /* don't use private color registers for each frame. */
    bstr_xappend(NULL, out, (bstr)bstr0_lit(TERM_ESC_USE_GLOBAL_COLOR_REG));
    terminal_swapchain_present(priv->swapchain, out);
    terminal_set_mouse_input(true);

    priv->dither = NULL;

    // create testdither only if dynamic palette mode is set
    if (!priv->opts.fixedpal) {
        status = sixel_dither_new(&priv->testdither, priv->opts.reqcolors, NULL);
        if (SIXEL_FAILED(status)) {
            MP_ERR(vo, "preinit: Failed to create new dither: %s\n",
                   sixel_helper_format_error(status));
            return -1;
        }
    }

    priv->previous_histogram_colors = 0;

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

    bstr *out = terminal_swapchain_acquire(priv->swapchain);
    bstr_xappend(NULL, out, (bstr)bstr0_lit(TERM_ESC_RESTORE_CURSOR));
    if (priv->opts.alt_screen)
        bstr_xappend(NULL, out, (bstr)bstr0_lit(TERM_ESC_NORMAL_SCREEN));
    terminal_swapchain_present(priv->swapchain, out);
    terminal_set_mouse_input(false);
    terminal_swapchain_destroy(priv->swapchain);
    priv->swapchain = NULL;

    talloc_free(priv->pool);
    priv->pool = NULL;
    dealloc_dithers_and_buffers(vo);
    mp_cond_destroy(&priv->cond);
    mp_mutex_destroy(&priv->lock);
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
        .opts.diffuse = DIFFUSE_AUTO,
        .opts.reqcolors = 256,
        .opts.threshold = -1,
        .opts.fixedpal = true,
        .opts.pad_y = -1,
        .opts.pad_x = -1,
        .opts.config_clear = true,
        .opts.alt_screen = true,
    },
    .options = (const m_option_t[]) {
        {"dither", OPT_CHOICE(opts.diffuse,
            {"auto", DIFFUSE_AUTO},
            {"none", DIFFUSE_NONE},
            {"atkinson", DIFFUSE_ATKINSON},
            {"fs", DIFFUSE_FS},
            {"jajuni", DIFFUSE_JAJUNI},
            {"stucki", DIFFUSE_STUCKI},
            {"burkes", DIFFUSE_BURKES},
            {"arithmetic", DIFFUSE_A_DITHER},
            {"xor", DIFFUSE_X_DITHER})},
        {"width", OPT_INT(opts.width)},
        {"height", OPT_INT(opts.height)},
        {"reqcolors", OPT_INT(opts.reqcolors)},
        {"fixedpalette", OPT_BOOL(opts.fixedpal)},
        {"threshold", OPT_INT(opts.threshold)},
        {"top", OPT_INT(opts.top)},
        {"left", OPT_INT(opts.left)},
        {"pad-y", OPT_INT(opts.pad_y)},
        {"pad-x", OPT_INT(opts.pad_x)},
        {"rows", OPT_INT(opts.rows)},
        {"cols", OPT_INT(opts.cols)},
        {"config-clear", OPT_BOOL(opts.config_clear), },
        {"alt-screen", OPT_BOOL(opts.alt_screen), },
        {"buffered", OPT_BOOL(opts.buffered),
            .deprecation_message = "frames are written in one piece"},
        {0}
    },
    .options_prefix = "vo-sixel",
};
