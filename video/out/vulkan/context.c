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
#include "video/out/placebo/ra_pl.h"

#include "context.h"
#include "utils.h"

struct vulkan_opts {
    char *device; // force a specific GPU
    int swap_mode;
    int queue_count;
    int async_transfer;
    int async_compute;
    int disable_events;
};

static int vk_validate_dev(struct mp_log *log, const struct m_option *opt,
                           struct bstr name, const char **value)
{
    struct bstr param = bstr0(*value);
    int ret = M_OPT_INVALID;
    VkResult res;

    // Create a dummy instance to validate/list the devices
    VkInstanceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    };

    VkInstance inst;
    VkPhysicalDevice *devices = NULL;
    uint32_t num = 0;

    res = vkCreateInstance(&info, NULL, &inst);
    if (res != VK_SUCCESS)
        goto done;

    res = vkEnumeratePhysicalDevices(inst, &num, NULL);
    if (res != VK_SUCCESS)
        goto done;

    devices = talloc_array(NULL, VkPhysicalDevice, num);
    vkEnumeratePhysicalDevices(inst, &num, devices);
    if (res != VK_SUCCESS)
        goto done;

    bool help = bstr_equals0(param, "help");
    if (help) {
        mp_info(log, "Available vulkan devices:\n");
        ret = M_OPT_EXIT;
    }

    for (int i = 0; i < num; i++) {
        VkPhysicalDeviceProperties prop;
        vkGetPhysicalDeviceProperties(devices[i], &prop);

        if (help) {
            mp_info(log, "  '%s' (GPU %d, ID %x:%x)\n", prop.deviceName, i,
                    (unsigned)prop.vendorID, (unsigned)prop.deviceID);
        } else if (bstr_equals0(param, prop.deviceName)) {
            ret = 0;
            goto done;
        }
    }

    if (!help)
        mp_err(log, "No device with name '%.*s'!\n", BSTR_P(param));

done:
    talloc_free(devices);
    return ret;
}

#define OPT_BASE_STRUCT struct vulkan_opts
const struct m_sub_options vulkan_conf = {
    .opts = (const struct m_option[]) {
        {"vulkan-device", OPT_STRING_VALIDATE(device, vk_validate_dev)},
        {"vulkan-swap-mode", OPT_CHOICE(swap_mode,
            {"auto",        -1},
            {"fifo",         VK_PRESENT_MODE_FIFO_KHR},
            {"fifo-relaxed", VK_PRESENT_MODE_FIFO_RELAXED_KHR},
            {"mailbox",      VK_PRESENT_MODE_MAILBOX_KHR},
            {"immediate",    VK_PRESENT_MODE_IMMEDIATE_KHR})},
        {"vulkan-queue-count", OPT_INT(queue_count), M_RANGE(1, 8)},
        {"vulkan-async-transfer", OPT_FLAG(async_transfer)},
        {"vulkan-async-compute", OPT_FLAG(async_compute)},
        {"vulkan-disable-events", OPT_FLAG(disable_events)},
        {0}
    },
    .size = sizeof(struct vulkan_opts),
    .defaults = &(struct vulkan_opts) {
        .swap_mode = -1,
        .queue_count = 1,
        .async_transfer = true,
        .async_compute = true,
    },
};

struct priv {
    struct mpvk_ctx *vk;
    struct vulkan_opts *opts;
    struct ra_vk_ctx_params params;
    const struct pl_swapchain *swapchain;
    struct ra_tex proxy_tex;
};

static const struct ra_swapchain_fns vulkan_swapchain;

struct mpvk_ctx *ra_vk_ctx_get(struct ra_ctx *ctx)
{
    if (ctx->swapchain->fns != &vulkan_swapchain)
        return NULL;

    struct priv *p = ctx->swapchain->priv;
    return p->vk;
}

void ra_vk_ctx_uninit(struct ra_ctx *ctx)
{
    if (!ctx->swapchain)
        return;

    struct priv *p = ctx->swapchain->priv;
    struct mpvk_ctx *vk = p->vk;

    if (ctx->ra) {
        pl_gpu_finish(vk->gpu);
        pl_swapchain_destroy(&p->swapchain);
        ctx->ra->fns->destroy(ctx->ra);
        ctx->ra = NULL;
    }

    vk->gpu = NULL;
    pl_vulkan_destroy(&vk->vulkan);
    TA_FREEP(&ctx->swapchain);
}

