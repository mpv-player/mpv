#include <math.h>
#include <pthread.h>

#include "common/common.h"
#include "common/global.h"
#include "common/msg.h"
#include "osdep/atomic.h"
#include "osdep/timer.h"
#include "video/hwdec.h"

#include "filter.h"
#include "filter_internal.h"

// Note about connections:
// They can be confusing, because pins come in pairs, and multiple pins can be
// transitively connected via mp_pin_connect(). To avoid dealing with this,
// mp_pin.conn is used to skip redundant connected pins.
// Consider <1a|1b> a symbol for mp_pin pair #1 and f1 as filter #1. Then:
//      f1 <-> <1a|1b> <-> <2a|2b> <-> <3a|3b> <-> f2
// would be a connection from 1a to 3b. 1a could be a private pin of f1 (e.g.
// mp_filter.ppin[0]), and 1b would be the public pin (e.g. mp_filter.pin[0]).
// A user could have called mp_pin_connect(2a, 1b) mp_pin_connect(3a, 2b)
// (assuming 1b has dir==MP_PIN_OUT). The end result are the following values:
//  pin  user_conn  conn   manual_connection within_conn (uses mp_pin.data)
//   1a   NULL       3b     f1                false        no
//   1b   2a         NULL   NULL              true         no
//   2a   1b         NULL   NULL              true         no
//   2b   3a         NULL   NULL              true         no
//   3a   2b         NULL   NULL              true         no
//   3b   NULL       1a     f2                false        yes
// The minimal case of f1 <-> <1a|1b> <-> f2 (1b dir=out) would be:
//   1a   NULL       1b     f1                false        no
//   1b   NULL       1a     f2                false        yes
// In both cases, only the final output pin uses mp_pin.data/data_requested.
struct mp_pin {
    const char *name;
    enum mp_pin_dir dir;
    struct mp_pin *other;           // paired mp_pin representing other end
    struct mp_filter *owner;

    struct mp_pin *user_conn;       // as set by mp_pin_connect()
    struct mp_pin *conn;            // transitive, actual end of the connection

    // Set if the pin is considered connected, but has no user_conn. pin
    // state changes are handled by the given filter. (Defaults to the root
    // filter if the pin is for the user of a filter graph.)
    // As an invariant, conn and manual_connection are both either set or unset.
    struct mp_filter *manual_connection;

    // Set if the pin is indirect part of a connection chain, but not one of
    // the end pins. Basically it's a redundant in-between pin. You never access
    // these with the pin data flow functions, because only the end pins matter.
    // This flag is for checking and enforcing this.
    bool within_conn;

    // This is used for the final output mp_pin in connections only.
    bool data_requested;            // true if out wants new data
    struct mp_frame data;           // possibly buffered frame (MP_FRAME_NONE if
                                    // empty, usually only temporary)
};

// Root filters create this, all other filters reference it.
struct filter_runner {
    struct mpv_global *global;

    void (*wakeup_cb)(void *ctx);
    void *wakeup_ctx;

    struct mp_filter *root_filter;

    double max_run_time;
    atomic_bool interrupt_flag;

    // If we're currently running the filter graph (for avoiding recursion).
    bool filtering;

    // If set, recursive filtering was initiated through this pin.
    struct mp_pin *recursive;

    // Set of filters which need process() to be called. A filter is in this
    // array iff mp_filter_internal.pending==true.
    struct mp_filter **pending;
    int num_pending;

    // Any outside pins have changed state.
    bool external_pending;

    // For async notifications only. We don't bother making this fine grained
    // across filters.
    pthread_mutex_t async_lock;

    // Wakeup is pending. Protected by async_lock.
    bool async_wakeup_sent;

    // Similar to pending[]. Uses mp_filter_internal.async_pending. Protected
    // by async_lock.
    struct mp_filter **async_pending;
    int num_async_pending;
};

struct mp_filter_internal {
    const struct mp_filter_info *info;

    struct mp_filter *parent;
    struct filter_runner *runner;

