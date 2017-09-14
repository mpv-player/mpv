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
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <linux/fb.h>
#include <EGL/fbdev_window.h>

#include "common/common.h"
#include "context.h"
#include "egl_helpers.h"

static bool get_fbdev_size(int *w, int *h)
{
    int fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
    if (fd < 0)
        return false;

    struct fb_var_screeninfo info = {0};
    bool ok = !ioctl(fd, FBIOGET_VSCREENINFO, &info);
    if (ok) {
        *w = info.xres;
        *h = info.yres;
    }

    close(fd);

    return ok;
}

struct priv {
    struct GL gl;
    EGLDisplay egl_display;
    EGLConfig egl_config;
    EGLContext egl_context;
    EGLSurface egl_surface;
    struct fbdev_window egl_window;
    int w, h;
};

static void mali_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    ra_gl_ctx_uninit(ctx);

    if (p->egl_surface) {
        eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        eglDestroySurface(p->egl_display, p->egl_surface);
    }
    if (p->egl_context)
        eglDestroyContext(p->egl_display, p->egl_context);
    eglReleaseThread();
}

static void mali_swap_buffers(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    eglSwapBuffers(p->egl_display, p->egl_surface);
}

static bool mali_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);

    if (!get_fbdev_size(&p->w, &p->h)) {
        MP_FATAL(ctx, "Could not get fbdev size.\n");
        goto fail;
    }

    p->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(p->egl_display, NULL, NULL)) {
        MP_FATAL(ctx, "EGL failed to initialize.\n");
        goto fail;
    }

    EGLConfig config;
    if (!mpegl_create_context(ctx, p->egl_display, &p->egl_context, &config))
        goto fail;

    p->egl_window = (struct fbdev_window){
        .width = p->w,
        .height = p->h,
    };

    p->egl_surface = eglCreateWindowSurface(p->egl_display, config,
                                    (EGLNativeWindowType)&p->egl_window, NULL);

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

    mpegl_load_functions(&p->gl, ctx->log);

    struct ra_gl_ctx_params params = {
        .swap_buffers = mali_swap_buffers,
    };

    if (!ra_gl_ctx_init(ctx, &p->gl, params))
        goto fail;

    return true;

fail:
    mali_uninit(ctx);
    return false;
}

static bool mali_reconfig(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    ctx->vo->dwidth = p->w;
    ctx->vo->dheight = p->h;
    ra_gl_ctx_resize(ctx->swapchain, p->w, p->h, 0);
}

static int mali_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    return VO_NOTIMPL;
}

const struct ra_ctx_fns ra_ctx_mali_fbdev = {
    .type           = "opengl",
    .name           = "mali-fbdev",
    .reconfig       = mali_reconfig,
    .control        = mali_control,
    .init           = mali_init,
    .uninit         = mali_uninit,
};