bool ra_vk_ctx_init(struct ra_ctx *ctx, struct mpvk_ctx *vk,
                    struct ra_vk_ctx_params params,
                    VkPresentModeKHR preferred_mode)
{
    struct ra_swapchain *sw = ctx->swapchain = talloc_zero(NULL, struct ra_swapchain);
    sw->ctx = ctx;
    sw->fns = &vulkan_swapchain;

    struct priv *p = sw->priv = talloc_zero(sw, struct priv);
    p->vk = vk;
    p->params = params;
    p->opts = mp_get_config_group(p, ctx->global, &vulkan_conf);

    assert(vk->ctx);
    assert(vk->vkinst);
    vk->vulkan = pl_vulkan_create(vk->ctx, &(struct pl_vulkan_params) {
        .instance = vk->vkinst->instance,
        .surface = vk->surface,
        .async_transfer = p->opts->async_transfer,
        .async_compute = p->opts->async_compute,
        .queue_count = p->opts->queue_count,
        .device_name = p->opts->device,
        .disable_events = p->opts->disable_events,
    });
    if (!vk->vulkan)
        goto error;

    vk->gpu = vk->vulkan->gpu;
    ctx->ra = ra_create_pl(vk->gpu, ctx->log);
    if (!ctx->ra)
        goto error;

    // Create the swapchain
    struct pl_vulkan_swapchain_params pl_params = {
        .surface = vk->surface,
        .present_mode = preferred_mode,
        .swapchain_depth = ctx->vo->opts->swapchain_depth,
        // mpv already handles resize events, so gracefully allow suboptimal
        // swapchains to exist in order to make resizing even smoother
        .allow_suboptimal = true,
    };

    if (p->opts->swap_mode >= 0) // user override
        pl_params.present_mode = p->opts->swap_mode;

    p->swapchain = pl_vulkan_create_swapchain(vk->vulkan, &pl_params);
    if (!p->swapchain)
        goto error;

    return true;

error:
    ra_vk_ctx_uninit(ctx);
    return false;
}

bool ra_vk_ctx_resize(struct ra_ctx *ctx, int width, int height)
{
    struct priv *p = ctx->swapchain->priv;

    bool ok = pl_swapchain_resize(p->swapchain, &width, &height);
    ctx->vo->dwidth = width;
    ctx->vo->dheight = height;

    return ok;
}

char *ra_vk_ctx_get_device_name(struct ra_ctx *ctx)
{
    /*
     * This implementation is a bit odd because it has to work even if the
     * ctx hasn't been initialised yet. A context implementation may need access
     * to the device name before it can fully initialise the ctx.
     */
    struct vulkan_opts *opts = mp_get_config_group(NULL, ctx->global,
                                                   &vulkan_conf);
    char *device_name = talloc_strdup(NULL, opts->device);
    talloc_free(opts);
    return device_name;
}

static int color_depth(struct ra_swapchain *sw)
{
    return 0; // TODO: implement this somehow?
}

static bool start_frame(struct ra_swapchain *sw, struct ra_fbo *out_fbo)
{
    struct priv *p = sw->priv;
    struct pl_swapchain_frame frame;
    bool start = true;
    if (p->params.start_frame)
        start = p->params.start_frame(sw->ctx);
    if (!start)
        return false;
    if (!pl_swapchain_start_frame(p->swapchain, &frame))
        return false;
    if (!mppl_wrap_tex(sw->ctx->ra, frame.fbo, &p->proxy_tex))
        return false;

    *out_fbo = (struct ra_fbo) {
        .tex = &p->proxy_tex,
        .flip = frame.flipped,
    };

    return true;
}

static bool submit_frame(struct ra_swapchain *sw, const struct vo_frame *frame)
{
    struct priv *p = sw->priv;
    return pl_swapchain_submit_frame(p->swapchain);
}

static void swap_buffers(struct ra_swapchain *sw)
{
    struct priv *p = sw->priv;
    pl_swapchain_swap_buffers(p->swapchain);
    if (p->params.swap_buffers)
        p->params.swap_buffers(sw->ctx);
}

static void get_vsync(struct ra_swapchain *sw,
                      struct vo_vsync_info *info)
{
    struct priv *p = sw->priv;
    if (p->params.get_vsync)
        p->params.get_vsync(sw->ctx, info);
}

static const struct ra_swapchain_fns vulkan_swapchain = {
    .color_depth   = color_depth,
    .start_frame   = start_frame,
    .submit_frame  = submit_frame,
    .swap_buffers  = swap_buffers,
    .get_vsync     = get_vsync,
};
