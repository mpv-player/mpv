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

#include "options/m_config.h"
#include "video/filter/vf_gpu.h"
#include "video/out/placebo/ra_pl.h"
#include "video/out/placebo/utils.h"
#include "video/out/vulkan/context.h"
#include "video/out/vulkan/utils.h"

struct vk_offscreen_ctx {
    struct ra_ctx *ractx;
    struct mpvk_ctx *vk;
};

extern const struct m_sub_options vulkan_conf;

static void vk_ctx_destroy(void *p)
{
    struct offscreen_ctx *ctx = p;
    struct vk_offscreen_ctx *vkctx = ctx->priv;
    struct ra_ctx *ractx = vkctx->ractx;
    struct mpvk_ctx *vk = vkctx->vk;

    if (ractx->ra) {
        pl_gpu_finish(vk->gpu);
        ractx->ra->fns->destroy(ctx->ra);
        ractx->ra = NULL;
        ctx->ra = NULL;
    }

    vk->gpu = NULL;
    pl_vulkan_destroy(&vk->vulkan);
    mpvk_uninit(vk);
    talloc_free(vk);
    talloc_free(ractx);
}

static struct offscreen_ctx *vk_offscreen_ctx_create(struct mpv_global *global,
                                                     struct mp_log *log)
{
    struct offscreen_ctx *ctx = talloc(NULL, struct offscreen_ctx);
    talloc_set_destructor(ctx, vk_ctx_destroy);
    *ctx = (struct offscreen_ctx){
        .log = log,
    };

    struct ra_ctx *ractx = talloc_zero(ctx, struct ra_ctx);
    struct mpvk_ctx *vk = talloc_zero(ctx, struct mpvk_ctx);
    ractx->log = ctx->log;
    ractx->global = global;

    vk->pllog = mppl_log_create(ctx, log);
    if (!vk->pllog)
        goto error;

    struct pl_vk_inst_params pl_vk_params = {0};
    struct ra_ctx_opts *ctx_opts = mp_get_config_group(NULL, global, &ra_ctx_conf);
    pl_vk_params.debug = ctx_opts->debug;
    talloc_free(ctx_opts);
    mppl_log_set_probing(vk->pllog, true);
    vk->vkinst = pl_vk_inst_create(vk->pllog, &pl_vk_params);
    mppl_log_set_probing(vk->pllog, false);
    if (!vk->vkinst)
        goto error;

    struct vulkan_opts *vk_opts = mp_get_config_group(NULL, global, &vulkan_conf);
    vk->vulkan = mppl_create_vulkan(vk_opts, vk->vkinst, vk->pllog, NULL);
    talloc_free(vk_opts);
    if (!vk->vulkan)
        goto error;

    vk->gpu = vk->vulkan->gpu;
    ractx->ra = ra_create_pl(vk->gpu, ractx->log);
    if (!ractx->ra)
        goto error;

    struct vk_offscreen_ctx *vkctx = talloc(ctx, struct vk_offscreen_ctx);
    *vkctx = (struct vk_offscreen_ctx){
        .ractx = ractx,
        .vk = vk
    };

    ctx->ra = ractx->ra;
    ctx->priv = vkctx;

    return ctx;

error:
    pl_vulkan_destroy(&vk->vulkan);
    mpvk_uninit(vk);
    talloc_free(vk);
    talloc_free(ractx);
    talloc_free(ctx);
    return NULL;
}

const struct offscreen_context offscreen_vk = {
    .api = "vulkan",
    .offscreen_ctx_create = vk_offscreen_ctx_create
};
