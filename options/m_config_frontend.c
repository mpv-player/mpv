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

#include <assert.h>
#include <errno.h>
#include <float.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "libmpv/client.h"

#include "common/common.h"
#include "common/global.h"
#include "common/msg_control.h"
#include "common/msg.h"
#include "m_config_frontend.h"
#include "m_config.h"
#include "misc/dispatch.h"
#include "misc/node.h"
#include "options/m_option.h"
#include "osdep/threads.h"

extern const char mp_help_text[];

// Profiles allow to predefine some sets of options that can then
// be applied later on with the internal -profile option.
#define MAX_PROFILE_DEPTH 20
// Maximal include depth.
#define MAX_RECURSION_DEPTH 8

struct m_profile {
    struct m_profile *next;
    char *name;
    char *desc;
    char *cond;
    int restore_mode;
    int num_opts;
    // Option/value pair array.
    // name,value = opts[n*2+0],opts[n*2+1]
    char **opts;
    // For profile restoring.
    struct m_opt_backup *backups;
};

// In the file local case, this contains the old global value.
// It's also used for profile restoring.
struct m_opt_backup {
    struct m_opt_backup *next;
    struct m_config_option *co;
    int flags;
    void *backup, *nval;
};

static const struct m_option profile_restore_mode_opt = {
    .name = "profile-restore",
    .type = &m_option_type_choice,
    M_CHOICES({"default", 0}, {"copy", 1}, {"copy-equal", 2}),
};

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
    if (p->cond) {
        MP_INFO(config, "%*sprofile-cond=%s\n", config->profile_depth, "",
                p->cond);
    }
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
    return c;
}

struct m_config *m_config_from_obj_desc_noalloc(void *talloc_ctx,
                                                struct mp_log *log,
                                                struct m_obj_desc *desc)
{
    return m_config_from_obj_desc(talloc_ctx, log, NULL, desc);
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
    char **args)
{
    struct m_config *config = m_config_from_obj_desc(ta_parent, log, global, desc);
    if (m_config_set_obj_params(config, log, global, desc, args) < 0)
        goto error;

    return config;
error:
    talloc_free(config);
    return NULL;
}

static void backup_dtor(void *p)
{
    struct m_opt_backup *bc = p;
    m_option_free(bc->co->opt, bc->backup);
    if (bc->nval)
        m_option_free(bc->co->opt, bc->nval);
}

#define BACKUP_LOCAL 1
#define BACKUP_NVAL 2
static void ensure_backup(struct m_opt_backup **list, int flags,
                          struct m_config_option *co)
{
    if (!co->data)
        return;
    for (struct m_opt_backup *cur = *list; cur; cur = cur->next) {
        if (cur->co->data == co->data) // comparing data ptr catches aliases
            return;
    }
    struct m_opt_backup *bc = talloc_ptrtype(NULL, bc);
    talloc_set_destructor(bc, backup_dtor);
    *bc = (struct m_opt_backup) {
        .co = co,
        .backup = talloc_zero_size(bc, co->opt->type->size),
        .nval = flags & BACKUP_NVAL
            ? talloc_zero_size(bc, co->opt->type->size) : NULL,
        .flags = flags,
    };
    m_option_copy(co->opt, bc->backup, co->data);
    bc->next = *list;
    *list = bc;
    if (bc->flags & BACKUP_LOCAL)
        co->is_set_locally = true;
}

static void restore_backups(struct m_opt_backup **list, struct m_config *config)
{
    while (*list) {
        struct m_opt_backup *bc = *list;
        *list = bc->next;

        if (!bc->nval || m_option_equal(bc->co->opt, bc->co->data, bc->nval))
            m_config_set_option_raw(config, bc->co, bc->backup, 0);

        if (bc->flags & BACKUP_LOCAL)
            bc->co->is_set_locally = false;
        talloc_free(bc);
    }
}

void m_config_restore_backups(struct m_config *config)
{
    restore_backups(&config->backup_opts, config);
}

