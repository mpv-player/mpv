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

#include "video/out/gpu/context.h"
#include "osdep/macOS_swift.h"

//#import <MoltenVK/mvk_vulkan.h>

#include "common.h"
#include "context.h"
#include "utils.h"

struct priv {
    struct mpvk_ctx vk;
    MacCommon *vo_macos;
};


static void macos_vk_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    ra_vk_ctx_uninit(ctx);
    mpvk_uninit(&p->vk);
    [p->vo_macos uninit:ctx->vo];
}

static void macos_vk_swap_buffers(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    [p->vo_macos swapBuffer];
}

static bool macos_vk_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    struct mpvk_ctx *vk = &p->vk;
    int msgl = ctx->opts.probing ? MSGL_V : MSGL_ERR;

    if (!mpvk_init(vk, ctx, VK_EXT_METAL_SURFACE_EXTENSION_NAME))
        goto error;

    p->vo_macos = [[MacCommon alloc] init:ctx->vo];
    if (!p->vo_macos)
        goto error;

    VkMetalSurfaceCreateInfoEXT macos_info = {
        .sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK,
        .pNext = NULL,
        .flags = 0,
        .pLayer = p->vo_macos.layer,
    };

    struct ra_vk_ctx_params params = {
        .swap_buffers = macos_vk_swap_buffers,
    };

    VkInstance inst = vk->vkinst->instance;
    VkResult res = vkCreateMetalSurfaceEXT(inst, &macos_info, NULL, &vk->surface);
    if (res != VK_SUCCESS) {
        MP_MSG(ctx, msgl, "Failed creating macos surface\n");
        goto error;
    }

    if (!ra_vk_ctx_init(ctx, vk, params, VK_PRESENT_MODE_FIFO_KHR))
        goto error;

    return true;
error:
    if (p->vo_macos)
        [p->vo_macos uninit:ctx->vo];
    return false;
}

static bool resize(struct ra_ctx *ctx)
{

    return ra_vk_ctx_resize(ctx, ctx->vo->dwidth, ctx->vo->dheight);
}

static bool macos_vk_reconfig(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    if (![p->vo_macos config:ctx->vo])
        return false;
    return true;
}

static int macos_vk_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    struct priv *p = ctx->priv;
    int ret = [p->vo_macos control:ctx->vo events:events request:request data:arg];

    if (*events & VO_EVENT_RESIZE) {
        if (!resize(ctx))
            return VO_ERROR;
    }

    return ret;
}

const struct ra_ctx_fns ra_ctx_vulkan_macos = {
    .type           = "vulkan",
    .name           = "macosvk",
    .reconfig       = macos_vk_reconfig,
    .control        = macos_vk_control,
    .init           = macos_vk_init,
    .uninit         = macos_vk_uninit,
};
