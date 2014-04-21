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

#include "libmpv/client.h"

#include "talloc.h"

#include "m_config.h"
#include "options/m_option.h"
#include "common/msg.h"

static const union m_option_value default_value;

// Profiles allow to predefine some sets of options that can then
// be applied later on with the internal -profile option.
#define MAX_PROFILE_DEPTH 20

struct m_profile {
    struct m_profile *next;
    char *name;
    char *desc;
    int num_opts;
    // Option/value pair array.
    char **opts;
};

// In the file local case, this contains the old global value.
struct m_opt_backup {
    struct m_opt_backup *next;
    struct m_config_option *co;
    void *backup;
};

static int parse_include(struct m_config *config, struct bstr param, bool set,
                         int flags)
{
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;
    if (!set)
        return 1;
    char *filename = bstrdup0(NULL, param);
    config->includefunc(config->includefunc_ctx, filename, flags);
    talloc_free(filename);
    return 1;
}

static int parse_profile(struct m_config *config, const struct m_option *opt,
                         struct bstr name, struct bstr param, bool set, int flags)
{
    if (!bstrcmp0(param, "help")) {
        struct m_profile *p;
        if (!config->profiles) {
            MP_INFO(config, "No profiles have been defined.\n");
            return M_OPT_EXIT - 1;
        }
        MP_INFO(config, "Available profiles:\n");
        for (p = config->profiles; p; p = p->next)
            MP_INFO(config, "\t%s\t%s\n", p->name, p->desc ? p->desc : "");
        MP_INFO(config, "\n");
        return M_OPT_EXIT - 1;
    }

    char **list = NULL;
    int r = m_option_type_string_list.parse(config->log, opt, name, param, &list);
    if (r < 0)
        return r;
    if (!list || !list[0])
        return M_OPT_INVALID;
    for (int i = 0; list[i]; i++) {
        struct m_profile *p = m_config_get_profile0(config, list[i]);
        if (!p) {
            MP_WARN(config, "Unknown profile '%s'.\n", list[i]);
            r = M_OPT_INVALID;
        } else if (set)
            m_config_set_profile(config, p, flags);
    }
    m_option_free(opt, &list);
    return r;
}

static int show_profile(struct m_config *config, bstr param)
{
    struct m_profile *p;
    int i, j;
    if (!param.len)
        return M_OPT_MISSING_PARAM;
    if (!(p = m_config_get_profile(config, param))) {
        MP_ERR(config, "Unknown profile '%.*s'.\n", BSTR_P(param));
        return M_OPT_EXIT - 1;
    }
    if (!config->profile_depth)
        MP_INFO(config, "Profile %s: %s\n", p->name,
                p->desc ? p->desc : "");
    config->profile_depth++;
    for (i = 0; i < p->num_opts; i++) {
        char spc[config->profile_depth + 1];
        for (j = 0; j < config->profile_depth; j++)
            spc[j] = ' ';
        spc[config->profile_depth] = '\0';

        MP_INFO(config, "%s%s=%s\n", spc, p->opts[2 * i], p->opts[2 * i + 1]);

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
                show_profile(config, bstr0(tmp));
                list = e + 1;
            }
            if (list[0] != '\0')
                show_profile(config, bstr0(list));
        }
    }
    config->profile_depth--;
    if (!config->profile_depth)
        MP_INFO(config, "\n");
    return M_OPT_EXIT - 1;
}

static int list_options(struct m_config *config)
{
    m_config_print_option_list(config);
    return M_OPT_EXIT;
}

// The memcpys are supposed to work around the strict aliasing violation,
// that would result if we just dereferenced a void** (where the void** is
// actually casted from struct some_type* ).
static void *substruct_read_ptr(const void *ptr)
{
    void *res;
    memcpy(&res, ptr, sizeof(void*));
    return res;
}
static void substruct_write_ptr(void *ptr, void *val)
{
    memcpy(ptr, &val, sizeof(void*));
}

static void add_options(struct m_config *config,
                        const char *parent_name,
                        void *optstruct,
                        const void *optstruct_def,
                        const struct m_option *defs);

static void config_destroy(void *p)
{
    struct m_config *config = p;
    m_config_restore_backups(config);
    for (int n = 0; n < config->num_opts; n++)
        m_option_free(config->opts[n].opt, config->opts[n].data);
}

