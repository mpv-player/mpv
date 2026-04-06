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
#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include "common/msg.h"
#include "misc/ctype.h"
#include "user_shaders.h"

static int resolve_enum_name(struct gl_user_shader_param *param, struct bstr val)
{
    struct bstr enum_rest = param->enum_body;
    int idx = 0;
    while (enum_rest.len > 0) {
        struct bstr enum_line = bstr_strip(bstr_getline(enum_rest, &enum_rest));
        if (enum_line.len == 0)
            continue;
        if (bstr_equals(enum_line, val))
            return idx;
        idx++;
    }
    return -1;
}

bool parse_shader_param_value(struct mp_log *log, struct gl_user_shader_param *param,
                              struct bstr val, double *out)
{
    double v, range[2];
    struct bstr rest = {0};

    switch (param->type) {
    case GL_USER_SHADER_PARAM_UNKNOWN:
        mp_err(log, "Missing type for param '%.*s'\n", BSTR_P(param->name));
        return false;
    case GL_USER_SHADER_PARAM_INT:
    case GL_USER_SHADER_PARAM_DEFINE:
        v = resolve_enum_name(param, val);
        v = v >= 0 ? v : bstrtoll(val, &rest, 10);
        range[0] = INT_MIN;
        range[1] = INT_MAX;
        break;
    case GL_USER_SHADER_PARAM_FLOAT:
        v = bstrtod(val, &rest);
        range[0] = -FLT_MAX;
        range[1] = FLT_MAX;
        break;
    default:
        MP_ASSERT_UNREACHABLE();
    }

    if (val.len == 0 || rest.len > 0) {
        mp_err(log, "Invalid value for option '%.*s': '%.*s'\n",
               BSTR_P(param->name), BSTR_P(val));
        return false;
    }

    if (param->has_min)
        range[0] = MPMAX(param->min, range[0]);
    if (param->has_max)
        range[1] = MPMIN(param->max, range[1]);
    if (v < range[0] || v > range[1] || !isfinite(v)) {
        mp_err(log, "Value of %f for option '%.*s' out of range [%f, %f]\n",
               v, BSTR_P(param->name), range[0], range[1]);
        return false;
    }

    *out = v;
    return true;
}

static bool parse_bound(struct mp_log *log, struct bstr line,
                        const char *name, double *val, bool *has)
{
    struct bstr rest, strip = bstr_strip(line);
    *val = bstrtod(strip, &rest);
    *has = rest.len == 0 && strip.len > 0;
    if (!*has)
        mp_err(log, "Error while parsing PARAM %s!\n", name);
    return *has;
}

static bool parse_param(struct mp_log *log, struct bstr *body,
                        struct gl_user_shader_param *params, int *num_params)
{
    struct bstr rest;
    struct bstr line = bstr_strip(bstr_getline(*body, &rest));

    if (!bstr_eatstart0(&line, "//!PARAM")) {
        mp_err(log, "Expected PARAM header!\n");
        return false;
    }

    if (*num_params == SHADER_MAX_PARAMS) {
        mp_err(log, "Shader defines too many parameters!\n");
        return false;
    }

    struct gl_user_shader_param *param = &params[(*num_params)++];
    *param = (struct gl_user_shader_param){ .name = bstr_strip(line) };
    bool is_enum = false;

