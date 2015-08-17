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
 *
 * You can alternatively redistribute this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stddef.h>
#include <string.h>

#include "config.h"

#include "common/common.h"
#include "common/msg.h"
#include "gl_hwdec.h"

extern const struct gl_hwdec_driver gl_hwdec_vaglx;
extern const struct gl_hwdec_driver gl_hwdec_vda;
extern const struct gl_hwdec_driver gl_hwdec_videotoolbox;
extern const struct gl_hwdec_driver gl_hwdec_vdpau;
extern const struct gl_hwdec_driver gl_hwdec_dxva2;

static const struct gl_hwdec_driver *const mpgl_hwdec_drivers[] = {
#if HAVE_VAAPI_GLX
    &gl_hwdec_vaglx,
#endif
#if HAVE_VDPAU_GL_X11
    &gl_hwdec_vdpau,
#endif
#if HAVE_VIDEOTOOLBOX_VDA_GL
    &gl_hwdec_videotoolbox,
#endif
#if HAVE_DXVA2_HWACCEL
    &gl_hwdec_dxva2,
#endif
    NULL
};

static struct gl_hwdec *load_hwdec_driver(struct mp_log *log, GL *gl,
                                          const struct gl_hwdec_driver *drv,
                                          bool is_auto)
{
    struct gl_hwdec *hwdec = talloc(NULL, struct gl_hwdec);
    *hwdec = (struct gl_hwdec) {
        .driver = drv,
        .log = mp_log_new(hwdec, log, drv->api_name),
        .gl = gl,
        .gl_texture_target = GL_TEXTURE_2D,
        .reject_emulated = is_auto,
    };
    mp_verbose(log, "Loading hwdec driver '%s'\n", drv->api_name);
    if (hwdec->driver->create(hwdec) < 0) {
        talloc_free(hwdec);
        mp_verbose(log, "Loading failed.\n");
        return NULL;
    }
    return hwdec;
}

struct gl_hwdec *gl_hwdec_load_api(struct mp_log *log, GL *gl,
                                   const char *api_name)
{
    bool is_auto = api_name && strcmp(api_name, "auto") == 0;
    for (int n = 0; mpgl_hwdec_drivers[n]; n++) {
        const struct gl_hwdec_driver *drv = mpgl_hwdec_drivers[n];
        if (is_auto || (api_name && strcmp(drv->api_name, api_name) == 0)) {
            struct gl_hwdec *r = load_hwdec_driver(log, gl, drv, is_auto);
            if (r)
                return r;
        }
    }
    return NULL;
}

// Like gl_hwdec_load_api(), but use HWDEC_... identifiers.
struct gl_hwdec *gl_hwdec_load_api_id(struct mp_log *log, GL *gl, int id)
{
    return gl_hwdec_load_api(log, gl, m_opt_choice_str(mp_hwdec_names, id));
}

void gl_hwdec_uninit(struct gl_hwdec *hwdec)
{
    if (hwdec)
        hwdec->driver->destroy(hwdec);
    talloc_free(hwdec);
}
