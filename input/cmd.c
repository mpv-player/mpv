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

#include <stddef.h>

#include "misc/bstr.h"
#include "misc/node.h"
#include "common/common.h"
#include "common/msg.h"
#include "options/m_option.h"

#include "cmd.h"
#include "input.h"
#include "misc/json.h"

#include "libmpv/client.h"

static void destroy_cmd(void *ptr)
{
    struct mp_cmd *cmd = ptr;
    for (int n = 0; n < cmd->nargs; n++) {
        if (cmd->args[n].type)
            m_option_free(cmd->args[n].type, &cmd->args[n].v);
    }
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
    {"async",               MP_SYNC_CMD,     MP_ASYNC_CMD},
    {"sync",                MP_ASYNC_CMD,     MP_SYNC_CMD},
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

    char nname[80];
    snprintf(nname, sizeof(nname), "%.*s", BSTR_P(name));
    for (int n = 0; nname[n]; n++) {
        if (nname[n] == '_')
            nname[n] = '-';
    }

    for (int n = 0; mp_cmds[n].name; n++) {
        if (strcmp(nname, mp_cmds[n].name) == 0) {
            cmd->def = &mp_cmds[n];
            cmd->name = (char *)cmd->def->name;
            return true;
        }
    }
    mp_err(log, "Command '%.*s' not found.\n", BSTR_P(name));
    return false;
}

static bool is_vararg(const struct mp_cmd_def *m, int i)
{
    return m->vararg && (i + 1 >= MP_CMD_DEF_MAX_ARGS || !m->args[i + 1].type);
}

static const struct m_option *get_arg_type(const struct mp_cmd_def *cmd, int i)
{
    const struct m_option *opt = NULL;
    if (is_vararg(cmd, i)) {
        // The last arg in a vararg command sets all vararg types.
        for (int n = MPMIN(i, MP_CMD_DEF_MAX_ARGS - 1); n >= 0; n--) {
            if (cmd->args[n].type) {
                opt = &cmd->args[n];
                break;
            }
        }
    } else if (i < MP_CMD_DEF_MAX_ARGS) {
        opt = &cmd->args[i];
    }
    return opt && opt->type ? opt : NULL;
}

// Return the name of the argument, possibly as stack allocated string (which is
// why this is a macro, and out of laziness). Otherwise as get_arg_type().
#define get_arg_name(cmd, i)                                    \
    ((i) < MP_CMD_DEF_MAX_ARGS && (cmd)->args[(i)].name &&      \
     (cmd)->args[(i)].name[0]                                   \
     ? (cmd)->args[(i)].name : mp_tprintf(10, "%d", (i) + 1))

// Verify that there are no missing args, fill in missing optional args.
static bool finish_cmd(struct mp_log *log, struct mp_cmd *cmd)
{
    for (int i = 0; i < MP_CMD_DEF_MAX_ARGS; i++) {
        // (type==NULL is used for yet unset arguments)
        if (i < cmd->nargs && cmd->args[i].type)
            continue;
        const struct m_option *opt = get_arg_type(cmd->def, i);
        if (i >= cmd->nargs && (!opt || is_vararg(cmd->def, i)))
            break;
        if (!opt->defval && !(opt->flags & MP_CMD_OPT_ARG)) {
            mp_err(log, "Command %s: required argument %s not set.\n",
                   cmd->name, get_arg_name(cmd->def, i));
            return false;
        }
        struct mp_cmd_arg arg = {.type = opt};
        if (opt->defval)
            m_option_copy(opt, &arg.v, opt->defval);
        assert(i <= cmd->nargs);
        if (i == cmd->nargs) {
            MP_TARRAY_APPEND(cmd, cmd->args, cmd->nargs, arg);
        } else {
            cmd->args[i] = arg;
        }
    }

    if (!(cmd->flags & (MP_ASYNC_CMD | MP_SYNC_CMD)))
        cmd->flags |= cmd->def->default_async ? MP_ASYNC_CMD : MP_SYNC_CMD;

    return true;
}

