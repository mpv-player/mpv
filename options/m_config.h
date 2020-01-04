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

#ifndef MPLAYER_M_CONFIG_H
#define MPLAYER_M_CONFIG_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "misc/bstr.h"

// m_config provides an API to manipulate the config variables in MPlayer.
// It makes use of the Options API to provide a context stack that
// allows saving and later restoring the state of all variables.

typedef struct m_profile m_profile_t;
struct m_option;
struct m_option_type;
struct m_sub_options;
struct m_obj_desc;
struct m_obj_settings;
struct mp_log;
struct mp_dispatch_queue;

// Config option
struct m_config_option {
    bool is_hidden : 1;             // Does not show up in help
    bool is_set_from_cmdline : 1;   // Set by user from command line
    bool is_set_from_config : 1;    // Set by a config file
    bool is_set_locally : 1;        // Has a backup entry
    bool warning_was_printed : 1;
    int16_t group_index;            // Index into m_config.groups
    const char *name;               // Full name (ie option-subopt)
    const struct m_option *opt;     // Option description
    void *data;                     // Raw value of the option
};

// Config object
/** \ingroup Config */
typedef struct m_config {
    struct mp_log *log;
    struct mpv_global *global; // can be NULL

    // Registered options.
    struct m_config_option *opts; // all options, even suboptions
    int num_opts;

    // List of defined profiles.
    struct m_profile *profiles;
    // Depth when recursively including profiles.
    int profile_depth;

    struct m_opt_backup *backup_opts;

    bool use_profiles;
    bool is_toplevel;
    int (*includefunc)(void *ctx, char *filename, int flags);
    void *includefunc_ctx;

    // Notification after an option was successfully written to.
    // Uses flags as set in UPDATE_OPTS_MASK.
    // self_update==true means the update was caused by a call to
    // m_config_notify_change_opt_ptr(). If false, it's caused either by
    // m_config_set_option_*() (and similar) calls or external updates.
    void (*option_change_callback)(void *ctx, struct m_config_option *co,
                                   int flags, bool self_update);
    void *option_change_callback_ctx;

    // For the command line parser
    int recursion_depth;

    void *optstruct; // struct mpopts or other

    // Private. Non-NULL if data was allocated. m_config_option.data uses it.
    // API users call m_config_set_update_dispatch_queue() to get async updates.
    struct m_config_cache *cache;

    // Private. Thread-safe shadow memory; only set for the main m_config.
    struct m_config_shadow *shadow;
} m_config_t;

// Create a new config object.
//  talloc_ctx: talloc parent context for the m_config allocation
//  root: description of all options
// Note that the m_config object will keep pointers to root and log.
struct m_config *m_config_new(void *talloc_ctx, struct mp_log *log,
                              const struct m_sub_options *root);

// Create a m_config for the given desc. This is for --af/--vf, which have
// different sub-options for every filter (represented by separate desc
// structs).
// args is an array of key/value pairs (args=[k0, v0, k1, v1, ..., NULL]).
// name/defaults is only needed for the legacy af-defaults/vf-defaults options.
struct m_config *m_config_from_obj_desc_and_args(void *ta_parent,
    struct mp_log *log, struct mpv_global *global, struct m_obj_desc *desc,
    const char *name, struct m_obj_settings *defaults, char **args);

// Like m_config_from_obj_desc_and_args(), but don't allocate option the
// struct, i.e. m_config.optstruct==NULL. This is used by the sub-option
// parser (--af/--vf, to a lesser degree --ao/--vo) to check sub-option names
// and types.
struct m_config *m_config_from_obj_desc_noalloc(void *talloc_ctx,
                                                struct mp_log *log,
                                                struct m_obj_desc *desc);

// Allocate a priv struct that is backed by global options (like AOs and VOs,
// anything that uses m_obj_list.use_global_options == true).
// The result contains a snapshot of the current option values of desc->options.
// For convenience, desc->options can be NULL; then priv struct is allocated
// with just zero (or priv_defaults if set).
void *m_config_group_from_desc(void *ta_parent, struct mp_log *log,
        struct mpv_global *global, struct m_obj_desc *desc, const char *name);

// Make sure the option is backed up. If it's already backed up, do nothing.
// All backed up options can be restored with m_config_restore_backups().
void m_config_backup_opt(struct m_config *config, const char *opt);

// Call m_config_backup_opt() on all options.
void m_config_backup_all_opts(struct m_config *config);

// Restore all options backed up with m_config_backup_opt(), and delete the
// backups afterwards.
void m_config_restore_backups(struct m_config *config);

