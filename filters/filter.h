#pragma once

#include <stdbool.h>

#include "frame.h"

struct mpv_global;
struct mp_filter;

// A filter input or output. These always come in pairs: one mp_pin is for
// input, the other is for output. (The separation is mostly for checking
// their API use, and for the connection functions.)
// Effectively, this is a 1-frame queue. The data flow rules have the goal to
// reduce the number of buffered frames and the amount of time they are
// buffered.
// A mp_pin must be connected to be usable. The default state of a mp_pin is
// a manual connection, which means you use the mp_pin_*() functions to
// manually read or write data.
struct mp_pin;

enum mp_pin_dir {
    MP_PIN_INVALID = 0, // used as a placeholder value
    MP_PIN_IN,          // you write data to the pin
    MP_PIN_OUT,         // you read data from the pin
};

// The established direction for this pin. The direction of a pin is immutable.
// You must use the mp_pin_in_*() and mp_pin_out_*() functions on the correct
// pin type - mismatching it is an API violation.
enum mp_pin_dir mp_pin_get_dir(struct mp_pin *p);

// True if a new frame should be written to the pin.
bool mp_pin_in_needs_data(struct mp_pin *p);

// Write a frame to the pin. If the input was not accepted, false is returned
// (does not normally happen, as long as mp_pin_in_needs_data() returned true).
// The callee owns the reference to the frame data, even on failure.
// Writing a MP_FRAME_NONE has no effect (and returns false).
// If you did not call mp_pin_in_needs_data() before this, it's likely a bug.
bool mp_pin_in_write(struct mp_pin *p, struct mp_frame frame);

// True if a frame is actually available for reading right now, and
// mp_pin_out_read() will return success. If this returns false, the pin is
// flagged for needing data (the filter might either produce output the next
// time it's run, or request new input).
// You should call this only if you can immediately consume the data. The goal
// is to have no redundant buffering in the filter graph, and leaving frames
// buffered in mp_pins goes against this.
bool mp_pin_out_request_data(struct mp_pin *p);

// Same as mp_pin_out_request_data(), but call the filter's process() function
// next time even if there is new data. the intention is that the filter reads
// the data in the next iteration, without checking for the data now.
void mp_pin_out_request_data_next(struct mp_pin *p);

// Same as mp_pin_out_request_data(), but does not attempt to procure new frames
// if the return value is false.
bool mp_pin_out_has_data(struct mp_pin *p);

// Read a frame. Returns MP_FRAME_NONE if currently no frame is available.
// You need to call mp_pin_out_request_data() and wait until the frame is ready
// to be sure this returns a frame. (This call implicitly calls _request if no
// frame is available, but to get proper data flow in filters, you should
// probably follow the preferred conventions.)
// If no frame is returned, a frame is automatically requested via
// mp_pin_out_request_data() (so it might be retuned in the future).
// If a frame is returned, no new frame is automatically requested (this is
// usually not wanted, because it could lead to additional buffering).
// This is guaranteed to return a non-NONE frame if mp_pin_out_has_data()
// returned true and no other filter functions were called.
// The caller owns the reference to the returned data.
struct mp_frame mp_pin_out_read(struct mp_pin *p);

// Undo mp_pin_out_read(). This should be only used in special cases. Normally,
// you should make an effort to reduce buffering, which means you signal that
// you need a frame only once you know that you can use it (meaning you'll
// really use it and have no need to "undo" the read). But in special cases,
// especially if the behavior depends on the exact frame data, using this might
// be justified.
// If this is called, the next mp_pin_out_read() call will return the same frame
// again. You must not have called mp_pin_out_request_data() on this pin and
// you must not have disconnected or changed the pin in any way.
// This does not mark the filter for progress, i.e. the filter's process()
// function won't be repeated (unless other pins change). If you really need
// that, call mp_filter_internal_mark_progress() manually in addition.
void mp_pin_out_unread(struct mp_pin *p, struct mp_frame frame);

// A helper to make draining on MP_FRAME_EOF frames easier. For filters which
// buffer data, but have no easy way to buffer MP_FRAME_EOF frames natively.
// This is to be used as follows:
//  1. caller receives MP_FRAME_EOF
//  2. initiates draining (or continues, see step 4.)
//  2b. if there are no more buffered frames, just propagates the EOF frame and
//      exits
//  3. calls mp_pin_out_repeat_eof(pin)
//  4. returns a buffered frame normally, and continues normally
//  4b. pin returns "repeated" MP_FRAME_EOF, jump to 1.
//  5. if there's nothing more to do, stop
//  5b. there might be a sporadic wakeup, and an unwanted wait for output (in
//      a typical filter implementation)
// You must not have requested data before calling this. (Usually you'd call
// this after mp_pin_out_read(). Requesting data after queuing the repeat EOF
// is OK and idempotent.)
// This is equivalent to mp_pin_out_unread(p, MP_EOF_FRAME). See that function
// for further remarks.
void mp_pin_out_repeat_eof(struct mp_pin *p);

