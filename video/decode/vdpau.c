/*
 * This file is part of mpv.
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

#include <libavcodec/avcodec.h>
#include <libavcodec/vdpau.h>
#include <libavutil/common.h>

#include "lavc.h"
#include "common/common.h"
#include "video/vdpau.h"
#include "video/hwdec.h"

struct priv {
    struct mp_log              *log;
    struct mp_vdpau_ctx        *mpvdp;
    uint64_t                    preemption_counter;
};

static int init_decoder(struct lavc_ctx *ctx, int w, int h)
{
    struct priv *p = ctx->hwdec_priv;

    // During preemption, pretend everything is ok.
    if (mp_vdpau_handle_preemption(p->mpvdp, &p->preemption_counter) < 0)
        return 0;

    return av_vdpau_bind_context(ctx->avctx, p->mpvdp->vdp_device,
                                 p->mpvdp->get_proc_address,
                                 AV_HWACCEL_FLAG_IGNORE_LEVEL |
                                 AV_HWACCEL_FLAG_ALLOW_HIGH_DEPTH);
}

static struct mp_image *allocate_image(struct lavc_ctx *ctx, int w, int h)
{
    struct priv *p = ctx->hwdec_priv;

    // In case of preemption, reinit the decoder. Setting hwdec_request_reinit
    // will cause init_decoder() to be called again.
    if (mp_vdpau_handle_preemption(p->mpvdp, &p->preemption_counter) == 0)
        ctx->hwdec_request_reinit = true;

    VdpChromaType chroma = 0;
    uint32_t s_w = w, s_h = h;
    if (av_vdpau_get_surface_parameters(ctx->avctx, &chroma, &s_w, &s_h) < 0)
        return NULL;

    return mp_vdpau_get_video_surface(p->mpvdp, chroma, s_w, s_h);
}

static void uninit(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;

    talloc_free(p);

    av_freep(&ctx->avctx->hwaccel_context);
}

static int init(struct lavc_ctx *ctx)
{
    struct priv *p = talloc_ptrtype(NULL, p);
    *p = (struct priv) {
        .log = mp_log_new(p, ctx->log, "vdpau"),
        .mpvdp = ctx->hwdec_info->hwctx->vdpau_ctx,
    };
    ctx->hwdec_priv = p;

    if (mp_vdpau_handle_preemption(p->mpvdp, &p->preemption_counter) < 1)
        goto error;

    return 0;

error:
    uninit(ctx);
    return -1;
}

static int probe(struct vd_lavc_hwdec *hwdec, struct mp_hwdec_info *info,
                 const char *decoder)
{
    hwdec_request_api(info, "vdpau");
    if (!info || !info->hwctx || !info->hwctx->vdpau_ctx)
        return HWDEC_ERR_NO_CTX;
    if (mp_vdpau_guess_if_emulated(info->hwctx->vdpau_ctx))
        return HWDEC_ERR_EMULATED;
    return 0;
}

const struct vd_lavc_hwdec mp_vd_lavc_vdpau = {
    .type = HWDEC_VDPAU,
    .image_format = IMGFMT_VDPAU,
    .probe = probe,
    .init = init,
    .uninit = uninit,
    .init_decoder = init_decoder,
    .allocate_image = allocate_image,
};
