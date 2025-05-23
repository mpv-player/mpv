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

#include "common/common.h"
#include "video/filter/vf_gpu.h"
#include "video/out/opengl/egl_helpers.h"
#include "video/out/opengl/ra_gl.h"

struct gl_offscreen_ctx {
    GL gl;
    EGLDisplay egl_display;
    EGLContext egl_context;
};

static void gl_ctx_destroy(void *p)
{
    struct offscreen_ctx *ctx = p;
    struct gl_offscreen_ctx *gl = ctx->priv;

    ra_free(&ctx->ra);

    if (gl->egl_context)
        eglDestroyContext(gl->egl_display, gl->egl_context);
}

static void gl_ctx_set_context(struct offscreen_ctx *ctx, bool enable)
{
    struct gl_offscreen_ctx *gl = ctx->priv;
    EGLContext c = enable ? gl->egl_context : EGL_NO_CONTEXT;

    if (!eglMakeCurrent(gl->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, c))
        MP_ERR(ctx, "Could not make EGL context current.\n");
}

static struct offscreen_ctx *gl_offscreen_ctx_create(struct mpv_global *global,
                                                     struct mp_log *log)
{
    struct offscreen_ctx *ctx = talloc(NULL, struct offscreen_ctx);
    struct gl_offscreen_ctx *gl = talloc_zero(ctx, struct gl_offscreen_ctx);
    talloc_set_destructor(ctx, gl_ctx_destroy);
    *ctx = (struct offscreen_ctx){
        .log = log,
        .priv = gl,
        .set_context = gl_ctx_set_context,
    };

    // This appears to work with Mesa. EGL 1.5 doesn't specify what a "default
    // display" is at all.
    gl->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(gl->egl_display, NULL, NULL)) {
        MP_ERR(ctx, "Could not initialize EGL.\n");
        goto error;
    }

    // Unfortunately, mpegl_create_context() is entangled with ra_ctx.
    // Fortunately, it does not need much, and we can provide a stub.
    struct ra_ctx ractx = {
        .log = ctx->log,
        .global = global,
    };
    EGLConfig config;
    if (!mpegl_create_context(&ractx, gl->egl_display, &gl->egl_context, &config))
    {
        MP_ERR(ctx, "Could not create EGL context.\n");
        goto error;
    }

    if (!eglMakeCurrent(gl->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                        gl->egl_context))
    {
        MP_ERR(ctx, "Could not make EGL context current.\n");
        goto error;
    }

    mpegl_load_functions(&gl->gl, ctx->log);
    ctx->ra = ra_create_gl(&gl->gl, ctx->log);

    if (!ctx->ra)
        goto error;

    gl_ctx_set_context(ctx, false);

    return ctx;

error:
    talloc_free(ctx);
    return NULL;
}

const struct offscreen_context offscreen_egl = {
    .api = "egl",
    .offscreen_ctx_create = gl_offscreen_ctx_create
};
