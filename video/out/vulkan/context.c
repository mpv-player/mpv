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
#include "video/out/gpu/spirv.h"

#include "context.h"
#include "ra_vk.h"
#include "utils.h"

enum {
    SWAP_AUTO = 0,
    SWAP_FIFO,
    SWAP_FIFO_RELAXED,
    SWAP_MAILBOX,
    SWAP_IMMEDIATE,
    SWAP_COUNT,
};

struct vulkan_opts {
    struct mpvk_device_opts dev_opts; // logical device options
    char *device; // force a specific GPU
    int swap_mode;
};

static int vk_validate_dev(struct mp_log *log, const struct m_option *opt,
                           struct bstr name, struct bstr param)
{
    int ret = M_OPT_INVALID;
    VkResult res;

    // Create a dummy instance to validate/list the devices
    VkInstanceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    };

    VkInstance inst;
    VkPhysicalDevice *devices = NULL;
    uint32_t num = 0;

    res = vkCreateInstance(&info, MPVK_ALLOCATOR, &inst);
    if (res != VK_SUCCESS)
        goto error;

    res = vkEnumeratePhysicalDevices(inst, &num, NULL);
    if (res != VK_SUCCESS)
        goto error;

    devices = talloc_array(NULL, VkPhysicalDevice, num);
    vkEnumeratePhysicalDevices(inst, &num, devices);
    if (res != VK_SUCCESS)
        goto error;

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
            break;
        }
    }

    if (!help)
        mp_err(log, "No device with name '%.*s'!\n", BSTR_P(param));

error:
    talloc_free(devices);
    return ret;
}

#define OPT_BASE_STRUCT struct vulkan_opts
const struct m_sub_options vulkan_conf = {
    .opts = (const struct m_option[]) {
        OPT_STRING_VALIDATE("vulkan-device", device, 0, vk_validate_dev),
        OPT_CHOICE("vulkan-swap-mode", swap_mode, 0,
                   ({"auto",        SWAP_AUTO},
                   {"fifo",         SWAP_FIFO},
                   {"fifo-relaxed", SWAP_FIFO_RELAXED},
                   {"mailbox",      SWAP_MAILBOX},
                   {"immediate",    SWAP_IMMEDIATE})),
        OPT_INTRANGE("vulkan-queue-count", dev_opts.queue_count, 0, 1, 8,
                     OPTDEF_INT(1)),
        OPT_FLAG("vulkan-async-transfer", dev_opts.async_transfer, 0),
        OPT_FLAG("vulkan-async-compute", dev_opts.async_compute, 0),
        {0}
    },
    .size = sizeof(struct vulkan_opts),
    .defaults = &(struct vulkan_opts) {
        .dev_opts = {
            .async_transfer = 1,
        },
    },
};

struct priv {
    struct mpvk_ctx *vk;
    struct vulkan_opts *opts;
    // Swapchain metadata:
    int w, h;                 // current size
    VkSwapchainCreateInfoKHR protoInfo; // partially filled-in prototype
    VkSwapchainKHR swapchain;
    VkSwapchainKHR old_swapchain;
    int frames_in_flight;
    // state of the images:
    struct ra_tex **images;   // ra_tex wrappers for the vkimages
    int num_images;           // size of images
    VkSemaphore *sems_in;     // pool of semaphores used to synchronize images
    VkSemaphore *sems_out;    // outgoing semaphores (rendering complete)
    int num_sems;
    int idx_sems;             // index of next free semaphore pair
    int last_imgidx;          // the image index last acquired (for submit)
};

static const struct ra_swapchain_fns vulkan_swapchain;

struct mpvk_ctx *ra_vk_ctx_get(struct ra_ctx *ctx)
{
    if (ctx->swapchain->fns != &vulkan_swapchain)
        return NULL;

    struct priv *p = ctx->swapchain->priv;
    return p->vk;
}

