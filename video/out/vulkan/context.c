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

#include "config.h"

#include <libavutil/uuid.h>

#include "options/m_config.h"
#include "video/out/placebo/ra_pl.h"
#include "video/out/placebo/utils.h"

#include "context.h"

struct vulkan_opts {
    char *device; // force a specific GPU
    int swap_mode;
    int queue_count;
    bool async_transfer;
    bool async_compute;
};

static inline OPT_STRING_VALIDATE_FUNC(vk_validate_dev)
{
    int ret = M_OPT_INVALID;
    void *ta_ctx = talloc_new(NULL);
    pl_log pllog = mppl_log_create(ta_ctx, log);
    if (!pllog)
        goto done;

    // Create a dummy instance to validate/list the devices
    mppl_log_set_probing(pllog, true);
    pl_vk_inst inst = pl_vk_inst_create(pllog, pl_vk_inst_params());
    mppl_log_set_probing(pllog, false);
    if (!inst)
        goto done;

    uint32_t num = 0;
    VkResult res = vkEnumeratePhysicalDevices(inst->instance, &num, NULL);
    if (res != VK_SUCCESS)
        goto done;

    VkPhysicalDevice *devices = talloc_array(ta_ctx, VkPhysicalDevice, num);
    res = vkEnumeratePhysicalDevices(inst->instance, &num, devices);
    if (res != VK_SUCCESS)
        goto done;

    struct bstr param = bstr0(*value);
    bool help = bstr_equals0(param, "help");
    if (help) {
        mp_info(log, "Available vulkan devices:\n");
        ret = M_OPT_EXIT;
    }

    AVUUID param_uuid;
    bool is_uuid = av_uuid_parse(*value, param_uuid) == 0;

    for (int i = 0; i < num; i++) {
        VkPhysicalDeviceIDPropertiesKHR id_prop = { 0 };
        id_prop.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES_KHR;

        VkPhysicalDeviceProperties2KHR prop2 = { 0 };
        prop2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
        prop2.pNext = &id_prop;

        vkGetPhysicalDeviceProperties2(devices[i], &prop2);

        const VkPhysicalDeviceProperties *prop = &prop2.properties;

        if (help) {
            char device_uuid[37];
            av_uuid_unparse(id_prop.deviceUUID, device_uuid);
            mp_info(log, "  '%s' (GPU %d, PCI ID %x:%x, UUID %s)\n",
                    prop->deviceName, i, (unsigned)prop->vendorID,
                    (unsigned)prop->deviceID, device_uuid);
        } else if (bstr_equals0(param, prop->deviceName)) {
            ret = 0;
            goto done;
        } else if (is_uuid && av_uuid_equal(param_uuid, id_prop.deviceUUID)) {
            ret = 0;
            goto done;
        }
    }

    if (!help)
        mp_err(log, "No device with %s '%.*s'!\n", is_uuid ? "UUID" : "name",
               BSTR_P(param));

done:
    pl_vk_inst_destroy(&inst);
    pl_log_destroy(&pllog);
    talloc_free(ta_ctx);
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
        {"vulkan-async-transfer", OPT_BOOL(async_transfer)},
        {"vulkan-async-compute", OPT_BOOL(async_compute)},
        {0}
    },
    .size = sizeof(struct vulkan_opts),
    .defaults = &(struct vulkan_opts) {
        .swap_mode = -1,
        .queue_count = 1,
        .async_transfer = true,
        .async_compute = true,
    },
    .change_flags = UPDATE_VO,
};

struct priv {
    struct mpvk_ctx *vk;
    struct vulkan_opts *opts;
    struct ra_ctx_params params;
    struct ra_tex proxy_tex;
};

static const struct ra_swapchain_fns vulkan_swapchain;

struct mpvk_ctx *ra_vk_ctx_get(struct ra_ctx *ctx)
{
    if (!ctx->swapchain || ctx->swapchain->fns != &vulkan_swapchain)
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
        pl_swapchain_destroy(&vk->swapchain);
        ctx->ra->fns->destroy(ctx->ra);
        ctx->ra = NULL;
    }

    vk->gpu = NULL;
    pl_vulkan_destroy(&vk->vulkan);
    TA_FREEP(&ctx->swapchain);
}

