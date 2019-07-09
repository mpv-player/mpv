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

#include <assert.h>
#include <math.h>

#include "common/msg.h"
#include "misc/ctype.h"
#include "user_shaders.h"

static bool parse_rpn_szexpr(struct bstr line, struct szexp out[MAX_SZEXP_SIZE])
{
    int pos = 0;

    while (line.len > 0) {
        struct bstr word = bstr_strip(bstr_splitchar(line, &line, ' '));
        if (word.len == 0)
            continue;

        if (pos >= MAX_SZEXP_SIZE)
            return false;

        struct szexp *exp = &out[pos++];

        if (bstr_eatend0(&word, ".w") || bstr_eatend0(&word, ".width")) {
            exp->tag = SZEXP_VAR_W;
            exp->val.varname = word;
            continue;
        }

        if (bstr_eatend0(&word, ".h") || bstr_eatend0(&word, ".height")) {
            exp->tag = SZEXP_VAR_H;
            exp->val.varname = word;
            continue;
        }

        switch (word.start[0]) {
        case '+': exp->tag = SZEXP_OP2; exp->val.op = SZEXP_OP_ADD; continue;
        case '-': exp->tag = SZEXP_OP2; exp->val.op = SZEXP_OP_SUB; continue;
        case '*': exp->tag = SZEXP_OP2; exp->val.op = SZEXP_OP_MUL; continue;
        case '/': exp->tag = SZEXP_OP2; exp->val.op = SZEXP_OP_DIV; continue;
        case '%': exp->tag = SZEXP_OP2; exp->val.op = SZEXP_OP_MOD; continue;
        case '!': exp->tag = SZEXP_OP1; exp->val.op = SZEXP_OP_NOT; continue;
        case '>': exp->tag = SZEXP_OP2; exp->val.op = SZEXP_OP_GT;  continue;
        case '<': exp->tag = SZEXP_OP2; exp->val.op = SZEXP_OP_LT;  continue;
        case '=': exp->tag = SZEXP_OP2; exp->val.op = SZEXP_OP_EQ;  continue;
        }

        if (mp_isdigit(word.start[0])) {
            exp->tag = SZEXP_CONST;
            if (bstr_sscanf(word, "%f", &exp->val.cval) != 1)
                return false;
            continue;
        }

        // Some sort of illegal expression
        return false;
    }

    return true;
}

// Returns whether successful. 'result' is left untouched on failure
bool eval_szexpr(struct mp_log *log, void *priv,
                 bool (*lookup)(void *priv, struct bstr var, float size[2]),
                 struct szexp expr[MAX_SZEXP_SIZE], float *result)
{
    float stack[MAX_SZEXP_SIZE] = {0};
    int idx = 0; // points to next element to push

    for (int i = 0; i < MAX_SZEXP_SIZE; i++) {
        switch (expr[i].tag) {
        case SZEXP_END:
            goto done;

        case SZEXP_CONST:
            // Since our SZEXPs are bound by MAX_SZEXP_SIZE, it should be
            // impossible to overflow the stack
            assert(idx < MAX_SZEXP_SIZE);
            stack[idx++] = expr[i].val.cval;
            continue;

        case SZEXP_OP1:
            if (idx < 1) {
                mp_warn(log, "Stack underflow in RPN expression!\n");
                return false;
            }

            switch (expr[i].val.op) {
            case SZEXP_OP_NOT: stack[idx-1] = !stack[idx-1]; break;
            default: abort();
            }
            continue;

        case SZEXP_OP2:
            if (idx < 2) {
                mp_warn(log, "Stack underflow in RPN expression!\n");
                return false;
            }

            // Pop the operands in reverse order
            float op2 = stack[--idx];
            float op1 = stack[--idx];
            float res = 0.0;
            switch (expr[i].val.op) {
            case SZEXP_OP_ADD: res = op1 + op2; break;
            case SZEXP_OP_SUB: res = op1 - op2; break;
            case SZEXP_OP_MUL: res = op1 * op2; break;
            case SZEXP_OP_DIV: res = op1 / op2; break;
            case SZEXP_OP_MOD: res = fmodf(op1, op2); break;
            case SZEXP_OP_GT:  res = op1 > op2; break;
            case SZEXP_OP_LT:  res = op1 < op2; break;
            case SZEXP_OP_EQ:  res = op1 == op2; break;
            default: abort();
            }

            if (!isfinite(res)) {
                mp_warn(log, "Illegal operation in RPN expression!\n");
                return false;
            }

            stack[idx++] = res;
            continue;

        case SZEXP_VAR_W:
        case SZEXP_VAR_H: {
            struct bstr name = expr[i].val.varname;
            float size[2];

            if (!lookup(priv, name, size)) {
                mp_warn(log, "Variable %.*s not found in RPN expression!\n",
                        BSTR_P(name));
                return false;
            }

            stack[idx++] = (expr[i].tag == SZEXP_VAR_W) ? size[0] : size[1];
            continue;
            }
        }
    }

done:
    // Return the single stack element
    if (idx != 1) {
        mp_warn(log, "Malformed stack after RPN expression!\n");
        return false;
    }

