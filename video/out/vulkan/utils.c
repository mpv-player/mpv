#include <libavutil/macros.h>

#include "video/out/gpu/spirv.h"
#include "utils.h"
#include "malloc.h"

const char* vk_err(VkResult res)
{
    switch (res) {
    // These are technically success codes, but include them nonetheless
    case VK_SUCCESS:     return "VK_SUCCESS";
    case VK_NOT_READY:   return "VK_NOT_READY";
    case VK_TIMEOUT:     return "VK_TIMEOUT";
    case VK_EVENT_SET:   return "VK_EVENT_SET";
    case VK_EVENT_RESET: return "VK_EVENT_RESET";
    case VK_INCOMPLETE:  return "VK_INCOMPLETE";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";

    // Actual error codes
    case VK_ERROR_OUT_OF_HOST_MEMORY:    return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:  return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:           return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:     return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:     return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:   return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:   return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:      return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:  return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:       return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_INVALID_SHADER_NV:     return "VK_ERROR_INVALID_SHADER_NV";
    case VK_ERROR_OUT_OF_DATE_KHR:       return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_SURFACE_LOST_KHR:      return "VK_ERROR_SURFACE_LOST_KHR";
    }

    return "Unknown error!";
}

static const char* vk_dbg_type(VkDebugReportObjectTypeEXT type)
{
    switch (type) {
    case VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT:
        return "VkInstance";
    case VK_DEBUG_REPORT_OBJECT_TYPE_PHYSICAL_DEVICE_EXT:
        return "VkPhysicalDevice";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT:
        return "VkDevice";
    case VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT:
        return "VkQueue";
    case VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT:
        return "VkSemaphore";
    case VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT:
        return "VkCommandBuffer";
    case VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT:
        return "VkFence";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT:
        return "VkDeviceMemory";
    case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT:
        return "VkBuffer";
    case VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT:
        return "VkImage";
    case VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT:
        return "VkEvent";
    case VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT:
        return "VkQueryPool";
    case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_VIEW_EXT:
        return "VkBufferView";
    case VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT:
        return "VkImageView";
    case VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT:
        return "VkShaderModule";
    case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_CACHE_EXT:
        return "VkPipelineCache";
    case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT:
        return "VkPipelineLayout";
    case VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT:
        return "VkRenderPass";
    case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT:
        return "VkPipeline";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT:
        return "VkDescriptorSetLayout";
    case VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT:
        return "VkSampler";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT:
        return "VkDescriptorPool";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT:
        return "VkDescriptorSet";
    case VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT:
        return "VkFramebuffer";
    case VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT:
        return "VkCommandPool";
    case VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT:
        return "VkSurfaceKHR";
    case VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT:
        return "VkSwapchainKHR";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DEBUG_REPORT_EXT:
        return "VkDebugReportCallbackEXT";
    case VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT:
    default:
        return "unknown object";
    }
}

static VkBool32 vk_dbg_callback(VkDebugReportFlagsEXT flags,
                                VkDebugReportObjectTypeEXT objType,
                                uint64_t obj, size_t loc, int32_t msgCode,
                                const char *layer, const char *msg, void *priv)
{
    struct mpvk_ctx *vk = priv;
    int lev = MSGL_V;

    switch (flags) {
    case VK_DEBUG_REPORT_ERROR_BIT_EXT:               lev = MSGL_ERR;   break;
    case VK_DEBUG_REPORT_WARNING_BIT_EXT:             lev = MSGL_WARN;  break;
    case VK_DEBUG_REPORT_INFORMATION_BIT_EXT:         lev = MSGL_TRACE; break;
    case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT: lev = MSGL_WARN;  break;
    case VK_DEBUG_REPORT_DEBUG_BIT_EXT:               lev = MSGL_DEBUG; break;
    };

    MP_MSG(vk, lev, "vk [%s] %d: %s (obj 0x%llx (%s), loc 0x%zx)\n",
           layer, (int)msgCode, msg, (unsigned long long)obj,
           vk_dbg_type(objType), loc);

    // The return value of this function determines whether the call will
    // be explicitly aborted (to prevent GPU errors) or not. In this case,
    // we generally want this to be on for the errors.
    return (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT);
}

