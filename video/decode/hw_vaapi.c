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

#include <stddef.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavutil/common.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>

#include "config.h"

#include "lavc.h"
#include "common/common.h"
#include "common/av_common.h"
#include "video/fmt-conversion.h"
#include "video/vaapi.h"
#include "video/mp_image_pool.h"
#include "video/hwdec.h"
#include "video/filter/vf.h"

#define ADDITIONAL_SURFACES (HWDEC_EXTRA_SURFACES + HWDEC_DELAY_QUEUE_COUNT)

struct priv {
    struct mp_log *log;
    struct mp_vaapi_ctx *ctx;
    struct mp_hwdec_ctx *hwdev;
};

static int init_decoder(struct lavc_ctx *ctx, int w, int h)
{
    struct priv *p = ctx->hwdec_priv;
    // libavcodec has no way yet to communicate the exact surface format needed
    // for the frame pool, or the required minimum size of the frame pool.
    // Hopefully, this weakness in the libavcodec API will be fixed in the
    // future.
    // For the pixel format, we try to second-guess from what the libavcodec
    // software decoder would require (sw_pix_fmt). It could break and require
    // adjustment if new VAAPI surface formats are added.
    int sw_format = ctx->avctx->sw_pix_fmt == AV_PIX_FMT_YUV420P10 ?
                    AV_PIX_FMT_P010 : AV_PIX_FMT_NV12;

    // The video output might not support all formats.
    // Note that supported_formats==NULL means any are accepted.
    if (p->hwdev && p->hwdev->supported_formats) {
        int mp_format = pixfmt2imgfmt(sw_format);
        bool found = false;
        for (int n = 0; p->hwdev->supported_formats[n]; n++) {
            if (p->hwdev->supported_formats[n] == mp_format) {
                found = true;
                break;
            }
        }
        if (!found) {
            MP_WARN(ctx, "Surface format %s not supported for direct rendering.\n",
                    mp_imgfmt_to_name(mp_format));
            return -1;
        }
    }

    return hwdec_setup_hw_frames_ctx(ctx, p->ctx->av_device_ref, sw_format,
                                hwdec_get_max_refs(ctx) + ADDITIONAL_SURFACES);
}

static void uninit(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;

    if (!p)
        return;

    if (!p->hwdev)
        va_destroy(p->ctx);

    talloc_free(p);
    ctx->hwdec_priv = NULL;
}

static int init(struct lavc_ctx *ctx, bool direct)
{
    struct priv *p = talloc_ptrtype(NULL, p);
    *p = (struct priv) {
        .log = mp_log_new(p, ctx->log, "vaapi"),
    };

    if (direct) {
        p->hwdev = hwdec_devices_get(ctx->hwdec_devs, HWDEC_VAAPI);
        p->ctx = p->hwdev->ctx;
    } else {
        p->ctx = va_create_standalone(ctx->log, false);
        if (!p->ctx) {
            talloc_free(p);
            return -1;
        }
    }

    ctx->hwdec_priv = p;

    if (!p->ctx->av_device_ref)
        return -1;

    return 0;
}

static int init_direct(struct lavc_ctx *ctx)
{
    return init(ctx, true);
}

static int probe(struct lavc_ctx *ctx, struct vd_lavc_hwdec *hwdec,
                 const char *codec)
{
    if (!hwdec_devices_load(ctx->hwdec_devs, HWDEC_VAAPI))
        return HWDEC_ERR_NO_CTX;
    return 0;
}

static int probe_copy(struct lavc_ctx *ctx, struct vd_lavc_hwdec *hwdec,
                      const char *codec)
{
    struct mp_vaapi_ctx *dummy = va_create_standalone(ctx->log, true);
    if (!dummy)
        return HWDEC_ERR_NO_CTX;
    bool emulated = va_guess_if_emulated(dummy);
    va_destroy(dummy);
    if (emulated)
        return HWDEC_ERR_EMULATED;
    return 0;
}

static int init_copy(struct lavc_ctx *ctx)
{
    return init(ctx, false);
}

const struct vd_lavc_hwdec mp_vd_lavc_vaapi = {
    .type = HWDEC_VAAPI,
    .image_format = IMGFMT_VAAPI,
    .volatile_context = true,
    .probe = probe,
    .init = init_direct,
    .uninit = uninit,
    .init_decoder = init_decoder,
};

const struct vd_lavc_hwdec mp_vd_lavc_vaapi_copy = {
    .type = HWDEC_VAAPI_COPY,
    .copying = true,
    .image_format = IMGFMT_VAAPI,
    .volatile_context = true,
    .probe = probe_copy,
    .init = init_copy,
    .uninit = uninit,
    .init_decoder = init_decoder,
    .delay_queue = HWDEC_DELAY_QUEUE_COUNT,
};
