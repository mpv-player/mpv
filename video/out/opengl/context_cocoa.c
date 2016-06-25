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

#include <OpenGL/OpenGL.h>
#include <dlfcn.h>
#include "video/out/cocoa_common.h"
#include "osdep/macosx_versions.h"
#include "context.h"

struct cgl_context {
    CGLPixelFormatObj pix;
    CGLContextObj ctx;
};

static int set_swap_interval(int enabled)
{
    CGLContextObj ctx = CGLGetCurrentContext();
    CGLError err = CGLSetParameter(ctx, kCGLCPSwapInterval, &enabled);
    return (err == kCGLNoError) ? 0 : -1;
}

static void *cocoa_glgetaddr(const char *s)
{
    void *ret = NULL;
    void *handle = dlopen(
        "/System/Library/Frameworks/OpenGL.framework/OpenGL",
        RTLD_LAZY | RTLD_LOCAL);
    if (!handle)
        return NULL;
    ret = dlsym(handle, s);
    dlclose(handle);
    return ret;
}

static CGLError test_gl_version(struct vo *vo,
                                CGLContextObj *ctx,
                                CGLPixelFormatObj *pix,
                                CGLOpenGLProfile version)
{
    CGLPixelFormatAttribute attrs[] = {
        kCGLPFAOpenGLProfile,
        (CGLPixelFormatAttribute) version,
        kCGLPFADoubleBuffer,
        kCGLPFAAccelerated,
        #if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_8
        // leave this as the last entry of the array to not break the fallback
        // code
        kCGLPFASupportsAutomaticGraphicsSwitching,
        #endif
        0
    };

    GLint npix;
    CGLError err;
    err = CGLChoosePixelFormat(attrs, pix, &npix);
    if (err == kCGLBadAttribute) {
        // kCGLPFASupportsAutomaticGraphicsSwitching is probably not supported
        // by the current hardware. Falling back to not using it.
        attrs[MP_ARRAY_SIZE(attrs) - 2] = 0;
        err = CGLChoosePixelFormat(attrs, pix, &npix);
    }

    if (err != kCGLNoError) {
        MP_ERR(vo, "error creating CGL pixel format: %s (%d)\n",
               CGLErrorString(err), err);
        goto error_out;
    }

    err = CGLCreateContext(*pix, 0, ctx);

error_out:
    return err;
}

static bool create_gl_context(struct MPGLContext *ctx, int vo_flags)
{
    struct cgl_context *p = ctx->priv;
    CGLError err;

    CGLOpenGLProfile gl_versions[] = {
        kCGLOGLPVersion_3_2_Core,
        kCGLOGLPVersion_Legacy,
    };

    for (int n = 0; n < MP_ARRAY_SIZE(gl_versions); n++) {
        err = test_gl_version(ctx->vo, &p->ctx, &p->pix, gl_versions[n]);
        if (err == kCGLNoError)
            break;
    }

    if (err != kCGLNoError) {
        MP_FATAL(ctx->vo, "error creating CGL context: %s (%d)\n",
                 CGLErrorString(err), err);
        return false;
    }

    vo_cocoa_set_opengl_ctx(ctx->vo, p->ctx);
    CGLSetCurrentContext(p->ctx);

    if (vo_flags & VOFLAG_ALPHA)
        CGLSetParameter(p->ctx, kCGLCPSurfaceOpacity, &(GLint){0});

    mpgl_load_functions(ctx->gl, (void *)cocoa_glgetaddr, NULL, ctx->vo->log);

    CGLReleasePixelFormat(p->pix);

    return true;
}

static void cocoa_uninit(MPGLContext *ctx)
{
    struct cgl_context *p = ctx->priv;
    CGLReleaseContext(p->ctx);
    vo_cocoa_uninit(ctx->vo);
}

static int cocoa_init(MPGLContext *ctx, int vo_flags)
{
    vo_cocoa_init(ctx->vo);

    if (!create_gl_context(ctx, vo_flags))
        return -1;

    ctx->gl->SwapInterval = set_swap_interval;
    return 0;
}

static int cocoa_reconfig(struct MPGLContext *ctx)
{
    vo_cocoa_config_window(ctx->vo);
    return 0;
}

static int cocoa_control(struct MPGLContext *ctx, int *events, int request,
                         void *arg)
{
    return vo_cocoa_control(ctx->vo, events, request, arg);
}

static void cocoa_swap_buffers(struct MPGLContext *ctx)
{
    vo_cocoa_swap_buffers(ctx->vo);
}

const struct mpgl_driver mpgl_driver_cocoa = {
    .name           = "cocoa",
    .priv_size      = sizeof(struct cgl_context),
    .init           = cocoa_init,
    .reconfig       = cocoa_reconfig,
    .swap_buffers   = cocoa_swap_buffers,
    .control        = cocoa_control,
    .uninit         = cocoa_uninit,
};