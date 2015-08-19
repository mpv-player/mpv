/*
 * This file is part of mpv.
 *
 * Copyright (c) 2015 Sebastien Zwickert
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libavcodec/version.h>
#include <libavcodec/videotoolbox.h>

#include "common/av_common.h"
#include "common/msg.h"
#include "video/mp_image.h"
#include "video/decode/lavc.h"
#include "config.h"


static int probe(struct vd_lavc_hwdec *hwdec, struct mp_hwdec_info *info,
                 const char *decoder)
{
    hwdec_request_api(info, "videotoolbox");
    if (!info || !info->hwctx)
        return HWDEC_ERR_NO_CTX;
    switch (mp_codec_to_av_codec_id(decoder)) {
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

static int init(struct lavc_ctx *ctx)
{
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
    vtctx->cv_pix_fmt_type = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
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
}

const struct vd_lavc_hwdec mp_vd_lavc_videotoolbox = {
    .type = HWDEC_VIDEOTOOLBOX,
    .image_format = IMGFMT_VIDEOTOOLBOX,
    .probe = probe,
    .init = init,
    .uninit = uninit,
    .init_decoder = init_decoder,
};
