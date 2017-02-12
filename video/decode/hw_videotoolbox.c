/*
 * This file is part of mpv.
 *
 * Copyright (c) 2015 Sebastien Zwickert
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

#include <libavcodec/version.h>
#include <libavcodec/videotoolbox.h>

#include "common/av_common.h"
#include "common/msg.h"
#include "video/mp_image.h"
#include "video/decode/lavc.h"
#include "video/mp_image_pool.h"
#include "config.h"

struct priv {
    struct mp_image_pool *sw_pool;
};

static int probe_copy(struct lavc_ctx *ctx, struct vd_lavc_hwdec *hwdec,
                 const char *codec)
{
    switch (mp_codec_to_av_codec_id(codec)) {
    case AV_CODEC_ID_H264:
    case AV_CODEC_ID_H263:
    case AV_CODEC_ID_MPEG1VIDEO:
    case AV_CODEC_ID_MPEG2VIDEO:
    case AV_CODEC_ID_MPEG4:
        break;
    default:
        return HWDEC_ERR_NO_CODEC;
    }
    return 0;
}

static int probe(struct lavc_ctx *ctx, struct vd_lavc_hwdec *hwdec,
                 const char *codec)
{
    if (!hwdec_devices_load(ctx->hwdec_devs, HWDEC_VIDEOTOOLBOX))
        return HWDEC_ERR_NO_CTX;
    return probe_copy(ctx, hwdec, codec);
}

static int init(struct lavc_ctx *ctx)
{
    struct priv *p = talloc_ptrtype(NULL, p);
    p->sw_pool = talloc_steal(p, mp_image_pool_new(17));
    ctx->hwdec_priv = p;
    return 0;
}

struct videotoolbox_error {
    int  code;
    char *reason;
};

static const struct videotoolbox_error videotoolbox_errors[] = {
    { AVERROR(ENOSYS),
        "Hardware doesn't support accelerated decoding for this stream"
        " or Videotoolbox decoder is not available at the moment (another"
        " application is using it)."
    },
    { AVERROR(EINVAL),
        "Invalid configuration provided to VTDecompressionSessionCreate" },
    { AVERROR_INVALIDDATA,
        "Generic error returned by the decoder layer. The cause can be Videotoolbox"
        " found errors in the bitstream." },
    { 0, NULL },
};

static void print_videotoolbox_error(struct mp_log *log, int lev, char *message,
                            int error_code)
{
    for (int n = 0; videotoolbox_errors[n].code < 0; n++)
        if (videotoolbox_errors[n].code == error_code) {
            mp_msg(log, lev, "%s: %s (%d)\n",
                   message, videotoolbox_errors[n].reason, error_code);
            return;
        }

    mp_msg(log, lev, "%s: %d\n", message, error_code);
}

static int init_decoder_common(struct lavc_ctx *ctx, int w, int h, AVVideotoolboxContext *vtctx)
{
    av_videotoolbox_default_free(ctx->avctx);

    int err = av_videotoolbox_default_init2(ctx->avctx, vtctx);
    if (err < 0) {
        print_videotoolbox_error(ctx->log, MSGL_ERR, "failed to init videotoolbox decoder", err);
        return -1;
    }

    return 0;
}

static int init_decoder(struct lavc_ctx *ctx, int w, int h)
{
    AVVideotoolboxContext *vtctx = av_videotoolbox_alloc_context();
    struct mp_vt_ctx *vt = hwdec_devices_load(ctx->hwdec_devs, HWDEC_VIDEOTOOLBOX);
    vtctx->cv_pix_fmt_type = vt->get_vt_fmt(vt);

    return init_decoder_common(ctx, w, h, vtctx);
}

static int init_decoder_copy(struct lavc_ctx *ctx, int w, int h)
{
    return init_decoder_common(ctx, w, h, NULL);
}

static void uninit(struct lavc_ctx *ctx)
{
    if (ctx->avctx)
        av_videotoolbox_default_free(ctx->avctx);

    struct priv *p = ctx->hwdec_priv;
    if (!p)
        return;

    talloc_free(p->sw_pool);
    p->sw_pool = NULL;

    talloc_free(p);
    ctx->hwdec_priv = NULL;
}

static int mp_imgfmt_from_cvpixelformat(uint32_t cvpixfmt)
{
    switch (cvpixfmt) {
    case kCVPixelFormatType_420YpCbCr8Planar:               return IMGFMT_420P;
    case kCVPixelFormatType_422YpCbCr8:                     return IMGFMT_UYVY;
    case kCVPixelFormatType_32BGRA:                         return IMGFMT_RGB0;
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:   return IMGFMT_NV12;
    }
    return 0;
}

static struct mp_image *copy_image(struct lavc_ctx *ctx, struct mp_image *hw_image)
{
    if (hw_image->imgfmt != IMGFMT_VIDEOTOOLBOX)
        return hw_image;

    struct priv *p = ctx->hwdec_priv;
    struct mp_image *image = NULL;
    CVPixelBufferRef pbuf = (CVPixelBufferRef)hw_image->planes[3];
    CVPixelBufferLockBaseAddress(pbuf, kCVPixelBufferLock_ReadOnly);
    size_t width  = CVPixelBufferGetWidth(pbuf);
    size_t height = CVPixelBufferGetHeight(pbuf);
    uint32_t cvpixfmt = CVPixelBufferGetPixelFormatType(pbuf);
    int pixfmt = mp_imgfmt_from_cvpixelformat(cvpixfmt);
    if (!pixfmt)
        goto unlock;

    struct mp_image img = {0};
    mp_image_setfmt(&img, pixfmt);
    mp_image_set_size(&img, width, height);

    if (CVPixelBufferIsPlanar(pbuf)) {
        int planes = CVPixelBufferGetPlaneCount(pbuf);
        for (int i = 0; i < planes; i++) {
            img.planes[i] = CVPixelBufferGetBaseAddressOfPlane(pbuf, i);
            img.stride[i] = CVPixelBufferGetBytesPerRowOfPlane(pbuf, i);
        }
    } else {
        img.planes[0] = CVPixelBufferGetBaseAddress(pbuf);
        img.stride[0] = CVPixelBufferGetBytesPerRow(pbuf);
    }

    mp_image_copy_attributes(&img, hw_image);

    image = mp_image_pool_new_copy(p->sw_pool, &img);

unlock:
    CVPixelBufferUnlockBaseAddress(pbuf, kCVPixelBufferLock_ReadOnly);

    if (image) {
        talloc_free(hw_image);
        return image;
    } else {
        return hw_image;
    }
}

static struct mp_image *process_image(struct lavc_ctx *ctx, struct mp_image *img)
{
    if (img->imgfmt == IMGFMT_VIDEOTOOLBOX) {
        CVPixelBufferRef pbuf = (CVPixelBufferRef)img->planes[3];
        uint32_t cvpixfmt = CVPixelBufferGetPixelFormatType(pbuf);
        img->params.hw_subfmt = mp_imgfmt_from_cvpixelformat(cvpixfmt);
    }
    return img;
}

const struct vd_lavc_hwdec mp_vd_lavc_videotoolbox = {
    .type = HWDEC_VIDEOTOOLBOX,
    .image_format = IMGFMT_VIDEOTOOLBOX,
    .probe = probe,
    .init = init,
    .uninit = uninit,
    .init_decoder = init_decoder,
    .process_image = process_image,
};

const struct vd_lavc_hwdec mp_vd_lavc_videotoolbox_copy = {
    .type = HWDEC_VIDEOTOOLBOX_COPY,
    .copying = true,
    .image_format = IMGFMT_VIDEOTOOLBOX,
    .probe = probe_copy,
    .init = init,
    .uninit = uninit,
    .init_decoder = init_decoder_copy,
    .process_image = copy_image,
    .delay_queue = HWDEC_DELAY_QUEUE_COUNT,
};
