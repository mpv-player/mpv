/*
 * Copyright (C) 2021 Niklas Haas
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <libplacebo/renderer.h>

#include "common/common.h"
#include "misc/bstr.h"
#include "sub/osd.h"

struct gpu_ctx;
struct hwdec_imgfmt_request;
struct mp_hwdec_devices;
struct mp_image;
struct mp_image_params;
struct mp_log;
struct mpv_global;
struct vo;
struct vo_frame;
struct voctrl_performance_data;
struct voctrl_screenshot;

enum mppl_video_hint {
    MPPL_VIDEO_HINT_NONE,  // hinting is disabled
    MPPL_VIDEO_HINT_KEEP,  // there is no frame to derive a hint from
    MPPL_VIDEO_HINT_SET,   // the hint is to be set
};

struct mppl_video;

struct mppl_video *mppl_video_init(struct mp_log *log, struct mpv_global *global,
                                   struct gpu_ctx *ctx);
void mppl_video_uninit(struct mppl_video *p);
void mppl_video_init_hwdecs(struct mppl_video *p, struct mp_hwdec_devices *devs,
                            bool load_all_by_default);
void mppl_video_load_hwdecs(struct mppl_video *p, struct mp_hwdec_devices *devs,
                            struct hwdec_imgfmt_request *params);
void mppl_video_set_osd_source(struct mppl_video *p, struct osd_state *osd);
bool mppl_video_check_format(struct mppl_video *p, int format);
struct mp_image *mppl_video_get_image(struct mppl_video *p, int imgfmt, int w, int h,
                                      int stride_align, int flags);
void mppl_video_config(struct mppl_video *p, struct mp_image_params *params);
void mppl_video_resize(struct mppl_video *p, struct mp_rect src, struct mp_rect dst,
                       struct mp_osd_res osd);
void mppl_video_configure_queue(struct mppl_video *p, struct vo *vo);
void mppl_video_reload_options(struct mppl_video *p);
bool mppl_video_want_alpha(struct mppl_video *p);
bool mppl_video_opts_want_alpha(struct mpv_global *global);
bool mppl_video_icc_auto_enabled(struct mppl_video *p);
bool mppl_video_set_icc_profile(struct mppl_video *p, struct bstr icc);
void mppl_video_begin_frame(struct mppl_video *p, struct vo_frame *frame,
                            struct pl_color_space display_csp, float display_ref_luma);
enum mppl_video_hint mppl_video_get_target_hint(struct mppl_video *p, struct vo_frame *frame,
                                                struct pl_color_space *out_hint);
void mppl_video_skip_frame(struct mppl_video *p, struct vo_frame *frame);
bool mppl_video_render_frame(struct mppl_video *p, struct vo_frame *frame,
                             const struct pl_swapchain_frame *swframe, int fb_depth,
                             const struct pl_color_space *hint,
                             struct mp_image_params *target_params);
bool mppl_video_get_hdr_metadata(struct mppl_video *p, struct pl_hdr_metadata *out);
enum pl_color_levels mppl_video_get_output_levels(struct mppl_video *p);
void mppl_video_set_paused(struct mppl_video *p, bool paused);
bool mppl_video_showing_interpolated_frame(struct mppl_video *p);
void mppl_video_reset(struct mppl_video *p);
void mppl_video_perfdata(struct mppl_video *p, struct voctrl_performance_data *perf);
void mppl_video_screenshot(struct mppl_video *p, struct voctrl_screenshot *args);
