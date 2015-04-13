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

/// \file
/// \ingroup Properties

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include <libavutil/common.h>

#include "libmpv/client.h"

#include "talloc.h"
#include "m_option.h"
#include "m_property.h"
#include "common/msg.h"
#include "common/common.h"

struct legacy_prop {
    const char *old, *new;
};
static const struct legacy_prop legacy_props[] = {
    {"switch_video",    "video"},
    {"switch_audio",    "audio"},
    {"switch_program",  "program"},
    {"framedropping",   "framedrop"},
    {"osdlevel",        "osd-level"},
    {0}
};

static bool translate_legacy_property(struct mp_log *log, const char *name,
                                      char *buffer, size_t buffer_size)
{
    if (strlen(name) + 1 > buffer_size)
        return false;

    const char *old_name = name;

    for (int n = 0; legacy_props[n].new; n++) {
        if (strcmp(name, legacy_props[n].old) == 0) {
            name = legacy_props[n].new;
            break;
        }
    }

    snprintf(buffer, buffer_size, "%s", name);

    // Old names used "_" instead of "-"
    for (int n = 0; buffer[n]; n++) {
        if (buffer[n] == '_')
            buffer[n] = '-';
    }

    if (log && strcmp(old_name, buffer) != 0) {
        mp_warn(log, "Warning: property '%s' is deprecated, replaced with '%s'."
                " Fix your input.conf!\n", old_name, buffer);
    }

    return true;
}

static struct m_property *m_property_list_find(const struct m_property *list,
                                                     const char *name)
{
    for (int n = 0; list && list[n].name; n++) {
        if (strcmp(list[n].name, name) == 0)
            return (struct m_property *)&list[n];
    }
    return NULL;
}

static int do_action(const struct m_property *prop_list, const char *name,
                     int action, void *arg, void *ctx)
{
    const char *sep;
    struct m_property *prop;
    struct m_property_action_arg ka;
    if ((sep = strchr(name, '/')) && sep[1]) {
        int len = sep - name;
        char base[len + 1];
        memcpy(base, name, len);
        base[len] = 0;
        prop = m_property_list_find(prop_list, base);
        ka = (struct m_property_action_arg) {
            .key = sep + 1,
            .action = action,
            .arg = arg,
        };
        action = M_PROPERTY_KEY_ACTION;
        arg = &ka;
    } else
        prop = m_property_list_find(prop_list, name);
    if (!prop)
        return M_PROPERTY_UNKNOWN;
    return prop->call(ctx, prop, action, arg);
}

