#include "video/out/gpu/utils.h"
#include "video/out/gpu/spirv.h"

#include "ra_vk.h"
#include "malloc.h"

static struct ra_fns ra_fns_vk;

enum queue_type {
    GRAPHICS,
    COMPUTE,
    TRANSFER,
};

// For ra.priv
struct ra_vk {
    struct mpvk_ctx *vk;
    struct ra_tex *clear_tex; // stupid hack for clear()
    struct vk_cmd *cmd;       // currently recording cmd
};

struct mpvk_ctx *ra_vk_get(struct ra *ra)
{
    if (ra->fns != &ra_fns_vk)
        return NULL;

    struct ra_vk *p = ra->priv;
    return p->vk;
}

static void vk_submit(struct ra *ra)
{
    struct ra_vk *p = ra->priv;
    struct mpvk_ctx *vk = ra_vk_get(ra);

    if (p->cmd) {
        vk_cmd_queue(vk, p->cmd);
        p->cmd = NULL;
    }
}

// Returns a command buffer, or NULL on error
static struct vk_cmd *vk_require_cmd(struct ra *ra, enum queue_type type)
{
    struct ra_vk *p = ra->priv;
    struct mpvk_ctx *vk = ra_vk_get(ra);

    struct vk_cmdpool *pool;
    switch (type) {
    case GRAPHICS: pool = vk->pool_graphics; break;
    case COMPUTE:  pool = vk->pool_compute;  break;

    // GRAPHICS and COMPUTE also imply TRANSFER capability (vulkan spec)
    case TRANSFER:
        pool = vk->pool_transfer;
        if (!pool)
            pool = vk->pool_compute;
        if (!pool)
            pool = vk->pool_graphics;
        break;
    default: abort();
    }

    assert(pool);
    if (p->cmd && p->cmd->pool == pool)
        return p->cmd;

    vk_submit(ra);
    p->cmd = vk_cmd_begin(vk, pool);
    return p->cmd;
}

#define MAKE_LAZY_DESTRUCTOR(fun, argtype)                  \
    static void fun##_lazy(struct ra *ra, argtype *arg) {   \
        struct ra_vk *p = ra->priv;                         \
        struct mpvk_ctx *vk = ra_vk_get(ra);                \
        if (p->cmd) {                                       \
            vk_cmd_callback(p->cmd, (vk_cb) fun, ra, arg);  \
        } else {                                            \
            vk_dev_callback(vk, (vk_cb) fun, ra, arg);      \
        }                                                   \
    }

static void vk_destroy_ra(struct ra *ra)
{
    struct ra_vk *p = ra->priv;
    struct mpvk_ctx *vk = ra_vk_get(ra);

    vk_submit(ra);
    mpvk_flush_commands(vk);
    mpvk_poll_commands(vk, UINT64_MAX);
    ra_tex_free(ra, &p->clear_tex);

    talloc_free(ra);
}

