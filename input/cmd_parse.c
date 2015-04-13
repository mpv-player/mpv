/*
 * This file is part of mpv.
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
 */

#include <stddef.h>

#include "misc/bstr.h"
#include "common/common.h"
#include "common/msg.h"
#include "options/m_option.h"

#include "cmd_parse.h"
#include "cmd_list.h"
#include "input.h"

#include "libmpv/client.h"

static void destroy_cmd(void *ptr)
{
    struct mp_cmd *cmd = ptr;
    for (int n = 0; n < cmd->nargs; n++)
        m_option_free(cmd->args[n].type, &cmd->args[n].v);
}

struct flag {
    const char *name;
    unsigned int remove, add;
};

static const struct flag cmd_flags[] = {
    {"no-osd",              MP_ON_OSD_FLAGS, MP_ON_OSD_NO},
    {"osd-bar",             MP_ON_OSD_FLAGS, MP_ON_OSD_BAR},
    {"osd-msg",             MP_ON_OSD_FLAGS, MP_ON_OSD_MSG},
    {"osd-msg-bar",         MP_ON_OSD_FLAGS, MP_ON_OSD_MSG | MP_ON_OSD_BAR},
    {"osd-auto",            MP_ON_OSD_FLAGS, MP_ON_OSD_AUTO},
    {"expand-properties",   0,               MP_EXPAND_PROPERTIES},
    {"raw",                 MP_EXPAND_PROPERTIES, 0},
    {"repeatable",          0,               MP_ALLOW_REPEAT},
    {0}
};

static bool apply_flag(struct mp_cmd *cmd, bstr str)
{
    for (int n = 0; cmd_flags[n].name; n++) {
        if (bstr_equals0(str, cmd_flags[n].name)) {
            cmd->flags = (cmd->flags & ~cmd_flags[n].remove) | cmd_flags[n].add;
            return true;
        }
    }
    return false;
}

static bool find_cmd(struct mp_log *log, struct mp_cmd *cmd, bstr name)
{
    if (name.len == 0) {
        mp_err(log, "Command name missing.\n");
        return false;
    }
    for (int n = 0; mp_cmds[n].name; n++) {
        if (bstr_equals0(name, mp_cmds[n].name)) {
            cmd->def = &mp_cmds[n];
            cmd->name = (char *)cmd->def->name;
            cmd->id = cmd->def->id;
            return true;
        }
    }
    mp_err(log, "Command '%.*s' not found.\n", BSTR_P(name));
    return false;
}

static bool is_vararg(const struct mp_cmd_def *m, int i)
{
    return m->vararg && (i + 1 >= MP_CMD_MAX_ARGS || !m->args[i + 1].type);
}

static const struct m_option *get_arg_type(const struct mp_cmd_def *cmd, int i)
{
    if (i >= MP_CMD_MAX_ARGS)
        return NULL;
    const struct m_option *opt = &cmd->args[i];
    if (!opt->type && is_vararg(cmd, i)) {
        // The last arg in a vararg command sets all vararg types.
        for (int n = i; n >= 0; n--) {
            if (cmd->args[n].type) {
                opt = &cmd->args[n];
                break;
            }
        }
    }
    return opt->type ? opt : NULL;
}

// Set correct arg count, and fill in missing optional args.
static bool finish_cmd(struct mp_log *log, struct mp_cmd *cmd)
{
    cmd->nargs = 0;
    for (int i = 0; i < MP_CMD_MAX_ARGS; i++) {
        if (!cmd->args[i].type) {
            const struct m_option *opt = get_arg_type(cmd->def, i);
            if (!opt || is_vararg(cmd->def, i))
                break;
            if (!opt->defval && !(opt->flags & MP_CMD_OPT_ARG)) {
                mp_err(log, "Command %s: more than %d arguments required.\n",
                       cmd->name, cmd->nargs);
                return false;
            }
            cmd->args[i].type = opt;
            if (opt->defval)
                m_option_copy(opt, &cmd->args[i].v, opt->defval);
        }
        if (cmd->args[i].type)
            cmd->nargs = i + 1;
    }
    return true;
}

