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
#include "options/m_config.h"
#include "video/out/cocoa_common.h"
#include "context.h"

struct cocoa_opts {
    int cocoa_force_dedicated_gpu;
};

#define OPT_BASE_STRUCT struct cocoa_opts
const struct m_sub_options cocoa_conf = {
    .opts = (const struct m_option[]) {
        OPT_FLAG("cocoa-force-dedicated-gpu", cocoa_force_dedicated_gpu, 0),
        {0}
    },
    .size = sizeof(struct cocoa_opts),
};

struct priv {
    GL gl;
    void (GLAPIENTRY *Flush)(void);
    CGLPixelFormatObj pix;
    CGLContextObj ctx;

    struct cocoa_opts *opts;
};

static int set_swap_interval(int enabled)
{
    CGLContextObj ctx = CGLGetCurrentContext();
    CGLError err = CGLSetParameter(ctx, kCGLCPSwapInterval, &enabled);
    return (err == kCGLNoError) ? 0 : -1;
}

static void glFlushDummy(void) { }

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

static CGLError test_gl_version(struct ra_ctx *ctx, CGLOpenGLProfile ver)
{
    struct priv *p = ctx->priv;

    CGLPixelFormatAttribute attrs[] = {
        // let this array ordered by the inverse order of the most probably
        // rejected attribute to preserve the fallback code
        kCGLPFAOpenGLProfile,
        (CGLPixelFormatAttribute) ver,
        kCGLPFAAccelerated,
        kCGLPFAAllowOfflineRenderers,
        // keep this one last to apply the cocoa-force-dedicated-gpu option
        kCGLPFASupportsAutomaticGraphicsSwitching,
        0
    };

    GLint npix;
    CGLError err;
    int supported_attribute = MP_ARRAY_SIZE(attrs)-1;

    if (p->opts->cocoa_force_dedicated_gpu)
        attrs[--supported_attribute] = 0;

    err = CGLChoosePixelFormat(attrs, &p->pix, &npix);
    while (err == kCGLBadAttribute && supported_attribute > 3) {
        // kCGLPFASupportsAutomaticGraphicsSwitching is probably not
        // supported by the current hardware. Falling back to not using
        // it and disallowing Offline Renderers if further restrictions
        // apply
        attrs[--supported_attribute] = 0;
        err = CGLChoosePixelFormat(attrs, &p->pix, &npix);
    }

    if (err != kCGLNoError) {
        MP_ERR(ctx->vo, "error creating CGL pixel format: %s (%d)\n",
               CGLErrorString(err), err);
        goto error_out;
    }

    err = CGLCreateContext(p->pix, 0, &p->ctx);

error_out:
    return err;
}

static bool create_gl_context(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    GL *gl = &p->gl;
    CGLError err;

    CGLOpenGLProfile gl_versions[] = {
        kCGLOGLPVersion_3_2_Core,
        kCGLOGLPVersion_Legacy,
    };

    for (int n = 0; n < MP_ARRAY_SIZE(gl_versions); n++) {
        err = test_gl_version(ctx, gl_versions[n]);
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

    if (ctx->opts.want_alpha)
        CGLSetParameter(p->ctx, kCGLCPSurfaceOpacity, &(GLint){0});

    mpgl_load_functions(gl, (void *)cocoa_glgetaddr, NULL, ctx->vo->log);
    gl->SwapInterval = set_swap_interval;
    p->Flush = gl->Flush;
    gl->Flush = glFlushDummy;

    CGLReleasePixelFormat(p->pix);

    return true;
}

static void cocoa_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    ra_gl_ctx_uninit(ctx);
    CGLReleaseContext(p->ctx);
    vo_cocoa_uninit(ctx->vo);
}

static void cocoa_swap_buffers(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    vo_cocoa_swap_buffers(ctx->vo);
    p->Flush();
}

static bool cocoa_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    GL *gl = &p->gl;
    p->opts = mp_get_config_group(ctx, ctx->global, &cocoa_conf);
    vo_cocoa_init(ctx->vo);

    MP_WARN(ctx->vo, "opengl cocoa backend is deprecated, use vo=libmpv instead\n");

    if (!create_gl_context(ctx))
        goto fail;

    struct ra_gl_ctx_params params = {
        .swap_buffers = cocoa_swap_buffers,
    };

    if (!ra_gl_ctx_init(ctx, gl, params))
        goto fail;

    return true;

fail:
    cocoa_uninit(ctx);
    return false;
}

static void resize(struct ra_ctx *ctx)
{
    ra_gl_ctx_resize(ctx->swapchain, ctx->vo->dwidth, ctx->vo->dheight, 0);
}

static bool cocoa_reconfig(struct ra_ctx *ctx)
{
    vo_cocoa_config_window(ctx->vo);
    resize(ctx);
    return true;
}

static int cocoa_control(struct ra_ctx *ctx, int *events, int request,
                         void *arg)
{
    int ret = vo_cocoa_control(ctx->vo, events, request, arg);
    if (*events & VO_EVENT_RESIZE)
        resize(ctx);
    return ret;
}

const struct ra_ctx_fns ra_ctx_cocoa = {
    .type           = "opengl",
    .name           = "cocoa",
    .init           = cocoa_init,
    .reconfig       = cocoa_reconfig,
    .control        = cocoa_control,
    .uninit         = cocoa_uninit,
};