    *result = stack[0];
    return true;
}

static bool parse_hook(struct mp_log *log, struct bstr *body,
                       struct gl_user_shader_hook *out)
{
    *out = (struct gl_user_shader_hook){
        .pass_desc = bstr0("(unknown)"),
        .offset = identity_trans,
        .align_offset = false,
        .width = {{ SZEXP_VAR_W, { .varname = bstr0("HOOKED") }}},
        .height = {{ SZEXP_VAR_H, { .varname = bstr0("HOOKED") }}},
        .cond = {{ SZEXP_CONST, { .cval = 1.0 }}},
    };

    int hook_idx = 0;
    int bind_idx = 0;

    // Parse all headers
    while (true) {
        struct bstr rest;
        struct bstr line = bstr_strip(bstr_getline(*body, &rest));

        // Check for the presence of the magic line beginning
        if (!bstr_eatstart0(&line, "//!"))
            break;

        *body = rest;

        // Parse the supported commands
        if (bstr_eatstart0(&line, "HOOK")) {
            if (hook_idx == SHADER_MAX_HOOKS) {
                mp_err(log, "Passes may only hook up to %d textures!\n",
                       SHADER_MAX_HOOKS);
                return false;
            }
            out->hook_tex[hook_idx++] = bstr_strip(line);
            continue;
        }

        if (bstr_eatstart0(&line, "BIND")) {
            if (bind_idx == SHADER_MAX_BINDS) {
                mp_err(log, "Passes may only bind up to %d textures!\n",
                       SHADER_MAX_BINDS);
                return false;
            }
            out->bind_tex[bind_idx++] = bstr_strip(line);
            continue;
        }

        if (bstr_eatstart0(&line, "SAVE")) {
            out->save_tex = bstr_strip(line);
            continue;
        }

        if (bstr_eatstart0(&line, "DESC")) {
            out->pass_desc = bstr_strip(line);
            continue;
        }

        if (bstr_eatstart0(&line, "OFFSET")) {
            line = bstr_strip(line);
            if (bstr_equals0(line, "ALIGN")) {
                out->align_offset = true;
            } else {
                float ox, oy;
                if (bstr_sscanf(line, "%f %f", &ox, &oy) != 2) {
                    mp_err(log, "Error while parsing OFFSET!\n");
                    return false;
                }
                out->offset.t[0] = ox;
                out->offset.t[1] = oy;
            }
            continue;
        }

        if (bstr_eatstart0(&line, "WIDTH")) {
            if (!parse_rpn_szexpr(line, out->width)) {
                mp_err(log, "Error while parsing WIDTH!\n");
                return false;
            }
            continue;
        }

        if (bstr_eatstart0(&line, "HEIGHT")) {
            if (!parse_rpn_szexpr(line, out->height)) {
                mp_err(log, "Error while parsing HEIGHT!\n");
                return false;
            }
            continue;
        }

        if (bstr_eatstart0(&line, "WHEN")) {
            if (!parse_rpn_szexpr(line, out->cond)) {
                mp_err(log, "Error while parsing WHEN!\n");
                return false;
            }
            continue;
        }

        if (bstr_eatstart0(&line, "COMPONENTS")) {
            if (bstr_sscanf(line, "%d", &out->components) != 1) {
                mp_err(log, "Error while parsing COMPONENTS!\n");
                return false;
            }
            continue;
        }

        if (bstr_eatstart0(&line, "COMPUTE")) {
            struct compute_info *ci = &out->compute;
            int num = bstr_sscanf(line, "%d %d %d %d", &ci->block_w, &ci->block_h,
                                  &ci->threads_w, &ci->threads_h);

            if (num == 2 || num == 4) {
                ci->active = true;
                ci->directly_writes = true;
            } else {
                mp_err(log, "Error while parsing COMPUTE!\n");
                return false;
            }
            continue;
        }

        // Unknown command type
        mp_err(log, "Unrecognized command '%.*s'!\n", BSTR_P(line));
        return false;
    }

    // The rest of the file up until the next magic line beginning (if any)
    // shall be the shader body
    if (bstr_split_tok(*body, "//!", &out->pass_body, body)) {
        // Make sure the magic line is part of the rest
        body->start -= 3;
        body->len += 3;
    }

    // Sanity checking
    if (hook_idx == 0)
        mp_warn(log, "Pass has no hooked textures (will be ignored)!\n");

    return true;
}