// Trivial helper to determine whether src is readable and dst is writable right
// now. Defers or requests new data if not ready. This means it has the side
// effect of telling the filters that you want to transfer data.
// You use this in a filter process() function. If the result is false, it will
// have requested new output from src, and your process() function will be
// called again once src has output and dst is accepts input (the latest).
bool mp_pin_can_transfer_data(struct mp_pin *dst, struct mp_pin *src);

// Trivial helper to copy data between two manual pins. This uses filter data
// flow - so if data can't be copied, it requests the pins to make it possible
// on the next filter run. This implies you call this either from a filter
// process() function, or call it manually when needed. Also see
// mp_pin_can_transfer_data(). Returns whether a transfer happened.
bool mp_pin_transfer_data(struct mp_pin *dst, struct mp_pin *src);

// Connect src and dst, for automatic data flow. Pin src will reflect the request
// state of pin dst, and accept and pass down frames to dst when appropriate.
// src must be MP_PIN_OUT, dst must be MP_PIN_IN.
// Previous connections are always removed. If the pins were already connected,
// no action is taken.
// Creating circular connections will just cause infinite recursion or such.
// Both API user and filter implementations can use this, but always only on
// the pins they're allowed to access.
void mp_pin_connect(struct mp_pin *dst, struct mp_pin *src);

// Enable manual filter access. This means you want to directly use the
// mp_pin_in*() and mp_pin_out_*() functions for data flow.
// Always severs previous connections.
void mp_pin_set_manual_connection(struct mp_pin *p, bool connected);

// Enable manual filter access, like mp_pin_set_manual_connection(). In
// addition, this specifies which filter's process function should be invoked
// on pin state changes. Using mp_pin_set_manual_connection() will default to
// the parent filter for this.
// Passing f=NULL disconnects.
void mp_pin_set_manual_connection_for(struct mp_pin *p, struct mp_filter *f);

// Return the manual connection for this pin, or NULL if none.
struct mp_filter *mp_pin_get_manual_connection(struct mp_pin *p);

// Disconnect the pin, possibly breaking connections.
void mp_pin_disconnect(struct mp_pin *p);

// Return whether a connection was set on this pin. Note that this is not
// transitive (if the pin is connected to an pin with no further connections,
// there is no active connection, but this still returns true).
bool mp_pin_is_connected(struct mp_pin *p);

// Return a symbolic name of the pin. Usually it will be something redundant
// (like "in" or "out"), or something the user set.
// The returned pointer is valid as long as the mp_pin is allocated.
const char *mp_pin_get_name(struct mp_pin *p);

