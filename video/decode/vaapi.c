/*
 * This file is part of mpv.
 *
 * With some chunks from original MPlayer VAAPI patch:
 * Copyright (C) 2008-2009 Splitted-Desktop Systems
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
#include <libavcodec/vaapi.h>
#include <libavutil/common.h>

#include "lavc.h"
#include "mpvcore/mp_common.h"
#include "mpvcore/av_common.h"
#include "video/fmt-conversion.h"
#include "video/vaapi.h"
#include "video/decode/dec_video.h"

/*
 * The VAAPI decoder can work only with surfaces passed to the decoder at
 * creation time. This means all surfaces have to be created in advance.
 * So, additionally to the maximum number of reference frames, we need
 * surfaces for:
 * - 1 decode frame
 * - decoding 1 frame ahead (done by generic playback code)
 * - keeping the reference to the previous frame (done by vo_vaapi.c)
 * Note that redundant additional surfaces also might allow for some
 * buffering (i.e. not trying to reuse a surface while it's busy).
 */
#define ADDTIONAL_SURFACES 3

// Magic number taken from original MPlayer vaapi patch.
#define MAX_DECODER_SURFACES 21

#define MAX_SURFACES (MAX_DECODER_SURFACES + ADDTIONAL_SURFACES)

struct priv {
    struct mp_vaapi_ctx *ctx;
    VADisplay display;

    // libavcodec shared struct
    struct vaapi_context *va_context;
    struct vaapi_context va_context_storage;

    int format, w, h;
    VASurfaceID surfaces[MAX_SURFACES];
};

struct profile_entry {
    enum AVCodecID av_codec;
    int ff_profile;
    VAProfile va_profile;
    int maxrefs;
};

#define PE(av_codec_id, ff_profile, va_dcoder_profile, maxrefs) \
    {AV_CODEC_ID_ ## av_codec_id,                               \
     FF_PROFILE_ ## ff_profile,                                 \
     VAProfile ## va_dcoder_profile,                            \
     maxrefs}