    struct mp_filter **children;
    int num_children;

    struct mp_filter *error_handler;

    char *name;
    bool high_priority;

    bool pending;
    bool async_pending;
    bool failed;
};

// Called when new work needs to be done on a pin belonging to the filter:
//  - new data was requested
//  - new data has been queued
//  - or just an connect/disconnect/async notification happened
// This means the process function for this filter has to be called at some
// point in the future to continue filtering.
static void add_pending(struct mp_filter *f)
{
    struct filter_runner *r = f->in->runner;

    if (f->in->pending)
        return;

    // This should probably really be some sort of priority queue, but for now
    // something naive and dumb does the job too.
    f->in->pending = true;
    if (f->in->high_priority) {
        MP_TARRAY_INSERT_AT(r, r->pending, r->num_pending, 0, f);
    } else {
        MP_TARRAY_APPEND(r, r->pending, r->num_pending, f);
    }
}

static void add_pending_pin(struct mp_pin *p)
{
    struct mp_filter *f = p->manual_connection;
    assert(f);

    if (f->in->pending)
        return;

    add_pending(f);

    // Need to tell user that something changed.
    if (f == f->in->runner->root_filter && p != f->in->runner->recursive)
        f->in->runner->external_pending = true;
}

// Possibly enter recursive filtering. This is done as convenience for
// "external" filter users only. (Normal filtering does this iteratively via
// mp_filter_graph_run() to avoid filter reentrancy issues and deep call
// stacks.) If the API users uses an external manually connected pin, do
// recursive filtering as a not strictly necessary feature which makes outside
// I/O with filters easier.
static void filter_recursive(struct mp_pin *p)
{
    struct mp_filter *f = p->conn->manual_connection;
    assert(f);
    struct filter_runner *r = f->in->runner;

    // Never do internal filtering recursively.
    if (r->filtering)
        return;

    assert(!r->recursive);
    r->recursive = p;

    // Also don't lose the pending state, which the user may or may not
    // care about.
    r->external_pending |= mp_filter_graph_run(r->root_filter);

    assert(r->recursive == p);
    r->recursive = NULL;
}

void mp_filter_internal_mark_progress(struct mp_filter *f)
{
    struct filter_runner *r = f->in->runner;
    assert(r->filtering); // only call from f's process()
    add_pending(f);
}

// Basically copy the async notifications to the sync ones. Done so that the
// sync notifications don't need any locking.
static void flush_async_notifications(struct filter_runner *r)
{
    pthread_mutex_lock(&r->async_lock);
    for (int n = 0; n < r->num_async_pending; n++) {
        struct mp_filter *f = r->async_pending[n];
        add_pending(f);
        f->in->async_pending = false;
    }
    r->num_async_pending = 0;
    r->async_wakeup_sent = false;
    pthread_mutex_unlock(&r->async_lock);
}

bool mp_filter_graph_run(struct mp_filter *filter)
{
    struct filter_runner *r = filter->in->runner;
    assert(filter == r->root_filter); // user is supposed to call this on root only

    int64_t end_time = 0;
    if (isfinite(r->max_run_time))
        end_time = mp_add_timeout(mp_time_us(), MPMAX(r->max_run_time, 0));

    // (could happen with separate filter graphs calling each other, for now
    // ignore this issue as we don't use such a setup anywhere)
    assert(!r->filtering);

    r->filtering = true;

    flush_async_notifications(r);

    bool exit_req = false;

    while (1) {
        if (atomic_exchange_explicit(&r->interrupt_flag, false,
                                     memory_order_acq_rel))
        {
            pthread_mutex_lock(&r->async_lock);
            if (!r->async_wakeup_sent && r->wakeup_cb)
                r->wakeup_cb(r->wakeup_ctx);
            r->async_wakeup_sent = true;
            pthread_mutex_unlock(&r->async_lock);
            exit_req = true;
        }

        if (!r->num_pending) {
            flush_async_notifications(r);
            if (!r->num_pending)
                break;
        }

        struct mp_filter *next = NULL;

        if (r->pending[0]->in->high_priority) {
            next = r->pending[0];
            MP_TARRAY_REMOVE_AT(r->pending, r->num_pending, 0);
        } else if (!exit_req) {
            next = r->pending[r->num_pending - 1];
            r->num_pending -= 1;
        }

        if (!next)
            break;

        next->in->pending = false;
        if (next->in->info->process)
            next->in->info->process(next);

        if (end_time && mp_time_us() >= end_time)
            mp_filter_graph_interrupt(r->root_filter);
    }

    r->filtering = false;

    bool externals = r->external_pending;
    r->external_pending = false;
    return externals;
}