// (as a hack, log can be NULL on read-only paths)
int m_property_do(struct mp_log *log, const struct m_property *prop_list,
                  const char *in_name, int action, void *arg, void *ctx)
{
    union m_option_value val = {0};
    int r;

    char name[64];
    if (!translate_legacy_property(log, in_name, name, sizeof(name)))
        return M_PROPERTY_UNKNOWN;

    struct m_option opt = {0};
    r = do_action(prop_list, name, M_PROPERTY_GET_TYPE, &opt, ctx);
    if (r <= 0)
        return r;
    assert(opt.type);

    switch (action) {
    case M_PROPERTY_PRINT: {
        if ((r = do_action(prop_list, name, M_PROPERTY_PRINT, arg, ctx)) >= 0)
            return r;
        // Fallback to m_option
        if ((r = do_action(prop_list, name, M_PROPERTY_GET, &val, ctx)) <= 0)
            return r;
        char *str = m_option_pretty_print(&opt, &val);
        m_option_free(&opt, &val);
        *(char **)arg = str;
        return str != NULL;
    }
    case M_PROPERTY_GET_STRING: {
        if ((r = do_action(prop_list, name, M_PROPERTY_GET, &val, ctx)) <= 0)
            return r;
        char *str = m_option_print(&opt, &val);
        m_option_free(&opt, &val);
        *(char **)arg = str;
        return str != NULL;
    }
    case M_PROPERTY_SET_STRING: {
        if (!log)
            return M_PROPERTY_ERROR;
        bstr optname = bstr0(name), a, b;
        if (bstr_split_tok(optname, "/", &a, &b))
            optname = b;
        if (m_option_parse(log, &opt, optname, bstr0(arg), &val) < 0)
            return M_PROPERTY_ERROR;
        r = do_action(prop_list, name, M_PROPERTY_SET, &val, ctx);
        m_option_free(&opt, &val);
        return r;
    }
    case M_PROPERTY_SWITCH: {
        if (!log)
            return M_PROPERTY_ERROR;
        struct m_property_switch_arg *sarg = arg;
        if ((r = do_action(prop_list, name, M_PROPERTY_SWITCH, arg, ctx)) !=
            M_PROPERTY_NOT_IMPLEMENTED)
            return r;
        // Fallback to m_option
        if (!opt.type->add)
            return M_PROPERTY_NOT_IMPLEMENTED;
        if ((r = do_action(prop_list, name, M_PROPERTY_GET, &val, ctx)) <= 0)
            return r;
        opt.type->add(&opt, &val, sarg->inc, sarg->wrap);
        r = do_action(prop_list, name, M_PROPERTY_SET, &val, ctx);
        m_option_free(&opt, &val);
        return r;
    }
    case M_PROPERTY_SET: {
        if (!log)
            return M_PROPERTY_ERROR;
        m_option_copy(&opt, &val, arg);
        r = opt.type->clamp ? opt.type->clamp(&opt, arg) : 0;
        m_option_free(&opt, &val);
        if (r != 0) {
            mp_err(log, "Property '%s': invalid value.\n", name);
            return M_PROPERTY_ERROR;
        }
        return do_action(prop_list, name, M_PROPERTY_SET, arg, ctx);
    }
    case M_PROPERTY_GET_NODE: {
        if ((r = do_action(prop_list, name, M_PROPERTY_GET_NODE, arg, ctx)) !=
            M_PROPERTY_NOT_IMPLEMENTED)
            return r;
        if ((r = do_action(prop_list, name, M_PROPERTY_GET, &val, ctx)) <= 0)
            return r;
        struct mpv_node *node = arg;
        int err = m_option_get_node(&opt, NULL, node, &val);
        if (err == M_OPT_UNKNOWN) {
            r = M_PROPERTY_NOT_IMPLEMENTED;
        } else if (err < 0) {
            r = M_PROPERTY_INVALID_FORMAT;
        } else {
            r = M_PROPERTY_OK;
        }
        m_option_free(&opt, &val);
        return r;
    }
    case M_PROPERTY_SET_NODE: {
        if ((r = do_action(prop_list, name, M_PROPERTY_SET_NODE, arg, ctx)) !=
            M_PROPERTY_NOT_IMPLEMENTED)
            return r;
        struct mpv_node *node = arg;
        int err = m_option_set_node(&opt, &val, node);
        if (err == M_OPT_UNKNOWN) {
            r = M_PROPERTY_NOT_IMPLEMENTED;
        } else if (err < 0) {
            r = M_PROPERTY_INVALID_FORMAT;
        } else {
            r = do_action(prop_list, name, M_PROPERTY_SET, &val, ctx);
        }
        m_option_free(&opt, &val);
        return r;
    }
    default:
        return do_action(prop_list, name, action, arg, ctx);
    }
}

bool m_property_split_path(const char *path, bstr *prefix, char **rem)
{
    char *next = strchr(path, '/');
    if (next) {
        *prefix = bstr_splice(bstr0(path), 0, next - path);
        *rem = next + 1;
        return true;
    } else {
        *prefix = bstr0(path);
        *rem = "";
        return false;
    }
}

// If *action is M_PROPERTY_KEY_ACTION, but the associated path is "", then
// make this into a top-level action.
static void m_property_unkey(int *action, void **arg)
{
    if (*action == M_PROPERTY_KEY_ACTION) {
        struct m_property_action_arg *ka = *arg;
        if (!ka->key[0]) {
            *action = ka->action;
            *arg = ka->arg;
        }
    }
}

