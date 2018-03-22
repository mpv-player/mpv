#pragma once

#include "video/out/libmpv.h"

struct ra_tex;

struct libmpv_gpu_context {
    struct mpv_global *global;
    struct mp_log *log;
    const struct libmpv_gpu_context_fns *fns;

    struct ra *ra;
    void *priv;
};

// Manage backend specific interaction between libmpv and ra backend, that can't
// be managed by ra itself (initialization and passing FBOs).
struct libmpv_gpu_context_fns {
    // The libmpv API type name, see MPV_RENDER_PARAM_API_TYPE.
    const char *api_name;
    // Pretty much works like render_backend_fns.init, except that the
    // API type is already checked by the caller.
    // Successful init must set ctx->ra.
    int (*init)(struct libmpv_gpu_context *ctx, mpv_render_param *params);
    // Wrap the surface passed to mpv_render_context_render() (via the params
    // array) into a ra_tex and return it. Returns a libmpv error code, and sets
    // *out to a temporary object on success. The returned object is valid until
    // another wrap_fbo() or done_frame() is called.
    // This does not need to care about generic attributes, like flipping.
    int (*wrap_fbo)(struct libmpv_gpu_context *ctx, mpv_render_param *params,
                    struct ra_tex **out);
    // Signal that the ra_tex object obtained with wrap_fbo is no longer used.
    // For certain backends, this might also be used to signal the end of
    // rendering (like OpenGL doing weird crap).
    void (*done_frame)(struct libmpv_gpu_context *ctx, bool ds);
    // Free all data in ctx->priv.
    void (*destroy)(struct libmpv_gpu_context *ctx);
};

extern const struct libmpv_gpu_context_fns libmpv_gpu_context_gl;
