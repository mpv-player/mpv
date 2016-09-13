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
#define EGL_CONTEXT_OPENGL_PROFILE_MASK         0x30FD
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT     0x00000001
#endif

static bool create_context(EGLDisplay display, struct mp_log *log, int msgl,
                           int vo_flags,
                           EGLContext *out_context, EGLConfig *out_config)
{
    bool es = vo_flags & VOFLAG_GLES;

    mp_msg(log, MSGL_V, "Trying to create %s context.\n", es ? "GLES" : "GL");

    if (!eglBindAPI(es ? EGL_OPENGL_ES_API : EGL_OPENGL_API)) {
        mp_msg(log, msgl, "Could not bind API!\n");
        return false;
    }

    EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_ALPHA_SIZE, (vo_flags & VOFLAG_ALPHA ) ? 1 : 0,
        EGL_DEPTH_SIZE, 0,
        EGL_RENDERABLE_TYPE, es ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT,
        EGL_NONE
    };

    EGLint config_count;
    EGLConfig config;

    eglChooseConfig(display, attributes, &config, 1, &config_count);

    if (!config_count) {
        mp_msg(log, msgl, "Could not find EGL configuration!\n");
        return false;
    }

    EGLint context_attributes[] = {
        // aka EGL_CONTEXT_MAJOR_VERSION_KHR
        EGL_CONTEXT_CLIENT_VERSION, es ? 2 : 3,
        EGL_NONE, EGL_NONE,
        EGL_NONE
    };

    if (!es) {
        context_attributes[2] = EGL_CONTEXT_OPENGL_PROFILE_MASK;
        context_attributes[3] = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;
    }

    EGLContext *ctx = eglCreateContext(display, config,
                                       EGL_NO_CONTEXT, context_attributes);

    if (!ctx) {
        mp_msg(log, msgl, "Could not create EGL context!\n");
        return false;
    }

    *out_context = ctx;
    *out_config = config;
    return true;
}

// Create a context and return it and the config it was created with. If it
// returns false, the out_* pointers are set to NULL.
// vo_flags is a combination of VOFLAG_* values.
bool mpegl_create_context(EGLDisplay display, struct mp_log *log, int vo_flags,
                          EGLContext *out_context, EGLConfig *out_config)
{
    *out_context = NULL;
    *out_config = NULL;

    int clean_flags = vo_flags & ~(unsigned)(VOFLAG_GLES | VOFLAG_NO_GLES);
    int msgl = vo_flags & VOFLAG_PROBING ? MSGL_V : MSGL_FATAL;

    if (!(vo_flags & VOFLAG_GLES)) {
        if (create_context(display, log, msgl, clean_flags,
                           out_context, out_config))
            return true;
    }

    if (!(vo_flags & VOFLAG_NO_GLES)) {
        if (create_context(display, log, msgl, clean_flags | VOFLAG_GLES,
                           out_context, out_config))
            return true;
    }

    mp_msg(log, msgl, "Could not create a GL context.\n");
    return false;
}

