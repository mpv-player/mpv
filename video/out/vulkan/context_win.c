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
#include "video/out/gpu/d3d11_helpers.h"
#include "video/out/w32_common.h"

#include "common.h"
#include "context.h"
#include "utils.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)

struct priv {
    struct mpvk_ctx vk;
    struct mp_dxgi_factory_ctx dxgi_ctx;
};

static void win_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    ra_vk_ctx_uninit(ctx);
    mpvk_uninit(&p->vk);
    vo_w32_uninit(ctx->vo);
    mp_dxgi_factory_uninit(&p->dxgi_ctx);
}

static int color_depth(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    DXGI_OUTPUT_DESC1 desc;
    if (mp_dxgi_output_desc_from_hwnd(&p->dxgi_ctx, vo_w32_hwnd(ctx->vo), &desc))
        return desc.BitsPerColor;

    return -1;
}

static struct pl_color_space preferred_csp(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    DXGI_OUTPUT_DESC1 desc;
    if (mp_dxgi_output_desc_from_hwnd(&p->dxgi_ctx, vo_w32_hwnd(ctx->vo), &desc))
        return mp_dxgi_desc_to_color_space(&desc);

    return (struct pl_color_space){0};
}

static bool win_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    struct mpvk_ctx *vk = &p->vk;
    int msgl = ctx->opts.probing ? MSGL_V : MSGL_ERR;

    if (!mpvk_init(vk, ctx, VK_KHR_WIN32_SURFACE_EXTENSION_NAME))
        goto error;

    if (!vo_w32_init(ctx->vo))
        goto error;

    if (ctx->opts.want_alpha)
        vo_w32_set_transparency(ctx->vo, ctx->opts.want_alpha);

    VkWin32SurfaceCreateInfoKHR wininfo = {
         .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
         .hinstance = HINST_THISCOMPONENT,
         .hwnd = vo_w32_hwnd(ctx->vo),
    };

    struct ra_ctx_params params = {
        .color_depth = color_depth,
        .preferred_csp = preferred_csp,
    };

    VkInstance inst = vk->vkinst->instance;
    VkResult res = vkCreateWin32SurfaceKHR(inst, &wininfo, NULL, &vk->surface);
    if (res != VK_SUCCESS) {
        MP_MSG(ctx, msgl, "Failed creating Windows surface\n");
        goto error;
    }

    if (!ra_vk_ctx_init(ctx, vk, params, VK_PRESENT_MODE_FIFO_KHR))
        goto error;

    return true;

error:
    win_uninit(ctx);
    return false;
}

static bool resize(struct ra_ctx *ctx)
{
    return ra_vk_ctx_resize(ctx, ctx->vo->dwidth, ctx->vo->dheight);
}

static bool win_reconfig(struct ra_ctx *ctx)
{
    vo_w32_config(ctx->vo);
    return resize(ctx);
}

static int win_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    int ret = vo_w32_control(ctx->vo, events, request, arg);
    if (*events & VO_EVENT_RESIZE) {
        if (!resize(ctx))
            return VO_ERROR;
    }
    return ret;
}

static void win_update_render_opts(struct ra_ctx *ctx)
{
    vo_w32_set_transparency(ctx->vo, ctx->opts.want_alpha);
}

const struct ra_ctx_fns ra_ctx_vulkan_win = {
    .type               = "vulkan",
    .name               = "winvk",
    .description        = "Win32/Vulkan",
    .reconfig           = win_reconfig,
    .control            = win_control,
    .update_render_opts = win_update_render_opts,
    .init               = win_init,
    .uninit             = win_uninit,
};