enum {
    M_SETOPT_PRE_PARSE_ONLY = 1,    // Silently ignore non-M_OPT_PRE_PARSE opt.
    M_SETOPT_CHECK_ONLY = 2,        // Don't set, just check name/value
    M_SETOPT_FROM_CONFIG_FILE = 4,  // Reject M_OPT_NOCFG opt. (print error)
    M_SETOPT_FROM_CMDLINE = 8,      // Mark as set by command line
    M_SETOPT_BACKUP = 16,           // Call m_config_backup_opt() before
    M_SETOPT_PRESERVE_CMDLINE = 32, // Don't set if already marked as FROM_CMDLINE
    M_SETOPT_NO_PRE_PARSE = 128,    // Reject M_OPT_PREPARSE options
    M_SETOPT_NO_OVERWRITE = 256,    // Skip options marked with FROM_*
};

// Set the named option to the given string. This is for command line and config
// file use only.
// flags: combination of M_SETOPT_* flags (0 for normal operation)
// Returns >= 0 on success, otherwise see OptionParserReturn.
int m_config_set_option_cli(struct m_config *config, struct bstr name,
                            struct bstr param, int flags);

// Similar to m_config_set_option_cli(), but set as data in its native format.
// This takes care of some details like sending change notifications.
// The type data points to is as in: co->opt
int m_config_set_option_raw(struct m_config *config, struct m_config_option *co,
                            void *data, int flags);

void m_config_mark_co_flags(struct m_config_option *co, int flags);

// Convert the mpv_node to raw option data, then call m_config_set_option_raw().
struct mpv_node;
int m_config_set_option_node(struct m_config *config, bstr name,
                             struct mpv_node *data, int flags);

// Return option descriptor. You shouldn't use this.
struct m_config_option *m_config_get_co(const struct m_config *config,
                                        struct bstr name);
// Same as above, but does not resolve aliases or trigger warning messages.
struct m_config_option *m_config_get_co_raw(const struct m_config *config,
                                            struct bstr name);

// Special uses only. Look away.
int m_config_get_co_count(struct m_config *config);
struct m_config_option *m_config_get_co_index(struct m_config *config, int index);
const void *m_config_get_co_default(const struct m_config *config,
                                    struct m_config_option *co);

// Return the n-th option by position. n==0 is the first option. If there are
// less than (n + 1) options, return NULL.
const char *m_config_get_positional_option(const struct m_config *config, int n);

// Return a hint to the option parser whether a parameter is/may be required.
// The option may still accept empty/non-empty parameters independent from
// this, and this function is useful only for handling ambiguous options like
// flags (e.g. "--a" is ok, "--a=yes" is also ok).
// Returns: error code (<0), or number of expected params (0, 1)
int m_config_option_requires_param(struct m_config *config, bstr name);

// Notify m_config_cache users that the option has (probably) changed its value.
// This will force a self-notification back to config->option_change_callback.
void m_config_notify_change_opt_ptr(struct m_config *config, void *ptr);

// Exactly like m_config_notify_change_opt_ptr(), but the option change callback
// (config->option_change_callback()) is invoked with self_update=false, if at all.
void m_config_notify_change_opt_ptr_notify(struct m_config *config, void *ptr);

// Return all (visible) option names as NULL terminated string list.
char **m_config_list_options(void *ta_parent, const struct m_config *config);

void m_config_print_option_list(const struct m_config *config, const char *name);


/*  Find the profile with the given name.
 *  \param config The config object.
 *  \param arg The profile's name.
 *  \return The profile object or NULL.
 */
struct m_profile *m_config_get_profile0(const struct m_config *config,
                                        char *name);
struct m_profile *m_config_get_profile(const struct m_config *config, bstr name);

// Apply and clear the default profile - it's the only profile that new config
// files do not simply append to (for configfile parser).
void m_config_finish_default_profile(struct m_config *config, int flags);

/*  Get the profile with the given name, creating it if necessary.
 *  \param config The config object.
 *  \param arg The profile's name.
 *  \return The profile object.
 */
struct m_profile *m_config_add_profile(struct m_config *config, char *name);

/*  Set the description of a profile.
 *  Used by the config file parser when defining a profile.
 *
 *  \param p The profile object.
 *  \param arg The profile's name.
 */
void m_profile_set_desc(struct m_profile *p, bstr desc);

/*  Add an option to a profile.
 *  Used by the config file parser when defining a profile.
 *
 *  \param config The config object.
 *  \param p The profile object.
 *  \param name The option's name.
 *  \param val The option's value.
 */
int m_config_set_profile_option(struct m_config *config, struct m_profile *p,
                                bstr name, bstr val);

/*  Enables profile usage
 *  Used by the config file parser when loading a profile.
 *
 *  \param config The config object.
 *  \param p The profile object.
 *  \param flags M_SETOPT_* bits
 * Returns error code (<0) or 0 on success
 */
int m_config_set_profile(struct m_config *config, char *name, int flags);

struct mpv_node m_config_get_profiles(struct m_config *config);

// Run async option updates here. This will call option_change_callback() on it.
void m_config_set_update_dispatch_queue(struct m_config *config,
                                        struct mp_dispatch_queue *dispatch);

