#include "common/msg.h"
#include "ra_d3d11.h"
#include "libmpv/render_dxgi.h"
#include "video/out/gpu/libmpv_gpu.h"
#include "osdep/windows_utils.h"

struct priv {
    struct ra_tex *tex;
    ID3D11Device *device;
    IDXGISwapChain *swapchain;
};

static int init(struct libmpv_gpu_context *ctx, mpv_render_param *params)
{
    MP_VERBOSE(ctx, "Creating libmpv d3d11 context\n");
    struct priv *p = ctx->priv = talloc_zero(NULL, struct priv);

    mpv_dxgi_init_params *init_params =
        get_mpv_render_param(params, MPV_RENDER_PARAM_DXGI_INIT_PARAMS, NULL);
    if (!init_params)
        return MPV_ERROR_INVALID_PARAMETER;

    p->device = init_params->device;
    p->swapchain = init_params->swapchain;
    ID3D11Device_AddRef(p->device);
    ID3D11Device_AddRef(p->swapchain);

    // initialize a blank ra_ctx to reuse ra_gl_ctx
    struct ra_ctx *ra_ctx = talloc_zero(p, struct ra_ctx);
    ra_ctx->log = ctx->log;
    ra_ctx->global = ctx->global;

    if (!spirv_compiler_init(ra_ctx))
        return MPV_ERROR_UNSUPPORTED;

    ra_ctx->ra = ra_d3d11_create(p->device, ctx->log, ra_ctx->spirv);
    if (!ra_ctx->ra)
        return MPV_ERROR_UNSUPPORTED;    

    ctx->ra_ctx = ra_ctx;
    return 0;
}

static int wrap_fbo(struct libmpv_gpu_context *ctx, mpv_render_param *params, struct ra_tex **out)
{
    struct priv *p = ctx->priv;
    ID3D11Resource *backbuffer = NULL;

    if (!p->tex) {
        HRESULT hr = IDXGISwapChain_GetBuffer(p->swapchain, 0, &IID_ID3D11Texture2D,
                        (void**)&backbuffer);
        if (FAILED(hr)) {
            MP_ERR(ctx, "Couldn't get swapchain image\n");
            return MPV_ERROR_UNSUPPORTED;
        }
        p->tex = ra_d3d11_wrap_tex(ctx->ra_ctx->ra, backbuffer);
        SAFE_RELEASE(backbuffer);
    }

    *out = p->tex;
    return 0;
}

static void done_frame(struct libmpv_gpu_context *ctx, bool ds)
{
    struct priv *p = ctx->priv;

    ra_d3d11_flush(ctx->ra_ctx->ra);
    ra_tex_free(ctx->ra_ctx->ra, &p->tex);
}

static void destroy(struct libmpv_gpu_context *ctx)
{
    struct priv *p = ctx->priv;
    if (ctx->ra_ctx->ra)
        ra_tex_free(ctx->ra_ctx->ra, &p->tex);
    SAFE_RELEASE(p->swapchain);
    SAFE_RELEASE(p->device);
}

const struct libmpv_gpu_context_fns libmpv_gpu_context_d3d11 = {
    .api_name    = MPV_RENDER_API_TYPE_DXGI,
    .init        = init,
    .wrap_fbo    = wrap_fbo,
    .done_frame  = done_frame,
    .destroy     = destroy,
};