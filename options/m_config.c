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

/// \file
/// \ingroup Config

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>

#include "libmpv/client.h"

#include "mpv_talloc.h"

#include "m_config.h"
#include "options/m_option.h"
#include "common/common.h"
#include "common/global.h"
#include "common/msg.h"
#include "common/msg_control.h"
#include "misc/dispatch.h"
#include "misc/node.h"
#include "osdep/atomic.h"

extern const char mp_help_text[];

static const union m_option_value default_value;

// Profiles allow to predefine some sets of options that can then
// be applied later on with the internal -profile option.
#define MAX_PROFILE_DEPTH 20
// Maximal include depth.
#define MAX_RECURSION_DEPTH 8

// For use with m_config_cache.
struct m_config_shadow {
    pthread_mutex_t lock;
    struct m_config *root;
    char *data;
    struct m_config_cache **listeners;
    int num_listeners;
};

// Represents a sub-struct (OPT_SUBSTRUCT()).
struct m_config_group {
    const struct m_sub_options *group; // or NULL for top-level options
    int parent_group;   // index of parent group in m_config.groups
    void *opts;         // pointer to group user option struct
    atomic_llong ts;    // incremented on every write access
};

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

static int show_profile(struct m_config *config, bstr param)
{
    struct m_profile *p;
    if (!param.len)
        return M_OPT_MISSING_PARAM;
    if (!(p = m_config_get_profile(config, param))) {
        MP_ERR(config, "Unknown profile '%.*s'.\n", BSTR_P(param));
        return M_OPT_EXIT;
    }
    if (!config->profile_depth)
        MP_INFO(config, "Profile %s: %s\n", p->name,
                p->desc ? p->desc : "");
    config->profile_depth++;
    for (int i = 0; i < p->num_opts; i++) {
        MP_INFO(config, "%*s%s=%s\n", config->profile_depth, "",
                p->opts[2 * i], p->opts[2 * i + 1]);

        if (config->profile_depth < MAX_PROFILE_DEPTH
            && !strcmp(p->opts[2*i], "profile")) {
            char *e, *list = p->opts[2 * i + 1];
            while ((e = strchr(list, ','))) {
                int l = e - list;
                if (!l)
                    continue;
                show_profile(config, (bstr){list, e - list});
                list = e + 1;
            }
            if (list[0] != '\0')
                show_profile(config, bstr0(list));
        }
    }
    config->profile_depth--;
    if (!config->profile_depth)
        MP_INFO(config, "\n");
    return M_OPT_EXIT;
}

// The memcpys are supposed to work around the strict aliasing violation,
// that would result if we just dereferenced a void** (where the void** is
// actually casted from struct some_type* ). The dummy struct type is in
// theory needed, because void* and struct pointers could have different
// representations, while pointers to different struct types don't.
static void *substruct_read_ptr(const void *ptr)
{
    struct mp_dummy_ *res;
    memcpy(&res, ptr, sizeof(res));
    return res;
}
static void substruct_write_ptr(void *ptr, void *val)
{
    struct mp_dummy_ *src = val;
    memcpy(ptr, &src, sizeof(src));
}

static void add_options(struct m_config *config,
                        struct m_config_option *parent,
                        void *optstruct,
                        const void *optstruct_def,
                        const struct m_option *defs);

static void config_destroy(void *p)
{
    struct m_config *config = p;
    m_config_restore_backups(config);
    for (int n = 0; n < config->num_opts; n++) {
        struct m_config_option *co = &config->opts[n];

        m_option_free(co->opt, co->data);

        if (config->shadow && co->shadow_offset >= 0)
            m_option_free(co->opt, config->shadow->data + co->shadow_offset);
    }

    if (config->shadow) {
        // must all have been unregistered
        assert(config->shadow->num_listeners == 0);
        pthread_mutex_destroy(&config->shadow->lock);
    }
}

struct m_config *m_config_new(void *talloc_ctx, struct mp_log *log,
                              size_t size, const void *defaults,
                              const struct m_option *options)
{
    struct m_config *config = talloc(talloc_ctx, struct m_config);
    talloc_set_destructor(config, config_destroy);
    *config = (struct m_config)
        {.log = log, .size = size, .defaults = defaults, .options = options};

    // size==0 means a dummy object is created
    if (size) {
        config->optstruct = talloc_zero_size(config, size);
        if (defaults)
            memcpy(config->optstruct, defaults, size);
    }

    config->num_groups = 1;
    MP_TARRAY_GROW(config, config->groups, 1);
    config->groups[0] = (struct m_config_group){
        .parent_group = -1,
        .opts = config->optstruct,
    };

    if (options)
        add_options(config, NULL, config->optstruct, defaults, options);
    return config;
}

static struct m_config *m_config_from_obj_desc(void *talloc_ctx,
                                               struct mp_log *log,
                                               struct mpv_global *global,
                                               struct m_obj_desc *desc)
{
    struct m_config *c =
        m_config_new(talloc_ctx, log, desc->priv_size, desc->priv_defaults,
                     desc->options);
    c->global = global;
    if (desc->set_defaults && c->global)
        desc->set_defaults(c->global, c->optstruct);
    return c;
}

// Like m_config_from_obj_desc(), but don't allocate option struct.
struct m_config *m_config_from_obj_desc_noalloc(void *talloc_ctx,
                                                struct mp_log *log,
                                                struct m_obj_desc *desc)
{
    return m_config_new(talloc_ctx, log, 0, desc->priv_defaults, desc->options);
}