/**
 * A filter converts input frames to output frames (mp_frame, usually audio or
 * video data). It can support multiple inputs and outputs. Data always flows
 * through mp_pin instances.
 *
 * --- General rules for data flow:
 *
 * All data goes through mp_pin (present in the mp_filter inputs/outputs list).
 * Actual work is done in the filter's process() function. This function
 * queries whether input mp_pins have data and output mp_pins require data. If
 * both is the case, a frame is read, filtered, and written to the output.
 * Depending on the filter type, the filter might internally buffer data (e.g.
 * things that require readahead). But in general, a filter should not request
 * input before output is needed.
 *
 * The general goal is to reduce the amount of data buffered. This is why
 * mp_pins buffer at most 1 frame, and the API is designed such that queued
 * data in pins will be immediately passed to the next filter. If buffering is
 * actually desired, explicit filters for buffering have to be introduced into
 * the filter chain.
 *
 * Typically a filter will do something like this:
 *
 *  process(struct mp_filter *f) {
 *      if (!mp_pin_in_needs_data(f->ppins[1]))
 *          return; // reader needs no output yet, so stop filtering
 *      if (!have_enough_data_for_output) {
 *          // Could check mp_pin_out_request_data(), but often just trying to
 *          // read is enough, as a failed read will request more data.
 *          struct mp_frame fr = mp_pin_out_read_data(f->ppins[0]);
 *          if (!fr.type)
 *              return; // no frame was returned - data was requested, and will
 *                      // be queued when available, and invoke process() again
 *           ... do something with fr here ...
 *      }
 *      ... produce output frame (i.e. actual filtering) ...
 *      mp_pin_in_write(f->ppins[1], output_frame);
 *  }
 *
 * Simpler filters can use utility functions like mp_pin_can_transfer_data(),
 * which reduce the boilerplate. Such filters also may not need to buffer data
 * as internal state.
 *
 * --- Driving filters:
 *
 * The filter root (created by mp_filter_create_root()) will internally create
 * a graph runner, that can be entered with mp_filter_run(). This will check if
 * any filter/pin has unhandled requests, and call filter process() functions
 * accordingly. Outside of the filter, this can be triggered implicitly via the
 * mp_pin_* functions.
 *
 * Multiple filters are driven by letting mp_pin flag filters which need
 * process() to be called. The process starts by requesting output from the
 * last filter. The requests will "bubble up" by iteratively calling process()
 * on each filter, which will request further input, until input on the first
 * filter's input pin is requested. The API user feeds it a frame, which will
 * call the first filter's process() function, which will filter and output
 * the frame, and the frame is iteratively filtered until it reaches the output.
 *
 * (The mp_pin_* calls can recursively start filtering, but this is only the
 * case if you access a separate graph with a different filter root. Most
 * importantly, calling them from outside the filter's process() function (e.g.
 * an outside filter user) will enter filtering. Within the filter, mp_pin_*
 * will usually only set or query flags.)
 *
 * --- General rules for thread safety:
 *
 * Filters are by default not thread safe. However, some filters can be
 * partially thread safe and allow certain functions to be accessed from
 * foreign threads. The common filter code itself is not thread safe, except
 * for some utility functions explicitly marked as such, and which are meant
 * to make implementing threaded filters easier.
 *
 * --- Rules for manual connections:
 *
 * A pin can be marked for manual connection via mp_pin_set_manual_connection().
 * It's also the default. These have two uses:
 *
 *      1. filter internal (the filter actually does something with a frame)
 *      2. filter user manually feeding/retrieving frames
 *
 * Basically, a manual connection means someone uses the mp_pin_in_*() or
 * mp_pin_out_*() functions on a pin. The alternative is an automatic connection
 * made via mp_pin_connect(). Manual connections need special considerations
 * for wakeups:
 *
 * Internal manual pins (within a filter) will invoke the filter's process()
 * function, and the filter polls the state of all pins to see if anything
 * needs to be filtered or requested.
 *
 * External manual pins (filter user) require the user to poll all manual pins
 * that are part of the graph. In addition, the filter's wakeup callback must be
 * set, and trigger repolling all pins. This is needed in case any filters do
 * async filtering internally.
 *
 * --- Rules for filters with multiple inputs or outputs:
 *
 * The generic filter code does not do any kind of scheduling. It's the filter's
 * responsibility to request frames from input when needed, and to avoid
 * internal excessive buffering if outputs aren't read.
 *
 * --- Rules for async filters:
 *
 * Async filters will have a synchronous interface with asynchronous waiting.
 * They change mp_pin data flow to being poll based, with a wakeup mechanism to
 * avoid active waiting. Once polling results in no change, the API user can go
 * to sleep, and wait until the wakeup callback set via mp_filter_create_root()
 * is invoked. Then it can poll the filters again. Internally, filters use
 * mp_filter_wakeup() to get their process() function invoked on the user
 * thread, and update the mp_pin states.
 *
 * For running parts of a filter graph on a different thread, f_async_queue.h
 * can be used.
 *
 * --- Format conversions and mid-stream format changes:
 *
 * Generally, all filters must support all formats, as well as mid-stream
 * format changes. If they don't, they will have to error out. There are some
 * helpers for dealing with these two things.
 *
 * mp_pin_out_unread() can temporarily put back an input frame. If the input
 * format changed, and you have to drain buffered data, you can put back the
 * frame every time you output a buffered frame. Once all buffered data is
 * drained this way, you can actually change the internal filter state to the
 * new format, and actually consume the input frame.
 *
 * There is an f_autoconvert filter, which lets you transparently convert to
 * a set of target formats (and which passes through the data if no conversion
 * is needed).
 *
 * --- Rules for format negotiation:
 *
 * Since libavfilter does not provide _any_ kind of format negotiation to the
 * user, and most filters use the libavfilter wrapper anyway, this is pretty
 * broken and rudimentary. (The only thing libavfilter provides is that you
 * can try to create a filter with a specific input format. Then you get
 * either failure, or an output format. It involves actually initializing all
 * filters, so a try run is not cheap or even side effect free.)
 */
struct mp_filter {
    // Private state for the filter implementation. API users must not access
    // this.
    void *priv;

    struct mpv_global *global;
    struct mp_log *log;

    // Array of public pins. API users can read this, but are not allowed to
    // modify the array. Filter implementations use mp_filter_add_pin() to add
    // pins to the array. The array is in order of the add calls.
    // Most filters will use pins[0] for input (MP_PIN_IN), and pins[1] for
    // output (MP_PIN_OUT). This is the default convention for filters. Some
    // filters may have more complex usage, and assign pin entries with
    // different meanings.
    // The filter implementation must not use this. It must access ppins[]
    // instead.
    struct mp_pin **pins;
    int num_pins;

