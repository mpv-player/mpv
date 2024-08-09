/*
 * Copyright (c) 2022 Philip Langdale <philipl@overt.org>
 *
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

#include "config.h"
#include "video/out/gpu/hwdec.h"
#include "video/out/vulkan/context.h"
#include "video/out/placebo/ra_pl.h"

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>

struct vulkan_hw_priv {
    struct mp_hwdec_ctx hwctx;
    pl_gpu gpu;
};

struct vulkan_mapper_priv {
    struct mp_image layout;
    AVVkFrame *vkf;
    pl_tex tex[4];
};

static void lock_queue(struct AVHWDeviceContext *ctx,
                       uint32_t queue_family, uint32_t index)
{
    pl_vulkan vulkan = ctx->user_opaque;
    vulkan->lock_queue(vulkan, queue_family, index);
}

static void unlock_queue(struct AVHWDeviceContext *ctx,
                         uint32_t queue_family, uint32_t index)
{
    pl_vulkan vulkan = ctx->user_opaque;
    vulkan->unlock_queue(vulkan, queue_family, index);
}

static int vulkan_init(struct ra_hwdec *hw)
{
    AVBufferRef *hw_device_ctx = NULL;
    int ret = 0;
    struct vulkan_hw_priv *p = hw->priv;
    int level = hw->probing ? MSGL_V : MSGL_ERR;

    struct mpvk_ctx *vk = ra_vk_ctx_get(hw->ra_ctx);
    if (!vk) {
        MP_MSG(hw, level, "This is not a libplacebo vulkan gpu api context.\n");
        return 0;
    }

    p->gpu = ra_pl_get(hw->ra_ctx->ra);
    if (!p->gpu) {
        MP_MSG(hw, level, "Failed to obtain pl_gpu.\n");
        return 0;
    }

    /*
     * libplacebo initialises all queues, but we still need to discover which
     * one is the decode queue.
     */
    uint32_t num_qf = 0;
    VkQueueFamilyProperties *qf = NULL;
    vkGetPhysicalDeviceQueueFamilyProperties(vk->vulkan->phys_device, &num_qf, NULL);
    if (!num_qf)
        goto error;

    qf = talloc_array(NULL, VkQueueFamilyProperties, num_qf);
    vkGetPhysicalDeviceQueueFamilyProperties(vk->vulkan->phys_device, &num_qf, qf);

    int decode_index = -1, decode_count = 0;
    for (int i = 0; i < num_qf; i++) {
        /*
         * Pick the first discovered decode queue that we find. Maybe a day will
         * come when this needs to be smarter, but I'm sure a bunch of other
         * things will have to change too.
         */
        if ((qf[i].queueFlags) & VK_QUEUE_VIDEO_DECODE_BIT_KHR) {
            decode_index = i;
            decode_count = qf[i].queueCount;
        }
    }

    hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VULKAN);
    if (!hw_device_ctx)
        goto error;

    AVHWDeviceContext *device_ctx = (void *)hw_device_ctx->data;
    AVVulkanDeviceContext *device_hwctx = device_ctx->hwctx;

    device_ctx->user_opaque = (void *)vk->vulkan;
    device_hwctx->lock_queue = lock_queue;
    device_hwctx->unlock_queue = unlock_queue;
    device_hwctx->get_proc_addr = vk->vkinst->get_proc_addr;
    device_hwctx->inst = vk->vkinst->instance;
    device_hwctx->phys_dev = vk->vulkan->phys_device;
    device_hwctx->act_dev = vk->vulkan->device;
    device_hwctx->device_features = *vk->vulkan->features;
    device_hwctx->enabled_inst_extensions = vk->vkinst->extensions;
    device_hwctx->nb_enabled_inst_extensions = vk->vkinst->num_extensions;
    device_hwctx->enabled_dev_extensions = vk->vulkan->extensions;
    device_hwctx->nb_enabled_dev_extensions = vk->vulkan->num_extensions;
    device_hwctx->queue_family_index = vk->vulkan->queue_graphics.index;
    device_hwctx->nb_graphics_queues = vk->vulkan->queue_graphics.count;
    device_hwctx->queue_family_tx_index = vk->vulkan->queue_transfer.index;
    device_hwctx->nb_tx_queues = vk->vulkan->queue_transfer.count;
    device_hwctx->queue_family_comp_index = vk->vulkan->queue_compute.index;
    device_hwctx->nb_comp_queues = vk->vulkan->queue_compute.count;
    device_hwctx->queue_family_decode_index = decode_index;
    device_hwctx->nb_decode_queues = decode_count;

    ret = av_hwdevice_ctx_init(hw_device_ctx);
    if (ret < 0) {
        MP_MSG(hw, level, "av_hwdevice_ctx_init failed\n");
        goto error;
    }

    p->hwctx = (struct mp_hwdec_ctx) {
        .driver_name = hw->driver->name,
        .av_device_ref = hw_device_ctx,
        .hw_imgfmt = IMGFMT_VULKAN,
    };
    hwdec_devices_add(hw->devs, &p->hwctx);

    talloc_free(qf);
    return 0;

 error:
    talloc_free(qf);
    av_buffer_unref(&hw_device_ctx);
    return -1;
}

