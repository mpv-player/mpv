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
#include "mpvcore/mp_common.h"
#include "mpvcore/av_common.h"
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
    Display *x11_display;

    // libavcodec shared struct
    struct vaapi_context *va_context;
    struct vaapi_context va_context_storage;

    int format, w, h;
    VASurfaceID surfaces[MAX_SURFACES];

    struct va_surface_pool *pool;
    int rt_format;

    struct mp_image_pool *sw_pool;
    bool printed_readback_warning;
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

// We must allocate only surfaces that were passed to the decoder on creation.
// We achieve this by reserving surfaces in the pool as needed.
// Releasing surfaces is necessary after filling the surface id list so
// that reserved surfaces can be reused for decoding.
static bool preallocate_surfaces(struct lavc_ctx *ctx, int num)
{
    struct priv *p = ctx->hwdec_priv;
    if (!va_surface_pool_reserve(p->pool, num, p->w, p->h)) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vaapi] Could not allocate surfaces.\n");
        return false;
    }
    for (int i = 0; i < num; i++) {
        struct va_surface *s = va_surface_pool_get(p->pool, p->w, p->h);
        p->surfaces[i] = s->id;
        va_surface_release(s);
    }
    return true;
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

static bool has_profile(VAProfile *va_profiles, int num_profiles, VAProfile p)
{
    for (int i = 0; i < num_profiles; i++) {
        if (va_profiles[i] == p)
            return true;
    }
    return false;
}

static int create_decoder(struct lavc_ctx *ctx)
{
    void *tmp = talloc_new(NULL);

    struct priv *p = ctx->hwdec_priv;
    VAStatus status;
    int res = -1;

    assert(IMGFMT_IS_VAAPI(p->format));

    destroy_decoder(ctx);

    const struct hwdec_profile_entry *pe = hwdec_find_profile(ctx, profiles);
    if (!pe) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vaapi] Unsupported codec or profile.\n");
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

    VAProfile va_profile = pe->hw_profile;
    if (!has_profile(va_profiles, num_profiles, va_profile)) {
        mp_msg(MSGT_VO, MSGL_ERR,
               "[vaapi] Decoder profile '%s' not available.\n",
               str_va_profile(va_profile));
        goto error;
    }

    mp_msg(MSGT_VO, MSGL_V, "[vaapi] Using profile '%s'.\n",
           str_va_profile(va_profile));

    int num_surfaces = hwdec_get_max_refs(ctx);
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

    if (!preallocate_surfaces(ctx, num_surfaces)) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vaapi] Could not allocate surfaces.\n");
        goto error;
    }

    int num_ep = vaMaxNumEntrypoints(p->display);
    VAEntrypoint *ep = talloc_zero_array(tmp, VAEntrypoint, num_ep);
    status = vaQueryConfigEntrypoints(p->display, va_profile, ep, &num_ep);
    if (!check_va_status(status, "vaQueryConfigEntrypoints()"))
        goto error;

    int entrypoint = find_entrypoint(p->format, ep, num_ep);
    if (entrypoint < 0) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vaapi] Could not find VA entrypoint.\n");
        goto error;
    }

    VAConfigAttrib attrib = {
        .type = VAConfigAttribRTFormat,
    };
    status = vaGetConfigAttributes(p->display, va_profile, entrypoint,
                                   &attrib, 1);
    if (!check_va_status(status, "vaGetConfigAttributes()"))
        goto error;
    if ((attrib.value & p->rt_format) == 0) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vaapi] Chroma format not supported.\n");
        goto error;
    }

    status = vaCreateConfig(p->display, va_profile, entrypoint, &attrib, 1,
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

    struct va_surface *s = va_surface_pool_get(p->pool, p->w, p->h);
    if (s) {
        for (int n = 0; n < MAX_SURFACES; n++) {
            if (p->surfaces[n] == s->id)
                return va_surface_wrap(s);
        }
        va_surface_release(s);
    }
    mp_msg(MSGT_VO, MSGL_ERR, "[vaapi] Insufficient number of surfaces.\n");
    return NULL;
}


static void destroy_va_dummy_ctx(struct priv *p)
{
    if (p->x11_display)
        XCloseDisplay(p->x11_display);
    p->x11_display = NULL;
    va_destroy(p->ctx);
    p->ctx = NULL;
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
    p->ctx = va_initialize(display);
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
    va_surface_pool_release(p->pool);

    if (p->x11_display)
        destroy_va_dummy_ctx(p);

    talloc_free(p);
    ctx->hwdec_priv = NULL;
}

static int init_with_vactx(struct lavc_ctx *ctx, struct mp_vaapi_ctx *vactx)
{
    struct priv *p = talloc_ptrtype(NULL, p);
    *p = (struct priv) {
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
    p->pool = va_surface_pool_alloc(p->display, p->rt_format);
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
    if (!ctx->hwdec_info->vaapi_ctx)
        return -1;
    return init_with_vactx(ctx, ctx->hwdec_info->vaapi_ctx);
}

static int probe(struct vd_lavc_hwdec *hwdec, struct mp_hwdec_info *info,
                 const char *decoder)
{
    hwdec_request_api(info, "vaapi");
    if (!info || !info->vaapi_ctx)
        return HWDEC_ERR_NO_CTX;
    if (!hwdec_check_codec_support(decoder, profiles))
        return HWDEC_ERR_NO_CODEC;
    return 0;
}

static int probe_copy(struct vd_lavc_hwdec *hwdec, struct mp_hwdec_info *info,
                      const char *decoder)
{
    struct priv dummy = {0};
    if (!create_va_dummy_ctx(&dummy))
        return HWDEC_ERR_NO_CTX;
    destroy_va_dummy_ctx(&dummy);
    if (!hwdec_check_codec_support(decoder, profiles))
        return HWDEC_ERR_NO_CODEC;
    return 0;
}

static int init_copy(struct lavc_ctx *ctx)
{
    return init_with_vactx(ctx, NULL);
}

static struct mp_image *copy_image(struct lavc_ctx *ctx, struct mp_image *img)
{
    struct priv *p = ctx->hwdec_priv;

    struct va_surface *surface = va_surface_in_mp_image(img);
    if (surface) {
        struct mp_image *simg =
            va_surface_download(surface, p->ctx->image_formats, p->sw_pool);
        if (simg) {
            if (!p->printed_readback_warning) {
                mp_msg(MSGT_VO, MSGL_WARN, "[vaapi] Using GPU readback. This "
                       "is usually inefficient.\n");
                p->printed_readback_warning = true;
            }
            talloc_free(img);
            return simg;
        }
    }
    return img;
}

const struct vd_lavc_hwdec mp_vd_lavc_vaapi = {
    .type = HWDEC_VAAPI,
    .image_formats = (const int[]) {IMGFMT_VAAPI, 0},
    .probe = probe,
    .init = init,
    .uninit = uninit,
    .allocate_image = allocate_image,
};

const struct vd_lavc_hwdec mp_vd_lavc_vaapi_copy = {
    .type = HWDEC_VAAPI_COPY,
    .image_formats = (const int[]) {IMGFMT_VAAPI, 0},
    .probe = probe_copy,
    .init = init_copy,
    .uninit = uninit,
    .allocate_image = allocate_image,
    .process_image = copy_image,
};
