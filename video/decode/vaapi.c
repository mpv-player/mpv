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

#include <X11/Xlib.h>

#include "lavc.h"
#include "common/common.h"
#include "common/av_common.h"
#include "video/fmt-conversion.h"
#include "video/vaapi.h"
#include "video/mp_image_pool.h"
#include "video/hwdec.h"
#include "video/filter/vf.h"

/*
 * The VAAPI decoder can work only with surfaces passed to the decoder at
 * creation time. This means all surfaces have to be created in advance.
 * So, additionally to the maximum number of reference frames, we need
 * surfaces for:
 * - 1 decode frame
 * - decoding 2 frames ahead (done by generic playback code)
 * - keeping the reference to the previous frame (done by vo_vaapi.c)
 * - keeping the reference to a dropped frame (done by vo.c)
 * Note that redundant additional surfaces also might allow for some
 * buffering (i.e. not trying to reuse a surface while it's busy).
 */
#define ADDTIONAL_SURFACES 5

// Magic number taken from original MPlayer vaapi patch.
#define MAX_DECODER_SURFACES 21

#define MAX_SURFACES (MAX_DECODER_SURFACES + ADDTIONAL_SURFACES)

struct priv {
    struct mp_log *log;
    struct mp_vaapi_ctx *ctx;
    VADisplay display;
    Display *x11_display;

    // libavcodec shared struct
    struct vaapi_context *va_context;
    struct vaapi_context va_context_storage;

    struct mp_image_pool *pool;
    int rt_format;

    struct mp_image_pool *sw_pool;
};

#define PE(av_codec_id, ff_profile, vdp_profile)                \
    {AV_CODEC_ID_ ## av_codec_id, FF_PROFILE_ ## ff_profile,    \
     VAProfile ## vdp_profile}

static const struct hwdec_profile_entry profiles[] = {
    PE(MPEG2VIDEO,  MPEG2_MAIN,         MPEG2Main),
    PE(MPEG2VIDEO,  MPEG2_SIMPLE,       MPEG2Simple),
    PE(MPEG4,       MPEG4_ADVANCED_SIMPLE, MPEG4AdvancedSimple),
    PE(MPEG4,       MPEG4_MAIN,         MPEG4Main),
    PE(MPEG4,       MPEG4_SIMPLE,       MPEG4Simple),
    PE(H264,        H264_HIGH,          H264High),
    PE(H264,        H264_MAIN,          H264Main),
    PE(H264,        H264_BASELINE,      H264Baseline),
    PE(VC1,         VC1_ADVANCED,       VC1Advanced),
    PE(VC1,         VC1_MAIN,           VC1Main),
    PE(VC1,         VC1_SIMPLE,         VC1Simple),
    PE(WMV3,        VC1_ADVANCED,       VC1Advanced),
    PE(WMV3,        VC1_MAIN,           VC1Main),
    PE(WMV3,        VC1_SIMPLE,         VC1Simple),
    {0}
};

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
    }
    for (int n = 0; n < num_ep; n++) {
        if (ep[n] == entrypoint)
            return entrypoint;
    }
    return -1;
}