static bool update_swapchain_info(struct priv *p,
                                  VkSwapchainCreateInfoKHR *info)
{
    struct mpvk_ctx *vk = p->vk;

    // Query the supported capabilities and update this struct as needed
    VkSurfaceCapabilitiesKHR caps;
    VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->physd, vk->surf, &caps));

    // Sorted by preference
    static const VkCompositeAlphaFlagsKHR alphaModes[] = {
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    };

    for (int i = 0; i < MP_ARRAY_SIZE(alphaModes); i++) {
        if (caps.supportedCompositeAlpha & alphaModes[i]) {
            info->compositeAlpha = alphaModes[i];
            break;
        }
    }

    if (!info->compositeAlpha) {
        MP_ERR(vk, "Failed picking alpha compositing mode (caps: 0x%x)\n",
               caps.supportedCompositeAlpha);
        goto error;
    }

    static const VkSurfaceTransformFlagsKHR rotModes[] = {
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR,
    };

    for (int i = 0; i < MP_ARRAY_SIZE(rotModes); i++) {
        if (caps.supportedTransforms & rotModes[i]) {
            info->preTransform = rotModes[i];
            break;
        }
    }

    if (!info->preTransform) {
        MP_ERR(vk, "Failed picking surface transform mode (caps: 0x%x)\n",
               caps.supportedTransforms);
        goto error;
    }

    // Image count as required
    MP_VERBOSE(vk, "Requested image count: %d (min %d max %d)\n",
               (int)info->minImageCount, (int)caps.minImageCount,
               (int)caps.maxImageCount);

    info->minImageCount = MPMAX(info->minImageCount, caps.minImageCount);
    if (caps.maxImageCount)
        info->minImageCount = MPMIN(info->minImageCount, caps.maxImageCount);

    // Check the extent against the allowed parameters
    if (caps.currentExtent.width != info->imageExtent.width &&
        caps.currentExtent.width != 0xFFFFFFFF)
    {
        MP_WARN(vk, "Requested width %d does not match current width %d\n",
                (int)info->imageExtent.width, (int)caps.currentExtent.width);
        info->imageExtent.width = caps.currentExtent.width;
    }

    if (caps.currentExtent.height != info->imageExtent.height &&
        caps.currentExtent.height != 0xFFFFFFFF)
    {
        MP_WARN(vk, "Requested height %d does not match current height %d\n",
                (int)info->imageExtent.height, (int)caps.currentExtent.height);
        info->imageExtent.height = caps.currentExtent.height;
    }

    if (caps.minImageExtent.width  > info->imageExtent.width ||
        caps.minImageExtent.height > info->imageExtent.height)
    {
        MP_ERR(vk, "Requested size %dx%d smaller than device minimum %d%d\n",
               (int)info->imageExtent.width, (int)info->imageExtent.height,
               (int)caps.minImageExtent.width, (int)caps.minImageExtent.height);
        goto error;
    }

    if (caps.maxImageExtent.width  < info->imageExtent.width ||
        caps.maxImageExtent.height < info->imageExtent.height)
    {
        MP_ERR(vk, "Requested size %dx%d larger than device maximum %d%d\n",
               (int)info->imageExtent.width, (int)info->imageExtent.height,
               (int)caps.maxImageExtent.width, (int)caps.maxImageExtent.height);
        goto error;
    }

    // We just request whatever usage we can, and let the ra_vk decide what
    // ra_tex_params that translates to. This makes the images as flexible
    // as possible.
    info->imageUsage = caps.supportedUsageFlags;
    return true;

error:
    return false;
}

