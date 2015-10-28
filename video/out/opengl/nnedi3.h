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

#ifndef MP_GL_NNEDI3_H
#define MP_GL_NNEDI3_H

#include "common.h"
#include "utils.h"

#define NNEDI3_UPLOAD_UBO 0
#define NNEDI3_UPLOAD_SHADER 1

struct nnedi3_opts {
    int neurons;
    int window;
    int upload;
};

extern const struct nnedi3_opts nnedi3_opts_def;
extern const struct m_sub_options nnedi3_conf;

const float* get_nnedi3_weights(const struct nnedi3_opts *conf, int *size);

void pass_nnedi3(struct gl_shader_cache *sc, int planes, int tex_num,
                 int step, const struct nnedi3_opts *conf,
                 struct gl_transform *transform);

#endif