static bool set_node_arg(struct mp_log *log, struct mp_cmd *cmd, int i,
                         mpv_node *val)
{
    const char *name = get_arg_name(cmd->def, i);

    const struct m_option *opt = get_arg_type(cmd->def, i);
    if (!opt) {
        mp_err(log, "Command %s: has only %d arguments.\n", cmd->name, i);
        return false;
    }

    if (i < cmd->nargs && cmd->args[i].type) {
        mp_err(log, "Command %s: argument %s was already set.\n", cmd->name, name);
        return false;
    }

    struct mp_cmd_arg arg = {.type = opt};
    void *dst = &arg.v;
    if (val->format == MPV_FORMAT_STRING) {
        int r = m_option_parse(log, opt, bstr0(cmd->name),
                                bstr0(val->u.string), dst);
        if (r < 0) {
            mp_err(log, "Command %s: argument %s can't be parsed: %s.\n",
                   cmd->name, name, m_option_strerror(r));
            return false;
        }
    } else {
        int r = m_option_set_node(opt, dst, val);
        if (r < 0) {
            mp_err(log, "Command %s: argument %s has incompatible type.\n",
                   cmd->name, name);
            return false;
        }
    }

    // (leave unset arguments blank, to be set later or checked by finish_cmd())
    while (i >= cmd->nargs) {
        struct mp_cmd_arg t = {0};
        MP_TARRAY_APPEND(cmd, cmd->args, cmd->nargs, t);
    }

    cmd->args[i] = arg;
    return true;
}

static bool cmd_node_array(struct mp_log *log, struct mp_cmd *cmd, mpv_node *node)
{
    assert(node->format == MPV_FORMAT_NODE_ARRAY);
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
        return false;

    int first = cur;
    for (int i = 0; i < args->num - first; i++) {
        if (!set_node_arg(log, cmd, cmd->nargs, &args->values[cur++]))
            return false;
    }

    return true;
}

static bool cmd_node_map(struct mp_log *log, struct mp_cmd *cmd, mpv_node *node)
{
    assert(node->format == MPV_FORMAT_NODE_MAP);
    mpv_node_list *args = node->u.list;

    mpv_node *name = node_map_get(node, "name");
    if (!name || name->format != MPV_FORMAT_STRING)
        return false;

    if (!find_cmd(log, cmd, bstr0(name->u.string)))
        return false;

    if (cmd->def->vararg) {
        mp_err(log, "Command %s: this command uses a variable number of "
               "arguments, which does not work with named arguments.\n",
               cmd->name);
        return false;
    }

    for (int n = 0; n < args->num; n++) {
        const char *key = args->keys[n];
        mpv_node *val = &args->values[n];

        if (strcmp(key, "name") == 0) {
            // already handled above
        } else if (strcmp(key, "_flags") == 0) {
            if (val->format != MPV_FORMAT_NODE_ARRAY)
                return false;
            mpv_node_list *flags = val->u.list;
            for (int i = 0; i < flags->num; i++) {
                if (flags->values[i].format != MPV_FORMAT_STRING)
                    return false;
                if (!apply_flag(cmd, bstr0(flags->values[i].u.string)))
                    return false;
            }
        } else {
            int arg = -1;

            for (int i = 0; i < MP_CMD_DEF_MAX_ARGS; i++) {
                const char *arg_name = cmd->def->args[i].name;
                if (arg_name && arg_name[0] && strcmp(key, arg_name) == 0) {
                    arg = i;
                    break;
                }
            }

            if (arg < 0) {
                mp_err(log, "Command %s: no argument %s.\n", cmd->name, key);
                return false;
            }

            if (!set_node_arg(log, cmd, arg, val))
                return false;
        }
    }

    return true;
}

struct mp_cmd *mp_input_parse_cmd_node(struct mp_log *log, mpv_node *node)
{
    struct mp_cmd *cmd = talloc_ptrtype(NULL, cmd);
    talloc_set_destructor(cmd, destroy_cmd);
    *cmd = (struct mp_cmd) { .scale = 1, .scale_units = 1 };

    bool res = false;
    if (node->format == MPV_FORMAT_NODE_ARRAY) {
        res = cmd_node_array(log, cmd, node);
    } else if (node->format == MPV_FORMAT_NODE_MAP) {
        res = cmd_node_map(log, cmd, node);
    }

