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
 *
 * You can alternatively redistribute this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef MP_GL_VIDEO_SHADERS_H
#define MP_GL_VIDEO_SHADERS_H

#include <libavutil/lfg.h>

#include "common.h"
#include "utils.h"
#include "video.h"

extern const struct deband_opts deband_opts_def;
extern const struct m_sub_options deband_conf;

void sampler_prelude(struct gl_shader_cache *sc, int tex_num);
void pass_sample_separated_gen(struct gl_shader_cache *sc, struct scaler *scaler,
                               int d_x, int d_y);
void pass_sample_polar(struct gl_shader_cache *sc, struct scaler *scaler);
void pass_sample_bicubic_fast(struct gl_shader_cache *sc);
void pass_sample_sharpen3(struct gl_shader_cache *sc, struct scaler *scaler);
void pass_sample_sharpen5(struct gl_shader_cache *sc, struct scaler *scaler);
void pass_sample_oversample(struct gl_shader_cache *sc, struct scaler *scaler,
                            int w, int h);

void pass_linearize(struct gl_shader_cache *sc, enum mp_csp_trc trc);
void pass_delinearize(struct gl_shader_cache *sc, enum mp_csp_trc trc);

void pass_sample_deband(struct gl_shader_cache *sc, struct deband_opts *opts,
                        int tex_num, GLenum tex_target, float tex_mul,
                        float img_w, float img_h, AVLFG *lfg);

#endif
