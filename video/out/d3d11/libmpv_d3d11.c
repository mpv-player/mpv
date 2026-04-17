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

#include "common/msg.h"
#include "mpv/render_d3d11.h"
#include "ra_d3d11.h"
#include "video/out/gpu/libmpv_gpu.h"
#include "video/out/gpu/ra.h"

struct priv {
    struct ra_tex *wrapped_tex;
};

static int init(struct libmpv_gpu_context *ctx, mpv_render_param *params)
{
    struct priv *p = ctx->priv = talloc_zero(NULL, struct priv);

    mpv_d3d11_init_params *init_params = get_mpv_render_param(params, MPV_RENDER_PARAM_D3D11_INIT_PARAMS, NULL);
    if (!init_params || !init_params->device)
        return MPV_ERROR_INVALID_PARAMETER;

    struct ra_ctx *ra_ctx = talloc_zero(p, struct ra_ctx);
    ra_ctx->log = ctx->log;
    ra_ctx->global = ctx->global;

    if (!spirv_compiler_init(ra_ctx))
        return MPV_ERROR_UNSUPPORTED;

    ra_ctx->ra = ra_d3d11_create(init_params->device, ctx->log, ra_ctx->spirv);
    if (!ra_ctx->ra)
        return MPV_ERROR_UNSUPPORTED;

    ctx->ra_ctx = ra_ctx;
    return 0;
}

static int wrap_fbo(struct libmpv_gpu_context *ctx, mpv_render_param *params,
                    struct ra_tex **out)
{
    struct priv *p = ctx->priv;

    mpv_d3d11_fbo *fbo = get_mpv_render_param(params, MPV_RENDER_PARAM_D3D11_FBO, NULL);
    if (!fbo || !fbo->tex)
        return MPV_ERROR_INVALID_PARAMETER;

    ra_tex_free(ctx->ra_ctx->ra, &p->wrapped_tex);
    p->wrapped_tex = ra_d3d11_wrap_tex(ctx->ra_ctx->ra, (ID3D11Resource *)fbo->tex);
    if (!p->wrapped_tex)
        return MPV_ERROR_UNSUPPORTED;

    if (!p->wrapped_tex->params.render_dst) {
        MP_ERR(ctx, "Texture was not created with D3D11_BIND_RENDER_TARGET\n");
        ra_tex_free(ctx->ra_ctx->ra, &p->wrapped_tex);
        return MPV_ERROR_INVALID_PARAMETER;
    }

    *out = p->wrapped_tex;
    return 0;
}

static void done_frame(struct libmpv_gpu_context *ctx, bool ds)
{
    struct priv *p = ctx->priv;

    ra_d3d11_flush(ctx->ra_ctx->ra);
    ra_tex_free(ctx->ra_ctx->ra, &p->wrapped_tex);
}

static void destroy(struct libmpv_gpu_context *ctx)
{
    struct priv *p = ctx->priv;

    if (ctx->ra_ctx && ctx->ra_ctx->ra) {
        ra_tex_free(ctx->ra_ctx->ra, &p->wrapped_tex);
        // ra_d3d11 frees its own struct from inside its destroy callback.
        ctx->ra_ctx->ra->fns->destroy(ctx->ra_ctx->ra);
        ctx->ra_ctx->ra = NULL;
    }
}

const struct libmpv_gpu_context_fns libmpv_gpu_context_d3d11 = {
    .api_name   = MPV_RENDER_API_TYPE_D3D11,
    .init       = init,
    .wrap_fbo   = wrap_fbo,
    .done_frame = done_frame,
    .destroy    = destroy,
};