static bool vk_setup_formats(struct ra *ra)
{
    struct mpvk_ctx *vk = ra_vk_get(ra);

    for (const struct vk_format *vk_fmt = vk_formats; vk_fmt->name; vk_fmt++) {
        VkFormatProperties prop;
        vkGetPhysicalDeviceFormatProperties(vk->physd, vk_fmt->iformat, &prop);

        // As a bare minimum, we need to sample from an allocated image
        VkFormatFeatureFlags flags = prop.optimalTilingFeatures;
        if (!(flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
            continue;

        VkFormatFeatureFlags linear_bits, render_bits;
        linear_bits = VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        render_bits = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;

        struct ra_format *fmt = talloc_zero(ra, struct ra_format);
        *fmt = (struct ra_format) {
            .name            = vk_fmt->name,
            .priv            = (void *)vk_fmt,
            .ctype           = vk_fmt->ctype,
            .ordered         = !vk_fmt->fucked_order,
            .num_components  = vk_fmt->components,
            .pixel_size      = vk_fmt->bytes,
            .linear_filter   = !!(flags & linear_bits),
            .renderable      = !!(flags & render_bits),
        };

        for (int i = 0; i < 4; i++)
            fmt->component_size[i] = fmt->component_depth[i] = vk_fmt->bits[i];

        fmt->glsl_format = ra_fmt_glsl_format(fmt);

        MP_TARRAY_APPEND(ra, ra->formats, ra->num_formats, fmt);
    }

    // Populate some other capabilities related to formats while we're at it
    VkImageType imgType[3] = {
        VK_IMAGE_TYPE_1D,
        VK_IMAGE_TYPE_2D,
        VK_IMAGE_TYPE_3D
    };

    // R8_UNORM is supported on literally every single vulkan implementation
    const VkFormat testfmt = VK_FORMAT_R8_UNORM;

    for (int d = 0; d < 3; d++) {
        VkImageFormatProperties iprop;
        VkResult res = vkGetPhysicalDeviceImageFormatProperties(vk->physd,
                testfmt, imgType[d], VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_SAMPLED_BIT, 0, &iprop);

        switch (imgType[d]) {
        case VK_IMAGE_TYPE_1D:
            if (res == VK_SUCCESS)
                ra->caps |= RA_CAP_TEX_1D;
            break;
        case VK_IMAGE_TYPE_2D:
            // 2D formats must be supported by RA, so ensure this is the case
            VK_ASSERT(res, "Querying 2D format limits");
            ra->max_texture_wh = MPMIN(iprop.maxExtent.width, iprop.maxExtent.height);
            break;
        case VK_IMAGE_TYPE_3D:
            if (res == VK_SUCCESS)
                ra->caps |= RA_CAP_TEX_3D;
            break;
        }
    }

    // RA_CAP_BLIT implies both blitting between images as well as blitting
    // directly to the swapchain image, so check for all three operations
    bool blittable = true;
    VkFormatProperties prop;
    vkGetPhysicalDeviceFormatProperties(vk->physd, testfmt, &prop);
    if (!(prop.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT))
        blittable = false;
    if (!(prop.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
        blittable = false;

    vkGetPhysicalDeviceFormatProperties(vk->physd, vk->surf_format.format, &prop);
    if (!(prop.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
        blittable = false;

    if (blittable)
        ra->caps |= RA_CAP_BLIT;

    return true;

error:
    return false;
}

static struct ra_fns ra_fns_vk;

struct ra *ra_create_vk(struct mpvk_ctx *vk, struct mp_log *log)
{
    assert(vk->dev);
    assert(vk->alloc);

    struct ra *ra = talloc_zero(NULL, struct ra);
    ra->log = log;
    ra->fns = &ra_fns_vk;

    struct ra_vk *p = ra->priv = talloc_zero(ra, struct ra_vk);
    p->vk = vk;

    ra->caps |= vk->spirv->ra_caps;
    ra->glsl_version = vk->spirv->glsl_version;
    ra->glsl_vulkan = true;
    ra->max_shmem = vk->limits.maxComputeSharedMemorySize;
    ra->max_pushc_size = vk->limits.maxPushConstantsSize;

    if (vk->pool_compute) {
        ra->caps |= RA_CAP_COMPUTE | RA_CAP_NUM_GROUPS;
        // If we have more compute queues than graphics queues, we probably
        // want to be using them. (This seems mostly relevant for AMD)
        if (vk->pool_compute->num_queues > vk->pool_graphics->num_queues)
            ra->caps |= RA_CAP_PARALLEL_COMPUTE;
    }

    if (!vk_setup_formats(ra))
        goto error;

    // UBO support is required
    ra->caps |= RA_CAP_BUF_RO | RA_CAP_FRAGCOORD;

    // textureGather requires the ImageGatherExtended capability
    if (vk->features.shaderImageGatherExtended)
        ra->caps |= RA_CAP_GATHER;

    // Try creating a shader storage buffer
    struct ra_buf_params ssbo_params = {
        .type = RA_BUF_TYPE_SHADER_STORAGE,
        .size = 16,
    };

    struct ra_buf *ssbo = ra_buf_create(ra, &ssbo_params);
    if (ssbo) {
        ra->caps |= RA_CAP_BUF_RW;
        ra_buf_free(ra, &ssbo);
    }

    // To support clear() by region, we need to allocate a dummy 1x1 image that
    // will be used as the source of blit operations
    struct ra_tex_params clear_params = {
        .dimensions = 1, // no point in using a 2D image if height = 1
        .w = 1,
        .h = 1,
        .d = 1,
        .format = ra_find_float16_format(ra, 4),
        .blit_src = 1,
        .host_mutable = 1,
    };

    p->clear_tex = ra_tex_create(ra, &clear_params);
    if (!p->clear_tex) {
        MP_ERR(ra, "Failed creating 1x1 dummy texture for clear()!\n");
        goto error;
    }

    return ra;

error:
    vk_destroy_ra(ra);
    return NULL;
}

// Boilerplate wrapper around vkCreateRenderPass to ensure passes remain
// compatible. The renderpass will automatically transition the image out of
// initialLayout and into finalLayout.
static VkResult vk_create_render_pass(VkDevice dev, const struct ra_format *fmt,
                                      VkAttachmentLoadOp loadOp,
                                      VkImageLayout initialLayout,
                                      VkImageLayout finalLayout,
                                      VkRenderPass *out)
{
    struct vk_format *vk_fmt = fmt->priv;
    assert(fmt->renderable);

    VkRenderPassCreateInfo rinfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &(VkAttachmentDescription) {
            .format = vk_fmt->iformat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = loadOp,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = initialLayout,
            .finalLayout = finalLayout,
        },
        .subpassCount = 1,
        .pSubpasses = &(VkSubpassDescription) {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &(VkAttachmentReference) {
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
        },
    };

    return vkCreateRenderPass(dev, &rinfo, MPVK_ALLOCATOR, out);
}

// For ra_tex.priv
struct ra_tex_vk {
    bool external_img;
    enum queue_type upload_queue;
    VkImageType type;
    VkImage img;
    struct vk_memslice mem;
    // for sampling
    VkImageView view;
    VkSampler sampler;
    // for rendering
    VkFramebuffer framebuffer;
    VkRenderPass dummyPass;
    // for uploading
    struct ra_buf_pool pbo;
    // "current" metadata, can change during the course of execution
    VkImageLayout current_layout;
    VkAccessFlags current_access;
    // the signal guards reuse, and can be NULL
    struct vk_signal *sig;
    VkPipelineStageFlags sig_stage;
    VkSemaphore ext_dep; // external semaphore, not owned by the ra_tex
};

void ra_tex_vk_external_dep(struct ra *ra, struct ra_tex *tex, VkSemaphore dep)
{
    struct ra_tex_vk *tex_vk = tex->priv;
    assert(!tex_vk->ext_dep);
    tex_vk->ext_dep = dep;
}

// Small helper to ease image barrier creation. if `discard` is set, the contents
// of the image will be undefined after the barrier
static void tex_barrier(struct ra *ra, struct vk_cmd *cmd, struct ra_tex *tex,
                        VkPipelineStageFlags stage, VkAccessFlags newAccess,
                        VkImageLayout newLayout, bool discard)
{
    struct mpvk_ctx *vk = ra_vk_get(ra);
    struct ra_tex_vk *tex_vk = tex->priv;

    if (tex_vk->ext_dep) {
        vk_cmd_dep(cmd, tex_vk->ext_dep, stage);
        tex_vk->ext_dep = NULL;
    }

    VkImageMemoryBarrier imgBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = tex_vk->current_layout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .srcAccessMask = tex_vk->current_access,
        .dstAccessMask = newAccess,
        .image = tex_vk->img,
        .subresourceRange = vk_range,
    };

    if (discard) {
        imgBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgBarrier.srcAccessMask = 0;
    }

    VkEvent event = NULL;
    vk_cmd_wait(vk, cmd, &tex_vk->sig, stage, &event);

    bool need_trans = tex_vk->current_layout != newLayout ||
                      tex_vk->current_access != newAccess;

    // Transitioning to VK_IMAGE_LAYOUT_UNDEFINED is a pseudo-operation
    // that for us means we don't need to perform the actual transition
    if (need_trans && newLayout != VK_IMAGE_LAYOUT_UNDEFINED) {
        if (event) {
            vkCmdWaitEvents(cmd->buf, 1, &event, tex_vk->sig_stage,
                            stage, 0, NULL, 0, NULL, 1, &imgBarrier);
        } else {
            // If we're not using an event, then the source stage is irrelevant
            // because we're coming from a different queue anyway, so we can
            // safely set it to TOP_OF_PIPE.
            imgBarrier.srcAccessMask = 0;
            vkCmdPipelineBarrier(cmd->buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 stage, 0, 0, NULL, 0, NULL, 1, &imgBarrier);
        }
    }

    tex_vk->current_layout = newLayout;
    tex_vk->current_access = newAccess;
}

static void tex_signal(struct ra *ra, struct vk_cmd *cmd, struct ra_tex *tex,
                       VkPipelineStageFlags stage)
{
    struct ra_tex_vk *tex_vk = tex->priv;
    struct mpvk_ctx *vk = ra_vk_get(ra);
    assert(!tex_vk->sig);

    tex_vk->sig = vk_cmd_signal(vk, cmd, stage);
    tex_vk->sig_stage = stage;
}

static void vk_tex_destroy(struct ra *ra, struct ra_tex *tex)
{
    if (!tex)
        return;

    struct mpvk_ctx *vk = ra_vk_get(ra);
    struct ra_tex_vk *tex_vk = tex->priv;

    ra_buf_pool_uninit(ra, &tex_vk->pbo);
    vk_signal_destroy(vk, &tex_vk->sig);
    vkDestroyFramebuffer(vk->dev, tex_vk->framebuffer, MPVK_ALLOCATOR);
    vkDestroyRenderPass(vk->dev, tex_vk->dummyPass, MPVK_ALLOCATOR);
    vkDestroySampler(vk->dev, tex_vk->sampler, MPVK_ALLOCATOR);
    vkDestroyImageView(vk->dev, tex_vk->view, MPVK_ALLOCATOR);
    if (!tex_vk->external_img) {
        vkDestroyImage(vk->dev, tex_vk->img, MPVK_ALLOCATOR);
        vk_free_memslice(vk, tex_vk->mem);
    }

    talloc_free(tex);
}

MAKE_LAZY_DESTRUCTOR(vk_tex_destroy, struct ra_tex);

// Initializes non-VkImage values like the image view, samplers, etc.
static bool vk_init_image(struct ra *ra, struct ra_tex *tex)
{
    struct mpvk_ctx *vk = ra_vk_get(ra);

    struct ra_tex_params *params = &tex->params;
    struct ra_tex_vk *tex_vk = tex->priv;
    assert(tex_vk->img);

    tex_vk->current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    tex_vk->current_access = 0;

    if (params->render_src || params->render_dst) {
        static const VkImageViewType viewType[] = {
            [VK_IMAGE_TYPE_1D] = VK_IMAGE_VIEW_TYPE_1D,
            [VK_IMAGE_TYPE_2D] = VK_IMAGE_VIEW_TYPE_2D,
            [VK_IMAGE_TYPE_3D] = VK_IMAGE_VIEW_TYPE_3D,
        };

        const struct vk_format *fmt = params->format->priv;
        VkImageViewCreateInfo vinfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = tex_vk->img,
            .viewType = viewType[tex_vk->type],
            .format = fmt->iformat,
            .subresourceRange = vk_range,
        };

        VK(vkCreateImageView(vk->dev, &vinfo, MPVK_ALLOCATOR, &tex_vk->view));
    }

    if (params->render_src) {
        assert(params->format->linear_filter || !params->src_linear);
        VkFilter filter = params->src_linear
            ? VK_FILTER_LINEAR
            : VK_FILTER_NEAREST;
        VkSamplerAddressMode wrap = params->src_repeat
            ? VK_SAMPLER_ADDRESS_MODE_REPEAT
            : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VkSamplerCreateInfo sinfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = filter,
            .minFilter = filter,
            .addressModeU = wrap,
            .addressModeV = wrap,
            .addressModeW = wrap,
            .maxAnisotropy = 1.0,
        };

        VK(vkCreateSampler(vk->dev, &sinfo, MPVK_ALLOCATOR, &tex_vk->sampler));
    }

    if (params->render_dst) {
        // Framebuffers need to be created against a specific render pass
        // layout, so we need to temporarily create a skeleton/dummy render
        // pass for vulkan to figure out the compatibility
        VK(vk_create_render_pass(vk->dev, params->format,
                                 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                 VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                 &tex_vk->dummyPass));

        VkFramebufferCreateInfo finfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = tex_vk->dummyPass,
            .attachmentCount = 1,
            .pAttachments = &tex_vk->view,
            .width = tex->params.w,
            .height = tex->params.h,
            .layers = 1,
        };

        VK(vkCreateFramebuffer(vk->dev, &finfo, MPVK_ALLOCATOR,
                               &tex_vk->framebuffer));

        // NOTE: Normally we would free the dummyPass again here, but a bug
        // in the nvidia vulkan driver causes a segfault if you do.
    }

    return true;

error:
    return false;
}

static struct ra_tex *vk_tex_create(struct ra *ra,
                                    const struct ra_tex_params *params)
{
    struct mpvk_ctx *vk = ra_vk_get(ra);
    assert(!params->format->dummy_format);

    struct ra_tex *tex = talloc_zero(NULL, struct ra_tex);
    tex->params = *params;
    tex->params.initial_data = NULL;

    struct ra_tex_vk *tex_vk = tex->priv = talloc_zero(tex, struct ra_tex_vk);
    tex_vk->upload_queue = GRAPHICS;

    const struct vk_format *fmt = params->format->priv;
    switch (params->dimensions) {
    case 1: tex_vk->type = VK_IMAGE_TYPE_1D; break;
    case 2: tex_vk->type = VK_IMAGE_TYPE_2D; break;
    case 3: tex_vk->type = VK_IMAGE_TYPE_3D; break;
    default: abort();
    }

    VkImageUsageFlags usage = 0;
    if (params->render_src)
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (params->render_dst)
        usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (params->storage_dst)
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (params->blit_src)
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (params->host_mutable || params->blit_dst || params->initial_data)
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // Always use the transfer pool if available, for efficiency
    if (params->host_mutable && vk->pool_transfer)
        tex_vk->upload_queue = TRANSFER;

    // Double-check image usage support and fail immediately if invalid
    VkImageFormatProperties iprop;
    VkResult res = vkGetPhysicalDeviceImageFormatProperties(vk->physd,
            fmt->iformat, tex_vk->type, VK_IMAGE_TILING_OPTIMAL, usage, 0,
            &iprop);
    if (res == VK_ERROR_FORMAT_NOT_SUPPORTED) {
        return NULL;
    } else {
        VK_ASSERT(res, "Querying image format properties");
    }

    VkFormatProperties prop;
    vkGetPhysicalDeviceFormatProperties(vk->physd, fmt->iformat, &prop);
    VkFormatFeatureFlags flags = prop.optimalTilingFeatures;

    bool has_blit_src   = flags & VK_FORMAT_FEATURE_BLIT_SRC_BIT,
         has_src_linear = flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

    if (params->w > iprop.maxExtent.width ||
        params->h > iprop.maxExtent.height ||
        params->d > iprop.maxExtent.depth ||
        (params->blit_src && !has_blit_src) ||
        (params->src_linear && !has_src_linear))
    {
        return NULL;
    }

    // FIXME: Since we can't keep track of queue family ownership properly,
    // and we don't know in advance what types of queue families this image
    // will belong to, we're forced to share all of our images between all
    // command pools.
    uint32_t qfs[3] = {0};
    for (int i = 0; i < vk->num_pools; i++)
        qfs[i] = vk->pools[i]->qf;

    VkImageCreateInfo iinfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = tex_vk->type,
        .format = fmt->iformat,
        .extent = (VkExtent3D) { params->w, params->h, params->d },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .sharingMode = vk->num_pools > 1 ? VK_SHARING_MODE_CONCURRENT
                                         : VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = vk->num_pools,
        .pQueueFamilyIndices = qfs,
    };

    VK(vkCreateImage(vk->dev, &iinfo, MPVK_ALLOCATOR, &tex_vk->img));

    VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkMemoryRequirements reqs;
    vkGetImageMemoryRequirements(vk->dev, tex_vk->img, &reqs);

    struct vk_memslice *mem = &tex_vk->mem;
    if (!vk_malloc_generic(vk, reqs, memFlags, mem))
        goto error;

    VK(vkBindImageMemory(vk->dev, tex_vk->img, mem->vkmem, mem->offset));

    if (!vk_init_image(ra, tex))
        goto error;

    if (params->initial_data) {
        struct ra_tex_upload_params ul_params = {
            .tex = tex,
            .invalidate = true,
            .src = params->initial_data,
            .stride = params->w * fmt->bytes,
        };
        if (!ra->fns->tex_upload(ra, &ul_params))
            goto error;
    }

    return tex;

error:
    vk_tex_destroy(ra, tex);
    return NULL;
}

struct ra_tex *ra_vk_wrap_swapchain_img(struct ra *ra, VkImage vkimg,
                                        VkSwapchainCreateInfoKHR info)
{
    struct mpvk_ctx *vk = ra_vk_get(ra);
    struct ra_tex *tex = NULL;

    const struct ra_format *format = NULL;
    for (int i = 0; i < ra->num_formats; i++) {
        const struct vk_format *fmt = ra->formats[i]->priv;
        if (fmt->iformat == vk->surf_format.format) {
            format = ra->formats[i];
            break;
        }
    }

    if (!format) {
        MP_ERR(ra, "Could not find ra_format suitable for wrapped swchain image "
                   "with surface format 0x%x\n", vk->surf_format.format);
        goto error;
    }

    tex = talloc_zero(NULL, struct ra_tex);
    tex->params = (struct ra_tex_params) {
        .format = format,
        .dimensions = 2,
        .w = info.imageExtent.width,
        .h = info.imageExtent.height,
        .d = 1,
        .blit_src    = !!(info.imageUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
        .blit_dst    = !!(info.imageUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT),
        .render_src  = !!(info.imageUsage & VK_IMAGE_USAGE_SAMPLED_BIT),
        .render_dst  = !!(info.imageUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
        .storage_dst = !!(info.imageUsage & VK_IMAGE_USAGE_STORAGE_BIT),
    };

    struct ra_tex_vk *tex_vk = tex->priv = talloc_zero(tex, struct ra_tex_vk);
    tex_vk->type = VK_IMAGE_TYPE_2D;
    tex_vk->external_img = true;
    tex_vk->img = vkimg;

    if (!vk_init_image(ra, tex))
        goto error;

    return tex;

error:
    vk_tex_destroy(ra, tex);
    return NULL;
}

// For ra_buf.priv
struct ra_buf_vk {
    struct vk_bufslice slice;
    int refcount; // 1 = object allocated but not in use, > 1 = in use
    bool needsflush;
    enum queue_type update_queue;
    // "current" metadata, can change during course of execution
    VkPipelineStageFlags current_stage;
    VkAccessFlags current_access;
};

static void vk_buf_deref(struct ra *ra, struct ra_buf *buf)
{
    if (!buf)
        return;

    struct mpvk_ctx *vk = ra_vk_get(ra);
    struct ra_buf_vk *buf_vk = buf->priv;

    if (--buf_vk->refcount == 0) {
        vk_free_memslice(vk, buf_vk->slice.mem);
        talloc_free(buf);
    }
}

static void buf_barrier(struct ra *ra, struct vk_cmd *cmd, struct ra_buf *buf,
                        VkPipelineStageFlags newStage,
                        VkAccessFlags newAccess, int offset, size_t size)
{
    struct ra_buf_vk *buf_vk = buf->priv;

    VkBufferMemoryBarrier buffBarrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = buf_vk->current_access,
        .dstAccessMask = newAccess,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = buf_vk->slice.buf,
        .offset = offset,
        .size = size,
    };

    if (buf_vk->needsflush || buf->params.host_mapped) {
        buffBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        buf_vk->current_stage = VK_PIPELINE_STAGE_HOST_BIT;
        buf_vk->needsflush = false;
    }

    if (buffBarrier.srcAccessMask != buffBarrier.dstAccessMask) {
        vkCmdPipelineBarrier(cmd->buf, buf_vk->current_stage, newStage, 0,
                             0, NULL, 1, &buffBarrier, 0, NULL);
    }

    buf_vk->current_stage = newStage;
    buf_vk->current_access = newAccess;
    buf_vk->refcount++;
    vk_cmd_callback(cmd, (vk_cb) vk_buf_deref, ra, buf);
}

#define vk_buf_destroy vk_buf_deref
MAKE_LAZY_DESTRUCTOR(vk_buf_destroy, struct ra_buf);

static void vk_buf_update(struct ra *ra, struct ra_buf *buf, ptrdiff_t offset,
                          const void *data, size_t size)
{
    assert(buf->params.host_mutable || buf->params.initial_data);
    struct ra_buf_vk *buf_vk = buf->priv;

    // For host-mapped buffers, we can just directly memcpy the buffer contents.
    // Otherwise, we can update the buffer from the GPU using a command buffer.
    if (buf_vk->slice.data) {
        assert(offset + size <= buf->params.size);
        uintptr_t addr = (uintptr_t)buf_vk->slice.data + offset;
        memcpy((void *)addr, data, size);
        buf_vk->needsflush = true;
    } else {
        struct vk_cmd *cmd = vk_require_cmd(ra, buf_vk->update_queue);
        if (!cmd) {
            MP_ERR(ra, "Failed updating buffer!\n");
            return;
        }

        buf_barrier(ra, cmd, buf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_ACCESS_TRANSFER_WRITE_BIT, offset, size);

        VkDeviceSize bufOffset = buf_vk->slice.mem.offset + offset;
        assert(bufOffset == MP_ALIGN_UP(bufOffset, 4));
        vkCmdUpdateBuffer(cmd->buf, buf_vk->slice.buf, bufOffset, size, data);
    }
}

static struct ra_buf *vk_buf_create(struct ra *ra,
                                    const struct ra_buf_params *params)
{
    struct mpvk_ctx *vk = ra_vk_get(ra);

    struct ra_buf *buf = talloc_zero(NULL, struct ra_buf);
    buf->params = *params;

    struct ra_buf_vk *buf_vk = buf->priv = talloc_zero(buf, struct ra_buf_vk);
    buf_vk->current_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    buf_vk->current_access = 0;
    buf_vk->refcount = 1;

    VkBufferUsageFlags bufFlags = 0;
    VkMemoryPropertyFlags memFlags = 0;
    VkDeviceSize align = 4; // alignment 4 is needed for buf_update

    switch (params->type) {
    case RA_BUF_TYPE_TEX_UPLOAD:
        bufFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        memFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        // Use TRANSFER-style updates for large enough buffers for efficiency
        if (params->size > 1024*1024) // 1 MB
            buf_vk->update_queue = TRANSFER;
        break;
    case RA_BUF_TYPE_UNIFORM:
        bufFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        memFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        align = MP_ALIGN_UP(align, vk->limits.minUniformBufferOffsetAlignment);
        break;
    case RA_BUF_TYPE_SHADER_STORAGE:
        bufFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        memFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        align = MP_ALIGN_UP(align, vk->limits.minStorageBufferOffsetAlignment);
        buf_vk->update_queue = COMPUTE;
        break;
    case RA_BUF_TYPE_VERTEX:
        bufFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        memFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        break;
    default: abort();
    }

    if (params->host_mutable || params->initial_data) {
        bufFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        align = MP_ALIGN_UP(align, vk->limits.optimalBufferCopyOffsetAlignment);
    }

    if (params->host_mapped) {
        memFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                    VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    }

    if (!vk_malloc_buffer(vk, bufFlags, memFlags, params->size, align,
                          &buf_vk->slice))
    {
        goto error;
    }

    if (params->host_mapped)
        buf->data = buf_vk->slice.data;

    if (params->initial_data)
        vk_buf_update(ra, buf, 0, params->initial_data, params->size);

    buf->params.initial_data = NULL; // do this after vk_buf_update
    return buf;

error:
    vk_buf_destroy(ra, buf);
    return NULL;
}

static bool vk_buf_poll(struct ra *ra, struct ra_buf *buf)
{
    struct ra_buf_vk *buf_vk = buf->priv;
    return buf_vk->refcount == 1;
}

static bool vk_tex_upload(struct ra *ra,
                          const struct ra_tex_upload_params *params)
{
    struct ra_tex *tex = params->tex;
    struct ra_tex_vk *tex_vk = tex->priv;

    if (!params->buf)
        return ra_tex_upload_pbo(ra, &tex_vk->pbo, params);

    assert(!params->src);
    assert(params->buf);
    struct ra_buf *buf = params->buf;
    struct ra_buf_vk *buf_vk = buf->priv;

    VkBufferImageCopy region = {
        .bufferOffset = buf_vk->slice.mem.offset + params->buf_offset,
        .bufferRowLength = tex->params.w,
        .bufferImageHeight = tex->params.h,
        .imageSubresource = vk_layers,
        .imageExtent = (VkExtent3D){tex->params.w, tex->params.h, tex->params.d},
    };

    if (tex->params.dimensions == 2) {
        int pix_size = tex->params.format->pixel_size;
        region.bufferRowLength = params->stride / pix_size;
        if (region.bufferRowLength * pix_size != params->stride) {
            MP_ERR(ra, "Texture upload strides must be a multiple of the texel "
                       "size!\n");
            goto error;
        }

        if (params->rc) {
            struct mp_rect *rc = params->rc;
            region.imageOffset = (VkOffset3D){rc->x0, rc->y0, 0};
            region.imageExtent = (VkExtent3D){mp_rect_w(*rc), mp_rect_h(*rc), 1};
        }
    }

    uint64_t size = region.bufferRowLength * region.bufferImageHeight *
                    region.imageExtent.depth;

    struct vk_cmd *cmd = vk_require_cmd(ra, tex_vk->upload_queue);
    if (!cmd)
        goto error;

    buf_barrier(ra, cmd, buf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_READ_BIT, region.bufferOffset, size);

    tex_barrier(ra, cmd, tex, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                params->invalidate);

    vkCmdCopyBufferToImage(cmd->buf, buf_vk->slice.buf, tex_vk->img,
                           tex_vk->current_layout, 1, &region);

    tex_signal(ra, cmd, tex, VK_PIPELINE_STAGE_TRANSFER_BIT);

    return true;

error:
    return false;
}

#define MPVK_NUM_DS MPVK_MAX_STREAMING_DEPTH

// For ra_renderpass.priv
struct ra_renderpass_vk {
    // Pipeline / render pass
    VkPipeline pipe;
    VkPipelineLayout pipeLayout;
    VkRenderPass renderPass;
    VkImageLayout initialLayout;
    VkImageLayout finalLayout;
    // Descriptor set (bindings)
    VkDescriptorSetLayout dsLayout;
    VkDescriptorPool dsPool;
    VkDescriptorSet dss[MPVK_NUM_DS];
    int dindex;
    // Vertex buffers (vertices)
    struct ra_buf_pool vbo;

    // For updating
    VkWriteDescriptorSet *dswrite;
    VkDescriptorImageInfo *dsiinfo;
    VkDescriptorBufferInfo *dsbinfo;
};

static void vk_renderpass_destroy(struct ra *ra, struct ra_renderpass *pass)
{
    if (!pass)
        return;

    struct mpvk_ctx *vk = ra_vk_get(ra);
    struct ra_renderpass_vk *pass_vk = pass->priv;

    ra_buf_pool_uninit(ra, &pass_vk->vbo);
    vkDestroyPipeline(vk->dev, pass_vk->pipe, MPVK_ALLOCATOR);
    vkDestroyRenderPass(vk->dev, pass_vk->renderPass, MPVK_ALLOCATOR);
    vkDestroyPipelineLayout(vk->dev, pass_vk->pipeLayout, MPVK_ALLOCATOR);
    vkDestroyDescriptorPool(vk->dev, pass_vk->dsPool, MPVK_ALLOCATOR);
    vkDestroyDescriptorSetLayout(vk->dev, pass_vk->dsLayout, MPVK_ALLOCATOR);

    talloc_free(pass);
}

MAKE_LAZY_DESTRUCTOR(vk_renderpass_destroy, struct ra_renderpass);

static const VkDescriptorType dsType[] = {
    [RA_VARTYPE_TEX]    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    [RA_VARTYPE_IMG_W]  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    [RA_VARTYPE_BUF_RO] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    [RA_VARTYPE_BUF_RW] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
};

static bool vk_get_input_format(struct ra *ra, struct ra_renderpass_input *inp,
                                VkFormat *out_fmt)
{
    struct mpvk_ctx *vk = ra_vk_get(ra);

    enum ra_ctype ctype;
    switch (inp->type) {
    case RA_VARTYPE_FLOAT:      ctype = RA_CTYPE_FLOAT; break;
    case RA_VARTYPE_BYTE_UNORM: ctype = RA_CTYPE_UNORM; break;
    default: abort();
    }

    assert(inp->dim_m == 1);
    for (const struct vk_format *fmt = vk_formats; fmt->name; fmt++) {
        if (fmt->ctype != ctype)
            continue;
        if (fmt->components != inp->dim_v)
            continue;
        if (fmt->bytes != ra_renderpass_input_layout(inp).size)
            continue;

        // Ensure this format is valid for vertex attributes
        VkFormatProperties prop;
        vkGetPhysicalDeviceFormatProperties(vk->physd, fmt->iformat, &prop);
        if (!(prop.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT))
            continue;

        *out_fmt = fmt->iformat;
        return true;
    }

    return false;
}

static const char vk_cache_magic[4] = {'R','A','V','K'};
static const int vk_cache_version = 2;

struct vk_cache_header {
    char magic[sizeof(vk_cache_magic)];
    int cache_version;
    char compiler[SPIRV_NAME_MAX_LEN];
    int compiler_version;
    size_t vert_spirv_len;
    size_t frag_spirv_len;
    size_t comp_spirv_len;
    size_t pipecache_len;
};

static bool vk_use_cached_program(const struct ra_renderpass_params *params,
                                  const struct spirv_compiler *spirv,
                                  struct bstr *vert_spirv,
                                  struct bstr *frag_spirv,
                                  struct bstr *comp_spirv,
                                  struct bstr *pipecache)
{
    struct bstr cache = params->cached_program;
    if (cache.len < sizeof(struct vk_cache_header))
        return false;

    struct vk_cache_header *header = (struct vk_cache_header *)cache.start;
    cache = bstr_cut(cache, sizeof(*header));

    if (strncmp(header->magic, vk_cache_magic, sizeof(vk_cache_magic)) != 0)
        return false;
    if (header->cache_version != vk_cache_version)
        return false;
    if (strncmp(header->compiler, spirv->name, sizeof(header->compiler)) != 0)
        return false;
    if (header->compiler_version != spirv->compiler_version)
        return false;

#define GET(ptr) \
    if (cache.len < header->ptr##_len)                      \
            return false;                                   \
        *ptr = bstr_splice(cache, 0, header->ptr##_len);    \
        cache = bstr_cut(cache, ptr->len);

    GET(vert_spirv);
    GET(frag_spirv);
    GET(comp_spirv);
    GET(pipecache);
    return true;
}

static VkResult vk_compile_glsl(struct ra *ra, void *tactx,
                                enum glsl_shader type, const char *glsl,
                                struct bstr *spirv)
{
    struct mpvk_ctx *vk = ra_vk_get(ra);
    VkResult ret = VK_SUCCESS;
    int msgl = MSGL_DEBUG;

    if (!vk->spirv->fns->compile_glsl(vk->spirv, tactx, type, glsl, spirv)) {
        ret = VK_ERROR_INVALID_SHADER_NV;
        msgl = MSGL_ERR;
    }

    static const char *shader_names[] = {
        [GLSL_SHADER_VERTEX]   = "vertex",
        [GLSL_SHADER_FRAGMENT] = "fragment",
        [GLSL_SHADER_COMPUTE]  = "compute",
    };

    if (mp_msg_test(ra->log, msgl)) {
        MP_MSG(ra, msgl, "%s shader source:\n", shader_names[type]);
        mp_log_source(ra->log, msgl, glsl);
    }
    return ret;
}

static const VkShaderStageFlags stageFlags[] = {
    [RA_RENDERPASS_TYPE_RASTER]  = VK_SHADER_STAGE_FRAGMENT_BIT,
    [RA_RENDERPASS_TYPE_COMPUTE] = VK_SHADER_STAGE_COMPUTE_BIT,
};

static struct ra_renderpass *vk_renderpass_create(struct ra *ra,
                                    const struct ra_renderpass_params *params)
{
    struct mpvk_ctx *vk = ra_vk_get(ra);
    bool success = false;
    assert(vk->spirv);

    struct ra_renderpass *pass = talloc_zero(NULL, struct ra_renderpass);
    pass->params = *ra_renderpass_params_copy(pass, params);
    pass->params.cached_program = (bstr){0};
    struct ra_renderpass_vk *pass_vk = pass->priv =
        talloc_zero(pass, struct ra_renderpass_vk);

    // temporary allocations/objects
    void *tmp = talloc_new(NULL);
    VkPipelineCache pipeCache = NULL;
    VkShaderModule vert_shader = NULL;
    VkShaderModule frag_shader = NULL;
    VkShaderModule comp_shader = NULL;

    static int dsCount[RA_VARTYPE_COUNT] = {0};
    VkDescriptorSetLayoutBinding *bindings = NULL;
    int num_bindings = 0;

    for (int i = 0; i < params->num_inputs; i++) {
        struct ra_renderpass_input *inp = &params->inputs[i];
        switch (inp->type) {
        case RA_VARTYPE_TEX:
        case RA_VARTYPE_IMG_W:
        case RA_VARTYPE_BUF_RO:
        case RA_VARTYPE_BUF_RW: {
            VkDescriptorSetLayoutBinding desc = {
                .binding = inp->binding,
                .descriptorType = dsType[inp->type],
                .descriptorCount = 1,
                .stageFlags = stageFlags[params->type],
            };

            MP_TARRAY_APPEND(tmp, bindings, num_bindings, desc);
            dsCount[inp->type]++;
            break;
        }
        default: abort();
        }
    }

    VkDescriptorPoolSize *dsPoolSizes = NULL;
    int poolSizeCount = 0;

    for (enum ra_vartype t = 0; t < RA_VARTYPE_COUNT; t++) {
        if (dsCount[t] > 0) {
            VkDescriptorPoolSize dssize = {
                .type = dsType[t],
                .descriptorCount = dsCount[t] * MPVK_NUM_DS,
            };

            MP_TARRAY_APPEND(tmp, dsPoolSizes, poolSizeCount, dssize);
        }
    }

    VkDescriptorPoolCreateInfo pinfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = MPVK_NUM_DS,
        .pPoolSizes = dsPoolSizes,
        .poolSizeCount = poolSizeCount,
    };

    VK(vkCreateDescriptorPool(vk->dev, &pinfo, MPVK_ALLOCATOR, &pass_vk->dsPool));

    pass_vk->dswrite = talloc_array(pass, VkWriteDescriptorSet, num_bindings);
    pass_vk->dsiinfo = talloc_array(pass, VkDescriptorImageInfo, num_bindings);
    pass_vk->dsbinfo = talloc_array(pass, VkDescriptorBufferInfo, num_bindings);

    VkDescriptorSetLayoutCreateInfo dinfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pBindings = bindings,
        .bindingCount = num_bindings,
    };

    VK(vkCreateDescriptorSetLayout(vk->dev, &dinfo, MPVK_ALLOCATOR,
                                   &pass_vk->dsLayout));

    VkDescriptorSetLayout layouts[MPVK_NUM_DS];
    for (int i = 0; i < MPVK_NUM_DS; i++)
        layouts[i] = pass_vk->dsLayout;

    VkDescriptorSetAllocateInfo ainfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pass_vk->dsPool,
        .descriptorSetCount = MPVK_NUM_DS,
        .pSetLayouts = layouts,
    };

    VK(vkAllocateDescriptorSets(vk->dev, &ainfo, pass_vk->dss));

    VkPipelineLayoutCreateInfo linfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &pass_vk->dsLayout,
        .pushConstantRangeCount = params->push_constants_size ? 1 : 0,
        .pPushConstantRanges = &(VkPushConstantRange){
            .stageFlags = stageFlags[params->type],
            .offset = 0,
            .size = params->push_constants_size,
        },
    };

    VK(vkCreatePipelineLayout(vk->dev, &linfo, MPVK_ALLOCATOR,
                              &pass_vk->pipeLayout));

    struct bstr vert = {0}, frag = {0}, comp = {0}, pipecache = {0};
    if (vk_use_cached_program(params, vk->spirv, &vert, &frag, &comp, &pipecache)) {
        MP_VERBOSE(ra, "Using cached SPIR-V and VkPipeline.\n");
    } else {
        pipecache.len = 0;
        switch (params->type) {
        case RA_RENDERPASS_TYPE_RASTER:
            VK(vk_compile_glsl(ra, tmp, GLSL_SHADER_VERTEX,
                               params->vertex_shader, &vert));
            VK(vk_compile_glsl(ra, tmp, GLSL_SHADER_FRAGMENT,
                               params->frag_shader, &frag));
            comp.len = 0;
            break;
        case RA_RENDERPASS_TYPE_COMPUTE:
            VK(vk_compile_glsl(ra, tmp, GLSL_SHADER_COMPUTE,
                               params->compute_shader, &comp));
            frag.len = 0;
            vert.len = 0;
            break;
        }
    }

    VkPipelineCacheCreateInfo pcinfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .pInitialData = pipecache.start,
        .initialDataSize = pipecache.len,
    };

    VK(vkCreatePipelineCache(vk->dev, &pcinfo, MPVK_ALLOCATOR, &pipeCache));

    VkShaderModuleCreateInfo sinfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    };

    switch (params->type) {
    case RA_RENDERPASS_TYPE_RASTER: {
        sinfo.pCode = (uint32_t *)vert.start;
        sinfo.codeSize = vert.len;
        VK(vkCreateShaderModule(vk->dev, &sinfo, MPVK_ALLOCATOR, &vert_shader));

        sinfo.pCode = (uint32_t *)frag.start;
        sinfo.codeSize = frag.len;
        VK(vkCreateShaderModule(vk->dev, &sinfo, MPVK_ALLOCATOR, &frag_shader));

        VkVertexInputAttributeDescription *attrs = talloc_array(tmp,
                VkVertexInputAttributeDescription, params->num_vertex_attribs);

        for (int i = 0; i < params->num_vertex_attribs; i++) {
            struct ra_renderpass_input *inp = &params->vertex_attribs[i];
            attrs[i] = (VkVertexInputAttributeDescription) {
                .location = i,
                .binding = 0,
                .offset = inp->offset,
            };

            if (!vk_get_input_format(ra, inp, &attrs[i].format)) {
                MP_ERR(ra, "No suitable VkFormat for vertex attrib '%s'!\n",
                       inp->name);
                goto error;
            }
        }

        // This is the most common case, so optimize towards it. In this case,
        // the renderpass will take care of almost all layout transitions
        pass_vk->initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        pass_vk->finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

        // If we're blending, then we need to explicitly load the previous
        // contents of the color attachment
        if (pass->params.enable_blend)
            loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

        // If we're invalidating the target, we don't need to load or transition
        if (pass->params.invalidate_target) {
            pass_vk->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }

        VK(vk_create_render_pass(vk->dev, params->target_format, loadOp,
                                 pass_vk->initialLayout, pass_vk->finalLayout,
                                 &pass_vk->renderPass));

        static const VkBlendFactor blendFactors[] = {
            [RA_BLEND_ZERO]                = VK_BLEND_FACTOR_ZERO,
            [RA_BLEND_ONE]                 = VK_BLEND_FACTOR_ONE,
            [RA_BLEND_SRC_ALPHA]           = VK_BLEND_FACTOR_SRC_ALPHA,
            [RA_BLEND_ONE_MINUS_SRC_ALPHA] = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        };

        VkGraphicsPipelineCreateInfo cinfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = (VkPipelineShaderStageCreateInfo[]) {
                {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_VERTEX_BIT,
                    .module = vert_shader,
                    .pName = "main",
                }, {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .module = frag_shader,
                    .pName = "main",
                }
            },
            .pVertexInputState = &(VkPipelineVertexInputStateCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &(VkVertexInputBindingDescription) {
                    .binding = 0,
                    .stride = params->vertex_stride,
                    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                },
                .vertexAttributeDescriptionCount = params->num_vertex_attribs,
                .pVertexAttributeDescriptions = attrs,
            },
            .pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            },
            .pViewportState = &(VkPipelineViewportStateCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .scissorCount = 1,
            },
            .pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_NONE,
                .lineWidth = 1.0f,
            },
            .pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            },
            .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = 1,
                .pAttachments = &(VkPipelineColorBlendAttachmentState) {
                    .blendEnable = params->enable_blend,
                    .colorBlendOp = VK_BLEND_OP_ADD,
                    .srcColorBlendFactor = blendFactors[params->blend_src_rgb],
                    .dstColorBlendFactor = blendFactors[params->blend_dst_rgb],
                    .alphaBlendOp = VK_BLEND_OP_ADD,
                    .srcAlphaBlendFactor = blendFactors[params->blend_src_alpha],
                    .dstAlphaBlendFactor = blendFactors[params->blend_dst_alpha],
                    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                      VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT |
                                      VK_COLOR_COMPONENT_A_BIT,
                },
            },
            .pDynamicState = &(VkPipelineDynamicStateCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount = 2,
                .pDynamicStates = (VkDynamicState[]){
                    VK_DYNAMIC_STATE_VIEWPORT,
                    VK_DYNAMIC_STATE_SCISSOR,
                },
            },
            .layout = pass_vk->pipeLayout,
            .renderPass = pass_vk->renderPass,
        };

        VK(vkCreateGraphicsPipelines(vk->dev, pipeCache, 1, &cinfo,
                                     MPVK_ALLOCATOR, &pass_vk->pipe));
        break;
    }
    case RA_RENDERPASS_TYPE_COMPUTE: {
        sinfo.pCode = (uint32_t *)comp.start;
        sinfo.codeSize = comp.len;
        VK(vkCreateShaderModule(vk->dev, &sinfo, MPVK_ALLOCATOR, &comp_shader));

        VkComputePipelineCreateInfo cinfo = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = comp_shader,
                .pName = "main",
            },
            .layout = pass_vk->pipeLayout,
        };

        VK(vkCreateComputePipelines(vk->dev, pipeCache, 1, &cinfo,
                                    MPVK_ALLOCATOR, &pass_vk->pipe));
        break;
    }
    }

    // Update params->cached_program
    struct bstr cache = {0};
    VK(vkGetPipelineCacheData(vk->dev, pipeCache, &cache.len, NULL));
    cache.start = talloc_size(tmp, cache.len);
    VK(vkGetPipelineCacheData(vk->dev, pipeCache, &cache.len, cache.start));

    struct vk_cache_header header = {
        .cache_version = vk_cache_version,
        .compiler_version = vk->spirv->compiler_version,
        .vert_spirv_len = vert.len,
        .frag_spirv_len = frag.len,
        .comp_spirv_len = comp.len,
        .pipecache_len = cache.len,
    };

    for (int i = 0; i < MP_ARRAY_SIZE(header.magic); i++)
        header.magic[i] = vk_cache_magic[i];
    for (int i = 0; i < sizeof(vk->spirv->name); i++)
        header.compiler[i] = vk->spirv->name[i];

    struct bstr *prog = &pass->params.cached_program;
    bstr_xappend(pass, prog, (struct bstr){ (char *) &header, sizeof(header) });
    bstr_xappend(pass, prog, vert);
    bstr_xappend(pass, prog, frag);
    bstr_xappend(pass, prog, comp);
    bstr_xappend(pass, prog, cache);

    success = true;