static void vk_cmdpool_destroy(struct mpvk_ctx *vk, struct vk_cmdpool *pool);
static struct vk_cmdpool *vk_cmdpool_create(struct mpvk_ctx *vk,
                                            VkDeviceQueueCreateInfo qinfo,
                                            VkQueueFamilyProperties props);

void mpvk_uninit(struct mpvk_ctx *vk)
{
    if (!vk->inst)
        return;

    if (vk->dev) {
        mpvk_flush_commands(vk);
        mpvk_poll_commands(vk, UINT64_MAX);
        assert(vk->num_cmds_queued == 0);
        assert(vk->num_cmds_pending == 0);
        talloc_free(vk->cmds_queued);
        talloc_free(vk->cmds_pending);
        for (int i = 0; i < vk->num_pools; i++)
            vk_cmdpool_destroy(vk, vk->pools[i]);
        talloc_free(vk->pools);
        for (int i = 0; i < vk->num_signals; i++)
            vk_signal_destroy(vk, &vk->signals[i]);
        talloc_free(vk->signals);
        vk_malloc_uninit(vk);
        vkDestroyDevice(vk->dev, MPVK_ALLOCATOR);
    }

    if (vk->dbg) {
        // Same deal as creating the debug callback, we need to load this
        // first.
        VK_LOAD_PFN(vkDestroyDebugReportCallbackEXT)
        pfn_vkDestroyDebugReportCallbackEXT(vk->inst, vk->dbg, MPVK_ALLOCATOR);
    }

    vkDestroySurfaceKHR(vk->inst, vk->surf, MPVK_ALLOCATOR);
    vkDestroyInstance(vk->inst, MPVK_ALLOCATOR);

    *vk = (struct mpvk_ctx){0};
}

bool mpvk_instance_init(struct mpvk_ctx *vk, struct mp_log *log,
                        const char *surf_ext_name, bool debug)
{
    *vk = (struct mpvk_ctx) {
        .log = log,
    };

    VkInstanceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    };

    if (debug) {
        // Enables the LunarG standard validation layer, which
        // is a meta-layer that loads lots of other validators
        static const char* layers[] = {
            "VK_LAYER_LUNARG_standard_validation",
        };

        info.ppEnabledLayerNames = layers;
        info.enabledLayerCount = MP_ARRAY_SIZE(layers);
    }

    // Enable whatever extensions were compiled in.
    const char *extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        surf_ext_name,

        // Extra extensions only used for debugging. These are toggled by
        // decreasing the enabledExtensionCount, so the number needs to be
        // synchronized with the code below.
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
    };

    const int debugExtensionCount = 1;

    info.ppEnabledExtensionNames = extensions;
    info.enabledExtensionCount = MP_ARRAY_SIZE(extensions);

    if (!debug)
        info.enabledExtensionCount -= debugExtensionCount;

    MP_VERBOSE(vk, "Creating instance with extensions:\n");
    for (int i = 0; i < info.enabledExtensionCount; i++)
        MP_VERBOSE(vk, "    %s\n", info.ppEnabledExtensionNames[i]);

    VkResult res = vkCreateInstance(&info, MPVK_ALLOCATOR, &vk->inst);
    if (res != VK_SUCCESS) {
        MP_VERBOSE(vk, "Failed creating instance: %s\n", vk_err(res));
        return false;
    }

    if (debug) {
        // Set up a debug callback to catch validation messages
        VkDebugReportCallbackCreateInfoEXT dinfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
            .flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
                     VK_DEBUG_REPORT_WARNING_BIT_EXT |
                     VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
                     VK_DEBUG_REPORT_ERROR_BIT_EXT |
                     VK_DEBUG_REPORT_DEBUG_BIT_EXT,
            .pfnCallback = vk_dbg_callback,
            .pUserData = vk,
        };

        // Since this is not part of the core spec, we need to load it. This
        // can't fail because we've already successfully created an instance
        // with this extension enabled.
        VK_LOAD_PFN(vkCreateDebugReportCallbackEXT)
        pfn_vkCreateDebugReportCallbackEXT(vk->inst, &dinfo, MPVK_ALLOCATOR,
                                           &vk->dbg);
    }

    return true;
}

