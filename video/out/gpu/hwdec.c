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

#include "config.h"

#include "common/common.h"
#include "common/msg.h"
#include "options/m_config.h"
#include "hwdec.h"

extern const struct ra_hwdec_driver ra_hwdec_vaapi;
extern const struct ra_hwdec_driver ra_hwdec_videotoolbox;
extern const struct ra_hwdec_driver ra_hwdec_vdpau;
extern const struct ra_hwdec_driver ra_hwdec_dxva2egl;
extern const struct ra_hwdec_driver ra_hwdec_d3d11egl;
extern const struct ra_hwdec_driver ra_hwdec_dxva2gldx;
extern const struct ra_hwdec_driver ra_hwdec_d3d11va;
extern const struct ra_hwdec_driver ra_hwdec_dxva2dxgi;
extern const struct ra_hwdec_driver ra_hwdec_cuda;
extern const struct ra_hwdec_driver ra_hwdec_drmprime;
extern const struct ra_hwdec_driver ra_hwdec_drmprime_overlay;
extern const struct ra_hwdec_driver ra_hwdec_aimagereader;
extern const struct ra_hwdec_driver ra_hwdec_vulkan;

const struct ra_hwdec_driver *const ra_hwdec_drivers[] = {
#if HAVE_VAAPI
    &ra_hwdec_vaapi,
#endif
#if HAVE_VIDEOTOOLBOX_GL || HAVE_IOS_GL || HAVE_VIDEOTOOLBOX_PL
    &ra_hwdec_videotoolbox,
#endif
#if HAVE_D3D_HWACCEL
 #if HAVE_EGL_ANGLE
    &ra_hwdec_d3d11egl,
  #if HAVE_D3D9_HWACCEL
    &ra_hwdec_dxva2egl,
  #endif
 #endif
 #if HAVE_D3D11
    &ra_hwdec_d3d11va,
  #if HAVE_D3D9_HWACCEL
    &ra_hwdec_dxva2dxgi,
  #endif
 #endif
#endif
#if HAVE_GL_DXINTEROP_D3D9
    &ra_hwdec_dxva2gldx,
#endif
#if HAVE_CUDA_INTEROP
    &ra_hwdec_cuda,
#endif
#if HAVE_VDPAU_GL_X11
    &ra_hwdec_vdpau,
#endif
#if HAVE_DRM
    &ra_hwdec_drmprime,
    &ra_hwdec_drmprime_overlay,
#endif
#if HAVE_ANDROID_MEDIA_NDK
    &ra_hwdec_aimagereader,
#endif
#if HAVE_VULKAN_INTEROP
    &ra_hwdec_vulkan,
#endif

    NULL
};

struct ra_hwdec *ra_hwdec_load_driver(struct ra_ctx *ra_ctx,
                                      struct mp_log *log,
                                      struct mpv_global *global,
                                      struct mp_hwdec_devices *devs,
                                      const struct ra_hwdec_driver *drv,
                                      bool is_auto)
{
    struct ra_hwdec *hwdec = talloc(NULL, struct ra_hwdec);
    *hwdec = (struct ra_hwdec) {
        .driver = drv,
        .log = mp_log_new(hwdec, log, drv->name),
        .global = global,
        .ra_ctx = ra_ctx,
        .devs = devs,
        .probing = is_auto,
        .priv = talloc_zero_size(hwdec, drv->priv_size),
    };
    mp_verbose(log, "Loading hwdec driver '%s'\n", drv->name);
    if (hwdec->driver->init(hwdec) < 0) {
        ra_hwdec_uninit(hwdec);
        mp_verbose(log, "Loading failed.\n");
        return NULL;
    }
    return hwdec;
}

void ra_hwdec_uninit(struct ra_hwdec *hwdec)
{
    if (hwdec)
        hwdec->driver->uninit(hwdec);
    talloc_free(hwdec);
}

bool ra_hwdec_test_format(struct ra_hwdec *hwdec, int imgfmt)
{
    for (int n = 0; hwdec->driver->imgfmts[n]; n++) {
        if (hwdec->driver->imgfmts[n] == imgfmt)
            return true;
    }
    return false;
}

