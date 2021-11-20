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

#include "context.h"
#include "config.h"
#include "common/common.h"
#include "options/m_config.h"
#include "video/out/placebo/utils.h"
#include "video/out/gpu/video.h"

#if HAVE_VULKAN
#include "video/out/vulkan/context.h"
#endif

struct priv {
    char dummy;
};

struct gpu_ctx *gpu_ctx_create(struct vo *vo, struct gl_video_opts *gl_opts)
{
    struct gpu_ctx *ctx = talloc_zero(NULL, struct gpu_ctx);
    ctx->log = vo->log;
    ctx->priv = talloc_zero(ctx, struct priv);

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

    // TODO: wrap GL contexts

err_out:
    gpu_ctx_destroy(&ctx);
    return NULL;
}

bool gpu_ctx_resize(struct gpu_ctx *ctx, int w, int h)
{
    // Not (yet) used
    return true;
}

void gpu_ctx_destroy(struct gpu_ctx **ctxp)
{
    struct gpu_ctx *ctx = *ctxp;
    if (!ctx)
        return;

    ra_ctx_destroy(&ctx->ra_ctx);

    talloc_free(ctx);
    *ctxp = NULL;
}