    while (true) {
        *body = rest;
        line = bstr_strip(bstr_getline(*body, &rest));

        if (line.len == 0) {
            if (rest.len == 0) {
                mp_err(log, "Missing initial parameter value!\n");
                return false;
            }
            continue;
        }

        if (!bstr_eatstart0(&line, "//!")) {
            if (is_enum) {
                int count = 0;
                struct bstr enum_rest = {
                    .start = line.start,
                    .len = rest.start + rest.len - line.start,
                };
                param->enum_body = enum_rest;

                while (enum_rest.len > 0) {
                    struct bstr next_line;
                    struct bstr enum_line = bstr_strip(bstr_getline(enum_rest, &next_line));
                    if (bstr_startswith0(enum_line, "//!"))
                        break;
                    if (next_line.len == 0)
                        continue;
                    enum_rest = next_line;
                    count++;
                }

                if (param->type != GL_USER_SHADER_PARAM_INT &&
                    param->type != GL_USER_SHADER_PARAM_DEFINE)
                {
                    mp_err(log, "ENUM parameter '%.*s' must be type int or DEFINE!\n",
                           BSTR_P(param->name));
                    return false;
                }
                if (count == 0) {
                    mp_err(log, "ENUM parameter '%.*s' has no values!\n",
                           BSTR_P(param->name));
                    return false;
                }

                param->enum_body.len -= enum_rest.len;
                param->has_min = true;
                param->has_max = true;
                param->max = count - 1;

                *body = enum_rest;
                return true;
            }

            if (!parse_shader_param_value(log, param, line, &param->initial))
                return false;
            param->value = param->initial;

            *body = rest;
            struct bstr discard;
            if (bstr_split_tok(*body, "//!", &discard, body)) {
                body->start -= 3;
                body->len += 3;
            }
            return true;
        }

        if (bstr_eatstart0(&line, "DESC")) {
            param->desc = bstr_strip(line);
            continue;
        }

        if (bstr_eatstart0(&line, "TYPE")) {
            line = bstr_strip(line);
            is_enum = bstr_eatstart0(&line, "ENUM");
            line = bstr_strip(line);
            if (bstr_equals0(line, "float")) {
                param->type = GL_USER_SHADER_PARAM_FLOAT;
            } else if (bstr_equals0(line, "int")) {
                param->type = GL_USER_SHADER_PARAM_INT;
            } else if (bstr_equals0(line, "DEFINE")) {
                param->type = GL_USER_SHADER_PARAM_DEFINE;
            } else {
                mp_err(log, "Unrecognized PARAM TYPE: '%.*s'!\n", BSTR_P(line));
                return false;
            }
            continue;
        }

        if (bstr_eatstart0(&line, "MINIMUM")) {
            if (!parse_bound(log, line, "MINIMUM", &param->min, &param->has_min))
                return false;
            continue;
        }

        if (bstr_eatstart0(&line, "MAXIMUM")) {
            if (!parse_bound(log, line, "MAXIMUM", &param->max, &param->has_max))
                return false;
            continue;
        }

        mp_err(log, "Unrecognized command '%.*s'!\n", BSTR_P(line));
        return false;
    }
}

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
            mp_assert(idx < MAX_SZEXP_SIZE);
            stack[idx++] = expr[i].val.cval;
            continue;

        case SZEXP_OP1:
            if (idx < 1) {
                mp_warn(log, "Stack underflow in RPN expression!\n");
                return false;
            }

            switch (expr[i].val.op) {
            case SZEXP_OP_NOT: stack[idx-1] = !stack[idx-1]; break;
            default: MP_ASSERT_UNREACHABLE();
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
            default: MP_ASSERT_UNREACHABLE();
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
            if (out->components < 0 || out->components > 4) {
                mp_err(log, "Invalid COMPONENTS: %d\n", out->components);
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
                       const char *path,
                       void *priv,
                       bool (*dohook)(void *p, const char *path,
                                      const struct gl_user_shader_hook *hook),
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

    // params need to be collected in a first pass before dohook()
    struct gl_user_shader_param global_params[SHADER_MAX_PARAMS] = {0};
    int num_global_params = 0;
    struct gl_user_shader_hook *hooks = NULL;
    int num_hooks = 0;

    while (shader.len > 0) {
        if (bstr_startswith0(shader, "//!TEXTURE")) {
            struct gl_user_shader_tex t;
            if (!parse_tex(log, ra, &shader, &t) || !dotex(priv, t))
                goto out;
            continue;
        }

        if (bstr_startswith0(shader, "//!PARAM")) {
            if (!parse_param(log, &shader, global_params, &num_global_params))
                goto out;
            continue;
        }

        struct gl_user_shader_hook h;
        if (!parse_hook(log, &shader, &h))
            goto out;

        MP_TARRAY_APPEND(NULL, hooks, num_hooks, h);
    }

    for (int i = 0; i < num_hooks; i++) {
        struct gl_user_shader_hook *h = &hooks[i];

        h->num_params = num_global_params;
        memcpy(h->params, global_params, num_global_params * sizeof(h->params[0]));

        if (!dohook(priv, path, h))
            goto out;
    }
out:
    talloc_free(hooks);
}
