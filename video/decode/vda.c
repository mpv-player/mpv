/*
 * This file is part of mpv.
 *
 * Copyright (c) 2013 Stefano Pigozzi <stefano.pigozzi@gmail.com>
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
#include <libavcodec/vda.h>

#include "common/av_common.h"
#include "common/msg.h"
#include "video/mp_image.h"
#include "video/decode/lavc.h"
#include "config.h"


static int probe(struct vd_lavc_hwdec *hwdec, struct mp_hwdec_info *info,
                 const char *decoder)
{
    hwdec_request_api(info, "vda");

    if (mp_codec_to_av_codec_id(decoder) != AV_CODEC_ID_H264)
        return HWDEC_ERR_NO_CODEC;
    return 0;
}

static int init(struct lavc_ctx *ctx)
{
    return 0;
}

struct vda_error {
    int  code;
    char *reason;
};

static const struct vda_error vda_errors[] = {
    { AVERROR(ENOSYS),
        "Hardware doesn't support accelerated decoding for this stream" },
    { AVERROR(EINVAL),
        "Invalid configuration provided to VDADecoderCreate" },
    { AVERROR_INVALIDDATA,
        "Generic error returned by the decoder layer. The cause can range from"
        " VDADecoder finding errors in the bitstream to another application"
        " using VDA at the moment. Only one application can use VDA at a"
        " givent time." },
    { 0, NULL },
};

static void print_vda_error(struct mp_log *log, int lev, char *message,
                            int error_code)
{
    for (int n = 0; vda_errors[n].code < 0; n++)
        if (vda_errors[n].code == error_code) {
            mp_msg(log, lev, "%s: %s (%d)\n",
                   message, vda_errors[n].reason, error_code);
            return;
        }

    mp_msg(log, lev, "%s: %d\n", message, error_code);
}

static int init_decoder(struct lavc_ctx *ctx, int fmt, int w, int h)
{
    av_vda_default_free(ctx->avctx);
    int err = av_vda_default_init(ctx->avctx);
    if (err < 0) {
        print_vda_error(ctx->log, MSGL_ERR, "failed to init VDA decoder", err);
        return -1;
    }
    return 0;
}

static void uninit(struct lavc_ctx *ctx)
{
    if (ctx->avctx)
        av_vda_default_free(ctx->avctx);
}

const struct vd_lavc_hwdec mp_vd_lavc_vda = {
    .type = HWDEC_VDA,
    .image_format = IMGFMT_VDA,
    .probe = probe,
    .init = init,
    .uninit = uninit,
    .init_decoder = init_decoder,
};