    res = res && finish_cmd(log, cmd);

    if (!res)
        TA_FREEP(&cmd);

    return cmd;
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
            MP_ERR(ctx, "Unterminated double quote: ...>%.*s<.\n", BSTR_P(start));
            return -1;
        }
        return 1;
    }
    if (bstr_eatstart0(&ctx->str, "'")) {
        int next = bstrchr(ctx->str, '\'');
        if (next < 0) {
            MP_ERR(ctx, "Unterminated single quote: ...>%.*s<.\n", BSTR_P(start));
            return -1;
        }
        *out = bstr_splice(ctx->str, 0, next);
        ctx->str = bstr_cut(ctx->str, next+1);
        return 1;
    }
    if (ctx->start.len > 1 && bstr_eatstart0(&ctx->str, "`")) {
        char endquote[2] = {ctx->str.start[0], '`'};
        ctx->str = bstr_cut(ctx->str, 1);
        int next = bstr_find(ctx->str, (bstr){endquote, 2});
        if (next < 0) {
            MP_ERR(ctx, "Unterminated custom quote: ...>%.*s<.\n", BSTR_P(start));
            return -1;
        }
        *out = bstr_splice(ctx->str, 0, next);
        ctx->str = bstr_cut(ctx->str, next+2);
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
        .scale_units = 1,
    };

    ctx->str = bstr_lstrip(ctx->str);

    bstr cur_token;
    if (pctx_read_token(ctx, &cur_token) < 0)
        goto error;

    while (1) {
        if (!apply_flag(cmd, cur_token))
            break;
        if (pctx_read_token(ctx, &cur_token) < 0)
            goto error;
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

        struct mp_cmd_arg arg = {.type = opt};
        r = m_option_parse(ctx->log, opt, bstr0(cmd->name), cur_token, &arg.v);
        if (r < 0) {
            MP_ERR(ctx, "Command %s: argument %d can't be parsed: %s.\n",
                   cmd->name, i + 1, m_option_strerror(r));
            goto error;
        }

        MP_TARRAY_APPEND(cmd, cmd->args, cmd->nargs, arg);
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
    cmd->original = bstrto0(cmd, bstr_strip(orig));

    *str = ctx->str;
    return cmd;

error:
    MP_ERR(ctx, "Command was defined at %s.\n", loc);
    talloc_free(cmd);
    *str = ctx->str;
    return NULL;
}

mp_cmd_t *mp_input_parse_cmd_str(struct mp_log *log, bstr str, const char *loc)
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
                .name = (char *)mp_cmd_list.name,
                .def = &mp_cmd_list,
            };
            talloc_steal(list, cmd);
            struct mp_cmd_arg arg = {0};
            arg.v.p = cmd;
            list->args = talloc_dup(list, &arg);
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

    cmd->original = bstrto0(cmd, bstr_strip(
                        bstr_splice(original, 0, str.start - original.start)));

    str = bstr_strip(str);
    if (bstr_eatstart0(&str, "#") && !bstr_startswith0(str, "#")) {
        str = bstr_strip(str);
        if (str.len)
            cmd->desc = bstrto0(cmd, str);
    }

done:
    talloc_free(tmp);
    return cmd;
}

struct mp_cmd *mp_input_parse_cmd_strv(struct mp_log *log, const char **argv)
{
    int count = 0;
    while (argv[count])
        count++;
    mpv_node *items = talloc_zero_array(NULL, mpv_node, count);
    mpv_node_list list = {.values = items, .num = count};
    mpv_node node = {.format = MPV_FORMAT_NODE_ARRAY, .u = {.list = &list}};
    for (int n = 0; n < count; n++) {
        items[n] = (mpv_node){.format = MPV_FORMAT_STRING,
                              .u = {.string = (char *)argv[n]}};
    }
    struct mp_cmd *res = mp_input_parse_cmd_node(log, &node);
    talloc_free(items);
    return res;
}

void mp_cmd_free(mp_cmd_t *cmd)
{
    talloc_free(cmd);
}

