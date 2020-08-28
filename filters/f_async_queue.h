#pragma once

#include "filter.h"

// A thread safe queue, which buffers a configurable number of frames like a
// FIFO. It's part of the filter framework, and intended to provide such a
// queue between filters. Since a filter graph can't be used from multiple
// threads without synchronization, this provides 2 filters, which are
// implicitly connected. (This seemed much saner than having special thread
// safe mp_pins or such in the filter framework.)
struct mp_async_queue;

// Create a blank queue. Can be freed with talloc_free(). To use it, you need
// to create input and output filters with mp_async_queue_create_filter().
// Note that freeing it will only unref it. (E.g. you can free it once you've
// created the input and output filters.)
struct mp_async_queue *mp_async_queue_create(void);

// Clear all queued data and make the queue "inactive". The latter prevents any
// further communication until mp_async_queue_resume() is called.
// For correct operation, you also need to call reset on the access filters
void mp_async_queue_reset(struct mp_async_queue *queue);

// Put the queue into "active" mode. If it wasn't, then the consumer is woken
// up (and if there is no data in the queue, this will in turn wake up the
// producer, i.e. start transfers automatically).
// If there is a writer end but no reader end, this will simply make the queue
// fill up.
void mp_async_queue_resume(struct mp_async_queue *queue);

// Create a filter to access the queue, and connect it. It's not allowed to
// connect an already connected end of the queue. The filter can be freed at
// any time.
//
// The queue starts out in "inactive" mode, where the queue does not allow
// the producer to write any data. You need to call mp_async_queue_resume() to
// start communication. Actual transfers happen only once the consumer filter
// has read requests on its mp_pin.
// If the producer filter requested a new frame from its filter graph, and the
// queue is asynchronously set to "inactive", then the requested frame will be
// silently discarded once it reaches the producer filter.
//
// Resetting a queue filter does not affect the queue at all. Managing the
// queue state is the API user's responsibility. Note that resetting an input
// filter (dir==MP_PIN_IN) while the queue is active and in "reading" state
// (the output filter requested data at any point before the last
// mp_async_queue_reset() was called), the
// filter will immediately request data after the reset.
//
// For proper global reset, this order should be preferred:
//  - mp_async_queue_reset()
//  - reset producer and consumer filters on their respective threads (in any
//    order)
//  - do whatever other reset work is required
//  - mp_async_queue_resume()
//
//  parent: filter graph the filter should be part of (or for standalone use,
//          create one with mp_filter_create_root())
//  dir: MP_PIN_IN for a filter that writes to the queue, MP_PIN_OUT to read
//  queue: queue to attach to (which end of it depends on dir)
// The returned filter will have exactly 1 pin with the requested dir.
struct mp_filter *mp_async_queue_create_filter(struct mp_filter *parent,
                                               enum mp_pin_dir dir,
                                               struct mp_async_queue *queue);

enum mp_async_queue_sample_unit {
    AQUEUE_UNIT_FRAME = 0,  // a frame counts as 1 sample
    AQUEUE_UNIT_SAMPLES,    // number of audio samples (1 for other media types,
                            // 0 for signaling)
};

// Setting this struct to all-0 is equivalent to defaults.
struct mp_async_queue_config {
    // Maximum size of frames buffered. mp_frame_approx_size() is used. May be
    // overshot by up to 1 full frame. Clamped to [1, SIZE_MAX/2].
    int64_t max_bytes;

    // Defines what a "sample" is; affects the fields below.
    enum mp_async_queue_sample_unit sample_unit;

    // Maximum number of frames allowed to be buffered at a time (if
    // unit!=AQUEUE_UNIT_FRAME, can be overshot by the contents of 1 mp_frame).
    // 0 is treated as 1.
    int64_t max_samples;

    // Maximum allowed timestamp difference between 2 frames. This still allows
    // at least 2 samples. Behavior is unclear on timestamp resets (even if EOF
    // frames are between them). A value of 0 disables this completely.
    double max_duration;
};

// Configure the queue size. By default, the queue size is 1 frame.
// The wakeup_threshold_* fields can be used to avoid too frequent wakeups by
// delaying wakeups, and then making the producer to filter multiple frames at
// once.
// In all cases, the filters can still read/write if the producer/consumer got
// woken up by something else.
// If the current queue contains more frames than the new config allows, the
// queue will remain over-allocated until these frames have been read.
void mp_async_queue_set_config(struct mp_async_queue *queue,
                               struct mp_async_queue_config cfg);
