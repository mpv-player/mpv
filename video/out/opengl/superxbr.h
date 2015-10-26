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

#ifndef MP_GL_SUPERXBR_H
#define MP_GL_SUPERXBR_H

#include "common.h"
#include "utils.h"

extern const struct superxbr_opts superxbr_opts_def;
extern const struct m_sub_options superxbr_conf;

void pass_superxbr(struct gl_shader_cache *sc, int planes, int tex_num,
                   int step, const struct superxbr_opts *conf,
                   struct gl_transform *transform);

#endif