static const struct profile_entry profiles[] = {
    PE(MPEG2VIDEO,  MPEG2_SIMPLE,               MPEG2Simple,    2),
    PE(MPEG2VIDEO,  UNKNOWN,                    MPEG2Main,      2),
    PE(H264,        H264_BASELINE,              H264Baseline,   16),
    PE(H264,        H264_CONSTRAINED_BASELINE,  H264ConstrainedBaseline, 16),
    PE(H264,        H264_MAIN,                  H264Main,       16),
    PE(H264,        UNKNOWN,                    H264High,       16),
    PE(WMV3,        VC1_SIMPLE,                 VC1Simple,      2),
    PE(WMV3,        VC1_MAIN,                   VC1Main,        2),
    PE(WMV3,        UNKNOWN,                    VC1Advanced,    2),
    PE(VC1,         VC1_SIMPLE,                 VC1Simple,      2),
    PE(VC1,         VC1_MAIN,                   VC1Main,        2),
    PE(VC1,         UNKNOWN,                    VC1Advanced,    2),
    // No idea whether these are correct
    PE(MPEG4,       MPEG4_SIMPLE,               MPEG4Simple,    2),
    PE(MPEG4,       MPEG4_MAIN,                 MPEG4Main,      2),
    PE(MPEG4,       UNKNOWN,                    MPEG4AdvancedSimple, 2),
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


static const char *str_va_profile(VAProfile profile)
{
    switch (profile) {
#define PROFILE(profile) \
        case VAProfile##profile: return "VAProfile" #profile
        PROFILE(MPEG2Simple);
        PROFILE(MPEG2Main);
        PROFILE(MPEG4Simple);
        PROFILE(MPEG4AdvancedSimple);
        PROFILE(MPEG4Main);
        PROFILE(H264Baseline);
        PROFILE(H264Main);
        PROFILE(H264High);
        PROFILE(VC1Simple);
        PROFILE(VC1Main);
        PROFILE(VC1Advanced);
#undef PROFILE
    }
    return "<unknown>";
}

static int find_entrypoint(int format, VAEntrypoint *ep, int num_ep)
{
    int entrypoint = -1;
    switch (format) {
    case IMGFMT_VAAPI:              entrypoint = VAEntrypointVLD;    break;
    case IMGFMT_VAAPI_MPEG2_IDCT:   entrypoint = VAEntrypointIDCT;   break;
    case IMGFMT_VAAPI_MPEG2_MOCO:   entrypoint = VAEntrypointMoComp; break;
    }
    for (int n = 0; n < num_ep; n++) {
        if (ep[n] == entrypoint)
            return entrypoint;
    }
    return -1;
}

static int is_direct_mapping(VADisplay display)
{
    VADisplayAttribute attr;
    VAStatus status;

#if VA_CHECK_VERSION(0,34,0)
    attr.type  = VADisplayAttribRenderMode;
    attr.flags = VA_DISPLAY_ATTRIB_GETTABLE;

    status = vaGetDisplayAttributes(display, &attr, 1);
    if (status == VA_STATUS_SUCCESS)
        return !(attr.value & (VA_RENDER_MODE_LOCAL_OVERLAY|
                               VA_RENDER_MODE_EXTERNAL_OVERLAY));
#else
    /* If the driver doesn't make a copy of the VA surface for
       display, then we have to retain it until it's no longer the
       visible surface. In other words, if the driver is using
       DirectSurface mode, we don't want to decode the new surface
       into the previous one that was used for display. */
    attr.type  = VADisplayAttribDirectSurface;
    attr.flags = VA_DISPLAY_ATTRIB_GETTABLE;

    status = vaGetDisplayAttributes(display, &attr, 1);
    if (status == VA_STATUS_SUCCESS)
        return !attr.value;
#endif
    return 0;
}

// Make vo_vaapi.c pool the required number of surfaces.
// This is very touchy: vo_vaapi.c must not free surfaces while we decode,
// and we must allocate only surfaces that were passed to the decoder on
// creation.
// We achieve this by deleting all previous surfaces, then allocate every
// surface needed. Then we free these surfaces, and rely on the fact that
// vo_vaapi.c keeps the released surfaces in the pool, and only allocates
// new surfaces out of that pool.
static int preallocate_surfaces(struct lavc_ctx *ctx, int va_rt_format, int num)
{
    struct priv *p = ctx->hwdec_priv;
    int res = -1;

    struct mp_image *tmp_surfaces[MAX_SURFACES] = {0};

    p->ctx->flush(p->ctx); // free previously allocated surfaces

    for (int n = 0; n < num; n++) {
        tmp_surfaces[n] = p->ctx->get_surface(p->ctx, va_rt_format, p->format,
                                              p->w, p->h);
        if (!tmp_surfaces[n])
            goto done;
        p->surfaces[n] = (uintptr_t)tmp_surfaces[n]->planes[3];
    }
    res = 0;

done:
    for (int n = 0; n < num; n++)
        talloc_free(tmp_surfaces[n]);
    return res;
}

static void destroy_decoder(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;

    if (p->va_context->context_id != VA_INVALID_ID) {
        vaDestroyContext(p->display, p->va_context->context_id);
        p->va_context->context_id = VA_INVALID_ID;
    }

    if (p->va_context->config_id != VA_INVALID_ID) {
        vaDestroyConfig(p->display, p->va_context->config_id);
        p->va_context->config_id = VA_INVALID_ID;
    }

    for (int n = 0; n < MAX_SURFACES; n++)
        p->surfaces[n] = VA_INVALID_ID;
}

static int create_decoder(struct lavc_ctx *ctx)
{
    void *tmp = talloc_new(NULL);

    struct priv *p = ctx->hwdec_priv;
    VAStatus status;
    int res = -1;

    assert(IMGFMT_IS_VAAPI(p->format));

    destroy_decoder(ctx);

    const struct profile_entry *pe = find_codec(ctx->avctx->codec_id,
                                                ctx->avctx->profile);
    if (!pe) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vaapi] Unknown codec!\n");
        goto error;
    }

    int num_profiles = vaMaxNumProfiles(p->display);
    VAProfile *va_profiles = talloc_zero_array(tmp, VAProfile, num_profiles);
    status = vaQueryConfigProfiles(p->display, va_profiles, &num_profiles);
    if (!check_va_status(status, "vaQueryConfigProfiles()"))
        goto error;
    mp_msg(MSGT_VO, MSGL_DBG2, "[vaapi] %d profiles available:\n", num_profiles);
    for (int i = 0; i < num_profiles; i++)
        mp_msg(MSGT_VO, MSGL_DBG2, "  %s\n", str_va_profile(va_profiles[i]));

    bool profile_found = false;
    for (int i = 0; i < num_profiles; i++) {
        if (pe->va_profile == va_profiles[i]) {
            profile_found = true;
            break;
        }
    }
    if (!profile_found) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vaapi] Profile '%s' not available.\n",
               str_va_profile(pe->va_profile));
        goto error;
    }

    int num_surfaces = pe->maxrefs;
    if (!is_direct_mapping(p->display)) {
        mp_msg(MSGT_VO, MSGL_V, "[vaapi] No direct mapping.\n");
        // Note: not sure why it has to be *=2 rather than +=1.
        num_surfaces *= 2;
    }
    num_surfaces = MPMIN(num_surfaces, MAX_DECODER_SURFACES) + ADDTIONAL_SURFACES;

    if (num_surfaces > MAX_SURFACES) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vaapi] Internal error: too many surfaces.\n");
        goto error;
    }

    if (preallocate_surfaces(ctx, VA_RT_FORMAT_YUV420, num_surfaces) < 0) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vaapi] Could not allocate surfaces.\n");
        goto error;
    }

    int num_ep = vaMaxNumEntrypoints(p->display);
    VAEntrypoint *ep = talloc_zero_array(tmp, VAEntrypoint, num_ep);
    status = vaQueryConfigEntrypoints(p->display, pe->va_profile, ep, &num_ep);
    if (!check_va_status(status, "vaQueryConfigEntrypoints()"))
        goto error;

    VAEntrypoint entrypoint = find_entrypoint(p->format, ep, num_ep);
    if (entrypoint < 0) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vaapi] Could not find VA entrypoint.\n");
        goto error;
    }

    VAConfigAttrib attrib = {
        .type = VAConfigAttribRTFormat,
    };
    status = vaGetConfigAttributes(p->display, pe->va_profile, entrypoint,
                                   &attrib, 1);
    if (!check_va_status(status, "vaGetConfigAttributes()"))
        goto error;
    if ((attrib.value & VA_RT_FORMAT_YUV420) == 0) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vaapi] Chroma format not supported.\n");
        goto error;
    }

    status = vaCreateConfig(p->display, pe->va_profile, entrypoint, &attrib, 1,
                            &p->va_context->config_id);
    if (!check_va_status(status, "vaCreateConfig()"))
        goto error;

    status = vaCreateContext(p->display, p->va_context->config_id,
                             p->w, p->h, VA_PROGRESSIVE,
                             p->surfaces, num_surfaces,
                             &p->va_context->context_id);
    if (!check_va_status(status, "vaCreateContext()"))
        goto error;

    res = 0;
