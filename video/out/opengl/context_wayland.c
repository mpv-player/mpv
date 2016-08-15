/*
 * This file is part of mpv video player.
 * Copyright © 2013 Alexander Preisinger <alexander.preisinger@gmail.com>
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

#include "video/out/wayland_common.h"
#include "context.h"

static void egl_resize(struct vo_wayland_state *wl)
{
    int32_t x = wl->window.sh_x;
    int32_t y = wl->window.sh_y;
    int32_t width = wl->window.sh_width;
    int32_t height = wl->window.sh_height;
    int32_t scale = 1;

    if (!wl->egl_context.egl_window)
        return;

    if (wl->display.current_output)
        scale = wl->display.current_output->scale;

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

    wl_surface_set_buffer_scale(wl->window.video_surface, scale);
    wl_egl_window_resize(wl->egl_context.egl_window, scale*width, scale*height, x, y);

    wl->window.width = width;
    wl->window.height = height;

    /* set size for mplayer */
    wl->vo->dwidth  = scale*wl->window.width;
    wl->vo->dheight = scale*wl->window.height;
    wl->vo->want_redraw = true;
}

static int egl_create_context(struct vo_wayland_state *wl,
                              MPGLContext *ctx,
                              bool enable_alpha)
{
    EGLint major, minor, n;

    GL *gl = ctx->gl;
    const char *eglstr = "";

    if (!(wl->egl_context.egl.dpy = eglGetDisplay(wl->display.display)))
        return -1;

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
        return -1;

    MP_VERBOSE(wl, "EGL version %d.%d\n", major, minor);

    EGLint context_attribs[] = {
        // aka EGL_CONTEXT_MAJOR_VERSION_KHR
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    if (eglBindAPI(EGL_OPENGL_API) != EGL_TRUE)
        return -1;

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
            return -1;
    }

    eglMakeCurrent(wl->egl_context.egl.dpy, NULL, NULL, wl->egl_context.egl.ctx);

    eglstr = eglQueryString(wl->egl_context.egl.dpy, EGL_EXTENSIONS);

    mpgl_load_functions(gl, (void*(*)(const GLubyte*))eglGetProcAddress, eglstr,
                        wl->log);

    ctx->native_display_type = "wl";
    ctx->native_display = wl->display.display;

    return 0;
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

    /**
     * <http://lists.freedesktop.org/archives/wayland-devel/2013-November/012019.html>
     *
     * The main change is that if the swap interval is 0 then Mesa won't install a
     * frame callback so that eglSwapBuffers can be executed as often as necessary.
     * Instead it will do a sync request after the swap buffers. It will block for
     * sync complete event in get_back_bo instead of the frame callback. The
     * compositor is likely to send a release event while processing the new buffer
     * attach and this makes sure we will receive that before deciding whether to
     * allocate a new buffer.
     */

    eglSwapInterval(wl->egl_context.egl.dpy, 0);
}

static int waylandgl_reconfig(struct MPGLContext *ctx)
{
    struct vo_wayland_state * wl = ctx->vo->wayland;

    if (!vo_wayland_config(ctx->vo))
        return -1;

    if (!wl->egl_context.egl_window)
        egl_create_window(wl);

    return 0;
}

static void waylandgl_uninit(MPGLContext *ctx)
{
    struct vo_wayland_state *wl = ctx->vo->wayland;

    if (wl->egl_context.egl.ctx) {
        eglReleaseThread();
        if (wl->egl_context.egl_window)
            wl_egl_window_destroy(wl->egl_context.egl_window);
        eglDestroySurface(wl->egl_context.egl.dpy, wl->egl_context.egl_surface);
        eglMakeCurrent(wl->egl_context.egl.dpy, NULL, NULL, EGL_NO_CONTEXT);
        eglDestroyContext(wl->egl_context.egl.dpy, wl->egl_context.egl.ctx);
    }
    eglTerminate(wl->egl_context.egl.dpy);
    wl->egl_context.egl.ctx = NULL;

    vo_wayland_uninit(ctx->vo);
}

static void waylandgl_swap_buffers(MPGLContext *ctx)
{
    struct vo_wayland_state *wl = ctx->vo->wayland;

    if (!wl->frame.callback)
        vo_wayland_request_frame(ctx->vo, NULL, NULL);

    vo_wayland_wait_events(ctx->vo, 0);

    eglSwapBuffers(wl->egl_context.egl.dpy, wl->egl_context.egl_surface);
}

static int waylandgl_control(MPGLContext *ctx, int *events, int request,
                             void *data)
{
    struct vo_wayland_state *wl = ctx->vo->wayland;
    int r = vo_wayland_control(ctx->vo, events, request, data);

    if (*events & VO_EVENT_RESIZE)
        egl_resize(wl);

    return r;
}

static void wayland_wakeup(struct MPGLContext *ctx)
{
    vo_wayland_wakeup(ctx->vo);
}

static void wayland_wait_events(struct MPGLContext *ctx, int64_t until_time_us)
{
    vo_wayland_wait_events(ctx->vo, until_time_us);
}

static int waylandgl_init(struct MPGLContext *ctx, int flags)
{
    if (!vo_wayland_init(ctx->vo))
        return -1;

    return egl_create_context(ctx->vo->wayland, ctx, !!(flags & VOFLAG_ALPHA));
}

const struct mpgl_driver mpgl_driver_wayland = {
    .name           = "wayland",
    .init           = waylandgl_init,
    .reconfig       = waylandgl_reconfig,
    .swap_buffers   = waylandgl_swap_buffers,
    .control        = waylandgl_control,
    .wakeup         = wayland_wakeup,
    .wait_events    = wayland_wait_events,
    .uninit         = waylandgl_uninit,
};
