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

#include "wayland_common.h"
#include "gl_common.h"

static void egl_resize(struct vo_wayland_state *wl)
{
    int32_t x = wl->window.sh_x;
    int32_t y = wl->window.sh_y;
    int32_t width = wl->window.sh_width;
    int32_t height = wl->window.sh_height;

    if (!wl->egl_context.egl_window)
        return;

    // get the real size of the window
    // this improves moving the window while resizing it
    wl_egl_window_get_attached_size(wl->egl_context.egl_window,
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

    wl_egl_window_resize(wl->egl_context.egl_window, width, height, x, y);

    wl->window.width = width;
    wl->window.height = height;

    /* set size for mplayer */
    wl->vo->dwidth = wl->window.width;
    wl->vo->dheight = wl->window.height;

    wl->vo->want_redraw = true;
    wl->window.events = 0;
}

static bool egl_create_context(struct vo_wayland_state *wl,
                               MPGLContext *ctx,
                               bool enable_alpha)
{
    EGLint major, minor, n;

    GL *gl = ctx->gl;
    const char *eglstr = "";

    if (!(wl->egl_context.egl.dpy = eglGetDisplay(wl->display.display)))
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
    if (eglInitialize(wl->egl_context.egl.dpy, &major, &minor) != EGL_TRUE)
        return false;

    MP_VERBOSE(wl, "EGL version %d.%d\n", major, minor);

    EGLint context_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION_KHR,
        MPGL_VER_GET_MAJOR(ctx->requested_gl_version),
        EGL_NONE
    };

    if (eglBindAPI(EGL_OPENGL_API) != EGL_TRUE)
        return false;

    eglChooseConfig(wl->egl_context.egl.dpy, config_attribs,
                    &wl->egl_context.egl.conf, 1, &n);

    wl->egl_context.egl.ctx = eglCreateContext(wl->egl_context.egl.dpy,
                                               wl->egl_context.egl.conf,
                                               EGL_NO_CONTEXT,
                                               context_attribs);
    if (!wl->egl_context.egl.ctx) {
        /* fallback to any GL version */
        MP_WARN(wl, "can't create context for requested OpenGL version: "
                    "fall back to any version available\n");
        context_attribs[0] = EGL_NONE;
        wl->egl_context.egl.ctx = eglCreateContext(wl->egl_context.egl.dpy,
                                                   wl->egl_context.egl.conf,
                                                   EGL_NO_CONTEXT,
                                                   context_attribs);

        if (!wl->egl_context.egl.ctx)
            return false;
    }

    eglMakeCurrent(wl->egl_context.egl.dpy, NULL, NULL, wl->egl_context.egl.ctx);

    eglstr = eglQueryString(wl->egl_context.egl.dpy, EGL_EXTENSIONS);

    mpgl_load_functions(gl, (void*(*)(const GLubyte*))eglGetProcAddress, eglstr,
                        wl->log);

    return true;
}

static void egl_create_window(struct vo_wayland_state *wl)
{
    wl->egl_context.egl_window = wl_egl_window_create(wl->window.video_surface,
                                                      wl->window.width,
                                                      wl->window.height);

    wl->egl_context.egl_surface = eglCreateWindowSurface(wl->egl_context.egl.dpy,
                                                         wl->egl_context.egl.conf,
                                                         wl->egl_context.egl_window,
                                                         NULL);

    eglMakeCurrent(wl->egl_context.egl.dpy,
                   wl->egl_context.egl_surface,
                   wl->egl_context.egl_surface,
                   wl->egl_context.egl.ctx);

    wl_display_dispatch_pending(wl->display.display);
}

static bool config_window_wayland(struct MPGLContext *ctx, int flags)
{
    struct vo_wayland_state * wl = ctx->vo->wayland;
    bool enable_alpha = !!(flags & VOFLAG_ALPHA);
    bool ret = false;

    if (!vo_wayland_config(ctx->vo, flags))
        return false;

    if (!wl->egl_context.egl.ctx) {
        /* Create OpenGL context */
        ret = egl_create_context(wl, ctx, enable_alpha);

        /* If successfully created the context and we don't want to hide the
         * window than also create the window immediately */
        if (ret && !(VOFLAG_HIDDEN & flags))
            egl_create_window(wl);

        return ret;
    }
    else {
        if (!wl->egl_context.egl_window) {
            /* If the context exists and the hidden flag is unset then
             * create the window */
            if (!(VOFLAG_HIDDEN & flags))
                egl_create_window(wl);
        }
        return true;
    }
}

static void releaseGlContext_wayland(MPGLContext *ctx)
{
    struct vo_wayland_state *wl = ctx->vo->wayland;

    if (wl->egl_context.egl.ctx) {
        eglReleaseThread();
        wl_egl_window_destroy(wl->egl_context.egl_window);
        eglDestroySurface(wl->egl_context.egl.dpy, wl->egl_context.egl_surface);
        eglMakeCurrent(wl->egl_context.egl.dpy, NULL, NULL, EGL_NO_CONTEXT);
        eglDestroyContext(wl->egl_context.egl.dpy, wl->egl_context.egl.ctx);
    }
    eglTerminate(wl->egl_context.egl.dpy);
    wl->egl_context.egl.ctx = NULL;
}

static void swapGlBuffers_wayland(MPGLContext *ctx)
{
    struct vo_wayland_state *wl = ctx->vo->wayland;

    if (!wl->frame.pending)
        return;

    eglSwapBuffers(wl->egl_context.egl.dpy, wl->egl_context.egl_surface);
    wl->frame.pending = false;
}

static int control(struct vo *vo, int *events, int request, void *data)
{
    struct vo_wayland_state *wl = vo->wayland;
    int r = vo_wayland_control(vo, events, request, data);

    if (*events & VO_EVENT_RESIZE)
        egl_resize(wl);

    return r;
}

static bool start_frame(struct MPGLContext *ctx)
{
    struct vo_wayland_state *wl = ctx->vo->wayland;
    return wl->frame.pending;
}

void mpgl_set_backend_wayland(MPGLContext *ctx)
{
    ctx->config_window = config_window_wayland;
    ctx->releaseGlContext = releaseGlContext_wayland;
    ctx->swapGlBuffers = swapGlBuffers_wayland;
    ctx->vo_control = control;
    ctx->vo_init = vo_wayland_init;
    ctx->vo_uninit = vo_wayland_uninit;
    ctx->start_frame = start_frame;
}
