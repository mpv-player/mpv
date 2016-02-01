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
#include "hwdec.h"

extern const struct gl_hwdec_driver gl_hwdec_vaegl;
extern const struct gl_hwdec_driver gl_hwdec_vaglx;
extern const struct gl_hwdec_driver gl_hwdec_videotoolbox;
extern const struct gl_hwdec_driver gl_hwdec_vdpau;
extern const struct gl_hwdec_driver gl_hwdec_dxva2;

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
#if HAVE_DXVA2_HWACCEL
    &gl_hwdec_dxva2,
#endif
    NULL
};

static struct gl_hwdec *load_hwdec_driver(struct mp_log *log, GL *gl,
                                          struct mpv_global *global,
                                          const struct gl_hwdec_driver *drv,
                                          bool is_auto)
{
    struct gl_hwdec *hwdec = talloc(NULL, struct gl_hwdec);
    *hwdec = (struct gl_hwdec) {
        .driver = drv,
        .log = mp_log_new(hwdec, log, drv->name),
        .global = global,
        .gl = gl,
        .gl_texture_target = GL_TEXTURE_2D,
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

struct gl_hwdec *gl_hwdec_load_api_id(struct mp_log *log, GL *gl,
                                      struct mpv_global *g, int id)
{
    bool is_auto = id == HWDEC_AUTO;
    for (int n = 0; mpgl_hwdec_drivers[n]; n++) {
        const struct gl_hwdec_driver *drv = mpgl_hwdec_drivers[n];
        if (is_auto || id == drv->api) {
            struct gl_hwdec *r = load_hwdec_driver(log, gl, g, drv, is_auto);
            if (r)
                return r;
        }
    }
    return NULL;
}

// Like gl_hwdec_load_api_id(), but use option names.
struct gl_hwdec *gl_hwdec_load_api(struct mp_log *log, GL *gl,
                                   struct mpv_global *g, const char *api_name)
{
    int id = HWDEC_NONE;
    for (const struct m_opt_choice_alternatives *c = mp_hwdec_names; c->name; c++)
    {
        if (strcmp(c->name, api_name) == 0)
            id = c->value;
    }
    return gl_hwdec_load_api_id(log, gl, g, id);
}

void gl_hwdec_uninit(struct gl_hwdec *hwdec)
{
    if (hwdec)
        hwdec->driver->destroy(hwdec);
    talloc_free(hwdec);
}
