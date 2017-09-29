#pragma once

#include "video/out/gpu/ra.h"

#include "common.h"
#include "utils.h"

struct ra *ra_create_vk(struct mpvk_ctx *vk, struct mp_log *log);

// Access to the VkDevice is needed for swapchain creation
VkDevice ra_vk_get_dev(struct ra *ra);

// Allocates a ra_tex that wraps a swapchain image. The contents of the image
// will be invalidated, and access to it will only be internally synchronized.
// So the calling could should not do anything else with the VkImage.
struct ra_tex *ra_vk_wrap_swapchain_img(struct ra *ra, VkImage vkimg,
                                        VkSwapchainCreateInfoKHR info);

// Associates an external semaphore (dependency) with a ra_tex, such that this
// ra_tex will not be used by the ra_vk until the external semaphore fires.
void ra_tex_vk_external_dep(struct ra *ra, struct ra_tex *tex, VkSemaphore dep);

// This function finalizes rendering, transitions `tex` (which must be a
// wrapped swapchain image) into a format suitable for presentation, and returns
// the resulting command buffer (or NULL on error). The caller may add their
// own semaphores to this command buffer, and must submit it afterwards.
struct vk_cmd *ra_vk_submit(struct ra *ra, struct ra_tex *tex);

// May be called on a struct ra of any type. Returns NULL if the ra is not
// a vulkan ra.
struct mpvk_ctx *ra_vk_get(struct ra *ra);
