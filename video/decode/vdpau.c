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
    struct mp_image_pool       *sw_pool;
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

static struct mp_image *update_format(struct lavc_ctx *ctx, struct mp_image *img)
{
    VdpChromaType chroma = 0;
    uint32_t s_w, s_h;
    if (av_vdpau_get_surface_parameters(ctx->avctx, &chroma, &s_w, &s_h) >= 0) {
        if (chroma == VDP_CHROMA_TYPE_420)
            img->params.hw_subfmt = IMGFMT_NV12;
    }
    return img;
}

static void uninit(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;

    if (p->display) {
        // for copy path: we own this stuff
        mp_vdpau_destroy(p->mpvdp);
        XCloseDisplay(p->display);
    }

    talloc_free(p);

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

    p->sw_pool = talloc_steal(p, mp_image_pool_new(17));

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
    int r = init_copy(ctx);
    if (ctx->hwdec_priv)
        uninit(ctx);
    ctx->hwdec_priv = NULL;

    return r < 0 ? HWDEC_ERR_NO_CTX : 0;
}

static struct mp_image *copy_image(struct lavc_ctx *ctx, struct mp_image *img)
{
    struct priv *p = ctx->hwdec_priv;
    struct mp_hwdec_ctx *hwctx = &p->mpvdp->hwctx;
    struct mp_image *out = hwctx->download_image(hwctx, img, p->sw_pool);
    talloc_free(img);
    return out;
}

const struct vd_lavc_hwdec mp_vd_lavc_vdpau = {
    .type = HWDEC_VDPAU,
    .image_format = IMGFMT_VDPAU,
    .probe = probe,
    .init = init,
    .uninit = uninit,
    .init_decoder = init_decoder,
    .allocate_image = allocate_image,
    .process_image = update_format,
};

const struct vd_lavc_hwdec mp_vd_lavc_vdpau_copy = {
    .type = HWDEC_VDPAU_COPY,
    .copying = true,
    .image_format = IMGFMT_VDPAU,
    .probe = probe_copy,
    .init = init_copy,
    .uninit = uninit,
    .init_decoder = init_decoder,
    .allocate_image = allocate_image,
    .process_image = copy_image,
};
