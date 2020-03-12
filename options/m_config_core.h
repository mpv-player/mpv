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

struct mp_dispatch_queue;
struct m_sub_options;
struct m_option_type;
struct m_option;
struct mpv_global;

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

    // Global instance of option data. Read only.
    struct m_config_shadow *shadow;

    // Do not access.
    struct config_cache *internal;
};

// Maximum possibly option name buffer length (as it appears to the user).
#define M_CONFIG_MAX_OPT_NAME_LEN 80

// Create a mirror copy from the global options.
// Keep in mind that a m_config_cache object is not thread-safe; it merely
// provides thread-safe access to the global options. All API functions for
// the same m_config_cache object must synchronized, unless otherwise noted.
// This does not create an initial change event (m_config_cache_update() will
// return false), but note that a change might be asynchronously signaled at any
// time.
// This simply calls m_config_cache_from_shadow(ta_parent, global->shadow, group).
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

// Allocate a priv struct that is backed by global options (like AOs and VOs,
// anything that uses m_obj_list.use_global_options == true).
// The result contains a snapshot of the current option values of desc->options.
// For convenience, desc->options can be NULL; then priv struct is allocated
// with just zero (or priv_defaults if set).
// Bad function.
struct m_obj_desc;
void *m_config_group_from_desc(void *ta_parent, struct mp_log *log,
        struct mpv_global *global, struct m_obj_desc *desc, const char *name);

// Allocate new option shadow storage with all options set to defaults.
// root must stay valid for the lifetime of the return value.
// Result can be freed with ta_free().
struct m_config_shadow *m_config_shadow_new(const struct m_sub_options *root);

// See m_config_cache_alloc().
struct m_config_cache *m_config_cache_from_shadow(void *ta_parent,
                                            struct m_config_shadow *shadow,
                                            const struct m_sub_options *group);

// Iterate over all registered global options. *p_id must be set to -1 when this
// is called for the first time. Each time this call returns true, *p_id is set
// to a new valid option ID. p_id must not be changed for the next call. If
// false is returned, iteration ends.
bool m_config_shadow_get_next_opt(struct m_config_shadow *shadow, int32_t *p_id);

// Similar to m_config_shadow_get_next_opt(), but return only options that are
// covered by the m_config_cache.
bool m_config_cache_get_next_opt(struct m_config_cache *cache, int32_t *p_id);

// Return the m_option that was used to declare this option.
// id must be a valid option ID as returned by m_config_shadow_get_next_opt() or
// m_config_cache_get_next_opt().
const struct m_option *m_config_shadow_get_opt(struct m_config_shadow *shadow,
                                               int32_t id);

// Return the full (global) option name. buf must be supplied, but may not
// always be used. It should have the size M_CONFIG_MAX_OPT_NAME_LEN.
// The returned point points either to buf or a static string.
// id must be a valid option ID as returned by m_config_shadow_get_next_opt() or
// m_config_cache_get_next_opt().
const char *m_config_shadow_get_opt_name(struct m_config_shadow *shadow,
                                         int32_t id, char *buf, size_t buf_size);

// Pointer to default value, using m_option.type. NULL if option without data.
// id must be a valid option ID as returned by m_config_shadow_get_next_opt() or
// m_config_cache_get_next_opt().
const void *m_config_shadow_get_opt_default(struct m_config_shadow *shadow,
                                            int32_t id);

// Return the pointer to the allocated option data (the same pointers that are
// returned by m_config_cache_get_next_changed()). NULL if option without data.
// id must be a valid option ID as returned by m_config_cache_get_next_opt().
void *m_config_cache_get_opt_data(struct m_config_cache *cache, int32_t id);

// Return or-ed UPDATE_OPTS_MASK part of the option and containing sub-options.
// id must be a valid option ID as returned by m_config_cache_get_next_opt().
uint64_t m_config_cache_get_option_change_mask(struct m_config_cache *cache,
                                               int32_t id);

#endif /* MPLAYER_M_CONFIG_H */
