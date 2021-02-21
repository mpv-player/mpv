#pragma once

#include "video/out/vo.h"
#include "video/csputils.h"

#include "config.h"
#include "ra.h"

struct ra_ctx_opts {
    int allow_sw;        // allow software renderers
    int want_alpha;      // create an alpha framebuffer if possible
    int debug;           // enable debugging layers/callbacks etc.
    bool probing;        // the backend was auto-probed
};

struct ra_ctx {
    struct vo *vo;
    struct ra *ra;
    struct mpv_global *global;
    struct mp_log *log;

    struct ra_ctx_opts opts;
    const struct ra_ctx_fns *fns;
    struct ra_swapchain *swapchain;
    struct spirv_compiler *spirv;

    void *priv;
};

// The functions that make up a ra_ctx.
struct ra_ctx_fns {
    const char *type; // API type (for --gpu-api)
    const char *name; // name (for --gpu-context)

    // Resize the window, or create a new window if there isn't one yet.
    // Currently, there is an unfortunate interaction with ctx->vo, and
    // display size etc. are determined by it.
    bool (*reconfig)(struct ra_ctx *ctx);

    // This behaves exactly like vo_driver.control().
    int (*control)(struct ra_ctx *ctx, int *events, int request, void *arg);

    // These behave exactly like vo_driver.wakeup/wait_events. They are
    // optional.
    void (*wakeup)(struct ra_ctx *ctx);
    void (*wait_events)(struct ra_ctx *ctx, int64_t until_time_us);
    void (*update_render_opts)(struct ra_ctx *ctx);

    // Initialize/destroy the 'struct ra' and possibly the underlying VO backend.
    // Not normally called by the user of the ra_ctx.
    bool (*init)(struct ra_ctx *ctx);
    void (*uninit)(struct ra_ctx *ctx);
};

// Extra struct for the swapchain-related functions so they can be easily
// inherited from helpers.
struct ra_swapchain {
    struct ra_ctx *ctx;
    struct priv *priv;
    const struct ra_swapchain_fns *fns;
};

// Represents a framebuffer / render target
struct ra_fbo {
    struct ra_tex *tex;
    bool flip; // rendering needs to be inverted

    // Host system's colorspace that it will be interpreting
    // the frame buffer as.
    struct mp_colorspace color_space;
};

struct ra_swapchain_fns {
    // Gets the current framebuffer depth in bits (0 if unknown). Optional.
    int (*color_depth)(struct ra_swapchain *sw);

    // Called when rendering starts. Returns NULL on failure. This must be
    // followed by submit_frame, to submit the rendered frame. This function
    // can also fail sporadically, and such errors should be ignored unless
    // they persist.
    bool (*start_frame)(struct ra_swapchain *sw, struct ra_fbo *out_fbo);

    // Present the frame. Issued in lockstep with start_frame, with rendering
    // commands in between. The `frame` is just there for timing data, for
    // swapchains smart enough to do something with it.
    bool (*submit_frame)(struct ra_swapchain *sw, const struct vo_frame *frame);

    // Performs a buffer swap. This blocks for as long as necessary to meet
    // params.swapchain_depth, or until the next vblank (for vsynced contexts)
    void (*swap_buffers)(struct ra_swapchain *sw);

    // See vo. Usually called after swap_buffers().
    void (*get_vsync)(struct ra_swapchain *sw, struct vo_vsync_info *info);
};

// Create and destroy a ra_ctx. This also takes care of creating and destroying
// the underlying `struct ra`, and perhaps the underlying VO backend.
struct ra_ctx *ra_ctx_create(struct vo *vo, const char *context_type,
                             const char *context_name, struct ra_ctx_opts opts);
void ra_ctx_destroy(struct ra_ctx **ctx);

struct m_option;
int ra_ctx_api_help(struct mp_log *log, const struct m_option *opt,
                    struct bstr name);
int ra_ctx_validate_api(struct mp_log *log, const struct m_option *opt,
                        struct bstr name, const char **value);
int ra_ctx_context_help(struct mp_log *log, const struct m_option *opt,
                        struct bstr name);
int ra_ctx_validate_context(struct mp_log *log, const struct m_option *opt,
                            struct bstr name, const char **value);