bool mp_pin_can_transfer_data(struct mp_pin *dst, struct mp_pin *src)
{
    return mp_pin_in_needs_data(dst) && mp_pin_out_request_data(src);
}

bool mp_pin_transfer_data(struct mp_pin *dst, struct mp_pin *src)
{
    if (!mp_pin_can_transfer_data(dst, src))
        return false;
    mp_pin_in_write(dst, mp_pin_out_read(src));
    return true;
}

bool mp_pin_in_needs_data(struct mp_pin *p)
{
    assert(p->dir == MP_PIN_IN);
    assert(!p->within_conn);
    return p->conn && p->conn->manual_connection && p->conn->data_requested;
}

bool mp_pin_in_write(struct mp_pin *p, struct mp_frame frame)
{
    if (!mp_pin_in_needs_data(p) || frame.type == MP_FRAME_NONE) {
        if (frame.type)
            MP_ERR(p->owner, "losing frame on %s\n", p->name);
        mp_frame_unref(&frame);
        return false;
    }
    assert(p->conn->data.type == MP_FRAME_NONE);
    p->conn->data = frame;
    p->conn->data_requested = false;
    add_pending_pin(p->conn);
    filter_recursive(p);
    return true;
}

bool mp_pin_out_has_data(struct mp_pin *p)
{
    assert(p->dir == MP_PIN_OUT);
    assert(!p->within_conn);
    return p->conn && p->conn->manual_connection && p->data.type != MP_FRAME_NONE;
}

bool mp_pin_out_request_data(struct mp_pin *p)
{
    if (mp_pin_out_has_data(p))
        return true;
    if (p->conn && p->conn->manual_connection) {
        if (!p->data_requested) {
            p->data_requested = true;
            add_pending_pin(p->conn);
        }
        filter_recursive(p);
    }
    return mp_pin_out_has_data(p);
}

void mp_pin_out_request_data_next(struct mp_pin *p)
{
    if (mp_pin_out_request_data(p))
        add_pending_pin(p->conn);
}

struct mp_frame mp_pin_out_read(struct mp_pin *p)
{
    if (!mp_pin_out_request_data(p))
        return MP_NO_FRAME;
    struct mp_frame res = p->data;
    p->data = MP_NO_FRAME;
    return res;
}

void mp_pin_out_unread(struct mp_pin *p, struct mp_frame frame)
{
    assert(p->dir == MP_PIN_OUT);
    assert(!p->within_conn);
    assert(p->conn && p->conn->manual_connection);
    // Unread is allowed strictly only if you didn't do anything else with
    // the pin since the time you read it.
    assert(!mp_pin_out_has_data(p));
    assert(!p->data_requested);
    p->data = frame;
}

void mp_pin_out_repeat_eof(struct mp_pin *p)
{
    mp_pin_out_unread(p, MP_EOF_FRAME);
}

// Follow mp_pin pairs/connection into the "other" direction of the pin, until
// the last pin is found. (In the simplest case, this is just p->other.) E.g.:
//      <1a|1b> <-> <2a|2b> <-> <3a|3b>
//          find_connected_end(2b)==1a
//          find_connected_end(1b)==1a
//          find_connected_end(1a)==3b
static struct mp_pin *find_connected_end(struct mp_pin *p)
{
    while (1) {
        struct mp_pin *other = p->other;
        if (!other->user_conn)
            return other;
        p = other->user_conn;
    }
    assert(0);
}

