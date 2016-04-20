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

#ifndef MP_GL_USER_SHADERS_H
#define MP_GL_USER_SHADERS_H

#include "common.h"
#include "utils.h"

#define SHADER_API 1
#define SHADER_MAX_HOOKS 16
#define SHADER_MAX_BINDS 6

struct gl_user_shader {
    struct bstr hook_tex[SHADER_MAX_HOOKS];
    struct bstr bind_tex[SHADER_MAX_BINDS];
    struct bstr save_tex;
    struct bstr pass_body;
    struct gl_transform transform;
    int components;
};

// Parse the next shader pass from 'body'. Returns false if the end of the
// string was reached
bool parse_user_shader_pass(struct mp_log *log, struct bstr *body,
                            struct gl_user_shader *out);

#endif
