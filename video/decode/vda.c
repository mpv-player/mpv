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

#include "mpvcore/av_common.h"
#include "mpvcore/mp_msg.h"
#include "video/mp_image.h"
#include "video/decode/lavc.h"
#include "config.h"

struct priv {
    struct vda_context vda_ctx;
};

struct profile_entry {
    enum AVCodecID av_codec;
    int ff_profile;
    uint32_t vda_codec;
};

static const struct profile_entry profiles[] = {
    { AV_CODEC_ID_H264, FF_PROFILE_UNKNOWN, 'avc1' },
};

static const struct profile_entry *find_codec(enum AVCodecID id, int ff_profile)
{
    for (int n = 0; n < MP_ARRAY_SIZE(profiles); n++) {
        if (profiles[n].av_codec == id &&
            (profiles[n].ff_profile == ff_profile ||
             profiles[n].ff_profile == FF_PROFILE_UNKNOWN))
        {
            return &profiles[n];
        }
    }
    return NULL;
}

struct vda_error {
    int  code;
    char *reason;
};

static const struct vda_error vda_errors[] = {
    { kVDADecoderHardwareNotSupportedErr,
        "Hardware doesn't support accelerated decoding" },
    { kVDADecoderFormatNotSupportedErr,
        "Hardware doesn't support requested output format" },
    { kVDADecoderConfigurationError,
        "Invalid configuration provided to VDADecoderCreate" },
    { kVDADecoderDecoderFailedErr,
        "Generic error returned by the decoder layer. The cause can range from"
        " VDADecoder finding errors in the bitstream to another application"
        " using VDA at the moment. Only one application can use VDA at a"
        " givent time." },
    { 0, NULL },
};

static void print_vda_error(int lev, char *message, int error_code)
{
    for (int n = 0; vda_errors[n].code < 0; n++)
        if (vda_errors[n].code == error_code) {
            mp_msg(MSGT_DECVIDEO, lev, "%s: %s (%d)\n",
                   message, vda_errors[n].reason, error_code);
            return;
        }

    mp_msg(MSGT_DECVIDEO, lev, "%s: %d\n", message, error_code);
}

static int probe(struct vd_lavc_hwdec *hwdec, struct mp_hwdec_info *info,
                 const char *decoder)
{
    hwdec_request_api(info, "vda");

    if (!find_codec(mp_codec_to_av_codec_id(decoder), FF_PROFILE_UNKNOWN))
        return HWDEC_ERR_NO_CODEC;
    return 0;
}

static int init_vda_decoder(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;

    if (p->vda_ctx.decoder)
        ff_vda_destroy_decoder(&p->vda_ctx);

    const struct profile_entry *pe =
        find_codec(ctx->avctx->codec_id, ctx->avctx->profile);

    p->vda_ctx = (struct vda_context) {
        .width             = ctx->avctx->width,
        .height            = ctx->avctx->height,
        .format            = pe->vda_codec,
        // equals to k2vuyPixelFormat (= YUY2/UYVY)
        .cv_pix_fmt_type   = kCVPixelFormatType_422YpCbCr8,

#if HAVE_VDA_LIBAVCODEC_REFCOUNTING
        .use_ref_buffer    = 1,
#endif
        // use_ref_buffer is 1 in ffmpeg (while libav doesn't support this
        // feature). This means that in the libav case, libavcodec returns us
        // a CVPixelBuffer with refcount=1 AND hands over ownership of that
        // reference.

        // This is slightly different from a typical refcounted situation
        // where the API would return something that we need to to retain
        // for it to stay around (ffmpeg behaves like expected when using
        // use_ref_buffer = 1).

        // If mpv doesn't properly free CVPixelBufferRefs that are no longer
        // used, the wrapped IOSurface ids increase monotonically hinting at
        // a leaking of both CVPixelBuffers and IOSurfaces.
    };

    int status = ff_vda_create_decoder(
        &p->vda_ctx, ctx->avctx->extradata, ctx->avctx->extradata_size);

    if (status) {
        print_vda_error(MSGL_ERR, "[vda] failed to init decoder", status);
        return -1;
    }

    return 0;
}

static int init(struct lavc_ctx *ctx)
{
    struct priv *p = talloc_zero(NULL, struct priv);
    ctx->hwdec_priv = p;
    ctx->avctx->hwaccel_context = &p->vda_ctx;
    return 0;
}

static void uninit(struct lavc_ctx *ctx) {
    struct priv *p = ctx->hwdec_priv;
    if (p->vda_ctx.decoder)
        ff_vda_destroy_decoder(&p->vda_ctx);
}

static void cv_retain(void *pbuf)
{
    CVPixelBufferRetain((CVPixelBufferRef)pbuf);
}

static void cv_release(void *pbuf)
{
    CVPixelBufferRelease((CVPixelBufferRef)pbuf);
}

static struct mp_image *mp_image_new_cv_ref(struct mp_image *mpi)
{
    CVPixelBufferRef pbuf = (CVPixelBufferRef)mpi->planes[3];
    // mp_image_new_external_ref assumes the external reference count is
    // already 1 so the calls to cv_retain and cv_release are unbalanced (
    // in favor of cv_release). To balance out the retain count we need to
    // retain the CVPixelBufferRef if ffmpeg is set to automatically release
    // it when the AVFrame is unreffed.
#if HAVE_VDA_LIBAVCODEC_REFCOUNTING
    cv_retain(pbuf);
#endif
    return mp_image_new_external_ref(mpi,
        pbuf, cv_retain, cv_release, NULL, NULL);
}

static struct mp_image *process_image(struct lavc_ctx *ctx, struct mp_image *mpi)
{
    struct mp_image *cv_mpi = mp_image_new_cv_ref(mpi);
    mp_image_unrefp(&mpi);
    return cv_mpi;
}

// This actually returns dummy images, since vda_264 creates it's own AVFrames
// to wrap CVPixelBuffers in planes[3].
static struct mp_image *allocate_image(struct lavc_ctx *ctx, int fmt,
                                       int w, int h)
{
    struct priv *p = ctx->hwdec_priv;

    if (fmt != IMGFMT_VDA)
         return NULL;

    if (w != p->vda_ctx.width || h != p->vda_ctx.height)
        init_vda_decoder(ctx);

    struct mp_image img = {0};
    mp_image_setfmt(&img, fmt);
    mp_image_set_size(&img, w, h);

    // There is an `assert(!dst->f.buf[0])` in libavcodec/h264.c
    // Setting the first plane to some dummy value allows to satisfy it
    img.planes[0] = (void*)"dummy";

    return mp_image_new_custom_ref(&img, NULL, NULL);
}

const struct vd_lavc_hwdec mp_vd_lavc_vda = {
    .type = HWDEC_VDA,
    .image_formats = (const int[]) { IMGFMT_VDA, 0 },
    .probe = probe,
    .init = init,
    .uninit = uninit,
    .allocate_image = allocate_image,
    .process_image = process_image,
};
