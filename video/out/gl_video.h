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

#include "sub/sub.h"
#include "gl_common.h"

struct lut3d {
    uint16_t *data;
    int size[3];
};

struct gl_video_opts {
    char *scalers[2];
    float scaler_params[2];
    int indirect;
    int gamma;
    int srgb;
    int scale_sep;
    int fancy_downscaling;
    int npot;
    int pbo;
    int dither_depth;
    int fbo_format;
    int stereo_mode;
    int enable_alpha;
};

extern const struct m_sub_options gl_video_conf;

struct gl_video;

struct gl_video *gl_video_init(GL *gl);
void gl_video_uninit(struct gl_video *p);
void gl_video_set_options(struct gl_video *p, struct gl_video_opts *opts);
void gl_video_config(struct gl_video *p, int format, int w, int h, int dw, int dh);
void gl_video_set_output_depth(struct gl_video *p, int r, int g, int b);
void gl_video_set_lut3d(struct gl_video *p, struct lut3d *lut3d);
void gl_video_draw_osd(struct gl_video *p, struct osd_state *osd);
void gl_video_upload_image(struct gl_video *p, struct mp_image *img);
void gl_video_render_frame(struct gl_video *p);
struct mp_image *gl_video_download_image(struct gl_video *p);
void gl_video_resize(struct gl_video *p, struct mp_rect *window,
                     struct mp_rect *src, struct mp_rect *dst,
                     struct mp_osd_res *osd);
bool gl_video_get_csp_override(struct gl_video *p, struct mp_csp_details *csp);
bool gl_video_set_csp_override(struct gl_video *p, struct mp_csp_details *csp);
bool gl_video_set_equalizer(struct gl_video *p, const char *name, int val);
bool gl_video_get_equalizer(struct gl_video *p, const char *name, int *val);

void gl_video_set_debug(struct gl_video *p, bool enable);
void gl_video_resize_redraw(struct gl_video *p, int w, int h);

bool gl_video_check_format(int mp_format);

#endif
