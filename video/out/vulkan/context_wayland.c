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

static void wayland_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    ra_vk_ctx_uninit(ctx);
    mpvk_uninit(&p->vk);
    vo_wayland_uninit(ctx->vo);
}

static bool wayland_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    struct mpvk_ctx *vk = &p->vk;
    int msgl = ctx->opts.probing ? MSGL_V : MSGL_ERR;

    if (!mpvk_instance_init(vk, ctx->log, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
                            ctx->opts.debug))
        goto error;

    if (!vo_wayland_init(ctx->vo))
        goto error;

    if (!vo_wayland_config(ctx->vo))
        goto error;

    VkWaylandSurfaceCreateInfoKHR wlinfo = {
         .sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
         .display = ctx->vo->wayland->display.display,
         .surface = ctx->vo->wayland->window.video_surface,
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
    wayland_uninit(ctx);
    return false;
}

static bool resize(struct ra_ctx *ctx)
{
    struct vo_wayland_state *wl = ctx->vo->wayland;
    int32_t width = wl->window.sh_width;
    int32_t height = wl->window.sh_height;
    int32_t scale = 1;

    if (wl->display.current_output)
        scale = wl->display.current_output->scale;

    MP_VERBOSE(wl, "resizing %dx%d -> %dx%d\n", wl->window.width,
                                                wl->window.height,
                                                width,
                                                height);

    wl_surface_set_buffer_scale(wl->window.video_surface, scale);
    int err = ra_vk_ctx_resize(ctx->swapchain, scale*width, scale*height);

    wl->window.width = width;
    wl->window.height = height;

    wl->vo->dwidth  = scale*wl->window.width;
    wl->vo->dheight = scale*wl->window.height;
    wl->vo->want_redraw = true;

    return err;
}

static bool wayland_reconfig(struct ra_ctx *ctx)
{
    vo_wayland_config(ctx->vo);
    return resize(ctx);
}

static int wayland_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    int ret = vo_wayland_control(ctx->vo, events, request, arg);
    if (*events & VO_EVENT_RESIZE) {
        if (!resize(ctx))
            return VO_ERROR;
    }
    return ret;
}

static void wayland_wakeup(struct ra_ctx *ctx)
{
    vo_wayland_wakeup(ctx->vo);
}

static void wayland_wait_events(struct ra_ctx *ctx, int64_t until_time_us)
{
    vo_wayland_wait_events(ctx->vo, until_time_us);
}

const struct ra_ctx_fns ra_ctx_vulkan_wayland = {
    .type           = "vulkan",
    .name           = "wayland",
    .reconfig       = wayland_reconfig,
    .control        = wayland_control,
    .wakeup         = wayland_wakeup,
    .wait_events    = wayland_wait_events,
    .init           = wayland_init,
    .uninit         = wayland_uninit,
};