pl_vulkan mppl_create_vulkan(struct vulkan_opts *opts,
                             pl_vk_inst vkinst,
                             pl_log pllog,
                             VkSurfaceKHR surface,
                             bool allow_software)
{
    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    };

    /*
     * Request the additional extensions and features required to make full use
     * of the ffmpeg Vulkan hwcontext and video decoding capability.
     */
    const char *opt_extensions[] = {
        VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
        VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME,
        VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME,
        VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME,
        VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
        VK_KHR_ZERO_INITIALIZE_WORKGROUP_MEMORY_EXTENSION_NAME,
        /*
         * Extensions below this point are newer than our minimum required Vulkan
         * headers and so we only activate them if the build time headers contain
         * them.
         */
#ifdef VK_EXT_shader_object
        VK_EXT_SHADER_OBJECT_EXTENSION_NAME, /* 1.3.246 */
#endif
#ifdef VK_KHR_cooperative_matrix
        VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME, /* 1.3.255 */
#endif
#ifdef VK_KHR_video_maintenance1
        VK_KHR_VIDEO_MAINTENANCE_1_EXTENSION_NAME, /* 1.3.274 */
#endif
#ifdef VK_KHR_shader_expect_assume
        VK_KHR_SHADER_EXPECT_ASSUME_EXTENSION_NAME, /* 1.3.276 */
#endif
#ifdef VK_KHR_shader_subgroup_rotate
        VK_KHR_SHADER_SUBGROUP_ROTATE_EXTENSION_NAME, /* 1.3.276 */
#endif
        /*
         * Because the AV1 extension does not require a feature flag to be enabled,
         * we can include it as a string literal and have it work.
         */
        "VK_KHR_video_decode_av1", /* VK_KHR_VIDEO_DECODE_AV1_EXTENSION_NAME 1.3.277 */
#ifdef VK_KHR_shader_relaxed_extended_instruction
        VK_KHR_SHADER_RELAXED_EXTENDED_INSTRUCTION_EXTENSION_NAME, /* 1.3.288 */
#endif
#ifdef VK_KHR_video_maintenance2
        VK_KHR_VIDEO_MAINTENANCE_2_EXTENSION_NAME, /* 1.4.306 */
#endif
#ifdef VK_KHR_video_decode_vp9
        VK_KHR_VIDEO_DECODE_VP9_EXTENSION_NAME, /* 1.4.317 */
#endif
    };

    /*
     * The following chain of conditional extension features must be constructed
     * in descending vulkan version order. This ensures that if one is present,
     * the next extension that chains on to it will also be present.
     */

#ifdef VK_KHR_video_decode_vp9
     VkPhysicalDeviceVideoDecodeVP9FeaturesKHR video_decode_vp9_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_DECODE_VP9_FEATURES_KHR,
        .videoDecodeVP9 = true,
    };
#endif

#ifdef VK_KHR_video_maintenance2
    VkPhysicalDeviceVideoMaintenance2FeaturesKHR video_maintenance_2_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_MAINTENANCE_2_FEATURES_KHR,
#ifdef VK_KHR_video_decode_vp9
        .pNext = &video_decode_vp9_feature,
#endif
        .videoMaintenance2 = true,
    };
#endif

#ifdef VK_KHR_shader_relaxed_extended_instruction
    VkPhysicalDeviceShaderRelaxedExtendedInstructionFeaturesKHR shader_relaxed_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_RELAXED_EXTENDED_INSTRUCTION_FEATURES_KHR,
#ifdef VK_KHR_video_maintenance2
        .pNext = &video_maintenance_2_feature,
#endif
        .shaderRelaxedExtendedInstruction = true,
    };
#endif

#ifdef VK_KHR_shader_subgroup_rotate
    VkPhysicalDeviceShaderSubgroupRotateFeaturesKHR shader_subgroup_rotate_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_ROTATE_FEATURES_KHR,
#ifdef VK_KHR_shader_relaxed_extended_instruction
        .pNext = &shader_relaxed_feature,
#endif
       .shaderSubgroupRotate = true,
    };
#endif

#ifdef VK_KHR_shader_expect_assume
    VkPhysicalDeviceShaderExpectAssumeFeaturesKHR shader_expect_assume_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_EXPECT_ASSUME_FEATURES_KHR,
#ifdef VK_KHR_shader_subgroup_rotate
        .pNext = &shader_subgroup_rotate_feature,
#endif
       .shaderExpectAssume = true,
    };
#endif

#ifdef VK_KHR_video_maintenance1
    VkPhysicalDeviceVideoMaintenance1FeaturesKHR video_maintenance_1_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_MAINTENANCE_1_FEATURES_KHR,
#ifdef VK_KHR_shader_expect_assume
        .pNext = &shader_expect_assume_feature,
#endif
        .videoMaintenance1 = true,
    };
#endif

#ifdef VK_KHR_cooperative_matrix
     VkPhysicalDeviceCooperativeMatrixFeaturesKHR cooperative_matrix_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR,
#ifdef VK_KHR_video_maintenance1
        .pNext = &video_maintenance_1_feature,
#endif
        .cooperativeMatrix = true,
    };
#endif

#ifdef VK_EXT_shader_object
    VkPhysicalDeviceShaderObjectFeaturesEXT shader_object_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT,
#ifdef VK_KHR_cooperative_matrix
        .pNext = &cooperative_matrix_feature,
#endif
        .shaderObject = true,
    };
#endif

    VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeaturesKHR zero_init_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ZERO_INITIALIZE_WORKGROUP_MEMORY_FEATURES_KHR,
#ifdef VK_EXT_shader_object
        .pNext = &shader_object_feature,
