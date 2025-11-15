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

#include <libplacebo/config.h>                   // for PL_HAVE_OPENGL, PL_API_VER

#ifdef PL_HAVE_D3D11
#include <libplacebo/d3d11.h>
#endif

#ifdef PL_HAVE_OPENGL
#include "mpv/render_gl.h"                       // for mpv_opengl_init_params
#include <libplacebo/opengl.h>                   // for pl_opengl_destroy
#include "video/out/gpu_next/libmpv_gpu_next.h"  // for libmpv_gpu_next_context
#include "video/out/gpu_next/ra.h"               // for ra_pl_create, ra_pl_...
#endif

#include <stddef.h>                              // for NULL
#include "config.h"                              // for HAVE_GL, HAVE_D3D11
#include "context.h"                             // for gpu_ctx
#include "common/msg.h"                          // for MP_ERR, mp_msg, mp_msg_err
#include "mpv/client.h"                          // for mpv_error
#include "mpv/render.h"                          // for mpv_render_param
#include "options/options.h"                     // for mp_vo_opts
#include "ta/ta_talloc.h"                        // for talloc_zero, talloc_...
#include "video/out/gpu/context.h"               // for ra_ctx_opts, ra_ctx
#include "video/out/libmpv.h"                    // for get_mpv_render_param
#include "video/out/opengl/common.h"             // for GL
#include "video/out/placebo/utils.h"             // for mppl_log_set_probing
#include "video/out/vo.h"                        // for vo

#if HAVE_D3D11
#include "osdep/windows_utils.h"
#include "video/out/d3d11/ra_d3d11.h"
#include "video/out/d3d11/context.h"
#endif

#if HAVE_GL
#include "video/out/opengl/ra_gl.h"              // for ra_is_gl, ra_gl_get
# if HAVE_EGL
#include <EGL/egl.h>                             // for eglGetCurrentContext
# endif
#endif

#if HAVE_VULKAN
#include "video/out/vulkan/context.h"            // for ra_vk_ctx_get
#endif

#if HAVE_GL
// Store Libplacebo OpenGL context information.
struct priv {
    pl_log pl_log;
    pl_opengl gl;
    pl_gpu gpu;
    struct ra_next *ra;

    // Store a persistent copy of the init params to avoid a dangling pointer.
    mpv_opengl_init_params gl_params;
};
#endif

#if HAVE_D3D11
static bool d3d11_pl_init(struct vo *vo, struct gpu_ctx *ctx,
                          struct ra_ctx_opts *ctx_opts)
{
#if !defined(PL_HAVE_D3D11)
    MP_MSG(ctx, vo->probing ? MSGL_V : MSGL_ERR,
           "libplacebo was built without D3D11 support.\n");
    return false;
#else // defined(PL_HAVE_D3D11)
    bool success = false;

    ID3D11Device   *device    = ra_d3d11_get_device(ctx->ra_ctx->ra);
    IDXGISwapChain *swapchain = ra_d3d11_ctx_get_swapchain(ctx->ra_ctx);
    if (!device || !swapchain) {
        mp_err(ctx->log,
               "Failed to receive required components from the mpv d3d11 "
               "context! (device: %s, swap chain: %s)\n",
               device    ? "OK" : "failed",
               swapchain ? "OK" : "failed");
        goto err_out;
    }

    pl_d3d11 d3d11 = pl_d3d11_create(ctx->pllog,
        pl_d3d11_params(
            .device = device,
        )
    );
    if (!d3d11) {
        mp_err(ctx->log, "Failed to acquire a d3d11 libplacebo context!\n");
        goto err_out;
    }
    ctx->gpu = d3d11->gpu;

    mppl_log_set_probing(ctx->pllog, false);

    ctx->swapchain = pl_d3d11_create_swapchain(d3d11,
        pl_d3d11_swapchain_params(
            .swapchain = swapchain,
            .disable_10bit_sdr = ra_d3d11_ctx_prefer_8bit_output_format(ctx->ra_ctx),
        )
    );
    if (!ctx->swapchain) {
        mp_err(ctx->log, "Failed to acquire a d3d11 libplacebo swap chain!\n");
        goto err_out;
    }

    success = true;

err_out:
    SAFE_RELEASE(swapchain);
    SAFE_RELEASE(device);

    return success;
#endif // defined(PL_HAVE_D3D11)
}
#endif // HAVE_D3D11