struct m_config *m_config_new(void *talloc_ctx, struct mp_log *log,
                              size_t size, const void *defaults,
                              const struct m_option *options)
{
    struct m_config *config = talloc(talloc_ctx, struct m_config);
    talloc_set_destructor(config, config_destroy);
    *config = (struct m_config) {.log = log};
    // size==0 means a dummy object is created
    if (size) {
        config->optstruct = talloc_zero_size(config, size);
        if (defaults)
            memcpy(config->optstruct, defaults, size);
    }
    if (options)
        add_options(config, "", config->optstruct, defaults, options);
    return config;
}

struct m_config *m_config_from_obj_desc(void *talloc_ctx, struct mp_log *log,
                                        struct m_obj_desc *desc)
{
    return m_config_new(talloc_ctx, log, desc->priv_size, desc->priv_defaults,
                        desc->options);
}

// Like m_config_from_obj_desc(), but don't allocate option struct.
struct m_config *m_config_from_obj_desc_noalloc(void *talloc_ctx,
                                                struct mp_log *log,
                                                struct m_obj_desc *desc)
{
    return m_config_new(talloc_ctx, log, 0, desc->priv_defaults, desc->options);
}

int m_config_set_obj_params(struct m_config *conf, char **args)
{
    for (int n = 0; args && args[n * 2 + 0]; n++) {
        int r = m_config_set_option(conf, bstr0(args[n * 2 + 0]),
                                    bstr0(args[n * 2 + 1]));
        if (r < 0)
            return r;
    }
    return 0;
}

int m_config_apply_defaults(struct m_config *config, const char *name,
                            struct m_obj_settings *defaults)
{
    int r = 0;
    for (int n = 0; defaults && defaults[n].name; n++) {
        struct m_obj_settings *entry = &defaults[n];
        if (name && strcmp(entry->name, name) == 0) {
            r = m_config_set_obj_params(config, entry->attribs);
            break;
        }
    }
    return r;
}

static void ensure_backup(struct m_config *config, struct m_config_option *co)
{
    if (co->opt->type->flags & M_OPT_TYPE_HAS_CHILD)
        return;
    if (co->opt->flags & M_OPT_GLOBAL)
        return;
    if (!co->data)
        return;
    for (struct m_opt_backup *cur = config->backup_opts; cur; cur = cur->next) {
        if (cur->co->data == co->data) // comparing data ptr catches aliases
            return;
    }
    struct m_opt_backup *bc = talloc_ptrtype(NULL, bc);
    *bc = (struct m_opt_backup) {
        .co = co,
        .backup = talloc_zero_size(bc, co->opt->type->size),
    };
    m_option_copy(co->opt, bc->backup, co->data);
    bc->next = config->backup_opts;
    config->backup_opts = bc;
}

void m_config_restore_backups(struct m_config *config)
{
    while (config->backup_opts) {
        struct m_opt_backup *bc = config->backup_opts;
        config->backup_opts = bc->next;

        m_option_copy(bc->co->opt, bc->co->data, bc->backup);
        m_option_free(bc->co->opt, bc->backup);
        talloc_free(bc);
    }
}

void m_config_backup_opt(struct m_config *config, const char *opt)
{
    struct m_config_option *co = m_config_get_co(config, bstr0(opt));
    if (co) {
        ensure_backup(config, co);
    } else {
        MP_ERR(config, "Option %s not found.\n", opt);
    }
}

void m_config_backup_all_opts(struct m_config *config)
{
    for (int n = 0; n < config->num_opts; n++)
        ensure_backup(config, &config->opts[n]);
}

// Given an option --opt, add --no-opt (if applicable).
static void add_negation_option(struct m_config *config,
                                struct m_config_option *orig,
                                const char *parent_name)
{
    const struct m_option *opt = orig->opt;
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
        .name = opt->name,
        .type = CONF_TYPE_STORE,
        .flags = opt->flags & (M_OPT_NOCFG | M_OPT_GLOBAL | M_OPT_PRE_PARSE),
        .is_new_option = opt->is_new_option,
        .p = opt->p,
        .offset = opt->offset,
        .max = value,
    };
    // Add --no-sub-opt
    struct m_config_option co = *orig;
    co.name = talloc_asprintf(config, "no-%s", orig->name);
    co.opt = no_opt;
    co.is_generated = true;
    MP_TARRAY_APPEND(config, config->opts, config->num_opts, co);
    // Add --sub-no-opt (unfortunately needed for: "--sub=...:no-opt")
    if (parent_name[0]) {
        co.name = talloc_asprintf(config, "%s-no-%s", parent_name, opt->name);
        MP_TARRAY_APPEND(config, config->opts, config->num_opts, co);
    }
}

static void m_config_add_option(struct m_config *config,
                                const char *parent_name,
                                void *optstruct,
                                const void *optstruct_def,
                                const struct m_option *arg);

