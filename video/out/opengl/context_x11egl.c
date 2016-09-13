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

#include <assert.h>

#include <X11/Xlib.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifndef EGL_VERSION_1_5
#define EGL_CONTEXT_OPENGL_PROFILE_MASK         0x30FD
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT     0x00000001
#endif

#include "common/common.h"
#include "video/out/x11_common.h"
#include "context.h"
#include "egl_helpers.h"

struct priv {
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
};

static void mpegl_uninit(MPGLContext *ctx)
{
    struct priv *p = ctx->priv;
    if (p->egl_context) {
        eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        eglDestroyContext(p->egl_display, p->egl_context);
    }
    p->egl_context = EGL_NO_CONTEXT;
    vo_x11_uninit(ctx->vo);
}

static int mpegl_init(struct MPGLContext *ctx, int flags)
{
    struct priv *p = ctx->priv;
    struct vo *vo = ctx->vo;
    int msgl = vo->probing ? MSGL_V : MSGL_FATAL;

    if (!vo_x11_init(vo))
        goto uninit;

    p->egl_display = eglGetDisplay(vo->x11->display);
    if (!eglInitialize(p->egl_display, NULL, NULL)) {
        mp_msg(vo->log, msgl, "Could not initialize EGL.\n");
        goto uninit;
    }

    EGLConfig config;
    if (!mpegl_create_context(p->egl_display, vo->log, flags, &p->egl_context,
                              &config))
        goto uninit;

    int vID, n;
    eglGetConfigAttrib(p->egl_display, config, EGL_NATIVE_VISUAL_ID, &vID);
    XVisualInfo template = {.visualid = vID};
    XVisualInfo *vi = XGetVisualInfo(vo->x11->display, VisualIDMask, &template, &n);

    if (!vi) {
        MP_FATAL(vo, "Getting X visual failed!\n");
        goto uninit;
    }

    if (!vo_x11_create_vo_window(vo, vi, "gl")) {
        XFree(vi);
        goto uninit;
    }

    XFree(vi);

    p->egl_surface = eglCreateWindowSurface(p->egl_display, config,
                                    (EGLNativeWindowType)vo->x11->window, NULL);

    if (p->egl_surface == EGL_NO_SURFACE) {
        MP_FATAL(ctx->vo, "Could not create EGL surface!\n");
        goto uninit;
    }

    if (!eglMakeCurrent(p->egl_display, p->egl_surface, p->egl_surface,
                        p->egl_context))
    {
        MP_FATAL(ctx->vo, "Could not make context current!\n");
        goto uninit;
    }

    const char *egl_exts = eglQueryString(p->egl_display, EGL_EXTENSIONS);

    void *(*gpa)(const GLubyte*) = (void *(*)(const GLubyte*))eglGetProcAddress;
    mpgl_load_functions(ctx->gl, gpa, egl_exts, vo->log);

    ctx->native_display_type = "x11";
    ctx->native_display = vo->x11->display;
    return 0;

uninit:
    mpegl_uninit(ctx);
    return -1;
}

static int mpegl_reconfig(struct MPGLContext *ctx)
{
    vo_x11_config_vo_window(ctx->vo);
    return 0;
}

static int mpegl_control(struct MPGLContext *ctx, int *events, int request,
                         void *arg)
{
    return vo_x11_control(ctx->vo, events, request, arg);
}

static void mpegl_swap_buffers(MPGLContext *ctx)
{
    struct priv *p = ctx->priv;
    eglSwapBuffers(p->egl_display, p->egl_surface);
}

static void mpegl_wakeup(struct MPGLContext *ctx)
{
    vo_x11_wakeup(ctx->vo);
}

static void mpegl_wait_events(struct MPGLContext *ctx, int64_t until_time_us)
{
    vo_x11_wait_events(ctx->vo, until_time_us);
}

const struct mpgl_driver mpgl_driver_x11egl = {
    .name           = "x11egl",
    .priv_size      = sizeof(struct priv),
    .init           = mpegl_init,
    .reconfig       = mpegl_reconfig,
    .swap_buffers   = mpegl_swap_buffers,
    .control        = mpegl_control,
    .wakeup         = mpegl_wakeup,
    .wait_events    = mpegl_wait_events,
    .uninit         = mpegl_uninit,
};
