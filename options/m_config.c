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

// Maximum possibly option name length (as it appears to the user).
#define MAX_OPT_NAME_LEN 80

// For use with m_config_cache.
struct m_config_shadow {
    pthread_mutex_t lock;
    // Incremented on every option change.
    mp_atomic_uint64 ts;
    // -- immutable after init
    // List of m_sub_options instances.
    // Index 0 is the top-level and is always present.
    // Immutable after init.
    // Invariant: a parent is always at a lower index than any of its children.
    struct m_config_group *groups;
    int num_groups;
    // -- protected by lock
    struct m_config_data *data; // protected shadow copy of the option data
    struct config_cache **listeners;
    int num_listeners;
};

// Represents a sub-struct (OPT_SUBSTRUCT()).
struct m_config_group {
    const struct m_sub_options *group;
    int group_count;    // 1 + number of all sub groups owned by this (so
                        // m_config_shadow.groups[idx..idx+group_count] is used
                        // by the entire tree of sub groups included by this
                        // group)
    int parent_group;   // index of parent group into m_config_shadow.groups[],
                        // or -1 for group 0
    int parent_ptr;     // ptr offset in the parent group's data, or -1 if
                        // none
    const char *prefix; // concat_name(_, prefix, opt->name) => full name
                        // (the parent names are already included in this)
};

// A copy of option data. Used for the main option struct, the shadow data,
// and copies for m_config_cache.
struct m_config_data {
    struct m_config_shadow *shadow; // option definitions etc., main data copy
    int group_index;                // start index into m_config.groups[]
    struct m_group_data *gdata;     // user struct allocation (our copy of data)
    int num_gdata;                  // (group_index+num_gdata = end index)
};

struct config_cache {
    struct m_config_cache *public;

    struct m_config_data *data;     // public data
    struct m_config_data *src;      // global data (currently ==shadow->data)
    struct m_config_shadow *shadow; // global metadata
    uint64_t ts;                    // timestamp of this data copy
    bool in_list;                   // part of m_config_shadow->listeners[]
    int upd_group;                  // for "incremental" change notification
    int upd_opt;


    // --- Implicitly synchronized by setting/unsetting wakeup_cb.
    struct mp_dispatch_queue *wakeup_dispatch_queue;
    void (*wakeup_dispatch_cb)(void *ctx);
    void *wakeup_dispatch_cb_ctx;

    // --- Protected by shadow->lock
    void (*wakeup_cb)(void *ctx);
    void *wakeup_cb_ctx;
};