void ra_vk_ctx_uninit(struct ra_ctx *ctx)
{
    if (ctx->ra) {
        struct priv *p = ctx->swapchain->priv;
        struct mpvk_ctx *vk = p->vk;

        mpvk_flush_commands(vk);
        mpvk_poll_commands(vk, UINT64_MAX);

        for (int i = 0; i < p->num_images; i++)
            ra_tex_free(ctx->ra, &p->images[i]);
        for (int i = 0; i < p->num_sems; i++) {
            vkDestroySemaphore(vk->dev, p->sems_in[i], MPVK_ALLOCATOR);
            vkDestroySemaphore(vk->dev, p->sems_out[i], MPVK_ALLOCATOR);
        }

        vkDestroySwapchainKHR(vk->dev, p->swapchain, MPVK_ALLOCATOR);
        ctx->ra->fns->destroy(ctx->ra);
        ctx->ra = NULL;
    }

    talloc_free(ctx->swapchain);
    ctx->swapchain = NULL;
}

static const struct ra_swapchain_fns vulkan_swapchain;

bool ra_vk_ctx_init(struct ra_ctx *ctx, struct mpvk_ctx *vk,
                    VkPresentModeKHR preferred_mode)
{
    struct ra_swapchain *sw = ctx->swapchain = talloc_zero(NULL, struct ra_swapchain);
    sw->ctx = ctx;
    sw->fns = &vulkan_swapchain;

    struct priv *p = sw->priv = talloc_zero(sw, struct priv);
    p->vk = vk;
    p->opts = mp_get_config_group(p, ctx->global, &vulkan_conf);

    if (!mpvk_find_phys_device(vk, p->opts->device, ctx->opts.allow_sw))
        goto error;
    if (!spirv_compiler_init(ctx))
        goto error;
    vk->spirv = ctx->spirv;
    if (!mpvk_pick_surface_format(vk))
        goto error;
    if (!mpvk_device_init(vk, p->opts->dev_opts))
        goto error;

    ctx->ra = ra_create_vk(vk, ctx->log);
    if (!ctx->ra)
        goto error;

    static const VkPresentModeKHR present_modes[SWAP_COUNT] = {
        [SWAP_FIFO]         = VK_PRESENT_MODE_FIFO_KHR,
        [SWAP_FIFO_RELAXED] = VK_PRESENT_MODE_FIFO_RELAXED_KHR,
        [SWAP_MAILBOX]      = VK_PRESENT_MODE_MAILBOX_KHR,
        [SWAP_IMMEDIATE]    = VK_PRESENT_MODE_IMMEDIATE_KHR,
    };

    p->protoInfo = (VkSwapchainCreateInfoKHR) {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vk->surf,
        .imageFormat = vk->surf_format.format,
        .imageColorSpace = vk->surf_format.colorSpace,
        .imageArrayLayers = 1, // non-stereoscopic
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .minImageCount = ctx->opts.swapchain_depth + 1, // +1 for FB
        .presentMode = p->opts->swap_mode ? present_modes[p->opts->swap_mode]
                                          : preferred_mode,
        .clipped = true,
    };

    // Make sure the swapchain present mode is supported
    int num_modes;
    VK(vkGetPhysicalDeviceSurfacePresentModesKHR(vk->physd, vk->surf,
                                                 &num_modes, NULL));
    VkPresentModeKHR *modes = talloc_array(NULL, VkPresentModeKHR, num_modes);
    VK(vkGetPhysicalDeviceSurfacePresentModesKHR(vk->physd, vk->surf,
                                                 &num_modes, modes));
    bool supported = false;
    for (int i = 0; i < num_modes; i++)
        supported |= (modes[i] == p->protoInfo.presentMode);
    talloc_free(modes);

    if (!supported) {
        MP_ERR(ctx, "Requested swap mode unsupported by this device!\n");
        goto error;
    }

    return true;

error:
    ra_vk_ctx_uninit(ctx);
    return false;
}

static void destroy_swapchain(struct mpvk_ctx *vk, struct priv *p)
{
    assert(p->old_swapchain);
    vkDestroySwapchainKHR(vk->dev, p->old_swapchain, MPVK_ALLOCATOR);
    p->old_swapchain = NULL;
}

