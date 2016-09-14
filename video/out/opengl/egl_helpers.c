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

#include "common/common.h"

#include "egl_helpers.h"
#include "common.h"
#include "context.h"

// EGL 1.5
#ifndef EGL_CONTEXT_OPENGL_PROFILE_MASK
#define EGL_CONTEXT_MAJOR_VERSION               0x3098
#define EGL_CONTEXT_MINOR_VERSION               0x30FB
#define EGL_CONTEXT_OPENGL_PROFILE_MASK         0x30FD
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT     0x00000001
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE   0x31B1
#define EGL_OPENGL_ES3_BIT                      0x00000040
#endif

static bool create_context(EGLDisplay display, struct mp_log *log, int msgl,
                           int vo_flags, bool es3,
                           EGLContext *out_context, EGLConfig *out_config)
{
    bool es = vo_flags & VOFLAG_GLES;


    EGLenum api = EGL_OPENGL_API;
    EGLint rend = EGL_OPENGL_BIT;
    const char *name = "Desktop OpenGL";
    if (es) {
        api = EGL_OPENGL_ES_API;
        rend = EGL_OPENGL_ES2_BIT;
        name = "GLES 2.0";
    }
    if (es3) {
        api = EGL_OPENGL_ES_API;
        rend = EGL_OPENGL_ES3_BIT;
        name = "GLES 3.x";
    }

    mp_msg(log, MSGL_V, "Trying to create %s context.\n", name);

    if (!eglBindAPI(api)) {
        mp_msg(log, MSGL_V, "Could not bind API!\n");
        return false;
    }


    EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_ALPHA_SIZE, (vo_flags & VOFLAG_ALPHA ) ? 1 : 0,
        EGL_DEPTH_SIZE, 0,
        EGL_RENDERABLE_TYPE, rend,
        EGL_NONE
    };

    EGLint config_count;
    EGLConfig config;

    eglChooseConfig(display, attributes, &config, 1, &config_count);

    if (!config_count) {
        mp_msg(log, msgl, "Could not find EGL configuration!\n");
        return false;
    }

    EGLContext *ctx = NULL;

    if (es) {
        EGLint attrs[] = {
            EGL_CONTEXT_CLIENT_VERSION, es3 ? 3 : 2,
            EGL_NONE
        };

        ctx = eglCreateContext(display, config, EGL_NO_CONTEXT, attrs);
    } else {
        for (int n = 0; mpgl_preferred_gl_versions[n]; n++) {
            int ver = mpgl_preferred_gl_versions[n];

            EGLint attrs[] = {
                EGL_CONTEXT_MAJOR_VERSION, MPGL_VER_GET_MAJOR(ver),
                EGL_CONTEXT_MINOR_VERSION, MPGL_VER_GET_MINOR(ver),
                EGL_CONTEXT_OPENGL_PROFILE_MASK,
                    ver >= 320 ? EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT : 0,
                EGL_NONE
            };

            ctx = eglCreateContext(display, config, EGL_NO_CONTEXT, attrs);
            if (ctx)
                break;
        }

        if (!ctx) {
            // Fallback for EGL 1.4 without EGL_KHR_create_context.
            EGLint attrs[] = { EGL_NONE };

            ctx = eglCreateContext(display, config, EGL_NO_CONTEXT, attrs);
        }
    }

    if (!ctx) {
        mp_msg(log, msgl, "Could not create EGL context!\n");
        return false;
    }

    *out_context = ctx;
    *out_config = config;
    return true;
}

#define STR_OR_ERR(s) ((s) ? (s) : "(error)")

// Create a context and return it and the config it was created with. If it
// returns false, the out_* pointers are set to NULL.
// vo_flags is a combination of VOFLAG_* values.
bool mpegl_create_context(EGLDisplay display, struct mp_log *log, int vo_flags,
                          EGLContext *out_context, EGLConfig *out_config)
{
    *out_context = NULL;
    *out_config = NULL;

    const char *version = eglQueryString(display, EGL_VERSION);
    const char *vendor = eglQueryString(display, EGL_VENDOR);
    const char *apis = eglQueryString(display, EGL_CLIENT_APIS);
    mp_verbose(log, "EGL_VERSION=%s\nEGL_VENDOR=%s\nEGL_CLIENT_APIS=%s\n",
               STR_OR_ERR(version), STR_OR_ERR(vendor), STR_OR_ERR(apis));

    int clean_flags = vo_flags & ~(unsigned)(VOFLAG_GLES | VOFLAG_NO_GLES);
    int msgl = vo_flags & VOFLAG_PROBING ? MSGL_V : MSGL_FATAL;

    if (!(vo_flags & VOFLAG_GLES)) {
        // Desktop OpenGL
        if (create_context(display, log, msgl, clean_flags, false,
                           out_context, out_config))
            return true;
    }

    if (!(vo_flags & VOFLAG_NO_GLES)) {
        // ES 3.x
        if (create_context(display, log, msgl, clean_flags | VOFLAG_GLES, true,
                           out_context, out_config))
            return true;

        // ES 2.0
        if (create_context(display, log, msgl, clean_flags | VOFLAG_GLES, false,
                           out_context, out_config))
            return true;
    }

    mp_msg(log, msgl, "Could not create a GL context.\n");
    return false;
}

