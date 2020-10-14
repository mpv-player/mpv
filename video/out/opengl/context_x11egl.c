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

#include "common/common.h"
#include "video/out/x11_common.h"
#include "context.h"
#include "egl_helpers.h"
#include "oml_sync.h"
#include "utils.h"

#define EGL_PLATFORM_X11_EXT 0x31D5

struct priv {
    GL gl;
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;

    EGLBoolean (*GetSyncValues)(EGLDisplay, EGLSurface,
                                int64_t*, int64_t*, int64_t*);
    struct oml_sync sync;
};

static void mpegl_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    ra_gl_ctx_uninit(ctx);

    eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
    eglTerminate(p->egl_display);
    vo_x11_uninit(ctx->vo);
}

static int pick_xrgba_config(void *user_data, EGLConfig *configs, int num_configs)
{
    struct ra_ctx *ctx = user_data;
    struct priv *p = ctx->priv;
    struct vo *vo = ctx->vo;

    for (int n = 0; n < num_configs; n++) {
        int vID = 0, num;
        eglGetConfigAttrib(p->egl_display, configs[n], EGL_NATIVE_VISUAL_ID, &vID);
        XVisualInfo template = {.visualid = vID};
        XVisualInfo *vi = XGetVisualInfo(vo->x11->display, VisualIDMask,
                                         &template, &num);
        if (vi) {
            bool is_rgba = vo_x11_is_rgba_visual(vi);
            XFree(vi);
            if (is_rgba)
                return n;
        }
    }

    return 0;
}

static void mpegl_swap_buffers(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    eglSwapBuffers(p->egl_display, p->egl_surface);

    int64_t ust, msc, sbc;
    if (!p->GetSyncValues || !p->GetSyncValues(p->egl_display, p->egl_surface,
                                               &ust, &msc, &sbc))
        ust = msc = sbc = -1;

    oml_sync_swap(&p->sync, ust, msc, sbc);
}

static void mpegl_get_vsync(struct ra_ctx *ctx, struct vo_vsync_info *info)
{
    struct priv *p = ctx->priv;
    oml_sync_get_info(&p->sync, info);
}

static bool mpegl_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    struct vo *vo = ctx->vo;
    int msgl = ctx->opts.probing ? MSGL_V : MSGL_FATAL;

    if (!vo_x11_init(vo))
        goto uninit;

    p->egl_display = mpegl_get_display(EGL_PLATFORM_X11_EXT,
                                       "EGL_EXT_platform_x11",
                                        vo->x11->display);
    if (!eglInitialize(p->egl_display, NULL, NULL)) {
        MP_MSG(ctx, msgl, "Could not initialize EGL.\n");
        goto uninit;
    }

    struct mpegl_cb cb = {
        .user_data = ctx,
        .refine_config = ctx->opts.want_alpha ? pick_xrgba_config : NULL,
    };

    if (!strcmp(eglQueryString(p->egl_display, EGL_VENDOR), "Mesa Project"))
        ctx->opts.want_alpha = 0;

    EGLConfig config;
    if (!mpegl_create_context_cb(ctx, p->egl_display, cb, &p->egl_context, &config))
        goto uninit;

    int cid, vID, n;
    if (!eglGetConfigAttrib(p->egl_display, config, EGL_CONFIG_ID, &cid)) {
        MP_FATAL(ctx, "Getting EGL_CONFIG_ID failed!\n");
        goto uninit;
    }
    if (!eglGetConfigAttrib(p->egl_display, config, EGL_NATIVE_VISUAL_ID, &vID)) {
        MP_FATAL(ctx, "Getting X visual ID failed!\n");
        goto uninit;
    }
    MP_VERBOSE(ctx, "Choosing visual EGL config 0x%x, visual ID 0x%x\n", cid, vID);
    XVisualInfo template = {.visualid = vID};
    XVisualInfo *vi = XGetVisualInfo(vo->x11->display, VisualIDMask, &template, &n);

    if (!vi) {
        MP_FATAL(ctx, "Getting X visual failed!\n");
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
        MP_FATAL(ctx, "Could not create EGL surface!\n");
        goto uninit;
    }

    if (!eglMakeCurrent(p->egl_display, p->egl_surface, p->egl_surface,
                        p->egl_context))
    {
        MP_FATAL(ctx, "Could not make context current!\n");
        goto uninit;
    }

    mpegl_load_functions(&p->gl, ctx->log);

    struct ra_gl_ctx_params params = {
        .swap_buffers = mpegl_swap_buffers,
        .get_vsync    = mpegl_get_vsync,
    };

    if (!ra_gl_ctx_init(ctx, &p->gl, params))
        goto uninit;

    const char *exts = eglQueryString(eglGetCurrentDisplay(), EGL_EXTENSIONS);
    if (gl_check_extension(exts, "EGL_CHROMIUM_sync_control"))
        p->GetSyncValues = (void *)eglGetProcAddress("eglGetSyncValuesCHROMIUM");

    ra_add_native_resource(ctx->ra, "x11", vo->x11->display);

    return true;

uninit:
    mpegl_uninit(ctx);
    return false;
}

static void resize(struct ra_ctx *ctx)
{
    ra_gl_ctx_resize(ctx->swapchain, ctx->vo->dwidth, ctx->vo->dheight, 0);
}

static bool mpegl_reconfig(struct ra_ctx *ctx)
{
    vo_x11_config_vo_window(ctx->vo);
    resize(ctx);
    return true;
}

static int mpegl_control(struct ra_ctx *ctx, int *events, int request,
                         void *arg)
{
    int ret = vo_x11_control(ctx->vo, events, request, arg);
    if (*events & VO_EVENT_RESIZE)
        resize(ctx);
    return ret;
}

static void mpegl_wakeup(struct ra_ctx *ctx)
{
    vo_x11_wakeup(ctx->vo);
}

static void mpegl_wait_events(struct ra_ctx *ctx, int64_t until_time_us)
{
    vo_x11_wait_events(ctx->vo, until_time_us);
}

const struct ra_ctx_fns ra_ctx_x11_egl = {
    .type           = "opengl",
    .name           = "x11egl",
    .reconfig       = mpegl_reconfig,
    .control        = mpegl_control,
    .wakeup         = mpegl_wakeup,
    .wait_events    = mpegl_wait_events,
    .init           = mpegl_init,
    .uninit         = mpegl_uninit,
};
