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
    struct m_config *root;
    pthread_mutex_t lock;
    // -- protected by lock
    struct m_config_data *data; // protected shadow copy of the option data
    struct m_config_cache **listeners;
    int num_listeners;
};

// Represents a sub-struct (OPT_SUBSTRUCT()).
struct m_config_group {
    const struct m_sub_options *group;
    int group_count;    // 1 + number of all sub groups owned by this (so
                        // m_config.groups[idx..idx+group_count] is used by the
                        // entire tree of sub groups included by this group)
    int parent_group;   // index of parent group into m_config.groups[], or
                        // -1 for group 0
    int parent_ptr;     // ptr offset in the parent group's data, or -1 if
                        // none
    int co_index;       // index of the first group opt into m_config.opts[]
    int co_end_index;   // index of the last group opt + 1 (i.e. exclusive)
};

// A copy of option data. Used for the main option struct, the shadow data,
// and copies for m_config_cache.
struct m_config_data {
    struct m_config *root;          // root config (with up-to-date data)
    int group_index;                // start index into m_config.groups[]
    struct m_group_data *gdata;     // user struct allocation (our copy of data)
    int num_gdata;                  // (group_index+num_gdata = end index)
    atomic_llong ts;                // last change timestamp we've seen
};

// Per m_config_data state for each m_config_group.
struct m_group_data {
    char *udata;        // pointer to group user option struct
    long long ts;       // incremented on every write access
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

static void add_sub_group(struct m_config *config, const char *name_prefix,
                          int parent_group_index, int parent_ptr,
                          const struct m_sub_options *subopts);

static struct m_group_data *m_config_gdata(struct m_config_data *data,
                                           int group_index)
{
    if (group_index < data->group_index ||
        group_index >= data->group_index + data->num_gdata)
        return NULL;

    return &data->gdata[group_index - data->group_index];
}

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

// Initialize a field with a given value. In case this is dynamic data, it has
// to be allocated and copied. src can alias dst.
static void init_opt_inplace(const struct m_option *opt, void *dst,
                             const void *src)
{
    // The option will use dynamic memory allocation iff it has a free callback.
    if (opt->type->free) {
        union m_option_value temp;
        memcpy(&temp, src, opt->type->size);
        memset(dst, 0, opt->type->size);
        m_option_copy(opt, dst, &temp);
    } else if (src != dst) {
        memcpy(dst, src, opt->type->size);
    }
}

static void alloc_group(struct m_config_data *data, int group_index,
                        struct m_config_data *copy)
{
    assert(group_index == data->group_index + data->num_gdata);
    assert(group_index < data->root->num_groups);
    struct m_config_group *group = &data->root->groups[group_index];
    const struct m_sub_options *opts = group->group;

    MP_TARRAY_GROW(data, data->gdata, data->num_gdata);
    struct m_group_data *gdata = &data->gdata[data->num_gdata++];

    struct m_group_data *copy_gdata =
        copy ? m_config_gdata(copy, group_index) : NULL;

    *gdata = (struct m_group_data){
        .udata = talloc_zero_size(data, opts->size),
        .ts = copy_gdata ? copy_gdata->ts : 0,
    };

    if (opts->defaults)
        memcpy(gdata->udata, opts->defaults, opts->size);

    char *copy_src = copy_gdata ? copy_gdata->udata : NULL;

    for (int n = group->co_index; n < group->co_end_index; n++) {
        assert(n >= 0 && n < data->root->num_opts);
        struct m_config_option *co = &data->root->opts[n];

        if (co->opt->offset < 0 || co->opt->type->size == 0)
            continue;

        void *dst = gdata->udata + co->opt->offset;
        const void *defptr = co->opt->defval ? co->opt->defval : dst;
        if (copy_src)
            defptr = copy_src + co->opt->offset;

        init_opt_inplace(co->opt, dst, defptr);
    }

