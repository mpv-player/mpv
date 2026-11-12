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

#ifdef PL_HAVE_D3D11
#include <libplacebo/d3d11.h>
#endif

#ifdef PL_HAVE_OPENGL
#include <libplacebo/opengl.h>
#endif

#include "context.h"
#include "config.h"
#include "common/common.h"
#include "options/m_config.h"
#include "video/out/placebo/utils.h"
#include "video/out/gpu/video.h"

#if HAVE_D3D11
#include "osdep/windows_utils.h"
#include "video/out/d3d11/ra_d3d11.h"
#include "video/out/d3d11/context.h"
#endif

#if HAVE_GL
#include "video/out/opengl/context.h"
#include "video/out/opengl/ra_gl.h"
# if HAVE_EGL
#include <EGL/egl.h>
# endif
#endif

#if HAVE_VULKAN
#include "video/out/vulkan/context.h"
#endif

static bool create_gpu(struct gpu_ctx *ctx, bool probing)
{
#if HAVE_VULKAN
    struct mpvk_ctx *vkctx = ra_vk_ctx_get(ctx->ra_ctx);
    if (vkctx) {
        ctx->pllog = vkctx->pllog;
        ctx->gpu = vkctx->gpu;
        return true;
    }
#endif

    ctx->pllog = mppl_log_create(ctx, ctx->log);
    if (!ctx->pllog)
        return false;

    mppl_log_set_probing(ctx->pllog, probing);

#if HAVE_D3D11 && defined(PL_HAVE_D3D11)
    if (ra_is_d3d11(ctx->ra_ctx->ra)) {
        ID3D11Device *device = ra_d3d11_get_device(ctx->ra_ctx->ra);
        pl_d3d11 d3d11 = pl_d3d11_create(ctx->pllog,
            pl_d3d11_params(
                .device = device,
            )
        );
        SAFE_RELEASE(device);
        if (!d3d11) {
            mp_err(ctx->log, "Failed to acquire a d3d11 libplacebo context!\n");
            return false;
        }
        ctx->gpu = d3d11->gpu;
    }
#elif HAVE_D3D11
    if (ra_is_d3d11(ctx->ra_ctx->ra)) {
        MP_MSG(ctx, probing ? MSGL_V : MSGL_ERR,
               "libplacebo was built without D3D11 support.\n");
    }
#endif

#if HAVE_GL && defined(PL_HAVE_OPENGL)
    if (ra_is_gl(ctx->ra_ctx->ra)) {
        struct GL *gl = ra_gl_get(ctx->ra_ctx->ra);
        struct pl_opengl_params params = *pl_opengl_params(
            .debug = ctx->ra_ctx->opts.debug,
            .allow_software = ctx->ra_ctx->opts.allow_sw,
            .get_proc_addr_ex = (void *) gl->get_fn,
            .proc_ctx = gl->fn_ctx,
        );
# if HAVE_EGL
        params.egl_display = eglGetCurrentDisplay();
        params.egl_context = eglGetCurrentContext();
# endif
        pl_opengl opengl = pl_opengl_create(ctx->pllog, &params);
        if (!opengl)
            return false;
        ctx->gpu = opengl->gpu;
    }
#elif HAVE_GL
    if (ra_is_gl(ctx->ra_ctx->ra)) {
        MP_MSG(ctx, probing ? MSGL_V : MSGL_ERR,
            "libplacebo was built without OpenGL support.\n");
    }
#endif

    if (!ctx->gpu)
        return false;

    mppl_log_set_probing(ctx->pllog, false);
    return true;
}

static bool create_swapchain(struct gpu_ctx *ctx, int swapchain_depth)
{
#if HAVE_VULKAN
    struct mpvk_ctx *vkctx = ra_vk_ctx_get(ctx->ra_ctx);
    if (vkctx) {
        ctx->swapchain = vkctx->swapchain;
        return true;
    }
#endif

#if HAVE_D3D11 && defined(PL_HAVE_D3D11)
    if (ra_is_d3d11(ctx->ra_ctx->ra)) {
        IDXGISwapChain *swapchain = ra_d3d11_ctx_get_swapchain(ctx->ra_ctx);
        if (!swapchain) {
            mp_err(ctx->log, "Failed to receive the swap chain from the mpv d3d11 context!\n");
            return false;
        }

        struct pl_d3d11_swapchain_params *params = pl_d3d11_swapchain_params(
             .swapchain = swapchain,
        );
        ra_d3d11_ctx_set_swapchain_params(ctx->ra_ctx, params);
        ctx->swapchain = pl_d3d11_create_swapchain(pl_d3d11_get(ctx->gpu), params);
        SAFE_RELEASE(swapchain);
        if (!ctx->swapchain)
            mp_err(ctx->log, "Failed to acquire a d3d11 libplacebo swap chain!\n");
    }
#endif

#if HAVE_GL && defined(PL_HAVE_OPENGL)
    if (ra_is_gl(ctx->ra_ctx->ra)) {
        struct GL *gl = ra_gl_get(ctx->ra_ctx->ra);
        pl_opengl opengl = pl_opengl_get(ctx->gpu);
        ctx->swapchain = pl_opengl_create_swapchain(opengl, pl_opengl_swapchain_params(
            .max_swapchain_depth = swapchain_depth,
            .framebuffer.flipped = gl->flipped,
        ));
    }
#endif

    return ctx->swapchain;
}

struct gpu_ctx *gpu_ctx_create(struct vo *vo, struct ra_ctx *ra_ctx)
{
    struct gpu_ctx *ctx = talloc_zero(NULL, struct gpu_ctx);
    ctx->log = vo->log;
    ctx->ra_ctx = ra_ctx;

    if (!create_gpu(ctx, vo->probing))
        goto err_out;

    if (!create_swapchain(ctx, vo->opts->swapchain_depth))
        goto err_out;

    return ctx;

err_out:
    gpu_ctx_destroy(&ctx);
    return NULL;
}

bool gpu_ctx_resize(struct gpu_ctx *ctx, int w, int h)
{
#if HAVE_VULKAN
    if (ra_vk_ctx_get(ctx->ra_ctx))
        // vulkan RA handles this by itself
        return true;
#endif

    return pl_swapchain_resize(ctx->swapchain, &w, &h);
}

void gpu_ctx_destroy(struct gpu_ctx **ctxp)
{
    struct gpu_ctx *ctx = *ctxp;
    if (!ctx)
        return;

#if HAVE_VULKAN
    if (ra_vk_ctx_get(ctx->ra_ctx))
        // vulkan RA context handles pl cleanup by itself,
        // skip common local clean-up.
        goto skip_common_pl_cleanup;
#endif

    if (ctx->swapchain)
        pl_swapchain_destroy(&ctx->swapchain);

    if (ctx->gpu) {
#if HAVE_GL && defined(PL_HAVE_OPENGL)
        if (ra_is_gl(ctx->ra_ctx->ra)) {
            pl_opengl opengl = pl_opengl_get(ctx->gpu);
            pl_opengl_destroy(&opengl);
        }
#endif

#if HAVE_D3D11 && defined(PL_HAVE_D3D11)
        if (ra_is_d3d11(ctx->ra_ctx->ra)) {
            pl_d3d11 d3d11 = pl_d3d11_get(ctx->gpu);
            pl_d3d11_destroy(&d3d11);
        }
#endif
    }

    if (ctx->pllog)
        pl_log_destroy(&ctx->pllog);

#if HAVE_VULKAN
skip_common_pl_cleanup:
#endif

    talloc_free(ctx);
    *ctxp = NULL;
}
