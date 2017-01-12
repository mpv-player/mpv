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
    bool own_ctx;

    AVBufferRef *device_ref;
    AVBufferRef *frames_ref;
};


static int init_decoder(struct lavc_ctx *ctx, int w, int h)
{
    struct priv *p = ctx->hwdec_priv;
    // From avconv_vaapi.c. Disgusting, but apparently this is the best we get.
    int required_sw_format = ctx->avctx->sw_pix_fmt == AV_PIX_FMT_YUV420P10 ?
                             AV_PIX_FMT_P010 : AV_PIX_FMT_NV12;

    assert(!ctx->avctx->hw_frames_ctx);

    if (p->frames_ref) {
        AVHWFramesContext *fctx = (void *)p->frames_ref->data;
        if (fctx->width != w || fctx->height != h ||
            fctx->sw_format != required_sw_format)
        {
            av_buffer_unref(&p->frames_ref);
        }
    }

    if (!p->frames_ref) {
        p->frames_ref = av_hwframe_ctx_alloc(p->device_ref);
        if (!p->frames_ref)
            return -1;

        AVHWFramesContext *fctx = (void *)p->frames_ref->data;

        fctx->format = AV_PIX_FMT_VAAPI;
        fctx->sw_format = required_sw_format;
        fctx->width = w;
        fctx->height = h;

        fctx->initial_pool_size = hwdec_get_max_refs(ctx) + ADDITIONAL_SURFACES;

        // Some mpv downstream code uses this.
        fctx->user_opaque = p->ctx;

        va_lock(p->ctx);
        int res = av_hwframe_ctx_init(p->frames_ref);
        va_unlock(p->ctx);

        if (res > 0) {
            MP_ERR(ctx, "Failed to allocate hw frames.\n");
            av_buffer_unref(&p->frames_ref);
            return -1;
        }
    }

    ctx->avctx->hw_frames_ctx = av_buffer_ref(p->frames_ref);
    if (!ctx->avctx->hw_frames_ctx)
        return -1;

    return 0;
}

static void uninit(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;

    if (!p)
        return;

    av_buffer_unref(&p->frames_ref);
    av_buffer_unref(&p->device_ref);

    if (p->own_ctx)
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
        p->ctx = hwdec_devices_get(ctx->hwdec_devs, HWDEC_VAAPI)->ctx;
    } else {
        p->ctx = va_create_standalone(ctx->log, false);
        if (!p->ctx) {
            talloc_free(p);
            return -1;
        }
        p->own_ctx = true;
    }

    ctx->hwdec_priv = p;

    p->device_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
    if (!p->device_ref)
        return -1;

    AVHWDeviceContext *hwctx = (void *)p->device_ref->data;
    AVVAAPIDeviceContext *vactx = hwctx->hwctx;

    vactx->display = p->ctx->display;

    if (av_hwdevice_ctx_init(p->device_ref) < 0)
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

static void intel_shit_lock(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;
    va_lock(p->ctx);
}

static void intel_crap_unlock(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;
    va_unlock(p->ctx);
}

const struct vd_lavc_hwdec mp_vd_lavc_vaapi = {
    .type = HWDEC_VAAPI,
    .image_format = IMGFMT_VAAPI,
    .volatile_context = true,
    .probe = probe,
    .init = init_direct,
    .uninit = uninit,
    .init_decoder = init_decoder,
    .lock = intel_shit_lock,
    .unlock = intel_crap_unlock,
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
