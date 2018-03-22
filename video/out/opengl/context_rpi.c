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

#include <stddef.h>
#include <assert.h>

#include <bcm_host.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "common/common.h"
#include "osdep/atomic.h"
#include "video/out/win_state.h"
#include "context.h"
#include "egl_helpers.h"

struct priv {
    struct GL gl;
    DISPMANX_DISPLAY_HANDLE_T display;
    DISPMANX_ELEMENT_HANDLE_T window;
    DISPMANX_UPDATE_HANDLE_T update;
    EGLDisplay egl_display;
    EGLConfig egl_config;
    EGLContext egl_context;
    EGLSurface egl_surface;
    // yep, the API keeps a pointer to it
    EGL_DISPMANX_WINDOW_T egl_window;
    int x, y, w, h;
    double display_fps;
    atomic_int reload_display;
    int win_params[4];
};

static void tv_callback(void *callback_data, uint32_t reason, uint32_t param1,
                        uint32_t param2)
{
    struct ra_ctx *ctx = callback_data;
    struct priv *p = ctx->priv;
    atomic_store(&p->reload_display, true);
    vo_wakeup(ctx->vo);
}

static void destroy_dispmanx(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    if (p->egl_surface) {
        eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        eglDestroySurface(p->egl_display, p->egl_surface);
        p->egl_surface = EGL_NO_SURFACE;
    }

    if (p->window)
        vc_dispmanx_element_remove(p->update, p->window);
    p->window = 0;
    if (p->display)
        vc_dispmanx_display_close(p->display);
    p->display = 0;
    if (p->update)
        vc_dispmanx_update_submit_sync(p->update);
    p->update = 0;
}

static void rpi_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    ra_gl_ctx_uninit(ctx);

    vc_tv_unregister_callback_full(tv_callback, ctx);

    destroy_dispmanx(ctx);

    if (p->egl_context)
        eglDestroyContext(p->egl_display, p->egl_context);
    p->egl_context = EGL_NO_CONTEXT;
    eglReleaseThread();
    p->egl_display = EGL_NO_DISPLAY;
}

static bool recreate_dispmanx(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    int display_nr = 0;
    int layer = 0;

    MP_VERBOSE(ctx, "Recreating DISPMANX state...\n");

    destroy_dispmanx(ctx);

    p->display = vc_dispmanx_display_open(display_nr);
    p->update = vc_dispmanx_update_start(0);
    if (!p->display || !p->update) {
        MP_FATAL(ctx, "Could not get DISPMANX objects.\n");
        goto fail;
    }

    uint32_t dispw, disph;
    if (graphics_get_display_size(0, &dispw, &disph) < 0) {
        MP_FATAL(ctx, "Could not get display size.\n");
        goto fail;
    }
    p->w = dispw;
    p->h = disph;

    if (ctx->vo->opts->fullscreen) {
        p->x = p->y = 0;
    } else {
        struct vo_win_geometry geo;
        struct mp_rect screenrc = {0, 0, p->w, p->h};

        vo_calc_window_geometry(ctx->vo, &screenrc, &geo);

        mp_rect_intersection(&geo.win, &screenrc);

        p->x = geo.win.x0;
        p->y = geo.win.y0;
        p->w = geo.win.x1 - geo.win.x0;
        p->h = geo.win.y1 - geo.win.y0;
    }

    // dispmanx is like a neanderthal version of Wayland - you can add an
    // overlay any place on the screen.
    VC_RECT_T dst = {.x = p->x, .y = p->y, .width = p->w, .height = p->h};
    VC_RECT_T src = {.width = p->w << 16, .height = p->h << 16};
    VC_DISPMANX_ALPHA_T alpha = {
        .flags = DISPMANX_FLAGS_ALPHA_FROM_SOURCE,
        .opacity = 0xFF,
    };
    p->window = vc_dispmanx_element_add(p->update, p->display, layer, &dst, 0,
                                        &src, DISPMANX_PROTECTION_NONE, &alpha,
                                        0, 0);
    if (!p->window) {
        MP_FATAL(ctx, "Could not add DISPMANX element.\n");
        goto fail;
    }

    vc_dispmanx_update_submit_sync(p->update);
    p->update = vc_dispmanx_update_start(0);

    p->egl_window = (EGL_DISPMANX_WINDOW_T){
        .element = p->window,
        .width = p->w,
        .height = p->h,
    };
    p->egl_surface = eglCreateWindowSurface(p->egl_display, p->egl_config,
                                            &p->egl_window, NULL);

    if (p->egl_surface == EGL_NO_SURFACE) {
        MP_FATAL(ctx, "Could not create EGL surface!\n");
        goto fail;
    }

    if (!eglMakeCurrent(p->egl_display, p->egl_surface, p->egl_surface,
                        p->egl_context))
    {
        MP_FATAL(ctx, "Failed to set context!\n");
        goto fail;
    }

    p->display_fps = 0;
    TV_GET_STATE_RESP_T tvstate;
    TV_DISPLAY_STATE_T tvstate_disp;
    if (!vc_tv_get_state(&tvstate) && !vc_tv_get_display_state(&tvstate_disp)) {
        if (tvstate_disp.state & (VC_HDMI_HDMI | VC_HDMI_DVI)) {
            p->display_fps = tvstate_disp.display.hdmi.frame_rate;

            HDMI_PROPERTY_PARAM_T param = {
                .property = HDMI_PROPERTY_PIXEL_CLOCK_TYPE,
            };
            if (!vc_tv_hdmi_get_property(&param) &&
                param.param1 == HDMI_PIXEL_CLOCK_TYPE_NTSC)
                p->display_fps = p->display_fps / 1.001;
        } else {
            p->display_fps = tvstate_disp.display.sdtv.frame_rate;
        }
    }

    p->win_params[0] = display_nr;
    p->win_params[1] = layer;
    p->win_params[2] = p->x;
    p->win_params[3] = p->y;

    ctx->vo->dwidth = p->w;
    ctx->vo->dheight = p->h;
    if (ctx->swapchain)
        ra_gl_ctx_resize(ctx->swapchain, p->w, p->h, 0);

    ctx->vo->want_redraw = true;

    vo_event(ctx->vo, VO_EVENT_WIN_STATE);
    return true;

fail:
    destroy_dispmanx(ctx);
    return false;
}

