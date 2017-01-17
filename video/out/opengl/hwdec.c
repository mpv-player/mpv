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

extern const struct gl_hwdec_driver gl_hwdec_vaegl;
extern const struct gl_hwdec_driver gl_hwdec_vaglx;
extern const struct gl_hwdec_driver gl_hwdec_videotoolbox;
extern const struct gl_hwdec_driver gl_hwdec_vdpau;
extern const struct gl_hwdec_driver gl_hwdec_dxva2egl;
extern const struct gl_hwdec_driver gl_hwdec_d3d11egl;
extern const struct gl_hwdec_driver gl_hwdec_d3d11eglrgb;
extern const struct gl_hwdec_driver gl_hwdec_dxva2gldx;
extern const struct gl_hwdec_driver gl_hwdec_dxva2;
extern const struct gl_hwdec_driver gl_hwdec_cuda;
extern const struct gl_hwdec_driver gl_hwdec_rpi_overlay;

static const struct gl_hwdec_driver *const mpgl_hwdec_drivers[] = {
#if HAVE_VAAPI_EGL
    &gl_hwdec_vaegl,
#endif
#if HAVE_VAAPI_GLX
    &gl_hwdec_vaglx,
#endif
#if HAVE_VDPAU_GL_X11
    &gl_hwdec_vdpau,
#endif
#if HAVE_VIDEOTOOLBOX_GL
    &gl_hwdec_videotoolbox,
#endif
#if HAVE_D3D_HWACCEL
#if HAVE_EGL_ANGLE
    &gl_hwdec_d3d11egl,
    &gl_hwdec_d3d11eglrgb,
    &gl_hwdec_dxva2egl,
#endif
#if HAVE_GL_DXINTEROP
    &gl_hwdec_dxva2gldx,
#endif
    &gl_hwdec_dxva2,
#endif
#if HAVE_CUDA_HWACCEL
    &gl_hwdec_cuda,
#endif
#if HAVE_RPI
    &gl_hwdec_rpi_overlay,
#endif
    NULL
};

static struct gl_hwdec *load_hwdec_driver(struct mp_log *log, GL *gl,
                                          struct mpv_global *global,
                                          struct mp_hwdec_devices *devs,
                                          const struct gl_hwdec_driver *drv,
                                          bool is_auto)
{
    struct gl_hwdec *hwdec = talloc(NULL, struct gl_hwdec);
    *hwdec = (struct gl_hwdec) {
        .driver = drv,
        .log = mp_log_new(hwdec, log, drv->name),
        .global = global,
        .gl = gl,
        .devs = devs,
        .probing = is_auto,
    };
    mp_verbose(log, "Loading hwdec driver '%s'\n", drv->name);
    if (hwdec->driver->create(hwdec) < 0) {
        talloc_free(hwdec);
        mp_verbose(log, "Loading failed.\n");
        return NULL;
    }
    return hwdec;
}

struct gl_hwdec *gl_hwdec_load_api(struct mp_log *log, GL *gl,
                                   struct mpv_global *g,
                                   struct mp_hwdec_devices *devs,
                                   enum hwdec_type api)
{
    bool is_auto = HWDEC_IS_AUTO(api);
    for (int n = 0; mpgl_hwdec_drivers[n]; n++) {
        const struct gl_hwdec_driver *drv = mpgl_hwdec_drivers[n];
        if (is_auto || api == drv->api) {
            struct gl_hwdec *r = load_hwdec_driver(log, gl, g, devs, drv, is_auto);
            if (r)
                return r;
        }
    }
    return NULL;
}

// Load by option name.
struct gl_hwdec *gl_hwdec_load(struct mp_log *log, GL *gl,
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
        const struct gl_hwdec_driver *drv = mpgl_hwdec_drivers[n];
        if (name && strcmp(drv->name, name) == 0) {
            struct gl_hwdec *r = load_hwdec_driver(log, gl, g, devs, drv, false);
            if (r)
                return r;
        }
    }

    return gl_hwdec_load_api(log, gl, g, devs, api_id);
}

int gl_hwdec_validate_opt(struct mp_log *log, const m_option_t *opt,
                          struct bstr name, struct bstr param)
{
    bool help = bstr_equals0(param, "help");
    if (help)
        mp_info(log, "Available hwdecs:\n");
    for (int n = 0; mpgl_hwdec_drivers[n]; n++) {
        const struct gl_hwdec_driver *drv = mpgl_hwdec_drivers[n];
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

void gl_hwdec_uninit(struct gl_hwdec *hwdec)
{
    if (hwdec)
        hwdec->driver->destroy(hwdec);
    talloc_free(hwdec);
}

bool gl_hwdec_test_format(struct gl_hwdec *hwdec, int imgfmt)
{
    if (!imgfmt)
        return false;
    if (hwdec->driver->test_format)
        return hwdec->driver->test_format(hwdec, imgfmt);
    return hwdec->driver->imgfmt == imgfmt;
}
