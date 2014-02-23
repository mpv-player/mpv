/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stddef.h>

#include "bstr/bstr.h"
#include "common/common.h"
#include "common/msg.h"
#include "options/m_option.h"

#include "cmd_parse.h"
#include "cmd_list.h"
#include "input.h"

static void destroy_cmd(void *ptr)
{
    struct mp_cmd *cmd = ptr;
    for (int n = 0; n < cmd->nargs; n++)
        m_option_free(cmd->args[n].type, &cmd->args[n].v);
}

static bool read_token(bstr str, bstr *out_rest, bstr *out_token)
{
    bstr t = bstr_lstrip(str);
    char nextc = t.len > 0 ? t.start[0] : 0;
    if (nextc == '#' || nextc == ';')
        return false; // comment or command separator
    int next = bstrcspn(t, WHITESPACE);
    if (!next)
        return false;
    *out_token = bstr_splice(t, 0, next);
    *out_rest = bstr_cut(t, next);
    return true;
}

// Somewhat awkward; the main purpose is supporting both strings and
// pre-split string arrays as input.
struct parse_ctx {
    struct mp_log *log;
    const char *loc;
    void *tmp;
    bool array_input; // false: use start/str, true: use num_strs/strs
    bstr start, str;
    bstr *strs;
    int num_strs;
};

static int pctx_read_token(struct parse_ctx *ctx, bstr *out)
{
    *out = (bstr){0};
    if (ctx->array_input) {
        if (!ctx->num_strs)
            return 0;
        *out = ctx->strs[0];
        ctx->strs++;
        ctx->num_strs--;
        return 1;
    } else {
        ctx->str = bstr_lstrip(ctx->str);
        bstr start = ctx->str;
        if (bstr_eatstart0(&ctx->str, "\"")) {
            if (!mp_append_escaped_string_noalloc(ctx->tmp, out, &ctx->str)) {
                MP_ERR(ctx, "Broken string escapes: ...>%.*s<.\n", BSTR_P(start));
                return -1;
            }
            if (!bstr_eatstart0(&ctx->str, "\"")) {
                MP_ERR(ctx, "Unterminated quotes: ...>%.*s<.\n", BSTR_P(start));
                return -1;
            }
            return 1;
        }
        return read_token(ctx->str, &ctx->str, out) ? 1 : 0;
    }
}

static bstr pctx_get_trailing(struct parse_ctx *ctx)
{
    if (ctx->array_input) {
        if (ctx->num_strs == 0)
            return (bstr){0};
        return ctx->strs[0]; // mentioning the first trailing arg is enough?
    } else {
        bstr dummy;
        if (!read_token(ctx->str, &dummy, &dummy))
            return (bstr){0};
        return ctx->str;
    }
}

struct flag {
    const char *name;
    unsigned int remove, add;
};

static const struct flag cmd_flags[] = {
    {"pausing",             MP_PAUSING_FLAGS, MP_PAUSING},
    {"pausing-toggle",      MP_PAUSING_FLAGS, MP_PAUSING_TOGGLE},
    {"no-osd",              MP_ON_OSD_FLAGS, MP_ON_OSD_NO},
    {"osd-bar",             MP_ON_OSD_FLAGS, MP_ON_OSD_BAR},
    {"osd-msg",             MP_ON_OSD_FLAGS, MP_ON_OSD_MSG},
    {"osd-msg-bar",         MP_ON_OSD_FLAGS, MP_ON_OSD_MSG | MP_ON_OSD_BAR},
    {"osd-auto",            MP_ON_OSD_FLAGS, MP_ON_OSD_AUTO},
    {"expand-properties",   0,                    MP_EXPAND_PROPERTIES},
    {"raw",                 MP_EXPAND_PROPERTIES, 0},
    {0}
};

static struct mp_cmd *parse_cmd(struct parse_ctx *ctx, int def_flags)
{
    struct mp_cmd *cmd = NULL;
    int r;

    if (!ctx->array_input) {
        ctx->str = bstr_lstrip(ctx->str);
        bstr old = ctx->str;
        if (mp_replace_legacy_cmd(ctx->tmp, &ctx->str)) {
            MP_WARN(ctx, "Warning: command '%.*s' is deprecated, "
                    "replaced with '%.*s' at %s.\n",
                    BSTR_P(old), BSTR_P(ctx->str), ctx->loc);
            ctx->start = ctx->str;
        }
    }