error:
    if (!success) {
        vk_renderpass_destroy(ra, pass);
        pass = NULL;
    }

    vkDestroyShaderModule(vk->dev, vert_shader, MPVK_ALLOCATOR);
    vkDestroyShaderModule(vk->dev, frag_shader, MPVK_ALLOCATOR);
    vkDestroyShaderModule(vk->dev, comp_shader, MPVK_ALLOCATOR);
    vkDestroyPipelineCache(vk->dev, pipeCache, MPVK_ALLOCATOR);
    talloc_free(tmp);
    return pass;
}

static const VkPipelineStageFlags passStages[] = {
    [RA_RENDERPASS_TYPE_RASTER]  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    [RA_RENDERPASS_TYPE_COMPUTE] = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
};

static void vk_update_descriptor(struct ra *ra, struct vk_cmd *cmd,
                                 struct ra_renderpass *pass,
                                 struct ra_renderpass_input_val val,
                                 VkDescriptorSet ds, int idx)
{
    struct ra_renderpass_vk *pass_vk = pass->priv;
    struct ra_renderpass_input *inp = &pass->params.inputs[val.index];

    VkWriteDescriptorSet *wds = &pass_vk->dswrite[idx];
    *wds = (VkWriteDescriptorSet) {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = ds,
        .dstBinding = inp->binding,
        .descriptorCount = 1,
        .descriptorType = dsType[inp->type],
    };

    switch (inp->type) {
    case RA_VARTYPE_TEX: {
        struct ra_tex *tex = *(struct ra_tex **)val.data;
        struct ra_tex_vk *tex_vk = tex->priv;

        assert(tex->params.render_src);
        tex_barrier(ra, cmd, tex, passStages[pass->params.type],
                    VK_ACCESS_SHADER_READ_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);

        VkDescriptorImageInfo *iinfo = &pass_vk->dsiinfo[idx];
        *iinfo = (VkDescriptorImageInfo) {
            .sampler = tex_vk->sampler,
            .imageView = tex_vk->view,
            .imageLayout = tex_vk->current_layout,
        };

        wds->pImageInfo = iinfo;
        break;
    }
    case RA_VARTYPE_IMG_W: {
        struct ra_tex *tex = *(struct ra_tex **)val.data;
        struct ra_tex_vk *tex_vk = tex->priv;

        assert(tex->params.storage_dst);
        tex_barrier(ra, cmd, tex, passStages[pass->params.type],
                    VK_ACCESS_SHADER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_GENERAL, false);

        VkDescriptorImageInfo *iinfo = &pass_vk->dsiinfo[idx];
        *iinfo = (VkDescriptorImageInfo) {
            .imageView = tex_vk->view,
            .imageLayout = tex_vk->current_layout,
        };

        wds->pImageInfo = iinfo;
        break;
    }
    case RA_VARTYPE_BUF_RO:
    case RA_VARTYPE_BUF_RW: {
        struct ra_buf *buf = *(struct ra_buf **)val.data;
        struct ra_buf_vk *buf_vk = buf->priv;

        VkBufferUsageFlags access = VK_ACCESS_SHADER_READ_BIT;
        if (inp->type == RA_VARTYPE_BUF_RW)
            access |= VK_ACCESS_SHADER_WRITE_BIT;

        buf_barrier(ra, cmd, buf, passStages[pass->params.type],
                    access, buf_vk->slice.mem.offset, buf->params.size);

        VkDescriptorBufferInfo *binfo = &pass_vk->dsbinfo[idx];
        *binfo = (VkDescriptorBufferInfo) {
            .buffer = buf_vk->slice.buf,
            .offset = buf_vk->slice.mem.offset,
            .range = buf->params.size,
        };

        wds->pBufferInfo = binfo;
        break;
    }
    }
}

