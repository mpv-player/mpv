#pragma once

#include "video/out/gpu/context.h"
#include "common.h"

extern const int mpgl_min_required_gl_versions[];

enum gles_mode {
    GLES_AUTO = 0,
    GLES_YES,
    GLES_NO,
};

// Returns the gles mode based on the --opengl opts.
enum gles_mode ra_gl_ctx_get_glesmode(struct ra_ctx *ctx);

void ra_gl_ctx_uninit(struct ra_ctx *ctx);
bool ra_gl_ctx_init(struct ra_ctx *ctx, GL *gl, struct ra_ctx_params params);

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
