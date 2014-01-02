/*
 * This file is part of mpv video player.
 * Copyright © 2013 Alexander Preisinger <alexander.preisinger@gmail.com>
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
 */

#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "wayland_common.h"
#include "gl_common.h"

struct egl_context {
    EGLSurface egl_surface;

    struct wl_egl_window *egl_window;

    struct {
        EGLDisplay dpy;
        EGLContext ctx;
        EGLConfig conf;
    } egl;
};

static void egl_resize(struct vo_wayland_state *wl,
                       struct egl_context *ctx)
{
    int32_t x = wl->window.sh_x;
    int32_t y = wl->window.sh_y;
    int32_t width = wl->window.sh_width;
    int32_t height = wl->window.sh_height;

    // get the real size of the window
    // this improves moving the window while resizing it
    wl_egl_window_get_attached_size(ctx->egl_window,
                                    &wl->window.width,
                                    &wl->window.height);

    MP_VERBOSE(wl, "resizing %dx%d -> %dx%d\n", wl->window.width,
                                                wl->window.height,
                                                width,
                                                height);

    if (x != 0)
        x = wl->window.width - width;

    if (y != 0)
        y = wl->window.height - height;

    wl_egl_window_resize(ctx->egl_window, width, height, x, y);

    wl->window.width = width;
    wl->window.height = height;

    /* set size for mplayer */
    wl->vo->dwidth = wl->window.width;
    wl->vo->dheight = wl->window.height;

    wl->vo->want_redraw = true;
    wl->window.events = 0;
}

static bool egl_create_context(struct vo_wayland_state *wl,
                               struct egl_context *egl_ctx,
                               MPGLContext *ctx,
                               bool enable_alpha)
{
    EGLint major, minor, n;

    GL *gl = ctx->gl;
    const char *eglstr = "";

    if (!(egl_ctx->egl.dpy = eglGetDisplay(wl->display.display)))
        return false;

    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_ALPHA_SIZE, enable_alpha,
        EGL_DEPTH_SIZE, 1,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };

    /* major and minor here returns the supported EGL version (e.g.: 1.4) */
    if (eglInitialize(egl_ctx->egl.dpy, &major, &minor) != EGL_TRUE)
        return false;

    MP_VERBOSE(wl, "EGL version %d.%d\n", major, minor);

    EGLint context_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION_KHR,
        MPGL_VER_GET_MAJOR(ctx->requested_gl_version),
        /* EGL_CONTEXT_MINOR_VERSION_KHR, */
        /* MPGL_VER_GET_MINOR(ctx->requested_gl_version), */
        /* EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR, 0, */
        /* Segfaults on anything else than the major version */
        EGL_NONE
    };

    if (eglBindAPI(EGL_OPENGL_API) != EGL_TRUE)
        return false;

    eglChooseConfig(egl_ctx->egl.dpy, config_attribs,
                    &egl_ctx->egl.conf, 1, &n);

    egl_ctx->egl.ctx = eglCreateContext(egl_ctx->egl.dpy,
                                        egl_ctx->egl.conf,
                                        EGL_NO_CONTEXT,
                                        context_attribs);
    if (!egl_ctx->egl.ctx) {
        /* fallback to any GL version */
        context_attribs[0] = EGL_NONE;
        egl_ctx->egl.ctx = eglCreateContext(egl_ctx->egl.dpy,
                                            egl_ctx->egl.conf,
                                            EGL_NO_CONTEXT,
                                            context_attribs);

        if (!egl_ctx->egl.ctx)
            return false;
    }

    eglMakeCurrent(egl_ctx->egl.dpy, NULL, NULL, egl_ctx->egl.ctx);

    eglstr = eglQueryString(egl_ctx->egl.dpy, EGL_EXTENSIONS);

    mpgl_load_functions(gl, (void*(*)(const GLubyte*))eglGetProcAddress, eglstr,
                        wl->log);
    if (!gl->BindProgram)
        mpgl_load_functions(gl, NULL, eglstr, wl->log);

    return true;
}

static void egl_create_window(struct vo_wayland_state *wl,
                              struct egl_context *egl_ctx,
                              uint32_t width,
                              uint32_t height)
{
    egl_ctx->egl_window = wl_egl_window_create(wl->window.surface,
                                               wl->window.width,
                                               wl->window.height);

    egl_ctx->egl_surface = eglCreateWindowSurface(egl_ctx->egl.dpy,
                                                  egl_ctx->egl.conf,
                                                  egl_ctx->egl_window,
                                                  NULL);

    eglMakeCurrent(egl_ctx->egl.dpy,
                   egl_ctx->egl_surface,
                   egl_ctx->egl_surface,
                   egl_ctx->egl.ctx);

    wl_display_dispatch_pending(wl->display.display);
}

static bool config_window_wayland(struct MPGLContext *ctx,
                                  uint32_t d_width,
                                  uint32_t d_height,
                                  uint32_t flags)
{
    struct egl_context * egl_ctx = ctx->priv;
    struct vo_wayland_state * wl = ctx->vo->wayland;
    bool enable_alpha = !!(flags & VOFLAG_ALPHA);
    bool ret = false;

    if (!vo_wayland_config(ctx->vo, d_width, d_height, flags))
        return false;

    if (!egl_ctx->egl.ctx) {
        /* Create OpenGL context */
        ret = egl_create_context(wl, egl_ctx, ctx, enable_alpha);

        /* If successfully created the context and we don't want to hide the
         * window than also create the window immediately */
        if (ret && !(VOFLAG_HIDDEN & flags))
            egl_create_window(wl, egl_ctx, d_width, d_height);

        return ret;
    }
    else {
        if (!egl_ctx->egl_window) {
            /* If the context exists and the hidden flag is unset then
             * create the window */
            if (!(VOFLAG_HIDDEN & flags))
                egl_create_window(wl, egl_ctx, d_width, d_height);
        }
        return true;
    }
}

static void releaseGlContext_wayland(MPGLContext *ctx)
{
    GL *gl = ctx->gl;
    struct egl_context * egl_ctx = ctx->priv;

    gl->Finish();
    eglMakeCurrent(egl_ctx->egl.dpy, NULL, NULL, EGL_NO_CONTEXT);
    eglDestroyContext(egl_ctx->egl.dpy, egl_ctx->egl.ctx);
    eglTerminate(egl_ctx->egl.dpy);
    eglReleaseThread();
    wl_egl_window_destroy(egl_ctx->egl_window);
    egl_ctx->egl.ctx = NULL;
}

static void swapGlBuffers_wayland(MPGLContext *ctx)
{
    struct egl_context * egl_ctx = ctx->priv;
    struct vo_wayland_state *wl = ctx->vo->wayland;

    eglSwapBuffers(egl_ctx->egl.dpy, egl_ctx->egl_surface);

    if (wl->window.events & VO_EVENT_RESIZE)
        egl_resize(wl, egl_ctx);
}

void mpgl_set_backend_wayland(MPGLContext *ctx)
{
    ctx->priv = talloc_zero(ctx, struct egl_context);
    ctx->config_window = config_window_wayland;
    ctx->releaseGlContext = releaseGlContext_wayland;
    ctx->swapGlBuffers = swapGlBuffers_wayland;
    ctx->vo_control = vo_wayland_control;
    ctx->vo_init = vo_wayland_init;
    ctx->vo_uninit = vo_wayland_uninit;
}
