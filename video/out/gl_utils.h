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
    const char *name;
    int num_elems;
    GLenum type;
    bool normalized;
    int offset;
};

struct gl_vao {
    GL *gl;
    GLuint vao;
    GLuint buffer;
    int stride; // always assuming interleaved elements
    const struct gl_vao_entry *entries;
};

void gl_vao_init(struct gl_vao *vao, GL *gl, int stride,
                 const struct gl_vao_entry *entries);
void gl_vao_uninit(struct gl_vao *vao);
void gl_vao_bind(struct gl_vao *vao);
void gl_vao_unbind(struct gl_vao *vao);
void gl_vao_bind_attribs(struct gl_vao *vao, GLuint program);

#endif