    bstr cur_token;
    if (pctx_read_token(ctx, &cur_token) < 0)
        goto error;

    while (1) {
        for (int n = 0; cmd_flags[n].name; n++) {
            if (bstr_equals0(cur_token, cmd_flags[n].name)) {
                if (pctx_read_token(ctx, &cur_token) < 0)
                    goto error;
                def_flags &= ~cmd_flags[n].remove;
                def_flags |= cmd_flags[n].add;
                goto cont;
            }
        }
        break;
    cont: ;
    }

    if (cur_token.len == 0) {
        MP_ERR(ctx, "Command name missing.\n");
        goto error;
    }
    const struct mp_cmd_def *cmd_def = NULL;
    for (int n = 0; mp_cmds[n].name; n++) {
        if (bstr_equals0(cur_token, mp_cmds[n].name)) {
            cmd_def = &mp_cmds[n];
            break;
        }
    }

    if (!cmd_def) {
        MP_ERR(ctx, "Command '%.*s' not found.\n", BSTR_P(cur_token));
        goto error;
    }

    cmd = talloc_ptrtype(NULL, cmd);
    talloc_set_destructor(cmd, destroy_cmd);
    *cmd = (struct mp_cmd) {
        .name = (char *)cmd_def->name,
        .id = cmd_def->id,
        .flags = def_flags,
        .scale = 1,
        .def = cmd_def,
    };

    for (int i = 0; i < MP_CMD_MAX_ARGS; i++) {
        const struct m_option *opt = &cmd_def->args[i];
        bool is_vararg = cmd_def->vararg &&
            (i + 1 >= MP_CMD_MAX_ARGS || !cmd_def->args[i + 1].type); // last arg
        if (!opt->type && is_vararg && cmd->nargs > 0)
            opt = cmd->args[cmd->nargs - 1].type;
        if (!opt->type)
            break;

        r = pctx_read_token(ctx, &cur_token);
        if (r < 0) {
            MP_ERR(ctx, "Command %s: error in argument %d.\n", cmd->name, i + 1);
            goto error;
        }
        if (r < 1) {
            if (is_vararg)
                continue;
            // Skip optional arguments
            if (opt->defval) {
                struct mp_cmd_arg *cmdarg = &cmd->args[cmd->nargs];
                cmdarg->type = opt;
                m_option_copy(opt, &cmdarg->v, opt->defval);
                cmd->nargs++;
                continue;
            }
            MP_ERR(ctx, "Command %s: more than %d arguments required.\n",
                   cmd->name, cmd->nargs);
            goto error;
        }

        struct mp_cmd_arg *cmdarg = &cmd->args[cmd->nargs];
        cmdarg->type = opt;
        cmd->nargs++;
        r = m_option_parse(ctx->log, opt, bstr0(cmd->name), cur_token, &cmdarg->v);
        if (r < 0) {
            MP_ERR(ctx, "Command %s: argument %d can't be parsed: %s.\n",
                   cmd->name, i + 1, m_option_strerror(r));
            goto error;
        }
    }

    bstr left = pctx_get_trailing(ctx);
    if (left.len) {
        MP_ERR(ctx, "Command %s has trailing unused arguments: '%.*s'.\n",
               cmd->name, BSTR_P(left));
        // Better make it fatal to make it clear something is wrong.
        goto error;
    }

    if (!ctx->array_input) {
        bstr orig = {ctx->start.start, ctx->str.start - ctx->start.start};
        cmd->original = bstrdup(cmd, bstr_strip(orig));
    }

    return cmd;

error:
    MP_ERR(ctx, "Command was defined at %s.\n", ctx->loc);
    talloc_free(cmd);
    return NULL;
}

static struct mp_cmd *parse_cmd_str(struct mp_log *log, bstr *str, const char *loc)
{
    struct parse_ctx ctx = {
        .log = log,
        .loc = loc,
        .tmp = talloc_new(NULL),
        .str = *str,
        .start = *str,
    };
    struct mp_cmd *res = parse_cmd(&ctx, MP_ON_OSD_AUTO | MP_EXPAND_PROPERTIES);
    talloc_free(ctx.tmp);
    *str = ctx.str;
    return res;
}

