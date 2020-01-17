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
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>

#include "config.h"

#include "video/out/gpu/hwdec.h"
#include "video/out/hwdec/hwdec_vaapi.h"
#include "video/fmt-conversion.h"
#include "video/mp_image_pool.h"
#include "video/vaapi.h"

#if HAVE_VAAPI_DRM
#include "libmpv/render_gl.h"
#endif

#if HAVE_VAAPI_X11
#include <va/va_x11.h>

static VADisplay *create_x11_va_display(struct ra *ra)
{
    Display *x11 = ra_get_native_resource(ra, "x11");
    return x11 ? vaGetDisplay(x11) : NULL;
}
#endif

#if HAVE_VAAPI_WAYLAND
#include <va/va_wayland.h>

static VADisplay *create_wayland_va_display(struct ra *ra)
{
    struct wl_display *wl = ra_get_native_resource(ra, "wl");
    return wl ? vaGetDisplayWl(wl) : NULL;
}
#endif

#if HAVE_VAAPI_DRM
#include <va/va_drm.h>

static VADisplay *create_drm_va_display(struct ra *ra)
{
    mpv_opengl_drm_params_v2 *params = ra_get_native_resource(ra, "drm_params_v2");
    if (!params || params->render_fd == -1)
        return NULL;

    return vaGetDisplayDRM(params->render_fd);
}
#endif

struct va_create_native {
    const char *name;
    VADisplay *(*create)(struct ra *ra);
};

static const struct va_create_native create_native_cbs[] = {
#if HAVE_VAAPI_X11
    {"x11",     create_x11_va_display},
#endif
#if HAVE_VAAPI_WAYLAND
    {"wayland", create_wayland_va_display},
#endif
#if HAVE_VAAPI_DRM
    {"drm",     create_drm_va_display},
#endif
};

static VADisplay *create_native_va_display(struct ra *ra, struct mp_log *log)
{
    for (int n = 0; n < MP_ARRAY_SIZE(create_native_cbs); n++) {
        const struct va_create_native *disp = &create_native_cbs[n];
        mp_verbose(log, "Trying to open a %s VA display...\n", disp->name);
        VADisplay *display = disp->create(ra);
        if (display)
            return display;
    }
    return NULL;
}

static void determine_working_formats(struct ra_hwdec *hw);

static void uninit(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;
    if (p->ctx)
        hwdec_devices_remove(hw->devs, &p->ctx->hwctx);
    va_destroy(p->ctx);
}

const static vaapi_interop_init interop_inits[] = {
#if HAVE_VAAPI_EGL
    vaapi_gl_init,
#endif
#if HAVE_VAAPI_VULKAN
    vaapi_vk_init,
#endif
    NULL
};

static int init(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;

    for (int i = 0; interop_inits[i]; i++) {
        if (interop_inits[i](hw)) {
            break;
        }
    }

    if (!p->interop_map || !p->interop_unmap) {
        MP_VERBOSE(hw, "VAAPI hwdec only works with OpenGL or Vulkan backends.\n");
        return -1;
    }

    p->display = create_native_va_display(hw->ra, hw->log);
    if (!p->display) {
        MP_VERBOSE(hw, "Could not create a VA display.\n");
        return -1;
    }

    p->ctx = va_initialize(p->display, hw->log, true);
    if (!p->ctx) {
        vaTerminate(p->display);
        return -1;
    }
    if (!p->ctx->av_device_ref) {
        MP_VERBOSE(hw, "libavutil vaapi code rejected the driver?\n");
        return -1;
    }

    if (hw->probing && va_guess_if_emulated(p->ctx)) {
        return -1;
    }

    determine_working_formats(hw);
    if (!p->formats || !p->formats[0]) {
        return -1;
    }

    p->ctx->hwctx.hw_imgfmt = IMGFMT_VAAPI;
    p->ctx->hwctx.supported_formats = p->formats;
    p->ctx->hwctx.driver_name = hw->driver->name;
    hwdec_devices_add(hw->devs, &p->ctx->hwctx);
    return 0;
}

