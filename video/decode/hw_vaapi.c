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

struct priv {
    struct mp_log *log;
    struct mp_vaapi_ctx *ctx;
};

static void uninit(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;

    if (!p)
        return;

    va_destroy(p->ctx);

    talloc_free(p);
    ctx->hwdec_priv = NULL;
    ctx->hwdec_dev = NULL;
}

static int init(struct lavc_ctx *ctx, bool direct)
{
    struct priv *p = talloc_ptrtype(NULL, p);
    *p = (struct priv) {
        .log = mp_log_new(p, ctx->log, "vaapi"),
    };

    if (direct) {
        ctx->hwdec_dev = hwdec_devices_get(ctx->hwdec_devs, HWDEC_VAAPI);
    } else {
        p->ctx = va_create_standalone(ctx->log, false);
        if (!p->ctx) {
            talloc_free(p);
            return -1;
        }
        ctx->hwdec_dev = &p->ctx->hwctx;
    }

    ctx->hwdec_priv = p;

    if (!ctx->hwdec_dev->av_device_ref)
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
    .probe = probe,
    .init = init_direct,
    .uninit = uninit,
    .generic_hwaccel = true,
    .static_pool = true,
    .pixfmt_map = (const enum AVPixelFormat[][2]) {
        {AV_PIX_FMT_YUV420P10, AV_PIX_FMT_P010},
        {AV_PIX_FMT_YUV420P,   AV_PIX_FMT_NV12},
        {AV_PIX_FMT_NONE}
    },
};

const struct vd_lavc_hwdec mp_vd_lavc_vaapi_copy = {
    .type = HWDEC_VAAPI_COPY,
    .copying = true,
    .image_format = IMGFMT_VAAPI,
    .probe = probe_copy,
    .init = init_copy,
    .uninit = uninit,
    .generic_hwaccel = true,
    .static_pool = true,
    .pixfmt_map = (const enum AVPixelFormat[][2]) {
        {AV_PIX_FMT_YUV420P10, AV_PIX_FMT_P010},
        {AV_PIX_FMT_YUV420P,   AV_PIX_FMT_NV12},
        {AV_PIX_FMT_NONE}
    },
};