#endif
       .shaderZeroInitializeWorkgroupMemory = true,
    };

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .pNext = &zero_init_feature,
       .dynamicRendering = true,
    };

    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptor_buffer_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
        .pNext = &dynamic_rendering_feature,
        .descriptorBuffer = true,
        .descriptorBufferPushDescriptors = true,
    };

    VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomic_float_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT,
        .pNext = &descriptor_buffer_feature,
        .shaderBufferFloat32Atomics = true,
        .shaderBufferFloat32AtomicAdd = true,
    };

    VkPhysicalDeviceVulkan12Features recommended_vk12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &atomic_float_feature,
        .scalarBlockLayout = true,
        .shaderBufferInt64Atomics = true,
        .uniformBufferStandardLayout = true,
    };

    VkPhysicalDeviceVulkan11Features recommended_vk11 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = &recommended_vk12,
        .storageBuffer16BitAccess = true,
        .uniformAndStorageBuffer16BitAccess = true,
    };

    VkPhysicalDeviceFeatures2 recommended_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &recommended_vk11,
        .features = {
            .shaderInt16 = true,
            .shaderFloat64 = true,
        },
    };

    features.pNext = &recommended_features;

    AVUUID param_uuid = { 0 };
    bool is_uuid = opts->device &&
                   av_uuid_parse(opts->device, param_uuid) == 0;

    mp_assert(pllog);
    mp_assert(vkinst);
    struct pl_vulkan_params device_params = {
        .instance = vkinst->instance,
        .get_proc_addr = vkinst->get_proc_addr,
        .surface = surface,
        .allow_software = allow_software,
        .async_transfer = opts->async_transfer,
        .async_compute = opts->async_compute,
        .queue_count = opts->queue_count,
        .extra_queues = VK_QUEUE_VIDEO_DECODE_BIT_KHR,
        .opt_extensions = opt_extensions,
        .num_opt_extensions = MP_ARRAY_SIZE(opt_extensions),
        .features = &features,
        .device_name = is_uuid ? NULL : opts->device,
    };
    if (is_uuid)
        av_uuid_copy(device_params.device_uuid, param_uuid);

    return pl_vulkan_create(pllog, &device_params);

}

bool ra_vk_ctx_init(struct ra_ctx *ctx, struct mpvk_ctx *vk,
                    struct ra_ctx_params params,
                    VkPresentModeKHR preferred_mode)
{
    struct ra_swapchain *sw = ctx->swapchain = talloc_zero(NULL, struct ra_swapchain);
    sw->ctx = ctx;
    sw->fns = &vulkan_swapchain;

    struct priv *p = sw->priv = talloc_zero(sw, struct priv);
    p->vk = vk;
    p->params = params;
    p->opts = mp_get_config_group(p, ctx->global, &vulkan_conf);

    vk->vulkan = mppl_create_vulkan(p->opts, vk->vkinst, vk->pllog, vk->surface,
                                    ctx->opts.allow_sw);
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
    };

    if (p->opts->swap_mode >= 0) // user override
        pl_params.present_mode = p->opts->swap_mode;

    vk->swapchain = pl_vulkan_create_swapchain(vk->vulkan, &pl_params);
    if (!vk->swapchain)
        goto error;

    return true;

error:
    ra_vk_ctx_uninit(ctx);
    return false;
}

bool ra_vk_ctx_resize(struct ra_ctx *ctx, int width, int height)
{
    struct priv *p = ctx->swapchain->priv;

    bool ok = pl_swapchain_resize(p->vk->swapchain, &width, &height);
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
    struct priv *p = sw->priv;
    if (p->params.color_depth)
        return p->params.color_depth(sw->ctx);

    return -1;
}

static bool start_frame(struct ra_swapchain *sw, struct ra_fbo *out_fbo)
{
    struct priv *p = sw->priv;
    struct pl_swapchain_frame frame;

    bool visible = true;
    if (p->params.check_visible)
        visible = p->params.check_visible(sw->ctx);

    // If out_fbo is NULL, this was called from vo_gpu_next. Bail out.
    if (out_fbo == NULL || !visible)
        return visible;

    if (!pl_swapchain_start_frame(p->vk->swapchain, &frame))
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
    return pl_swapchain_submit_frame(p->vk->swapchain);
}

static void swap_buffers(struct ra_swapchain *sw)
{
    struct priv *p = sw->priv;
    pl_swapchain_swap_buffers(p->vk->swapchain);
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

static pl_color_space_t target_csp(struct ra_swapchain *sw)
{
    struct priv *p = sw->priv;
    if (p->params.preferred_csp)
        return p->params.preferred_csp(sw->ctx);
    return (pl_color_space_t){0};
}

static const struct ra_swapchain_fns vulkan_swapchain = {
    .color_depth   = color_depth,
    .target_csp    = target_csp,
    .start_frame   = start_frame,
    .submit_frame  = submit_frame,
    .swap_buffers  = swap_buffers,
    .get_vsync     = get_vsync,
};