bool ra_vk_ctx_resize(struct ra_swapchain *sw, int w, int h)
{
    struct priv *p = sw->priv;
    if (w == p->w && h == p->h)
        return true;

    struct ra *ra = sw->ctx->ra;
    struct mpvk_ctx *vk = p->vk;
    VkImage *vkimages = NULL;

    // It's invalid to trigger another swapchain recreation while there's
    // more than one swapchain already active, so we need to flush any pending
    // asynchronous swapchain release operations that may be ongoing.
    while (p->old_swapchain)
        mpvk_poll_commands(vk, 100000); // 100μs

    VkSwapchainCreateInfoKHR sinfo = p->protoInfo;
    sinfo.imageExtent  = (VkExtent2D){ w, h };
    sinfo.oldSwapchain = p->swapchain;

    if (!update_swapchain_info(p, &sinfo))
        goto error;

    VK(vkCreateSwapchainKHR(vk->dev, &sinfo, MPVK_ALLOCATOR, &p->swapchain));
    p->w = w;
    p->h = h;

    // Freeing the old swapchain while it's still in use is an error, so do
    // it asynchronously once the device is idle.
    if (sinfo.oldSwapchain) {
        p->old_swapchain = sinfo.oldSwapchain;
        vk_dev_callback(vk, (vk_cb) destroy_swapchain, vk, p);
    }

    // Get the new swapchain images
    int num;
    VK(vkGetSwapchainImagesKHR(vk->dev, p->swapchain, &num, NULL));
    vkimages = talloc_array(NULL, VkImage, num);
    VK(vkGetSwapchainImagesKHR(vk->dev, p->swapchain, &num, vkimages));

    // If needed, allocate some more semaphores
    while (num > p->num_sems) {
        VkSemaphore sem_in, sem_out;
        static const VkSemaphoreCreateInfo seminfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        VK(vkCreateSemaphore(vk->dev, &seminfo, MPVK_ALLOCATOR, &sem_in));
        VK(vkCreateSemaphore(vk->dev, &seminfo, MPVK_ALLOCATOR, &sem_out));

        int idx = p->num_sems++;
        MP_TARRAY_GROW(p, p->sems_in, idx);
        MP_TARRAY_GROW(p, p->sems_out, idx);
        p->sems_in[idx] = sem_in;
        p->sems_out[idx] = sem_out;
    }

    // Recreate the ra_tex wrappers
    for (int i = 0; i < p->num_images; i++)
        ra_tex_free(ra, &p->images[i]);

    p->num_images = num;
    MP_TARRAY_GROW(p, p->images, p->num_images);
    for (int i = 0; i < num; i++) {
        p->images[i] = ra_vk_wrap_swapchain_img(ra, vkimages[i], sinfo);
        if (!p->images[i])
            goto error;
    }

    talloc_free(vkimages);
    return true;

error:
    talloc_free(vkimages);
    vkDestroySwapchainKHR(vk->dev, p->swapchain, MPVK_ALLOCATOR);
    p->swapchain = NULL;
    return false;
}

static int color_depth(struct ra_swapchain *sw)
{
    struct priv *p = sw->priv;
    int bits = 0;

    if (!p->num_images)
        return bits;

    // The channel with the most bits is probably the most authoritative about
    // the actual color information (consider e.g. a2bgr10). Slight downside
    // in that it results in rounding r/b for e.g. rgb565, but we don't pick
    // surfaces with fewer than 8 bits anyway.
    const struct ra_format *fmt = p->images[0]->params.format;
    for (int i = 0; i < fmt->num_components; i++) {
        int depth = fmt->component_depth[i];
        bits = MPMAX(bits, depth ? depth : fmt->component_size[i]);
    }

    return bits;
}

