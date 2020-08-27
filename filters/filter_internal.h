#pragma once

#include <stddef.h>

#include "filter.h"

// Flag the thread as needing mp_filter_process() to be called. Useful for
// (some) async filters only. Idempotent.
// Explicitly thread-safe.
void mp_filter_wakeup(struct mp_filter *f);

// Same as mp_filter_wakeup(), but skip the wakeup, and only mark the filter
// as requiring processing to possibly update pin states changed due to async
// processing.
// Explicitly thread-safe.
void mp_filter_mark_async_progress(struct mp_filter *f);

// Flag the thread as needing mp_filter_process() to be called. Unlike
// mp_filter_wakeup(), not thread-safe, and must be called from the process()
// function of f (in exchange this is very light-weight).
// In practice, this means process() is repeated.
void mp_filter_internal_mark_progress(struct mp_filter *f);

// Flag the filter as having failed, and propagate the error to the parent
// filter. The error propagation stops either at the root filter, or if a filter
// has an error handler set.
// Must be called from f's process function.
void mp_filter_internal_mark_failed(struct mp_filter *f);

// If handler is not NULL, then if filter f errors, don't propagate the error
// flag to its parent. Also invoke the handler's process() function, which is
// supposed to use mp_filter_has_failed(f) to check any filters for which it has
// set itself as error handler.
// A filter must manually unset itself as error handler if it gets destroyed
// before the filter f, otherwise dangling pointers will occur.
void mp_filter_set_error_handler(struct mp_filter *f, struct mp_filter *handler);

// Add a pin. Returns the private handle (same as f->ppins[f->num_pins-1]).
// The name must be unique across all filter pins (you must verify this
// yourself if filter names are from user input). name=="" is not allowed.
// Never returns NULL. dir should be the external filter direction (a filter
// input will use dir==MP_PIN_IN, and the returned pin will use MP_PIN_OUT,
// because the internal pin is the opposite end of the external one).
struct mp_pin *mp_filter_add_pin(struct mp_filter *f, enum mp_pin_dir dir,
                                 const char *name);

// Remove and deallocate a pin. The caller must be sure that nothing else
// references the pin anymore. You must pass the private pin (from
// mp_filter.ppin). This removes/deallocates the public paired pin as well.
void mp_filter_remove_pin(struct mp_filter *f, struct mp_pin *p);

// Free all filters which have f as parent. (This has nothing to do with
// talloc.)
void mp_filter_free_children(struct mp_filter *f);

struct mp_filter_params;

struct mp_filter_info {
    // Informational name, in some cases might be used for filter discovery.
    const char *name;

    // mp_filter.priv is set to memory allocated with this size (if > 0)
    size_t priv_size;

    // Called during mp_filter_create(). Optional, can be NULL if use of a
    // constructor function is required, which sets up the real filter after
    // creation. Actually turns out nothing uses this.
    bool (*init)(struct mp_filter *f, struct mp_filter_params *params);

    // Free internal resources. Optional.
    void (*destroy)(struct mp_filter *f);

    // Called if any mp_pin was signalled (i.e. likely new data to process), or
    // an async wakeup was received some time earlier.
    // Generally, the implementation would consist of 2 stages:
    //  1. check for the pin states, possibly request/probe for input/output
    //  2. if data flow can happen, read a frame, perform actual work, write
    //     result
    // The process function will usually run very often, when pin states are
    // updated, so the generic code can determine where data flow can happen.
    // The common case will be that process() is called running stage 1 a bunch
    // of times, until it finally can run stage 2 too.
    // Optional.
    void (*process)(struct mp_filter *f);

    // Clear internal state and buffers (e.g. on seeks). Filtering can restart
    // after this, and all settings are preserved. It makes sense to preserve
    // internal resources for further filtering as well if you can.
    // Input/output pins are always cleared by the common code before invoking
    // this callback.
    // Optional, can be NULL for filters without state.
    // Don't create or destroy filters in this function, don't reconnect pins,
    // don't access pins.
    void (*reset)(struct mp_filter *f);

    // Send a command to the filter. Highly implementation specific, usually
    // user-initiated. Optional.
    bool (*command)(struct mp_filter *f, struct mp_filter_command *cmd);
};

// Return the mp_filter_info this filter was crated with.
const struct mp_filter_info *mp_filter_get_info(struct mp_filter *f);

// Create a filter instance. Returns NULL on failure.
// Destroy/free with talloc_free().
// This is for filter implementers only. Filters are created with their own
// constructor functions (instead of a generic one), which call this function
// to create the filter itself.
// parent is never NULL; use mp_filter_create_root() to create a top most
// filter.
// The parent does not imply anything about the position of the filter in
// the dataflow (only the mp_pin connections matter). The parent exists for
// convenience, which includes:
//  - passing down implicit and explicit parameters (such as the filter driver
//    loop)
//  - auto freeing child filters if the parent is free'd
//  - propagating errors
//  - setting the parent as default manual connection for new external filter
//    pins
// The parent filter stays valid for the lifetime of any filters having it
// directly or indirectly as parent. If the parent is free'd, all children are
// automatically free'd.
// All filters in the same parent tree must be driven in the same thread (or be
// explicitly synchronized otherwise).
struct mp_filter *mp_filter_create(struct mp_filter *parent,
                                   const struct mp_filter_info *info);

struct mp_filter_params {
    // Identifies the filter and its implementation. The pointer must stay
    // valid for the life time of the created filter instance.
    const struct mp_filter_info *info;

    // Must be set if global==NULL. See mp_filter_create() for remarks.
    struct mp_filter *parent;

    // Must be set if parent==NULL, can otherwise be NULL.
    struct mpv_global *global;

    // Filter specific parameters. Most filters will have a constructor
    // function, and pass in something internal.
    void *params;
};

// Same as mp_filter_create(), but technically more flexible.
struct mp_filter *mp_filter_create_with_params(struct mp_filter_params *params);
