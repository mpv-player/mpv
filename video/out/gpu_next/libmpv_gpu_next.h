#pragma once

#include <libplacebo/gpu.h>  // for pl_gpu, pl_tex
#include "mpv/render.h"      // for mpv_render_param

/**
 * This struct represents an instance of a specific API context implementation.
 * It's created by the Host (`libmpv_gpu_next.c`) and passed to the concrete
 * implementation (e.g., `libmpv_gpu_next_gl.c`) so it can access global state
 * and return its results.
 */
struct libmpv_gpu_next_context {
    // Inputs from the Host
    struct mpv_global *global;
    struct mp_log *log;
    void *priv; // Private state for the implementation's use

    const struct libmpv_gpu_next_context_fns *fns;

    // The abstract "toolbox" that the engine will use.
    struct ra_next *ra;
    // The underlying GPU object, needed by the Host for resource management.
    pl_gpu gpu;
};

/**
 * This struct defines the "vtable" or "interface" for a specific API context
 * implementation. Our Host (`libmpv_gpu_next.c`) will find an implementation
 * that matches the user's request and will then call these functions through
 * this table.
 */
struct libmpv_gpu_next_context_fns {
    // The name of the API, e.g., "opengl". This must match the string provided
    // by the user in MPV_RENDER_PARAM_API_TYPE.
    const char *api_name;

    /**
     * Initializes the graphics context based on user parameters.
     * On success, it must populate the `ra` and `gpu` fields of the
     * `libmpv_gpu_next_context` struct.
     * @param ctx The context instance to initialize.
     * @param params The list of parameters from the user.
     * @return 0 on success, or a negative mpv_error code.
     */
    int (*init)(struct libmpv_gpu_next_context *ctx, mpv_render_param *params);

    /**
     * Wraps the user's render target (provided in `params`) into a `pl_tex`
     * that our engine can understand.
     * @param ctx The context instance.
     * @param params The list of parameters from the user, containing the target.
     * @param out_tex On success, this will point to a newly created, temporary
     *                `pl_tex` that wraps the user's target. The caller is
     *                responsible for freeing this texture.
     * @return 0 on success, or a negative mpv_error code.
     */
    int (*wrap_fbo)(struct libmpv_gpu_next_context *ctx,
                    mpv_render_param *params, pl_tex *out_tex);

    /**
     * Called by the Host after a frame has been rendered to the user's target.
     * This can be used for presentation timing or swapchain management.
     * This function can be NULL if no action is needed.
     * @param ctx The context instance.
     */
    void (*done_frame)(struct libmpv_gpu_next_context *ctx);

    /**
     * Destroys the graphics context and all associated resources.
     * @param ctx The context instance to destroy.
     */
    void (*destroy)(struct libmpv_gpu_next_context *ctx);
};

/**
 * A forward declaration for the concrete OpenGL API context implementation,
 * which is defined in `libmpv_gpu_next_gl.c`. The Host needs this to add it
 * to its list of available backends.
 */
extern const struct libmpv_gpu_next_context_fns libmpv_gpu_next_context_gl;
