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
/// \ingroup Config

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "talloc.h"

#include "m_config.h"
#include "core/m_option.h"
#include "core/mp_msg.h"

#define MAX_PROFILE_DEPTH 20

static int parse_include(struct m_config *config, struct bstr param, bool set)
{
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;
    if (!set)
        return 1;
    char *filename = bstrdup0(NULL, param);
    config->includefunc(config, filename);
    talloc_free(filename);
    return 1;
}

static int parse_profile(struct m_config *config, const struct m_option *opt,
                         struct bstr name, struct bstr param, bool set)
{
    if (!bstrcmp0(param, "help")) {
        struct m_profile *p;
        if (!config->profiles) {
            mp_tmsg(MSGT_CFGPARSER, MSGL_INFO,
                    "No profiles have been defined.\n");
            return M_OPT_EXIT - 1;
        }
        mp_tmsg(MSGT_CFGPARSER, MSGL_INFO, "Available profiles:\n");
        for (p = config->profiles; p; p = p->next)
            mp_msg(MSGT_CFGPARSER, MSGL_INFO, "\t%s\t%s\n", p->name,
                   p->desc ? p->desc : "");
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "\n");
        return M_OPT_EXIT - 1;
    }

    char **list = NULL;
    int r = m_option_type_string_list.parse(opt, name, param, &list);
    if (r < 0)
        return r;
    if (!list || !list[0])
        return M_OPT_INVALID;
    for (int i = 0; list[i]; i++) {
        struct m_profile *p = m_config_get_profile(config, list[i]);
        if (!p) {
            mp_tmsg(MSGT_CFGPARSER, MSGL_WARN, "Unknown profile '%s'.\n",
                    list[i]);
            r = M_OPT_INVALID;
        } else if (set)
            m_config_set_profile(config, p);
    }
    m_option_free(opt, &list);
    return r;
}

static int show_profile(struct m_option *opt, char *name, char *param)
{
    struct m_config *config = opt->priv;
    struct m_profile *p;
    int i, j;
    if (!param)
        return M_OPT_MISSING_PARAM;
    if (!(p = m_config_get_profile(config, param))) {
        mp_tmsg(MSGT_CFGPARSER, MSGL_ERR, "Unknown profile '%s'.\n", param);
        return M_OPT_EXIT - 1;
    }
    if (!config->profile_depth)
        mp_tmsg(MSGT_CFGPARSER, MSGL_INFO, "Profile %s: %s\n", param,
                p->desc ? p->desc : "");
    config->profile_depth++;
    for (i = 0; i < p->num_opts; i++) {
        char spc[config->profile_depth + 1];
        for (j = 0; j < config->profile_depth; j++)
            spc[j] = ' ';
        spc[config->profile_depth] = '\0';

        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "%s%s=%s\n", spc,
               p->opts[2 * i], p->opts[2 * i + 1]);

        if (config->profile_depth < MAX_PROFILE_DEPTH
            && !strcmp(p->opts[2*i], "profile")) {
            char *e, *list = p->opts[2 * i + 1];
            while ((e = strchr(list, ','))) {
                int l = e - list;
                char tmp[l+1];
                if (!l)
                    continue;
                memcpy(tmp, list, l);
                tmp[l] = '\0';
                show_profile(opt, name, tmp);
                list = e + 1;
            }
            if (list[0] != '\0')
                show_profile(opt, name, list);
        }
    }
    config->profile_depth--;
    if (!config->profile_depth)
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "\n");
    return M_OPT_EXIT - 1;
}

static int list_options(struct m_option *opt, char *name, char *param)
{
    struct m_config *config = opt->priv;
    m_config_print_option_list(config);
    return M_OPT_EXIT;
}

// The memcpys are supposed to work around the struct aliasing violation,
// that would result if we just dereferenced a void** (where the void** is
// actually casted from struct some_type* ).
static void *substruct_read_ptr(void *ptr)
{
    void *res;
    memcpy(&res, ptr, sizeof(void*));
    return res;
}
static void substruct_write_ptr(void *ptr, void *val)
{
    memcpy(ptr, &val, sizeof(void*));
}

static void m_config_add_option(struct m_config *config,
                                const char *prefix,
                                struct m_config_option *parent,
                                const struct m_option *arg);

