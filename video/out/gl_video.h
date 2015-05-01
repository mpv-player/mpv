/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef MP_GL_VIDEO_H
#define MP_GL_VIDEO_H

#include <stdbool.h>

#include "options/m_option.h"
#include "sub/osd.h"
#include "gl_common.h"

struct lut3d {
    uint16_t *data;
    int size[3];
};

struct scaler_fun {
    char *name;
    float params[2];
    float blur;
};

struct scaler_config {
    struct scaler_fun kernel;
    struct scaler_fun window;
    float radius;
    float antiring;
};

struct gl_video_opts {
    struct scaler_config scaler[4];
    float gamma;
    int gamma_auto;
    int target_prim;
    int target_trc;
    int linear_scaling;
    int fancy_downscaling;
    int sigmoid_upscaling;
    float sigmoid_center;
    float sigmoid_slope;
    int scaler_resizes_only;
    int npot;
    int pbo;
    int dither_depth;
    int dither_algo;
    int dither_size;
    int temporal_dither;
    int fbo_format;
    int alpha_mode;
    int chroma_location;
    int use_rectangle;
    struct m_color background;
    int interpolation;
    int blend_subs;
};

extern const struct m_sub_options gl_video_conf;
extern const struct gl_video_opts gl_video_opts_hq_def;
extern const struct gl_video_opts gl_video_opts_def;

struct gl_video;

struct gl_video *gl_video_init(GL *gl, struct mp_log *log);
void gl_video_uninit(struct gl_video *p);
void gl_video_set_osd_source(struct gl_video *p, struct osd_state *osd);
void gl_video_set_options(struct gl_video *p, struct gl_video_opts *opts,
                          int *queue_size);
bool gl_video_check_format(struct gl_video *p, int mp_format);
void gl_video_config(struct gl_video *p, struct mp_image_params *params);
void gl_video_set_output_depth(struct gl_video *p, int r, int g, int b);
void gl_video_set_lut3d(struct gl_video *p, struct lut3d *lut3d);
void gl_video_set_image(struct gl_video *p, struct mp_image *img);
void gl_video_render_frame(struct gl_video *p, int fbo, struct frame_timing *t);
void gl_video_resize(struct gl_video *p, int vp_w, int vp_h,
                     struct mp_rect *src, struct mp_rect *dst,
                     struct mp_osd_res *osd);
struct mp_csp_equalizer;
struct mp_csp_equalizer *gl_video_eq_ptr(struct gl_video *p);
void gl_video_eq_update(struct gl_video *p);

void gl_video_set_debug(struct gl_video *p, bool enable);
void gl_video_resize_redraw(struct gl_video *p, int w, int h);

float gl_video_scale_ambient_lux(float lmin, float lmax,
                                 float rmin, float rmax, float lux);
void gl_video_set_ambient_lux(struct gl_video *p, int lux);

void gl_video_set_gl_state(struct gl_video *p);
void gl_video_unset_gl_state(struct gl_video *p);
void gl_video_reset(struct gl_video *p);
bool gl_video_showing_interpolated_frame(struct gl_video *p);

struct gl_hwdec;
void gl_video_set_hwdec(struct gl_video *p, struct gl_hwdec *hwdec);

#endif
