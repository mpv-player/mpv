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

#import <QuartzCore/QuartzCore.h>

#include "video/out/gpu/context.h"
#include "osdep/mac/swift.h"

#include "common.h"
#include "context.h"
#include "utils.h"

struct priv {
    struct mpvk_ctx vk;
    MacCommon *vo_mac;
};

static void mac_vk_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    ra_vk_ctx_uninit(ctx);
    mpvk_uninit(&p->vk);
    [p->vo_mac uninit:ctx->vo];
}

static void mac_vk_swap_buffers(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    [p->vo_mac swapBuffer];
}

static void mac_vk_get_vsync(struct ra_ctx *ctx, struct vo_vsync_info *info)
{
    struct priv *p = ctx->priv;
    [p->vo_mac fillVsyncWithInfo:info];
}

static int mac_vk_color_depth(struct ra_ctx *ctx)
{
    return 0;
}

static bool mac_vk_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    struct mpvk_ctx *vk = &p->vk;
    int msgl = ctx->opts.probing ? MSGL_V : MSGL_ERR;

    if (!NSApp) {
        MP_ERR(ctx, "Failed to initialize macvk context, no NSApplication initialized.\n");
        goto error;
    }

    if (!mpvk_init(vk, ctx, VK_EXT_METAL_SURFACE_EXTENSION_NAME))
        goto error;

    p->vo_mac = [[MacCommon alloc] init:ctx->vo];
    if (!p->vo_mac)
        goto error;

    VkMetalSurfaceCreateInfoEXT mac_info = {
        .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
        .pNext = NULL,
        .flags = 0,
        .pLayer = p->vo_mac.layer,
    };

    struct ra_ctx_params params = {
        .swap_buffers = mac_vk_swap_buffers,
        .get_vsync = mac_vk_get_vsync,
        .color_depth = mac_vk_color_depth,
    };

    VkInstance inst = vk->vkinst->instance;
    VkResult res = vkCreateMetalSurfaceEXT(inst, &mac_info, NULL, &vk->surface);
    if (res != VK_SUCCESS) {
        MP_MSG(ctx, msgl, "Failed creating metal surface\n");
        goto error;
    }

    if (!ra_vk_ctx_init(ctx, vk, params, VK_PRESENT_MODE_FIFO_KHR))
        goto error;

    return true;
error:
    if (p->vo_mac)
        [p->vo_mac uninit:ctx->vo];
    return false;
}

static bool resize(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    if (!p->vo_mac.window) {
        return false;
    }
    CGSize size = p->vo_mac.window.framePixel.size;

    return ra_vk_ctx_resize(ctx, (int)size.width, (int)size.height);
}

static bool mac_vk_reconfig(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    if (![p->vo_mac config:ctx->vo])
        return false;
    return true;
}

static int mac_vk_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    struct priv *p = ctx->priv;
    int ret = [p->vo_mac control:ctx->vo events:events request:request data:arg];

    if (*events & VO_EVENT_RESIZE) {
        if (!resize(ctx))
            return VO_ERROR;
    }

    return ret;
}

const struct ra_ctx_fns ra_ctx_vulkan_mac = {
    .type           = "vulkan",
    .name           = "macvk",
    .description    = "mac/Vulkan (via Metal)",
    .reconfig       = mac_vk_reconfig,
    .control        = mac_vk_control,
    .init           = mac_vk_init,
    .uninit         = mac_vk_uninit,
};
