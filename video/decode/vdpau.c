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

#include <libavcodec/vdpau.h>
#include <libavutil/common.h>

#include "lavc.h"
#include "mpvcore/mp_common.h"
#include "mpvcore/av_common.h"
#include "video/fmt-conversion.h"
#include "video/vdpau.h"
#include "video/decode/dec_video.h"

struct priv {
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
    int ff_profile;
    VdpDecoderProfile vdp_profile;
    int maxrefs;
};

#define PE(av_codec_id, ff_profile, vdp_dcoder_profile, maxrefs) \
    {AV_CODEC_ID_ ## av_codec_id,                                \
     FF_PROFILE_ ## ff_profile,                                  \
     VDP_DECODER_PROFILE_ ## vdp_dcoder_profile,                 \
     maxrefs}

static const struct profile_entry profiles[] = {
    PE(MPEG1VIDEO,  UNKNOWN,                    MPEG1,          2),
    PE(MPEG2VIDEO,  MPEG2_SIMPLE,               MPEG2_SIMPLE,   2),
    PE(MPEG2VIDEO,  UNKNOWN,                    MPEG2_MAIN,     2),
    PE(H264,        H264_BASELINE,              H264_BASELINE,  16),
    PE(H264,        H264_CONSTRAINED_BASELINE,  H264_BASELINE,  16),
    PE(H264,        H264_MAIN,                  H264_MAIN,      16),
    PE(H264,        UNKNOWN,                    H264_HIGH,      16),
    PE(WMV3,        VC1_SIMPLE,                 VC1_SIMPLE,     2),
    PE(WMV3,        VC1_MAIN,                   VC1_MAIN,       2),
    PE(WMV3,        UNKNOWN,                    VC1_ADVANCED,   2),
    PE(VC1,         VC1_SIMPLE,                 VC1_SIMPLE,     2),
    PE(VC1,         VC1_MAIN,                   VC1_MAIN,       2),
    PE(VC1,         UNKNOWN,                    VC1_ADVANCED,   2),
    PE(MPEG4,       MPEG4_SIMPLE,               MPEG4_PART2_SP, 2),
    PE(MPEG4,       UNKNOWN,                    MPEG4_PART2_ASP,2),
};

// libavcodec absolutely wants a non-NULL render callback
static VdpStatus dummy_render(
    VdpDecoder                 decoder,
    VdpVideoSurface            target,
    VdpPictureInfo const *     picture_info,
    uint32_t                   bitstream_buffer_count,
    VdpBitstreamBuffer const * bitstream_buffers)
{
    return VDP_STATUS_DISPLAY_PREEMPTED;
}

static void mark_uninitialized(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;

    p->vdp_device = VDP_INVALID_HANDLE;
    p->context.decoder = VDP_INVALID_HANDLE;
    p->context.render = dummy_render;
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

static bool create_vdp_decoder(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;
    struct vdp_functions *vdp = p->mpvdp->vdp;
    VdpStatus vdp_st;

    if (handle_preemption(ctx) < 0)
        return false;

    if (p->context.decoder != VDP_INVALID_HANDLE)
        vdp->decoder_destroy(p->context.decoder);

    const struct profile_entry *pe = find_codec(ctx->avctx->codec_id,
                                                ctx->avctx->profile);
    if (!pe) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] Unknown codec!\n");
        goto fail;
    }

    vdp_st = vdp->decoder_create(p->vdp_device, pe->vdp_profile,
                                 p->vid_width, p->vid_height, pe->maxrefs,
                                 &p->context.decoder);
    CHECK_ST_WARNING("Failed creating VDPAU decoder");
    p->context.render = p->vdp->decoder_render;
    if (vdp_st != VDP_STATUS_OK)
        goto fail;
    return true;

fail:
    p->context.decoder = VDP_INVALID_HANDLE;
    p->context.render = dummy_render;
    return false;
}

static struct mp_image *allocate_image(struct lavc_ctx *ctx, AVFrame *frame)
{
    struct priv *p = ctx->hwdec_priv;

    if (frame->format != AV_PIX_FMT_VDPAU)
        return NULL;

    // frame->width/height lie. Using them breaks with non-mod 16 video.
    int w = ctx->avctx->width;
    int h = ctx->avctx->height;

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

    // Free bitstream buffers allocated by libavcodec
    av_freep(&p->context.bitstream_buffers);

    talloc_free(p);

    ctx->hwdec_priv = NULL;
}

static int init(struct lavc_ctx *ctx)
{
    struct priv *p = talloc_ptrtype(NULL, p);
    *p = (struct priv) {
        .mpvdp = ctx->hwdec_info->vdpau_ctx,
    };
    ctx->hwdec_priv = p;

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
    if (!info || !info->vdpau_ctx)
        return HWDEC_ERR_NO_CTX;
    if (!find_codec(mp_codec_to_av_codec_id(decoder), FF_PROFILE_UNKNOWN))
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
