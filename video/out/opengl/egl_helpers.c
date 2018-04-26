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

#include "config.h"

#if HAVE_LIBDL
#include <dlfcn.h>
#endif

#include "common/common.h"

#include "egl_helpers.h"
#include "common.h"
#include "utils.h"
#include "context.h"

#if HAVE_EGL_ANGLE
// On Windows, egl_helpers.c is only used by ANGLE, where the EGL functions may
// be loaded dynamically from ANGLE DLLs
#include "angle_dynamic.h"
#endif

// EGL 1.5
#ifndef EGL_CONTEXT_OPENGL_PROFILE_MASK
#define EGL_CONTEXT_MAJOR_VERSION               0x3098
#define EGL_CONTEXT_MINOR_VERSION               0x30FB
#define EGL_CONTEXT_OPENGL_PROFILE_MASK         0x30FD
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT     0x00000001
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE   0x31B1
#define EGL_OPENGL_ES3_BIT                      0x00000040
#endif

struct mp_egl_config_attr {
    int attrib;
    const char *name;
};

#define MP_EGL_ATTRIB(id) {id, # id}

static const struct mp_egl_config_attr mp_egl_attribs[] = {
    MP_EGL_ATTRIB(EGL_CONFIG_ID),
    MP_EGL_ATTRIB(EGL_RED_SIZE),
    MP_EGL_ATTRIB(EGL_GREEN_SIZE),
    MP_EGL_ATTRIB(EGL_BLUE_SIZE),
    MP_EGL_ATTRIB(EGL_ALPHA_SIZE),
    MP_EGL_ATTRIB(EGL_COLOR_BUFFER_TYPE),
    MP_EGL_ATTRIB(EGL_CONFIG_CAVEAT),
    MP_EGL_ATTRIB(EGL_CONFORMANT),
};

static void dump_egl_config(struct mp_log *log, int msgl, EGLDisplay display,
                            EGLConfig config)
{
    for (int n = 0; n < MP_ARRAY_SIZE(mp_egl_attribs); n++) {
        const char *name = mp_egl_attribs[n].name;
        EGLint v = -1;
        if (eglGetConfigAttrib(display, config, mp_egl_attribs[n].attrib, &v)) {
            mp_msg(log, msgl, "  %s=%d\n", name, v);
        } else {
            mp_msg(log, msgl, "  %s=<error>\n", name);
        }
    }
}

// es_version: 0 (core), 2 or 3
static bool create_context(struct ra_ctx *ctx, EGLDisplay display,
                           int es_version, struct mpegl_cb cb,
                           EGLContext *out_context, EGLConfig *out_config)
{
    int msgl = ctx->opts.probing ? MSGL_V : MSGL_FATAL;

    EGLenum api;
    EGLint rend;
    const char *name;

    switch (es_version) {
    case 0:
        api = EGL_OPENGL_API;
        rend = EGL_OPENGL_BIT;
        name = "Desktop OpenGL";
        break;
    case 2:
        api = EGL_OPENGL_ES_API;
        rend = EGL_OPENGL_ES2_BIT;
        name = "GLES 2.x";
        break;
    case 3:
        api = EGL_OPENGL_ES_API;
        rend = EGL_OPENGL_ES3_BIT;
        name = "GLES 3.x";
        break;
    default: abort();
    }

    MP_VERBOSE(ctx, "Trying to create %s context.\n", name);

    if (!eglBindAPI(api)) {
        MP_VERBOSE(ctx, "Could not bind API!\n");
        return false;
    }

    EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, ctx->opts.want_alpha ? 1 : 0,
        EGL_RENDERABLE_TYPE, rend,
        EGL_NONE
    };

    EGLint num_configs;
    if (!eglChooseConfig(display, attributes, NULL, 0, &num_configs))
        num_configs = 0;

    EGLConfig *configs = talloc_array(NULL, EGLConfig, num_configs);
    if (!eglChooseConfig(display, attributes, configs, num_configs, &num_configs))
        num_configs = 0;

    if (!num_configs) {
        talloc_free(configs);
        MP_MSG(ctx, msgl, "Could not choose EGLConfig for %s!\n", name);
        return false;
    }

    for (int n = 0; n < num_configs; n++)
        dump_egl_config(ctx->log, MSGL_TRACE, display, configs[n]);

    int chosen = 0;
    if (cb.refine_config)
        chosen = cb.refine_config(cb.user_data, configs, num_configs);
    if (chosen < 0) {
        talloc_free(configs);
        MP_MSG(ctx, msgl, "Could not refine EGLConfig for %s!\n", name);
        return false;
    }
    EGLConfig config = configs[chosen];

    talloc_free(configs);

    MP_DBG(ctx, "Chosen EGLConfig:\n");
    dump_egl_config(ctx->log, MSGL_DEBUG, display, config);

    EGLContext *egl_ctx = NULL;

    if (es_version) {
        if (!ra_gl_ctx_test_version(ctx, MPGL_VER(es_version, 0), true))
            return false;

        EGLint attrs[] = {
            EGL_CONTEXT_CLIENT_VERSION, es_version,
            EGL_NONE
        };

        egl_ctx = eglCreateContext(display, config, EGL_NO_CONTEXT, attrs);
    } else {
        for (int n = 0; mpgl_preferred_gl_versions[n]; n++) {
            int ver = mpgl_preferred_gl_versions[n];
            if (!ra_gl_ctx_test_version(ctx, ver, false))
                continue;

            EGLint attrs[] = {
                EGL_CONTEXT_MAJOR_VERSION, MPGL_VER_GET_MAJOR(ver),
                EGL_CONTEXT_MINOR_VERSION, MPGL_VER_GET_MINOR(ver),
                EGL_CONTEXT_OPENGL_PROFILE_MASK,
                    ver >= 320 ? EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT : 0,
                EGL_NONE
            };

            egl_ctx = eglCreateContext(display, config, EGL_NO_CONTEXT, attrs);
            if (egl_ctx)
                break;
        }

        if (!egl_ctx && ra_gl_ctx_test_version(ctx, 140, false)) {
            // Fallback for EGL 1.4 without EGL_KHR_create_context.
            EGLint attrs[] = { EGL_NONE };

            egl_ctx = eglCreateContext(display, config, EGL_NO_CONTEXT, attrs);
        }
    }

    if (!egl_ctx) {
        MP_MSG(ctx, msgl, "Could not create EGL context for %s!\n", name);
        return false;
    }

    *out_context = egl_ctx;
    *out_config = config;
    return true;
}

