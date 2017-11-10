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

extern const struct ra_hwdec_driver ra_hwdec_vaegl;
extern const struct ra_hwdec_driver ra_hwdec_vaglx;
extern const struct ra_hwdec_driver ra_hwdec_videotoolbox;
extern const struct ra_hwdec_driver ra_hwdec_vdpau;
extern const struct ra_hwdec_driver ra_hwdec_dxva2egl;
extern const struct ra_hwdec_driver ra_hwdec_d3d11egl;
extern const struct ra_hwdec_driver ra_hwdec_d3d11eglrgb;
extern const struct ra_hwdec_driver ra_hwdec_dxva2gldx;
extern const struct ra_hwdec_driver ra_hwdec_dxva2;
extern const struct ra_hwdec_driver ra_hwdec_d3d11va;
extern const struct ra_hwdec_driver ra_hwdec_cuda;
extern const struct ra_hwdec_driver ra_hwdec_cuda_nvdec;
extern const struct ra_hwdec_driver ra_hwdec_rpi_overlay;
extern const struct ra_hwdec_driver ra_hwdec_drmprime_drm;

static const struct ra_hwdec_driver *const mpgl_hwdec_drivers[] = {
#if HAVE_VAAPI_EGL
    &ra_hwdec_vaegl,
#endif
#if HAVE_VAAPI_GLX
    &ra_hwdec_vaglx,
#endif
#if HAVE_VDPAU_GL_X11
    &ra_hwdec_vdpau,
#endif
#if HAVE_VIDEOTOOLBOX_GL || HAVE_IOS_GL
    &ra_hwdec_videotoolbox,
#endif
#if HAVE_D3D_HWACCEL
    &ra_hwdec_d3d11egl,
    &ra_hwdec_d3d11eglrgb,
 #if HAVE_D3D9_HWACCEL
    &ra_hwdec_dxva2egl,
 #endif
 #if HAVE_D3D11
    &ra_hwdec_d3d11va,
 #endif
#endif
#if HAVE_GL_DXINTEROP_D3D9
    &ra_hwdec_dxva2gldx,
#endif
#if HAVE_CUDA_HWACCEL
    &ra_hwdec_cuda,
#endif
#if HAVE_RPI
    &ra_hwdec_rpi_overlay,
#endif
#if HAVE_DRMPRIME && HAVE_DRM
    &ra_hwdec_drmprime_drm,
#endif

    NULL
};

static struct ra_hwdec *load_hwdec_driver(struct mp_log *log, struct ra *ra,
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
        .ra = ra,
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

struct ra_hwdec *ra_hwdec_load_api(struct mp_log *log, struct ra *ra,
                                   struct mpv_global *g,
                                   struct mp_hwdec_devices *devs,
                                   enum hwdec_type api)
{
    bool is_auto = HWDEC_IS_AUTO(api);
    for (int n = 0; mpgl_hwdec_drivers[n]; n++) {
        const struct ra_hwdec_driver *drv = mpgl_hwdec_drivers[n];
        if ((is_auto || api == drv->api) && !drv->testing_only) {
            struct ra_hwdec *r = load_hwdec_driver(log, ra, g, devs, drv, is_auto);
            if (r)
                return r;
        }
    }
    return NULL;
}

// Load by option name.
struct ra_hwdec *ra_hwdec_load(struct mp_log *log, struct ra *ra,
                               struct mpv_global *g,
                               struct mp_hwdec_devices *devs,
                               const char *name)
{
    int g_hwdec_api;
    mp_read_option_raw(g, "hwdec", &m_option_type_choice, &g_hwdec_api);
    if (!name || !name[0])
        name = m_opt_choice_str(mp_hwdec_names, g_hwdec_api);

    int api_id = HWDEC_NONE;
    for (int n = 0; mp_hwdec_names[n].name; n++) {
        if (name && strcmp(mp_hwdec_names[n].name, name) == 0)
            api_id = mp_hwdec_names[n].value;
    }

    for (int n = 0; mpgl_hwdec_drivers[n]; n++) {
        const struct ra_hwdec_driver *drv = mpgl_hwdec_drivers[n];
        if (name && strcmp(drv->name, name) == 0) {
            struct ra_hwdec *r = load_hwdec_driver(log, ra, g, devs, drv, false);
            if (r)
                return r;
        }
    }

    return ra_hwdec_load_api(log, ra, g, devs, api_id);
}

int ra_hwdec_validate_opt(struct mp_log *log, const m_option_t *opt,
                          struct bstr name, struct bstr param)
{
    bool help = bstr_equals0(param, "help");
    if (help)
        mp_info(log, "Available hwdecs:\n");
    for (int n = 0; mpgl_hwdec_drivers[n]; n++) {
        const struct ra_hwdec_driver *drv = mpgl_hwdec_drivers[n];
        const char *api_name = m_opt_choice_str(mp_hwdec_names, drv->api);
        if (help) {
            mp_info(log, "    %s [%s]\n", drv->name, api_name);
        } else if (bstr_equals0(param, drv->name) ||
                   bstr_equals0(param, api_name))
        {
            return 1;
        }
    }
    if (help) {
        mp_info(log, "    auto (loads best)\n"
                     "    (other --hwdec values)\n"
                     "Setting an empty string means use --hwdec.\n");
        return M_OPT_EXIT;
    }
    if (!param.len)
        return 1; // "" is treated specially
    for (int n = 0; mp_hwdec_names[n].name; n++) {
        if (bstr_equals0(param, mp_hwdec_names[n].name))
            return 1;
    }
    mp_fatal(log, "No hwdec backend named '%.*s' found!\n", BSTR_P(param));
    return M_OPT_INVALID;
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
                                               struct mp_image_params *params)
{
    assert(ra_hwdec_test_format(hwdec, params->imgfmt));

    struct ra_hwdec_mapper *mapper = talloc_ptrtype(NULL, mapper);
    *mapper = (struct ra_hwdec_mapper){
        .owner = hwdec,
        .driver = hwdec->driver->mapper,
        .log = hwdec->log,
        .ra = hwdec->ra,
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