struct mp_cmd *mp_input_parse_cmd_node(struct mp_log *log, mpv_node *node)
{
    struct mp_cmd *cmd = talloc_ptrtype(NULL, cmd);
    talloc_set_destructor(cmd, destroy_cmd);
    *cmd = (struct mp_cmd) { .scale = 1, };

    if (node->format != MPV_FORMAT_NODE_ARRAY)
        goto error;
    mpv_node_list *args = node->u.list;
    int cur = 0;

    while (cur < args->num) {
        if (args->values[cur].format != MPV_FORMAT_STRING)
            break;
        if (!apply_flag(cmd, bstr0(args->values[cur].u.string)))
            break;
        cur++;
    }

    bstr cmd_name = {0};
    if (cur < args->num && args->values[cur].format == MPV_FORMAT_STRING)
        cmd_name = bstr0(args->values[cur++].u.string);
    if (!find_cmd(log, cmd, cmd_name))
        goto error;

    int first = cur;
    for (int i = 0; i < args->num - first; i++) {
        const struct m_option *opt = get_arg_type(cmd->def, i);
        if (!opt) {
            mp_err(log, "Command %s: has only %d arguments.\n", cmd->name, i);
            goto error;
        }
        mpv_node *val = &args->values[cur++];
        cmd->args[i].type = opt;
        void *dst = &cmd->args[i].v;
        if (val->format == MPV_FORMAT_STRING) {
            int r = m_option_parse(log, opt, bstr0(cmd->name),
                                   bstr0(val->u.string), dst);
            if (r < 0) {
                mp_err(log, "Command %s: argument %d can't be parsed: %s.\n",
                       cmd->name, i + 1, m_option_strerror(r));
                goto error;
            }
        } else {
            int r = m_option_set_node(opt, dst, val);
            if (r < 0) {
                mp_err(log, "Command %s: argument %d has incompatible type.\n",
                       cmd->name, i + 1);
                goto error;
            }
        }
    }

    if (!finish_cmd(log, cmd))
        goto error;

    return cmd;
error:
    for (int n = 0; n < MP_CMD_MAX_ARGS; n++) {
        if (cmd->args[n].type)
            m_option_free(cmd->args[n].type, &cmd->args[n].v);
    }
    talloc_free(cmd);
    return NULL;
}


static bool read_token(bstr str, bstr *out_rest, bstr *out_token)
{
    bstr t = bstr_lstrip(str);
    int next = bstrcspn(t, WHITESPACE "#;");
    if (!next)
        return false;
    *out_token = bstr_splice(t, 0, next);
    *out_rest = bstr_cut(t, next);
    return true;
}

struct parse_ctx {
    struct mp_log *log;
    void *tmp;
    bstr start, str;
};

