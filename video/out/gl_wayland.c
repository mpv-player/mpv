/*
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * You can alternatively redistribute this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

static void egl_resize_func(struct vo_wayland_state *wl,
                            struct egl_context *ctx)
{
    int32_t x, y, scaled_height;
    double ratio;
    int minimum_size = 50;

    if (wl->window->pending_width < minimum_size)
        wl->window->pending_width = minimum_size;
    if (wl->window->pending_height < minimum_size)
        wl->window->pending_height = minimum_size;

    ratio = (double) wl->vo->aspdat.orgw / wl->vo->aspdat.orgh;
    scaled_height = wl->window->pending_height * ratio;
    if (wl->window->pending_width > scaled_height) {
        wl->window->pending_height = wl->window->pending_width / ratio;
    } else {
        wl->window->pending_width = scaled_height;
    }

    if (wl->window->edges & WL_SHELL_SURFACE_RESIZE_LEFT)
        x = wl->window->width - wl->window->pending_width;
    else
        x = 0;

    if (wl->window->edges & WL_SHELL_SURFACE_RESIZE_TOP)
        y = wl->window->height - wl->window->pending_height;
    else
        y = 0;

    wl_egl_window_resize(ctx->egl_window,
            wl->window->pending_width,
            wl->window->pending_height,
            x, y);

    wl->window->width = wl->window->pending_width;
    wl->window->height = wl->window->pending_height;

    /* set size for mplayer */
    wl->vo->dwidth = wl->window->pending_width;
    wl->vo->dheight = wl->window->pending_height;
    wl->window->events |= VO_EVENT_RESIZE;
    wl->window->edges = 0;
    wl->window->resize_needed = 0;
}

static bool egl_create_context(struct vo_wayland_state *wl,
                               struct egl_context *egl_ctx,
                               MPGLContext *ctx,
                               bool enable_alpha)
{
    EGLint major, minor, n;

    GL *gl = ctx->gl;
    const char *eglstr = "";

    if (!(egl_ctx->egl.dpy = eglGetDisplay(wl->display->display)))
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
    if (!egl_ctx->egl.ctx)
        return false;

    eglMakeCurrent(egl_ctx->egl.dpy, NULL, NULL, egl_ctx->egl.ctx);

    eglstr = eglQueryString(egl_ctx->egl.dpy, EGL_EXTENSIONS);

    mpgl_load_functions(gl, (void*(*)(const GLubyte*))eglGetProcAddress, eglstr);
    if (!gl->BindProgram)
        mpgl_load_functions(gl, NULL, eglstr);

    return true;
}

static void egl_create_window(struct vo_wayland_state *wl,
                              struct egl_context *egl_ctx,
                              uint32_t width,
                              uint32_t height)
{
    egl_ctx->egl_window = wl_egl_window_create(wl->window->surface,
                                               wl->window->width,
                                               wl->window->height);

    egl_ctx->egl_surface = eglCreateWindowSurface(egl_ctx->egl.dpy,
                                                  egl_ctx->egl.conf,
                                                  egl_ctx->egl_window,
                                                  NULL);

    eglMakeCurrent(egl_ctx->egl.dpy,
                   egl_ctx->egl_surface,
                   egl_ctx->egl_surface,
                   egl_ctx->egl.ctx);

    wl_display_dispatch_pending(wl->display->display);
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

    wl->window->pending_width = d_width;
    wl->window->pending_height = d_height;
    wl->window->width = d_width;
    wl->window->height = d_height;

    vo_wayland_update_window_title(ctx->vo);

    if ((VOFLAG_FULLSCREEN & flags) && wl->window->type != TYPE_FULLSCREEN)
        vo_wayland_fullscreen(ctx->vo);

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
        /* If the window exists just resize it */
        if (egl_ctx->egl_window)
            egl_resize_func(wl, egl_ctx);

        else {
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

    /* resize window after the buffers have swapped
     * makes resizing more fluid */
    if (wl->window->resize_needed) {
        wl_egl_window_get_attached_size(egl_ctx->egl_window,
            &wl->window->width,
            &wl->window->height);
        egl_resize_func(wl, egl_ctx);
    }
}

void mpgl_set_backend_wayland(MPGLContext *ctx)
{
    ctx->priv = talloc_zero(ctx, struct egl_context);
    ctx->config_window = config_window_wayland;
    ctx->releaseGlContext = releaseGlContext_wayland;
    ctx->swapGlBuffers = swapGlBuffers_wayland;
    ctx->update_xinerama_info = vo_wayland_update_screeninfo;
    ctx->border = vo_wayland_border;
    ctx->check_events = vo_wayland_check_events;
    ctx->fullscreen = vo_wayland_fullscreen;
    ctx->ontop = vo_wayland_ontop;
    ctx->vo_init = vo_wayland_init;
    ctx->vo_uninit = vo_wayland_uninit;
}
