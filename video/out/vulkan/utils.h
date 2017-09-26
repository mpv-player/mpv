#pragma once

#include "video/out/vo.h"
#include "video/out/gpu/context.h"
#include "video/mp_image.h"

#include "common.h"
#include "formats.h"

#define VK_LOAD_PFN(name) PFN_##name pfn_##name = (PFN_##name) \
                            vkGetInstanceProcAddr(vk->inst, #name);

// Return a human-readable name for various struct mpvk_ctx enums
const char* vk_err(VkResult res);

// Convenience macros to simplify a lot of common boilerplate
#define VK_ASSERT(res, str)                               \
    do {                                                  \
        if (res != VK_SUCCESS) {                          \
            MP_ERR(vk, str ": %s\n", vk_err(res));        \
            goto error;                                   \
        }                                                 \
    } while (0)

#define VK(cmd)                                           \
    do {                                                  \
        MP_TRACE(vk, #cmd "\n");                          \
        VkResult res ## __LINE__ = (cmd);                 \
        VK_ASSERT(res ## __LINE__, #cmd);                 \
    } while (0)

// Uninits everything in the correct order
void mpvk_uninit(struct mpvk_ctx *vk);

// Initialization functions: As a rule of thumb, these need to be called in
// this order, followed by vk_malloc_init, followed by RA initialization, and
// finally followed by vk_swchain initialization.

// Create a vulkan instance. Returns VK_NULL_HANDLE on failure
bool mpvk_instance_init(struct mpvk_ctx *vk, struct mp_log *log,
                        const char *surf_ext_name, bool debug);

// Generate a VkSurfaceKHR usable for video output. Returns VK_NULL_HANDLE on
// failure. Must be called after mpvk_instance_init.
bool mpvk_surface_init(struct vo *vo, struct mpvk_ctx *vk);

// Find a suitable physical device for use with rendering and which supports
// the surface.
// name: only match a device with this name
// sw: also allow software/virtual devices
bool mpvk_find_phys_device(struct mpvk_ctx *vk, const char *name, bool sw);

// Pick a suitable surface format that's supported by this physical device.
bool mpvk_pick_surface_format(struct mpvk_ctx *vk);

struct mpvk_device_opts {
    int queue_count;    // number of queues to use
};

// Create a logical device and initialize the vk_cmdpools
bool mpvk_device_init(struct mpvk_ctx *vk, struct mpvk_device_opts opts);

// Wait until all commands submitted to all queues have completed
void mpvk_pool_wait_idle(struct mpvk_ctx *vk, struct vk_cmdpool *pool);
void mpvk_dev_wait_idle(struct mpvk_ctx *vk);

// Wait until at least one command submitted to any queue has completed, and
// process the callbacks. Good for event loops that need to delay until a
// command completes. Will block at most `timeout` nanoseconds. If used with
// 0, it only garbage collects completed commands without blocking.
void mpvk_pool_poll_cmds(struct mpvk_ctx *vk, struct vk_cmdpool *pool,
                         uint64_t timeout);
void mpvk_dev_poll_cmds(struct mpvk_ctx *vk, uint32_t timeout);

// Since lots of vulkan operations need to be done lazily once the affected
// resources are no longer in use, provide an abstraction for tracking these.
// In practice, these are only checked and run when submitting new commands, so
// the actual execution may be delayed by a frame.
typedef void (*vk_cb)(void *priv, void *arg);

struct vk_callback {
    vk_cb run;
    void *priv;
    void *arg; // as a convenience, you also get to pass an arg for "free"
};

// Associate a callback with the completion of all currently pending commands.
// This will essentially run once the device is completely idle.
void vk_dev_callback(struct mpvk_ctx *vk, vk_cb callback, void *p, void *arg);

#define MPVK_MAX_CMD_DEPS 8

// Helper wrapper around command buffers that also track dependencies,
// callbacks and synchronization primitives
struct vk_cmd {
    struct vk_cmdpool *pool; // pool it was allocated from
    VkCommandBuffer buf;
    VkFence fence; // the fence guards cmd buffer reuse
    VkSemaphore done; // the semaphore signals when execution is done
    // The semaphores represent dependencies that need to complete before
    // this command can be executed. These are *not* owned by the vk_cmd
    VkSemaphore deps[MPVK_MAX_CMD_DEPS];
    VkPipelineStageFlags depstages[MPVK_MAX_CMD_DEPS];
    int num_deps;
    // Since VkFences are useless, we have to manually track "callbacks"
    // to fire once the VkFence completes. These are used for multiple purposes,
    // ranging from garbage collection (resource deallocation) to fencing.
    struct vk_callback *callbacks;
    int num_callbacks;
};

// Associate a callback with the completion of the current command. This
// bool will be set to `true` once the command completes, or shortly thereafter.
void vk_cmd_callback(struct vk_cmd *cmd, vk_cb callback, void *p, void *arg);

// Associate a dependency for the current command. This semaphore must signal
// by the corresponding stage before the command may execute.
void vk_cmd_dep(struct vk_cmd *cmd, VkSemaphore dep,
                VkPipelineStageFlags depstage);

#define MPVK_MAX_QUEUES 8
#define MPVK_MAX_CMDS 64

// Command pool / queue family hybrid abstraction
struct vk_cmdpool {
    VkQueueFamilyProperties props;
    uint32_t qf; // queue family index
    VkCommandPool pool;
    VkQueue queues[MPVK_MAX_QUEUES];
    int qcount;
    int qindex;
    // Command buffers associated with this queue
    struct vk_cmd cmds[MPVK_MAX_CMDS];
    int cindex;
    int cindex_pending;
};

// Fetch the next command buffer from a command pool and begin recording to it.
// Returns NULL on failure.
struct vk_cmd *vk_cmd_begin(struct mpvk_ctx *vk, struct vk_cmdpool *pool);

// Finish the currently recording command buffer and submit it for execution.
// If `done` is not NULL, it will be set to a semaphore that will signal once
// the command completes. (And MUST have a corresponding semaphore wait)
// Returns whether successful.
bool vk_cmd_submit(struct mpvk_ctx *vk, struct vk_cmd *cmd, VkSemaphore *done);

// Rotate the queues for each vk_cmdpool. Call this once per frame to ensure
// good parallelism between frames when using multiple queues
void vk_cmd_cycle_queues(struct mpvk_ctx *vk);

// Predefined structs for a simple non-layered, non-mipped image
extern const VkImageSubresourceRange vk_range;
extern const VkImageSubresourceLayers vk_layers;