static int m_property_do_bstr(const struct m_property *prop_list, bstr name,
                              int action, void *arg, void *ctx)
{
    char name0[64];
    if (name.len >= sizeof(name0))
        return M_PROPERTY_UNKNOWN;
    snprintf(name0, sizeof(name0), "%.*s", BSTR_P(name));
    return m_property_do(NULL, prop_list, name0, action, arg, ctx);
}

static void append_str(char **s, int *len, bstr append)
{
    MP_TARRAY_GROW(NULL, *s, *len + append.len);
    if (append.len)
        memcpy(*s + *len, append.start, append.len);
    *len = *len + append.len;
}

static int expand_property(const struct m_property *prop_list, char **ret,
                           int *ret_len, bstr prop, bool silent_error, void *ctx)
{
    bool cond_yes = bstr_eatstart0(&prop, "?");
    bool cond_no = !cond_yes && bstr_eatstart0(&prop, "!");
    bool test = cond_yes || cond_no;
    bool raw = bstr_eatstart0(&prop, "=");
    bstr comp_with = {0};
    bool comp = test && bstr_split_tok(prop, "==", &prop, &comp_with);
    if (test && !comp)
        raw = true;
    int method = raw ? M_PROPERTY_GET_STRING : M_PROPERTY_PRINT;

    char *s = NULL;
    int r = m_property_do_bstr(prop_list, prop, method, &s, ctx);
    bool skip;
    if (comp) {
        skip = ((s && bstr_equals0(comp_with, s)) != cond_yes);
    } else if (test) {
        skip = (!!s != cond_yes);
    } else {
        skip = !!s;
        char *append = s;
        if (!s && !silent_error && !raw)
            append = (r == M_PROPERTY_UNAVAILABLE) ? "(unavailable)" : "(error)";
        append_str(ret, ret_len, bstr0(append));
    }
    talloc_free(s);
    return skip;
}

char *m_properties_expand_string(const struct m_property *prop_list,
                                 const char *str0, void *ctx)
{
    char *ret = NULL;
    int ret_len = 0;
    bool skip = false;
    int level = 0, skip_level = 0;
    bstr str = bstr0(str0);

    while (str.len) {
        if (level > 0 && bstr_eatstart0(&str, "}")) {
            if (skip && level <= skip_level)
                skip = false;
            level--;
        } else if (bstr_startswith0(str, "${") && bstr_find0(str, "}") >= 0) {
            str = bstr_cut(str, 2);
            level++;

            // Assume ":" and "}" can't be part of the property name
            // => if ":" comes before "}", it must be for the fallback
            int term_pos = bstrcspn(str, ":}");
            bstr name = bstr_splice(str, 0, term_pos < 0 ? str.len : term_pos);
            str = bstr_cut(str, term_pos);
            bool have_fallback = bstr_eatstart0(&str, ":");

            if (!skip) {
                skip = expand_property(prop_list, &ret, &ret_len, name,
                                       have_fallback, ctx);
                if (skip)
                    skip_level = level;
            }
        } else if (level == 0 && bstr_eatstart0(&str, "$>")) {
            append_str(&ret, &ret_len, str);
            break;
        } else {
            char c;

            // Other combinations, e.g. "$x", are added verbatim
            if (bstr_eatstart0(&str, "$$")) {
                c = '$';
            } else if (bstr_eatstart0(&str, "$}")) {
                c = '}';
            } else {
                c = str.start[0];
                str = bstr_cut(str, 1);
            }

            if (!skip)
                MP_TARRAY_APPEND(NULL, ret, ret_len, c);
        }
    }

    MP_TARRAY_APPEND(NULL, ret, ret_len, '\0');
    return ret;
}

