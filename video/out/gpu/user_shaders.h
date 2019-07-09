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

#include "utils.h"
#include "ra.h"

#define SHADER_MAX_HOOKS 16
#define SHADER_MAX_BINDS 16
#define MAX_SZEXP_SIZE 32

enum szexp_op {
    SZEXP_OP_ADD,
    SZEXP_OP_SUB,
    SZEXP_OP_MUL,
    SZEXP_OP_DIV,
    SZEXP_OP_MOD,
    SZEXP_OP_NOT,
    SZEXP_OP_GT,
    SZEXP_OP_LT,
    SZEXP_OP_EQ,
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

struct compute_info {
    bool active;
    int block_w, block_h;     // Block size (each block corresponds to one WG)
    int threads_w, threads_h; // How many threads form a working group
    bool directly_writes;     // If true, shader is assumed to imageStore(out_image)
};

struct gl_user_shader_hook {
    struct bstr pass_desc;
    struct bstr hook_tex[SHADER_MAX_HOOKS];
    struct bstr bind_tex[SHADER_MAX_BINDS];
    struct bstr save_tex;
    struct bstr pass_body;
    struct gl_transform offset;
    bool align_offset;
    struct szexp width[MAX_SZEXP_SIZE];
    struct szexp height[MAX_SZEXP_SIZE];
    struct szexp cond[MAX_SZEXP_SIZE];
    int components;
    struct compute_info compute;
};

struct gl_user_shader_tex {
    struct bstr name;
    struct ra_tex_params params;
    // for video.c
    struct ra_tex *tex;
};

// Parse the next shader block from `body`. The callbacks are invoked on every
// valid shader block parsed.
void parse_user_shader(struct mp_log *log, struct ra *ra, struct bstr shader,
                       void *priv,
                       bool (*dohook)(void *p, struct gl_user_shader_hook hook),
                       bool (*dotex)(void *p, struct gl_user_shader_tex tex));

// Evaluate a szexp, given a lookup function for named textures
bool eval_szexpr(struct mp_log *log, void *priv,
                 bool (*lookup)(void *priv, struct bstr var, float size[2]),
                 struct szexp expr[MAX_SZEXP_SIZE], float *result);

#endif
