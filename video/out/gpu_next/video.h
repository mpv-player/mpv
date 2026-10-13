/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 */

#ifndef MP_GPU_NEXT_VIDEO_H
#define MP_GPU_NEXT_VIDEO_H

#include <libplacebo/renderer.h>
#include <libplacebo/swapchain.h>

#include "misc/bstr.h"
#include "sub/osd.h"
#include "video/mp_image.h"

struct mp_log;
struct mpv_global;
struct mp_hwdec_devices;
struct hwdec_imgfmt_request;
struct ra_ctx;
struct vo;
struct vo_frame;
struct voctrl_screenshot;
struct voctrl_performance_data;
struct pl_video;

// Native display information and the result of optional colorspace negotiation.
struct pl_video_target {
    struct pl_color_space color;
    struct pl_color_space hint;
    float reference_luma;
    int depth;
    bool hint_enabled;
    bool color_overridden;
    bool color_known; // The host supplied a known target transfer function.
};

// The caller owns the context, log and GPU, which must outlive the renderer.
struct pl_video *pl_video_init(struct ra_ctx *ra_ctx, pl_log pllog, pl_gpu gpu,
                               struct mpv_global *global, struct mp_log *log);
void pl_video_uninit(struct pl_video *p);
bool pl_video_wants_alpha(struct mpv_global *global);

// The renderer owns the returned device list. The caller installs its loader.
struct mp_hwdec_devices *pl_video_get_hwdec_devices(struct pl_video *p);
void pl_video_load_hwdec_api(struct pl_video *p, struct hwdec_imgfmt_request *params);
bool pl_video_check_format(struct pl_video *p, int format);
void pl_video_config(struct pl_video *p, const struct mp_image_params *params);
void pl_video_resize(struct pl_video *p, struct mp_rect src, struct mp_rect dst,
                     struct mp_osd_res osd);
void pl_video_set_osd_source(struct pl_video *p, struct osd_state *osd);
void pl_video_update_options(struct pl_video *p);
void pl_video_configure_queue(struct pl_video *p, struct vo *vo);
void pl_video_reset(struct pl_video *p);
bool pl_video_set_paused(struct pl_video *p, bool paused);

// Compute a native display hint before acquiring its swapchain image.
void pl_video_get_target_hint(struct pl_video *p, struct vo_frame *frame,
                              struct pl_video_target *target);
// Render into a caller-owned target without acquiring, submitting or presenting it.
// A NULL display uses the framebuffer's colorspace without native negotiation.
// Returns whether rendering succeeded; an acquired target is flushed on failure too.
bool pl_video_render_frame(struct pl_video *p, struct vo_frame *frame,
                            const struct pl_swapchain_frame *target, bool flip,
                            const struct pl_video_target *display);
enum pl_color_levels pl_video_get_output_levels(struct pl_video *p);
const struct mp_image_params *pl_video_get_target_params(struct pl_video *p);
bool pl_video_get_hdr_metadata(struct pl_video *p, struct mp_image_params *params);
void pl_video_screenshot(struct pl_video *p, struct voctrl_screenshot *args);
void pl_video_get_perfdata(struct pl_video *p, struct voctrl_performance_data *perf);
struct mp_image *pl_video_get_image(struct pl_video *p, int imgfmt, int w, int h,
                                   int stride_align, int flags);
// Takes ownership of the profile data, including when unloading an empty profile.
bool pl_video_set_icc_profile(struct pl_video *p, struct bstr icc);
bool pl_video_icc_auto_enabled(struct pl_video *p);

#endif