// With p being part of a connection, create the pin_connection and set all
// state flags.
static void init_connection(struct mp_pin *p)
{
    struct filter_runner *runner = p->owner->in->runner;

    if (p->dir == MP_PIN_IN)
        p = p->other;

    struct mp_pin *in = find_connected_end(p);
    struct mp_pin *out = find_connected_end(p->other);

    // These are the "outer" pins by definition, they have no user connections.
    assert(!in->user_conn);
    assert(!out->user_conn);

    // This and similar checks enforce the same root filter requirement.
    if (in->manual_connection)
        assert(in->manual_connection->in->runner == runner);
    if (out->manual_connection)
        assert(out->manual_connection->in->runner == runner);

    // Logicaly, the ends are always manual connections. A pin chain without
    // manual connections at the ends is still disconnected (or if this
    // attempted to extend an existing connection, becomes dangling and gets
    // disconnected).
    if (!in->manual_connection || !out->manual_connection)
        return;

    assert(in->dir == MP_PIN_IN);
    assert(out->dir == MP_PIN_OUT);

    struct mp_pin *cur = in;
    while (cur) {
        assert(!cur->within_conn && !cur->other->within_conn);
        assert(!cur->conn && !cur->other->conn);
        assert(!cur->data_requested); // unused for in pins
        assert(!cur->data.type); // unused for in pins
        assert(!cur->other->data_requested); // unset for unconnected out pins
        assert(!cur->other->data.type); // unset for unconnected out pins
        assert(cur->owner->in->runner == runner);
        cur->within_conn = cur->other->within_conn = true;
        cur = cur->other->user_conn;
    }

    in->conn = out;
    in->within_conn = false;
    out->conn = in;
    out->within_conn = false;

    // Scheduling so far will be messed up.
    add_pending(in->manual_connection);
    add_pending(out->manual_connection);
}

void mp_pin_connect(struct mp_pin *dst, struct mp_pin *src)
{
    assert(src->dir == MP_PIN_OUT);
    assert(dst->dir == MP_PIN_IN);

    if (dst->user_conn == src) {
        assert(src->user_conn == dst);
        return;
    }

    mp_pin_disconnect(src);
    mp_pin_disconnect(dst);

    src->user_conn = dst;
    dst->user_conn = src;

    init_connection(src);
}

void mp_pin_set_manual_connection(struct mp_pin *p, bool connected)
{
    mp_pin_set_manual_connection_for(p, connected ? p->owner->in->parent : NULL);
}

void mp_pin_set_manual_connection_for(struct mp_pin *p, struct mp_filter *f)
{
    if (p->manual_connection == f)
        return;
    if (p->within_conn)
        mp_pin_disconnect(p);
    p->manual_connection = f;
    init_connection(p);
}

struct mp_filter *mp_pin_get_manual_connection(struct mp_pin *p)
{
    return p->manual_connection;
}

static void deinit_connection(struct mp_pin *p)
{
    if (p->dir == MP_PIN_OUT)
        p = p->other;

    p = find_connected_end(p);

    while (p) {
        p->conn = p->other->conn = NULL;
        p->within_conn = p->other->within_conn = false;
        assert(!p->other->data_requested); // unused for in pins
        assert(!p->other->data.type); // unused for in pins
        p->data_requested = false;
        if (p->data.type)
            MP_VERBOSE(p->owner, "dropping frame due to pin disconnect\n");
        if (p->data_requested)
            MP_VERBOSE(p->owner, "dropping request due to pin disconnect\n");
        mp_frame_unref(&p->data);
        p = p->other->user_conn;
    }
}

void mp_pin_disconnect(struct mp_pin *p)
{
    if (!mp_pin_is_connected(p))
        return;

    p->manual_connection = NULL;

    struct mp_pin *conn = p->user_conn;
    if (conn) {
        p->user_conn = NULL;
        conn->user_conn = NULL;
        deinit_connection(conn);
    }

    deinit_connection(p);
}

