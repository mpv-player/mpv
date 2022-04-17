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
#include "video/out/placebo/ra_pl.h"
#include "video/out/placebo/utils.h"
#include "video/out/vulkan/utils.h"

struct vk_offscreen_ctx {
    struct ra_ctx *ractx;
    struct mpvk_ctx *vk;
};

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
    struct offscreen_ctx *ctx = talloc_zero(NULL, struct offscreen_ctx);
    talloc_set_destructor(ctx, vk_ctx_destroy);
    *ctx = (struct offscreen_ctx){
        .log = log,
    };

    struct ra_ctx *ractx = talloc_zero(NULL, struct ra_ctx);
    struct mpvk_ctx *vk = talloc_zero(NULL, struct mpvk_ctx);
    ractx->log = ctx->log;
    ractx->global = global;

    vk->log = mp_log_new(ctx, ctx->log, "libplacebo");
    vk->pllog = mppl_log_create(vk->log);
    if (!vk->pllog)
        goto error;

    struct pl_vk_inst_params pl_vk_params = {0};
    pl_vk_params.debug = true;
    mppl_log_set_probing(vk->pllog, true);
    vk->vkinst = pl_vk_inst_create(vk->pllog, &pl_vk_params);
    mppl_log_set_probing(vk->pllog, false);
    if (!vk->vkinst)
        goto error;

    vk->vulkan = pl_vulkan_create(vk->pllog, &(struct pl_vulkan_params) {
        .instance = vk->vkinst->instance,
        .get_proc_addr = vk->vkinst->get_proc_addr,
        .surface = NULL,
        .async_transfer = true,
        .async_compute = true,
        .queue_count = 1,
        .device_name = NULL,
    });
    if (!vk->vulkan)
        goto error;

    vk->gpu = vk->vulkan->gpu;
    ractx->ra = ra_create_pl(vk->gpu, ractx->log);
    if (!ractx->ra)
        goto error;

    struct vk_offscreen_ctx *vkctx = talloc_zero(ctx, struct vk_offscreen_ctx);;

    vkctx->ractx = ractx;
    vkctx->vk = vk;
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
