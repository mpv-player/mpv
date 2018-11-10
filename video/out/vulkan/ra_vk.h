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

struct vk_external_mem {
#if HAVE_WIN32_DESKTOP
    HANDLE mem_handle;
#else
    int mem_fd;
#endif
    size_t mem_size;
    size_t size;
    size_t offset;
};

// Export an ra_buf for importing by another api.
bool ra_vk_buf_get_external_info(struct ra *ra, struct ra_buf *buf, struct vk_external_mem *ret);

// Export an ra_tex for importing by another api.
bool ra_vk_tex_get_external_info(struct ra *ra, struct ra_tex *tex, struct vk_external_mem *ret);

// Set the buffer user data
void ra_vk_buf_set_user_data(struct ra_buf *buf, void *priv);

// Get the buffer user data
void *ra_vk_buf_get_user_data(struct ra_buf *buf);

struct vk_external_semaphore {
    VkSemaphore s;
#if HAVE_WIN32_DESKTOP
    HANDLE handle;
#else
    int fd;
#endif
};

// Create and export a semaphore for external use. Both the semaphore and
// exported object are returned. The caller is responsible for destroying
// the semaphore and for closing the fd or handle if it is not imported by
// another API.
//
// Returns whether successful.
bool ra_vk_create_external_semaphore(struct ra *ra,
                                     struct vk_external_semaphore *ret);

// "Hold" a shared image. This will transition the image into the layout and
// access mode specified by the user, and fire the given semaphore (required!)
// when this is done. This marks the image as held. Attempting to perform any
// ra_tex_* operation (except ra_tex_destroy) on a held image is an error.
//
// Returns whether successful.
bool ra_vk_hold(struct ra *ra, struct ra_tex *tex,
                VkImageLayout layout, VkAccessFlags access,
                VkSemaphore sem_out);

// "Release" a shared image, meaning it is no longer held. `layout` and
// `access` describe the current state of the image at the point in time when
// the user is releasing it. Performing any operation on the VkImage underlying
// this `ra_tex` while it is not being held by the user is undefined behavior.
//
// If `sem_in` is specified, it must fire before ra_vk will actually use
// or modify the image. (Optional)
void ra_vk_release(struct ra *ra, struct ra_tex *tex,
                   VkImageLayout layout, VkAccessFlags access,
                   VkSemaphore sem_in);
