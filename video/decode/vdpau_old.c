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

#include <stddef.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavcodec/vdpau.h>
#include <libavutil/common.h>

#include "config.h"
#include "lavc.h"
#include "common/common.h"
#include "common/av_common.h"
#include "video/fmt-conversion.h"
#include "video/vdpau.h"
#include "video/hwdec.h"
#include "video/decode/dec_video.h"

struct priv {
    struct mp_log              *log;
    struct mp_vdpau_ctx        *mpvdp;
    struct vdp_functions       *vdp;
    uint64_t                    preemption_counter;
    int                         fmt, w, h;

    AVVDPAUContext             *context;
};

#define PE(av_codec_id, ff_profile, vdp_profile)                \
    {AV_CODEC_ID_ ## av_codec_id, FF_PROFILE_ ## ff_profile,    \
     VDP_DECODER_PROFILE_ ## vdp_profile}

static const struct hwdec_profile_entry profiles[] = {
    PE(MPEG1VIDEO,  UNKNOWN,            MPEG1),
    PE(MPEG2VIDEO,  MPEG2_MAIN,         MPEG2_MAIN),
    PE(MPEG2VIDEO,  MPEG2_SIMPLE,       MPEG2_SIMPLE),
    PE(MPEG4,       MPEG4_ADVANCED_SIMPLE, MPEG4_PART2_ASP),
    PE(MPEG4,       MPEG4_SIMPLE,       MPEG4_PART2_SP),
    PE(H264,        H264_HIGH,          H264_HIGH),
    PE(H264,        H264_MAIN,          H264_MAIN),
    PE(H264,        H264_BASELINE,      H264_BASELINE),
    PE(VC1,         VC1_ADVANCED,       VC1_ADVANCED),
    PE(VC1,         VC1_MAIN,           VC1_MAIN),
    PE(VC1,         VC1_SIMPLE,         VC1_SIMPLE),
    PE(WMV3,        VC1_ADVANCED,       VC1_ADVANCED),
    PE(WMV3,        VC1_MAIN,           VC1_MAIN),
    PE(WMV3,        VC1_SIMPLE,         VC1_SIMPLE),
    {0}
};

static int init_decoder(struct lavc_ctx *ctx, int fmt, int w, int h)
{
    struct priv *p = ctx->hwdec_priv;
    struct vdp_functions *vdp = &p->mpvdp->vdp;
    VdpDevice vdp_device = p->mpvdp->vdp_device;
    VdpStatus vdp_st;

    p->fmt = fmt;
    p->w = w;
    p->h = h;

    // During preemption, pretend everything is ok.
    if (mp_vdpau_handle_preemption(p->mpvdp, &p->preemption_counter) < 0)
        return 0;

    if (p->context->decoder != VDP_INVALID_HANDLE)
        vdp->decoder_destroy(p->context->decoder);

    const struct hwdec_profile_entry *pe = hwdec_find_profile(ctx, profiles);
    if (!pe) {
        MP_ERR(p, "Unsupported codec or profile.\n");
        goto fail;
    }

    VdpBool supported;
    uint32_t maxl, maxm, maxw, maxh;
    vdp_st = vdp->decoder_query_capabilities(vdp_device, pe->hw_profile,
                                             &supported, &maxl, &maxm,
                                             &maxw, &maxh);
    CHECK_VDP_WARNING(p, "Querying VDPAU decoder capabilities");
    if (!supported) {
        MP_ERR(p, "Codec or profile not supported by hardware.\n");
        goto fail;
    }
    if (w > maxw || h > maxh) {
        MP_ERR(p, "Video resolution(%dx%d) is larger than the maximum size(%dx%d) supported.\n",
               w, h, maxw, maxh);
        goto fail;
    }

    int maxrefs = hwdec_get_max_refs(ctx);

    vdp_st = vdp->decoder_create(vdp_device, pe->hw_profile, w, h, maxrefs,
                                 &p->context->decoder);
    CHECK_VDP_WARNING(p, "Failed creating VDPAU decoder");
    if (vdp_st != VDP_STATUS_OK)
        goto fail;
    return 0;

fail:
    p->context->decoder = VDP_INVALID_HANDLE;
    return -1;
}

static struct mp_image *allocate_image(struct lavc_ctx *ctx, int fmt,
                                       int w, int h)
{
    struct priv *p = ctx->hwdec_priv;

    if (mp_vdpau_handle_preemption(p->mpvdp, &p->preemption_counter) == 0) {
        if (init_decoder(ctx, p->fmt, p->w, p->h) < 0)
            return NULL;
    }

    VdpChromaType chroma;
    mp_vdpau_get_format(IMGFMT_VDPAU, &chroma, NULL);

    return mp_vdpau_get_video_surface(p->mpvdp, chroma, w, h);
}

static void uninit(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;

    if (!p)
        return;

    if (p->context && p->context->decoder != VDP_INVALID_HANDLE)
        p->vdp->decoder_destroy(p->context->decoder);

    av_free(p->context);
    talloc_free(p);

    ctx->hwdec_priv = NULL;
}

#if LIBAVCODEC_VERSION_MICRO >= 100
static int render2(struct AVCodecContext *avctx, struct AVFrame *frame,
                   const VdpPictureInfo *pic_info, uint32_t buffers_used,
                   const VdpBitstreamBuffer *buffers)
{
    struct dec_video *vd = avctx->opaque;
    struct lavc_ctx *ctx = vd->priv;
    struct priv *p = ctx->hwdec_priv;
    VdpVideoSurface surf = (uintptr_t)frame->data[3];
    VdpStatus status;

    status = p->vdp->decoder_render(p->context->decoder, surf, pic_info,
                                    buffers_used, buffers);

    return status;
}
#endif

static int init(struct lavc_ctx *ctx)
{
    struct priv *p = talloc_ptrtype(NULL, p);
    *p = (struct priv) {
        .log = mp_log_new(p, ctx->log, "vdpau"),
        .mpvdp = ctx->hwdec_info->hwctx->vdpau_ctx,
    };
    ctx->hwdec_priv = p;

    p->context = av_vdpau_alloc_context();
    if (!p->context)
        goto error;

    p->vdp = &p->mpvdp->vdp;
#if LIBAVCODEC_VERSION_MICRO >= 100
    p->context->render2 = render2;
#else
    p->context->render = p->vdp->decoder_render;
#endif
    p->context->decoder = VDP_INVALID_HANDLE;

    if (mp_vdpau_handle_preemption(p->mpvdp, &p->preemption_counter) < 1)
        goto error;

    ctx->avctx->hwaccel_context = p->context;

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
    if (!hwdec_check_codec_support(decoder, profiles))
        return HWDEC_ERR_NO_CODEC;
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
