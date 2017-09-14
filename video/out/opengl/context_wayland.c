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
#include "egl_helpers.h"
#include "utils.h"

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

static void waylandgl_swap_buffers(struct ra_ctx *ctx)
{
    struct vo_wayland_state *wl = ctx->vo->wayland;
    vo_wayland_wait_events(ctx->vo, 0);
    eglSwapBuffers(wl->egl_context.egl.dpy, wl->egl_context.egl_surface);
}

static bool egl_create_context(struct ra_ctx *ctx, struct vo_wayland_state *wl)
{
    GL *gl = ctx->priv = talloc_zero(ctx, GL);

    if (!(wl->egl_context.egl.dpy = eglGetDisplay(wl->display.display)))
        return false;

    if (eglInitialize(wl->egl_context.egl.dpy, NULL, NULL) != EGL_TRUE)
        return false;

    if (!mpegl_create_context(ctx, wl->egl_context.egl.dpy,
                              &wl->egl_context.egl.ctx,
                              &wl->egl_context.egl.conf))
        return false;

    eglMakeCurrent(wl->egl_context.egl.dpy, NULL, NULL, wl->egl_context.egl.ctx);

    mpegl_load_functions(gl, wl->log);

    struct ra_gl_ctx_params params = {
        .swap_buffers = waylandgl_swap_buffers,
        .native_display_type = "wl",
        .native_display = wl->display.display,
    };

    if (!ra_gl_ctx_init(ctx, gl, params))
        return false;

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

static bool waylandgl_reconfig(struct ra_ctx *ctx)
{
    struct vo_wayland_state * wl = ctx->vo->wayland;

    if (!vo_wayland_config(ctx->vo))
        return false;

    if (!wl->egl_context.egl_window)
        egl_create_window(wl);

    return true;
}

static void waylandgl_uninit(struct ra_ctx *ctx)
{
    struct vo_wayland_state *wl = ctx->vo->wayland;

    ra_gl_ctx_uninit(ctx);

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

static int waylandgl_control(struct ra_ctx *ctx, int *events, int request,
                             void *data)
{
    struct vo_wayland_state *wl = ctx->vo->wayland;
    int r = vo_wayland_control(ctx->vo, events, request, data);

    if (*events & VO_EVENT_RESIZE) {
        egl_resize(wl);
        ra_gl_ctx_resize(ctx->swapchain, wl->vo->dwidth, wl->vo->dheight, 0);
    }

    return r;
}

static void wayland_wakeup(struct ra_ctx *ctx)
{
    vo_wayland_wakeup(ctx->vo);
}

static void wayland_wait_events(struct ra_ctx *ctx, int64_t until_time_us)
{
    vo_wayland_wait_events(ctx->vo, until_time_us);
}

static bool waylandgl_init(struct ra_ctx *ctx)
{
    if (!vo_wayland_init(ctx->vo))
        return false;

    return egl_create_context(ctx, ctx->vo->wayland);
}

const struct ra_ctx_fns ra_ctx_wayland_egl = {
    .type           = "opengl",
    .name           = "wayland",
    .reconfig       = waylandgl_reconfig,
    .control        = waylandgl_control,
    .wakeup         = wayland_wakeup,
    .wait_events    = wayland_wait_events,
    .init           = waylandgl_init,
    .uninit         = waylandgl_uninit,
};