static struct m_config_group *find_group(struct mpv_global *global,
                                         const struct m_option *cfg)
{
    struct m_config_shadow *shadow = global->config;
    struct m_config *root = shadow->root;

    for (int n = 0; n < root->num_groups; n++) {
        if (cfg && root->groups[n].group && root->groups[n].group->opts == cfg)
            return &root->groups[n];
    }

    return NULL;
}

// Allocate a priv struct that is backed by global options (like AOs and VOs,
// anything that uses m_obj_list.use_global_options == true).
// The result contains a snapshot of the current option values of desc->options.
// For convenience, desc->options can be NULL; then priv struct is allocated
// with just zero (or priv_defaults if set).
void *m_config_group_from_desc(void *ta_parent, struct mp_log *log,
        struct mpv_global *global, struct m_obj_desc *desc, const char *name)
{
    struct m_config_group *group = find_group(global, desc->options);
    if (group) {
        return mp_get_config_group(ta_parent, global, group->group);
    } else {
        void *d = talloc_zero_size(ta_parent, desc->priv_size);
        if (desc->priv_defaults)
            memcpy(d, desc->priv_defaults, desc->priv_size);
        return d;
    }
}

static int m_config_set_obj_params(struct m_config *config, struct mp_log *log,
                                   struct mpv_global *global,
                                   struct m_obj_desc *desc, char **args)
{
    for (int n = 0; args && args[n * 2 + 0]; n++) {
        bstr opt = bstr0(args[n * 2 + 0]);
        bstr val = bstr0(args[n * 2 + 1]);
        if (m_config_set_option_cli(config, opt, val, 0) < 0)
            return -1;
    }

    return 0;
}

struct m_config *m_config_from_obj_desc_and_args(void *ta_parent,
    struct mp_log *log, struct mpv_global *global, struct m_obj_desc *desc,
    const char *name, struct m_obj_settings *defaults, char **args)
{
    struct m_config *config = m_config_from_obj_desc(ta_parent, log, global, desc);

    for (int n = 0; defaults && defaults[n].name; n++) {
        struct m_obj_settings *entry = &defaults[n];
        if (name && strcmp(entry->name, name) == 0) {
            if (m_config_set_obj_params(config, log, global, desc, entry->attribs) < 0)
                goto error;
        }
    }

    if (m_config_set_obj_params(config, log, global, desc, args) < 0)
        goto error;

    return config;
error:
    talloc_free(config);
    return NULL;
}

static void ensure_backup(struct m_config *config, struct m_config_option *co)
{
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
    co->is_set_locally = true;
}

