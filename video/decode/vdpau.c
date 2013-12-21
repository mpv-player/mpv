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

#include "lavc.h"
#include "common/common.h"
#include "common/av_common.h"
#include "video/fmt-conversion.h"
#include "video/vdpau.h"
#include "video/hwdec.h"

struct priv {
    struct mp_log              *log;
    struct mp_vdpau_ctx        *mpvdp;
    struct vdp_functions       *vdp;
    VdpDevice                   vdp_device;
    uint64_t                    preemption_counter;

    AVVDPAUContext              context;

    int                         vid_width;
    int                         vid_height;
};

struct profile_entry {
    enum AVCodecID av_codec;
    VdpDecoderProfile vdp_profile;
    int maxrefs;
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

static void mark_uninitialized(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;

    p->vdp_device = VDP_INVALID_HANDLE;
    p->context.decoder = VDP_INVALID_HANDLE;
}

static int handle_preemption(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;

    if (!mp_vdpau_status_ok(p->mpvdp))
        return -1;

    // Mark objects as destroyed if preemption+reinit occured
    if (p->preemption_counter < p->mpvdp->preemption_counter) {
        p->preemption_counter = p->mpvdp->preemption_counter;
        mark_uninitialized(ctx);
    }

    p->vdp_device = p->mpvdp->vdp_device;
    p->vdp = p->mpvdp->vdp;

    return 0;
}

static bool create_vdp_decoder(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;
    struct vdp_functions *vdp = p->mpvdp->vdp;
    VdpStatus vdp_st;

    if (handle_preemption(ctx) < 0)
        return false;

    if (p->context.decoder != VDP_INVALID_HANDLE)
        vdp->decoder_destroy(p->context.decoder);

    const struct hwdec_profile_entry *pe = hwdec_find_profile(ctx, profiles);
    if (!pe) {
        MP_ERR(p, "Unsupported codec or profile.\n");
        goto fail;
    }

    VdpBool supported;
    uint32_t maxl, maxm, maxw, maxh;
    vdp_st = vdp->decoder_query_capabilities(p->vdp_device, pe->hw_profile,
                                             &supported, &maxl, &maxm,
                                             &maxw, &maxh);
    CHECK_VDP_WARNING(p, "Querying VDPAU decoder capabilities");
    if (!supported) {
        MP_ERR(p, "Codec or profile not supported by hardware.\n");
        goto fail;
    }
    if (p->vid_width > maxw || p->vid_height > maxh) {
        MP_ERR(p, "Video too large.\n");
        goto fail;
    }

    int maxrefs = hwdec_get_max_refs(ctx);

    vdp_st = vdp->decoder_create(p->vdp_device, pe->hw_profile,
                                 p->vid_width, p->vid_height, maxrefs,
                                 &p->context.decoder);
    CHECK_VDP_WARNING(p, "Failed creating VDPAU decoder");
    if (vdp_st != VDP_STATUS_OK)
        goto fail;
    return true;

fail:
    p->context.decoder = VDP_INVALID_HANDLE;
    return false;
}

static struct mp_image *allocate_image(struct lavc_ctx *ctx, int fmt,
                                       int w, int h)
{
    struct priv *p = ctx->hwdec_priv;

    if (fmt != IMGFMT_VDPAU)
        return NULL;

    handle_preemption(ctx);

    if (w != p->vid_width || h != p->vid_height ||
        p->context.decoder == VDP_INVALID_HANDLE)
    {
        p->vid_width = w;
        p->vid_height = h;
        if (!create_vdp_decoder(ctx))
            return NULL;
    }

    VdpChromaType chroma;
    mp_vdpau_get_format(IMGFMT_VDPAU, &chroma, NULL);

    return mp_vdpau_get_video_surface(p->mpvdp, IMGFMT_VDPAU, chroma, w, h);
}

static void uninit(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;

    if (!p)
        return;

    if (p->context.decoder != VDP_INVALID_HANDLE)
        p->vdp->decoder_destroy(p->context.decoder);

    talloc_free(p);

    ctx->hwdec_priv = NULL;
}

static int init(struct lavc_ctx *ctx)
{
    struct priv *p = talloc_ptrtype(NULL, p);
    *p = (struct priv) {
        .log = mp_log_new(p, ctx->log, "vdpau"),
        .mpvdp = ctx->hwdec_info->vdpau_ctx,
    };
    ctx->hwdec_priv = p;

    p->vdp = p->mpvdp->vdp;
    p->context.render = p->vdp->decoder_render;

    p->preemption_counter = p->mpvdp->preemption_counter;
    mark_uninitialized(ctx);

    if (handle_preemption(ctx) < 0)
        return -1;

    ctx->avctx->hwaccel_context = &p->context;

    return 0;
}

static int probe(struct vd_lavc_hwdec *hwdec, struct mp_hwdec_info *info,
                 const char *decoder)
{
    hwdec_request_api(info, "vdpau");
    if (!info || !info->vdpau_ctx)
        return HWDEC_ERR_NO_CTX;
    if (!hwdec_check_codec_support(decoder, profiles))
        return HWDEC_ERR_NO_CODEC;
    return 0;
}

const struct vd_lavc_hwdec mp_vd_lavc_vdpau = {
    .type = HWDEC_VDPAU,
    .image_formats = (const int[]) {IMGFMT_VDPAU, 0},
    .probe = probe,
    .init = init,
    .uninit = uninit,
    .allocate_image = allocate_image,
};