static int is_direct_mapping(VADisplay display)
{
    VADisplayAttribute attr = {0};
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

// We must allocate only surfaces that were passed to the decoder on creation.
// We achieve this by reserving surfaces in the pool as needed.
// Releasing surfaces is necessary after filling the surface id list so
// that reserved surfaces can be reused for decoding.
static bool preallocate_surfaces(struct lavc_ctx *ctx, int num, int w, int h,
                                 VASurfaceID out_surfaces[MAX_SURFACES])
{
    struct priv *p = ctx->hwdec_priv;
    assert(num <= MAX_SURFACES);
    struct mp_image *reserve[MAX_SURFACES] = {0};
    bool res = true;

    for (int n = 0; n < num; n++) {
        reserve[n] = mp_image_pool_get(p->pool, IMGFMT_VAAPI, w, h);
        out_surfaces[n] = va_surface_id(reserve[n]);
        if (out_surfaces[n] == VA_INVALID_ID) {
            MP_ERR(p, "Could not allocate surfaces.\n");
            res = false;
            break;
        }
    }
    for (int i = 0; i < num; i++)
        talloc_free(reserve[i]);
    return res;
}

static void destroy_decoder(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;

    va_lock(p->ctx);

    if (p->va_context->context_id != VA_INVALID_ID) {
        vaDestroyContext(p->display, p->va_context->context_id);
        p->va_context->context_id = VA_INVALID_ID;
    }

    if (p->va_context->config_id != VA_INVALID_ID) {
        vaDestroyConfig(p->display, p->va_context->config_id);
        p->va_context->config_id = VA_INVALID_ID;
    }

    va_unlock(p->ctx);

    mp_image_pool_clear(p->pool);
}

static bool has_profile(VAProfile *va_profiles, int num_profiles, VAProfile p)
{
    for (int i = 0; i < num_profiles; i++) {
        if (va_profiles[i] == p)
            return true;
    }
    return false;
}

static int init_decoder(struct lavc_ctx *ctx, int fmt, int w, int h)
{
    void *tmp = talloc_new(NULL);

    struct priv *p = ctx->hwdec_priv;
    VAStatus status;
    int res = -1;

    destroy_decoder(ctx);

    va_lock(p->ctx);

    const struct hwdec_profile_entry *pe = hwdec_find_profile(ctx, profiles);
    if (!pe) {
        MP_ERR(p, "Unsupported codec or profile.\n");
        goto error;
    }

    int num_profiles = vaMaxNumProfiles(p->display);
    VAProfile *va_profiles = talloc_zero_array(tmp, VAProfile, num_profiles);
    status = vaQueryConfigProfiles(p->display, va_profiles, &num_profiles);
    if (!CHECK_VA_STATUS(p, "vaQueryConfigProfiles()"))
        goto error;
    MP_DBG(p, "%d profiles available:\n", num_profiles);
    for (int i = 0; i < num_profiles; i++)
        MP_DBG(p, "  %s\n", str_va_profile(va_profiles[i]));

    VAProfile va_profile = pe->hw_profile;
    if (!has_profile(va_profiles, num_profiles, va_profile)) {
        MP_ERR(p, "Decoder profile '%s' not available.\n",
               str_va_profile(va_profile));
        goto error;
    }

    MP_VERBOSE(p, "Using profile '%s'.\n", str_va_profile(va_profile));

    int num_surfaces = hwdec_get_max_refs(ctx);
    if (!is_direct_mapping(p->display)) {
        MP_VERBOSE(p, "No direct mapping.\n");
        // Note: not sure why it has to be *=2 rather than +=1.
        num_surfaces *= 2;
    }
    num_surfaces = MPMIN(num_surfaces, MAX_DECODER_SURFACES) + ADDTIONAL_SURFACES;

    if (num_surfaces > MAX_SURFACES) {
        MP_ERR(p, "Internal error: too many surfaces.\n");
        goto error;
    }

    VASurfaceID surfaces[MAX_SURFACES];
    if (!preallocate_surfaces(ctx, num_surfaces, w, h, surfaces)) {
        MP_ERR(p, "Could not allocate surfaces.\n");
        goto error;
    }

    int num_ep = vaMaxNumEntrypoints(p->display);
    VAEntrypoint *ep = talloc_zero_array(tmp, VAEntrypoint, num_ep);
    status = vaQueryConfigEntrypoints(p->display, va_profile, ep, &num_ep);
    if (!CHECK_VA_STATUS(p, "vaQueryConfigEntrypoints()"))
        goto error;

    int entrypoint = find_entrypoint(IMGFMT_VAAPI, ep, num_ep);
    if (entrypoint < 0) {
        MP_ERR(p, "Could not find VA entrypoint.\n");
        goto error;
    }

    VAConfigAttrib attrib = {
        .type = VAConfigAttribRTFormat,
    };
    status = vaGetConfigAttributes(p->display, va_profile, entrypoint,
                                   &attrib, 1);
    if (!CHECK_VA_STATUS(p, "vaGetConfigAttributes()"))
        goto error;
    if ((attrib.value & p->rt_format) == 0) {
        MP_ERR(p, "Chroma format not supported.\n");
        goto error;
    }

    status = vaCreateConfig(p->display, va_profile, entrypoint, &attrib, 1,
                            &p->va_context->config_id);
    if (!CHECK_VA_STATUS(p, "vaCreateConfig()"))
        goto error;

    status = vaCreateContext(p->display, p->va_context->config_id,
                             w, h, VA_PROGRESSIVE,
                             surfaces, num_surfaces,
                             &p->va_context->context_id);
    if (!CHECK_VA_STATUS(p, "vaCreateContext()"))
        goto error;

    res = 0;
error:
    va_unlock(p->ctx);
    talloc_free(tmp);
    return res;
}

static struct mp_image *allocate_image(struct lavc_ctx *ctx, int format,
                                       int w, int h)
{
    struct priv *p = ctx->hwdec_priv;

    struct mp_image *img =
        mp_image_pool_get_no_alloc(p->pool, IMGFMT_VAAPI, w, h);
    if (!img)
        MP_ERR(p, "Insufficient number of surfaces.\n");
    return img;
}

static void destroy_va_dummy_ctx(struct priv *p)
{
    va_destroy(p->ctx);
    p->ctx = NULL;
    p->display = NULL;
    if (p->x11_display)
        XCloseDisplay(p->x11_display);
    p->x11_display = NULL;
}

// Creates a "private" VADisplay, disconnected from the VO. We just create a
// new X connection, because that's simpler. (We could also pass the X
// connection along with struct mp_hwdec_info, if we wanted.)
static bool create_va_dummy_ctx(struct priv *p)
{
    p->x11_display = XOpenDisplay(NULL);
    if (!p->x11_display)
        goto destroy_ctx;
    VADisplay *display = vaGetDisplay(p->x11_display);
    if (!display)
        goto destroy_ctx;
    p->ctx = va_initialize(display, p->log);
    if (!p->ctx) {
        vaTerminate(display);
        goto destroy_ctx;
    }
    return true;
destroy_ctx:
    destroy_va_dummy_ctx(p);
    return false;
}

static void uninit(struct lavc_ctx *ctx)
{
    struct priv *p = ctx->hwdec_priv;

    if (!p)
        return;

    destroy_decoder(ctx);

    talloc_free(p->pool);
    p->pool = NULL;

    if (p->x11_display)
        destroy_va_dummy_ctx(p);

    talloc_free(p);
    ctx->hwdec_priv = NULL;
}

static int init_with_vactx(struct lavc_ctx *ctx, struct mp_vaapi_ctx *vactx)
{
    struct priv *p = talloc_ptrtype(NULL, p);
    *p = (struct priv) {
        .log = mp_log_new(p, ctx->log, "vaapi"),
        .ctx = vactx,
        .va_context = &p->va_context_storage,
        .rt_format = VA_RT_FORMAT_YUV420
    };

    if (!p->ctx)
        create_va_dummy_ctx(p);
    if (!p->ctx) {
        talloc_free(p);
        return -1;
    }

    p->display = p->ctx->display;
    p->pool = talloc_steal(p, mp_image_pool_new(MAX_SURFACES));
    va_pool_set_allocator(p->pool, p->ctx, p->rt_format);
    p->sw_pool = talloc_steal(p, mp_image_pool_new(17));

    p->va_context->display = p->display;
    p->va_context->config_id = VA_INVALID_ID;
    p->va_context->context_id = VA_INVALID_ID;

    ctx->avctx->hwaccel_context = p->va_context;
    ctx->hwdec_priv = p;

    return 0;
}

static int init(struct lavc_ctx *ctx)
{
    return init_with_vactx(ctx, ctx->hwdec_info->hwctx->vaapi_ctx);
}

static int probe(struct vd_lavc_hwdec *hwdec, struct mp_hwdec_info *info,
                 const char *decoder)
{
    hwdec_request_api(info, "vaapi");
    if (!info || !info->hwctx || !info->hwctx->vaapi_ctx)
        return HWDEC_ERR_NO_CTX;
    if (!hwdec_check_codec_support(decoder, profiles))
        return HWDEC_ERR_NO_CODEC;
    if (va_guess_if_emulated(info->hwctx->vaapi_ctx))
        return HWDEC_ERR_EMULATED;
    return 0;
}

static int probe_copy(struct vd_lavc_hwdec *hwdec, struct mp_hwdec_info *info,
                      const char *decoder)
{
    struct priv dummy = {mp_null_log};
    if (!create_va_dummy_ctx(&dummy))
        return HWDEC_ERR_NO_CTX;
    bool emulated = va_guess_if_emulated(dummy.ctx);
    destroy_va_dummy_ctx(&dummy);
    if (!hwdec_check_codec_support(decoder, profiles))
        return HWDEC_ERR_NO_CODEC;
    if (emulated)
        return HWDEC_ERR_EMULATED;
    return 0;
}

static int init_copy(struct lavc_ctx *ctx)
{
    return init_with_vactx(ctx, NULL);
}

static struct mp_image *copy_image(struct lavc_ctx *ctx, struct mp_image *img)
{
    struct priv *p = ctx->hwdec_priv;

    struct mp_image *simg = va_surface_download(img, p->sw_pool);
    if (simg) {
        talloc_free(img);
        return simg;
    }
    return img;
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
    .probe = probe,
    .init = init,
    .uninit = uninit,
    .init_decoder = init_decoder,
    .allocate_image = allocate_image,
    .lock = intel_shit_lock,
    .unlock = intel_crap_unlock,
};

const struct vd_lavc_hwdec mp_vd_lavc_vaapi_copy = {
    .type = HWDEC_VAAPI_COPY,
    .image_format = IMGFMT_VAAPI,
    .probe = probe_copy,
    .init = init_copy,
    .uninit = uninit,
    .init_decoder = init_decoder,
    .allocate_image = allocate_image,
    .process_image = copy_image,
};