#define MPVK_MAX_DEVICES 16

static bool physd_supports_surface(struct mpvk_ctx *vk, VkPhysicalDevice physd)
{
    uint32_t qfnum;
    vkGetPhysicalDeviceQueueFamilyProperties(physd, &qfnum, NULL);

    for (int i = 0; i < qfnum; i++) {
        VkBool32 sup;
        VK(vkGetPhysicalDeviceSurfaceSupportKHR(physd, i, vk->surf, &sup));
        if (sup)
            return true;
    }

error:
    return false;
}

bool mpvk_find_phys_device(struct mpvk_ctx *vk, const char *name, bool sw)
{
    assert(vk->surf);

    MP_VERBOSE(vk, "Probing for vulkan devices:\n");

    VkPhysicalDevice *devices = NULL;
    uint32_t num = 0;
    VK(vkEnumeratePhysicalDevices(vk->inst, &num, NULL));
    devices = talloc_array(NULL, VkPhysicalDevice, num);
    VK(vkEnumeratePhysicalDevices(vk->inst, &num, devices));

    // Sorted by "priority". Reuses some m_opt code for convenience
    static const struct m_opt_choice_alternatives types[] = {
        {"discrete",   VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU},
        {"integrated", VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU},
        {"virtual",    VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU},
        {"software",   VK_PHYSICAL_DEVICE_TYPE_CPU},
        {"unknown",    VK_PHYSICAL_DEVICE_TYPE_OTHER},
        {0}
    };

    VkPhysicalDeviceProperties props[MPVK_MAX_DEVICES];
    for (int i = 0; i < num; i++) {
        vkGetPhysicalDeviceProperties(devices[i], &props[i]);
        MP_VERBOSE(vk, "    GPU %d: %s (%s)\n", i, props[i].deviceName,
                   m_opt_choice_str(types, props[i].deviceType));
    }

    // Iterate through each type in order of decreasing preference
    for (int t = 0; types[t].name; t++) {
        // Disallow SW rendering unless explicitly enabled
        if (types[t].value == VK_PHYSICAL_DEVICE_TYPE_CPU && !sw)
            continue;

        for (int i = 0; i < num; i++) {
            VkPhysicalDeviceProperties prop = props[i];
            if (prop.deviceType != types[t].value)
                continue;
            if (name && strcmp(name, prop.deviceName) != 0)
                continue;
            if (!physd_supports_surface(vk, devices[i]))
                continue;

            MP_VERBOSE(vk, "Chose device:\n");
            MP_VERBOSE(vk, "    Device Name: %s\n", prop.deviceName);
            MP_VERBOSE(vk, "    Device ID: %x:%x\n",
                       (unsigned)prop.vendorID, (unsigned)prop.deviceID);
            MP_VERBOSE(vk, "    Driver version: %d\n", (int)prop.driverVersion);
            MP_VERBOSE(vk, "    API version: %d.%d.%d\n",
                    (int)VK_VERSION_MAJOR(prop.apiVersion),
                    (int)VK_VERSION_MINOR(prop.apiVersion),
                    (int)VK_VERSION_PATCH(prop.apiVersion));
            vk->physd = devices[i];
            vk->limits = prop.limits;
            vkGetPhysicalDeviceFeatures(vk->physd, &vk->features);
            talloc_free(devices);
            return true;
        }
    }

error:
    MP_VERBOSE(vk, "Found no suitable device, giving up.\n");
    talloc_free(devices);
    return false;
}

