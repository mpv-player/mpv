#include "common.h"
#include "context.h"
#include "ra_gl.h"
#include "options/m_config.h"
#include "libmpv/render_gl.h"
#include "video/out/gpu/libmpv_gpu.h"
#include "video/out/gpu/ra.h"

struct priv {
    GL *gl;
    struct ra_ctx *ra_ctx;
};

static int init(struct libmpv_gpu_context *ctx, mpv_render_param *params)
{
    ctx->priv = talloc_zero(NULL, struct priv);
    struct priv *p = ctx->priv;

    mpv_opengl_init_params *init_params =
        get_mpv_render_param(params, MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, NULL);
    if (!init_params)
        return MPV_ERROR_INVALID_PARAMETER;

    p->gl = talloc_zero(p, GL);

    mpgl_load_functions2(p->gl, init_params->get_proc_address,
                         init_params->get_proc_address_ctx,
                         init_params->extra_exts, ctx->log);
    if (!p->gl->version && !p->gl->es) {
        MP_FATAL(ctx, "OpenGL not initialized.\n");
        return MPV_ERROR_UNSUPPORTED;
    }

    // initialize a blank ra_ctx to reuse ra_gl_ctx
    p->ra_ctx = talloc_zero(p, struct ra_ctx);
    p->ra_ctx->log = ctx->log;
    p->ra_ctx->global = ctx->global;
    p->ra_ctx->opts = (struct ra_ctx_opts) {
        .probing = false,
        .allow_sw = true,
    };

    static const struct ra_swapchain_fns empty_swapchain_fns = {0};
    struct ra_gl_ctx_params gl_params = {
        // vo_opengl_cb is essentially like a gigantic external swapchain where
        // the user is in charge of presentation / swapping etc. But we don't
        // actually need to provide any of these functions, since we can just
        // not call them to begin with - so just set it to an empty object to
        // signal to ra_gl_p that we don't care about its latency emulation
        // functionality
        .external_swapchain = &empty_swapchain_fns
    };

    p->gl->SwapInterval = NULL; // we shouldn't randomly change this, so lock it
    if (!ra_gl_ctx_init(p->ra_ctx, p->gl, gl_params))
        return MPV_ERROR_UNSUPPORTED;

    int debug;
    mp_read_option_raw(ctx->global, "gpu-debug", &m_option_type_flag, &debug);
    p->ra_ctx->opts.debug = debug;
    p->gl->debug_context = debug;
    ra_gl_set_debug(p->ra_ctx->ra, debug);

    ctx->ra = p->ra_ctx->ra;

    // Legacy API user loading for opengl-cb. Explicitly inactive for render API.
    if (get_mpv_render_param(params, (mpv_render_param_type)-1, NULL) ==
        ctx->global && p->gl->MPGetNativeDisplay)
    {
        void *x11 = p->gl->MPGetNativeDisplay("x11");
        if (x11)
            ra_add_native_resource(ctx->ra, "x11", x11);
        void *wl = p->gl->MPGetNativeDisplay("wl");
        if (wl)
            ra_add_native_resource(ctx->ra, "wl", wl);
    }

    return 0;
}

static int wrap_fbo(struct libmpv_gpu_context *ctx, mpv_render_param *params,
                    struct ra_tex **out)
{
    struct priv *p = ctx->priv;

    mpv_opengl_fbo *fbo =
        get_mpv_render_param(params, MPV_RENDER_PARAM_OPENGL_FBO, NULL);
    if (!fbo)
        return MPV_ERROR_INVALID_PARAMETER;

    if (fbo->fbo && !(p->gl->mpgl_caps & MPGL_CAP_FB)) {
        MP_FATAL(ctx, "Rendering to FBO requested, but no FBO extension found!\n");
        return MPV_ERROR_UNSUPPORTED;
    }

    struct ra_swapchain *sw = p->ra_ctx->swapchain;
    struct ra_fbo target;
    ra_gl_ctx_resize(sw, fbo->w, fbo->h, fbo->fbo);
    ra_gl_ctx_start_frame(sw, &target);
    *out = target.tex;
    return 0;
}

static void done_frame(struct libmpv_gpu_context *ctx, bool ds)
{
    struct priv *p = ctx->priv;

    struct ra_swapchain *sw = p->ra_ctx->swapchain;
    struct vo_frame dummy = {.display_synced = ds};
    ra_gl_ctx_submit_frame(sw, &dummy);
}

static void destroy(struct libmpv_gpu_context *ctx)
{
    struct priv *p = ctx->priv;

    if (p->ra_ctx)
        ra_gl_ctx_uninit(p->ra_ctx);
}

const struct libmpv_gpu_context_fns libmpv_gpu_context_gl = {
    .api_name = MPV_RENDER_API_TYPE_OPENGL,
    .init = init,
    .wrap_fbo = wrap_fbo,
    .done_frame = done_frame,
    .destroy = destroy,
};