static int pctx_read_token(struct parse_ctx *ctx, bstr *out)
{
    *out = (bstr){0};
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

static struct mp_cmd *parse_cmd_str(struct mp_log *log, void *tmp,
                                    bstr *str, const char *loc)
{
    struct parse_ctx *ctx = &(struct parse_ctx){
        .log = log,
        .tmp = tmp,
        .str = *str,
        .start = *str,
    };

    struct mp_cmd *cmd = talloc_ptrtype(NULL, cmd);
    talloc_set_destructor(cmd, destroy_cmd);
    *cmd = (struct mp_cmd) {
        .flags = MP_ON_OSD_AUTO | MP_EXPAND_PROPERTIES,
        .scale = 1,
    };

    ctx->str = bstr_lstrip(ctx->str);
    bstr old = ctx->str;
    if (mp_replace_legacy_cmd(ctx->tmp, &ctx->str)) {
        MP_WARN(ctx, "Warning: command '%.*s' is deprecated, "
                "replaced with '%.*s' at %s.\n",
                BSTR_P(old), BSTR_P(ctx->str), loc);
        ctx->start = ctx->str;
    }

    bstr cur_token;
    if (pctx_read_token(ctx, &cur_token) < 0)
        goto error;

    while (1) {
        if (!apply_flag(cmd, cur_token))
            break;
        if (pctx_read_token(ctx, &cur_token) < 0)
            goto error;
        break;
    }

    if (!find_cmd(ctx->log, cmd, cur_token))
        goto error;

    for (int i = 0; i < MP_CMD_MAX_ARGS; i++) {
        const struct m_option *opt = get_arg_type(cmd->def, i);
        if (!opt)
            break;

        int r = pctx_read_token(ctx, &cur_token);
        if (r < 0) {
            MP_ERR(ctx, "Command %s: error in argument %d.\n", cmd->name, i + 1);
            goto error;
        }
        if (r < 1)
            break;

        struct mp_cmd_arg *cmdarg = &cmd->args[i];
        cmdarg->type = opt;
        r = m_option_parse(ctx->log, opt, bstr0(cmd->name), cur_token, &cmdarg->v);
        if (r < 0) {
            MP_ERR(ctx, "Command %s: argument %d can't be parsed: %s.\n",
                   cmd->name, i + 1, m_option_strerror(r));
            goto error;
        }
    }

    if (!finish_cmd(ctx->log, cmd))
        goto error;

    bstr dummy;
    if (read_token(ctx->str, &dummy, &dummy) && ctx->str.len) {
        MP_ERR(ctx, "Command %s has trailing unused arguments: '%.*s'.\n",
               cmd->name, BSTR_P(ctx->str));
        // Better make it fatal to make it clear something is wrong.
        goto error;
    }

    bstr orig = {ctx->start.start, ctx->str.start - ctx->start.start};
    cmd->original = bstrdup(cmd, bstr_strip(orig));

    *str = ctx->str;
    return cmd;

error:
    MP_ERR(ctx, "Command was defined at %s.\n", loc);
    talloc_free(cmd);
    *str = ctx->str;
    return NULL;
}

static struct mp_cmd_def list_def = {
    .id = MP_CMD_COMMAND_LIST,
    .name = "list",
};

mp_cmd_t *mp_input_parse_cmd_(struct mp_log *log, bstr str, const char *loc)
{
    void *tmp = talloc_new(NULL);
    bstr original = str;
    struct mp_cmd *cmd = parse_cmd_str(log, tmp, &str, loc);
    if (!cmd)
        goto done;

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
                .id = list_def.id,
                .name = (char *)list_def.name,
                .def = &list_def,
                .original = bstrdup(list, original),
            };
            talloc_steal(list, cmd);
            list->args[0].v.p = cmd;
            p_prev = &cmd->queue_next;
            cmd = list;
        }
        struct mp_cmd *sub = parse_cmd_str(log, tmp, &str, loc);
        if (!sub) {
            talloc_free(cmd);
            cmd = NULL;
            goto done;
        }
        talloc_steal(cmd, sub);
        *p_prev = sub;
        p_prev = &sub->queue_next;
    }

done:
    talloc_free(tmp);
    return cmd;
}

struct mp_cmd *mp_input_parse_cmd_strv(struct mp_log *log, const char **argv)
{
    mpv_node items[MP_CMD_MAX_ARGS];
    mpv_node_list list = {.values = items};
    mpv_node node = {.format = MPV_FORMAT_NODE_ARRAY, .u = {.list = &list}};
    while (argv[list.num]) {
        if (list.num >= MP_CMD_MAX_ARGS) {
            mp_err(log, "Too many arguments to command.\n");
            return NULL;
        }
        char *s = (char *)argv[list.num];
        items[list.num++] = (mpv_node){.format = MPV_FORMAT_STRING,
                                       .u = {.string = s}};
    }
    return mp_input_parse_cmd_node(log, &node);
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
    ret->original = bstrdup(ret, cmd->original);

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

void mp_cmd_dump(struct mp_log *log, int msgl, char *header, struct mp_cmd *cmd)
{
    if (!mp_msg_test(log, msgl))
        return;
    if (header)
        mp_msg(log, msgl, "%s ", header);
    if (!cmd) {
        mp_msg(log, msgl, "(NULL)\n");
        return;
    }
    mp_msg(log, msgl, "%s, flags=%d, args=[", cmd->name, cmd->flags);
    for (int n = 0; n < cmd->nargs; n++) {
        char *s = m_option_print(cmd->args[n].type, &cmd->args[n].v);
        if (n)
            mp_msg(log, msgl, ", ");
        mp_msg(log, msgl, "%s", s ? s : "(NULL)");
        talloc_free(s);
    }
    mp_msg(log, msgl, "]\n");
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