bool mpvk_pick_surface_format(struct mpvk_ctx *vk)
{
    assert(vk->physd);

    VkSurfaceFormatKHR *formats = NULL;
    int num;

    // Enumerate through the surface formats and find one that we can map to
    // a ra_format
    VK(vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physd, vk->surf, &num, NULL));
    formats = talloc_array(NULL, VkSurfaceFormatKHR, num);
    VK(vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physd, vk->surf, &num, formats));

    for (int i = 0; i < num; i++) {
        // A value of VK_FORMAT_UNDEFINED means we can pick anything we want
        if (formats[i].format == VK_FORMAT_UNDEFINED) {
            vk->surf_format = (VkSurfaceFormatKHR) {
                .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                .format = VK_FORMAT_R16G16B16A16_UNORM,
            };
            break;
        }

        if (formats[i].colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            continue;

        // Format whitelist, since we want only >= 8 bit _UNORM formats
        switch (formats[i].format) {
        case VK_FORMAT_R8G8B8_UNORM:
        case VK_FORMAT_B8G8R8_UNORM:
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        case VK_FORMAT_R16G16B16_UNORM:
        case VK_FORMAT_R16G16B16A16_UNORM:
             break; // accept
        default: continue;
        }

        vk->surf_format = formats[i];
        break;
    }

    talloc_free(formats);

    if (!vk->surf_format.format)
        goto error;

    return true;

error:
    MP_ERR(vk, "Failed picking surface format!\n");
    talloc_free(formats);
    return false;
}

// Find the most specialized queue supported a combination of flags. In cases
// where there are multiple queue families at the same specialization level,
// this finds the one with the most queues. Returns -1 if no queue was found.
static int find_qf(VkQueueFamilyProperties *qfs, int qfnum, VkQueueFlags flags)
{
    int idx = -1;
    for (int i = 0; i < qfnum; i++) {
        if (!(qfs[i].queueFlags & flags))
            continue;

        // QF is more specialized. Since we don't care about other bits like
        // SPARSE_BIT, mask the ones we're interestew in
        const VkQueueFlags mask = VK_QUEUE_GRAPHICS_BIT |
                                  VK_QUEUE_TRANSFER_BIT |
                                  VK_QUEUE_COMPUTE_BIT;

        if (idx < 0 || (qfs[i].queueFlags & mask) < (qfs[idx].queueFlags & mask))
            idx = i;

        // QF has more queues (at the same specialization level)
        if (qfs[i].queueFlags == qfs[idx].queueFlags &&
            qfs[i].queueCount > qfs[idx].queueCount)
            idx = i;
    }

    return idx;
}

static void add_qinfo(void *tactx, VkDeviceQueueCreateInfo **qinfos,
                      int *num_qinfos, VkQueueFamilyProperties *qfs, int idx,
                      int qcount)
{
    if (idx < 0)
        return;

    // Check to see if we've already added this queue family
    for (int i = 0; i < *num_qinfos; i++) {
        if ((*qinfos)[i].queueFamilyIndex == idx)
            return;
    }

    float *priorities = talloc_zero_array(tactx, float, qcount);
    VkDeviceQueueCreateInfo qinfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = idx,
        .queueCount = MPMIN(qcount, qfs[idx].queueCount),
        .pQueuePriorities = priorities,
    };

    MP_TARRAY_APPEND(tactx, *qinfos, *num_qinfos, qinfo);
}