static bool start_frame(struct ra_swapchain *sw, struct ra_fbo *out_fbo)
{
    struct priv *p = sw->priv;
    struct mpvk_ctx *vk = p->vk;
    if (!p->swapchain)
        return false;

    VkSemaphore sem_in = p->sems_in[p->idx_sems];
    MP_TRACE(vk, "vkAcquireNextImageKHR signals %p\n", (void *)sem_in);

    for (int attempts = 0; attempts < 2; attempts++) {
        uint32_t imgidx = 0;
        VkResult res = vkAcquireNextImageKHR(vk->dev, p->swapchain, UINT64_MAX,
                                             sem_in, NULL, &imgidx);

        switch (res) {
        case VK_SUCCESS:
            p->last_imgidx = imgidx;
            *out_fbo = (struct ra_fbo) {
                .tex = p->images[imgidx],
                .flip = false,
            };
            ra_tex_vk_external_dep(sw->ctx->ra, out_fbo->tex, sem_in);
            return true;

        case VK_ERROR_OUT_OF_DATE_KHR: {
            // In these cases try recreating the swapchain
            int w = p->w, h = p->h;
            p->w = p->h = 0; // invalidate the current state
            if (!ra_vk_ctx_resize(sw, w, h))
                return false;
            continue;
        }

        default:
            MP_ERR(vk, "Failed acquiring swapchain image: %s\n", vk_err(res));
            return false;
        }
    }

    // If we've exhausted the number of attempts to recreate the swapchain,
    // just give up silently.
    return false;
}

static void present_cb(struct priv *p, void *arg)
{
    p->frames_in_flight--;
}

static bool submit_frame(struct ra_swapchain *sw, const struct vo_frame *frame)
{
    struct priv *p = sw->priv;
    struct ra *ra = sw->ctx->ra;
    struct mpvk_ctx *vk = p->vk;
    if (!p->swapchain)
        return false;

    struct vk_cmd *cmd = ra_vk_submit(ra, p->images[p->last_imgidx]);
    if (!cmd)
        return false;

    VkSemaphore sem_out = p->sems_out[p->idx_sems++];
    p->idx_sems %= p->num_sems;
    vk_cmd_sig(cmd, sem_out);

    p->frames_in_flight++;
    vk_cmd_callback(cmd, (vk_cb) present_cb, p, NULL);

    vk_cmd_queue(vk, cmd);
    if (!mpvk_flush_commands(vk))
        return false;

    // Older nvidia drivers can spontaneously combust when submitting to the
    // same queue as we're rendering from, in a multi-queue scenario. Safest
    // option is to flush the commands first and then submit to the next queue.
    // We can drop this hack in the future, I suppose.
    struct vk_cmdpool *pool = vk->pool_graphics;
    VkQueue queue = pool->queues[pool->idx_queues];

    VkPresentInfoKHR pinfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &sem_out,
        .swapchainCount = 1,
        .pSwapchains = &p->swapchain,
        .pImageIndices = &p->last_imgidx,
    };

    MP_TRACE(vk, "vkQueuePresentKHR waits on %p\n", (void *)sem_out);
    VkResult res = vkQueuePresentKHR(queue, &pinfo);
    switch (res) {
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
        return true;

    case VK_ERROR_OUT_OF_DATE_KHR:
        // We can silently ignore this error, since the next start_frame will
        // recreate the swapchain automatically.
        return true;

    default:
        MP_ERR(vk, "Failed presenting to queue %p: %s\n", (void *)queue,
               vk_err(res));
        return false;
    }
}

static void swap_buffers(struct ra_swapchain *sw)
{
    struct priv *p = sw->priv;

    while (p->frames_in_flight >= sw->ctx->opts.swapchain_depth)
        mpvk_poll_commands(p->vk, 100000); // 100μs
}

static const struct ra_swapchain_fns vulkan_swapchain = {
    .color_depth   = color_depth,
    .start_frame   = start_frame,
    .submit_frame  = submit_frame,
    .swap_buffers  = swap_buffers,
};
