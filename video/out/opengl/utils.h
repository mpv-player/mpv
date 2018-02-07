/*
 * This file is part of mpv.
 * Parts based on MPlayer code by Reimar Döffinger.
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

#ifndef MP_GL_UTILS_
#define MP_GL_UTILS_

#include <math.h>

#include "video/out/gpu/utils.h"
#include "common.h"

struct mp_log;

void gl_check_error(GL *gl, struct mp_log *log, const char *info);

void gl_upload_tex(GL *gl, GLenum target, GLenum format, GLenum type,
                   const void *dataptr, int stride,
                   int x, int y, int w, int h);

bool gl_read_fbo_contents(GL *gl, int fbo, int dir, GLenum format, GLenum type,
                          int w, int h, uint8_t *dst, int dst_stride);

struct gl_vao {
    GL *gl;
    GLuint vao;     // the VAO object, or 0 if unsupported by driver
    GLuint buffer;  // GL_ARRAY_BUFFER used for the data
    int stride;     // size of each element (interleaved elements are assumed)
    const struct ra_renderpass_input *entries;
    int num_entries;
};

void gl_vao_init(struct gl_vao *vao, GL *gl, int stride,
                 const struct ra_renderpass_input *entries,
                 int num_entries);
void gl_vao_uninit(struct gl_vao *vao);
void gl_vao_draw_data(struct gl_vao *vao, GLenum prim, void *ptr, size_t num);

void gl_set_debug_logger(GL *gl, struct mp_log *log);

bool gl_check_extension(const char *extensions, const char *ext);

#endif