void m_config_restore_backups(struct m_config *config)
{
    while (config->backup_opts) {
        struct m_opt_backup *bc = config->backup_opts;
        config->backup_opts = bc->next;

        m_config_set_option_raw(config, bc->co, bc->backup, 0);

        m_option_free(bc->co->opt, bc->backup);
        bc->co->is_set_locally = false;
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

static void m_config_add_option(struct m_config *config,
                                struct m_config_option *parent,
                                void *optstruct,
                                const void *optstruct_def,
                                const struct m_option *arg);

static void add_options(struct m_config *config,
                        struct m_config_option *parent,
                        void *optstruct,
                        const void *optstruct_def,
                        const struct m_option *defs)
{
    for (int i = 0; defs && defs[i].name; i++)
        m_config_add_option(config, parent, optstruct, optstruct_def, &defs[i]);
}

static void add_sub_options(struct m_config *config,
                            struct m_config_option *parent,
                            const struct m_sub_options *subopts)
{
    // Can't be used multiple times.
    for (int n = 0; n < config->num_groups; n++)
        assert(config->groups[n].group != subopts);

    // You can only use UPDATE_ flags here.
    assert(!(subopts->change_flags & ~(unsigned)UPDATE_OPTS_MASK));

    void *new_optstruct = NULL;
    if (config->optstruct) { // only if not noalloc
        new_optstruct = talloc_zero_size(config, subopts->size);
        if (subopts->defaults)
            memcpy(new_optstruct, subopts->defaults, subopts->size);
    }
    if (parent && parent->data)
        substruct_write_ptr(parent->data, new_optstruct);

    const void *new_optstruct_def = NULL;
    if (parent && parent->default_data)
        new_optstruct_def = substruct_read_ptr(parent->default_data);
    if (!new_optstruct_def)
        new_optstruct_def = subopts->defaults;

    int group = config->num_groups++;
    MP_TARRAY_GROW(config, config->groups, group);
    config->groups[group] = (struct m_config_group){
        .group = subopts,
        .parent_group = parent ? parent->group : 0,
        .opts = new_optstruct,
    };

    struct m_config_option next = {
        .name = "",
        .group = group,
    };
    if (parent && parent->name && parent->name[0])
        next.name = parent->name;
    if (subopts->prefix && subopts->prefix[0]) {
        assert(next.name);
        next.name = subopts->prefix;
    }
    add_options(config, &next, new_optstruct, new_optstruct_def, subopts->opts);
}

#define MAX_VO_AO 16

struct group_entry {
    const struct m_obj_list *entry;
    struct m_sub_options subs[MAX_VO_AO];
    bool initialized;
};

static struct group_entry g_groups[2]; // limited by max. m_obj_list overall
static int g_num_groups = 0;
static pthread_mutex_t g_group_mutex = PTHREAD_MUTEX_INITIALIZER;

static const struct m_sub_options *get_cached_group(const struct m_obj_list *list,
                                                    int n, struct m_sub_options *v)
{
    pthread_mutex_lock(&g_group_mutex);

    struct group_entry *group = NULL;
    for (int i = 0; i < g_num_groups; i++) {
        if (g_groups[i].entry == list) {
            group = &g_groups[i];
            break;
        }
    }
    if (!group) {
        assert(g_num_groups < MP_ARRAY_SIZE(g_groups));
        group = &g_groups[g_num_groups++];
        group->entry = list;
    }

    if (!group->initialized) {
        if (!v) {
            n = -1;
            group->initialized = true;
        } else {
            assert(n < MAX_VO_AO); // simply increase this if it fails
            group->subs[n] = *v;
        }
    }

    pthread_mutex_unlock(&g_group_mutex);

    return n >= 0 ? &group->subs[n] : NULL;
}

static void init_obj_settings_list(struct m_config *config,
                                   const struct m_obj_list *list)
{
    struct m_obj_desc desc;
    for (int n = 0; ; n++) {
        if (!list->get_desc(&desc, n)) {
            if (list->use_global_options)
                get_cached_group(list, n, NULL);
            break;
        }
        if (desc.global_opts)
            add_sub_options(config, NULL, desc.global_opts);
        if (list->use_global_options && desc.options) {
            struct m_sub_options conf = {
                .prefix = desc.options_prefix,
                .opts = desc.options,
                .defaults = desc.priv_defaults,
                .size = desc.priv_size,
            };
            add_sub_options(config, NULL, get_cached_group(list, n, &conf));
        }
    }
}

// Initialize a field with a given value. In case this is dynamic data, it has
// to be allocated and copied. src can alias dst, also can be NULL.
static void init_opt_inplace(const struct m_option *opt, void *dst,
                             const void *src)
{
    union m_option_value temp = {0};
    if (src)
        memcpy(&temp, src, opt->type->size);
    memset(dst, 0, opt->type->size);
    m_option_copy(opt, dst, &temp);
}

static void m_config_add_option(struct m_config *config,
                                struct m_config_option *parent,
                                void *optstruct,
                                const void *optstruct_def,
                                const struct m_option *arg)
{
    assert(config != NULL);
    assert(arg != NULL);

    const char *parent_name = parent ? parent->name : "";

    struct m_config_option co = {
        .opt = arg,
        .name = arg->name,
        .shadow_offset = -1,
        .group = parent ? parent->group : 0,
        .default_data = &default_value,
        .is_hidden = !!arg->deprecation_message,
    };

    if (arg->offset >= 0) {
        if (optstruct)
            co.data = (char *)optstruct + arg->offset;
        if (optstruct_def)
            co.default_data = (char *)optstruct_def + arg->offset;
    }

    if (arg->defval)
        co.default_data = arg->defval;

    // Fill in the full name
    if (!co.name[0]) {
        co.name = parent_name;
    } else if (parent_name[0]) {
        co.name = talloc_asprintf(config, "%s-%s", parent_name, co.name);
    }

    if (arg->type == &m_option_type_subconfig) {
        const struct m_sub_options *subopts = arg->priv;
        add_sub_options(config, &co, subopts);
    } else {
        int size = arg->type->size;
        if (optstruct && size) {
            // The required alignment is unknown, so go with the maximum C
            // could require. Slightly wasteful, but not that much.
            int align = (size - config->shadow_size % size) % size;
            int offset = config->shadow_size + align;
            assert(offset <= INT16_MAX);
            co.shadow_offset = offset;
            config->shadow_size = co.shadow_offset + size;
        }

        // Initialize options
        if (co.data && co.default_data)
            init_opt_inplace(arg, co.data, co.default_data);

        MP_TARRAY_APPEND(config, config->opts, config->num_opts, co);

        if (arg->type == &m_option_type_obj_settings_list)
            init_obj_settings_list(config, (const struct m_obj_list *)arg->priv);
    }
}

struct m_config_option *m_config_get_co_raw(const struct m_config *config,
                                            struct bstr name)
{
    if (!name.len)
        return NULL;

    for (int n = 0; n < config->num_opts; n++) {
        struct m_config_option *co = &config->opts[n];
        struct bstr coname = bstr0(co->name);
        if (bstrcmp(coname, name) == 0)
            return co;
    }

    return NULL;
}

// Like m_config_get_co_raw(), but resolve aliases.
static struct m_config_option *m_config_get_co_any(const struct m_config *config,
                                                   struct bstr name)
{
    struct m_config_option *co = m_config_get_co_raw(config, name);
    if (!co)
        return NULL;

    const char *prefix = config->is_toplevel ? "--" : "";
    if (co->opt->type == &m_option_type_alias) {
        const char *alias = (const char *)co->opt->priv;
        if (co->opt->deprecation_message && !co->warning_was_printed) {
            if (co->opt->deprecation_message[0]) {
                MP_WARN(config, "Warning: option %s%s was replaced with "
                        "%s%s: %s\n", prefix, co->name, prefix, alias,
                        co->opt->deprecation_message);
            } else {
                MP_WARN(config, "Warning: option %s%s was replaced with "
                        "%s%s and might be removed in the future.\n",
                        prefix, co->name, prefix, alias);
            }
            co->warning_was_printed = true;
        }
        return m_config_get_co_any(config, bstr0(alias));
    } else if (co->opt->type == &m_option_type_removed) {
        if (!co->warning_was_printed) {
            char *msg = co->opt->priv;
            if (msg) {
                MP_FATAL(config, "Option %s%s was removed: %s\n",
                         prefix, co->name, msg);
            } else {
                MP_FATAL(config, "Option %s%s was removed.\n",
                         prefix, co->name);
            }
            co->warning_was_printed = true;
        }
        return NULL;
    } else if (co->opt->deprecation_message) {
        if (!co->warning_was_printed) {
            MP_WARN(config, "Warning: option %s%s is deprecated "
                    "and might be removed in the future (%s).\n",
                    prefix, co->name, co->opt->deprecation_message);
            co->warning_was_printed = true;
        }
    }
    return co;
}

struct m_config_option *m_config_get_co(const struct m_config *config,
                                        struct bstr name)
{
    struct m_config_option *co = m_config_get_co_any(config, name);
    // CLI aliases should not be real options, and are explicitly handled by
    // m_config_set_option_cli(). So pretend it does not exist.
    if (co && co->opt->type == &m_option_type_cli_alias)
        co = NULL;
    return co;
}

int m_config_get_co_count(struct m_config *config)
{
    return config->num_opts;
}

struct m_config_option *m_config_get_co_index(struct m_config *config, int index)
{
    return &config->opts[index];
}

const char *m_config_get_positional_option(const struct m_config *config, int p)
{
    int pos = 0;
    for (int n = 0; n < config->num_opts; n++) {
        struct m_config_option *co = &config->opts[n];
        if (!co->is_hidden) {
            if (pos == p)
                return co->name;
            pos++;
        }
    }
    return NULL;
}

// return: <0: M_OPT_ error, 0: skip, 1: check, 2: set
static int handle_set_opt_flags(struct m_config *config,
                                struct m_config_option *co, int flags)
{
    int optflags = co->opt->flags;
    bool set = !(flags & M_SETOPT_CHECK_ONLY);

    if ((flags & M_SETOPT_PRE_PARSE_ONLY) && !(optflags & M_OPT_PRE_PARSE))
        return 0;

    if ((flags & M_SETOPT_PRESERVE_CMDLINE) && co->is_set_from_cmdline)
        set = false;

    if ((flags & M_SETOPT_NO_OVERWRITE) &&
        (co->is_set_from_cmdline || co->is_set_from_config))
        set = false;

    if ((flags & M_SETOPT_NO_FIXED) && (optflags & M_OPT_FIXED))
        return M_OPT_INVALID;

    if ((flags & M_SETOPT_NO_PRE_PARSE) && (optflags & M_OPT_PRE_PARSE))
        return M_OPT_INVALID;

    // Check if this option isn't forbidden in the current mode
    if ((flags & M_SETOPT_FROM_CONFIG_FILE) && (optflags & M_OPT_NOCFG)) {
        MP_ERR(config, "The %s option can't be used in a config file.\n",
               co->name);
        return M_OPT_INVALID;
    }
    if ((flags & M_SETOPT_BACKUP) && set)
        ensure_backup(config, co);

    return set ? 2 : 1;
}

void m_config_mark_co_flags(struct m_config_option *co, int flags)
{
    if (flags & M_SETOPT_FROM_CMDLINE)
        co->is_set_from_cmdline = true;

    if (flags & M_SETOPT_FROM_CONFIG_FILE)
        co->is_set_from_config = true;
}

// Special options that don't really fit into the option handling mode. They
// usually store no data, but trigger actions. Caller is assumed to have called
// handle_set_opt_flags() to make sure the option can be set.
// Returns M_OPT_UNKNOWN if the option is not a special option.
static int m_config_handle_special_options(struct m_config *config,
                                           struct m_config_option *co,
                                           void *data, int flags)
{
    if (config->use_profiles && strcmp(co->name, "profile") == 0) {
        char **list = *(char ***)data;

        if (list && list[0] && !list[1] && strcmp(list[0], "help") == 0) {
            if (!config->profiles) {
                MP_INFO(config, "No profiles have been defined.\n");
                return M_OPT_EXIT;
            }
            MP_INFO(config, "Available profiles:\n");
            for (struct m_profile *p = config->profiles; p; p = p->next)
                MP_INFO(config, "\t%s\t%s\n", p->name, p->desc ? p->desc : "");
            MP_INFO(config, "\n");
            return M_OPT_EXIT;
        }

        for (int n = 0; list && list[n]; n++) {
            int r = m_config_set_profile(config, list[n], flags);
            if (r < 0)
                return r;
        }
        return 0;
    }

    if (config->includefunc && strcmp(co->name, "include") == 0) {
        char *param = *(char **)data;
        if (!param || !param[0])
            return M_OPT_MISSING_PARAM;
        if (config->recursion_depth >= MAX_RECURSION_DEPTH) {
            MP_ERR(config, "Maximum 'include' nesting depth exceeded.\n");
            return M_OPT_INVALID;
        }
        config->recursion_depth += 1;
        config->includefunc(config->includefunc_ctx, param, flags);
        config->recursion_depth -= 1;
        if (config->recursion_depth == 0 && config->profile_depth == 0)
            m_config_finish_default_profile(config, flags);
        return 1;
    }

    if (config->use_profiles && strcmp(co->name, "show-profile") == 0)
        return show_profile(config, bstr0(*(char **)data));

    if (config->is_toplevel && (strcmp(co->name, "h") == 0 ||
                                strcmp(co->name, "help") == 0))
    {
        char *h = *(char **)data;
        mp_info(config->log, "%s", mp_help_text);
        if (h && h[0])
            m_config_print_option_list(config, h);
        return M_OPT_EXIT;
    }

    if (strcmp(co->name, "list-options") == 0) {
        m_config_print_option_list(config, "*");
        return M_OPT_EXIT;
    }

    return M_OPT_UNKNOWN;
}


// Unlike m_config_set_option_raw() this does not go through the property layer
// via config.option_set_callback.
int m_config_set_option_raw_direct(struct m_config *config,
                                   struct m_config_option *co,
                                   void *data, int flags)
{
    if (!co)
        return M_OPT_UNKNOWN;

    int r = handle_set_opt_flags(config, co, flags);
    if (r <= 1)
        return r;

    r = m_config_handle_special_options(config, co, data, flags);
    if (r != M_OPT_UNKNOWN)
        return r;

    // This affects some special options like "playlist", "v". Maybe these
    // should work, or maybe not. For now they would require special code.
    if (!co->data)
        return flags & M_SETOPT_FROM_CMDLINE ? 0 : M_OPT_UNKNOWN;

    m_option_copy(co->opt, co->data, data);

    m_config_mark_co_flags(co, flags);
    m_config_notify_change_co(config, co);

    return 0;
}

// Similar to m_config_set_option_cli(), but set as data in its native format.
// This takes care of some details like sending change notifications.
// The type data points to is as in: co->opt
int m_config_set_option_raw(struct m_config *config, struct m_config_option *co,
                            void *data, int flags)
{
    if (!co)
        return M_OPT_UNKNOWN;

    if (config->option_set_callback) {
        int r = handle_set_opt_flags(config, co, flags);
        if (r <= 1)
            return r;

        return config->option_set_callback(config->option_set_callback_cb,
                                           co, data, flags);
    } else {
        return m_config_set_option_raw_direct(config, co, data, flags);
    }
}

// Handle CLI exceptions to option handling.
// Used to turn "--no-foo" into "--foo=no".
// It also handles looking up "--vf-add" as "--vf".
static struct m_config_option *m_config_mogrify_cli_opt(struct m_config *config,
                                                        struct bstr *name,
                                                        bool *out_negate,
                                                        int *out_add_flags)
{
    *out_negate = false;
    *out_add_flags = 0;

    struct m_config_option *co = m_config_get_co(config, *name);
    if (co)
        return co;

    // Turn "--no-foo" into "foo" + set *out_negate.
    bstr no_name = *name;
    if (!co && bstr_eatstart0(&no_name, "no-")) {
        co = m_config_get_co(config, no_name);

        // Not all choice types have this value - if they don't, then parsing
        // them will simply result in an error. Good enough.
        if (!co || !(co->opt->type->flags & M_OPT_TYPE_CHOICE))
            return NULL;

        *name = no_name;
        *out_negate = true;
        return co;
    }

    // Resolve CLI alias. (We don't allow you to combine them with "--no-".)
    co = m_config_get_co_any(config, *name);
    if (co && co->opt->type == &m_option_type_cli_alias)
        *name = bstr0((char *)co->opt->priv);

    // Might be a suffix "action", like "--vf-add". Expensively check for
    // matches. (We don't allow you to combine them with "--no-".)
    for (int n = 0; n < config->num_opts; n++) {
        co = &config->opts[n];
        struct bstr basename = bstr0(co->name);

        if (!bstr_startswith(*name, basename))
            continue;

        // Aliased option + a suffix action, e.g. --opengl-shaders-append
        if (co->opt->type == &m_option_type_alias)
            co = m_config_get_co_any(config, basename);
        if (!co)
            continue;

        const struct m_option_type *type = co->opt->type;
        for (int i = 0; type->actions && type->actions[i].name; i++) {
            const struct m_option_action *action = &type->actions[i];
            bstr suffix = bstr0(action->name);

            if (bstr_endswith(*name, suffix) &&
                (name->len == basename.len + 1 + suffix.len) &&
                name->start[basename.len] == '-')
            {
                *out_add_flags = action->flags;
                return co;
            }
        }
    }

    return NULL;
}

// Set the named option to the given string. This is for command line and config
// file use only.
// flags: combination of M_SETOPT_* flags (0 for normal operation)
// Returns >= 0 on success, otherwise see OptionParserReturn.
int m_config_set_option_cli(struct m_config *config, struct bstr name,
                            struct bstr param, int flags)
{
    int r;
    assert(config != NULL);

    bool negate;
    struct m_config_option *co =
        m_config_mogrify_cli_opt(config, &name, &negate, &(int){0});

    if (!co) {
        r = M_OPT_UNKNOWN;
        goto done;
    }

    if (negate) {
        if (param.len) {
            r = M_OPT_DISALLOW_PARAM;
            goto done;
        }

        param = bstr0("no");
    }

    // This is the only mandatory function
    assert(co->opt->type->parse);

    r = handle_set_opt_flags(config, co, flags);
    if (r <= 0)
        goto done;

    if (r == 2) {
        MP_DBG(config, "Setting option '%.*s' = '%.*s' (flags = %d)\n",
               BSTR_P(name), BSTR_P(param), flags);
    }

    union m_option_value val = {0};

    // Some option types are "impure" and work on the existing data.
    // (Prime examples: --vf-add, --sub-file)
    if (co->data)
        m_option_copy(co->opt, &val, co->data);

    r = m_option_parse(config->log, co->opt, name, param, &val);

    if (r >= 0)
        r = m_config_set_option_raw(config, co, &val, flags);

    m_option_free(co->opt, &val);

done:
    if (r < 0 && r != M_OPT_EXIT) {
        MP_ERR(config, "Error parsing option %.*s (%s)\n",
               BSTR_P(name), m_option_strerror(r));
        r = M_OPT_INVALID;
    }
    return r;
}

int m_config_set_option_node(struct m_config *config, bstr name,
                             struct mpv_node *data, int flags)
{
    int r;

    struct m_config_option *co = m_config_get_co(config, name);
    if (!co)
        return M_OPT_UNKNOWN;

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
        r = m_config_set_option_raw(config, co, &val, flags);

    if (mp_msg_test(config->log, MSGL_V)) {
        char *s = m_option_type_node.print(NULL, data);
        MP_DBG(config, "Setting option '%.*s' = %s (flags = %d) -> %d\n",
               BSTR_P(name), s ? s : "?", flags, r);
        talloc_free(s);
    }

    m_option_free(co->opt, &val);
    return r;
}

int m_config_option_requires_param(struct m_config *config, bstr name)
{
    bool negate;
    int flags;
    struct m_config_option *co =
        m_config_mogrify_cli_opt(config, &name, &negate, &flags);

    if (!co)
        return M_OPT_UNKNOWN;

    if (negate || (flags & M_OPT_TYPE_OPTIONAL_PARAM))
        return 0;

    return m_option_required_params(co->opt);
}

static int sort_opt_compare(const void *pa, const void *pb)
{
    const struct m_config_option *a = pa;
    const struct m_config_option *b = pb;
    return strcasecmp(a->name, b->name);
}

void m_config_print_option_list(const struct m_config *config, const char *name)
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
        if (co->is_hidden)
            continue;
        if (strcmp(name, "*") != 0 && !strstr(co->name, name))
            continue;
        MP_INFO(config, " %s%-30s", prefix, co->name);
        if (opt->type == &m_option_type_choice) {
            MP_INFO(config, " Choices:");
            struct m_opt_choice_alternatives *alt = opt->priv;
            for (int n = 0; alt[n].name; n++)
                MP_INFO(config, " %s", alt[n].name);
            if (opt->flags & (M_OPT_MIN | M_OPT_MAX))
                MP_INFO(config, " (or an integer)");
        } else {
            MP_INFO(config, " %s", opt->type->name);
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
            def = m_option_pretty_print(opt, co->default_data);
        if (def) {
            MP_INFO(config, " (default: %s)", def);
            talloc_free(def);
        }
        if (opt->flags & M_OPT_NOCFG)
            MP_INFO(config, " [not in config files]");
        if (opt->flags & M_OPT_FILE)
            MP_INFO(config, " [file]");
        if (opt->flags & M_OPT_FIXED)
            MP_INFO(config, " [no runtime changes]");
        if (opt->type == &m_option_type_alias)
            MP_INFO(config, " for %s", (char *)opt->priv);
        if (opt->type == &m_option_type_cli_alias)
            MP_INFO(config, " for --%s (CLI/config files only)", (char *)opt->priv);
        MP_INFO(config, "\n");
        for (int n = 0; opt->type->actions && opt->type->actions[n].name; n++) {
            const struct m_option_action *action = &opt->type->actions[n];
            MP_INFO(config, "    %s%s-%s\n", prefix, co->name, action->name);
            count++;
        }
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
        if (co->is_hidden)
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
    if (!name || !name[0])
        name = "default";
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
    p->desc = bstrto0(p, desc);
}

int m_config_set_profile_option(struct m_config *config, struct m_profile *p,
                                bstr name, bstr val)
{
    int i = m_config_set_option_cli(config, name, val,
                                    M_SETOPT_CHECK_ONLY |
                                    M_SETOPT_FROM_CONFIG_FILE);
    if (i < 0)
        return i;
    p->opts = talloc_realloc(p, p->opts, char *, 2 * (p->num_opts + 2));
    p->opts[p->num_opts * 2] = bstrto0(p, name);
    p->opts[p->num_opts * 2 + 1] = bstrto0(p, val);
    p->num_opts++;
    p->opts[p->num_opts * 2] = p->opts[p->num_opts * 2 + 1] = NULL;
    return 1;
}

int m_config_set_profile(struct m_config *config, char *name, int flags)
{
    struct m_profile *p = m_config_get_profile0(config, name);
    if (!p) {
        MP_WARN(config, "Unknown profile '%s'.\n", name);
        return M_OPT_INVALID;
    }

    if (config->profile_depth > MAX_PROFILE_DEPTH) {
        MP_WARN(config, "WARNING: Profile inclusion too deep.\n");
        return M_OPT_INVALID;
    }
    config->profile_depth++;
    for (int i = 0; i < p->num_opts; i++) {
        m_config_set_option_cli(config,
                                bstr0(p->opts[2 * i]),
                                bstr0(p->opts[2 * i + 1]),
                                flags | M_SETOPT_FROM_CONFIG_FILE);
    }
    config->profile_depth--;

    return 0;
}

void m_config_finish_default_profile(struct m_config *config, int flags)
{
    struct m_profile *p = m_config_add_profile(config, NULL);
    m_config_set_profile(config, p->name, flags);
    p->num_opts = 0;
}

struct mpv_node m_config_get_profiles(struct m_config *config)
{
    struct mpv_node root;
    node_init(&root, MPV_FORMAT_NODE_ARRAY, NULL);

    for (m_profile_t *profile = config->profiles; profile; profile = profile->next)
    {
        struct mpv_node *entry = node_array_add(&root, MPV_FORMAT_NODE_MAP);

        node_map_add_string(entry, "name", profile->name);
        if (profile->desc)
            node_map_add_string(entry, "profile-desc", profile->desc);

        struct mpv_node *opts =
            node_map_add(entry, "options", MPV_FORMAT_NODE_ARRAY);

        for (int n = 0; n < profile->num_opts; n++) {
            struct mpv_node *opt_entry = node_array_add(opts, MPV_FORMAT_NODE_MAP);
            node_map_add_string(opt_entry, "key", profile->opts[n * 2 + 0]);
            node_map_add_string(opt_entry, "value", profile->opts[n * 2 + 1]);
        }
    }

    return root;
}

void m_config_create_shadow(struct m_config *config)
{
    assert(config->global && config->options && config->size);
    assert(!config->shadow && !config->global->config);

    config->shadow = talloc_zero(config, struct m_config_shadow);
    config->shadow->data = talloc_zero_size(config->shadow, config->shadow_size);

    config->shadow->root = config;
    pthread_mutex_init(&config->shadow->lock, NULL);

    config->global->config = config->shadow;

    for (int n = 0; n < config->num_opts; n++) {
        struct m_config_option *co = &config->opts[n];
        if (co->shadow_offset < 0)
            continue;
        m_option_copy(co->opt, config->shadow->data + co->shadow_offset, co->data);
    }
}

// Return whether parent is a parent of group. Also returns true if they're equal.
static bool is_group_included(struct m_config *config, int group, int parent)
{
    for (;;) {
        if (group == parent)
            return true;
        if (group < 0)
            break;
        group = config->groups[group].parent_group;
    }
    return false;
}

static void cache_destroy(void *p)
{
    struct m_config_cache *cache = p;

    // (technically speaking, being able to call them both without anything
    // breaking is a feature provided by these functions)
    m_config_cache_set_wakeup_cb(cache, NULL, NULL);
    m_config_cache_set_dispatch_change_cb(cache, NULL, NULL, NULL);
}

struct m_config_cache *m_config_cache_alloc(void *ta_parent,
                                            struct mpv_global *global,
                                            const struct m_sub_options *group)
{
    struct m_config_shadow *shadow = global->config;
    struct m_config *root = shadow->root;

    struct m_config_cache *cache = talloc_zero(ta_parent, struct m_config_cache);
    talloc_set_destructor(cache, cache_destroy);
    cache->shadow = shadow;
    cache->shadow_config = m_config_new(cache, mp_null_log, root->size,
                                        root->defaults, root->options);

    struct m_config *config = cache->shadow_config;

    assert(config->num_opts == root->num_opts);
    for (int n = 0; n < root->num_opts; n++) {
        assert(config->opts[n].opt->type == root->opts[n].opt->type);
        assert(config->opts[n].shadow_offset == root->opts[n].shadow_offset);
    }

    cache->ts = -1;
    cache->group = -1;

    for (int n = 0; n < config->num_groups; n++) {
        if (config->groups[n].group == group) {
            cache->opts = config->groups[n].opts;
            cache->group = n;
            break;
        }
    }

    assert(cache->group >= 0);
    assert(cache->opts);

    // If we're not on the top-level, restrict set of options to the sub-group
    // to reduce update costs. (It would be better not to add them in the first
    // place.)
    if (cache->group > 0) {
        int num_opts = config->num_opts;
        config->num_opts = 0;
        for (int n = 0; n < num_opts; n++) {
            struct m_config_option *co = &config->opts[n];
            if (is_group_included(config, co->group, cache->group)) {
                config->opts[config->num_opts++] = *co;
            } else {
                m_option_free(co->opt, co->data);
            }
        }
        for (int n = 0; n < config->num_groups; n++) {
            if (!is_group_included(config, n, cache->group))
                TA_FREEP(&config->groups[n].opts);
        }
    }

    m_config_cache_update(cache);

    return cache;
}

bool m_config_cache_update(struct m_config_cache *cache)
{
    struct m_config_shadow *shadow = cache->shadow;

    // Using atomics and checking outside of the lock - it's unknown whether
    // this makes it faster or slower. Just cargo culting it.
    if (atomic_load(&shadow->root->groups[cache->group].ts) <= cache->ts)
        return false;

    pthread_mutex_lock(&shadow->lock);
    cache->ts = atomic_load(&shadow->root->groups[cache->group].ts);
    for (int n = 0; n < cache->shadow_config->num_opts; n++) {
        struct m_config_option *co = &cache->shadow_config->opts[n];
        if (co->shadow_offset >= 0)
            m_option_copy(co->opt, co->data, shadow->data + co->shadow_offset);
    }
    pthread_mutex_unlock(&shadow->lock);
    return true;
}

void m_config_notify_change_co(struct m_config *config,
                               struct m_config_option *co)
{
    struct m_config_shadow *shadow = config->shadow;

    if (shadow) {
        pthread_mutex_lock(&shadow->lock);
        if (co->shadow_offset >= 0)
            m_option_copy(co->opt, shadow->data + co->shadow_offset, co->data);
        pthread_mutex_unlock(&shadow->lock);
    }

    int changed = co->opt->flags & UPDATE_OPTS_MASK;

    int group = co->group;
    while (group >= 0) {
        struct m_config_group *g = &config->groups[group];
        atomic_fetch_add(&g->ts, 1);
        if (g->group)
            changed |= g->group->change_flags;
        group = g->parent_group;
    }

    if (shadow) {
        pthread_mutex_lock(&shadow->lock);
        for (int n = 0; n < shadow->num_listeners; n++) {
            struct m_config_cache *cache = shadow->listeners[n];
            if (cache->wakeup_cb)
                cache->wakeup_cb(cache->wakeup_cb_ctx);
        }
        pthread_mutex_unlock(&shadow->lock);
    }

    if (config->option_change_callback) {
        config->option_change_callback(config->option_change_callback_ctx, co,
                                       changed);
    }
}

void m_config_notify_change_opt_ptr(struct m_config *config, void *ptr)
{
    for (int n = 0; n < config->num_opts; n++) {
        struct m_config_option *co = &config->opts[n];
        if (co->data == ptr) {
            m_config_notify_change_co(config, co);
            return;
        }
    }
    // ptr doesn't point to any config->optstruct field declared in the
    // option list?
    assert(false);
}

void m_config_cache_set_wakeup_cb(struct m_config_cache *cache,
                                  void (*cb)(void *ctx), void *cb_ctx)
{
    struct m_config_shadow *shadow = cache->shadow;

    pthread_mutex_lock(&shadow->lock);
    if (cache->in_list) {
        for (int n = 0; n < shadow->num_listeners; n++) {
            if (shadow->listeners[n] == cache)
                MP_TARRAY_REMOVE_AT(shadow->listeners, shadow->num_listeners, n);
        }
        if (!shadow->num_listeners) {
            talloc_free(shadow->listeners);
            shadow->listeners = NULL;
        }
    }
    if (cb) {
        MP_TARRAY_APPEND(NULL, shadow->listeners, shadow->num_listeners, cache);
        cache->in_list = true;
        cache->wakeup_cb = cb;
        cache->wakeup_cb_ctx = cb_ctx;
    }
    pthread_mutex_unlock(&shadow->lock);
}

static void dispatch_notify(void *p)
{
    struct m_config_cache *cache = p;

    assert(cache->wakeup_dispatch_queue);
    mp_dispatch_enqueue_notify(cache->wakeup_dispatch_queue,
                               cache->wakeup_dispatch_cb,
                               cache->wakeup_dispatch_cb_ctx);
}

void m_config_cache_set_dispatch_change_cb(struct m_config_cache *cache,
                                           struct mp_dispatch_queue *dispatch,
                                           void (*cb)(void *ctx), void *cb_ctx)
{
    // Removing the old one is tricky. First make sure no new notifications will
    // come.
    m_config_cache_set_wakeup_cb(cache, NULL, NULL);
    // Remove any pending notifications (assume we're on the same thread as
    // any potential mp_dispatch_queue_process() callers).
    if (cache->wakeup_dispatch_queue) {
        mp_dispatch_cancel_fn(cache->wakeup_dispatch_queue,
                              cache->wakeup_dispatch_cb,
                              cache->wakeup_dispatch_cb_ctx);
    }

    cache->wakeup_dispatch_queue = NULL;
    cache->wakeup_dispatch_cb = NULL;
    cache->wakeup_dispatch_cb_ctx = NULL;

    if (cb) {
        cache->wakeup_dispatch_queue = dispatch;
        cache->wakeup_dispatch_cb = cb;
        cache->wakeup_dispatch_cb_ctx = cb_ctx;
        m_config_cache_set_wakeup_cb(cache, dispatch_notify, cache);
    }
}

void *mp_get_config_group(void *ta_parent, struct mpv_global *global,
                          const struct m_sub_options *group)
{
    struct m_config_cache *cache = m_config_cache_alloc(NULL, global, group);
    // Make talloc_free(cache->opts) free the entire cache.
    ta_set_parent(cache->opts, ta_parent);
    ta_set_parent(cache, cache->opts);
    return cache->opts;
}

void mp_read_option_raw(struct mpv_global *global, const char *name,
                        const struct m_option_type *type, void *dst)
{
    struct m_config_shadow *shadow = global->config;
    struct m_config_option *co = m_config_get_co_raw(shadow->root, bstr0(name));
    assert(co);
    assert(co->shadow_offset >= 0);
    assert(co->opt->type == type);

    memset(dst, 0, co->opt->type->size);
    m_option_copy(co->opt, dst, shadow->data + co->shadow_offset);
}

struct m_config *mp_get_root_config(struct mpv_global *global)
{
    return global->config->root;
}