#define STR_OR_ERR(s) ((s) ? (s) : "(error)")

// Create a context and return it and the config it was created with. If it
// returns false, the out_* pointers are set to NULL.
// vo_flags is a combination of VOFLAG_* values.
bool mpegl_create_context(struct ra_ctx *ctx, EGLDisplay display,
                          EGLContext *out_context, EGLConfig *out_config)
{
    return mpegl_create_context_cb(ctx, display, (struct mpegl_cb){0},
                                   out_context, out_config);
}

// Create a context and return it and the config it was created with. If it
// returns false, the out_* pointers are set to NULL.
bool mpegl_create_context_cb(struct ra_ctx *ctx, EGLDisplay display,
                             struct mpegl_cb cb, EGLContext *out_context,
                             EGLConfig *out_config)
{
    *out_context = NULL;
    *out_config = NULL;

    const char *version = eglQueryString(display, EGL_VERSION);
    const char *vendor = eglQueryString(display, EGL_VENDOR);
    const char *apis = eglQueryString(display, EGL_CLIENT_APIS);
    MP_VERBOSE(ctx, "EGL_VERSION=%s\nEGL_VENDOR=%s\nEGL_CLIENT_APIS=%s\n",
               STR_OR_ERR(version), STR_OR_ERR(vendor), STR_OR_ERR(apis));

    int es[] = {0, 3, 2}; // preference order
    for (int i = 0; i < MP_ARRAY_SIZE(es); i++) {
        if (create_context(ctx, display, es[i], cb, out_context, out_config))
            return true;
    }

    int msgl = ctx->opts.probing ? MSGL_V : MSGL_ERR;
    MP_MSG(ctx, msgl, "Could not create a GL context.\n");
    return false;
}

static int GLAPIENTRY swap_interval(int interval)
{
    EGLDisplay display = eglGetCurrentDisplay();
    if (!display)
        return 1;
    return !eglSwapInterval(display, interval);
}

static void *mpegl_get_proc_address(void *ctx, const char *name)
{
    void *p = eglGetProcAddress(name);
#if defined(__GLIBC__) && HAVE_LIBDL
    // Some crappy ARM/Linux things do not provide EGL 1.5, so above call does
    // not necessarily return function pointers for core functions. Try to get
    // them from a loaded GLES lib. As POSIX leaves RTLD_DEFAULT "reserved",
    // use it only with glibc.
    if (!p)
        p = dlsym(RTLD_DEFAULT, name);
#endif
    return p;
}

// Load gl version and function pointers into *gl.
// Expects a current EGL context set.
void mpegl_load_functions(struct GL *gl, struct mp_log *log)
{
    const char *egl_exts = "";
    EGLDisplay display = eglGetCurrentDisplay();
    if (display != EGL_NO_DISPLAY)
        egl_exts = eglQueryString(display, EGL_EXTENSIONS);

    mpgl_load_functions2(gl, mpegl_get_proc_address, NULL, egl_exts, log);
    if (!gl->SwapInterval)
        gl->SwapInterval = swap_interval;
}