bool m_config_watch_later_backup_opt_changed(struct m_config *config,
                                             char *opt_name)
{
    struct m_config_option *co = m_config_get_co(config, bstr0(opt_name));
    if (!co) {
        MP_ERR(config, "Option %s not found.\n", opt_name);
        return false;
    }

    for (struct m_opt_backup *bc = config->watch_later_backup_opts; bc;
         bc = bc->next) {
        if (strcmp(bc->co->name, co->name) == 0) {
            struct m_config_option *bc_co = (struct m_config_option *)bc->backup;
            return !m_option_equal(co->opt, co->data, bc_co);
        }
    }

    return false;
}

void m_config_backup_opt(struct m_config *config, const char *opt)
{
    struct m_config_option *co = m_config_get_co(config, bstr0(opt));
    if (co) {
        ensure_backup(&config->backup_opts, BACKUP_LOCAL, co);
    } else {
        MP_ERR(config, "Option %s not found.\n", opt);
    }
}

void m_config_backup_all_opts(struct m_config *config)
{
    for (int n = 0; n < config->num_opts; n++)
        ensure_backup(&config->backup_opts, BACKUP_LOCAL, &config->opts[n]);
}

void m_config_backup_watch_later_opts(struct m_config *config)
{
    for (int n = 0; n < config->num_opts; n++)
        ensure_backup(&config->watch_later_backup_opts, 0, &config->opts[n]);
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
    return m_config_shadow_get_opt_default(config->shadow, co->opt_id);
}

