#pragma once

#include "filter.h"

// Filter that computes the exact duration of video frames by buffering 1 frame,
// and taking the PTS difference. This supports video frames only, and stores
// the duration in mp_image.pkt_duration. All other frame types are passed
// through.
struct mp_filter *mp_compute_frame_duration_create(struct mp_filter *parent);

// Given the filters[0..num_filters] array, connect in with the input of the
// first filter, connect the output of the first filter to the input to the
// second filter, etc., until out. All filters are assumed to be bidrectional,
// with input on pin 0 and output on pin 1. NULL entries are skipped.
void mp_chain_filters(struct mp_pin *in, struct mp_pin *out,
                      struct mp_filter **filters, int num_filters);

// Helper for maintaining a sub-filter that is created or destroyed on demand,
// because it might depend on frame input formats or is otherwise dynamically
// changing. (This is overkill for more static sub filters, or entirely manual
// filtering.)
// To initialize this, zero-init all fields, and set the in/out fields.
struct mp_subfilter {
    // These two fields must be set on init. The pins must have a manual
    // connection to the filter whose process() function calls the
    // mp_subfilter_*() functions.
    struct mp_pin *in, *out;
    // Temporary buffered frame, as triggered by mp_subfilter_read(). You can
    // not mutate this (unless you didn't create or destroy sub->filter).
    struct mp_frame frame;
    // The sub-filter, set by the user. Can be NULL if disabled. If set, this
    // must be a bidirectional filter, with manual connections same as
    // mp_sub_filter.in/out (to get the correct process() function called).
    // Set this only if it's NULL. You should not overwrite this if it's set.
    // Use either mp_subfilter_drain_destroy(), mp_subfilter_destroy(), or
    // mp_subfilter_reset() to unset and destroy the filter gracefully.
    struct mp_filter *filter;
    // Internal state.
    bool draining;
};

// Make requests for a new frame.
// Returns whether sub->frame is set to anything. If true is returned, you
// must either call mp_subfilter_continue() or mp_subfilter_drain_destroy()
// once to continue data flow normally (otherwise it will stall). If you call
// mp_subfilter_drain_destroy(), and it returns true, or you call
// mp_subfilter_destroy(), you can call mp_subfilter_continue() once after it.
// If this returns true, sub->frame is never unset (MP_FRAME_NONE).
bool mp_subfilter_read(struct mp_subfilter *sub);

// Clear internal state (usually to be called by parent filter's reset(), or
// destroy()). This usually does not free sub->filter.
void mp_subfilter_reset(struct mp_subfilter *sub);

// Continue filtering sub->frame. This can happen after setting a new filter
// too.
void mp_subfilter_continue(struct mp_subfilter *sub);

// Destroy the filter immediately (if it's set). You must call
// mp_subfilter_continue() after this to propagate sub->frame.
void mp_subfilter_destroy(struct mp_subfilter *sub);

// Make sure the filter is destroyed. Returns true if the filter was destroyed.
// If this returns false, exit your process() function, so dataflow can
// continue normally. (process() is repeated until this function returns true,
// which can take a while if sub->filter has many frames buffered).
// If this returns true, call mp_subfilter_continue() to propagate sub->frame.
// The filter is destroyed with talloc_free(sub->filter).
bool mp_subfilter_drain_destroy(struct mp_subfilter *sub);

// A bidrectional filter which passes through all data.
struct mp_filter *mp_bidir_nop_filter_create(struct mp_filter *parent);

// A filter which repacks audio frame to fixed frame sizes with the given
// number of samples. On hard format changes (sample format/channels/srate),
// the frame can be shorter, unless pad_silence is true. Fails on non-aframes.
struct mp_filter *mp_fixed_aframe_size_create(struct mp_filter *parent,
                                              int samples, bool pad_silence);
