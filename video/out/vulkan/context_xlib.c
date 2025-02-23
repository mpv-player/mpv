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
#include "video/out/present_sync.h"
#include "video/out/x11_common.h"

#include "common.h"
#include "context.h"
#include "utils.h"

struct priv {
    struct mpvk_ctx vk;
};

static bool xlib_check_visible(struct ra_ctx *ctx)
{
    return vo_x11_check_visible(ctx->vo);
}

static void xlib_vk_swap_buffers(struct ra_ctx *ctx)
{
    if (ctx->vo->x11->use_present)
        present_sync_swap(ctx->vo->x11->present);
}

static void xlib_vk_get_vsync(struct ra_ctx *ctx, struct vo_vsync_info *info)
{
    struct vo_x11_state *x11 = ctx->vo->x11;
    if (ctx->vo->x11->use_present)
        present_sync_get_info(x11->present, info);
}

static void xlib_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    ra_vk_ctx_uninit(ctx);
    mpvk_uninit(&p->vk);
    vo_x11_uninit(ctx->vo);
}

static bool xlib_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    struct mpvk_ctx *vk = &p->vk;
    int msgl = ctx->opts.probing ? MSGL_V : MSGL_ERR;

    if (!mpvk_init(vk, ctx, VK_KHR_XLIB_SURFACE_EXTENSION_NAME))
        goto error;

    if (!vo_x11_init(ctx->vo))
        goto error;

    if (!vo_x11_create_vo_window(ctx->vo, NULL, "mpvk"))
        goto error;

    VkXlibSurfaceCreateInfoKHR xinfo = {
         .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
         .dpy = ctx->vo->x11->display,
         .window = ctx->vo->x11->window,
    };

    struct ra_ctx_params params = {
        .check_visible = xlib_check_visible,
        .swap_buffers = xlib_vk_swap_buffers,
        .get_vsync = xlib_vk_get_vsync,
    };

    VkInstance inst = vk->vkinst->instance;
    VkResult res = vkCreateXlibSurfaceKHR(inst, &xinfo, NULL, &vk->surface);
    if (res != VK_SUCCESS) {
        MP_MSG(ctx, msgl, "Failed creating Xlib surface\n");
        goto error;
    }

    if (!ra_vk_ctx_init(ctx, vk, params, VK_PRESENT_MODE_FIFO_KHR))
        goto error;

    ra_add_native_resource(ctx->ra, "x11", ctx->vo->x11->display);

    return true;

error:
    xlib_uninit(ctx);
    return false;
}

static bool resize(struct ra_ctx *ctx)
{
    return ra_vk_ctx_resize(ctx, ctx->vo->dwidth, ctx->vo->dheight);
}

static bool xlib_reconfig(struct ra_ctx *ctx)
{
    vo_x11_config_vo_window(ctx->vo);
    return resize(ctx);
}

static int xlib_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    int ret = vo_x11_control(ctx->vo, events, request, arg);
    if (*events & VO_EVENT_RESIZE) {
        if (!resize(ctx))
            return VO_ERROR;
    }
    return ret;
}

static void xlib_wakeup(struct ra_ctx *ctx)
{
    vo_x11_wakeup(ctx->vo);
}

static void xlib_wait_events(struct ra_ctx *ctx, int64_t until_time_ns)
{
    vo_x11_wait_events(ctx->vo, until_time_ns);
}

const struct ra_ctx_fns ra_ctx_vulkan_xlib = {
    .type           = "vulkan",
    .name           = "x11vk",
    .description    = "X11/Vulkan",
    .reconfig       = xlib_reconfig,
    .control        = xlib_control,
    .wakeup         = xlib_wakeup,
    .wait_events    = xlib_wait_events,
    .init           = xlib_init,
    .uninit         = xlib_uninit,
};
