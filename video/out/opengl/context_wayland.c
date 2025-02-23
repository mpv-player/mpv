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

#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "video/out/present_sync.h"
#include "video/out/wayland_common.h"
#include "context.h"
#include "egl_helpers.h"
#include "utils.h"

#define EGL_PLATFORM_WAYLAND_EXT 0x31D8

struct priv {
    GL gl;
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
    EGLConfig  egl_config;
    struct wl_egl_window *egl_window;
};

static void resize(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    struct vo_wayland_state *wl = ctx->vo->wl;

    MP_VERBOSE(wl, "Handling resize on the egl side\n");

    const int32_t width = mp_rect_w(wl->geometry);
    const int32_t height = mp_rect_h(wl->geometry);

    vo_wayland_handle_scale(wl);

    vo_wayland_set_opaque_region(wl, ctx->opts.want_alpha);
    if (p->egl_window)
        wl_egl_window_resize(p->egl_window, width, height, 0, 0);

    wl->vo->dwidth  = width;
    wl->vo->dheight = height;
}

static bool wayland_egl_check_visible(struct ra_ctx *ctx)
{
    return vo_wayland_check_visible(ctx->vo);
}

static void wayland_egl_swap_buffers(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    struct vo_wayland_state *wl = ctx->vo->wl;

    eglSwapBuffers(p->egl_display, p->egl_surface);

    if (wl->opts->wl_internal_vsync)
        vo_wayland_wait_frame(wl);

    if (wl->use_present)
        present_sync_swap(wl->present);
}

static void wayland_egl_get_vsync(struct ra_ctx *ctx, struct vo_vsync_info *info)
{
    struct vo_wayland_state *wl = ctx->vo->wl;
    if (wl->use_present)
        present_sync_get_info(wl->present, info);
}

static bool egl_create_context(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    struct vo_wayland_state *wl = ctx->vo->wl;

    if (!(p->egl_display = mpegl_get_display(EGL_PLATFORM_WAYLAND_EXT,
                                             "EGL_EXT_platform_wayland",
                                             wl->display)))
        return false;

    if (eglInitialize(p->egl_display, NULL, NULL) != EGL_TRUE)
        return false;

    if (!mpegl_create_context(ctx, p->egl_display, &p->egl_context,
                              &p->egl_config))
        return false;

    eglMakeCurrent(p->egl_display, NULL, NULL, p->egl_context);

    mpegl_load_functions(&p->gl, wl->log);

    struct ra_ctx_params params = {
        .check_visible      = wayland_egl_check_visible,
        .swap_buffers       = wayland_egl_swap_buffers,
        .get_vsync          = wayland_egl_get_vsync,
    };

    if (!ra_gl_ctx_init(ctx, &p->gl, params))
        return false;

    ra_add_native_resource(ctx->ra, "wl", wl->display);

    return true;
}

static void egl_create_window(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    struct vo_wayland_state *wl = ctx->vo->wl;

    p->egl_window = wl_egl_window_create(wl->surface,
                                         mp_rect_w(wl->geometry),
                                         mp_rect_h(wl->geometry));

    p->egl_surface = mpegl_create_window_surface(
        p->egl_display, p->egl_config, p->egl_window);
    if (p->egl_surface == EGL_NO_SURFACE) {
        p->egl_surface = eglCreateWindowSurface(
            p->egl_display, p->egl_config, p->egl_window, NULL);
    }

    eglMakeCurrent(p->egl_display, p->egl_surface, p->egl_surface, p->egl_context);
    // eglMakeCurrent may not configure the draw or read buffers if the context
    // has been made current previously. On nvidia GL_NONE is bound because EGL_NO_SURFACE
    // is used initially and we must bind the read and draw buffers here.
    if(!p->gl.es) {
        p->gl.ReadBuffer(GL_BACK);
        p->gl.DrawBuffer(GL_BACK);
    }

    eglSwapInterval(p->egl_display, 0);
}

static bool wayland_egl_reconfig(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    if (!vo_wayland_reconfig(ctx->vo))
        return false;

    if (!p->egl_window)
        egl_create_window(ctx);

    return true;
}

static void wayland_egl_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    ra_gl_ctx_uninit(ctx);

    if (p->egl_context) {
        eglReleaseThread();
        if (p->egl_window)
            wl_egl_window_destroy(p->egl_window);
        eglDestroySurface(p->egl_display, p->egl_surface);
        eglMakeCurrent(p->egl_display, NULL, NULL, EGL_NO_CONTEXT);
        eglDestroyContext(p->egl_display, p->egl_context);
        p->egl_context = NULL;
    }
    eglTerminate(p->egl_display);

    vo_wayland_uninit(ctx->vo);
}

static int wayland_egl_control(struct ra_ctx *ctx, int *events, int request,
                             void *data)
{
    struct vo_wayland_state *wl = ctx->vo->wl;
    int r = vo_wayland_control(ctx->vo, events, request, data);

    if (*events & VO_EVENT_RESIZE) {
        resize(ctx);
        ra_gl_ctx_resize(ctx->swapchain, wl->vo->dwidth, wl->vo->dheight, 0);
    }

    return r;
}

static void wayland_egl_wakeup(struct ra_ctx *ctx)
{
    vo_wayland_wakeup(ctx->vo);
}

static void wayland_egl_wait_events(struct ra_ctx *ctx, int64_t until_time_ns)
{
    vo_wayland_wait_events(ctx->vo, until_time_ns);
}

static void wayland_egl_update_render_opts(struct ra_ctx *ctx)
{
    struct vo_wayland_state *wl = ctx->vo->wl;
    vo_wayland_set_opaque_region(wl, ctx->opts.want_alpha);
    wl_surface_commit(wl->surface);
}

static bool wayland_egl_init(struct ra_ctx *ctx)
{
    if (!vo_wayland_init(ctx->vo))
        return false;
    return egl_create_context(ctx);
}

const struct ra_ctx_fns ra_ctx_wayland_egl = {
    .type               = "opengl",
    .name               = "wayland",
    .description        = "Wayland/EGL",
    .reconfig           = wayland_egl_reconfig,
    .control            = wayland_egl_control,
    .wakeup             = wayland_egl_wakeup,
    .wait_events        = wayland_egl_wait_events,
    .update_render_opts = wayland_egl_update_render_opts,
    .init               = wayland_egl_init,
    .uninit             = wayland_egl_uninit,
};
