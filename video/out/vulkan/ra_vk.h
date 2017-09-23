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

// This function flushes the command buffers, transitions `tex` (which must be
// a wrapped swapchain image) into a format suitable for presentation, and
// submits the current rendering commands. The indicated semaphore must fire
// before the submitted command can run. If `done` is non-NULL, it will be
// fired once the command completes. If `inflight` is non-NULL, it will be
// incremented when the command starts and decremented when it completes.
bool ra_vk_submit(struct ra *ra, struct ra_tex *tex, VkSemaphore done,
                  int *inflight);

// May be called on a struct ra of any type. Returns NULL if the ra is not
// a vulkan ra.
struct mpvk_ctx *ra_vk_get(struct ra *ra);

// Associate an external dependency (semaphore) with a ra_tex. This ra_tex
// will not be used again until the external dependency has fired.
void ra_vk_tex_dep(struct ra_tex *tex, VkSemaphore dep);