static int config_destroy(void *p)
{
    struct m_config *config = p;
    for (struct m_config_option *copt = config->opts; copt; copt = copt->next) {
        if (copt->alias_owner)
            continue;
        if (copt->opt->type->flags & M_OPT_TYPE_DYNAMIC) {
            m_option_free(copt->opt, copt->data);
        }
        if (copt->global_backup)
            m_option_free(copt->opt, copt->global_backup);
    }
    return 0;
}

struct m_config *m_config_simple(void *optstruct)
{
    struct m_config *config = talloc_struct(NULL, struct m_config, {
        .optstruct = optstruct,
    });
    talloc_set_destructor(config, config_destroy);
    return config;
}

struct m_config *m_config_new(void *optstruct,
                              int includefunc(struct m_config *conf,
                                              char *filename))
{
    static const struct m_option ref_opts[] = {
        { "profile", NULL, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL },
        { "show-profile", show_profile, &m_option_type_print_func_param,
          CONF_NOCFG },
        { "list-options", list_options, CONF_TYPE_PRINT_FUNC, CONF_NOCFG },
        { NULL }
    };

    struct m_config *config = m_config_simple(optstruct);

    struct m_option *self_opts = talloc_memdup(config, ref_opts,
                                               sizeof(ref_opts));
    for (int i = 1; self_opts[i].name; i++)
        self_opts[i].priv = config;
    m_config_register_options(config, self_opts);
    if (includefunc) {
        struct m_option *p = talloc_ptrtype(config, p);
        *p = (struct m_option){
            "include", NULL, CONF_TYPE_STRING, 0,
        };
        m_config_add_option(config, "", NULL, p);
        config->includefunc = includefunc;
    }

    return config;
}

void m_config_free(struct m_config *config)
{
    talloc_free(config);
}

static void ensure_backup(struct m_config *config, struct m_config_option *co)
{
    while (co->alias_owner)
        co = co->alias_owner;
    if (!config->file_local_mode)
        return;
    if (co->opt->type->flags & M_OPT_TYPE_HAS_CHILD)
        return;
    if (co->opt->flags & M_OPT_GLOBAL)
        return;
    if (co->global_backup)
        return;
    co->global_backup = talloc_zero_size(co, co->opt->type->size);
    m_option_copy(co->opt, co->global_backup, co->data);
}

void m_config_enter_file_local(struct m_config *config)
{
    assert(!config->file_local_mode);
    config->file_local_mode = true;
    for (struct m_config_option *co = config->opts; co; co = co->next) {
        if (co->opt->flags & M_OPT_LOCAL)
            ensure_backup(config, co);
    }
}

void m_config_leave_file_local(struct m_config *config)
{
    assert(config->file_local_mode);
    config->file_local_mode = false;
    for (struct m_config_option *co = config->opts; co; co = co->next) {
        if (co->global_backup) {
            m_option_copy(co->opt, co->data, co->global_backup);
            m_option_free(co->opt, co->global_backup);
            talloc_free(co->global_backup);
            co->global_backup = NULL;
        }
    }
}

void m_config_mark_file_local(struct m_config *config, const char *opt)
{
    struct m_config_option *co = m_config_get_co(config, bstr0(opt));
    if (co) {
        ensure_backup(config, co);
    } else {
        mp_tmsg(MSGT_CFGPARSER, MSGL_ERR, "Option %s not found.\n", opt);
    }
}

void m_config_mark_all_file_local(struct m_config *config)
{
    for (struct m_config_option *co = config->opts; co; co = co->next)
        ensure_backup(config, co);
}

// Given an option --opt, add --no-opt (if applicable).
static void add_negation_option(struct m_config *config,
                                struct m_config_option *parent,
                                const struct m_option *opt)
{
    int value;
    if (opt->type == CONF_TYPE_FLAG) {
        value = opt->min;
    } else if (opt->type == CONF_TYPE_CHOICE) {
        // Find out whether there's a "no" choice.
        // m_option_parse() should be used for this, but it prints
        // unsilenceable error messages.
        struct m_opt_choice_alternatives *alt = opt->priv;
        for ( ; alt->name; alt++) {
            if (strcmp(alt->name, "no") == 0)
                break;
        }
        if (!alt->name)
            return;
        value = alt->value;
    } else {
        return;
    }
    struct m_option *no_opt = talloc_ptrtype(config, no_opt);
    *no_opt = (struct m_option) {
        .name = talloc_asprintf(no_opt, "no-%s", opt->name),
        .type = CONF_TYPE_STORE,
        .flags = opt->flags & (M_OPT_NOCFG | M_OPT_GLOBAL | M_OPT_LOCAL |
                               M_OPT_PRE_PARSE),
        .new = opt->new,
        .p = opt->p,
        .offset = opt->offset,
        .max = value,
    };
    m_config_add_option(config, "", parent, no_opt);
    // Consider a parent option "--sub" and a subopt "opt". Then the above
    // call will add "no-opt". Add "--no-sub-opt" too. (This former call will
    // also generate "--sub-no-opt", which is not really needed or wanted, but
    // is a consequence of supporting "--sub=...:no-opt".)
    if (parent && parent->name && strlen(parent->name)) {
        no_opt = talloc_memdup(config, no_opt, sizeof(*no_opt));
        no_opt->name = opt->name;
        m_config_add_option(config, "no-", parent, no_opt);
    }
}

