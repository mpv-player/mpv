/*
 * This file is part of mpv.
 * Parts based on MPlayer code by Reimar Döffinger.
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

#ifndef MP_GL_UTILS_
#define MP_GL_UTILS_

#include "gl_common.h"

struct mp_log;

void glCheckError(GL *gl, struct mp_log *log, const char *info);

int glFmt2bpp(GLenum format, GLenum type);
void glUploadTex(GL *gl, GLenum target, GLenum format, GLenum type,
                 const void *dataptr, int stride,
                 int x, int y, int w, int h, int slice);
void glClearTex(GL *gl, GLenum target, GLenum format, GLenum type,
                int x, int y, int w, int h, uint8_t val, void **scratch);

mp_image_t *glGetWindowScreenshot(GL *gl);

// print a multi line string with line numbers (e.g. for shader sources)
// log, lev: module and log level, as in mp_msg()
void mp_log_source(struct mp_log *log, int lev, const char *src);

struct gl_vao_entry {
    // used for shader / glBindAttribLocation
    const char *name;
    // glVertexAttribPointer() arguments
    int num_elems;      // size (number of elements)
    GLenum type;
    bool normalized;
    int offset;
};

struct gl_vao {
    GL *gl;
    GLuint vao;     // the VAO object, or 0 if unsupported by driver
    GLuint buffer;  // GL_ARRAY_BUFFER used for the data
    int stride;     // size of each element (interleaved elements are assumed)
    const struct gl_vao_entry *entries;
};

void gl_vao_init(struct gl_vao *vao, GL *gl, int stride,
                 const struct gl_vao_entry *entries);
void gl_vao_uninit(struct gl_vao *vao);
void gl_vao_bind(struct gl_vao *vao);
void gl_vao_unbind(struct gl_vao *vao);
void gl_vao_bind_attribs(struct gl_vao *vao, GLuint program);
void gl_vao_draw_data(struct gl_vao *vao, GLenum prim, void *ptr, size_t num);

struct fbotex {
    GL *gl;
    GLuint fbo;
    GLuint texture;
    int tex_w, tex_h;           // size of .texture
    int vp_x, vp_y, vp_w, vp_h; // viewport of fbo / used part of the texture
};

bool fbotex_init(struct fbotex *fbo, GL *gl, struct mp_log *log, int w, int h,
                 GLenum gl_target, GLenum gl_filter, GLenum iformat);
void fbotex_uninit(struct fbotex *fbo);

void gl_matrix_ortho2d(float m[3][3], float x0, float x1, float y0, float y1);

void gl_set_debug_logger(GL *gl, struct mp_log *log);

#endif