struct ra_hwdec_mapper *ra_hwdec_mapper_create(struct ra_hwdec *hwdec,
                                               const struct mp_image_params *params)
{
    assert(ra_hwdec_test_format(hwdec, params->imgfmt));

    struct ra_hwdec_mapper *mapper = talloc_ptrtype(NULL, mapper);
    *mapper = (struct ra_hwdec_mapper){
        .owner = hwdec,
        .driver = hwdec->driver->mapper,
        .log = hwdec->log,
        .ra = hwdec->ra_ctx->ra,
        .priv = talloc_zero_size(mapper, hwdec->driver->mapper->priv_size),
        .src_params = *params,
        .dst_params = *params,
    };
    if (mapper->driver->init(mapper) < 0)
        ra_hwdec_mapper_free(&mapper);
    return mapper;
}

void ra_hwdec_mapper_free(struct ra_hwdec_mapper **mapper)
{
    struct ra_hwdec_mapper *p = *mapper;
    if (p) {
        ra_hwdec_mapper_unmap(p);
        p->driver->uninit(p);
        talloc_free(p);
    }
    *mapper = NULL;
}

void ra_hwdec_mapper_unmap(struct ra_hwdec_mapper *mapper)
{
    if (mapper->driver->unmap)
        mapper->driver->unmap(mapper);

    // Clean up after the image if the mapper didn't already
    mp_image_unrefp(&mapper->src);
}

int ra_hwdec_mapper_map(struct ra_hwdec_mapper *mapper, struct mp_image *img)
{
    ra_hwdec_mapper_unmap(mapper);
    mp_image_setrefp(&mapper->src, img);
    if (mapper->driver->map(mapper) < 0) {
        ra_hwdec_mapper_unmap(mapper);
        return -1;
    }
    return 0;
}

static int ra_hwdec_validate_opt_full(struct mp_log *log, bool include_modes,
                                      mp_unused const m_option_t *opt,
                                      mp_unused struct bstr name, const char **value)
{
    struct bstr param = bstr0(*value);
    bool help = bstr_equals0(param, "help");
    if (help)
        mp_info(log, "Available hwdecs:\n");
    for (int n = 0; ra_hwdec_drivers[n]; n++) {
        const struct ra_hwdec_driver *drv = ra_hwdec_drivers[n];
        if (help) {
            mp_info(log, "    %s\n", drv->name);
        } else if (bstr_equals0(param, drv->name)) {
            return 1;
        }
    }
    if (help) {
        if (include_modes) {
            mp_info(log, "    auto (behavior depends on context)\n"
                        "    all (load all hwdecs)\n"
                        "    no (do not load any and block loading on demand)\n");
        }
        return M_OPT_EXIT;
    }
    if (!param.len)
        return 1; // "" is treated specially
    if (include_modes &&
       (bstr_equals0(param, "all") || bstr_equals0(param, "auto") ||
        bstr_equals0(param, "no")))
        return 1;
    mp_fatal(log, "No hwdec backend named '%.*s' found!\n", BSTR_P(param));
    return M_OPT_INVALID;
}

int ra_hwdec_validate_opt(struct mp_log *log, const m_option_t *opt,
                          struct bstr name, const char **value)
{
    return ra_hwdec_validate_opt_full(log, true, opt, name, value);
}

int ra_hwdec_validate_drivers_only_opt(struct mp_log *log,
                                       const m_option_t *opt,
                                       struct bstr name, const char **value)
{
    return ra_hwdec_validate_opt_full(log, false, opt, name, value);
}

static void load_add_hwdec(struct ra_hwdec_ctx *ctx, struct mp_hwdec_devices *devs,
                           const struct ra_hwdec_driver *drv, bool is_auto)
{
    // Don't load duplicate hwdecs
    for (int j = 0; j < ctx->num_hwdecs; j++) {
        if (ctx->hwdecs[j]->driver == drv)
            return;
    }

    struct ra_hwdec *hwdec =
        ra_hwdec_load_driver(ctx->ra_ctx, ctx->log, ctx->global, devs, drv, is_auto);
    if (hwdec)
        MP_TARRAY_APPEND(NULL, ctx->hwdecs, ctx->num_hwdecs, hwdec);
}

