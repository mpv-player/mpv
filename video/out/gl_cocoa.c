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

#include <OpenGL/OpenGL.h>
#include "cocoa_common.h"
#include "gl_common.h"

struct cgl_context {
    CGLPixelFormatObj pix;
    CGLContextObj ctx;
};

static void gl_clear(void *ctx)
{
    struct GL *gl = ctx;
    gl->ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

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
    switch (value) {
        case 32:
        case 24:
            return 8;
        case 16:
            return 5;
        default:
            return 8;
    }
}

static bool create_gl_context(struct MPGLContext *ctx)
{
    struct cgl_context *p = ctx->priv;
    CGLError err;

    CGLOpenGLProfile gl_vers_map[] = {
        [2] = kCGLOGLPVersion_Legacy,
        [3] = kCGLOGLPVersion_GL3_Core,
        [4] = kCGLOGLPVersion_GL4_Core,
    };

    int gl_major = MPGL_VER_GET_MAJOR(ctx->requested_gl_version);
    if (gl_major < 2 || gl_major >= MP_ARRAY_SIZE(gl_vers_map)) {
        MP_FATAL(ctx->vo, "OpenGL major version %d not supported", gl_major);
        return false;
    }

    CGLPixelFormatAttribute attrs[] = {
        kCGLPFAOpenGLProfile,
        (CGLPixelFormatAttribute) gl_vers_map[gl_major],
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
    err = CGLChoosePixelFormat(attrs, &p->pix, &npix);
    if (err == kCGLBadAttribute) {
        // kCGLPFASupportsAutomaticGraphicsSwitching is probably not supported
        // by the current hardware. Falling back to not using it.
        MP_ERR(ctx->vo, "error creating CGL pixel format with automatic GPU "
                        "switching. falling back\n");
        attrs[MP_ARRAY_SIZE(attrs) - 2] = 0;
        err = CGLChoosePixelFormat(attrs, &p->pix, &npix);
    }

    if (err != kCGLNoError) {
        MP_FATAL(ctx->vo, "error creating CGL pixel format: %s (%d)\n",
                 CGLErrorString(err), err);
    }

    if ((err = CGLCreateContext(p->pix, 0, &p->ctx)) != kCGLNoError) {
        MP_FATAL(ctx->vo, "error creating CGL context: %s (%d)\n",
                 CGLErrorString(err), err);
        return false;
    }

    vo_cocoa_create_nsgl_ctx(ctx->vo, p->ctx);
    ctx->depth_r = ctx->depth_g = ctx->depth_b = cgl_color_size(ctx);
    mpgl_load_functions(ctx->gl, (void *)vo_cocoa_glgetaddr, NULL, ctx->vo->log);

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

    vo_cocoa_config_window(ctx->vo, flags, p->ctx);
    vo_cocoa_register_gl_clear_callback(ctx->vo, ctx->gl, gl_clear);

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
    ctx->register_resize_callback = vo_cocoa_register_resize_callback;
    ctx->vo_uninit = vo_cocoa_uninit;
    ctx->vo_control = vo_cocoa_control;
    ctx->set_current = set_current_cocoa;
}
