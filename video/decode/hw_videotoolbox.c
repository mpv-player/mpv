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
#include "options/options.h"
#include "video/mp_image.h"
#include "video/decode/lavc.h"
#include "video/mp_image_pool.h"
#include "video/vt.h"
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

static int init_decoder(struct lavc_ctx *ctx, int w, int h)
{
    av_videotoolbox_default_free(ctx->avctx);

    AVVideotoolboxContext *vtctx = av_videotoolbox_alloc_context();
    if (!vtctx)
        return -1;

    int imgfmt = ctx->opts->videotoolbox_format;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 81, 103)
    if (!imgfmt)
        imgfmt = IMGFMT_NV12;
#endif
    vtctx->cv_pix_fmt_type = mp_imgfmt_to_cvpixelformat(imgfmt);
    MP_VERBOSE(ctx, "Requesting cv_pix_fmt_type=0x%x\n",
               (unsigned)vtctx->cv_pix_fmt_type);

    int err = av_videotoolbox_default_init2(ctx->avctx, vtctx);
    if (err < 0) {
        print_videotoolbox_error(ctx->log, MSGL_ERR, "failed to init videotoolbox decoder", err);
        return -1;
    }

    return 0;
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

static struct mp_image *copy_image(struct lavc_ctx *ctx, struct mp_image *hw_image)
{
    struct priv *p = ctx->hwdec_priv;

    struct mp_image *image = mp_vt_download_image(NULL, hw_image, p->sw_pool);
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
    .init_decoder = init_decoder,
    .process_image = copy_image,
    .delay_queue = HWDEC_DELAY_QUEUE_COUNT,
};