static void vk_release_descriptor(struct ra *ra, struct vk_cmd *cmd,
                                  struct ra_renderpass *pass,
                                  struct ra_renderpass_input_val val)
{
    struct ra_renderpass_input *inp = &pass->params.inputs[val.index];

    switch (inp->type) {
    case RA_VARTYPE_IMG_W:
    case RA_VARTYPE_TEX: {
        struct ra_tex *tex = *(struct ra_tex **)val.data;
        tex_signal(ra, cmd, tex, passStages[pass->params.type]);
        break;
    }
    }
}

static void vk_renderpass_run(struct ra *ra,
                              const struct ra_renderpass_run_params *params)
{
    struct mpvk_ctx *vk = ra_vk_get(ra);
    struct ra_renderpass *pass = params->pass;
    struct ra_renderpass_vk *pass_vk = pass->priv;

    static const enum queue_type types[] = {
        [RA_RENDERPASS_TYPE_RASTER]  = GRAPHICS,
        [RA_RENDERPASS_TYPE_COMPUTE] = COMPUTE,
    };

    struct vk_cmd *cmd = vk_require_cmd(ra, types[pass->params.type]);
    if (!cmd)
        goto error;

    static const VkPipelineBindPoint bindPoint[] = {
        [RA_RENDERPASS_TYPE_RASTER]  = VK_PIPELINE_BIND_POINT_GRAPHICS,
        [RA_RENDERPASS_TYPE_COMPUTE] = VK_PIPELINE_BIND_POINT_COMPUTE,
    };

    vkCmdBindPipeline(cmd->buf, bindPoint[pass->params.type], pass_vk->pipe);

    VkDescriptorSet ds = pass_vk->dss[pass_vk->dindex++];
    pass_vk->dindex %= MPVK_NUM_DS;

    for (int i = 0; i < params->num_values; i++)
        vk_update_descriptor(ra, cmd, pass, params->values[i], ds, i);

    if (params->num_values > 0) {
        vkUpdateDescriptorSets(vk->dev, params->num_values, pass_vk->dswrite,
                               0, NULL);
    }

    vkCmdBindDescriptorSets(cmd->buf, bindPoint[pass->params.type],
                            pass_vk->pipeLayout, 0, 1, &ds, 0, NULL);

    if (pass->params.push_constants_size) {
        vkCmdPushConstants(cmd->buf, pass_vk->pipeLayout,
                           stageFlags[pass->params.type], 0,
                           pass->params.push_constants_size,
                           params->push_constants);
    }

    switch (pass->params.type) {
    case RA_RENDERPASS_TYPE_COMPUTE:
        vkCmdDispatch(cmd->buf, params->compute_groups[0],
                      params->compute_groups[1],
                      params->compute_groups[2]);
        break;
    case RA_RENDERPASS_TYPE_RASTER: {
        struct ra_tex *tex = params->target;
        struct ra_tex_vk *tex_vk = tex->priv;
        assert(tex->params.render_dst);

        struct ra_buf_params buf_params = {
            .type = RA_BUF_TYPE_VERTEX,
            .size = params->vertex_count * pass->params.vertex_stride,
            .host_mutable = true,
        };

        struct ra_buf *buf = ra_buf_pool_get(ra, &pass_vk->vbo, &buf_params);
        if (!buf) {
            MP_ERR(ra, "Failed allocating vertex buffer!\n");
            goto error;
        }
        struct ra_buf_vk *buf_vk = buf->priv;

        vk_buf_update(ra, buf, 0, params->vertex_data, buf_params.size);

        buf_barrier(ra, cmd, buf, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                    buf_vk->slice.mem.offset, buf_params.size);

        vkCmdBindVertexBuffers(cmd->buf, 0, 1, &buf_vk->slice.buf,
                               &buf_vk->slice.mem.offset);

        tex_barrier(ra, cmd, tex, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, pass_vk->initialLayout,
                    pass->params.invalidate_target);

        VkViewport viewport = {
            .x = params->viewport.x0,
            .y = params->viewport.y0,
            .width  = mp_rect_w(params->viewport),
            .height = mp_rect_h(params->viewport),
        };

        VkRect2D scissor = {
            .offset = {params->scissors.x0, params->scissors.y0},
            .extent = {mp_rect_w(params->scissors), mp_rect_h(params->scissors)},
        };

        vkCmdSetViewport(cmd->buf, 0, 1, &viewport);
        vkCmdSetScissor(cmd->buf, 0, 1, &scissor);

        VkRenderPassBeginInfo binfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = pass_vk->renderPass,
            .framebuffer = tex_vk->framebuffer,
            .renderArea = (VkRect2D){{0, 0}, {tex->params.w, tex->params.h}},
        };

        vkCmdBeginRenderPass(cmd->buf, &binfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdDraw(cmd->buf, params->vertex_count, 1, 0, 0);
        vkCmdEndRenderPass(cmd->buf);

        // The renderPass implicitly transitions the texture to this layout
        tex_vk->current_layout = pass_vk->finalLayout;
        tex_vk->current_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        tex_signal(ra, cmd, tex, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        break;
    }
    default: abort();
    };

    for (int i = 0; i < params->num_values; i++)
        vk_release_descriptor(ra, cmd, pass, params->values[i]);

    // flush the work so far into its own command buffer, for better cross-frame
    // granularity
    vk_submit(ra);

error:
    return;
}

static void vk_blit(struct ra *ra, struct ra_tex *dst, struct ra_tex *src,
                    struct mp_rect *dst_rc, struct mp_rect *src_rc)
{
    assert(src->params.blit_src);
    assert(dst->params.blit_dst);

    struct ra_tex_vk *src_vk = src->priv;
    struct ra_tex_vk *dst_vk = dst->priv;

    struct vk_cmd *cmd = vk_require_cmd(ra, GRAPHICS);
    if (!cmd)
        return;

    tex_barrier(ra, cmd, src, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                false);

    bool discard = dst_rc->x0 == 0 &&
                   dst_rc->y0 == 0 &&
                   dst_rc->x1 == dst->params.w &&
                   dst_rc->y1 == dst->params.h;

    tex_barrier(ra, cmd, dst, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                discard);

    // Under certain conditions we can use vkCmdCopyImage instead of
    // vkCmdBlitImage, namely when the blit operation does not require
    // scaling. and the formats are compatible.
    if (src->params.format->pixel_size == dst->params.format->pixel_size &&
        mp_rect_w(*src_rc) == mp_rect_w(*dst_rc) &&
        mp_rect_h(*src_rc) == mp_rect_h(*dst_rc) &&
        mp_rect_w(*src_rc) >= 0 && mp_rect_h(*src_rc) >= 0)
    {
        VkImageCopy region = {
            .srcSubresource = vk_layers,
            .dstSubresource = vk_layers,
            .srcOffset = {src_rc->x0, src_rc->y0, 0},
            .dstOffset = {dst_rc->x0, dst_rc->y0, 0},
            .extent = {mp_rect_w(*src_rc), mp_rect_h(*src_rc), 1},
        };

        vkCmdCopyImage(cmd->buf, src_vk->img, src_vk->current_layout,
                       dst_vk->img, dst_vk->current_layout, 1, &region);
    } else {
        VkImageBlit region = {
            .srcSubresource = vk_layers,
            .dstSubresource = vk_layers,
            .srcOffsets = {{src_rc->x0, src_rc->y0, 0},
                           {src_rc->x1, src_rc->y1, 1}},
            .dstOffsets = {{dst_rc->x0, dst_rc->y0, 0},
                           {dst_rc->x1, dst_rc->y1, 1}},
        };

        vkCmdBlitImage(cmd->buf, src_vk->img, src_vk->current_layout,
                       dst_vk->img, dst_vk->current_layout, 1, &region,
                       VK_FILTER_NEAREST);
    }

    tex_signal(ra, cmd, src, VK_PIPELINE_STAGE_TRANSFER_BIT);
    tex_signal(ra, cmd, dst, VK_PIPELINE_STAGE_TRANSFER_BIT);
}

static void vk_clear(struct ra *ra, struct ra_tex *tex, float color[4],
                     struct mp_rect *rc)
{
    struct ra_vk *p = ra->priv;
    struct ra_tex_vk *tex_vk = tex->priv;
    assert(tex->params.blit_dst);

    struct vk_cmd *cmd = vk_require_cmd(ra, GRAPHICS);
    if (!cmd)
        return;

    struct mp_rect full = {0, 0, tex->params.w, tex->params.h};
    if (!rc || mp_rect_equals(rc, &full)) {
        // To clear the entire image, we can use the efficient clear command
        tex_barrier(ra, cmd, tex, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true);

        VkClearColorValue clearColor = {0};
        for (int c = 0; c < 4; c++)
            clearColor.float32[c] = color[c];

        vkCmdClearColorImage(cmd->buf, tex_vk->img, tex_vk->current_layout,
                             &clearColor, 1, &vk_range);

        tex_signal(ra, cmd, tex, VK_PIPELINE_STAGE_TRANSFER_BIT);
    } else {
        // To simulate per-region clearing, we blit from a 1x1 texture instead
        struct ra_tex_upload_params ul_params = {
            .tex = p->clear_tex,
            .invalidate = true,
            .src = &color[0],
        };
        vk_tex_upload(ra, &ul_params);
        vk_blit(ra, tex, p->clear_tex, rc, &(struct mp_rect){0, 0, 1, 1});
    }
}

static int vk_desc_namespace(enum ra_vartype type)
{
    return 0;
}

#define VK_QUERY_POOL_SIZE (MPVK_MAX_STREAMING_DEPTH * 4)

struct vk_timer {
    VkQueryPool pool;
    int index_seen; // keeps track of which indices have been used at least once
    int index;
    uint64_t result;
};

static void vk_timer_destroy(struct ra *ra, ra_timer *ratimer)
{
    if (!ratimer)
        return;

    struct mpvk_ctx *vk = ra_vk_get(ra);
    struct vk_timer *timer = ratimer;

    vkDestroyQueryPool(vk->dev, timer->pool, MPVK_ALLOCATOR);

    talloc_free(timer);
}

MAKE_LAZY_DESTRUCTOR(vk_timer_destroy, ra_timer);

static ra_timer *vk_timer_create(struct ra *ra)
{
    struct mpvk_ctx *vk = ra_vk_get(ra);

    struct vk_timer *timer = talloc_zero(NULL, struct vk_timer);
    timer->index_seen = -1;

    struct VkQueryPoolCreateInfo qinfo = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = VK_QUERY_POOL_SIZE,
    };

    VK(vkCreateQueryPool(vk->dev, &qinfo, MPVK_ALLOCATOR, &timer->pool));

    return (ra_timer *)timer;

error:
    vk_timer_destroy(ra, timer);
    return NULL;
}