const char *m_config_get_positional_option(const struct m_config *config, int p)
{
    int pos = 0;
    for (int n = 0; n < config->num_opts; n++) {
        struct m_config_option *co = &config->opts[n];
        if (!co->opt->deprecation_message) {
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
        ensure_backup(&config->backup_opts, BACKUP_LOCAL, co);

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

static void config_destroy(void *p)
{
    struct m_config *config = p;
    config->option_change_callback = NULL;
    m_config_restore_backups(config);

    struct m_opt_backup **list = &config->watch_later_backup_opts;
    while (*list) {
        struct m_opt_backup *bc = *list;
        *list = bc->next;
        talloc_free(bc);
    }

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
        config->cache = m_config_cache_from_shadow(config, config->shadow, root);
        config->optstruct = config->cache->opts;
    }

    int32_t optid = -1;
    while (m_config_shadow_get_next_opt(config->shadow, &optid)) {
        char buf[M_CONFIG_MAX_OPT_NAME_LEN];
        const char *opt_name =
            m_config_shadow_get_opt_name(config->shadow, optid, buf, sizeof(buf));

        struct m_config_option co = {
            .name = talloc_strdup(config, opt_name),
            .opt = m_config_shadow_get_opt(config->shadow, optid),
            .opt_id = optid,
        };

        if (config->cache)
            co.data = m_config_cache_get_opt_data(config->cache, optid);

        MP_TARRAY_APPEND(config, config->opts, config->num_opts, co);
    }

    return config;
}

// Normally m_config_cache will not send notifications when _we_ change our
// own stuff. For whatever funny reasons, we need that, though.
static void force_self_notify_change_opt(struct m_config *config,
                                         struct m_config_option *co,
                                         bool self_notification)
{
    int changed =
        m_config_cache_get_option_change_mask(config->cache, co->opt_id);

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

    if (config->profile_backup_tmp)
        ensure_backup(config->profile_backup_tmp, config->profile_backup_flags, co);

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

    union m_option_value val = m_option_value_default;

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
    union m_option_value val = m_option_value_default;

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
        if (strcmp(name, "*") != 0 && !strstr(co->name, name))
            continue;
        MP_INFO(config, " %s%-30s", prefix, co->name);
        if (opt->type == &m_option_type_choice) {
            MP_INFO(config, " Choices:");
            const struct m_opt_choice_alternatives *alt = opt->priv;
            for (int n = 0; alt[n].name; n++)
                MP_INFO(config, " %s", alt[n].name);
            if (opt->min < opt->max)
                MP_INFO(config, " (or an integer)");
        } else {
            MP_INFO(config, " %s", opt->type->name);
        }
        if ((opt->type->flags & M_OPT_TYPE_USES_RANGE) && opt->min < opt->max) {
            snprintf(min, sizeof(min), "any");
            snprintf(max, sizeof(max), "any");
            if (opt->min != DBL_MIN)
                snprintf(min, sizeof(min), "%.14g", opt->min);
            if (opt->max != DBL_MAX)
                snprintf(max, sizeof(max), "%.14g", opt->max);
            MP_INFO(config, " (%s to %s)", min, max);
        }
        char *def = NULL;
        const void *defptr = m_config_get_co_default(config, co);
        if (!defptr)
            defptr = &m_option_value_default;
        if (defptr)
            def = m_option_pretty_print(opt, defptr, false);
        if (def) {
            MP_INFO(config, " (default: %s)", def);
            talloc_free(def);
        }
        if (opt->flags & M_OPT_NOCFG)
            MP_INFO(config, " [not in config files]");
        if (opt->flags & M_OPT_FILE)
            MP_INFO(config, " [file]");
        if (opt->deprecation_message)
            MP_INFO(config, " [deprecated]");
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

int m_config_set_profile_option(struct m_config *config, struct m_profile *p,
                                bstr name, bstr val)
{
    if (bstr_equals0(name, "profile-desc")) {
        talloc_free(p->desc);
        p->desc = bstrto0(p, val);
        return 0;
    }
    if (bstr_equals0(name, "profile-cond")) {
        TA_FREEP(&p->cond);
        val = bstr_strip(val);
        if (val.len)
            p->cond = bstrto0(p, val);
        return 0;
    }
    if (bstr_equals0(name, profile_restore_mode_opt.name)) {
        return m_option_parse(config->log, &profile_restore_mode_opt, name, val,
                              &p->restore_mode);
    }

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

static struct m_profile *find_check_profile(struct m_config *config, char *name)
{
    struct m_profile *p = m_config_get_profile0(config, name);
    if (!p) {
        MP_WARN(config, "Unknown profile '%s'.\n", name);
        return NULL;
    }
    if (config->profile_depth > MAX_PROFILE_DEPTH) {
        MP_WARN(config, "WARNING: Profile inclusion too deep.\n");
        return NULL;
    }
    return p;
}

int m_config_set_profile(struct m_config *config, char *name, int flags)
{
    MP_VERBOSE(config, "Applying profile '%s'...\n", name);
    struct m_profile *p = find_check_profile(config, name);
    if (!p)
        return M_OPT_INVALID;

    if (!config->profile_backup_tmp && p->restore_mode) {
        config->profile_backup_tmp = &p->backups;
        config->profile_backup_flags = p->restore_mode == 2 ? BACKUP_NVAL : 0;
    }

    config->profile_depth++;
    for (int i = 0; i < p->num_opts; i++) {
        m_config_set_option_cli(config,
                                bstr0(p->opts[2 * i]),
                                bstr0(p->opts[2 * i + 1]),
                                flags | M_SETOPT_FROM_CONFIG_FILE);
    }
    config->profile_depth--;

    if (config->profile_backup_tmp == &p->backups) {
        config->profile_backup_tmp = NULL;

        for (struct m_opt_backup *bc = p->backups; bc; bc = bc->next) {
            if (bc && bc->nval)
                m_option_copy(bc->co->opt, bc->nval, bc->co->data);
            talloc_steal(p, bc);
        }
    }

    return 0;
}

int m_config_restore_profile(struct m_config *config, char *name)
{
    MP_VERBOSE(config, "Restoring from profile '%s'...\n", name);
    struct m_profile *p = find_check_profile(config, name);
    if (!p)
        return M_OPT_INVALID;

    if (!p->backups)
        MP_WARN(config, "Profile '%s' contains no restore data.\n", name);

    restore_backups(&p->backups, config);

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
        if (profile->cond)
            node_map_add_string(entry, "profile-cond", profile->cond);
        if (profile->restore_mode) {
            char *s =
                m_option_print(&profile_restore_mode_opt, &profile->restore_mode);
            node_map_add_string(entry, profile_restore_mode_opt.name, s);
            talloc_free(s);
        }

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
