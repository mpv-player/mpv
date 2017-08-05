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

#include "common.h"
#include "ra.h"

struct mp_log;

void gl_check_error(GL *gl, struct mp_log *log, const char *info);

void gl_upload_tex(GL *gl, GLenum target, GLenum format, GLenum type,
                   const void *dataptr, int stride,
                   int x, int y, int w, int h);

mp_image_t *gl_read_fbo_contents(GL *gl, int fbo, int w, int h);

// print a multi line string with line numbers (e.g. for shader sources)
// log, lev: module and log level, as in mp_msg()
void mp_log_source(struct mp_log *log, int lev, const char *src);

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

#define NUM_PBO_BUFFERS 3

struct gl_pbo_upload {
    GL *gl;
    int index;
    GLuint buffer;
    size_t buffer_size;
};

void gl_pbo_upload_tex(struct gl_pbo_upload *pbo, GL *gl, bool use_pbo,
                       GLenum target, GLenum format,  GLenum type,
                       int tex_w, int tex_h, const void *dataptr, int stride,
                       int x, int y, int w, int h);
void gl_pbo_upload_uninit(struct gl_pbo_upload *pbo);

int gl_determine_16bit_tex_depth(GL *gl);
int gl_get_fb_depth(GL *gl, int fbo);

#endif