static void mapper_unmap(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct priv *p = mapper->priv;

    p_owner->interop_unmap(mapper);

    if (p->surface_acquired) {
        for (int n = 0; n < p->desc.num_objects; n++)
            close(p->desc.objects[n].fd);
        p->surface_acquired = false;
    }
}

static void mapper_uninit(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    if (p_owner->interop_uninit) {
        p_owner->interop_uninit(mapper);
    }
}

static bool check_fmt(struct ra_hwdec_mapper *mapper, int fmt)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    for (int n = 0; p_owner->formats && p_owner->formats[n]; n++) {
        if (p_owner->formats[n] == fmt)
            return true;
    }
    return false;
}

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct priv *p = mapper->priv;

    mapper->dst_params = mapper->src_params;
    mapper->dst_params.imgfmt = mapper->src_params.hw_subfmt;
    mapper->dst_params.hw_subfmt = 0;

    struct ra_imgfmt_desc desc = {0};

    if (!ra_get_imgfmt_desc(mapper->ra, mapper->dst_params.imgfmt, &desc))
        return -1;

    p->num_planes = desc.num_planes;
    mp_image_set_params(&p->layout, &mapper->dst_params);

    if (p_owner->interop_init)
        if (!p_owner->interop_init(mapper, &desc))
            return -1;

    if (!p_owner->probing_formats && !check_fmt(mapper, mapper->dst_params.imgfmt))
    {
        MP_FATAL(mapper, "unsupported VA image format %s\n",
                 mp_imgfmt_to_name(mapper->dst_params.imgfmt));
        return -1;
    }

    return 0;
}

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct priv *p = mapper->priv;
    VAStatus status;
    VADisplay *display = p_owner->display;

    status = vaExportSurfaceHandle(display, va_surface_id(mapper->src),
                                   VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                   VA_EXPORT_SURFACE_READ_ONLY |
                                   VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                                   &p->desc);
    if (!CHECK_VA_STATUS_LEVEL(mapper, "vaExportSurfaceHandle()",
                               p_owner->probing_formats ? MSGL_DEBUG : MSGL_ERR))
    {
        goto err;
    }
    vaSyncSurface(display, va_surface_id(mapper->src));
    // No need to error out if sync fails, but good to know if it did.
    CHECK_VA_STATUS(mapper, "vaSyncSurface()");
    p->surface_acquired = true;

    if (!p_owner->interop_map(mapper))
        goto err;

    if (p->desc.fourcc == VA_FOURCC_YV12)
        MPSWAP(struct ra_tex*, mapper->tex[1], mapper->tex[2]);

    return 0;

err:
    mapper_unmap(mapper);

    if (!p_owner->probing_formats)
        MP_FATAL(mapper, "mapping VAAPI EGL image failed\n");
    return -1;
}

static bool try_format_map(struct ra_hwdec *hw, struct mp_image *surface)
{
    bool ok = false;
    struct ra_hwdec_mapper *mapper = ra_hwdec_mapper_create(hw, &surface->params);
    if (mapper)
        ok = ra_hwdec_mapper_map(mapper, surface) >= 0;
    ra_hwdec_mapper_free(&mapper);
    return ok;
}

static void try_format_pixfmt(struct ra_hwdec *hw, enum AVPixelFormat pixfmt)
{
    struct priv_owner *p = hw->priv;

    int mp_fmt = pixfmt2imgfmt(pixfmt);
    if (!mp_fmt)
        return;

    int num_formats = 0;
    for (int n = 0; p->formats && p->formats[n]; n++) {
        if (p->formats[n] == mp_fmt)
            return; // already added
        num_formats += 1;
    }

    AVBufferRef *fref = NULL;
    struct mp_image *s = NULL;
    AVFrame *frame = NULL;
    fref = av_hwframe_ctx_alloc(p->ctx->av_device_ref);
    if (!fref)
        goto err;
    AVHWFramesContext *fctx = (void *)fref->data;
    fctx->format = AV_PIX_FMT_VAAPI;
    fctx->sw_format = pixfmt;
    fctx->width = 128;
    fctx->height = 128;
    if (av_hwframe_ctx_init(fref) < 0)
        goto err;
    frame = av_frame_alloc();
    if (!frame)
        goto err;
    if (av_hwframe_get_buffer(fref, frame, 0) < 0)
        goto err;
    s = mp_image_from_av_frame(frame);
    if (!s || !mp_image_params_valid(&s->params))
        goto err;
    if (try_format_map(hw, s)) {
        MP_TARRAY_APPEND(p, p->formats, num_formats, mp_fmt);
        MP_TARRAY_APPEND(p, p->formats, num_formats, 0); // terminate it
    }
err:
    talloc_free(s);
    av_frame_free(&frame);
    av_buffer_unref(&fref);
}