mp_cmd_t *mp_cmd_clone(mp_cmd_t *cmd)
{
    if (!cmd)
        return NULL;

    mp_cmd_t *ret = talloc_dup(NULL, cmd);
    talloc_set_destructor(ret, destroy_cmd);
    ret->name = talloc_strdup(ret, cmd->name);
    ret->args = talloc_zero_array(ret, struct mp_cmd_arg, ret->nargs);
    for (int i = 0; i < ret->nargs; i++) {
        ret->args[i].type = cmd->args[i].type;
        m_option_copy(ret->args[i].type, &ret->args[i].v, &cmd->args[i].v);
    }
    ret->original = talloc_strdup(ret, cmd->original);
    ret->desc = talloc_strdup(ret, cmd->desc);
    ret->sender = NULL;
    ret->key_name = talloc_strdup(ret, ret->key_name);
    ret->key_text = talloc_strdup(ret, ret->key_text);

    if (cmd->def == &mp_cmd_list) {
        struct mp_cmd *prev = NULL;
        for (struct mp_cmd *sub = cmd->args[0].v.p; sub; sub = sub->queue_next) {
            sub = mp_cmd_clone(sub);
            talloc_steal(ret, sub);
            if (prev) {
                prev->queue_next = sub;
            } else {
                struct mp_cmd_arg arg = {0};
                arg.v.p = sub;
                ret->args = talloc_dup(ret, &arg);
            }
            prev = sub;
        }
    }

    return ret;
}

static int get_arg_count(const struct mp_cmd_def *cmd)
{
    for (int i = MP_CMD_DEF_MAX_ARGS - 1; i >= 0; i--) {
        if (cmd->args[i].type)
            return i + 1;
    }
    return 0;
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
    int argc = get_arg_count(cmd->def);
    for (int n = 0; n < cmd->nargs; n++) {
        const char *argname = cmd->def->args[MPMIN(n, argc - 1)].name;
        char *s = m_option_print(cmd->args[n].type, &cmd->args[n].v);
        if (n)
            mp_msg(log, msgl, ", ");
        struct mpv_node node = {
            .format = MPV_FORMAT_STRING,
            .u.string = s ? s : "(NULL)",
        };
        char *esc = NULL;
        json_write(&esc, &node);
        mp_msg(log, msgl, "%s=%s", argname, esc ? esc : "<error>");
        talloc_free(esc);
        talloc_free(s);
    }
    mp_msg(log, msgl, "]\n");
}

bool mp_input_is_repeatable_cmd(struct mp_cmd *cmd)
{
    if (cmd->def == &mp_cmd_list && cmd->args[0].v.p)
        cmd = cmd->args[0].v.p;  // list - only 1st cmd is considered

    return (cmd->def->allow_auto_repeat) || (cmd->flags & MP_ALLOW_REPEAT);
}

bool mp_input_is_scalable_cmd(struct mp_cmd *cmd)
{
    return cmd->def->scalable;
}

void mp_print_cmd_list(struct mp_log *out)
{
    for (int i = 0; mp_cmds[i].name; i++) {
        const struct mp_cmd_def *def = &mp_cmds[i];
        mp_info(out, "%-20.20s", def->name);
        for (int j = 0; j < MP_CMD_DEF_MAX_ARGS && def->args[j].type; j++) {
            const struct m_option *arg = &def->args[j];
            bool is_opt = arg->defval || (arg->flags & MP_CMD_OPT_ARG);
            mp_info(out, " %s%s=%s%s", is_opt ? "[" : "", arg->name,
                    arg->type->name, is_opt ? "]" : "");
        }
        if (def->vararg)
            mp_info(out, "..."); // essentially append to last argument
        mp_info(out, "\n");
    }
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

static char *print_cycle_dir(const m_option_t *opt, const void *val)
{
    return talloc_asprintf(NULL, "%f", *(double *)val);
}

static void copy_opt(const m_option_t *opt, void *dst, const void *src)
{
    if (dst && src)
        memcpy(dst, src, opt->type->size);
}

const struct m_option_type m_option_type_cycle_dir = {
    .name = "up|down",
    .parse = parse_cycle_dir,
    .print = print_cycle_dir,
    .copy = copy_opt,
    .size = sizeof(double),
};
