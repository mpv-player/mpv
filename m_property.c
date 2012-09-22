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

#include "talloc.h"
#include "m_option.h"
#include "m_property.h"
#include "mp_msg.h"
#include "mpcommon.h"

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
    if (action == M_PROPERTY_GET_TYPE && r < 0) {
        *(struct m_option *)arg = *prop;
        return M_PROPERTY_OK;
    }
    return r;
}

int m_property_do(const m_option_t *prop_list, const char *name,
                  int action, void *arg, void *ctx)
{
    union m_option_value val = {0};
    int r;

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

char *m_properties_expand_string(const m_option_t *prop_list, char *str,
                                 void *ctx)
{
    int l, fr = 0, pos = 0, size = strlen(str) + 512;
    char *p = NULL, *e, *ret = malloc(size);
    int skip = 0, lvl = 0, skip_lvl = 0;

    while (str[0]) {
        if (lvl > 0 && str[0] == ')') {
            if (skip && lvl <= skip_lvl)
                skip = 0;
            lvl--, str++, l = 0;
        } else if (str[0] == '$' && str[1] == '{'
                   && (e = strchr(str + 2, '}'))) {
            str += 2;
            int method = M_PROPERTY_PRINT;
            if (str[0] == '=') {
                str += 1;
                method = M_PROPERTY_GET_STRING;
            }
            int pl = e - str;
            char pname[pl + 1];
            memcpy(pname, str, pl);
            pname[pl] = 0;
            if (m_property_do(prop_list, pname, method, &p, ctx) >= 0 && p)
                l = strlen(p), fr = 1;
            else
                l = 0;
            str = e + 1;
        } else if (str[0] == '?' && str[1] == '('
                   && (e = strchr(str + 2, ':'))) {
            lvl++;
            if (!skip) {
                int is_not = str[2] == '!';
                int pl = e - str - (is_not ? 3 : 2);
                char pname[pl + 1];
                memcpy(pname, str + (is_not ? 3 : 2), pl);
                pname[pl] = 0;
                struct m_option opt = {0};
                union m_option_value val = {0};
                if (m_property_do(prop_list, pname, M_PROPERTY_GET_TYPE, &opt, ctx) <= 0 &&
                    m_property_do(prop_list, pname, M_PROPERTY_GET, &val, ctx) <= 0)
                {
                    if (!is_not)
                        skip = 1, skip_lvl = lvl;
                    m_option_free(&opt, &val);
                } else if (is_not)
                    skip = 1, skip_lvl = lvl;
            }
            str = e + 1, l = 0;
        } else
            p = str, l = 1, str++;

        if (skip || l <= 0)
            continue;

        if (pos + l + 1 > size) {
            size = pos + l + 512;
            ret = realloc(ret, size);
        }
        memcpy(ret + pos, p, l);
        pos += l;
        if (fr)
            talloc_free(p), fr = 0;
    }

    ret[pos] = 0;
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
