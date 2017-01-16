/*
 * This file is part of mpv.
 *
 * Copyright (c) 2016 Philip Langdale <philipl@overt.org>
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

// This define and typedef prevent hwcontext_cuda.h trying to include cuda.h
#define CUDA_VERSION 7050
typedef void * CUcontext;

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>

#include "common/av_common.h"
#include "video/fmt-conversion.h"
#include "video/decode/lavc.h"

static int probe(struct lavc_ctx *ctx, struct vd_lavc_hwdec *hwdec,
                 const char *codec)
{
    if (!hwdec_devices_load(ctx->hwdec_devs, HWDEC_CUDA))
        return HWDEC_ERR_NO_CTX;
    return 0;
}

static int init(struct lavc_ctx *ctx)
{
    ctx->hwdec_priv = hwdec_devices_get(ctx->hwdec_devs, HWDEC_CUDA);
    return 0;
}

static int init_decoder(struct lavc_ctx *ctx, int w, int h)
{
    AVCodecContext *avctx = ctx->avctx;
    struct mp_hwdec_ctx *hwctx = ctx->hwdec_priv;

    if (avctx->hw_frames_ctx) {
        MP_ERR(ctx, "hw_frames_ctx already initialised!\n");
        return -1;
    }

    avctx->hw_frames_ctx = av_hwframe_ctx_alloc(hwctx->av_device_ref);
    if (!avctx->hw_frames_ctx) {
        MP_ERR(ctx, "av_hwframe_ctx_alloc failed\n");
        goto error;
    }

    AVHWFramesContext *hwframe_ctx = (void* )avctx->hw_frames_ctx->data;
    hwframe_ctx->format = AV_PIX_FMT_CUDA;

    // This is proper use of the hw_frames_ctx API, but it does not work
    // (appaears to work but fails e.g. with 10 bit). The cuvid wrapper
    // does non-standard things, and it's a meesy situation.
    /*
    hwframe_ctx->width = w;
    hwframe_ctx->height = h;
    hwframe_ctx->sw_format = avctx->sw_pix_fmt;

    if (av_hwframe_ctx_init(avctx->hw_frames_ctx) < 0)
        goto error;
    */

    return 0;

 error:
    av_buffer_unref(&avctx->hw_frames_ctx);
    return -1;
}

static void uninit(struct lavc_ctx *ctx)
{
    ctx->hwdec_priv = NULL;
}

static struct mp_image *process_image(struct lavc_ctx *ctx, struct mp_image *img)
{
    if (img->imgfmt == IMGFMT_CUDA)
        img->params.hw_subfmt = pixfmt2imgfmt(ctx->avctx->sw_pix_fmt);
    return img;
}

const struct vd_lavc_hwdec mp_vd_lavc_cuda = {
    .type = HWDEC_CUDA,
    .image_format = IMGFMT_CUDA,
    .lavc_suffix = "_cuvid",
    .probe = probe,
    .init = init,
    .uninit = uninit,
    .init_decoder = init_decoder,
    .process_image = process_image,
};