static bool parse_tex(struct mp_log *log, struct ra *ra, struct bstr *body,
                      struct gl_user_shader_tex *out)
{
    *out = (struct gl_user_shader_tex){
        .name = bstr0("USER_TEX"),
        .params = {
            .dimensions = 2,
            .w = 1, .h = 1, .d = 1,
            .render_src = true,
            .src_linear = true,
        },
    };
    struct ra_tex_params *p = &out->params;

    while (true) {
        struct bstr rest;
        struct bstr line = bstr_strip(bstr_getline(*body, &rest));

        if (!bstr_eatstart0(&line, "//!"))
            break;

        *body = rest;

        if (bstr_eatstart0(&line, "TEXTURE")) {
            out->name = bstr_strip(line);
            continue;
        }

        if (bstr_eatstart0(&line, "SIZE")) {
            p->dimensions = bstr_sscanf(line, "%d %d %d", &p->w, &p->h, &p->d);
            if (p->dimensions < 1 || p->dimensions > 3 ||
                p->w < 1 || p->h < 1 || p->d < 1)
            {
                mp_err(log, "Error while parsing SIZE!\n");
                return false;
            }
            continue;
        }

        if (bstr_eatstart0(&line, "FORMAT ")) {
            p->format = NULL;
            for (int n = 0; n < ra->num_formats; n++) {
                const struct ra_format *fmt = ra->formats[n];
                if (bstr_equals0(line, fmt->name)) {
                    p->format = fmt;
                    break;
                }
            }
            // (pixel_size==0 is for opaque formats)
            if (!p->format || !p->format->pixel_size) {
                mp_err(log, "Unrecognized/unavailable FORMAT name: '%.*s'!\n",
                       BSTR_P(line));
                return false;
            }
            continue;
        }

        if (bstr_eatstart0(&line, "FILTER")) {
            line = bstr_strip(line);
            if (bstr_equals0(line, "LINEAR")) {
                p->src_linear = true;
            } else if (bstr_equals0(line, "NEAREST")) {
                p->src_linear = false;
            } else {
                mp_err(log, "Unrecognized FILTER: '%.*s'!\n", BSTR_P(line));
                return false;
            }
            continue;
        }

        if (bstr_eatstart0(&line, "BORDER")) {
            line = bstr_strip(line);
            if (bstr_equals0(line, "CLAMP")) {
                p->src_repeat = false;
            } else if (bstr_equals0(line, "REPEAT")) {
                p->src_repeat = true;
            } else {
                mp_err(log, "Unrecognized BORDER: '%.*s'!\n", BSTR_P(line));
                return false;
            }
            continue;
        }

        mp_err(log, "Unrecognized command '%.*s'!\n", BSTR_P(line));
        return false;
    }

    if (!p->format) {
        mp_err(log, "No FORMAT specified.\n");
        return false;
    }

    if (p->src_linear && !p->format->linear_filter) {
        mp_err(log, "The specified texture format cannot be filtered!\n");
        return false;
    }

    // Decode the rest of the section (up to the next //! marker) as raw hex
    // data for the texture
    struct bstr hexdata;
    if (bstr_split_tok(*body, "//!", &hexdata, body)) {
        // Make sure the magic line is part of the rest
        body->start -= 3;
        body->len += 3;
    }

    struct bstr tex;
    if (!bstr_decode_hex(NULL, bstr_strip(hexdata), &tex)) {
        mp_err(log, "Error while parsing TEXTURE body: must be a valid "
                    "hexadecimal sequence, on a single line!\n");
        return false;
    }

    int expected_len = p->w * p->h * p->d * p->format->pixel_size;
    if (tex.len != expected_len) {
        mp_err(log, "Shader TEXTURE size mismatch: got %zd bytes, expected %d!\n",
               tex.len, expected_len);
        talloc_free(tex.start);
        return false;
    }

    p->initial_data = tex.start;
    return true;
}

void parse_user_shader(struct mp_log *log, struct ra *ra, struct bstr shader,
                       void *priv,
                       bool (*dohook)(void *p, struct gl_user_shader_hook hook),
                       bool (*dotex)(void *p, struct gl_user_shader_tex tex))
{
    if (!dohook || !dotex || !shader.len)
        return;

    // Skip all garbage (e.g. comments) before the first header
    int pos = bstr_find(shader, bstr0("//!"));
    if (pos < 0) {
        mp_warn(log, "Shader appears to contain no headers!\n");
        return;
    }
    shader = bstr_cut(shader, pos);

    // Loop over the file
    while (shader.len > 0)
    {
        // Peek at the first header to dispatch the right type
        if (bstr_startswith0(shader, "//!TEXTURE")) {
            struct gl_user_shader_tex t;
            if (!parse_tex(log, ra, &shader, &t) || !dotex(priv, t))
                return;
            continue;
        }

        struct gl_user_shader_hook h;
        if (!parse_hook(log, &shader, &h) || !dohook(priv, h))
            return;
    }
}
