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
#include "video/out/wayland_common.h"

#include "common.h"
#include "context.h"
#include "utils.h"

struct priv {
    struct mpvk_ctx vk;
};

static void wayland_vk_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    ra_vk_ctx_uninit(ctx);
    mpvk_uninit(&p->vk);
    vo_wayland_uninit(ctx->vo);
}

static bool wayland_vk_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    struct mpvk_ctx *vk = &p->vk;
    int msgl = ctx->opts.probing ? MSGL_V : MSGL_ERR;

    if (!mpvk_instance_init(vk, ctx->log, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
                            ctx->opts.debug))
        goto error;

    if (!vo_wayland_init(ctx->vo))
        goto error;

    VkWaylandSurfaceCreateInfoKHR wlinfo = {
         .sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
         .display = ctx->vo->wl->display,
         .surface = ctx->vo->wl->surface,
    };

    VkResult res = vkCreateWaylandSurfaceKHR(vk->inst, &wlinfo, MPVK_ALLOCATOR,
                                             &vk->surf);
    if (res != VK_SUCCESS) {
        MP_MSG(ctx, msgl, "Failed creating Wayland surface: %s\n", vk_err(res));
        goto error;
    }

    /* Because in Wayland clients render whenever they receive a callback from
     * the compositor, and the fact that the compositor usually stops sending
     * callbacks once the surface is no longer visible, using FIFO here would
     * mean the entire player would block on acquiring swapchain images. Hence,
     * use MAILBOX to guarantee that there'll always be a swapchain image and
     * the player won't block waiting on those */
    if (!ra_vk_ctx_init(ctx, vk, VK_PRESENT_MODE_MAILBOX_KHR))
        goto error;

    return true;

error:
    wayland_vk_uninit(ctx);
    return false;
}

static void resize(struct ra_ctx *ctx)
{
    struct vo_wayland_state *wl = ctx->vo->wl;

    MP_VERBOSE(wl, "Handling resize on the vk side\n");

    const int32_t width = wl->scaling*mp_rect_w(wl->geometry);
    const int32_t height = wl->scaling*mp_rect_h(wl->geometry);

    wl_surface_set_buffer_scale(wl->surface, wl->scaling);

    wl->vo->dwidth  = width;
    wl->vo->dheight = height;
}

static bool wayland_vk_reconfig(struct ra_ctx *ctx)
{
    if (!vo_wayland_reconfig(ctx->vo))
        return false;

    return true;
}

static int wayland_vk_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    int ret = vo_wayland_control(ctx->vo, events, request, arg);
    if (*events & VO_EVENT_RESIZE) {
        resize(ctx);
        if (ra_vk_ctx_resize(ctx->swapchain, ctx->vo->dwidth, ctx->vo->dheight))
            return VO_ERROR;
    }
    return ret;
}

static void wayland_vk_wakeup(struct ra_ctx *ctx)
{
    vo_wayland_wakeup(ctx->vo);
}

static void wayland_vk_wait_events(struct ra_ctx *ctx, int64_t until_time_us)
{
    vo_wayland_wait_events(ctx->vo, until_time_us);
}

const struct ra_ctx_fns ra_ctx_vulkan_wayland = {
    .type           = "vulkan",
    .name           = "waylandvk",
    .reconfig       = wayland_vk_reconfig,
    .control        = wayland_vk_control,
    .wakeup         = wayland_vk_wakeup,
    .wait_events    = wayland_vk_wait_events,
    .init           = wayland_vk_init,
    .uninit         = wayland_vk_uninit,
};