static void load_hwdecs_all(struct ra_hwdec_ctx *ctx, struct mp_hwdec_devices *devs)
{
    if (!ctx->loading_done) {
        for (int n = 0; ra_hwdec_drivers[n]; n++)
            load_add_hwdec(ctx, devs, ra_hwdec_drivers[n], true);
        ctx->loading_done = true;
    }
}

void ra_hwdec_ctx_init(struct ra_hwdec_ctx *ctx, struct mp_hwdec_devices *devs,
                       const char *type, bool load_all_by_default)
{
    assert(ctx->ra_ctx);

    /*
     * By default, or if the option value is "auto", we will not pre-emptively
     * load any interops, and instead allow them to be loaded on-demand.
     *
     * If the option value is "no", then no interops will be loaded now, and
     * no interops will be loaded, even if requested later.
     *
     * If the option value is "all", then all interops will be loaded now, and
     * obviously no interops will need to be loaded later.
     *
     * Finally, if a specific interop is requested, it will be loaded now, and
     * other interops can be loaded, if requested later.
     */
    if (!type || !type[0] || strcmp(type, "auto") == 0) {
        if (!load_all_by_default)
            return;
        type = "all";
    }
    if (strcmp(type, "no") == 0) {
        // do nothing, just block further loading
    } else if (strcmp(type, "all") == 0) {
        load_hwdecs_all(ctx, devs);
    } else {
        for (int n = 0; ra_hwdec_drivers[n]; n++) {
            const struct ra_hwdec_driver *drv = ra_hwdec_drivers[n];
            if (strcmp(type, drv->name) == 0) {
                load_add_hwdec(ctx, devs, drv, false);
                break;
            }
        }
    }
    ctx->loading_done = true;
}

void ra_hwdec_ctx_uninit(struct ra_hwdec_ctx *ctx)
{
    for (int n = 0; n < ctx->num_hwdecs; n++)
        ra_hwdec_uninit(ctx->hwdecs[n]);

    talloc_free(ctx->hwdecs);
    memset(ctx, 0, sizeof(*ctx));
}

void ra_hwdec_ctx_load_fmt(struct ra_hwdec_ctx *ctx, struct mp_hwdec_devices *devs,
                           struct hwdec_imgfmt_request *params)
{
    int imgfmt = params->imgfmt;
    if (ctx->loading_done) {
        /*
         * If we previously marked interop loading as done (for reasons
         * discussed above), then do not load any other interops regardless
         * of imgfmt.
         */
        return;
    }

    if (imgfmt == IMGFMT_NONE) {
        MP_VERBOSE(ctx, "Loading hwdec drivers for all formats\n");
        load_hwdecs_all(ctx, devs);
        return;
    }

    MP_VERBOSE(ctx, "Loading hwdec drivers for format: '%s'\n",
               mp_imgfmt_to_name(imgfmt));
    for (int i = 0; ra_hwdec_drivers[i]; i++) {
        bool matched_fmt = false;
        const struct ra_hwdec_driver *drv = ra_hwdec_drivers[i];
        for (int j = 0; drv->imgfmts[j]; j++) {
            if (imgfmt == drv->imgfmts[j]) {
                matched_fmt = true;
                break;
            }
        }
        if (!matched_fmt) {
            continue;
        }

        load_add_hwdec(ctx, devs, drv, params->probing);
    }
}

struct ra_hwdec *ra_hwdec_get(struct ra_hwdec_ctx *ctx, int imgfmt)
{
    for (int n = 0; n < ctx->num_hwdecs; n++) {
        if (ra_hwdec_test_format(ctx->hwdecs[n], imgfmt))
            return ctx->hwdecs[n];
    }

    return NULL;
}

int ra_hwdec_driver_get_imgfmt_for_name(const char *name)
{
    for (int i = 0; ra_hwdec_drivers[i]; i++) {
        if (!strcmp(ra_hwdec_drivers[i]->name, name)) {
            return ra_hwdec_drivers[i]->imgfmts[0];
        }
    }
    return IMGFMT_NONE;
}

enum AVHWDeviceType ra_hwdec_driver_get_device_type_for_name(const char *name)
{
    for (int i = 0; ra_hwdec_drivers[i]; i++) {
        if (!strcmp(ra_hwdec_drivers[i]->name, name)) {
            return ra_hwdec_drivers[i]->device_type;
        }
    }
    return AV_HWDEVICE_TYPE_NONE;
}