bool mpvk_device_init(struct mpvk_ctx *vk, struct mpvk_device_opts opts)
{
    assert(vk->physd);
    void *tmp = talloc_new(NULL);

    // Enumerate the queue families and find suitable families for each task
    int qfnum;
    vkGetPhysicalDeviceQueueFamilyProperties(vk->physd, &qfnum, NULL);
    VkQueueFamilyProperties *qfs = talloc_array(tmp, VkQueueFamilyProperties, qfnum);
    vkGetPhysicalDeviceQueueFamilyProperties(vk->physd, &qfnum, qfs);

    MP_VERBOSE(vk, "Queue families supported by device:\n");

    for (int i = 0; i < qfnum; i++) {
        MP_VERBOSE(vk, "    QF %d: flags 0x%x num %d\n", i,
                   (unsigned)qfs[i].queueFlags, (int)qfs[i].queueCount);
    }

    int idx_gfx = -1, idx_comp = -1, idx_tf = -1;
    idx_gfx = find_qf(qfs, qfnum, VK_QUEUE_GRAPHICS_BIT);
    if (opts.async_compute)
        idx_comp = find_qf(qfs, qfnum, VK_QUEUE_COMPUTE_BIT);
    if (opts.async_transfer)
        idx_tf = find_qf(qfs, qfnum, VK_QUEUE_TRANSFER_BIT);

    // Vulkan requires at least one GRAPHICS queue, so if this fails something
    // is horribly wrong.
    assert(idx_gfx >= 0);
    MP_VERBOSE(vk, "Using graphics queue (QF %d)\n", idx_gfx);

    // Ensure we can actually present to the surface using this queue
    VkBool32 sup;
    VK(vkGetPhysicalDeviceSurfaceSupportKHR(vk->physd, idx_gfx, vk->surf, &sup));
    if (!sup) {
        MP_ERR(vk, "Queue family does not support surface presentation!\n");
        goto error;
    }

    if (idx_tf >= 0 && idx_tf != idx_gfx)
        MP_VERBOSE(vk, "Using async transfer (QF %d)\n", idx_tf);
    if (idx_comp >= 0 && idx_comp != idx_gfx)
        MP_VERBOSE(vk, "Using async compute (QF %d)\n", idx_comp);

    // Fall back to supporting compute shaders via the graphics pool for
    // devices which support compute shaders but not async compute.
    if (idx_comp < 0 && qfs[idx_gfx].queueFlags & VK_QUEUE_COMPUTE_BIT)
        idx_comp = idx_gfx;

    // Now that we know which QFs we want, we can create the logical device
    VkDeviceQueueCreateInfo *qinfos = NULL;
    int num_qinfos = 0;
    add_qinfo(tmp, &qinfos, &num_qinfos, qfs, idx_gfx, opts.queue_count);
    add_qinfo(tmp, &qinfos, &num_qinfos, qfs, idx_comp, opts.queue_count);
    add_qinfo(tmp, &qinfos, &num_qinfos, qfs, idx_tf, opts.queue_count);

    const char **exts = NULL;
    int num_exts = 0;
    MP_TARRAY_APPEND(tmp, exts, num_exts, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if (vk->spirv->required_ext)
        MP_TARRAY_APPEND(tmp, exts, num_exts, vk->spirv->required_ext);

    // Enable all features we optionally use
#define FEATURE(name) .name = vk->features.name
    VkPhysicalDeviceFeatures feats = {
        FEATURE(shaderImageGatherExtended),
        FEATURE(shaderStorageImageExtendedFormats),
    };
#undef FEATURE

    VkDeviceCreateInfo dinfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = qinfos,
        .queueCreateInfoCount = num_qinfos,
        .ppEnabledExtensionNames = exts,
        .enabledExtensionCount = num_exts,
        .pEnabledFeatures = &feats,
    };

    MP_VERBOSE(vk, "Creating vulkan device with extensions:\n");
    for (int i = 0; i < num_exts; i++)
        MP_VERBOSE(vk, "    %s\n", exts[i]);

    VK(vkCreateDevice(vk->physd, &dinfo, MPVK_ALLOCATOR, &vk->dev));

    // Create the command pools and memory allocator
    for (int i = 0; i < num_qinfos; i++) {
        int qf = qinfos[i].queueFamilyIndex;
        struct vk_cmdpool *pool = vk_cmdpool_create(vk, qinfos[i], qfs[qf]);
        if (!pool)
            goto error;
        MP_TARRAY_APPEND(NULL, vk->pools, vk->num_pools, pool);

        // Update the pool_* pointers based on the corresponding QF index
        if (qf == idx_gfx)
            vk->pool_graphics = pool;
        if (qf == idx_comp)
            vk->pool_compute = pool;
        if (qf == idx_tf)
            vk->pool_transfer = pool;
    }

    vk_malloc_init(vk);
    talloc_free(tmp);
    return true;

error:
    MP_ERR(vk, "Failed creating logical device!\n");
    talloc_free(tmp);
    return false;
}

// returns VK_SUCCESS (completed), VK_TIMEOUT (not yet completed) or an error
static VkResult vk_cmd_poll(struct mpvk_ctx *vk, struct vk_cmd *cmd,
                            uint64_t timeout)
{
    return vkWaitForFences(vk->dev, 1, &cmd->fence, false, timeout);
}