static void add_options(struct m_config *config,
                        const char *parent_name,
                        void *optstruct,
                        const void *optstruct_def,
                        const struct m_option *defs)
{
    for (int i = 0; defs && defs[i].name; i++)
        m_config_add_option(config, parent_name, optstruct, optstruct_def, &defs[i]);
}

static void m_config_add_option(struct m_config *config,
                                const char *parent_name,
                                void *optstruct,
                                const void *optstruct_def,
                                const struct m_option *arg)
{
    assert(config != NULL);
    assert(arg != NULL);

    struct m_config_option co = {
        .opt = arg,
        .name = arg->name,
    };

    if (arg->is_new_option) {
        if (optstruct)
            co.data = (char *)optstruct + arg->offset;
        if (optstruct_def)
            co.default_data = (char *)optstruct_def + arg->offset;
    } else {
        co.data = arg->p;
        co.default_data = arg->p;
    }

    if (arg->defval)
        co.default_data = arg->defval;

    if (!co.default_data)
        co.default_data = &default_value;

    // Fill in the full name
    if (!co.name[0]) {
        co.name = parent_name;
    } else if (parent_name[0]) {
        co.name = talloc_asprintf(config, "%s-%s", parent_name, co.name);
    }

    // Option with children -> add them
    if (arg->type->flags & M_OPT_TYPE_HAS_CHILD) {
        if (arg->type->flags & M_OPT_TYPE_USE_SUBSTRUCT) {
            const struct m_sub_options *subopts = arg->priv;

            void *new_optstruct = NULL;
            if (co.data) {
                new_optstruct = m_config_alloc_struct(config, subopts);
                substruct_write_ptr(co.data, new_optstruct);
            }

            const void *new_optstruct_def = substruct_read_ptr(co.default_data);
            if (!new_optstruct_def)
                new_optstruct_def = subopts->defaults;

            add_options(config, co.name, new_optstruct,
                        new_optstruct_def, subopts->opts);
        } else {
            const struct m_option *sub = arg->p;
            add_options(config, co.name, optstruct, optstruct_def, sub);
        }
    } else {
        // Initialize options
        if (co.data && co.default_data) {
            if (arg->type->flags & M_OPT_TYPE_DYNAMIC) {
                // Would leak memory by overwriting *co.data repeatedly.
                for (int i = 0; i < config->num_opts; i++) {
                    if (co.data == config->opts[i].data)
                        assert(0);
                }
            }
            // In case this is dynamic data, it has to be allocated and copied.
            union m_option_value temp = {0};
            memcpy(&temp, co.default_data, arg->type->size);
            memset(co.data, 0, arg->type->size);
            m_option_copy(arg, co.data, &temp);
        }
    }

    if (arg->name[0]) // no own name -> hidden
        MP_TARRAY_APPEND(config, config->opts, config->num_opts, co);

    add_negation_option(config, &co, parent_name);
}

struct m_config_option *m_config_get_co(const struct m_config *config,
                                        struct bstr name)
{