mp_cmd_t *mp_input_parse_cmd_(struct mp_log *log, bstr str, const char *loc)
{
    bstr original = str;
    struct mp_cmd *cmd = parse_cmd_str(log, &str, loc);
    if (!cmd)
        return NULL;

    // Handle "multi" commands
    struct mp_cmd **p_prev = NULL;
    while (1) {
        str = bstr_lstrip(str);
        // read_token just to check whether it's trailing whitespace only
        bstr u1, u2;
        if (!bstr_eatstart0(&str, ";") || !read_token(str, &u1, &u2))
            break;
        // Multi-command. Since other input.c code uses queue_next for its
        // own purposes, a pseudo-command is used to wrap the command list.
        if (!p_prev) {
            struct mp_cmd *list = talloc_ptrtype(NULL, list);
            talloc_set_destructor(list, destroy_cmd);
            *list = (struct mp_cmd) {
                .id = MP_CMD_COMMAND_LIST,
                .name = "list",
                .original = bstrdup(list, original),
            };
            talloc_steal(list, cmd);
            list->args[0].v.p = cmd;
            p_prev = &cmd->queue_next;
            cmd = list;
        }
        struct mp_cmd *sub = parse_cmd_str(log, &str, loc);
        if (!sub) {
            talloc_free(cmd);
            return NULL;
        }
        talloc_steal(cmd, sub);
        *p_prev = sub;
        p_prev = &sub->queue_next;
    }

    return cmd;
}

struct mp_cmd *mp_input_parse_cmd_strv(struct mp_log *log, int def_flags,
                                       const char **argv, const char *location)
{
    bstr args[MP_CMD_MAX_ARGS];
    int num = 0;
    for (; argv[num]; num++) {
        if (num > MP_CMD_MAX_ARGS) {
            mp_err(log, "%s: too many arguments.\n", location);
            return NULL;
        }
        args[num] = bstr0(argv[num]);
    }
    return mp_input_parse_cmd_bstrv(log, def_flags, num, args, location);
}

struct mp_cmd *mp_input_parse_cmd_bstrv(struct mp_log *log, int def_flags,
                                        int argc, bstr *argv,
                                        const char *location)
{
    struct parse_ctx ctx = {
        .log = log,
        .loc = location,
        .tmp = talloc_new(NULL),
        .array_input = true,
        .strs = argv,
        .num_strs = argc,
    };
    struct mp_cmd *res = parse_cmd(&ctx, def_flags);
    talloc_free(ctx.tmp);
    return res;
}

void mp_cmd_free(mp_cmd_t *cmd)
{
    talloc_free(cmd);
}

mp_cmd_t *mp_cmd_clone(mp_cmd_t *cmd)
{
    mp_cmd_t *ret;
    int i;

    if (!cmd)
        return NULL;

    ret = talloc_memdup(NULL, cmd, sizeof(mp_cmd_t));
    talloc_set_destructor(ret, destroy_cmd);
    ret->name = talloc_strdup(ret, cmd->name);
    for (i = 0; i < ret->nargs; i++) {
        memset(&ret->args[i].v, 0, ret->args[i].type->type->size);
        m_option_copy(ret->args[i].type, &ret->args[i].v, &cmd->args[i].v);
    }

    if (cmd->id == MP_CMD_COMMAND_LIST) {
        struct mp_cmd *prev = NULL;
        for (struct mp_cmd *sub = cmd->args[0].v.p; sub; sub = sub->queue_next) {
            sub = mp_cmd_clone(sub);
            talloc_steal(ret, sub);
            if (prev) {
                prev->queue_next = sub;
            } else {
                ret->args[0].v.p = sub;
            }
            prev = sub;
        }
    }

    return ret;
}

static int parse_cycle_dir(struct mp_log *log, const struct m_option *opt,
                           struct bstr name, struct bstr param, void *dst)
{
    double val;
    if (bstrcmp0(param, "up") == 0) {
        val = +1;
    } else if (bstrcmp0(param, "down") == 0) {
        val = -1;
    } else {
        return m_option_type_double.parse(log, opt, name, param, dst);
    }
    *(double *)dst = val;
    return 1;
}

const struct m_option_type m_option_type_cycle_dir = {
    .name = "up|down",
    .parse = parse_cycle_dir,
    .size = sizeof(double),
};
