#pragma once

#include "common/global.h"
#include "video/out/gpu/context.h"
#include "common.h"

extern const int mpgl_preferred_gl_versions[];

// Returns whether or not a candidate GL version should be accepted or not
// (based on the --opengl opts). Implementations may call this before
// ra_gl_ctx_init if they wish to probe for multiple possible GL versions.
bool ra_gl_ctx_test_version(struct ra_ctx *ctx, int version, bool es);

// These are a set of helpers for ra_ctx providers based on ra_gl.
// The init function also initializes ctx->ra and ctx->swapchain, so the user
// doesn't have to do this manually. (Similarly, the uninit function will
// clean them up)

struct ra_gl_ctx_params {
    // Set to the platform-specific function to swap buffers, like
    // glXSwapBuffers, eglSwapBuffers etc. This will be called by
    // ra_gl_ctx_swap_buffers. Required unless you either never call that
    // function or if you override it yourself.
    void (*swap_buffers)(struct ra_ctx *ctx);

    // See ra_swapchain_fns.get_latency.
    double (*get_latency)(struct ra_ctx *ctx);

    // Set to false if the implementation follows normal GL semantics, which is
    // upside down. Set to true if it does *not*, i.e. if rendering is right
    // side up
    bool flipped;

    // If this is set to non-NULL, then the ra_gl_ctx will consider the GL
    // implementation to be using an external swapchain, which disables the
    // software simulation of --swapchain-depth. Any functions defined by this
    // ra_swapchain_fns structs will entirely replace the equivalent ra_gl_ctx
    // functions in the resulting ra_swapchain.
    const struct ra_swapchain_fns *external_swapchain;
};

void ra_gl_ctx_uninit(struct ra_ctx *ctx);
bool ra_gl_ctx_init(struct ra_ctx *ctx, GL *gl, struct ra_gl_ctx_params params);

// Call this any time the window size or main framebuffer changes
void ra_gl_ctx_resize(struct ra_swapchain *sw, int w, int h, int fbo);

// These functions are normally set in the ra_swapchain->fns, but if an
// implementation has a need to override this fns struct with custom functions
// for whatever reason, these can be used to inherit the original behavior.
int ra_gl_ctx_color_depth(struct ra_swapchain *sw);
struct mp_image *ra_gl_ctx_screenshot(struct ra_swapchain *sw);
bool ra_gl_ctx_start_frame(struct ra_swapchain *sw, struct ra_fbo *out_fbo);
bool ra_gl_ctx_submit_frame(struct ra_swapchain *sw, const struct vo_frame *frame);
void ra_gl_ctx_swap_buffers(struct ra_swapchain *sw);