bool mp_pin_is_connected(struct mp_pin *p)
{
    return p->user_conn || p->manual_connection;
}

const char *mp_pin_get_name(struct mp_pin *p)
{
    return p->name;
}

enum mp_pin_dir mp_pin_get_dir(struct mp_pin *p)
{
    return p->dir;
}

const char *mp_filter_get_name(struct mp_filter *f)
{
    return f->in->name;
}

const struct mp_filter_info *mp_filter_get_info(struct mp_filter *f)
{
    return f->in->info;
}

void mp_filter_set_high_priority(struct mp_filter *f, bool pri)
{
    f->in->high_priority = pri;
}

void mp_filter_set_name(struct mp_filter *f, const char *name)
{
    talloc_free(f->in->name);
    f->in->name = talloc_strdup(f, name);
}

struct mp_pin *mp_filter_get_named_pin(struct mp_filter *f, const char *name)
{
    for (int n = 0; n < f->num_pins; n++) {
        if (name && strcmp(f->pins[n]->name, name) == 0)
            return f->pins[n];
    }
    return NULL;
}

void mp_filter_set_error_handler(struct mp_filter *f, struct mp_filter *handler)
{
    f->in->error_handler = handler;
}

void mp_filter_internal_mark_failed(struct mp_filter *f)
{
    while (f) {
        f->in->failed = true;
        if (f->in->error_handler) {
            add_pending(f->in->error_handler);
            break;
        }
        f = f->in->parent;
    }
}

bool mp_filter_has_failed(struct mp_filter *filter)
{
    bool failed = filter->in->failed;
    filter->in->failed = false;
    return failed;
}

static void reset_pin(struct mp_pin *p)
{
    if (!p->conn || p->dir != MP_PIN_OUT) {
        assert(!p->data.type);
        assert(!p->data_requested);
    }
    mp_frame_unref(&p->data);
    p->data_requested = false;
}

void mp_filter_reset(struct mp_filter *filter)
{
    for (int n = 0; n < filter->in->num_children; n++)
        mp_filter_reset(filter->in->children[n]);

    for (int n = 0; n < filter->num_pins; n++) {
        struct mp_pin *p = filter->ppins[n];
        reset_pin(p);
        reset_pin(p->other);
    }

    if (filter->in->info->reset)
        filter->in->info->reset(filter);
}

struct mp_pin *mp_filter_add_pin(struct mp_filter *f, enum mp_pin_dir dir,
                                 const char *name)
{
    assert(dir == MP_PIN_IN || dir == MP_PIN_OUT);
    assert(name && name[0]);
    assert(!mp_filter_get_named_pin(f, name));

    // "Public" pin
    struct mp_pin *p = talloc_ptrtype(NULL, p);
    *p = (struct mp_pin){
        .name = talloc_strdup(p, name),
        .dir = dir,
        .owner = f,
        .manual_connection = f->in->parent,
    };

    // "Private" paired pin
    p->other = talloc_ptrtype(NULL, p);
    *p->other = (struct mp_pin){
        .name = p->name,
        .dir = p->dir == MP_PIN_IN ? MP_PIN_OUT : MP_PIN_IN,
        .owner = f,
        .other = p,
        .manual_connection = f,
    };

    MP_TARRAY_GROW(f, f->pins, f->num_pins);
    MP_TARRAY_GROW(f, f->ppins, f->num_pins);
    f->pins[f->num_pins] = p;
    f->ppins[f->num_pins] = p->other;
    f->num_pins += 1;

    init_connection(p);

    return p->other;
}

