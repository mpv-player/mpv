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

#include <libavcodec/avcodec.h>
#include <libavcodec/vdpau.h>
#include <libavutil/common.h>
#include <libavutil/hwcontext.h>

#include "lavc.h"
#include "common/common.h"
#include "video/mp_image_pool.h"
#include "video/vdpau.h"
#include "video/hwdec.h"

struct priv {
    struct mp_log              *log;
    struct mp_vdpau_ctx        *mpvdp;
    uint64_t                    preemption_counter;
    // vdpau-copy
    Display                    *display;
};

static int init_decoder(struct lavc_ctx *ctx, int w, int h)
{
    struct priv *p = ctx->hwdec_priv;
    int sw_format = ctx->avctx->sw_pix_fmt;

    if (sw_format != AV_PIX_FMT_YUV420P && sw_format != AV_PIX_FMT_NV12) {
        MP_VERBOSE(ctx, "Rejecting non 4:2:0 8 bit decoding.\n");
        return -1;
    }

    if (hwdec_setup_hw_frames_ctx(ctx, p->mpvdp->av_device_ref, sw_format, 0) < 0)
        return -1;

    // During preemption, pretend everything is ok.
    if (mp_vdpau_handle_preemption(p->mpvdp, &p->preemption_counter) < 0)
        return 0;

    return av_vdpau_bind_context(ctx->avctx, p->mpvdp->vdp_device,
                                 p->mpvdp->get_proc_address,
                                 AV_HWACCEL_FLAG_IGNORE_LEVEL |
                                 AV_HWACCEL_FLAG_ALLOW_HIGH_DEPTH);
}

static void uninit(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;

    if (p->display) {
        // for copy path: we own this stuff
        mp_vdpau_destroy(p->mpvdp);
        XCloseDisplay(p->display);
    }

    TA_FREEP(&ctx->hwdec_priv);

    if (ctx->avctx)
        av_freep(&ctx->avctx->hwaccel_context);
}

static int init(struct lavc_ctx *ctx)
{
    struct priv *p = talloc_ptrtype(NULL, p);
    *p = (struct priv) {
        .log = mp_log_new(p, ctx->log, "vdpau"),
        .mpvdp = hwdec_devices_get(ctx->hwdec_devs, HWDEC_VDPAU)->ctx,
    };
    ctx->hwdec_priv = p;

    mp_vdpau_handle_preemption(p->mpvdp, &p->preemption_counter);
    return 0;
}

static int probe(struct lavc_ctx *ctx, struct vd_lavc_hwdec *hwdec,
                 const char *codec)
{
    if (!hwdec_devices_load(ctx->hwdec_devs, HWDEC_VDPAU))
        return HWDEC_ERR_NO_CTX;
    return 0;
}

static int init_copy(struct lavc_ctx *ctx)
{
    struct priv *p = talloc_ptrtype(NULL, p);
    *p = (struct priv) {
        .log = mp_log_new(p, ctx->log, "vdpau"),
    };

    p->display = XOpenDisplay(NULL);
    if (!p->display)
        goto error;

    p->mpvdp = mp_vdpau_create_device_x11(p->log, p->display, true);
    if (!p->mpvdp)
        goto error;

    ctx->hwdec_priv = p;

    mp_vdpau_handle_preemption(p->mpvdp, &p->preemption_counter);
    return 0;

error:
    if (p->display)
        XCloseDisplay(p->display);
    talloc_free(p);
    return -1;
}

static int probe_copy(struct lavc_ctx *ctx, struct vd_lavc_hwdec *hwdec,
                      const char *codec)
{
    assert(!ctx->hwdec_priv);

    int r = HWDEC_ERR_NO_CTX;
    if (init_copy(ctx) >=0 ) {
        struct priv *p = ctx->hwdec_priv;
        r = mp_vdpau_guess_if_emulated(p->mpvdp) ? HWDEC_ERR_EMULATED : 0;
        uninit(ctx);
    }
    return r;
}

const struct vd_lavc_hwdec mp_vd_lavc_vdpau = {
    .type = HWDEC_VDPAU,
    .image_format = IMGFMT_VDPAU,
    .probe = probe,
    .init = init,
    .uninit = uninit,
    .init_decoder = init_decoder,
    .volatile_context = true,
};

const struct vd_lavc_hwdec mp_vd_lavc_vdpau_copy = {
    .type = HWDEC_VDPAU_COPY,
    .copying = true,
    .image_format = IMGFMT_VDPAU,
    .probe = probe_copy,
    .init = init_copy,
    .uninit = uninit,
    .init_decoder = init_decoder,
    .volatile_context = true,
};