static void try_format_config(struct ra_hwdec *hw, AVVAAPIHWConfig *hwconfig)
{
    struct priv_owner *p = hw->priv;
    AVHWFramesConstraints *fc =
            av_hwdevice_get_hwframe_constraints(p->ctx->av_device_ref, hwconfig);
    if (!fc) {
        MP_WARN(hw, "failed to retrieve libavutil frame constraints\n");
        return;
    }
    for (int n = 0; fc->valid_sw_formats &&
                    fc->valid_sw_formats[n] != AV_PIX_FMT_NONE; n++)
        try_format_pixfmt(hw, fc->valid_sw_formats[n]);
    av_hwframe_constraints_free(&fc);
}

static void determine_working_formats(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;
    VAStatus status;
    VAProfile *profiles = NULL;
    VAEntrypoint *entrypoints = NULL;

    MP_VERBOSE(hw, "Going to probe surface formats (may log bogus errors)...\n");
    p->probing_formats = true;

    AVVAAPIHWConfig *hwconfig = av_hwdevice_hwconfig_alloc(p->ctx->av_device_ref);
    if (!hwconfig) {
        MP_WARN(hw, "Could not allocate FFmpeg AVVAAPIHWConfig\n");
        goto done;
    }

    profiles = talloc_zero_array(NULL, VAProfile, vaMaxNumProfiles(p->display));
    entrypoints = talloc_zero_array(NULL, VAEntrypoint,
                                    vaMaxNumEntrypoints(p->display));
    int num_profiles = 0;
    status = vaQueryConfigProfiles(p->display, profiles, &num_profiles);
    if (!CHECK_VA_STATUS(hw, "vaQueryConfigProfiles()"))
        num_profiles = 0;

    for (int n = 0; n < num_profiles; n++) {
        VAProfile profile = profiles[n];
        int num_ep = 0;
        status = vaQueryConfigEntrypoints(p->display, profile, entrypoints,
                                          &num_ep);
        if (status != VA_STATUS_SUCCESS) {
            MP_DBG(hw, "vaQueryConfigEntrypoints(): '%s' for profile %d",
                   vaErrorStr(status), (int)profile);
            continue;
        }
        for (int ep = 0; ep < num_ep; ep++) {
            VAConfigID config = VA_INVALID_ID;
            status = vaCreateConfig(p->display, profile, entrypoints[ep],
                                    NULL, 0, &config);
            if (status != VA_STATUS_SUCCESS) {
                MP_DBG(hw, "vaCreateConfig(): '%s' for profile %d",
                       vaErrorStr(status), (int)profile);
                continue;
            }

            hwconfig->config_id = config;
            try_format_config(hw, hwconfig);

            vaDestroyConfig(p->display, config);
        }
    }

done:
    av_free(hwconfig);
    talloc_free(profiles);
    talloc_free(entrypoints);

    p->probing_formats = false;

    MP_DBG(hw, "Supported formats:\n");
    for (int n = 0; p->formats && p->formats[n]; n++)
        MP_DBG(hw, " %s\n", mp_imgfmt_to_name(p->formats[n]));
    MP_VERBOSE(hw, "Done probing surface formats.\n");
}

const struct ra_hwdec_driver ra_hwdec_vaegl = {
    .name = "vaapi-egl",
    .priv_size = sizeof(struct priv_owner),
    .imgfmts = {IMGFMT_VAAPI, 0},
    .init = init,
    .uninit = uninit,
    .mapper = &(const struct ra_hwdec_mapper_driver){
        .priv_size = sizeof(struct priv),
        .init = mapper_init,
        .uninit = mapper_uninit,
        .map = mapper_map,
        .unmap = mapper_unmap,
    },
};
