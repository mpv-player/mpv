/*
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

#ifndef MP_GL_VIDEO_H
#define MP_GL_VIDEO_H

#include <stdbool.h>

#include "options/m_option.h"
#include "sub/osd.h"
#include "utils.h"
#include "lcms.h"
#include "shader_cache.h"
#include "video/csputils.h"
#include "video/out/filter_kernels.h"

struct scaler_fun {
    int function;
    float params[2];
    float blur;
    float taper;
    const struct m_opt_choice_alternatives *functions;
};

struct scaler_config {
    struct scaler_fun kernel;
    struct scaler_fun window;
    float radius;
    float antiring;
    float clamp;
};

struct scaler {
    int index;
    struct scaler_config conf;
    double scale_factor;
    bool initialized;
    struct filter_kernel *kernel;
    struct ra_tex *lut;
    struct ra_tex *sep_fbo;
    bool insufficient;

    // kernel points here
    struct filter_kernel kernel_storage;
};

enum scaler_unit {
    SCALER_SCALE,  // luma/video
    SCALER_DSCALE, // luma-video downscaling
    SCALER_CSCALE, // chroma upscaling
    SCALER_TSCALE, // temporal scaling (interpolation)
    SCALER_COUNT
};

enum dither_algo {
    DITHER_NONE = 0,
    DITHER_FRUIT,
    DITHER_ORDERED,
    DITHER_ERROR_DIFFUSION,
};

enum background_type {
    BACKGROUND_NONE = 0,
    BACKGROUND_COLOR,
    BACKGROUND_TILES,
};

enum blend_subs_mode {
    BLEND_SUBS_NO = 0,
    BLEND_SUBS_YES,
    BLEND_SUBS_VIDEO,
};

enum tone_mapping {
    TONE_MAPPING_AUTO,
    TONE_MAPPING_CLIP,
    TONE_MAPPING_MOBIUS,
    TONE_MAPPING_REINHARD,
    TONE_MAPPING_HABLE,
    TONE_MAPPING_GAMMA,
    TONE_MAPPING_LINEAR,
    TONE_MAPPING_SPLINE,
    TONE_MAPPING_BT_2390,
    TONE_MAPPING_BT_2446A,
    TONE_MAPPING_ST2094_40,
    TONE_MAPPING_ST2094_10,
};

enum gamut_mode {
    GAMUT_AUTO,
    GAMUT_CLIP,
    GAMUT_PERCEPTUAL,
    GAMUT_RELATIVE,
    GAMUT_SATURATION,
    GAMUT_ABSOLUTE,
    GAMUT_DESATURATE,
    GAMUT_DARKEN,
    GAMUT_WARN,
    GAMUT_LINEAR,
};

struct gl_tone_map_opts {
    int curve;
    float curve_param;
    float max_boost;
    bool inverse;
    int compute_peak;
    float decay_rate;
    float scene_threshold_low;
    float scene_threshold_high;
    float peak_percentile;
    float contrast_recovery;
    float contrast_smoothness;
    int gamut_mode;
    bool visualize;
};

struct gl_video_opts {
    int dumb_mode;
    struct scaler_config scaler[4];
    float gamma;
    bool gamma_auto;
    int target_prim;
    int target_trc;
    int target_peak;
    int target_contrast;
    int target_gamut;
    struct gl_tone_map_opts tone_map;
    bool correct_downscaling;
    bool linear_downscaling;
    bool linear_upscaling;
    bool sigmoid_upscaling;
    float sigmoid_center;
    float sigmoid_slope;
    bool scaler_resizes_only;
    bool pbo;
    int dither_depth;
    int dither_algo;
    int dither_size;
    bool temporal_dither;
    int temporal_dither_period;
    char *error_diffusion;
    char *fbo_format;
    int background;
    bool use_rectangle;
    struct m_color background_color;
    struct m_color background_tile_color[2];
    int background_tile_size;
    bool interpolation;
    float interpolation_threshold;
    int blend_subs;
    char **user_shaders;
    char **user_shader_opts;
    bool deband;
    struct deband_opts *deband_opts;
    float unsharp;
    int tex_pad_x, tex_pad_y;
    struct mp_icc_opts *icc_opts;
    bool shader_cache;
    int early_flush;
    char *shader_cache_dir;
    char *hwdec_interop;
};

extern const struct m_sub_options gl_video_conf;

struct gl_video;
struct vo_frame;
struct voctrl_screenshot;

enum {
    RENDER_FRAME_SUBS = 1 << 0,
    RENDER_FRAME_OSD = 1 << 1,
    RENDER_FRAME_VF_SUBS = 1 << 2,
    RENDER_SCREEN_COLOR = 1 << 3, // 3D LUT and dithering
    RENDER_FRAME_DEF = RENDER_FRAME_SUBS | RENDER_FRAME_OSD | RENDER_SCREEN_COLOR,
};

struct gl_video *gl_video_init(struct ra *ra, struct mp_log *log,
                               struct mpv_global *g);
void gl_video_uninit(struct gl_video *p);
void gl_video_set_osd_source(struct gl_video *p, struct osd_state *osd);
bool gl_video_check_format(struct gl_video *p, int mp_format);
void gl_video_config(struct gl_video *p, struct mp_image_params *params);
void gl_video_render_frame(struct gl_video *p, struct vo_frame *frame,
                           const struct ra_fbo *fbo, int flags);
void gl_video_resize(struct gl_video *p,
                     struct mp_rect *src, struct mp_rect *dst,
                     struct mp_osd_res *osd);
void gl_video_set_fb_depth(struct gl_video *p, int fb_depth);
void gl_video_perfdata(struct gl_video *p, struct voctrl_performance_data *out);
void gl_video_set_clear_color(struct gl_video *p, struct m_color color);
void gl_video_set_osd_pts(struct gl_video *p, double pts);
bool gl_video_check_osd_change(struct gl_video *p, struct mp_osd_res *osd,
                               double pts);

void gl_video_screenshot(struct gl_video *p, struct vo_frame *frame,
                         struct voctrl_screenshot *args);

double gl_video_scale_ambient_lux(float lmin, float lmax,
                                  float rmin, float rmax, double lux);
void gl_video_set_ambient_lux(struct gl_video *p, double lux);
void gl_video_set_icc_profile(struct gl_video *p, bstr icc_data);
bool gl_video_icc_auto_enabled(struct gl_video *p);
bool gl_video_gamma_auto_enabled(struct gl_video *p);

void gl_video_reset(struct gl_video *p);
bool gl_video_showing_interpolated_frame(struct gl_video *p);

struct mp_hwdec_devices;
void gl_video_init_hwdecs(struct gl_video *p, struct ra_ctx *ra_ctx,
                          struct mp_hwdec_devices *devs,
                          bool load_all_by_default);
struct hwdec_imgfmt_request;
void gl_video_load_hwdecs_for_img_fmt(struct gl_video *p, struct mp_hwdec_devices *devs,
                                      struct hwdec_imgfmt_request *params);

struct vo;
void gl_video_configure_queue(struct gl_video *p, struct vo *vo);

struct mp_image *gl_video_get_image(struct gl_video *p, int imgfmt, int w, int h,
                                    int stride_align, int flags);

struct mp_image_params *gl_video_get_target_params_ptr(struct gl_video *p);

#endif
