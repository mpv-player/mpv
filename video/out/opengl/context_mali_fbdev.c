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

static void *get_proc_address(const GLubyte *name)
{
    void *p = eglGetProcAddress(name);
    // EGL 1.4 (supported by the MALI drivers) does not necessarily return
    // function pointers for core functions.
    if (!p)
        p = dlsym(RTLD_DEFAULT, name);
    return p;
}

struct priv {
    struct mp_log *log;
    struct GL *gl;
    EGLDisplay egl_display;
    EGLConfig egl_config;
    EGLContext egl_context;
    EGLSurface egl_surface;
    struct fbdev_window egl_window;
    int w, h;
};

static void mali_uninit(struct MPGLContext *ctx)
{
    struct priv *p = ctx->priv;

    if (p->egl_surface) {
        eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        eglDestroySurface(p->egl_display, p->egl_surface);
    }
    if (p->egl_context)
        eglDestroyContext(p->egl_display, p->egl_context);
    eglReleaseThread();
}

static int mali_init(struct MPGLContext *ctx, int flags)
{
    struct priv *p = ctx->priv;
    p->log = ctx->vo->log;

    if (!get_fbdev_size(&p->w, &p->h)) {
        MP_FATAL(p, "Could not get fbdev size.\n");
        goto fail;
    }

    p->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(p->egl_display, NULL, NULL)) {
        MP_FATAL(p, "EGL failed to initialize.\n");
        goto fail;
    }

    EGLConfig config;
    if (!mpegl_create_context(p->egl_display, p->log, flags, &p->egl_context,
                              &config))
        goto fail;

    p->egl_window = (struct fbdev_window){
        .width = p->w,
        .height = p->h,
    };

    p->egl_surface = eglCreateWindowSurface(p->egl_display, config,
                                    (EGLNativeWindowType)&p->egl_window, NULL);

    if (p->egl_surface == EGL_NO_SURFACE) {
        MP_FATAL(p, "Could not create EGL surface!\n");
        goto fail;
    }

    if (!eglMakeCurrent(p->egl_display, p->egl_surface, p->egl_surface,
                        p->egl_context))
    {
        MP_FATAL(p, "Failed to set context!\n");
        goto fail;
    }

    ctx->gl = talloc_zero(ctx, GL);

    const char *exts = eglQueryString(p->egl_display, EGL_EXTENSIONS);
    mpgl_load_functions(ctx->gl, get_proc_address, exts, p->log);

    return 0;

fail:
    mali_uninit(ctx);
    return -1;
}

static int mali_reconfig(struct MPGLContext *ctx)
{
    struct priv *p = ctx->priv;
    ctx->vo->dwidth = p->w;
    ctx->vo->dheight = p->h;
    return 0;
}

static void mali_swap_buffers(MPGLContext *ctx)
{
    struct priv *p = ctx->priv;
    eglSwapBuffers(p->egl_display, p->egl_surface);
}

static int mali_control(MPGLContext *ctx, int *events, int request, void *arg)
{
    return VO_NOTIMPL;
}

const struct mpgl_driver mpgl_driver_mali = {
    .name           = "mali-fbdev",
    .priv_size      = sizeof(struct priv),
    .init           = mali_init,
    .reconfig       = mali_reconfig,
    .swap_buffers   = mali_swap_buffers,
    .control        = mali_control,
    .uninit         = mali_uninit,
};