error:
    talloc_free(tmp);
    return res;
}

static struct mp_image *allocate_image(struct lavc_ctx *ctx, int format,
                                       int w, int h)
{
    struct priv *p = ctx->hwdec_priv;

    if (!IMGFMT_IS_VAAPI(format))
        return NULL;

    if (format != p->format || w != p->w || h != p->h ||
        p->va_context->context_id == VA_INVALID_ID)
    {
        p->format = format;
        p->w = w;
        p->h = h;
        if (create_decoder(ctx) < 0)
            return NULL;
    }

    struct mp_image *img = p->ctx->get_surface(p->ctx, VA_RT_FORMAT_YUV420,
                                               format, p->w, p->h);
    if (img) {
        for (int n = 0; n < MAX_SURFACES; n++) {
            if (p->surfaces[n] == (uintptr_t)img->planes[3])
                return img;
        }
        talloc_free(img);
    }
    mp_msg(MSGT_VO, MSGL_ERR, "[vaapi] Insufficient number of surfaces.\n");
    return NULL;
}

static void uninit(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;

    if (!p)
        return;

    destroy_decoder(ctx);

    talloc_free(p);
    ctx->hwdec_priv = NULL;
}

static int init(struct lavc_ctx *ctx)
{
    struct priv *p = talloc_ptrtype(NULL, p);
    *p = (struct priv) {
        .ctx = ctx->hwdec_info->vaapi_ctx,
        .va_context = &p->va_context_storage,
    };
    ctx->hwdec_priv = p;

    p->display = p->ctx->display;

    p->va_context->display = p->display;
    p->va_context->config_id = VA_INVALID_ID;
    p->va_context->context_id = VA_INVALID_ID;

    ctx->avctx->hwaccel_context = p->va_context;

    return 0;
}


static int probe(struct vd_lavc_hwdec *hwdec, struct mp_hwdec_info *info,
                 const char *decoder)
{
    if (!info || !info->vaapi_ctx)
        return HWDEC_ERR_NO_CTX;
    if (!find_codec(mp_codec_to_av_codec_id(decoder), FF_PROFILE_UNKNOWN))
        return HWDEC_ERR_NO_CODEC;
    return 0;
}

const struct vd_lavc_hwdec mp_vd_lavc_vaapi = {
    .type = HWDEC_VAAPI,
    .image_formats = (const int[]) {IMGFMT_VAAPI, IMGFMT_VAAPI_MPEG2_IDCT,
                                    IMGFMT_VAAPI_MPEG2_MOCO, 0},
    .probe = probe,
    .init = init,
    .uninit = uninit,
    .allocate_image = allocate_image,
};
