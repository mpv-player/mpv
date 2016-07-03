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
#define MAX_SZEXP_SIZE 32

enum szexp_op {
    SZEXP_OP_ADD,
    SZEXP_OP_SUB,
    SZEXP_OP_MUL,
    SZEXP_OP_DIV,
    SZEXP_OP_NOT,
    SZEXP_OP_GT,
    SZEXP_OP_LT,
};

enum szexp_tag {
    SZEXP_END = 0, // End of an RPN expression
    SZEXP_CONST, // Push a constant value onto the stack
    SZEXP_VAR_W, // Get the width/height of a named texture (variable)
    SZEXP_VAR_H,
    SZEXP_OP2, // Pop two elements and push the result of a dyadic operation
    SZEXP_OP1, // Pop one element and push the result of a monadic operation
};

struct szexp {
    enum szexp_tag tag;
    union {
        float cval;
        struct bstr varname;
        enum szexp_op op;
    } val;
};

struct gl_user_shader {
    struct bstr hook_tex[SHADER_MAX_HOOKS];
    struct bstr bind_tex[SHADER_MAX_BINDS];
    struct bstr save_tex;
    struct bstr pass_body;
    struct gl_transform offset;
    struct szexp width[MAX_SZEXP_SIZE];
    struct szexp height[MAX_SZEXP_SIZE];
    struct szexp cond[MAX_SZEXP_SIZE];
    int components;
};

// Parse the next shader pass from 'body'. Returns false if the end of the
// string was reached
bool parse_user_shader_pass(struct mp_log *log, struct bstr *body,
                            struct gl_user_shader *out);

// Evaluate a szexp, given a lookup function for named textures
bool eval_szexpr(struct mp_log *log, void *priv,
                 bool (*lookup)(void *priv, struct bstr var, float size[2]),
                 struct szexp expr[MAX_SZEXP_SIZE], float *result);

#endif
