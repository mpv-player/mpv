/*
 * This file is part of mpv.
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
 *
 * You can alternatively redistribute this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <OpenGL/OpenGL.h>
#include <dlfcn.h>
#include "cocoa_common.h"
#include "osdep/macosx_versions.h"
#include "gl_common.h"

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

static int cgl_color_size(struct MPGLContext *ctx)
{
    struct cgl_context *p = ctx->priv;
    GLint value;
    CGLDescribePixelFormat(p->pix, 0, kCGLPFAColorSize, &value);
    return value > 16 ? 8 : 5;
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

static bool create_gl_context(struct MPGLContext *ctx)
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

    vo_cocoa_create_nsgl_ctx(ctx->vo, p->ctx);
    ctx->depth_r = ctx->depth_g = ctx->depth_b = cgl_color_size(ctx);
    mpgl_load_functions(ctx->gl, (void *)cocoa_glgetaddr, NULL, ctx->vo->log);

    CGLReleasePixelFormat(p->pix);

    return true;
}

static bool config_window_cocoa(struct MPGLContext *ctx, int flags)
{
    struct cgl_context *p = ctx->priv;

    if (p->ctx == NULL)
        if (!create_gl_context(ctx))
            return false;

    if (!ctx->gl->SwapInterval)
        ctx->gl->SwapInterval = set_swap_interval;

    vo_cocoa_config_window(ctx->vo, flags);

    return true;
}

static void releaseGlContext_cocoa(MPGLContext *ctx)
{
    struct cgl_context *p = ctx->priv;
    vo_cocoa_release_nsgl_ctx(ctx->vo);
    CGLReleaseContext(p->ctx);
}

static void swapGlBuffers_cocoa(MPGLContext *ctx)
{
    vo_cocoa_swap_buffers(ctx->vo);
}

static void set_current_cocoa(MPGLContext *ctx, bool current)
{
    vo_cocoa_set_current_context(ctx->vo, current);
}

void mpgl_set_backend_cocoa(MPGLContext *ctx)
{
    ctx->priv = talloc_zero(ctx, struct cgl_context);
    ctx->config_window = config_window_cocoa;
    ctx->releaseGlContext = releaseGlContext_cocoa;
    ctx->swapGlBuffers = swapGlBuffers_cocoa;
    ctx->vo_init = vo_cocoa_init;
    ctx->vo_uninit = vo_cocoa_uninit;
    ctx->vo_control = vo_cocoa_control;
    ctx->set_current = set_current_cocoa;
}