static void vk_cmd_reset(struct mpvk_ctx *vk, struct vk_cmd *cmd)
{
    for (int i = 0; i < cmd->num_callbacks; i++) {
        struct vk_callback *cb = &cmd->callbacks[i];
        cb->run(cb->priv, cb->arg);
    }

    cmd->num_callbacks = 0;
    cmd->num_deps = 0;
    cmd->num_sigs = 0;

    // also make sure to reset vk->last_cmd in case this was the last command
    if (vk->last_cmd == cmd)
        vk->last_cmd = NULL;
}

static void vk_cmd_destroy(struct mpvk_ctx *vk, struct vk_cmd *cmd)
{
    if (!cmd)
        return;

    vk_cmd_poll(vk, cmd, UINT64_MAX);
    vk_cmd_reset(vk, cmd);
    vkDestroyFence(vk->dev, cmd->fence, MPVK_ALLOCATOR);
    vkFreeCommandBuffers(vk->dev, cmd->pool->pool, 1, &cmd->buf);

    talloc_free(cmd);
}

static struct vk_cmd *vk_cmd_create(struct mpvk_ctx *vk, struct vk_cmdpool *pool)
{
    struct vk_cmd *cmd = talloc_zero(NULL, struct vk_cmd);
    cmd->pool = pool;

    VkCommandBufferAllocateInfo ainfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool->pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VK(vkAllocateCommandBuffers(vk->dev, &ainfo, &cmd->buf));

    VkFenceCreateInfo finfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    VK(vkCreateFence(vk->dev, &finfo, MPVK_ALLOCATOR, &cmd->fence));

    return cmd;

error:
    vk_cmd_destroy(vk, cmd);
    return NULL;
}

void vk_cmd_callback(struct vk_cmd *cmd, vk_cb callback, void *p, void *arg)
{
    MP_TARRAY_APPEND(cmd, cmd->callbacks, cmd->num_callbacks, (struct vk_callback) {
        .run  = callback,
        .priv = p,
        .arg  = arg,
    });
}

void vk_cmd_dep(struct vk_cmd *cmd, VkSemaphore dep, VkPipelineStageFlags stage)
{
    int idx = cmd->num_deps++;
    MP_TARRAY_GROW(cmd, cmd->deps, idx);
    MP_TARRAY_GROW(cmd, cmd->depstages, idx);
    cmd->deps[idx] = dep;
    cmd->depstages[idx] = stage;
}

void vk_cmd_sig(struct vk_cmd *cmd, VkSemaphore sig)
{
    MP_TARRAY_APPEND(cmd, cmd->sigs, cmd->num_sigs, sig);
}

static void vk_cmdpool_destroy(struct mpvk_ctx *vk, struct vk_cmdpool *pool)
{
    if (!pool)
        return;

    for (int i = 0; i < pool->num_cmds; i++)
        vk_cmd_destroy(vk, pool->cmds[i]);

    vkDestroyCommandPool(vk->dev, pool->pool, MPVK_ALLOCATOR);
    talloc_free(pool);
}

static struct vk_cmdpool *vk_cmdpool_create(struct mpvk_ctx *vk,
                                            VkDeviceQueueCreateInfo qinfo,
                                            VkQueueFamilyProperties props)
{
    struct vk_cmdpool *pool = talloc_ptrtype(NULL, pool);
    *pool = (struct vk_cmdpool) {
        .props = props,
        .qf = qinfo.queueFamilyIndex,
        .queues = talloc_array(pool, VkQueue, qinfo.queueCount),
        .num_queues = qinfo.queueCount,
    };

    for (int n = 0; n < pool->num_queues; n++)
        vkGetDeviceQueue(vk->dev, pool->qf, n, &pool->queues[n]);

    VkCommandPoolCreateInfo cinfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                 VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = pool->qf,
    };

    VK(vkCreateCommandPool(vk->dev, &cinfo, MPVK_ALLOCATOR, &pool->pool));

    return pool;

error:
    vk_cmdpool_destroy(vk, pool);
    return NULL;
}