static void add_options(struct m_config *config,
                        struct m_config_option *parent,
                        const struct m_option *defs)
{
    for (int i = 0; defs[i].name; i++)
        m_config_add_option(config, "", parent, defs + i);
}

// Sub-config that adds all its children to the parent.
static bool is_merge_opt(const struct m_option *opt)
{
    return (opt->type->flags & M_OPT_TYPE_HAS_CHILD) && strlen(opt->name) == 0;
}

static void m_config_add_option(struct m_config *config,
                                const char *prefix,
                                struct m_config_option *parent,
                                const struct m_option *arg)
{
    assert(config != NULL);
    assert(arg != NULL);

    // Allocate a new entry for this option
    struct m_config_option *co = talloc_zero(config, struct m_config_option);
    co->opt = arg;

    void *optstruct = config->optstruct;
    if (parent && (parent->opt->type->flags & M_OPT_TYPE_USE_SUBSTRUCT))
        optstruct = substruct_read_ptr(parent->data);
    co->data = arg->new ? (char *)optstruct + arg->offset : arg->p;

    if (parent) {
        // Merge case: pretend it has no parent (note that we still must follow
        //             the "real" parent for accessing struct fields)
        co->parent = is_merge_opt(parent->opt) ? parent->parent : parent;
    }

    // Fill in the full name
    if (co->parent) {
        co->name = talloc_asprintf(co, "%s-%s", co->parent->name, arg->name);
    } else {
        co->name = (char *)arg->name;
    }
    co->name = talloc_asprintf(co, "%s%s", prefix, co->name);

    // Option with children -> add them
    if (arg->type->flags & M_OPT_TYPE_HAS_CHILD) {
        if (arg->type->flags & M_OPT_TYPE_USE_SUBSTRUCT) {
            const struct m_sub_options *subopts = arg->priv;
            if (!substruct_read_ptr(co->data)) {
                void *subdata = m_config_alloc_struct(config, subopts);
                substruct_write_ptr(co->data, subdata);
            }
            add_options(config, co, subopts->opts);
        } else {
            const struct m_option *sub = arg->p;
            add_options(config, co, sub);
        }
    } else {
        // Check if there is already an option pointing to this address
        if (co->data) {
            for (struct m_config_option *i = config->opts; i; i = i->next) {
                if (co->data == i->data) {
                    // So we don't save the same vars more than 1 time
                    co->alias_owner = i;
                    break;
                }
            }
        }
        if (co->alias_owner) {
            assert(!arg->defval);
        } else {
            if (arg->defval) {
                // Target data in optstruct is supposed to be cleared (consider
                // m_option freeing previously set dynamic data).
                m_option_copy(arg, co->data, arg->defval);
            } else if (arg->type->flags & M_OPT_TYPE_DYNAMIC) {
                // Initialize dynamically managed fields from static data (like
                // string options): copy the option into temporary memory,
                // clear the original option (to stop m_option from freeing the
                // static data), copy it back.
                if (co->data) {
                    union m_option_value temp = {0};
                    m_option_copy(arg, &temp, co->data);
                    memset(co->data, 0, arg->type->size);
                    m_option_copy(arg, co->data, &temp);
                    m_option_free(arg, &temp);
                }
            }
        }
    }

    // pretend that merge options don't exist (only their children matter)
    if (!is_merge_opt(co->opt)) {
        co->next = config->opts;
        config->opts = co;
    }

    add_negation_option(config, parent, arg);
}

int m_config_register_options(struct m_config *config,
                              const struct m_option *args)
{
    assert(config != NULL);
    assert(args != NULL);

    add_options(config, NULL, args);

    return 1;
}

struct m_config_option *m_config_get_co(const struct m_config *config,
                                        struct bstr name)
{
    struct m_config_option *co;