    // If there's a parent, update its pointer to the new struct.
    if (group->parent_group >= data->group_index && group->parent_ptr >= 0) {
        struct m_group_data *parent_gdata =
            m_config_gdata(data, group->parent_group);
        assert(parent_gdata);

        substruct_write_ptr(parent_gdata->udata + group->parent_ptr, gdata->udata);
    }
}

static void free_option_data(void *p)
{
    struct m_config_data *data = p;

    for (int i = 0; i < data->num_gdata; i++) {
        struct m_group_data *gdata = &data->gdata[i];
        struct m_config_group *group = &data->root->groups[data->group_index + i];

        for (int n = group->co_index; n < group->co_end_index; n++) {
            struct m_config_option *co = &data->root->opts[n];

            if (co->opt->offset >= 0 && co->opt->type->size > 0)
                m_option_free(co->opt, gdata->udata + co->opt->offset);
        }
    }
}

// Allocate data using the option description in root, starting at group_index
// (index into m_config.groups[]).
// If copy is not NULL, copy all data from there (for groups which are in both
// m_config_data instances), in all other cases init the data with the defaults.
static struct m_config_data *allocate_option_data(void *ta_parent,
                                                  struct m_config *root,
                                                  int group_index,
                                                  struct m_config_data *copy)
{
    assert(group_index >= 0 && group_index < root->num_groups);
    struct m_config_data *data = talloc_zero(ta_parent, struct m_config_data);
    talloc_set_destructor(data, free_option_data);

    data->root = root;
    data->group_index = group_index;

    struct m_config_group *root_group = &root->groups[group_index];
    assert(root_group->group_count > 0);

    for (int n = group_index; n < group_index + root_group->group_count; n++)
        alloc_group(data, n, copy);

    if (copy)
        data->ts = copy->ts;