// This can be used to create and synchronize per-thread option structs,
// which then can be read without synchronization. No concurrent access to
// the cache itself is allowed.
struct m_config_cache {
    // The struct as indicated by m_config_cache_alloc's group parameter.
    // (Internally the same as internal->gdata[0]->udata.)
    void *opts;
    // Accumulated change flags. The user can set this to 0 to unset all flags.
    // They are set when calling any of the update functions. A flag is only set
    // once the new value is visible in ->opts.
    uint64_t change_flags;

    // Set to non-NULL for logging all option changes as they are retrieved
    // with one of the update functions (like m_config_cache_update()).
    struct mp_log *debug;

    // Do not access.
    struct config_cache *internal;
};

// Create a mirror copy from the global options.
// Keep in mind that a m_config_cache object is not thread-safe; it merely
// provides thread-safe access to the global options. All API functions for
// the same m_config_cache object must synchronized, unless otherwise noted.
// This does not create an initial change event (m_config_cache_update() will
// return false), but note that a change might be asynchronously signaled at any
// time.
//  ta_parent: parent for the returned allocation
//  global: option data source
//  group: the option group to return
struct m_config_cache *m_config_cache_alloc(void *ta_parent,
                                            struct mpv_global *global,
                                            const struct m_sub_options *group);

// If any of the options in the group possibly changes, call this callback. The
// callback must not actually access the cache or anything option related.
// Instead, it must wake up the thread that normally accesses the cache.
void m_config_cache_set_wakeup_cb(struct m_config_cache *cache,
                                  void (*cb)(void *ctx), void *cb_ctx);

// If any of the options in the group change, call this callback on the given
// dispatch queue. This is higher level than m_config_cache_set_wakeup_cb(),
// and you can do anything you want in the callback (assuming the dispatch
// queue is processed in the same thread that accesses m_config_cache API).
// To ensure clean shutdown, you must destroy the m_config_cache (or unset the
// callback) before the dispatch queue is destroyed.
void m_config_cache_set_dispatch_change_cb(struct m_config_cache *cache,
                                           struct mp_dispatch_queue *dispatch,
                                           void (*cb)(void *ctx), void *cb_ctx);

// Update the options in cache->opts to current global values. Return whether
// there was an update notification at all (which may or may not indicate that
// some options have changed).
// Keep in mind that while the cache->opts pointer does not change, the option
// data itself will (e.g. string options might be reallocated).
// New change flags are or-ed into cache->change_flags with this call (if you
// use them, you should probably do cache->change_flags=0 before this call).
bool m_config_cache_update(struct m_config_cache *cache);

// Check for changes and return fine grained change information.
// Warning: this conflicts with m_config_cache_update(). If you call
//          m_config_cache_update(), all options will be marked as "not changed",
//          and this function will return false. Also, calling this function and
//          then m_config_cache_update() is not supported, and may skip updating
//          some fields.
// This returns true as long as there is a changed option, and false if all
// changed options have been returned.
// If multiple options have changed, the new option value is visible only once
// this function has returned the change for it.
//  out_ptr: pointer to a void*, which is set to the cache->opts field associated
//           with the changed option if the function returns true; set to NULL
//           if no option changed.
//  returns: *out_ptr!=NULL (true if there was a changed option)
bool m_config_cache_get_next_changed(struct m_config_cache *cache, void **out_ptr);

// Copy the option field pointed to by ptr to the global option storage. This
// is sort of similar to m_config_set_option_raw(), except doesn't require
// access to the main thread. (And you can't pass any flags.)
// You write the new value to the option struct, and then call this function
// with the pointer to it. You will not get a change notification for it (though
// you might still get a redundant wakeup callback).
// Changing the option struct and not calling this function before any update
// function (like m_config_cache_update()) will leave the value inconsistent,
// and will possibly (but not necessarily) overwrite it with the next update
// call.
//  ptr: points to any field in cache->opts that is managed by an option. If
//       this is not the case, the function crashes for your own good.
//  returns: if true, this was an update; if false, shadow had same value
bool m_config_cache_write_opt(struct m_config_cache *cache, void *ptr);

// Like m_config_cache_alloc(), but return the struct (m_config_cache->opts)
// directly, with no way to update the config. Basically this returns a copy
// with a snapshot of the current option values.
void *mp_get_config_group(void *ta_parent, struct mpv_global *global,
                          const struct m_sub_options *group);

// Read a single global option in a thread-safe way. For multiple options,
// use m_config_cache. The option must exist and match the provided type (the
// type is used as a sanity check only). Performs semi-expensive lookup.
// Warning: new code must not use this.
void mp_read_option_raw(struct mpv_global *global, const char *name,
                        const struct m_option_type *type, void *dst);

#endif /* MPLAYER_M_CONFIG_H */