void mp_filter_remove_pin(struct mp_filter *f, struct mp_pin *p)
{
    if (!p)
        return;

    assert(p->owner == f);
    mp_pin_disconnect(p);
    mp_pin_disconnect(p->other);

    int index = -1;
    for (int n = 0; n < f->num_pins; n++) {
        if (f->ppins[n] == p) {
            index = n;
            break;
        }
    }
    assert(index >= 0);

    talloc_free(f->pins[index]);
    talloc_free(f->ppins[index]);

    int count = f->num_pins;
    MP_TARRAY_REMOVE_AT(f->pins, count, index);
    count = f->num_pins;
    MP_TARRAY_REMOVE_AT(f->ppins, count, index);
    f->num_pins -= 1;
}

bool mp_filter_command(struct mp_filter *f, struct mp_filter_command *cmd)
{
    return f->in->info->command ? f->in->info->command(f, cmd) : false;
}

struct mp_stream_info *mp_filter_find_stream_info(struct mp_filter *f)
{
    while (f) {
        if (f->stream_info)
            return f->stream_info;
        f = f->in->parent;
    }
    return NULL;
}

struct AVBufferRef *mp_filter_load_hwdec_device(struct mp_filter *f, int avtype)
{
    struct mp_stream_info *info = mp_filter_find_stream_info(f);
    if (!info || !info->hwdec_devs)
        return NULL;

    hwdec_devices_request_all(info->hwdec_devs);

    return hwdec_devices_get_lavc(info->hwdec_devs, avtype);
}

static void filter_wakeup(struct mp_filter *f, bool mark_only)
{
    struct filter_runner *r = f->in->runner;
    pthread_mutex_lock(&r->async_lock);
    if (!f->in->async_pending) {
        f->in->async_pending = true;
        // (not using a talloc parent for thread safety reasons)
        MP_TARRAY_APPEND(NULL, r->async_pending, r->num_async_pending, f);
    }
    if (!mark_only && !r->async_wakeup_sent) {
        if (r->wakeup_cb)
            r->wakeup_cb(r->wakeup_ctx);
        r->async_wakeup_sent = true;
    }
    pthread_mutex_unlock(&r->async_lock);
}

void mp_filter_wakeup(struct mp_filter *f)
{
    filter_wakeup(f, false);
}

void mp_filter_mark_async_progress(struct mp_filter *f)
{
    filter_wakeup(f, true);
}

void mp_filter_graph_set_max_run_time(struct mp_filter *f, double seconds)
{
    struct filter_runner *r = f->in->runner;
    assert(f == r->root_filter); // user is supposed to call this on root only
    r->max_run_time = seconds;
}

void mp_filter_graph_interrupt(struct mp_filter *f)
{
    struct filter_runner *r = f->in->runner;
    assert(f == r->root_filter); // user is supposed to call this on root only
    atomic_store(&r->interrupt_flag, true);
}

void mp_filter_free_children(struct mp_filter *f)
{
    while(f->in->num_children)
        talloc_free(f->in->children[0]);
}

static void filter_destructor(void *p)
{
    struct mp_filter *f = p;
    struct filter_runner *r = f->in->runner;

    if (f->in->info->destroy)
        f->in->info->destroy(f);

    // For convenience, free child filters.
    mp_filter_free_children(f);

    while (f->num_pins)
        mp_filter_remove_pin(f, f->ppins[0]);

    // Just make sure the filter is not still in the async notifications set.
    // There will be no more new notifications at this point (due to destroy()).
    flush_async_notifications(r);

    for (int n = 0; n < r->num_pending; n++) {
        if (r->pending[n] == f) {
            MP_TARRAY_REMOVE_AT(r->pending, r->num_pending, n);
            break;
        }
    }

    if (f->in->parent) {
        struct mp_filter_internal *p_in = f->in->parent->in;
        for (int n = 0; n < p_in->num_children; n++) {
            if (p_in->children[n] == f) {
                MP_TARRAY_REMOVE_AT(p_in->children, p_in->num_children, n);
                break;
            }
        }
    }

    if (r->root_filter == f) {
        assert(!f->in->parent);
        pthread_mutex_destroy(&r->async_lock);
        talloc_free(r->async_pending);
        talloc_free(r);
    }
}