void m_properties_print_help_list(struct mp_log *log,
                                  const struct m_property *list)
{
    int count = 0;

    mp_info(log, "Name\n\n");
    for (int i = 0; list[i].name; i++) {
        const struct m_property *p = &list[i];
        mp_info(log, " %s\n", p->name);
        count++;
    }
    mp_info(log, "\nTotal: %d properties\n", count);
}

int m_property_flag_ro(int action, void* arg, int var)
{
    switch (action) {
    case M_PROPERTY_GET:
        *(int *)arg = !!var;
        return M_PROPERTY_OK;
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_FLAG};
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

int m_property_int_ro(int action, void *arg, int var)
{
    switch (action) {
    case M_PROPERTY_GET:
        *(int *)arg = var;
        return M_PROPERTY_OK;
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_INT};
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

int m_property_int64_ro(int action, void* arg, int64_t var)
{
    switch (action) {
    case M_PROPERTY_GET:
        *(int64_t *)arg = var;
        return M_PROPERTY_OK;
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_INT64};
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

int m_property_float_ro(int action, void *arg, float var)
{
    switch (action) {
    case M_PROPERTY_GET:
        *(float *)arg = var;
        return M_PROPERTY_OK;
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_FLOAT};
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

int m_property_double_ro(int action, void *arg, double var)
{
    switch (action) {
    case M_PROPERTY_GET:
        *(double *)arg = var;
        return M_PROPERTY_OK;
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_DOUBLE};
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

int m_property_strdup_ro(int action, void* arg, const char *var)
{
    if (!var)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_GET:
        *(char **)arg = talloc_strdup(NULL, var);
        return M_PROPERTY_OK;
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_STRING};
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

// This allows you to make a list of values (like from a struct) available
// as a number of sub-properties. The property list is set up with the current
// property values on the stack before calling this function.
// This does not support write access.
int m_property_read_sub(const struct m_sub_property *props, int action, void *arg)
{
    m_property_unkey(&action, &arg);
    switch (action) {
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_NODE};
        return M_PROPERTY_OK;
    case M_PROPERTY_GET: {
        struct mpv_node node;
        node.format = MPV_FORMAT_NODE_MAP;
        node.u.list = talloc_zero(NULL, mpv_node_list);
        mpv_node_list *list = node.u.list;
        for (int n = 0; props && props[n].name; n++) {
            const struct m_sub_property *prop = &props[n];
            if (prop->unavailable)
                continue;
            MP_TARRAY_GROW(list, list->values, list->num);
            MP_TARRAY_GROW(list, list->keys, list->num);
            mpv_node *val = &list->values[list->num];
            if (m_option_get_node(&prop->type, list, val, (void*)&prop->value) < 0)
            {
                char *s = m_option_print(&prop->type, &prop->value);
                val->format = MPV_FORMAT_STRING;
                val->u.string = talloc_steal(list, s);
            }
            list->keys[list->num] = (char *)prop->name;
            list->num++;
        }
        *(struct mpv_node *)arg = node;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_PRINT: {
        // Output "something" - what it really should return is not yet decided.
        // It should probably be something that is easy to consume by slave
        // mode clients. (M_PROPERTY_PRINT on the other hand can return this
        // as human readable version just fine).
        char *res = NULL;
        for (int n = 0; props && props[n].name; n++) {
            const struct m_sub_property *prop = &props[n];
            if (prop->unavailable)
                continue;
            char *s = m_option_print(&prop->type, &prop->value);
            ta_xasprintf_append(&res, "%s=%s\n", prop->name, s);
            talloc_free(s);
        }
        *(char **)arg = res;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_KEY_ACTION: {
        struct m_property_action_arg *ka = arg;
        const struct m_sub_property *prop = NULL;
        for (int n = 0; props && props[n].name; n++) {
            if (strcmp(props[n].name, ka->key) == 0) {
                prop = &props[n];
                break;
            }
        }
        if (!prop)
            return M_PROPERTY_UNKNOWN;
        if (prop->unavailable)
            return M_PROPERTY_UNAVAILABLE;
        switch (ka->action) {
        case M_PROPERTY_GET: {
            memset(ka->arg, 0, prop->type.type->size);
            m_option_copy(&prop->type, ka->arg, &prop->value);
            return M_PROPERTY_OK;
        }
        case M_PROPERTY_GET_TYPE:
            *(struct m_option *)ka->arg = prop->type;
            return M_PROPERTY_OK;
        }
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}


// Make a list of items available as indexed sub-properties. E.g. you can access
// item 0 as "property/0", item 1 as "property/1", etc., where each of these
// properties is redirected to the get_item(0, ...), get_item(1, ...), callback.
// Additionally, the number of entries is made available as "property/count".
// action, arg: property access.
// count: number of items.
// get_item: callback to access a single item.
// ctx: userdata passed to get_item.
int m_property_read_list(int action, void *arg, int count,
                         m_get_item_cb get_item, void *ctx)
{
    m_property_unkey(&action, &arg);
    switch (action) {
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_NODE};
        return M_PROPERTY_OK;
    case M_PROPERTY_GET: {
        struct mpv_node node;
        node.format = MPV_FORMAT_NODE_ARRAY;
        node.u.list = talloc_zero(NULL, mpv_node_list);
        node.u.list->num = count;
        node.u.list->values = talloc_array(node.u.list, mpv_node, count);
        for (int n = 0; n < count; n++) {
            struct mpv_node *sub = &node.u.list->values[n];
            sub->format = MPV_FORMAT_NONE;
            int r;
            r = get_item(n, M_PROPERTY_GET_NODE, sub, ctx);
            if (r == M_PROPERTY_NOT_IMPLEMENTED) {
                struct m_option opt = {0};
                r = get_item(n, M_PROPERTY_GET_TYPE, &opt, ctx);
                if (r != M_PROPERTY_OK)
                    goto err;
                union m_option_value val = {0};
                r = get_item(n, M_PROPERTY_GET, &val, ctx);
                if (r != M_PROPERTY_OK)
                    goto err;
                m_option_get_node(&opt, node.u.list, sub, &val);
                m_option_free(&opt, &val);
            err: ;
            }
        }
        *(struct mpv_node *)arg = node;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_PRINT: {
        // See m_property_read_sub() remarks.
        char *res = NULL;
        for (int n = 0; n < count; n++) {
            char *s = NULL;
            int r = get_item(n, M_PROPERTY_PRINT, &s, ctx);
            if (r != M_PROPERTY_OK) {
                talloc_free(res);
                return r;
            }
            ta_xasprintf_append(&res, "%d: %s\n", n, s);
            talloc_free(s);
        }
        *(char **)arg = res;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_KEY_ACTION: {
        struct m_property_action_arg *ka = arg;
        if (strcmp(ka->key, "count") == 0) {
            switch (ka->action) {
            case M_PROPERTY_GET_TYPE: {
                struct m_option opt = {.type = CONF_TYPE_INT};
                *(struct m_option *)ka->arg = opt;
                return M_PROPERTY_OK;
            }
            case M_PROPERTY_GET:
                *(int *)ka->arg = MPMAX(0, count);
                return M_PROPERTY_OK;
            }
            return M_PROPERTY_NOT_IMPLEMENTED;
        }
        // This is expected of the form "123" or "123/rest"
        char *next = strchr(ka->key, '/');
        char *end = NULL;
        const char *key_end = ka->key + strlen(ka->key);
        long int item = strtol(ka->key, &end, 10);
        // not a number, trailing characters, etc.
        if ((end != key_end || ka->key == key_end) && end != next)
            return M_PROPERTY_UNKNOWN;
        if (item < 0 || item >= count)
            return M_PROPERTY_UNKNOWN;
        if (next) {
            // Sub-path
            struct m_property_action_arg n_ka = *ka;
            n_ka.key = next + 1;
            return get_item(item, M_PROPERTY_KEY_ACTION, &n_ka, ctx);
        } else {
            // Direct query
            return get_item(item, ka->action, ka->arg, ctx);
        }
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}
