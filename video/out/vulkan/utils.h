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
    int async_transfer; // enable async transfer
    int async_compute;  // enable async compute
};

// Create a logical device and initialize the vk_cmdpools
bool mpvk_device_init(struct mpvk_ctx *vk, struct mpvk_device_opts opts);

// Wait for all currently pending commands to have completed. This is the only
// function that actually processes the callbacks. Will wait at most `timeout`
// nanoseconds for the completion of each command. Using it with a value of
// UINT64_MAX effectively means waiting until the pool/device is idle. The
// timeout may also be passed as 0, in which case this function will not block,
// but only poll for completed commands.
void mpvk_poll_commands(struct mpvk_ctx *vk, uint64_t timeout);

// Flush all currently queued commands. Call this once per frame, after
// submitting all of the command buffers for that frame. Calling this more
// often than that is possible but bad for performance.
// Returns whether successful. Failed commands will be implicitly dropped.
bool mpvk_flush_commands(struct mpvk_ctx *vk);

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

// Helper wrapper around command buffers that also track dependencies,
// callbacks and synchronization primitives
struct vk_cmd {
    struct vk_cmdpool *pool; // pool it was allocated from
    VkQueue queue;           // the submission queue (for recording/pending)
    VkCommandBuffer buf;     // the command buffer itself
    VkFence fence;           // the fence guards cmd buffer reuse
    // The semaphores represent dependencies that need to complete before
    // this command can be executed. These are *not* owned by the vk_cmd
    VkSemaphore *deps;
    VkPipelineStageFlags *depstages;
    int num_deps;
    // The signals represent semaphores that fire once the command finishes
    // executing. These are also not owned by the vk_cmd
    VkSemaphore *sigs;
    int num_sigs;
    // Since VkFences are useless, we have to manually track "callbacks"
    // to fire once the VkFence completes. These are used for multiple purposes,
    // ranging from garbage collection (resource deallocation) to fencing.
    struct vk_callback *callbacks;
    int num_callbacks;
};

// Associate a callback with the completion of the current command. This
// bool will be set to `true` once the command completes, or shortly thereafter.
void vk_cmd_callback(struct vk_cmd *cmd, vk_cb callback, void *p, void *arg);

// Associate a raw dependency for the current command. This semaphore must
// signal by the corresponding stage before the command may execute.
void vk_cmd_dep(struct vk_cmd *cmd, VkSemaphore dep, VkPipelineStageFlags stage);

// Associate a raw signal with the current command. This semaphore will signal
// after the command completes.
void vk_cmd_sig(struct vk_cmd *cmd, VkSemaphore sig);

// Signal abstraction: represents an abstract synchronization mechanism.
// Internally, this may either resolve as a semaphore or an event depending
// on whether the appropriate conditions are met.
struct vk_signal {
    VkSemaphore semaphore;
    VkEvent event;
    VkQueue event_source;
};

// Generates a signal after the execution of all previous commands matching the
// given the pipeline stage. The signal is owned by the caller, and must be
// consumed eith vk_cmd_wait or released with vk_signal_cancel in order to
// free the resources.
struct vk_signal *vk_cmd_signal(struct mpvk_ctx *vk, struct vk_cmd *cmd,
                                VkPipelineStageFlags stage);

// Consumes a previously generated signal. This signal must fire by the
// indicated stage before the command can run. If *event is not NULL, then it
// MAY be set to a VkEvent which the caller MUST manually wait on in the most
// appropriate way. This function takes over ownership of the signal (and the
// signal will be released/reused automatically)
void vk_cmd_wait(struct mpvk_ctx *vk, struct vk_cmd *cmd,
                 struct vk_signal **sigptr, VkPipelineStageFlags stage,
                 VkEvent *out_event);

// Destroys a currently pending signal, for example if the resource is no
// longer relevant.
void vk_signal_destroy(struct mpvk_ctx *vk, struct vk_signal **sig);

// Command pool / queue family hybrid abstraction
struct vk_cmdpool {
    VkQueueFamilyProperties props;
    int qf; // queue family index
    VkCommandPool pool;
    VkQueue *queues;
    int num_queues;
    int idx_queues;
    // Command buffers associated with this queue. These are available for
    // re-recording
    struct vk_cmd **cmds;
    int num_cmds;
};

// Fetch a command buffer from a command pool and begin recording to it.
// Returns NULL on failure.
struct vk_cmd *vk_cmd_begin(struct mpvk_ctx *vk, struct vk_cmdpool *pool);

// Finish recording a command buffer and queue it for execution. This function
// takes over ownership of *cmd, i.e. the caller should not touch it again.
void vk_cmd_queue(struct mpvk_ctx *vk, struct vk_cmd *cmd);

// Predefined structs for a simple non-layered, non-mipped image
extern const VkImageSubresourceRange vk_range;
extern const VkImageSubresourceLayers vk_layers;