struct mp_filter *mp_filter_create_with_params(struct mp_filter_params *params)
{
    struct mp_filter *f = talloc(NULL, struct mp_filter);
    talloc_set_destructor(f, filter_destructor);
    *f = (struct mp_filter){
        .priv = params->info->priv_size ?
                    talloc_zero_size(f, params->info->priv_size) : NULL,
        .global = params->global,
        .in = talloc(f, struct mp_filter_internal),
    };
    *f->in = (struct mp_filter_internal){
        .info = params->info,
        .parent = params->parent,
        .runner = params->parent ? params->parent->in->runner : NULL,
    };

    if (!f->in->runner) {
        assert(params->global);

        f->in->runner = talloc(NULL, struct filter_runner);
        *f->in->runner = (struct filter_runner){
            .global = params->global,
            .root_filter = f,
            .max_run_time = INFINITY,
        };
        pthread_mutex_init(&f->in->runner->async_lock, NULL);
    }

    if (!f->global)
        f->global = f->in->runner->global;

    if (f->in->parent) {
        struct mp_filter_internal *parent = f->in->parent->in;
        MP_TARRAY_APPEND(parent, parent->children, parent->num_children, f);
        f->log = mp_log_new(f, f->global->log, params->info->name);
    } else {
        f->log = mp_log_new(f, f->global->log, "!root");
    }

    if (f->in->info->init) {
        if (!f->in->info->init(f, params)) {
            talloc_free(f);
            return NULL;
        }
    }

    return f;
}

struct mp_filter *mp_filter_create(struct mp_filter *parent,
                                   const struct mp_filter_info *info)
{
    assert(parent);
    assert(info);
    struct mp_filter_params params = {
        .info = info,
        .parent = parent,
    };
    return mp_filter_create_with_params(&params);
}

// (the root filter is just a dummy filter - nothing special about it, except
// that it has no parent, and serves as manual connection for "external" pins)
static const struct mp_filter_info filter_root = {
    .name = "root",
};

struct mp_filter *mp_filter_create_root(struct mpv_global *global)
{
    struct mp_filter_params params = {
        .info = &filter_root,
        .global = global,
    };
    return mp_filter_create_with_params(&params);
}

void mp_filter_graph_set_wakeup_cb(struct mp_filter *root,
                                   void (*wakeup_cb)(void *ctx), void *ctx)
{
    struct filter_runner *r = root->in->runner;
    assert(root == r->root_filter); // user is supposed to call this on root only
    pthread_mutex_lock(&r->async_lock);
    r->wakeup_cb = wakeup_cb;
    r->wakeup_ctx = ctx;
    pthread_mutex_unlock(&r->async_lock);
}

static const char *filt_name(struct mp_filter *f)
{
    return f ? f->in->info->name : "-";
}

static void dump_pin_state(struct mp_filter *f, struct mp_pin *pin)
{
    MP_WARN(f, "  [%p] %s %s c=%s[%p] f=%s[%p] m=%s[%p] %s %s %s\n",
        pin, pin->name, pin->dir == MP_PIN_IN ? "->" : "<-",
        pin->user_conn ? filt_name(pin->user_conn->owner) : "-", pin->user_conn,
        pin->conn ? filt_name(pin->conn->owner) : "-", pin->conn,
        filt_name(pin->manual_connection), pin->manual_connection,
        pin->within_conn ? "(within)" : "",
        pin->data_requested ? "(request)" : "",
        mp_frame_type_str(pin->data.type));
}

void mp_filter_dump_states(struct mp_filter *f)
{
    MP_WARN(f, "%s[%p] (%s[%p])\n", filt_name(f), f,
            filt_name(f->in->parent), f->in->parent);
    for (int n = 0; n < f->num_pins; n++) {
        dump_pin_state(f, f->pins[n]);
        dump_pin_state(f, f->ppins[n]);
    }

    for (int n = 0; n < f->in->num_children; n++)
        mp_filter_dump_states(f->in->children[n]);
}
