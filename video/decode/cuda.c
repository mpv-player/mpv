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
    ctx->hwdec_priv = hwdec_devices_get(ctx->hwdec_devs, HWDEC_CUDA)->ctx;
    return 0;
}

static int init_decoder(struct lavc_ctx *ctx, int w, int h)
{
    AVCodecContext *avctx = ctx->avctx;
    AVCUDADeviceContext *device_hwctx;
    AVHWDeviceContext *device_ctx;
    AVHWFramesContext *hwframe_ctx;
    int ret = 0;

    if (avctx->hw_frames_ctx) {
        MP_ERR(ctx, "hw_frames_ctx already initialised!\n");
        return -1;
    }

    AVBufferRef *hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA);
    if (!hw_device_ctx) {
        MP_WARN(ctx, "av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA) failed\n");
        goto error;
    }

    device_ctx = (AVHWDeviceContext*)hw_device_ctx->data;

    device_hwctx = device_ctx->hwctx;
    device_hwctx->cuda_ctx = ctx->hwdec_priv;

    ret = av_hwdevice_ctx_init(hw_device_ctx);
    if (ret < 0) {
        MP_ERR(ctx, "av_hwdevice_ctx_init failed\n");
        goto error;
    }

    avctx->hw_frames_ctx = av_hwframe_ctx_alloc(hw_device_ctx);
    if (!avctx->hw_frames_ctx) {
        MP_ERR(ctx, "av_hwframe_ctx_alloc failed\n");
        goto error;
    }

    hwframe_ctx = (AVHWFramesContext*)avctx->hw_frames_ctx->data;
    hwframe_ctx->format = AV_PIX_FMT_CUDA;

    return 0;

 error:
    av_buffer_unref(&avctx->hw_frames_ctx);
    av_buffer_unref(&hw_device_ctx);
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