struct gpu_ctx *gpu_ctx_create(struct vo *vo, struct ra_ctx_opts *ctx_opts)
{
    struct gpu_ctx *ctx = talloc_zero(NULL, struct gpu_ctx);
    ctx->log = vo->log;
    ctx->ra_ctx = ra_ctx_create(vo, *ctx_opts);
    if (!ctx->ra_ctx)
        goto err_out;

#if HAVE_VULKAN
    struct mpvk_ctx *vkctx = ra_vk_ctx_get(ctx->ra_ctx);
    if (vkctx) {
        ctx->pllog = vkctx->pllog;
        ctx->gpu = vkctx->gpu;
        ctx->swapchain = vkctx->swapchain;
        return ctx;
    }
#endif

    ctx->pllog = mppl_log_create(ctx, ctx->log);
    if (!ctx->pllog)
        goto err_out;

    mppl_log_set_probing(ctx->pllog, vo->probing);

#if HAVE_D3D11
    if (ra_is_d3d11(ctx->ra_ctx->ra)) {
        if (!d3d11_pl_init(vo, ctx, ctx_opts))
            goto err_out;

        return ctx;
    }
#endif

#if HAVE_GL && defined(PL_HAVE_OPENGL)
    if (ra_is_gl(ctx->ra_ctx->ra)) {
        struct GL *gl = ra_gl_get(ctx->ra_ctx->ra);
        struct pl_opengl_params params = *pl_opengl_params(
            .debug = ctx_opts->debug,
            .allow_software = ctx_opts->allow_sw,
            .get_proc_addr_ex = (void *) gl->get_fn,
            .proc_ctx = gl->fn_ctx,
        );
# if HAVE_EGL
        params.egl_display = eglGetCurrentDisplay();
        params.egl_context = eglGetCurrentContext();
# endif
        pl_opengl opengl = pl_opengl_create(ctx->pllog, &params);
        if (!opengl)
            goto err_out;
        ctx->gpu = opengl->gpu;

        mppl_log_set_probing(ctx->pllog, false);

        ctx->swapchain = pl_opengl_create_swapchain(opengl, pl_opengl_swapchain_params(
            .max_swapchain_depth = vo->opts->swapchain_depth,
            .framebuffer.flipped = gl->flipped,
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
    if (!ctx->ra_ctx)
        goto skip_common_pl_cleanup;

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

skip_common_pl_cleanup:
    ra_ctx_destroy(&ctx->ra_ctx);

    talloc_free(ctx);
    *ctxp = NULL;
}

#if HAVE_GL && defined(PL_HAVE_OPENGL)
/**
 * @brief Callback to make the OpenGL context current.
 * @param priv Pointer to the private data (mpv_opengl_init_params).
 * @return True on success, false on failure.
 */
static bool pl_callback_makecurrent_gl(void *priv)
{
    mpv_opengl_init_params *gl_params = priv;
    // The mpv render API contract specifies that the client must make the
    // context current inside its get_proc_address callback. We can trigger
    // this by calling it with a harmless, common function name.
    if (gl_params && gl_params->get_proc_address) {
        gl_params->get_proc_address(gl_params->get_proc_address_ctx, "glGetString");
        return true;
    }

    return false;
}

/**
 * @brief Callback to release the OpenGL context.
 * @param priv Pointer to the private data (mpv_opengl_init_params).
 */
static void pl_callback_releasecurrent_gl(void *priv)
{
}

/**
 * @brief Callback to log messages from libplacebo.
 * @param log_priv Pointer to the private data (mp_log).
 * @param level The log level.
 * @param msg The log message.
 */
static void pl_log_cb(void *log_priv, enum pl_log_level level, const char *msg)
{
    struct mp_log *log = log_priv;
    mp_msg(log, MSGL_WARN, "[gpu-next:pl] %s\n", msg);
}

/**
 * @brief Initializes the OpenGL context for the GPU next renderer.
 * @param ctx The libmpv_gpu_next_context to initialize.
 * @param params The render parameters.
 * @return 0 on success, negative error code on failure.
 */
static int libmpv_gpu_next_init_gl(struct libmpv_gpu_next_context *ctx, mpv_render_param *params)
{
    ctx->priv = talloc_zero(NULL, struct priv);
    struct priv *p = ctx->priv;

    mpv_opengl_init_params *gl_params =
    get_mpv_render_param(params, MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, NULL);
    if (!gl_params || !gl_params->get_proc_address)
        return MPV_ERROR_INVALID_PARAMETER;

    // Make a persistent copy of the params struct's contents.
    p->gl_params = *gl_params;

    // Setup libplacebo logging
    struct pl_log_params log_params = {
        .log_level = PL_LOG_DEBUG
    };

    // Enable verbose logging if trace is enabled
    if (mp_msg_test(ctx->log, MSGL_TRACE)) {
        log_params.log_cb = pl_log_cb;
        log_params.log_priv = ctx->log;
    }

    p->pl_log = pl_log_create(PL_API_VER, &log_params);
    p->gl = pl_opengl_create(p->pl_log, pl_opengl_params(
        .get_proc_addr_ex = (pl_voidfunc_t (*)(void*, const char*))gl_params->get_proc_address,
        .proc_ctx = gl_params->get_proc_address_ctx,
        .make_current = pl_callback_makecurrent_gl,
        .release_current = pl_callback_releasecurrent_gl,
        .priv = &p->gl_params // Pass the ADDRESS of our persistent copy
    ));

    if (!p->gl) {
        MP_ERR(ctx, "Failed to create libplacebo OpenGL context.\n");
        pl_log_destroy(&p->pl_log);
        return MPV_ERROR_UNSUPPORTED;
    }
    p->gpu = p->gl->gpu;

    // Pass the libplacebo log to the RA as well.
    p->ra = ra_pl_create(p->gpu, ctx->log, p->pl_log);
    if (!p->ra) {
        pl_opengl_destroy(&p->gl);
        pl_log_destroy(&p->pl_log);
        return MPV_ERROR_VO_INIT_FAILED;
    }

    ctx->ra = p->ra;
    ctx->gpu = p->gpu;
    return 0;
}

/**
 * @brief Wraps an OpenGL framebuffer object (FBO) as a libplacebo texture.
 * @param ctx The libmpv_gpu_next_context.
 * @param params The render parameters.
 * @param out_tex Pointer to the output texture.
 * @return 0 on success, negative error code on failure.
 */
static int libmpv_gpu_next_wrap_fbo_gl(struct libmpv_gpu_next_context *ctx,
                    mpv_render_param *params, pl_tex *out_tex)
{
    struct priv *p = ctx->priv;
    *out_tex = NULL;

    // Get the FBO from the render parameters
    mpv_opengl_fbo *fbo =
        get_mpv_render_param(params, MPV_RENDER_PARAM_OPENGL_FBO, NULL);
    if (!fbo)
        return MPV_ERROR_INVALID_PARAMETER;

    // Wrap the FBO as a libplacebo texture
    pl_tex tex = pl_opengl_wrap(p->gpu, pl_opengl_wrap_params(
        .framebuffer = fbo->fbo,
        .width = fbo->w,
        .height = fbo->h,
        .iformat = fbo->internal_format
    ));

    if (!tex) {
        MP_ERR(ctx, "Failed to wrap provided FBO as a libplacebo texture.\n");
        return MPV_ERROR_GENERIC;
    }

    *out_tex = tex;
    return 0;
}

/**
 * @brief Callback to mark the end of a frame rendering.
 * @param ctx The libmpv_gpu_next_context.
 */
static void libmpv_gpu_next_done_frame_gl(struct libmpv_gpu_next_context *ctx)
{
    // Nothing to do (yet), leaving the function empty.
}

/**
 * @brief Destroys the OpenGL context for the GPU next renderer.
 * @param ctx The libmpv_gpu_next_context to destroy.
 */
static void libmpv_gpu_next_destroy_gl(struct libmpv_gpu_next_context *ctx)
{
    struct priv *p = ctx->priv;
    if (!p)
        return;

    if (p->ra) {
        ra_pl_destroy(&p->ra);
    }

    pl_opengl_destroy(&p->gl);
    pl_log_destroy(&p->pl_log);
}

/**
 * @brief Context functions for the OpenGL GPU next renderer.
 */
const struct libmpv_gpu_next_context_fns libmpv_gpu_next_context_gl = {
    .api_name = MPV_RENDER_API_TYPE_OPENGL,
    .init = libmpv_gpu_next_init_gl,
    .wrap_fbo = libmpv_gpu_next_wrap_fbo_gl,
    .done_frame = libmpv_gpu_next_done_frame_gl,
    .destroy = libmpv_gpu_next_destroy_gl,
};
#endif
