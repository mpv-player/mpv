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

#include <stdbool.h>

#include <libavcodec/mediacodec.h>

#include "options/options.h"
#include "video/decode/lavc.h"

static int probe(struct lavc_ctx *ctx, struct vd_lavc_hwdec *hwdec,
                 const char *codec)
{
    if (ctx->opts->vo->WinID == 0)
        return HWDEC_ERR_NO_CTX;

    return 0;
}

static int init(struct lavc_ctx *ctx)
{
    return 0;
}

static int init_decoder(struct lavc_ctx *ctx, int w, int h)
{
    av_mediacodec_default_free(ctx->avctx);

    AVMediaCodecContext *mcctx = av_mediacodec_alloc_context();
    if (!mcctx)
        return -1;

    void *surface = (void *)(intptr_t)(ctx->opts->vo->WinID);
    return av_mediacodec_default_init(ctx->avctx, mcctx, surface);
}

static void uninit(struct lavc_ctx *ctx)
{
    if (ctx->avctx)
        av_mediacodec_default_free(ctx->avctx);
}

const struct vd_lavc_hwdec mp_vd_lavc_mediacodec = {
    .type = HWDEC_MEDIACODEC,
    .image_format = IMGFMT_MEDIACODEC,
    .lavc_suffix = "_mediacodec",
    .probe = probe,
    .init = init,
    .init_decoder = init_decoder,
    .uninit = uninit,
};

const struct vd_lavc_hwdec mp_vd_lavc_mediacodec_copy = {
    .type = HWDEC_MEDIACODEC_COPY,
    .lavc_suffix = "_mediacodec",
    .copying = true,
};
