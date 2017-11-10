#pragma once

#include "common.h"

void vk_malloc_init(struct mpvk_ctx *vk);
void vk_malloc_uninit(struct mpvk_ctx *vk);

// Represents a single "slice" of generic (non-buffer) memory, plus some
// metadata for accounting. This struct is essentially read-only.
struct vk_memslice {
    VkDeviceMemory vkmem;
    size_t offset;
    size_t size;
    void *priv;
};

void vk_free_memslice(struct mpvk_ctx *vk, struct vk_memslice slice);
bool vk_malloc_generic(struct mpvk_ctx *vk, VkMemoryRequirements reqs,
                       VkMemoryPropertyFlags flags, struct vk_memslice *out);

// Represents a single "slice" of a larger buffer
struct vk_bufslice {
    struct vk_memslice mem; // must be freed by the user when done
    VkBuffer buf;           // the buffer this memory was sliced from
    // For persistently mapped buffers, this points to the first usable byte of
    // this slice.
    void *data;
};

// Allocate a buffer slice. This is more efficient than vk_malloc_generic for
// when the user needs lots of buffers, since it doesn't require
// creating/destroying lots of (little) VkBuffers.
bool vk_malloc_buffer(struct mpvk_ctx *vk, VkBufferUsageFlags bufFlags,
                      VkMemoryPropertyFlags memFlags, VkDeviceSize size,
                      VkDeviceSize alignment, struct vk_bufslice *out);