    // Internal pins, for access by the filter implementation. The meaning of
    // in/out is swapped from the public interface: inputs use MP_PIN_OUT,
    // because the filter reads from the inputs, and outputs use MP_PIN_IN,
    // because the filter writes to them. ppins[n] always corresponds to pin[n],
    // with swapped direction, and implicit data flow between the two.
    // Outside API users must not access this.
    struct mp_pin **ppins;

    // Dumb garbage.
    struct mp_stream_info *stream_info;

    // Private state for the generic filter code.
    struct mp_filter_internal *in;
};

// Return a symbolic name, which is set at init time. NULL if no name.
// Valid until filter is destroyed or next mp_filter_set_name() call.
const char *mp_filter_get_name(struct mp_filter *f);

// Change mp_filter_get_name() return value.
void mp_filter_set_name(struct mp_filter *f, const char *name);

// Get a pin from f->pins[] for which mp_pin_get_name() returns the same name.
// If name is NULL, always return NULL.
struct mp_pin *mp_filter_get_named_pin(struct mp_filter *f, const char *name);

// Return true if the filter has failed in some fatal way that does not allow
// it to continue. This resets the error state (but does not reset the child
// failed status on any parent filter).
bool mp_filter_has_failed(struct mp_filter *filter);

// Invoke mp_filter_info.reset on this filter and all children (but not
// other filters connected via pins).
void mp_filter_reset(struct mp_filter *filter);

enum mp_filter_command_type {
    MP_FILTER_COMMAND_TEXT = 1,
    MP_FILTER_COMMAND_GET_META,
    MP_FILTER_COMMAND_SET_SPEED,
    MP_FILTER_COMMAND_SET_SPEED_RESAMPLE,
    MP_FILTER_COMMAND_IS_ACTIVE,
};

struct mp_filter_command {
    enum mp_filter_command_type type;

    // For MP_FILTER_COMMAND_TEXT
    const char *cmd;
    const char *arg;

    // For MP_FILTER_COMMAND_GET_META
    void *res; // must point to struct mp_tags*, will be set to new instance

    // For MP_FILTER_COMMAND_SET_SPEED and MP_FILTER_COMMAND_SET_SPEED_RESAMPLE
    double speed;

    // For MP_FILTER_COMMAND_IS_ACTIVE
    bool is_active;
};

// Run a command on the filter. Returns success. For libavfilter.
bool mp_filter_command(struct mp_filter *f, struct mp_filter_command *cmd);

// Specific information about a sub-tree in a filter graph. Currently, this is
// mostly used to give filters access to VO mechanisms and capabilities.
struct mp_stream_info {
    void *priv; // for use by whoever implements the callbacks

    double (*get_display_fps)(struct mp_stream_info *i);

    struct mp_hwdec_devices *hwdec_devs;
    struct osd_state *osd;
    bool rotate90;
    struct vo *dr_vo; // for calling vo_get_image()
};

// Search for a parent filter (including f) that has this set, and return it.
struct mp_stream_info *mp_filter_find_stream_info(struct mp_filter *f);

struct AVBufferRef;
struct AVBufferRef *mp_filter_load_hwdec_device(struct mp_filter *f, int avtype);

// Perform filtering. This runs until the filter graph is blocked (due to
// missing external input or unread output). It returns whether any outside
// pins have changed state.
// Does not perform recursive filtering to connected filters with different
// root filter, though it notifies them.
bool mp_filter_run(struct mp_filter *f);

// Create a root dummy filter with no inputs or outputs. This fulfills the
// following functions:
// - passing it as parent filter to top-level filters
// - driving the filter loop between the shared filters
// - setting the wakeup callback for async filtering
// - implicitly passing down global data like mpv_global and keeping filter
//   constructor functions simple
// Note that you can still connect pins of filters with different parents or
// root filters, but then you may have to manually invoke mp_filter_run() on
// the root filters of the connected filters to drive data flow.
struct mp_filter *mp_filter_create_root(struct mpv_global *global);

// Asynchronous filters may need to wakeup the user thread if the status of any
// mp_pin has changed. If this is called, the callback provider should get the
// user's thread to call mp_filter_run() again.
// The wakeup callback must not recursively call into any filter APIs, or do
// blocking waits on the filter API (deadlocks will happen).
void mp_filter_root_set_wakeup_cb(struct mp_filter *root,
                                  void (*wakeup_cb)(void *ctx), void *ctx);

// Debugging internal stuff.
void mp_filter_dump_states(struct mp_filter *f);
