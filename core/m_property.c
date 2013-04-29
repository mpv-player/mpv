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

/// \file
/// \ingroup Properties

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include <libavutil/common.h>

#include "talloc.h"
#include "core/m_option.h"
#include "m_property.h"
#include "core/mp_msg.h"
#include "core/mp_common.h"

const struct m_option_type m_option_type_dummy = {
    .name = "Unknown",
};

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

static bool translate_legacy_property(const char *name, char *buffer,
                                      size_t buffer_size)
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

    if (strcmp(old_name, buffer) != 0) {
        mp_msg(MSGT_CPLAYER, MSGL_WARN, "Warning: property '%s' is deprecated, "
               "replaced with '%s'. Fix your input.conf!\n", old_name, buffer);
    }

    return true;
}

static int do_action(const m_option_t *prop_list, const char *name,
                     int action, void *arg, void *ctx)
{
    const char *sep;
    const m_option_t *prop;
    if ((sep = strchr(name, '/')) && sep[1]) {
        int len = sep - name;
        char base[len + 1];
        memcpy(base, name, len);
        base[len] = 0;
        prop = m_option_list_find(prop_list, base);
        struct m_property_action_arg ka = {
            .key = sep + 1,
            .action = action,
            .arg = arg,
        };
        action = M_PROPERTY_KEY_ACTION;
        arg = &ka;
    } else
        prop = m_option_list_find(prop_list, name);
    if (!prop)
        return M_PROPERTY_UNKNOWN;
    int (*control)(const m_option_t*, int, void*, void*) = prop->p;
    int r = control(prop, action, arg, ctx);
    if (action == M_PROPERTY_GET_TYPE && r < 0 &&
        prop->type != &m_option_type_dummy)
    {
        *(struct m_option *)arg = *prop;
        return M_PROPERTY_OK;
    }
    return r;
}

int m_property_do(const m_option_t *prop_list, const char *in_name,
                  int action, void *arg, void *ctx)
{
    union m_option_value val = {0};
    int r;

    char name[64];
    if (!translate_legacy_property(in_name, name, sizeof(name)))
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
        // (reject 0 return value: success, but empty string with flag)
        if (m_option_parse(&opt, bstr0(name), bstr0(arg), &val) <= 0)
            return M_PROPERTY_ERROR;
        r = do_action(prop_list, name, M_PROPERTY_SET, &val, ctx);
        m_option_free(&opt, &val);
        return r;
    }
    case M_PROPERTY_SWITCH: {
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
        if (!opt.type->clamp) {
            mp_msg(MSGT_CPLAYER, MSGL_WARN, "Property '%s' without clamp().\n",
                   name);
        } else {
            m_option_copy(&opt, &val, arg);
            r = opt.type->clamp(&opt, arg);
            m_option_free(&opt, &val);
            if (r != 0) {
                mp_msg(MSGT_CPLAYER, MSGL_ERR,
                       "Property '%s': invalid value.\n", name);
                return M_PROPERTY_ERROR;
            }
        }
        return do_action(prop_list, name, M_PROPERTY_SET, arg, ctx);
    }
    default:
        return do_action(prop_list, name, action, arg, ctx);
    }
}

static int m_property_do_bstr(const m_option_t *prop_list, bstr name,
                              int action, void *arg, void *ctx)
{
    char name0[64];
    if (name.len >= sizeof(name0))
        return M_PROPERTY_UNKNOWN;
    snprintf(name0, sizeof(name0), "%.*s", BSTR_P(name));
    return m_property_do(prop_list, name0, action, arg, ctx);
}

static void append_str(char **s, int *len, bstr append)
{
    MP_TARRAY_GROW(NULL, *s, *len + append.len);
    memcpy(*s + *len, append.start, append.len);
    *len = *len + append.len;
}

char *m_properties_expand_string(const m_option_t *prop_list, char *str0,
                                 void *ctx)
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
                bool cond_yes = bstr_eatstart0(&name, "?");
                bool cond_no = !cond_yes && bstr_eatstart0(&name, "!");
                bool raw = bstr_eatstart0(&name, "=");
                int method = (raw || cond_yes || cond_no)
                             ? M_PROPERTY_GET_STRING : M_PROPERTY_PRINT;

                char *s = NULL;
                int r = m_property_do_bstr(prop_list, name, method, &s, ctx);
                if (cond_yes || cond_no) {
                    skip = (!!s != cond_yes);
                } else {
                    skip = !!s;
                    char *append = s;
                    if (!s && !have_fallback && !raw) {
                        append = r == M_PROPERTY_UNAVAILABLE
                                 ? "(unavailable)" : "(error)";
                    }
                    append_str(&ret, &ret_len, bstr0(append));
                }
                talloc_free(s);
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

void m_properties_print_help_list(const m_option_t *list)
{
    char min[50], max[50];
    int i, count = 0;

    mp_tmsg(MSGT_CFGPARSER, MSGL_INFO,
            "\n Name                 Type            Min        Max\n\n");
    for (i = 0; list[i].name; i++) {
        const m_option_t *opt = &list[i];
        if (opt->flags & M_OPT_MIN)
            sprintf(min, "%-8.0f", opt->min);
        else
            strcpy(min, "No");
        if (opt->flags & M_OPT_MAX)
            sprintf(max, "%-8.0f", opt->max);
        else
            strcpy(max, "No");
        mp_msg(MSGT_CFGPARSER, MSGL_INFO,
               " %-20.20s %-15.15s %-10.10s %-10.10s\n",
               opt->name,
               opt->type->name,
               min,
               max);
        count++;
    }
    mp_tmsg(MSGT_CFGPARSER, MSGL_INFO, "\nTotal: %d properties\n", count);
}

int m_property_int_ro(const m_option_t *prop, int action,
                      void *arg, int var)
{
    if (action == M_PROPERTY_GET) {
        *(int *)arg = var;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

int m_property_int64_ro(const struct m_option* prop, int action, void* arg,
                        int64_t var)
{
    if (action == M_PROPERTY_GET) {
        *(int64_t *)arg = var;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

int m_property_float_ro(const m_option_t *prop, int action,
                        void *arg, float var)
{
    if (action == M_PROPERTY_GET) {
        *(float *)arg = var;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

int m_property_double_ro(const m_option_t *prop, int action,
                         void *arg, double var)
{
    if (action == M_PROPERTY_GET) {
        *(double *)arg = var;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

int m_property_strdup_ro(const struct m_option* prop, int action, void* arg,
                         const char *var)
{
    if (action == M_PROPERTY_GET) {
        if (!var)
            return M_PROPERTY_UNAVAILABLE;
        *(char **)arg = talloc_strdup(NULL, var);
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}