void mpvk_poll_commands(struct mpvk_ctx *vk, uint64_t timeout)
{
    while (vk->num_cmds_pending > 0) {
        struct vk_cmd *cmd = vk->cmds_pending[0];
        struct vk_cmdpool *pool = cmd->pool;
        VkResult res = vk_cmd_poll(vk, cmd, timeout);
        if (res == VK_TIMEOUT)
            break;
        vk_cmd_reset(vk, cmd);
        MP_TARRAY_REMOVE_AT(vk->cmds_pending, vk->num_cmds_pending, 0);
        MP_TARRAY_APPEND(pool, pool->cmds, pool->num_cmds, cmd);
    }
}

bool mpvk_flush_commands(struct mpvk_ctx *vk)
{
    bool ret = true;

    for (int i = 0; i < vk->num_cmds_queued; i++) {
        struct vk_cmd *cmd = vk->cmds_queued[i];
        struct vk_cmdpool *pool = cmd->pool;

        VkSubmitInfo sinfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd->buf,
            .waitSemaphoreCount = cmd->num_deps,
            .pWaitSemaphores = cmd->deps,
            .pWaitDstStageMask = cmd->depstages,
            .signalSemaphoreCount = cmd->num_sigs,
            .pSignalSemaphores = cmd->sigs,
        };

        VK(vkQueueSubmit(cmd->queue, 1, &sinfo, cmd->fence));
        MP_TARRAY_APPEND(NULL, vk->cmds_pending, vk->num_cmds_pending, cmd);

        if (mp_msg_test(vk->log, MSGL_TRACE)) {
            MP_TRACE(vk, "Submitted command on queue %p (QF %d):\n",
                     (void *)cmd->queue, pool->qf);
            for (int n = 0; n < cmd->num_deps; n++)
                MP_TRACE(vk, "    waits on semaphore %p\n", (void *)cmd->deps[n]);
            for (int n = 0; n < cmd->num_sigs; n++)
                MP_TRACE(vk, "    signals semaphore %p\n", (void *)cmd->sigs[n]);
        }
        continue;

error:
        vk_cmd_reset(vk, cmd);
        MP_TARRAY_APPEND(pool, pool->cmds, pool->num_cmds, cmd);
        ret = false;
    }

    vk->num_cmds_queued = 0;

    // Rotate the queues to ensure good parallelism across frames
    for (int i = 0; i < vk->num_pools; i++) {
        struct vk_cmdpool *pool = vk->pools[i];
        pool->idx_queues = (pool->idx_queues + 1) % pool->num_queues;
    }

    return ret;
}

void vk_dev_callback(struct mpvk_ctx *vk, vk_cb callback, void *p, void *arg)
{
    if (vk->last_cmd) {
        vk_cmd_callback(vk->last_cmd, callback, p, arg);
    } else {
        // The device was already idle, so we can just immediately call it
        callback(p, arg);
    }
}

struct vk_cmd *vk_cmd_begin(struct mpvk_ctx *vk, struct vk_cmdpool *pool)
{
    // garbage collect the cmdpool first, to increase the chances of getting
    // an already-available command buffer
    mpvk_poll_commands(vk, 0);

    struct vk_cmd *cmd = NULL;
    if (MP_TARRAY_POP(pool->cmds, pool->num_cmds, &cmd))
        goto done;

    // No free command buffers => allocate another one
    cmd = vk_cmd_create(vk, pool);
    if (!cmd)
        goto error;

done: ;

    VkCommandBufferBeginInfo binfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    VK(vkBeginCommandBuffer(cmd->buf, &binfo));

    cmd->queue = pool->queues[pool->idx_queues];
    return cmd;

error:
    // Something has to be seriously messed up if we get to this point
    vk_cmd_destroy(vk, cmd);
    return NULL;
}

void vk_cmd_queue(struct mpvk_ctx *vk, struct vk_cmd *cmd)
{
    struct vk_cmdpool *pool = cmd->pool;

    VK(vkEndCommandBuffer(cmd->buf));

    VK(vkResetFences(vk->dev, 1, &cmd->fence));
    MP_TARRAY_APPEND(NULL, vk->cmds_queued, vk->num_cmds_queued, cmd);
    vk->last_cmd = cmd;
    return;

error:
    vk_cmd_reset(vk, cmd);
    MP_TARRAY_APPEND(pool, pool->cmds, pool->num_cmds, cmd);
}