static void vulkan_uninit(struct ra_hwdec *hw)
{
    struct vulkan_hw_priv *p = hw->priv;

    hwdec_devices_remove(hw->devs, &p->hwctx);
    av_buffer_unref(&p->hwctx.av_device_ref);
}

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct vulkan_mapper_priv *p = mapper->priv;

    mapper->dst_params = mapper->src_params;
    mapper->dst_params.imgfmt = mapper->src_params.hw_subfmt;
    mapper->dst_params.hw_subfmt = 0;

    mp_image_set_params(&p->layout, &mapper->dst_params);

    struct ra_imgfmt_desc desc = {0};
    if (!ra_get_imgfmt_desc(mapper->ra, mapper->dst_params.imgfmt, &desc))
        return -1;

    return 0;
}

static void mapper_uninit(struct ra_hwdec_mapper *mapper)
{

}

static void mapper_unmap(struct ra_hwdec_mapper *mapper)
{
    struct vulkan_hw_priv *p_owner = mapper->owner->priv;
    struct vulkan_mapper_priv *p = mapper->priv;
    if (!mapper->src)
        goto end;

    AVHWFramesContext *hwfc = (AVHWFramesContext *) mapper->src->hwctx->data;;
    const AVVulkanFramesContext *vkfc = hwfc->hwctx;;
    AVVkFrame *vkf = p->vkf;

    int num_images;
    for (num_images = 0; (vkf->img[num_images] != VK_NULL_HANDLE); num_images++);

    for (int i = 0; (p->tex[i] != NULL); i++) {
        pl_tex *tex = &p->tex[i];
        if (!*tex)
            continue;

        // If we have multiple planes and one image, then that is a multiplane
        // frame. Anything else is treated as one-image-per-plane.
        int index = p->layout.num_planes > 1 && num_images == 1 ? 0 : i;

        // Update AVVkFrame state to reflect current layout
        bool ok = pl_vulkan_hold_ex(p_owner->gpu, pl_vulkan_hold_params(
            .tex = *tex,
            .out_layout = &vkf->layout[index],
            .qf = VK_QUEUE_FAMILY_IGNORED,
            .semaphore = (pl_vulkan_sem) {
                .sem = vkf->sem[index],
                .value = vkf->sem_value[index] + 1,
            },
        ));

        vkf->access[index] = 0;
        vkf->sem_value[index] += !!ok;
        *tex = NULL;
    }

    vkfc->unlock_frame(hwfc, vkf);

 end:
    for (int i = 0; i < p->layout.num_planes; i++)
        ra_tex_free(mapper->ra, &mapper->tex[i]);

    p->vkf = NULL;
}

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    bool result = false;
    struct vulkan_hw_priv *p_owner = mapper->owner->priv;
    struct vulkan_mapper_priv *p = mapper->priv;
    pl_vulkan vk = pl_vulkan_get(p_owner->gpu);
    if (!vk)
        return -1;

    AVHWFramesContext *hwfc = (AVHWFramesContext *) mapper->src->hwctx->data;
    const AVVulkanFramesContext *vkfc = hwfc->hwctx;
    AVVkFrame *vkf = (AVVkFrame *) mapper->src->planes[0];

    /*
     * We need to use the dimensions from the HW Frames Context for the
     * textures, as the underlying images may be larger than the logical frame
     * size. This most often happens with 1080p content where the actual frame
     * height is 1088.
     */
    struct mp_image raw_layout;
    mp_image_setfmt(&raw_layout, p->layout.params.imgfmt);
    mp_image_set_size(&raw_layout, hwfc->width, hwfc->height);

    int num_images;
    for (num_images = 0; (vkf->img[num_images] != VK_NULL_HANDLE); num_images++);
    const VkFormat *vk_fmt = av_vkfmt_from_pixfmt(hwfc->sw_format);

    vkfc->lock_frame(hwfc, vkf);

    for (int i = 0; i < p->layout.num_planes; i++) {
        pl_tex *tex = &p->tex[i];
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        int index = i;

        // If we have multiple planes and one image, then that is a multiplane
        // frame. Anything else is treated as one-image-per-plane.
        if (p->layout.num_planes > 1 && num_images == 1) {
            index = 0;

            switch (i) {
            case 0:
                aspect = VK_IMAGE_ASPECT_PLANE_0_BIT_KHR;
                break;
            case 1:
                aspect = VK_IMAGE_ASPECT_PLANE_1_BIT_KHR;
                break;
            case 2:
                aspect = VK_IMAGE_ASPECT_PLANE_2_BIT_KHR;
                break;
            default:
                goto error;
            }
        }

        *tex = pl_vulkan_wrap(p_owner->gpu, pl_vulkan_wrap_params(
            .image = vkf->img[index],
            .width = mp_image_plane_w(&raw_layout, i),
            .height = mp_image_plane_h(&raw_layout, i),
            .format = vk_fmt[i],
            .usage = vkfc->usage,
            .aspect = aspect,
        ));
        if (!*tex)
            goto error;

        pl_vulkan_release_ex(p_owner->gpu, pl_vulkan_release_params(
            .tex = p->tex[i],
            .layout = vkf->layout[index],
            .qf = VK_QUEUE_FAMILY_IGNORED,
            .semaphore = (pl_vulkan_sem) {
                .sem = vkf->sem[index],
                .value = vkf->sem_value[index],
            },
        ));

        struct ra_tex *ratex = talloc_ptrtype(NULL, ratex);
        result = mppl_wrap_tex(mapper->ra, *tex, ratex);
        if (!result) {
            pl_tex_destroy(p_owner->gpu, tex);
            talloc_free(ratex);
            goto error;
        }
        mapper->tex[i] = ratex;
    }

    p->vkf = vkf;
    return 0;

 error:
    vkfc->unlock_frame(hwfc, vkf);
    mapper_unmap(mapper);
    return -1;
}

const struct ra_hwdec_driver ra_hwdec_vulkan = {
    .name = "vulkan",
    .imgfmts = {IMGFMT_VULKAN, 0},
    .device_type = AV_HWDEVICE_TYPE_VULKAN,
    .priv_size = sizeof(struct vulkan_hw_priv),
    .init = vulkan_init,
    .uninit = vulkan_uninit,
    .mapper = &(const struct ra_hwdec_mapper_driver){
        .priv_size = sizeof(struct vulkan_mapper_priv),
        .init = mapper_init,
        .uninit = mapper_uninit,
        .map = mapper_map,
        .unmap = mapper_unmap,
    },
};