// Per m_config_data state for each m_config_group.
struct m_group_data {
    char *udata;        // pointer to group user option struct
    uint64_t ts;        // timestamp of the data copy
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

static struct m_config_cache *m_config_cache_alloc_internal(void *ta_parent,
                                            struct m_config_shadow *shadow,
                                            const struct m_sub_options *group);
static void add_sub_group(struct m_config_shadow *shadow, const char *name_prefix,
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

// Like concat_name(), but returns either a, b, or buf. buf/buf_size is used as
// target for snprintf(). (buf_size is recommended to be MAX_OPT_NAME_LEN.)
static const char *concat_name_buf(char *buf, size_t buf_size,
                                   const char *a, const char *b)
{
    assert(a);
    assert(b);
    if (!a[0])
        return b;
    if (!b[0])
        return a;
    snprintf(buf, buf_size, "%s-%s", a, b);
    return buf;
}

// Return full option name from prefix (a) and option name (b). Returns either
// a, b, or a talloc'ed string under ta_parent.
static const char *concat_name(void *ta_parent, const char *a, const char *b)
{
    char buf[MAX_OPT_NAME_LEN];
    const char *r = concat_name_buf(buf, sizeof(buf), a, b);
    return r == buf ? talloc_strdup(ta_parent, r) : r;
}

struct opt_iterate_state {
    // User can read these fields.
    int group_index;
    int opt_index;
    const struct m_option *opt;
    const char *full_name; // may point to name_buf

    // Internal.
    int group_index_end;
    char name_buf[MAX_OPT_NAME_LEN];
    struct m_config_shadow *shadow;
};

// Start iterating all options and sub-options in the given group.
static void opt_iterate_init(struct opt_iterate_state *iter,
                             struct m_config_shadow *shadow, int group_index)
{
    assert(group_index >= 0 && group_index < shadow->num_groups);
    iter->group_index = group_index;
    iter->group_index_end = group_index + shadow->groups[group_index].group_count;
    iter->opt_index = -1;
    iter->shadow = shadow;
}

// Get the first or next option. Returns false if end reached. If this returns
// true, most fields in *iter are valid.
// This does not return pseudo-option entries (like m_option_type_subconfig).
static bool opt_iterate_next(struct opt_iterate_state *iter)
{
    if (iter->group_index < 0)
        return false;


    while (1) {
        if (iter->group_index >= iter->group_index_end) {
            iter->group_index = -1;
            return false;
        }

        iter->opt_index += 1;

        struct m_config_group *g = &iter->shadow->groups[iter->group_index];
        const struct m_option *opts = g->group->opts;

        if (!opts || !opts[iter->opt_index].name) {
            iter->group_index += 1;
            iter->opt_index = -1;
            continue;
        }

        iter->opt = &opts[iter->opt_index];

        if (iter->opt->type == &m_option_type_subconfig)
            continue;

        iter->full_name = concat_name_buf(iter->name_buf, sizeof(iter->name_buf),
                                          g->prefix, iter->opt->name);
        return true;
    }
    assert(0);
}

static void list_profiles(struct m_config *config)
{
    MP_INFO(config, "Available profiles:\n");
    for (struct m_profile *p = config->profiles; p; p = p->next)
        MP_INFO(config, "\t%s\t%s\n", p->name, p->desc ? p->desc : "");
    MP_INFO(config, "\n");
}

static int show_profile(struct m_config *config, bstr param)
{
    struct m_profile *p;
    if (!param.len) {
        list_profiles(config);
        return M_OPT_EXIT;
    }
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
    assert(group_index < data->shadow->num_groups);
    struct m_config_group *group = &data->shadow->groups[group_index];
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

    for (int n = 0; opts->opts && opts->opts[n].name; n++) {
        const struct m_option *opt = &opts->opts[n];

        if (opt->offset < 0 || opt->type->size == 0)
            continue;

        void *dst = gdata->udata + opt->offset;
        const void *defptr = opt->defval ? opt->defval : dst;
        if (copy_src)
            defptr = copy_src + opt->offset;

        init_opt_inplace(opt, dst, defptr);
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
        struct m_config_group *group =
            &data->shadow->groups[data->group_index + i];
        const struct m_option *opts = group->group->opts;

        for (int n = 0; opts && opts[n].name; n++) {
            const struct m_option *opt = &opts[n];

            if (opt->offset >= 0 && opt->type->size > 0)
                m_option_free(opt, gdata->udata + opt->offset);
        }
    }
}

// Allocate data using the option description in shadow, starting at group_index
// (index into m_config.groups[]).
// If copy is not NULL, copy all data from there (for groups which are in both
// m_config_data instances), in all other cases init the data with the defaults.
static struct m_config_data *allocate_option_data(void *ta_parent,
                                                  struct m_config_shadow *shadow,
                                                  int group_index,
                                                  struct m_config_data *copy)
{
    assert(group_index >= 0 && group_index < shadow->num_groups);
    struct m_config_data *data = talloc_zero(ta_parent, struct m_config_data);
    talloc_set_destructor(data, free_option_data);

    data->shadow = shadow;
    data->group_index = group_index;

    struct m_config_group *root_group = &shadow->groups[group_index];
    assert(root_group->group_count > 0);

    for (int n = group_index; n < group_index + root_group->group_count; n++)
        alloc_group(data, n, copy);

    return data;
}

static void shadow_destroy(void *p)
{
    struct m_config_shadow *shadow = p;

    // must all have been unregistered
    assert(shadow->num_listeners == 0);

    talloc_free(shadow->data);
    pthread_mutex_destroy(&shadow->lock);
}

static struct m_config_shadow *m_config_shadow_new(const struct m_sub_options *root)
{
    struct m_config_shadow *shadow = talloc_zero(NULL, struct m_config_shadow);
    talloc_set_destructor(shadow, shadow_destroy);
    pthread_mutex_init(&shadow->lock, NULL);

    add_sub_group(shadow, NULL, -1, -1, root);

    if (!root->size)
        return shadow;

    shadow->data = allocate_option_data(shadow, shadow, 0, NULL);

    return shadow;
}

static void config_destroy(void *p)
{
    struct m_config *config = p;
    config->option_change_callback = NULL;
    m_config_restore_backups(config);

    talloc_free(config->cache);
    talloc_free(config->shadow);
}

struct m_config *m_config_new(void *talloc_ctx, struct mp_log *log,
                              const struct m_sub_options *root)
{
    struct m_config *config = talloc(talloc_ctx, struct m_config);
    talloc_set_destructor(config, config_destroy);
    *config = (struct m_config){.log = log,};

    config->shadow = m_config_shadow_new(root);

    if (root->size) {
        config->cache =
            m_config_cache_alloc_internal(config, config->shadow, root);
        config->optstruct = config->cache->opts;
    }

    struct opt_iterate_state it;
    opt_iterate_init(&it, config->shadow, 0);
    while (opt_iterate_next(&it)) {
        struct m_config_option co = {
            .name = talloc_strdup(config, it.full_name),
            .opt = it.opt,
            .group_index = it.group_index,
            .is_hidden = !!it.opt->deprecation_message,
        };

        struct m_group_data *gdata = config->cache
            ? m_config_gdata(config->cache->internal->data,  it.group_index)
            : NULL;

        if (gdata && co.opt->offset >= 0)
            co.data = gdata->udata + co.opt->offset;

        MP_TARRAY_APPEND(config, config->opts, config->num_opts, co);
    }

    return config;
}

static struct m_config *m_config_from_obj_desc(void *talloc_ctx,
                                               struct mp_log *log,
                                               struct mpv_global *global,
                                               struct m_obj_desc *desc)
{
    struct m_sub_options *root = talloc_ptrtype(NULL, root);
    *root = (struct m_sub_options){
        .opts = desc->options,
        // (global == NULL got repurposed to mean "no alloc")
        .size = global ? desc->priv_size : 0,
        .defaults = desc->priv_defaults,
    };

    struct m_config *c = m_config_new(talloc_ctx, log, root);
    talloc_steal(c, root);
    c->global = global;
    if (desc->set_defaults && c->global)
        desc->set_defaults(c->global, c->optstruct);
    return c;
}

struct m_config *m_config_from_obj_desc_noalloc(void *talloc_ctx,
                                                struct mp_log *log,
                                                struct m_obj_desc *desc)
{
    return m_config_from_obj_desc(talloc_ctx, log, NULL, desc);
}

static const struct m_config_group *find_group(struct mpv_global *global,
                                               const struct m_option *cfg)
{
    struct m_config_shadow *shadow = global->config;

    for (int n = 0; n < shadow->num_groups; n++) {
        if (shadow->groups[n].group->opts == cfg)
            return &shadow->groups[n];
    }

    return NULL;
}

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

static void init_obj_settings_list(struct m_config_shadow *shadow,
                                   int parent_group_index,
                                   const struct m_obj_list *list)
{
    struct m_obj_desc desc;
    for (int n = 0; ; n++) {
        if (!list->get_desc(&desc, n))
            break;
        if (desc.global_opts) {
            add_sub_group(shadow, NULL, parent_group_index, -1,
                          desc.global_opts);
        }
        if (list->use_global_options && desc.options) {
            struct m_sub_options *conf = talloc_ptrtype(shadow, conf);
            *conf = (struct m_sub_options){
                .prefix = desc.options_prefix,
                .opts = desc.options,
                .defaults = desc.priv_defaults,
                .size = desc.priv_size,
            };
            add_sub_group(shadow, NULL, parent_group_index, -1, conf);
        }
    }
}

static void add_sub_group(struct m_config_shadow *shadow, const char *name_prefix,
                          int parent_group_index, int parent_ptr,
                          const struct m_sub_options *subopts)
{
    // Can't be used multiple times.
    for (int n = 0; n < shadow->num_groups; n++)
        assert(shadow->groups[n].group != subopts);

    if (!name_prefix)
        name_prefix = "";
    if (subopts->prefix && subopts->prefix[0]) {
        assert(!name_prefix[0]);
        name_prefix = subopts->prefix;
    }

    // You can only use UPDATE_ flags here.
    assert(!(subopts->change_flags & ~(unsigned)UPDATE_OPTS_MASK));

    assert(parent_group_index >= -1 && parent_group_index < shadow->num_groups);

    int group_index = shadow->num_groups++;
    MP_TARRAY_GROW(shadow, shadow->groups, group_index);
    shadow->groups[group_index] = (struct m_config_group){
        .group = subopts,
        .parent_group = parent_group_index,
        .parent_ptr = parent_ptr,
        .prefix = name_prefix,
    };

    for (int i = 0; subopts->opts && subopts->opts[i].name; i++) {
        const struct m_option *opt = &subopts->opts[i];

        if (opt->type == &m_option_type_subconfig) {
            const struct m_sub_options *new_subopts = opt->priv;

            // Providing default structs in-place is not allowed.
            if (opt->offset >= 0 && subopts->defaults) {
                void *ptr = (char *)subopts->defaults + opt->offset;
                assert(!substruct_read_ptr(ptr));
            }

            const char *prefix = concat_name(shadow, name_prefix, opt->name);
            add_sub_group(shadow, prefix, group_index, opt->offset, new_subopts);

        } else if (opt->type == &m_option_type_obj_settings_list) {
            const struct m_obj_list *objlist = opt->priv;
            init_obj_settings_list(shadow, group_index, objlist);
        }
    }

    if (subopts->get_sub_options) {
        for (int i = 0; ; i++) {
            const struct m_sub_options *sub = NULL;
            if (!subopts->get_sub_options(i, &sub))
                break;
            if (sub)
                add_sub_group(shadow, NULL, group_index, -1, sub);
        }
    }

    shadow->groups[group_index].group_count = shadow->num_groups - group_index;
}

static uint64_t get_option_change_mask(struct m_config_shadow *shadow,
                                       int group_index, int group_root,
                                       const struct m_option *opt)
{
    uint64_t changed = opt->flags & UPDATE_OPTS_MASK;
    while (group_index != group_root) {
        struct m_config_group *g = &shadow->groups[group_index];
        changed |= g->group->change_flags;
        group_index = g->parent_group;
    }
    return changed;
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

    const struct m_sub_options *subopt =
        config->shadow->groups[co->group_index].group;

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

// Special options that don't really fit into the option handling model. They
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
            list_profiles(config);
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

// This notification happens when anyone other than m_config->cache (i.e. not
// through m_config_set_option_raw() or related) changes any options.
static void async_change_cb(void *p)
{
    struct m_config *config = p;

    void *ptr;
    while (m_config_cache_get_next_changed(config->cache, &ptr)) {
        // Regrettable linear search, might degenerate to quadratic.
        for (int n = 0; n < config->num_opts; n++) {
            struct m_config_option *co = &config->opts[n];
            if (co->data == ptr) {
                if (config->option_change_callback) {
                    config->option_change_callback(
                        config->option_change_callback_ctx, co,
                        config->cache->change_flags, false);
                }
                break;
            }
        }
        config->cache->change_flags = 0;
    }
}

void m_config_set_update_dispatch_queue(struct m_config *config,
                                        struct mp_dispatch_queue *dispatch)
{
    m_config_cache_set_dispatch_change_cb(config->cache, dispatch,
                                          async_change_cb, config);
}

// Normally m_config_cache will not send notifications when _we_ change our
// own stuff. For whatever funny reasons, we need that, though.
static void force_self_notify_change_opt(struct m_config *config,
                                         struct m_config_option *co,
                                         bool self_notification)
{
    int changed =
        get_option_change_mask(config->shadow, co->group_index, 0, co->opt);

    if (config->option_change_callback) {
        config->option_change_callback(config->option_change_callback_ctx, co,
                                       changed, self_notification);
    }
}

static void notify_opt(struct m_config *config, void *ptr, bool self_notification)
{
    for (int n = 0; n < config->num_opts; n++) {
        struct m_config_option *co = &config->opts[n];
        if (co->data == ptr) {
            if (m_config_cache_write_opt(config->cache, co->data))
                force_self_notify_change_opt(config, co, self_notification);
            return;
        }
    }
    // ptr doesn't point to any config->optstruct field declared in the
    // option list?
    assert(false);
}

void m_config_notify_change_opt_ptr(struct m_config *config, void *ptr)
{
    notify_opt(config, ptr, true);
}

void m_config_notify_change_opt_ptr_notify(struct m_config *config, void *ptr)
{
    // (the notify bool is inverted: by not marking it as self-notification,
    // the mpctx option change handler actually applies it)
    notify_opt(config, ptr, false);
}

int m_config_set_option_raw(struct m_config *config,
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

    m_config_mark_co_flags(co, flags);

    m_option_copy(co->opt, co->data, data);
    if (m_config_cache_write_opt(config->cache, co->data))
        force_self_notify_change_opt(config, co, false);

    return 0;
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
        MP_VERBOSE(config, "Setting option '%.*s' = '%.*s' (flags = %d)\n",
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
    MP_VERBOSE(config, "Applying profile '%s'...\n", name);

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

static void cache_destroy(void *p)
{
    struct m_config_cache *cache = p;

    // (technically speaking, being able to call them both without anything
    // breaking is a feature provided by these functions)
    m_config_cache_set_wakeup_cb(cache, NULL, NULL);
    m_config_cache_set_dispatch_change_cb(cache, NULL, NULL, NULL);
}

static struct m_config_cache *m_config_cache_alloc_internal(void *ta_parent,
                                            struct m_config_shadow *shadow,
                                            const struct m_sub_options *group)
{
    int group_index = -1;

    for (int n = 0; n < shadow->num_groups; n++) {
        if (shadow->groups[n].group == group) {
            group_index = n;
            break;
        }
    }

    assert(group_index >= 0); // invalid group (or not in option tree)

    struct cache_alloc {
        struct m_config_cache a;
        struct config_cache b;
    };
    struct cache_alloc *alloc = talloc_zero(ta_parent, struct cache_alloc);
    assert((void *)&alloc->a == (void *)alloc);
    struct m_config_cache *cache = &alloc->a;
    talloc_set_destructor(cache, cache_destroy);
    cache->internal = &alloc->b;

    struct config_cache *in = cache->internal;
    in->shadow = shadow;
    in->src = shadow->data;

    pthread_mutex_lock(&shadow->lock);
    in->data = allocate_option_data(cache, shadow, group_index, in->src);
    pthread_mutex_unlock(&shadow->lock);

    cache->opts = in->data->gdata[0].udata;

    in->upd_group = -1;

    return cache;
}

struct m_config_cache *m_config_cache_alloc(void *ta_parent,
                                            struct mpv_global *global,
                                            const struct m_sub_options *group)
{
    return m_config_cache_alloc_internal(ta_parent, global->config, group);
}

static void update_next_option(struct m_config_cache *cache, void **p_opt)
{
    struct config_cache *in = cache->internal;
    struct m_config_data *dst = in->data;
    struct m_config_data *src = in->src;

    assert(src->group_index == 0); // must be the option root currently

    *p_opt = NULL;

    while (in->upd_group < dst->group_index + dst->num_gdata) {
        struct m_group_data *gsrc = m_config_gdata(src, in->upd_group);
        struct m_group_data *gdst = m_config_gdata(dst, in->upd_group);
        assert(gsrc && gdst);

        if (gdst->ts < gsrc->ts) {
            struct m_config_group *g = &dst->shadow->groups[in->upd_group];
            const struct m_option *opts = g->group->opts;

            while (opts && opts[in->upd_opt].name) {
                const struct m_option *opt = &opts[in->upd_opt];

                if (opt->offset >= 0 && opt->type->size) {
                    void *dsrc = gsrc->udata + opt->offset;
                    void *ddst = gdst->udata + opt->offset;

                    if (!m_option_equal(opt, ddst, dsrc)) {
                        uint64_t ch = get_option_change_mask(dst->shadow,
                                        in->upd_group, dst->group_index, opt);

                        if (cache->debug) {
                            char *vdst = m_option_print(opt, ddst);
                            char *vsrc = m_option_print(opt, dsrc);
                            mp_warn(cache->debug, "Option '%s' changed from "
                                    "'%s' to' %s' (flags = 0x%"PRIx64")\n",
                                    opt->name, vdst, vsrc, ch);
                            talloc_free(vdst);
                            talloc_free(vsrc);
                        }

                        m_option_copy(opt, ddst, dsrc);
                        cache->change_flags |= ch;

                        in->upd_opt++; // skip this next time
                        *p_opt = ddst;
                        return;
                    }
                }

                in->upd_opt++;
            }

            gdst->ts = gsrc->ts;
        }

        in->upd_group++;
        in->upd_opt = 0;
    }

    in->upd_group = -1;
}

static bool cache_check_update(struct m_config_cache *cache)
{
    struct config_cache *in = cache->internal;
    struct m_config_shadow *shadow = in->shadow;

    // Using atomics and checking outside of the lock - it's unknown whether
    // this makes it faster or slower. Just cargo culting it.
    uint64_t new_ts = atomic_load(&shadow->ts);
    if (in->ts >= new_ts)
        return false;

    in->ts = new_ts;
    in->upd_group = in->data->group_index;
    in->upd_opt = 0;
    return true;
}

bool m_config_cache_update(struct m_config_cache *cache)
{
    struct config_cache *in = cache->internal;
    struct m_config_shadow *shadow = in->shadow;

    if (!cache_check_update(cache))
        return false;

    pthread_mutex_lock(&shadow->lock);
    bool res = false;
    while (1) {
        void *p;
        update_next_option(cache, &p);
        if (!p)
            break;
        res = true;
    }
    pthread_mutex_unlock(&shadow->lock);
    return res;
}

bool m_config_cache_get_next_changed(struct m_config_cache *cache, void **opt)
{
    struct config_cache *in = cache->internal;
    struct m_config_shadow *shadow = in->shadow;

    *opt = NULL;
    if (!cache_check_update(cache) && in->upd_group < 0)
        return false;

    pthread_mutex_lock(&shadow->lock);
    update_next_option(cache, opt);
    pthread_mutex_unlock(&shadow->lock);
    return !!*opt;
}

static void find_opt(struct m_config_shadow *shadow, struct m_config_data *data,
                     void *ptr, int *group_idx, int *opt_idx)
{
    *group_idx = -1;
    *opt_idx = -1;

    for (int n = data->group_index; n < data->group_index + data->num_gdata; n++)
    {
        struct m_group_data *gd = m_config_gdata(data, n);
        struct m_config_group *g = &shadow->groups[n];
        const struct m_option *opts = g->group->opts;

        for (int i = 0; opts && opts[i].name; i++) {
            const struct m_option *opt = &opts[i];

            if (opt->offset >= 0 && opt->type->size &&
                ptr == gd->udata + opt->offset)
            {
                *group_idx = n;
                *opt_idx = i;
                return;
            }
        }
    }
}

bool m_config_cache_write_opt(struct m_config_cache *cache, void *ptr)
{
    struct config_cache *in = cache->internal;
    struct m_config_shadow *shadow = in->shadow;

    int group_idx = -1;
    int opt_idx = -1;
    find_opt(shadow, in->data, ptr, &group_idx, &opt_idx);

    // ptr was not in cache->opts, or no option declaration matching it.
    assert(group_idx >= 0);

    struct m_config_group *g = &shadow->groups[group_idx];
    const struct m_option *opt = &g->group->opts[opt_idx];

    pthread_mutex_lock(&shadow->lock);

    struct m_group_data *gdst = m_config_gdata(in->data, group_idx);
    struct m_group_data *gsrc = m_config_gdata(in->src, group_idx);
    assert(gdst && gsrc);

    bool changed = !m_option_equal(opt, gsrc->udata + opt->offset, ptr);
    if (changed) {
        m_option_copy(opt, gsrc->udata + opt->offset, ptr);

        gsrc->ts = atomic_fetch_add(&shadow->ts, 1) + 1;

        for (int n = 0; n < shadow->num_listeners; n++) {
            struct config_cache *listener = shadow->listeners[n];
            if (listener->wakeup_cb && m_config_gdata(listener->data, group_idx))
                listener->wakeup_cb(listener->wakeup_cb_ctx);
        }
    }

    pthread_mutex_unlock(&shadow->lock);

    return changed;
}

void m_config_cache_set_wakeup_cb(struct m_config_cache *cache,
                                  void (*cb)(void *ctx), void *cb_ctx)
{
    struct config_cache *in = cache->internal;
    struct m_config_shadow *shadow = in->shadow;

    pthread_mutex_lock(&shadow->lock);
    if (in->in_list) {
        for (int n = 0; n < shadow->num_listeners; n++) {
            if (shadow->listeners[n] == in) {
                MP_TARRAY_REMOVE_AT(shadow->listeners, shadow->num_listeners, n);
                break;
            }
        }
        for (int n = 0; n < shadow->num_listeners; n++)
            assert(shadow->listeners[n] != in); // only 1 wakeup_cb per cache
        // (The deinitialization path relies on this to free all memory.)
        if (!shadow->num_listeners) {
            talloc_free(shadow->listeners);
            shadow->listeners = NULL;
        }
    }
    if (cb) {
        MP_TARRAY_APPEND(NULL, shadow->listeners, shadow->num_listeners, in);
        in->in_list = true;
        in->wakeup_cb = cb;
        in->wakeup_cb_ctx = cb_ctx;
    }
    pthread_mutex_unlock(&shadow->lock);
}

static void dispatch_notify(void *p)
{
    struct config_cache *in = p;

    assert(in->wakeup_dispatch_queue);
    mp_dispatch_enqueue_notify(in->wakeup_dispatch_queue,
                               in->wakeup_dispatch_cb,
                               in->wakeup_dispatch_cb_ctx);
}

void m_config_cache_set_dispatch_change_cb(struct m_config_cache *cache,
                                           struct mp_dispatch_queue *dispatch,
                                           void (*cb)(void *ctx), void *cb_ctx)
{
    struct config_cache *in = cache->internal;

    // Removing the old one is tricky. First make sure no new notifications will
    // come.
    m_config_cache_set_wakeup_cb(cache, NULL, NULL);
    // Remove any pending notifications (assume we're on the same thread as
    // any potential mp_dispatch_queue_process() callers).
    if (in->wakeup_dispatch_queue) {
        mp_dispatch_cancel_fn(in->wakeup_dispatch_queue,
                              in->wakeup_dispatch_cb,
                              in->wakeup_dispatch_cb_ctx);
    }

    in->wakeup_dispatch_queue = NULL;
    in->wakeup_dispatch_cb = NULL;
    in->wakeup_dispatch_cb_ctx = NULL;

    if (cb) {
        in->wakeup_dispatch_queue = dispatch;
        in->wakeup_dispatch_cb = cb;
        in->wakeup_dispatch_cb_ctx = cb_ctx;
        m_config_cache_set_wakeup_cb(cache, dispatch_notify, in);
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

    struct opt_iterate_state it;
    opt_iterate_init(&it, shadow, 0);
    while (opt_iterate_next(&it)) {
        if (strcmp(name, it.full_name) == 0) {
            struct m_group_data *gdata =
                m_config_gdata(shadow->data, it.group_index);
            assert(gdata);

            assert(it.opt->offset >= 0);
            assert(it.opt->type == type);

            memset(dst, 0, it.opt->type->size);
            m_option_copy(it.opt, dst, gdata->udata + it.opt->offset);
            return;
        }
    }

    assert(0); // not found
}
