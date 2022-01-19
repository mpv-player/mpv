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
 * License along with mpv.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <libplacebo/config.h>

#ifdef PL_HAVE_OPENGL
#include <libplacebo/opengl.h>
#endif

#include "context.h"
#include "config.h"
#include "common/common.h"
#include "options/m_config.h"
#include "video/out/placebo/utils.h"
#include "video/out/gpu/video.h"

#if HAVE_GL
#include "video/out/opengl/context.h"
#include "video/out/opengl/ra_gl.h"
#endif

#if HAVE_VULKAN
#include "video/out/vulkan/context.h"
#endif

struct priv {
#ifdef PL_HAVE_OPENGL
    pl_opengl opengl;
#else
    char dummy;
#endif
};

struct gpu_ctx *gpu_ctx_create(struct vo *vo, struct gl_video_opts *gl_opts)
{
    struct gpu_ctx *ctx = talloc_zero(NULL, struct gpu_ctx);
    ctx->log = vo->log;
    ctx->priv = talloc_zero(ctx, struct priv);
    struct priv *p = ctx->priv;

    struct ra_ctx_opts *ctx_opts = mp_get_config_group(ctx, vo->global, &ra_ctx_conf);
    ctx_opts->want_alpha = gl_opts->alpha_mode == ALPHA_YES;
    ctx->ra_ctx = ra_ctx_create(vo, *ctx_opts);
    if (!ctx->ra_ctx)
        goto err_out;

#if HAVE_VULKAN
    struct mpvk_ctx *vkctx = ra_vk_ctx_get(ctx->ra_ctx);
    if (vkctx) {
        ctx->pllog = vkctx->ctx;
        ctx->gpu = vkctx->gpu;
        ctx->swapchain = vkctx->swapchain;
        return ctx;
    }
#endif

    ctx->pllog = pl_log_create(PL_API_VER, NULL);
    if (!ctx->pllog)
        goto err_out;

    mppl_ctx_set_log(ctx->pllog, ctx->log, vo->probing);
    mp_verbose(ctx->log, "Initialized libplacebo %s (API v%d)\n",
               PL_VERSION, PL_API_VER);

#if HAVE_GL && defined(PL_HAVE_OPENGL)
    if (ra_is_gl(ctx->ra_ctx->ra)) {
        p->opengl = pl_opengl_create(ctx->pllog, pl_opengl_params(
            .debug = ctx_opts->debug,
            .allow_software = ctx_opts->allow_sw,
        ));
        if (!p->opengl)
            goto err_out;
        ctx->gpu = p->opengl->gpu;

        mppl_ctx_set_log(ctx->pllog, ctx->log, false); // disable probing

        ctx->swapchain = pl_opengl_create_swapchain(p->opengl, pl_opengl_swapchain_params(
            .max_swapchain_depth = vo->opts->swapchain_depth,
        ));
        if (!ctx->swapchain)
            goto err_out;

        return ctx;
    }
#elif HAVE_GL
    if (ra_is_gl(ctx->ra_ctx->ra)) {
        MP_MSG(ctx, vo->probing ? MSGL_V : MSGL_ERR,
            "libplacebo was built without OpenGL support.\n");
    }
#endif

err_out:
    gpu_ctx_destroy(&ctx);
    return NULL;
}

bool gpu_ctx_resize(struct gpu_ctx *ctx, int w, int h)
{
    struct priv *p = ctx->priv;

#ifdef PL_HAVE_OPENGL
    // The vulkan context handles this on its own, so only for OpenGL here
    if (p->opengl)
        return pl_swapchain_resize(ctx->swapchain, &w, &h);
#endif

    return true;
}

void gpu_ctx_destroy(struct gpu_ctx **ctxp)
{
    struct gpu_ctx *ctx = *ctxp;
    if (!ctx)
        return;
    if (!ctx->ra_ctx)
        goto skip_common_pl_cleanup;

    struct priv *p = ctx->priv;

#if HAVE_VULKAN
    if (ra_vk_ctx_get(ctx->ra_ctx))
        // vulkan RA context handles pl cleanup by itself,
        // skip common local clean-up.
        goto skip_common_pl_cleanup;
#endif

    if (ctx->swapchain)
        pl_swapchain_destroy(&ctx->swapchain);

#if HAVE_GL && defined(PL_HAVE_OPENGL)
    if (ra_is_gl(ctx->ra_ctx->ra)) {
        pl_opengl_destroy(&p->opengl);
    }
#endif

    if (ctx->pllog)
        pl_log_destroy(&ctx->pllog);

skip_common_pl_cleanup:
    ra_ctx_destroy(&ctx->ra_ctx);

    talloc_free(ctx);
    *ctxp = NULL;
}