    return data;
}

static void config_destroy(void *p)
{
    struct m_config *config = p;
    m_config_restore_backups(config);

    if (config->shadow) {
        // must all have been unregistered
        assert(config->shadow->num_listeners == 0);
        pthread_mutex_destroy(&config->shadow->lock);
        talloc_free(config->shadow);
    }

    talloc_free(config->data);
}

struct m_config *m_config_new(void *talloc_ctx, struct mp_log *log,
                              size_t size, const void *defaults,
                              const struct m_option *options)
{
    struct m_config *config = talloc(talloc_ctx, struct m_config);
    talloc_set_destructor(config, config_destroy);
    *config = (struct m_config){.log = log,};

    struct m_sub_options *subopts = talloc_ptrtype(config, subopts);
    *subopts = (struct m_sub_options){
        .opts = options,
        .size = size,
        .defaults = defaults,
    };
    add_sub_group(config, NULL, -1, -1, subopts);

    if (!size)
        return config;

    config->data = allocate_option_data(config, config, 0, NULL);
    config->optstruct = config->data->gdata[0].udata;

    for (int n = 0; n < config->num_opts; n++) {
        struct m_config_option *co = &config->opts[n];
        struct m_group_data *gdata = m_config_gdata(config->data, co->group_index);
        if (gdata && co->opt->offset >= 0)
            co->data = gdata->udata + co->opt->offset;
    }

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

static const struct m_config_group *find_group(struct mpv_global *global,
                                               const struct m_option *cfg)
{
    struct m_config_shadow *shadow = global->config;
    struct m_config *root = shadow->root;

    for (int n = 0; n < root->num_groups; n++) {
        if (root->groups[n].group->opts == cfg)
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
    const struct m_config_group *group = find_group(global, desc->options);
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

static void init_obj_settings_list(struct m_config *config,
                                   int parent_group_index,
                                   const struct m_obj_list *list)
{
    struct m_obj_desc desc;
    for (int n = 0; ; n++) {
        if (!list->get_desc(&desc, n))
            break;
        if (desc.global_opts) {
            add_sub_group(config, NULL, parent_group_index, -1,
                          desc.global_opts);
        }
        if (list->use_global_options && desc.options) {
            struct m_sub_options *conf = talloc_ptrtype(config, conf);
            *conf = (struct m_sub_options){
                .prefix = desc.options_prefix,
                .opts = desc.options,
                .defaults = desc.priv_defaults,
                .size = desc.priv_size,
            };
            add_sub_group(config, NULL, parent_group_index, -1, conf);
        }
    }
}

static const char *concat_name(void *ta_parent, const char *a, const char *b)
{
    assert(a);
    assert(b);
    if (!a[0])
        return b;
    if (!b[0])
        return a;
    return talloc_asprintf(ta_parent, "%s-%s", a, b);
}

static void add_sub_group(struct m_config *config, const char *name_prefix,
                          int parent_group_index, int parent_ptr,
                          const struct m_sub_options *subopts)
{
        // Can't be used multiple times.
    for (int n = 0; n < config->num_groups; n++)
        assert(config->groups[n].group != subopts);

    // You can only use UPDATE_ flags here.
    assert(!(subopts->change_flags & ~(unsigned)UPDATE_OPTS_MASK));

    assert(parent_group_index >= -1 && parent_group_index < config->num_groups);

    int group_index = config->num_groups++;
    MP_TARRAY_GROW(config, config->groups, group_index);
    config->groups[group_index] = (struct m_config_group){
        .group = subopts,
        .parent_group = parent_group_index,
        .parent_ptr = parent_ptr,
        .co_index = config->num_opts,
    };

    if (subopts->prefix && subopts->prefix[0])
        name_prefix = subopts->prefix;
    if (!name_prefix)
        name_prefix = "";

    for (int i = 0; subopts->opts && subopts->opts[i].name; i++) {
        const struct m_option *opt = &subopts->opts[i];

        if (opt->type == &m_option_type_subconfig)
            continue;

        struct m_config_option co = {
            .name = concat_name(config, name_prefix, opt->name),
            .opt = opt,
            .group_index = group_index,
            .is_hidden = !!opt->deprecation_message,
        };

        if (opt->type != &m_option_type_subconfig)
            MP_TARRAY_APPEND(config, config->opts, config->num_opts, co);
    }

    config->groups[group_index].co_end_index = config->num_opts;

    // Initialize sub-structs. These have to come after, because co_index and
    // co_end_index must strictly be for a single struct only.
    for (int i = 0; subopts->opts && subopts->opts[i].name; i++) {
        const struct m_option *opt = &subopts->opts[i];

        if (opt->type == &m_option_type_subconfig) {
            const struct m_sub_options *new_subopts = opt->priv;

            // Providing default structs in-place is not allowed.
            if (opt->offset >= 0 && subopts->defaults) {
                void *ptr = (char *)subopts->defaults + opt->offset;
                assert(!substruct_read_ptr(ptr));
            }

            const char *prefix = concat_name(config, name_prefix, opt->name);
            add_sub_group(config, prefix, group_index, opt->offset, new_subopts);
        } else if (opt->type == &m_option_type_obj_settings_list) {
            const struct m_obj_list *objlist = opt->priv;
            init_obj_settings_list(config, group_index, objlist);
        }
    }

    config->groups[group_index].group_count = config->num_groups - group_index;
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

const void *m_config_get_co_default(const struct m_config *config,
                                    struct m_config_option *co)
{
    if (co->opt->defval)
        return co->opt->defval;

    const struct m_sub_options *subopt = config->groups[co->group_index].group;
    if (co->opt->offset >= 0 && subopt->defaults)
        return (char *)subopt->defaults + co->opt->offset;

    return NULL;
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
        const void *defptr = m_config_get_co_default(config, co);
        if (!defptr)
            defptr = &default_value;
        if (defptr)
            def = m_option_pretty_print(opt, defptr);
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
    assert(config->global);
    assert(!config->shadow && !config->global->config);

    config->shadow = talloc_zero(NULL, struct m_config_shadow);
    config->shadow->data =
        allocate_option_data(config->shadow, config, 0, config->data);
    config->shadow->root = config;
    pthread_mutex_init(&config->shadow->lock, NULL);

    config->global->config = config->shadow;
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
    int group_index = -1;

    for (int n = 0; n < root->num_groups; n++) {
        // group==NULL is special cased to root group.
        if (root->groups[n].group == group || (!group && !n)) {
            group_index = n;
            break;
        }
    }

    assert(group_index >= 0); // invalid group (or not in option tree)

    struct m_config_cache *cache = talloc_zero(ta_parent, struct m_config_cache);
    talloc_set_destructor(cache, cache_destroy);
    cache->shadow = shadow;

    pthread_mutex_lock(&shadow->lock);
    cache->data = allocate_option_data(cache, root, group_index, shadow->data);
    pthread_mutex_unlock(&shadow->lock);

    cache->opts = cache->data->gdata[0].udata;

    return cache;
}

static bool update_options(struct m_config_data *dst, struct m_config_data *src)
{
    assert(dst->root == src->root);

    bool res = false;
    dst->ts = src->ts;

    // Must be from same root, but they can have arbitrary overlap.
    int group_s = MPMAX(dst->group_index, src->group_index);
    int group_e = MPMIN(dst->group_index + dst->num_gdata,
                        src->group_index + src->num_gdata);
    assert(group_s >= 0 && group_e <= dst->root->num_groups);
    for (int n = group_s; n < group_e; n++) {
        struct m_config_group *g = &dst->root->groups[n];
        struct m_group_data *gsrc = m_config_gdata(src, n);
        struct m_group_data *gdst = m_config_gdata(dst, n);
        assert(gsrc && gdst);

        if (gsrc->ts <= gdst->ts)
            continue;
        gdst->ts = gsrc->ts;
        res = true;

        for (int i = g->co_index; i < g->co_end_index; i++) {
            struct m_config_option *co = &dst->root->opts[i];
            if (co->opt->offset >= 0 && co->opt->type->size) {
                m_option_copy(co->opt, gdst->udata + co->opt->offset,
                                       gsrc->udata + co->opt->offset);
            }
        }
    }

    return res;
}

bool m_config_cache_update(struct m_config_cache *cache)
{
    struct m_config_shadow *shadow = cache->shadow;

    // Using atomics and checking outside of the lock - it's unknown whether
    // this makes it faster or slower. Just cargo culting it.
    if (atomic_load(&shadow->data->ts) <= cache->data->ts)
        return false;

    pthread_mutex_lock(&shadow->lock);
    bool res = update_options(cache->data, shadow->data);
    pthread_mutex_unlock(&shadow->lock);
    return res;
}

void m_config_notify_change_co(struct m_config *config,
                               struct m_config_option *co)
{
    struct m_config_shadow *shadow = config->shadow;
    assert(co->data);

    if (shadow) {
        pthread_mutex_lock(&shadow->lock);

        struct m_config_data *data = shadow->data;
        struct m_group_data *gdata = m_config_gdata(data, co->group_index);
        assert(gdata);

        gdata->ts = atomic_fetch_add(&data->ts, 1) + 1;

        m_option_copy(co->opt, gdata->udata + co->opt->offset, co->data);

        for (int n = 0; n < shadow->num_listeners; n++) {
            struct m_config_cache *cache = shadow->listeners[n];
            if (cache->wakeup_cb && m_config_gdata(cache->data, co->group_index))
                cache->wakeup_cb(cache->wakeup_cb_ctx);
        }

        pthread_mutex_unlock(&shadow->lock);
    }

    int changed = co->opt->flags & UPDATE_OPTS_MASK;
    int group_index = co->group_index;
    while (group_index >= 0) {
        struct m_config_group *g = &config->groups[group_index];
        changed |= g->group->change_flags;
        group_index = g->parent_group;
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
    assert(co->opt->offset >= 0);
    assert(co->opt->type == type);

    struct m_group_data *gdata = m_config_gdata(shadow->data, co->group_index);
    assert(gdata);

    memset(dst, 0, co->opt->type->size);
    m_option_copy(co->opt, dst, gdata->udata + co->opt->offset);
}

struct m_config *mp_get_root_config(struct mpv_global *global)
{
    return global->config->root;
}
