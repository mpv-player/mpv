/*
 * Copyright (c) 2012 Rudolf Polzer <divVerent@xonotic.org>
 *
 * This file is part of mpv's vf_dlopen examples.
 *
 * mpv's vf_dlopen examples are free software; you can redistribute them and/or
 * modify them under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * mpv's vf_dlopen examples are distributed in the hope that they will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv's vf_dlopen examples; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf_dlopen.h"
#include "filterutils.h"

/*
 * tile filter
 *
 * usage: --vf=dlopen=/path/to/tile.so:4:3
 *
 * only supports rgb24 and yuv420p for now
 * in theory can support any format where rows are a multiple of bytes, and the
 * multiple is known
 */

#define ALLFORMATS \
    /*     format    bytes   xmul   ymul */ \
    FORMAT("rgb24"  ,    3,     1,     1) \
    FORMAT("yuv420p",    1,     2,     2)

typedef struct {
    int rows, cols;
    unsigned char *buffer_plane[4];
    size_t buffer_size[4];
    int pos;
    int pixelbytes;
} tile_data_t;

static int tile_config(struct vf_dlopen_context *ctx)
{
    // we may return more than one pic!
    tile_data_t *tile = ctx->priv;

    ctx->out_width = tile->cols * ctx->in_width;
    ctx->out_height = tile->rows * ctx->in_height;
    ctx->out_d_width = tile->cols * ctx->in_d_width;
    ctx->out_d_height = tile->rows * ctx->in_d_height;

#define FORMAT(fmt,sz,xmul,ymul) \
        if (!strcmp(ctx->in_fmt, fmt)) { \
            if (ctx->in_width % xmul || ctx->in_height % ymul) { \
                printf("Format " fmt " requires width to be a multiple of %d and height to be a multiple of %d\n", \
                       xmul, ymul); \
                return -1; \
            } \
            tile->pixelbytes = sz; \
        } else
    ALLFORMATS
#undef FORMAT
    {
        printf("Format %s is not in the list, how come?\n", ctx->in_fmt);
        return -1;
    }

    return 1;
}

static int tile_put_image(struct vf_dlopen_context *ctx)
{
    tile_data_t *tile = ctx->priv;

    unsigned p;
    unsigned np = ctx->outpic[0].planes;
    assert(ctx->inpic.planes == ctx->outpic[0].planes);

    // fix buffers
    for (p = 0; p < np; ++p) {
        size_t sz = ctx->outpic->planestride[p] * ctx->outpic->planeheight[p];
        if (sz != tile->buffer_size[p]) {
            if (p == 0 && tile->buffer_plane[p])
                printf("WARNING: reinitializing output buffers.\n");
            tile->buffer_plane[p] = realloc(tile->buffer_plane[p], sz);
            tile->buffer_size[p] = sz;
            tile->pos = 0;
        }
    }

    for (p = 0; p < np; ++p) {
        assert(ctx->inpic.planewidth[p] * tile->cols == ctx->outpic->planewidth[p]);
        assert(ctx->inpic.planeheight[p] * tile->rows == ctx->outpic->planeheight[p]);
    }

    // copy this frame
    for (p = 0; p < np; ++p)
        copy_plane(
            &tile->buffer_plane[p][ctx->outpic->planestride[p] * ctx->inpic.planeheight[p] * (tile->pos / tile->cols) + tile->pixelbytes * ctx->inpic.planewidth[p] * (tile->pos % tile->cols)],
            ctx->outpic->planestride[p],
            ctx->inpic.plane[p],
            ctx->inpic.planestride[p],
            tile->pixelbytes * ctx->inpic.planewidth[p],
            ctx->inpic.planeheight[p]
            );

    ++tile->pos;
    if (tile->pos == tile->rows * tile->cols) {
        // copy THIS image to the buffer, we need it later
        for (p = 0; p < np; ++p)
            copy_plane(
                ctx->outpic->plane[p], ctx->outpic->planestride[p],
                &tile->buffer_plane[p][0], ctx->outpic->planestride[p],
                tile->pixelbytes * ctx->outpic->planewidth[p],
                ctx->outpic->planeheight[p]
                );
        ctx->outpic->pts = ctx->inpic.pts;
        tile->pos = 0;
        return 1;
    }

    return 0;
}

void tile_uninit(struct vf_dlopen_context *ctx)
{
    tile_data_t *tile = ctx->priv;
    free(tile->buffer_plane[3]);
    free(tile->buffer_plane[2]);
    free(tile->buffer_plane[1]);
    free(tile->buffer_plane[0]);
    free(tile);
}

int vf_dlopen_getcontext(struct vf_dlopen_context *ctx, int argc, const char **argv)
{
    VF_DLOPEN_CHECK_VERSION(ctx);

    if (argc != 2)
        return -1;

    tile_data_t *tile = calloc(1,sizeof(tile_data_t));

    tile->cols = atoi(argv[0]);
    tile->rows = atoi(argv[1]);

    if (!tile->rows || !tile->cols) {
        printf("tile: invalid rows/cols\n");
        free(tile);
        return -1;
    }

    ctx->priv = tile;
    static struct vf_dlopen_formatpair map[] = {
#define FORMAT(fmt,sz,xmul,ymul) {fmt, NULL},
        ALLFORMATS
#undef FORMAT
        {NULL, NULL}
    };
    ctx->format_mapping = map;
    ctx->config = tile_config;
    ctx->put_image = tile_put_image;
    ctx->uninit = tile_uninit;

    return 1;
}