    for (co = config->opts; co; co = co->next) {
        struct bstr coname = bstr0(co->name);
        if ((co->opt->type->flags & M_OPT_TYPE_ALLOW_WILDCARD)
                && bstr_endswith0(coname, "*")) {
            coname.len--;
            if (bstrcmp(bstr_splice(name, 0, coname.len), coname) == 0)
                return co;
        } else if (bstrcmp(coname, name) == 0)
            return co;
    }
    return NULL;
}

static int parse_subopts(struct m_config *config, void *optstruct, char *name,
                         char *prefix, struct bstr param, int flags);

static int m_config_parse_option(struct m_config *config, void *optstruct,
                                 struct bstr name, struct bstr param, int flags)
{
    assert(config != NULL);
    assert(name.len != 0);
    bool set = !(flags & M_SETOPT_CHECK_ONLY);

    struct m_config_option *co = m_config_get_co(config, name);
    if (!co)
        return M_OPT_UNKNOWN;

    // This is the only mandatory function
    assert(co->opt->type->parse);

    if ((flags & M_SETOPT_PRE_PARSE_ONLY) && !(co->opt->flags & M_OPT_PRE_PARSE))
        return 0;

    // Check if this option isn't forbidden in the current mode
    if ((flags & M_SETOPT_FROM_CONFIG_FILE) && (co->opt->flags & M_OPT_NOCFG)) {
        mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,
                "The %.*s option can't be used in a config file.\n",
                BSTR_P(name));
        return M_OPT_INVALID;
    }
    if (config->file_local_mode && (co->opt->flags & M_OPT_GLOBAL)) {
        mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,
                "The %.*s option is global and can't be set per-file.\n",
                BSTR_P(name));
        return M_OPT_INVALID;
    }

    if (config->includefunc && !bstrcmp0(name, "include")) {
        return parse_include(config, param, set);
    } else if (!bstrcmp0(name, "profile"))
        return parse_profile(config, co->opt, name, param, set);

    // Option with children are a bit different to parse
    if (co->opt->type->flags & M_OPT_TYPE_HAS_CHILD) {
        char prefix[110];
        assert(strlen(co->name) < 100);
        sprintf(prefix, "%s-", co->name);
        return parse_subopts(config, optstruct, co->name, prefix, param, flags);
    }

    if (set)
        ensure_backup(config, co);

    return m_option_parse(co->opt, name, param, set ? co->data : NULL);
}

static int parse_subopts(struct m_config *config, void *optstruct, char *name,
                         char *prefix, struct bstr param, int flags)
{
    char **lst = NULL;
    // Split the argument into child options
    int r = m_option_type_subconfig.parse(NULL, bstr0(""), param, &lst);
    if (r < 0)
        return r;
    // Parse the child options
    for (int i = 0; lst && lst[2 * i]; i++) {
        // Build the full name
        char n[110];
        if (snprintf(n, 110, "%s%s", prefix, lst[2 * i]) > 100)
            abort();
        r = m_config_parse_option(config, optstruct, bstr0(n),
                                  bstr0(lst[2 * i + 1]), flags);
        if (r < 0) {
            if (r > M_OPT_EXIT) {
                mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,
                        "Error parsing suboption %s/%s (%s)\n",
                        name, lst[2 * i], m_option_strerror(r));
                r = M_OPT_INVALID;
            }
            break;
        }
    }
    talloc_free(lst);
    return r;
}

int m_config_parse_suboptions(struct m_config *config, char *name,
                              char *subopts)
{
    if (!subopts || !*subopts)
        return 0;
    int r = parse_subopts(config, config->optstruct, name, "", bstr0(subopts), 0);
    if (r < 0 && r > M_OPT_EXIT) {
        mp_tmsg(MSGT_CFGPARSER, MSGL_ERR, "Error parsing suboption %s (%s)\n",
                name, m_option_strerror(r));
        r = M_OPT_INVALID;
    }
    return r;
}

int m_config_set_option_ext(struct m_config *config, struct bstr name,
                            struct bstr param, int flags)
{
    int r = m_config_parse_option(config, config->optstruct, name, param, flags);
    if (r < 0 && r > M_OPT_EXIT) {
        mp_tmsg(MSGT_CFGPARSER, MSGL_ERR, "Error parsing option %.*s (%s)\n",
                BSTR_P(name), m_option_strerror(r));
        r = M_OPT_INVALID;
    }
    return r;
}

