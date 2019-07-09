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

#ifndef MP_GL_VIDEO_SHADERS_H
#define MP_GL_VIDEO_SHADERS_H

#include <libavutil/lfg.h>

#include "utils.h"
#include "video.h"

extern const struct deband_opts deband_opts_def;
extern const struct m_sub_options deband_conf;

void sampler_prelude(struct gl_shader_cache *sc, int tex_num);
void pass_sample_separated_gen(struct gl_shader_cache *sc, struct scaler *scaler,
                               int d_x, int d_y);
void pass_sample_polar(struct gl_shader_cache *sc, struct scaler *scaler,
                       int components, bool sup_gather);
void pass_compute_polar(struct gl_shader_cache *sc, struct scaler *scaler,
                        int components, int bw, int bh, int iw, int ih);
void pass_sample_bicubic_fast(struct gl_shader_cache *sc);
void pass_sample_oversample(struct gl_shader_cache *sc, struct scaler *scaler,
                            int w, int h);

void pass_linearize(struct gl_shader_cache *sc, enum mp_csp_trc trc);
void pass_delinearize(struct gl_shader_cache *sc, enum mp_csp_trc trc);

void pass_color_map(struct gl_shader_cache *sc, bool is_linear,
                    struct mp_colorspace src, struct mp_colorspace dst,
                    const struct gl_tone_map_opts *opts);

void pass_sample_deband(struct gl_shader_cache *sc, struct deband_opts *opts,
                        AVLFG *lfg, enum mp_csp_trc trc);

void pass_sample_unsharp(struct gl_shader_cache *sc, float param);

#endif
