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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>

#include "m_config_core.h"
#include "options/m_option.h"
#include "common/common.h"
#include "common/global.h"
#include "common/msg.h"
#include "common/msg_control.h"
#include "misc/dispatch.h"
#include "osdep/atomic.h"

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
    int opt_count;      // cached opt. count; group->opts[opt_count].name==NULL
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
    int group_start, group_end;     // derived from data->group_index etc.
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

static const union m_option_value default_value = {0};

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
    char buf[M_CONFIG_MAX_OPT_NAME_LEN];
    const char *r = concat_name_buf(buf, sizeof(buf), a, b);
    return r == buf ? talloc_strdup(ta_parent, r) : r;
}

static bool iter_next(struct m_config_shadow *shadow, int group_start,
                      int group_end, int32_t *p_id)
{
    int32_t id = *p_id;
    int group_index = id == -1 ? group_start : id >> 16;
    int opt_index = id == -1 ? -1 : id & 0xFFFF;

    assert(group_index >= group_start && group_index <= group_end);

    while (1) {
        if (group_index >= group_end)
            return false;

        struct m_config_group *g = &shadow->groups[group_index];
        const struct m_option *opts = g->group->opts;

        assert(opt_index >= -1 && opt_index < g->opt_count);

        opt_index += 1;

        if (!opts || !opts[opt_index].name) {
            group_index += 1;
            opt_index = -1;
            continue;
        }

        if (opts[opt_index].type == &m_option_type_subconfig)
            continue;

        *p_id = (group_index << 16) | opt_index;
        return true;
    }
}

bool m_config_shadow_get_next_opt(struct m_config_shadow *shadow, int32_t *p_id)
{
    return iter_next(shadow, 0, shadow->num_groups, p_id);
}

bool m_config_cache_get_next_opt(struct m_config_cache *cache, int32_t *p_id)
{
    return iter_next(cache->shadow, cache->internal->group_start,
                     cache->internal->group_end, p_id);
}

static void get_opt_from_id(struct m_config_shadow *shadow, int32_t id,
                            int *out_group_index, int *out_opt_index)
{
    int group_index = id >> 16;
    int opt_index = id & 0xFFFF;

    assert(group_index >= 0 && group_index < shadow->num_groups);
    assert(opt_index >= 0 && opt_index < shadow->groups[group_index].opt_count);

    *out_group_index = group_index;
    *out_opt_index = opt_index;
}

const struct m_option *m_config_shadow_get_opt(struct m_config_shadow *shadow,
                                               int32_t id)
{
    int group_index, opt_index;
    get_opt_from_id(shadow, id, &group_index, &opt_index);

    return &shadow->groups[group_index].group->opts[opt_index];
}

const char *m_config_shadow_get_opt_name(struct m_config_shadow *shadow,
                                         int32_t id, char *buf, size_t buf_size)
{
    int group_index, opt_index;
    get_opt_from_id(shadow, id, &group_index, &opt_index);

    struct m_config_group *g = &shadow->groups[group_index];
    return concat_name_buf(buf, buf_size, g->prefix,
                           g->group->opts[opt_index].name);
}

const void *m_config_shadow_get_opt_default(struct m_config_shadow *shadow,
                                            int32_t id)
{
    int group_index, opt_index;
    get_opt_from_id(shadow, id, &group_index, &opt_index);

    const struct m_sub_options *subopt = shadow->groups[group_index].group;
    const struct m_option *opt = &subopt->opts[opt_index];

    if (opt->offset < 0)
        return NULL;

    if (opt->defval)
        return opt->defval;

    if (subopt->defaults)
        return (char *)subopt->defaults + opt->offset;

    return &default_value;
}

void *m_config_cache_get_opt_data(struct m_config_cache *cache, int32_t id)
{
    int group_index, opt_index;
    get_opt_from_id(cache->shadow, id, &group_index, &opt_index);

    assert(group_index >= cache->internal->group_start &&
           group_index < cache->internal->group_end);

    struct m_group_data *gd = m_config_gdata(cache->internal->data, group_index);
    const struct m_option *opt =
        &cache->shadow->groups[group_index].group->opts[opt_index];

    return gd && opt->offset >= 0 ? gd->udata + opt->offset : NULL;
}

static uint64_t get_opt_change_mask(struct m_config_shadow *shadow, int group_index,
                                    int group_root, const struct m_option *opt)
{
    uint64_t changed = opt->flags & UPDATE_OPTS_MASK;
    while (group_index != group_root) {
        struct m_config_group *g = &shadow->groups[group_index];
        changed |= g->group->change_flags;
        group_index = g->parent_group;
    }
    return changed;
}

uint64_t m_config_cache_get_option_change_mask(struct m_config_cache *cache,
                                               int32_t id)
{
    struct m_config_shadow *shadow = cache->shadow;
    int group_index, opt_index;
    get_opt_from_id(shadow, id, &group_index, &opt_index);

    assert(group_index >= cache->internal->group_start &&
           group_index < cache->internal->group_end);

    return get_opt_change_mask(cache->shadow, group_index,
                               cache->internal->data->group_index,
                               &shadow->groups[group_index].group->opts[opt_index]);
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

struct m_config_shadow *m_config_shadow_new(const struct m_sub_options *root)
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

        shadow->groups[group_index].opt_count = i + 1;
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

static void cache_destroy(void *p)
{
    struct m_config_cache *cache = p;

    // (technically speaking, being able to call them both without anything
    // breaking is a feature provided by these functions)
    m_config_cache_set_wakeup_cb(cache, NULL, NULL);
    m_config_cache_set_dispatch_change_cb(cache, NULL, NULL, NULL);
}

struct m_config_cache *m_config_cache_from_shadow(void *ta_parent,
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
    cache->shadow = shadow;

    struct config_cache *in = cache->internal;
    in->shadow = shadow;
    in->src = shadow->data;

    pthread_mutex_lock(&shadow->lock);
    in->data = allocate_option_data(cache, shadow, group_index, in->src);
    pthread_mutex_unlock(&shadow->lock);

    cache->opts = in->data->gdata[0].udata;

    in->group_start = in->data->group_index;
    in->group_end = in->group_start + in->data->num_gdata;
    assert(shadow->groups[in->group_start].group_count == in->data->num_gdata);

    in->upd_group = -1;

    return cache;
}

struct m_config_cache *m_config_cache_alloc(void *ta_parent,
                                            struct mpv_global *global,
                                            const struct m_sub_options *group)
{
    return m_config_cache_from_shadow(ta_parent, global->config, group);
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
                        uint64_t ch = get_opt_change_mask(dst->shadow,
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