int m_config_set_option(struct m_config *config, struct bstr name,
                                 struct bstr param)
{
    return m_config_set_option_ext(config, name, param, 0);
}

const struct m_option *m_config_get_option(const struct m_config *config,
                                           struct bstr name)
{
    struct m_config_option *co;

    assert(config != NULL);

    co = m_config_get_co(config, name);
    if (co)
        return co->opt;
    else
        return NULL;
}

int m_config_option_requires_param(struct m_config *config, bstr name)
{
    const struct m_option *opt = m_config_get_option(config, name);
    if (opt) {
        if (bstr_endswith0(name, "-clr"))
            return 0;
        return m_option_required_params(opt);
    }
    return M_OPT_UNKNOWN;
}

void m_config_print_option_list(const struct m_config *config)
{
    char min[50], max[50];
    struct m_config_option *co;
    int count = 0;

    if (!config->opts)
        return;

    mp_tmsg(MSGT_CFGPARSER, MSGL_INFO,
            "\n Name                 Type            Min        Max      Global  Cfg\n\n");
    for (co = config->opts; co; co = co->next) {
        const struct m_option *opt = co->opt;
        if (opt->type->flags & M_OPT_TYPE_HAS_CHILD)
            continue;
        if (opt->flags & M_OPT_MIN)
            sprintf(min, "%-8.0f", opt->min);
        else
            strcpy(min, "No");
        if (opt->flags & M_OPT_MAX)
            sprintf(max, "%-8.0f", opt->max);
        else
            strcpy(max, "No");
        mp_msg(MSGT_CFGPARSER, MSGL_INFO,
             " %-20.20s %-15.15s %-10.10s %-10.10s %-3.3s   %-3.3s\n",
               co->name,
               co->opt->type->name,
               min,
               max,
               opt->flags & CONF_GLOBAL ? "Yes" : "No",
               opt->flags & CONF_NOCFG ? "No" : "Yes");
        count++;
    }
    mp_tmsg(MSGT_CFGPARSER, MSGL_INFO, "\nTotal: %d options\n", count);
}

struct m_profile *m_config_get_profile(const struct m_config *config,
                                       char *name)
{
    struct m_profile *p;
    for (p = config->profiles; p; p = p->next)
        if (!strcmp(p->name, name))
            return p;
    return NULL;
}

struct m_profile *m_config_add_profile(struct m_config *config, char *name)
{
    struct m_profile *p = m_config_get_profile(config, name);
    if (p)
        return p;
    p = talloc_zero(config, struct m_profile);
    p->name = talloc_strdup(p, name);
    p->next = config->profiles;
    config->profiles = p;
    return p;
}

void m_profile_set_desc(struct m_profile *p, char *desc)
{
    talloc_free(p->desc);
    p->desc = talloc_strdup(p, desc);
}

int m_config_set_profile_option(struct m_config *config, struct m_profile *p,
                                bstr name, bstr val)
{
    int i = m_config_set_option_ext(config, name, val,
                                    M_SETOPT_CHECK_ONLY |
                                    M_SETOPT_FROM_CONFIG_FILE);
    if (i < 0)
        return i;
    p->opts = talloc_realloc(p, p->opts, char *, 2 * (p->num_opts + 2));
    p->opts[p->num_opts * 2] = bstrdup0(p, name);
    p->opts[p->num_opts * 2 + 1] = bstrdup0(p, val);
    p->num_opts++;
    p->opts[p->num_opts * 2] = p->opts[p->num_opts * 2 + 1] = NULL;
    return 1;
}

void m_config_set_profile(struct m_config *config, struct m_profile *p)
{
    if (config->profile_depth > MAX_PROFILE_DEPTH) {
        mp_tmsg(MSGT_CFGPARSER, MSGL_WARN,
                "WARNING: Profile inclusion too deep.\n");
        return;
    }
    config->profile_depth++;
    for (int i = 0; i < p->num_opts; i++) {
        m_config_set_option_ext(config,
                                bstr0(p->opts[2 * i]),
                                bstr0(p->opts[2 * i + 1]),
                                M_SETOPT_FROM_CONFIG_FILE);
    }
    config->profile_depth--;
}

void *m_config_alloc_struct(void *talloc_parent,
                            const struct m_sub_options *subopts)
{
    void *substruct = talloc_zero_size(talloc_parent, subopts->size);
    if (subopts->defaults)
        memcpy(substruct, subopts->defaults, subopts->size);
    return substruct;
}