void vk_signal_destroy(struct mpvk_ctx *vk, struct vk_signal **sig)
{
    if (!*sig)
        return;

    vkDestroySemaphore(vk->dev, (*sig)->semaphore, MPVK_ALLOCATOR);
    vkDestroyEvent(vk->dev, (*sig)->event, MPVK_ALLOCATOR);
    talloc_free(*sig);
    *sig = NULL;
}

struct vk_signal *vk_cmd_signal(struct mpvk_ctx *vk, struct vk_cmd *cmd,
                                VkPipelineStageFlags stage)
{
    struct vk_signal *sig = NULL;
    if (MP_TARRAY_POP(vk->signals, vk->num_signals, &sig))
        goto done;

    // no available signal => initialize a new one
    sig = talloc_zero(NULL, struct vk_signal);
    static const VkSemaphoreCreateInfo sinfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VK(vkCreateSemaphore(vk->dev, &sinfo, MPVK_ALLOCATOR, &sig->semaphore));

    static const VkEventCreateInfo einfo = {
        .sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
    };

    VK(vkCreateEvent(vk->dev, &einfo, MPVK_ALLOCATOR, &sig->event));

done:
    // Signal both the semaphore and the event if possible. (We will only
    // end up using one or the other)
    vk_cmd_sig(cmd, sig->semaphore);

    VkQueueFlags req = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    if (cmd->pool->props.queueFlags & req) {
        vkCmdSetEvent(cmd->buf, sig->event, stage);
        sig->event_source = cmd->queue;
    }

    return sig;

error:
    vk_signal_destroy(vk, &sig);
    return NULL;
}

static bool unsignal_cmd(struct vk_cmd *cmd, VkSemaphore sem)
{
    for (int n = 0; n < cmd->num_sigs; n++) {
        if (cmd->sigs[n] == sem) {
            MP_TARRAY_REMOVE_AT(cmd->sigs, cmd->num_sigs, n);
            return true;
        }
    }

    return false;
}

// Attempts to remove a queued signal operation. Returns true if sucessful,
// i.e. the signal could be removed before it ever got fired.
static bool unsignal(struct mpvk_ctx *vk, struct vk_cmd *cmd, VkSemaphore sem)
{
    if (unsignal_cmd(cmd, sem))
        return true;

    // Attempt to remove it from any queued commands
    for (int i = 0; i < vk->num_cmds_queued; i++) {
        if (unsignal_cmd(vk->cmds_queued[i], sem))
            return true;
    }

    return false;
}

static void release_signal(struct mpvk_ctx *vk, struct vk_signal *sig)
{
    // The semaphore never needs to be recreated, because it's either
    // unsignaled while still queued, or unsignaled as a result of a device
    // wait. But the event *may* need to be reset, so just always reset it.
    if (sig->event_source)
        vkResetEvent(vk->dev, sig->event);
    sig->event_source = NULL;
    MP_TARRAY_APPEND(NULL, vk->signals, vk->num_signals, sig);
}

void vk_cmd_wait(struct mpvk_ctx *vk, struct vk_cmd *cmd,
                 struct vk_signal **sigptr, VkPipelineStageFlags stage,
                 VkEvent *out_event)
{
    struct vk_signal *sig = *sigptr;
    if (!sig)
        return;

    if (out_event && sig->event && sig->event_source == cmd->queue &&
        unsignal(vk, cmd, sig->semaphore))
    {
        // If we can remove the semaphore signal operation from the history and
        // pretend it never happened, then we get to use the VkEvent. This also
        // requires that the VkEvent was signalled from the same VkQueue.
        *out_event = sig->event;
    } else if (sig->semaphore) {
        // Otherwise, we use the semaphore. (This also unsignals it as a result
        // of the command execution)
        vk_cmd_dep(cmd, sig->semaphore, stage);
    }

    // In either case, once the command completes, we can release the signal
    // resource back to the pool.
    vk_cmd_callback(cmd, (vk_cb) release_signal, vk, sig);
    *sigptr = NULL;
}

const VkImageSubresourceRange vk_range = {
    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .levelCount = 1,
    .layerCount = 1,
};

const VkImageSubresourceLayers vk_layers = {
    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .layerCount = 1,
};