    for (int n = 0; n < config->num_opts; n++) {
        struct m_config_option *co = &config->opts[n];
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

const char *m_config_get_positional_option(const struct m_config *config, int p)
{
    int pos = 0;
    for (int n = 0; n < config->num_opts; n++) {
        struct m_config_option *co = &config->opts[n];
        if (!co->is_generated) {
            if (pos == p)
                return co->name;
            pos++;
        }
    }
    return NULL;
}

static int parse_subopts(struct m_config *config, char *name, char *prefix,
                         struct bstr param, int flags);

static int m_config_parse_option(struct m_config *config, struct bstr name,
                                 struct bstr param, int flags)
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

    if ((flags & M_SETOPT_PRESERVE_CMDLINE) && co->is_set_from_cmdline)
        set = false;

    if (set) {
        MP_VERBOSE(config, "Setting option '%.*s' = '%.*s' (flags = %d)\n",
                   BSTR_P(name), BSTR_P(param), flags);
    }

    // Check if this option isn't forbidden in the current mode
    if ((flags & M_SETOPT_FROM_CONFIG_FILE) && (co->opt->flags & M_OPT_NOCFG)) {
        MP_ERR(config, "The %.*s option can't be used in a config file.\n",
                BSTR_P(name));
        return M_OPT_INVALID;
    }
    if (flags & M_SETOPT_BACKUP) {
        if (co->opt->flags & M_OPT_GLOBAL) {
            MP_ERR(config, "The %.*s option is global and can't be set per-file.\n",
                    BSTR_P(name));
            return M_OPT_INVALID;
        }
        if (set)
            ensure_backup(config, co);
    }

    if (config->includefunc && bstr_equals0(name, "include"))
        return parse_include(config, param, set, flags);
    if (config->use_profiles && bstr_equals0(name, "profile"))
        return parse_profile(config, co->opt, name, param, set, flags);
    if (config->use_profiles && bstr_equals0(name, "show-profile"))
        return show_profile(config, param);
    if (bstr_equals0(name, "list-options"))
        return list_options(config);

    // Option with children are a bit different to parse
    if (co->opt->type->flags & M_OPT_TYPE_HAS_CHILD) {
        char prefix[110];
        assert(strlen(co->name) < 100);
        sprintf(prefix, "%s-", co->name);
        return parse_subopts(config, (char *)co->name, prefix, param, flags);
    }

    int r = m_option_parse(config->log, co->opt, name, param,
                           set ? co->data : NULL);

    if (r >= 0 && set && (flags & M_SETOPT_FROM_CMDLINE)) {
        co->is_set_from_cmdline = true;
        // Mark aliases too
        if (co->data) {
            for (int n = 0; n < config->num_opts; n++) {
                struct m_config_option *co2 = &config->opts[n];
                if (co2->data == co->data)
                    co2->is_set_from_cmdline = true;
            }
        }
    }

    return r;
}

static int parse_subopts(struct m_config *config, char *name, char *prefix,
                         struct bstr param, int flags)
{
    char **lst = NULL;
    // Split the argument into child options
    int r = m_option_type_subconfig.parse(config->log, NULL, bstr0(""), param, &lst);
    if (r < 0)
        return r;
    // Parse the child options
    for (int i = 0; lst && lst[2 * i]; i++) {
        // Build the full name
        char n[110];
        if (snprintf(n, 110, "%s%s", prefix, lst[2 * i]) > 100)
            abort();
        r = m_config_parse_option(config,bstr0(n), bstr0(lst[2 * i + 1]), flags);
        if (r < 0) {
            if (r > M_OPT_EXIT) {
                MP_ERR(config, "Error parsing suboption %s/%s (%s)\n",
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
    int r = parse_subopts(config, name, "", bstr0(subopts), 0);
    if (r < 0 && r > M_OPT_EXIT) {
        MP_ERR(config, "Error parsing suboption %s (%s)\n",
               name, m_option_strerror(r));
        r = M_OPT_INVALID;
    }
    return r;
}

int m_config_set_option_ext(struct m_config *config, struct bstr name,
                            struct bstr param, int flags)
{
    int r = m_config_parse_option(config, name, param, flags);
    if (r < 0 && r > M_OPT_EXIT) {
        MP_ERR(config, "Error parsing option %.*s (%s)\n",
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

int m_config_set_option_node(struct m_config *config, bstr name,
                             struct mpv_node *data)
{
    struct m_config_option *co = m_config_get_co(config, name);
    if (!co)
        return M_OPT_UNKNOWN;

    // This affects some special options like "include", "profile". Maybe these
    // should work, or maybe not. For now they would require special code.
    if (!co->data)
        return M_OPT_UNKNOWN;

    int r;

    // Do this on an "empty" type to make setting the option strictly overwrite
    // the old value, as opposed to e.g. appending to lists.
    union m_option_value val = {0};

    if (data->format == MPV_FORMAT_STRING) {
        bstr param = bstr0(data->u.string);
        r = m_option_parse(mp_null_log, co->opt, name, param, &val);
    } else {
        r = m_option_set_node(co->opt, &val, data);
    }

    if (r >= 0)
        m_option_copy(co->opt, co->data, &val);

    m_option_free(co->opt, &val);
    return r;
}

const struct m_option *m_config_get_option(const struct m_config *config,
                                           struct bstr name)
{
    assert(config != NULL);

    struct m_config_option *co = m_config_get_co(config, name);
    return co ? co->opt : NULL;
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

static int sort_opt_compare(const void *pa, const void *pb)
{
    const struct m_config_option *a = pa;
    const struct m_config_option *b = pb;
    return strcasecmp(a->name, b->name);
}

void m_config_print_option_list(const struct m_config *config)
{
    char min[50], max[50];
    int count = 0;
    const char *prefix = config->is_toplevel ? "--" : "";

    struct m_config_option *sorted =
        talloc_memdup(NULL, config->opts, config->num_opts * sizeof(sorted[0]));
    if (config->is_toplevel)
        qsort(sorted, config->num_opts, sizeof(sorted[0]), sort_opt_compare);

    MP_INFO(config, "Options:\n\n");
    for (int i = 0; i < config->num_opts; i++) {
        struct m_config_option *co = &sorted[i];
        const struct m_option *opt = co->opt;
        if (opt->type->flags & M_OPT_TYPE_HAS_CHILD)
            continue;
        if (co->is_generated)
            continue;
        MP_INFO(config, " %s%-30.30s", prefix, co->name);
        if (opt->type == &m_option_type_choice) {
            MP_INFO(config, " Choices:");
            struct m_opt_choice_alternatives *alt = opt->priv;
            for (int n = 0; alt[n].name; n++)
                MP_INFO(config, " %s", alt[n].name);
            if (opt->flags & (M_OPT_MIN | M_OPT_MAX))
                MP_INFO(config, " (or an integer)");
        } else {
            MP_INFO(config, " %s", co->opt->type->name);
        }
        if (opt->flags & (M_OPT_MIN | M_OPT_MAX)) {
            snprintf(min, sizeof(min), "any");
            snprintf(max, sizeof(max), "any");
            if (opt->flags & M_OPT_MIN)
                snprintf(min, sizeof(min), "%.14g", opt->min);
            if (opt->flags & M_OPT_MAX)
                snprintf(max, sizeof(max), "%.14g", opt->max);
            MP_INFO(config, " (%s to %s)", min, max);
        }
        char *def = NULL;
        if (co->default_data)
            def = m_option_print(co->opt, co->default_data);
        if (def) {
            MP_INFO(config, " (default: %s)", def);
            talloc_free(def);
        }
        if (opt->flags & M_OPT_GLOBAL)
            MP_INFO(config, " [global]");
        if (opt->flags & M_OPT_NOCFG)
            MP_INFO(config, " [nocfg]");
        MP_INFO(config, "\n");
        count++;
    }
    MP_INFO(config, "\nTotal: %d options\n", count);
    talloc_free(sorted);
}

char **m_config_list_options(void *ta_parent, const struct m_config *config)
{
    char **list = talloc_new(ta_parent);
    int count = 0;
    for (int i = 0; i < config->num_opts; i++) {
        struct m_config_option *co = &config->opts[i];
        const struct m_option *opt = co->opt;
        if (opt->type->flags & M_OPT_TYPE_HAS_CHILD)
            continue;
        if (co->is_generated)
            continue;
        // For use with CONF_TYPE_STRING_LIST, it's important not to set list
        // as allocation parent.
        char *s = talloc_strdup(ta_parent, co->name);
        MP_TARRAY_APPEND(ta_parent, list, count, s);
    }
    MP_TARRAY_APPEND(ta_parent, list, count, NULL);
    return list;
}

struct m_profile *m_config_get_profile(const struct m_config *config, bstr name)
{
    for (struct m_profile *p = config->profiles; p; p = p->next) {
        if (bstr_equals0(name, p->name))
            return p;
    }
    return NULL;
}

struct m_profile *m_config_get_profile0(const struct m_config *config,
                                        char *name)
{
    return m_config_get_profile(config, bstr0(name));
}

struct m_profile *m_config_add_profile(struct m_config *config, char *name)
{
    if (!name || !name[0] || strcmp(name, "default") == 0)
        return NULL; // never a real profile
    struct m_profile *p = m_config_get_profile0(config, name);
    if (p)
        return p;
    p = talloc_zero(config, struct m_profile);
    p->name = talloc_strdup(p, name);
    p->next = config->profiles;
    config->profiles = p;
    return p;
}

void m_profile_set_desc(struct m_profile *p, bstr desc)
{
    talloc_free(p->desc);
    p->desc = bstrdup0(p, desc);
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

void m_config_set_profile(struct m_config *config, struct m_profile *p,
                          int flags)
{
    if (config->profile_depth > MAX_PROFILE_DEPTH) {
        MP_WARN(config, "WARNING: Profile inclusion too deep.\n");
        return;
    }
    config->profile_depth++;
    for (int i = 0; i < p->num_opts; i++) {
        m_config_set_option_ext(config,
                                bstr0(p->opts[2 * i]),
                                bstr0(p->opts[2 * i + 1]),
                                flags | M_SETOPT_FROM_CONFIG_FILE);
    }
    config->profile_depth--;
}

void *m_config_alloc_struct(void *talloc_ctx,
                            const struct m_sub_options *subopts)
{
    void *substruct = talloc_zero_size(talloc_ctx, subopts->size);
    if (subopts->defaults)
        memcpy(substruct, subopts->defaults, subopts->size);
    return substruct;
}