static void rpi_swap_buffers(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    eglSwapBuffers(p->egl_display, p->egl_surface);
}

static bool rpi_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);

    bcm_host_init();

    vc_tv_register_callback(tv_callback, ctx);

    p->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(p->egl_display, NULL, NULL)) {
        MP_FATAL(ctx, "EGL failed to initialize.\n");
        goto fail;
    }

    if (!mpegl_create_context(ctx, p->egl_display, &p->egl_context, &p->egl_config))
        goto fail;

    if (recreate_dispmanx(ctx) < 0)
        goto fail;

    mpegl_load_functions(&p->gl, ctx->log);

    struct ra_gl_ctx_params params = {
        .swap_buffers = rpi_swap_buffers,
    };

    if (!ra_gl_ctx_init(ctx, &p->gl, params))
        goto fail;

    ra_add_native_resource(ctx->ra, "MPV_RPI_WINDOW", p->win_params);

    ra_gl_ctx_resize(ctx->swapchain, ctx->vo->dwidth, ctx->vo->dheight, 0);
    return true;

fail:
    rpi_uninit(ctx);
    return false;
}

static bool rpi_reconfig(struct ra_ctx *ctx)
{
    return recreate_dispmanx(ctx);
}

static struct mp_image *take_screenshot(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    if (!p->display)
        return NULL;

    struct mp_image *img = mp_image_alloc(IMGFMT_BGR0, p->w, p->h);
    if (!img)
        return NULL;

    DISPMANX_RESOURCE_HANDLE_T resource =
        vc_dispmanx_resource_create(VC_IMAGE_ARGB8888,
                                    img->w | ((img->w * 4) << 16), img->h,
                                    &(int32_t){0});
    if (!resource)
        goto fail;

    if (vc_dispmanx_snapshot(p->display, resource, 0))
        goto fail;

    VC_RECT_T rc = {.width = img->w, .height = img->h};
    if (vc_dispmanx_resource_read_data(resource, &rc, img->planes[0], img->stride[0]))
        goto fail;

    vc_dispmanx_resource_delete(resource);
    return img;

fail:
    vc_dispmanx_resource_delete(resource);
    talloc_free(img);
    return NULL;
}

static int rpi_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    struct priv *p = ctx->priv;

    switch (request) {
    case VOCTRL_SCREENSHOT_WIN:
        *(struct mp_image **)arg = take_screenshot(ctx);
        return VO_TRUE;
    case VOCTRL_FULLSCREEN:
        recreate_dispmanx(ctx);
        return VO_TRUE;
    case VOCTRL_CHECK_EVENTS:
        if (atomic_fetch_and(&p->reload_display, 0)) {
            MP_WARN(ctx, "Recovering from display mode switch...\n");
            recreate_dispmanx(ctx);
        }
        return VO_TRUE;
    case VOCTRL_GET_DISPLAY_FPS:
        *(double *)arg = p->display_fps;
        return VO_TRUE;
    }

    return VO_NOTIMPL;
}

const struct ra_ctx_fns ra_ctx_rpi = {
    .type           = "opengl",
    .name           = "rpi",
    .reconfig       = rpi_reconfig,
    .control        = rpi_control,
    .init           = rpi_init,
    .uninit         = rpi_uninit,
};