static void vk_timer_record(struct ra *ra, VkQueryPool pool, int index,
                            VkPipelineStageFlags stage)
{
    struct vk_cmd *cmd = vk_require_cmd(ra, GRAPHICS);
    if (!cmd)
        return;

    vkCmdWriteTimestamp(cmd->buf, stage, pool, index);
}

static void vk_timer_start(struct ra *ra, ra_timer *ratimer)
{
    struct mpvk_ctx *vk = ra_vk_get(ra);
    struct vk_timer *timer = ratimer;

    VkResult res = VK_NOT_READY;
    uint64_t out[2];

    if (timer->index <= timer->index_seen) {
        res = vkGetQueryPoolResults(vk->dev, timer->pool, timer->index, 2,
                                    sizeof(out), &out[0], sizeof(uint64_t),
                                    VK_QUERY_RESULT_64_BIT);
    }

    switch (res) {
    case VK_SUCCESS:
        timer->result = (out[1] - out[0]) * vk->limits.timestampPeriod;
        break;
    case VK_NOT_READY:
        timer->result = 0;
        break;
    default:
        MP_WARN(vk, "Failed reading timer query result: %s\n", vk_err(res));
        return;
    };

    vk_timer_record(ra, timer->pool, timer->index,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

static uint64_t vk_timer_stop(struct ra *ra, ra_timer *ratimer)
{
    struct vk_timer *timer = ratimer;
    vk_timer_record(ra, timer->pool, timer->index + 1,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    timer->index_seen = MPMAX(timer->index_seen, timer->index);
    timer->index = (timer->index + 2) % VK_QUERY_POOL_SIZE;

    return timer->result;
}

static struct ra_fns ra_fns_vk = {
    .destroy                = vk_destroy_ra,
    .tex_create             = vk_tex_create,
    .tex_destroy            = vk_tex_destroy_lazy,
    .tex_upload             = vk_tex_upload,
    .buf_create             = vk_buf_create,
    .buf_destroy            = vk_buf_destroy_lazy,
    .buf_update             = vk_buf_update,
    .buf_poll               = vk_buf_poll,
    .clear                  = vk_clear,
    .blit                   = vk_blit,
    .uniform_layout         = std140_layout,
    .push_constant_layout   = std430_layout,
    .desc_namespace         = vk_desc_namespace,
    .renderpass_create      = vk_renderpass_create,
    .renderpass_destroy     = vk_renderpass_destroy_lazy,
    .renderpass_run         = vk_renderpass_run,
    .timer_create           = vk_timer_create,
    .timer_destroy          = vk_timer_destroy_lazy,
    .timer_start            = vk_timer_start,
    .timer_stop             = vk_timer_stop,
};

struct vk_cmd *ra_vk_submit(struct ra *ra, struct ra_tex *tex)
{
    struct ra_vk *p = ra->priv;
    struct vk_cmd *cmd = vk_require_cmd(ra, GRAPHICS);
    if (!cmd)
        return NULL;

    struct ra_tex_vk *tex_vk = tex->priv;
    assert(tex_vk->external_img);
    tex_barrier(ra, cmd, tex, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                false);

    // Return this directly instead of going through vk_submit
    p->cmd = NULL;
    return cmd;
}
