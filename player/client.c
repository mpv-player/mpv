/* Copyright (C) 2017 the mpv developers
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <assert.h>

#include "common/common.h"
#include "common/global.h"
#include "common/msg.h"
#include "common/msg_control.h"
#include "common/global.h"
#include "input/input.h"
#include "input/cmd.h"
#include "misc/ctype.h"
#include "misc/dispatch.h"
#include "misc/node.h"
#include "misc/rendezvous.h"
#include "misc/thread_tools.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "options/m_property.h"
#include "options/path.h"
#include "options/parse_configfile.h"
#include "osdep/atomic.h"
#include "osdep/threads.h"
#include "osdep/timer.h"
#include "osdep/io.h"
#include "stream/stream.h"

#include "command.h"
#include "core.h"
#include "client.h"

/*
 * Locking hierarchy:
 *
 *  MPContext > mp_client_api.lock > mpv_handle.lock > * > mpv_handle.wakeup_lock
 *
 * MPContext strictly speaking has no locks, and instead is implicitly managed
 * by MPContext.dispatch, which basically stops the playback thread at defined
 * points in order to let clients access it in a synchronized manner. Since
 * MPContext code accesses the client API, it's on top of the lock hierarchy.
 *
 */

struct mp_client_api {
    struct MPContext *mpctx;

    pthread_mutex_t lock;

    // -- protected by lock

    struct mpv_handle **clients;
    int num_clients;
    bool shutting_down; // do not allow new clients
    bool have_terminator; // a client took over the role of destroying the core
    bool terminate_core_thread; // make libmpv core thread exit
    // This is incremented whenever the clients[] array above changes. This is
    // used to safely unlock mp_client_api.lock while iterating the list of
    // clients.
    uint64_t clients_list_change_ts;
    int64_t id_alloc;

    struct mp_custom_protocol *custom_protocols;
    int num_custom_protocols;

    struct mpv_render_context *render_context;
};

struct observe_property {
    // -- immutable
    struct mpv_handle *owner;
    char *name;
    int id;                 // ==mp_get_property_id(name)
    uint64_t event_mask;    // ==mp_get_property_event_mask(name)
    int64_t reply_id;
    mpv_format format;
    const struct m_option *type;
    // -- protected by owner->lock
    size_t refcount;
    uint64_t change_ts;     // logical timestamp incremented on each change
    uint64_t value_ts;      // logical timestamp for value contents
    bool value_valid;
    union m_option_value value;
    uint64_t value_ret_ts;  // logical timestamp of value returned to user
    union m_option_value value_ret;
    bool waiting_for_hook;  // flag for draining old property changes on a hook
};

struct mpv_handle {
    // -- immmutable
    char name[MAX_CLIENT_NAME];
    struct mp_log *log;
    struct MPContext *mpctx;
    struct mp_client_api *clients;
    int64_t id;

    // -- not thread-safe
    struct mpv_event *cur_event;
    struct mpv_event_property cur_property_event;
    struct observe_property *cur_property;

    pthread_mutex_t lock;

    pthread_mutex_t wakeup_lock;
    pthread_cond_t wakeup;

    // -- protected by wakeup_lock
    bool need_wakeup;
    void (*wakeup_cb)(void *d);
    void *wakeup_cb_ctx;
    int wakeup_pipe[2];

    // -- protected by lock

    uint64_t event_mask;
    bool queued_wakeup;

    mpv_event *events;      // ringbuffer of max_events entries
    int max_events;         // allocated number of entries in events
    int first_event;        // events[first_event] is the first readable event
    int num_events;         // number of readable events
    int reserved_events;    // number of entries reserved for replies
    size_t async_counter;   // pending other async events
    bool choked;            // recovering from queue overflow
    bool destroying;        // pending destruction; no API accesses allowed
    bool hook_pending;      // hook events are returned after draining properties

    struct observe_property **properties;
    int num_properties;
    bool has_pending_properties; // (maybe) new property events (producer side)
    bool new_property_events; // new property events (consumer side)
    int cur_property_index; // round-robin for property events (consumer side)
    uint64_t property_event_masks; // or-ed together event masks of all properties
    // This is incremented whenever the properties[] array above changes. This
    // is used to safely unlock mpv_handle.lock while reading a property. If
    // the counter didn't change between unlock and relock, then it will assume
    // the array did not change.
    uint64_t properties_change_ts;

    bool fuzzy_initialized; // see scripting.c wait_loaded()
    bool is_weak;           // can not keep core alive on its own
    struct mp_log_buffer *messages;
    int messages_level;
};

static bool gen_log_message_event(struct mpv_handle *ctx);
static bool gen_property_change_event(struct mpv_handle *ctx);
static void notify_property_events(struct mpv_handle *ctx, int event);

// Must be called with prop->owner->lock held.
static void prop_unref(struct observe_property *prop)
{
    if (!prop)
        return;

    assert(prop->refcount > 0);
    prop->refcount -= 1;
    if (!prop->refcount)
        talloc_free(prop);
}

void mp_clients_init(struct MPContext *mpctx)
{
    mpctx->clients = talloc_ptrtype(NULL, mpctx->clients);
    *mpctx->clients = (struct mp_client_api) {
        .mpctx = mpctx,
    };
    mpctx->global->client_api = mpctx->clients;
    pthread_mutex_init(&mpctx->clients->lock, NULL);
}

void mp_clients_destroy(struct MPContext *mpctx)
{
    if (!mpctx->clients)
        return;
    assert(mpctx->clients->num_clients == 0);

    // The API user is supposed to call mpv_render_context_free(). It's simply
    // not allowed not to do this.
    if (mpctx->clients->render_context) {
        MP_FATAL(mpctx, "Broken API use: mpv_render_context_free() not called.\n");
        abort();
    }

    pthread_mutex_destroy(&mpctx->clients->lock);
    talloc_free(mpctx->clients);
    mpctx->clients = NULL;
}

// Test for "fuzzy" initialization of all clients. That is, all clients have
// at least called mpv_wait_event() at least once since creation (or exited).
bool mp_clients_all_initialized(struct MPContext *mpctx)
{
    bool all_ok = true;
    pthread_mutex_lock(&mpctx->clients->lock);
    for (int n = 0; n < mpctx->clients->num_clients; n++) {
        struct mpv_handle *ctx = mpctx->clients->clients[n];
        pthread_mutex_lock(&ctx->lock);
        all_ok &= ctx->fuzzy_initialized;
        pthread_mutex_unlock(&ctx->lock);
    }
    pthread_mutex_unlock(&mpctx->clients->lock);
    return all_ok;
}

static struct mpv_handle *find_client_id(struct mp_client_api *clients, int64_t id)
{
    for (int n = 0; n < clients->num_clients; n++) {
        if (clients->clients[n]->id == id)
            return clients->clients[n];
    }
    return NULL;
}

static struct mpv_handle *find_client(struct mp_client_api *clients,
                                      const char *name)
{
    if (name[0] == '@') {
        char *end;
        errno = 0;
        long long int id = strtoll(name + 1, &end, 10);
        if (errno || end[0])
            return NULL;
        return find_client_id(clients, id);
    }

    for (int n = 0; n < clients->num_clients; n++) {
        if (strcmp(clients->clients[n]->name, name) == 0)
            return clients->clients[n];
    }

    return NULL;
}

bool mp_client_id_exists(struct MPContext *mpctx, int64_t id)
{
    pthread_mutex_lock(&mpctx->clients->lock);
    bool r = find_client_id(mpctx->clients, id);
    pthread_mutex_unlock(&mpctx->clients->lock);
    return r;
}

struct mpv_handle *mp_new_client(struct mp_client_api *clients, const char *name)
{
    pthread_mutex_lock(&clients->lock);

    char nname[MAX_CLIENT_NAME];
    for (int n = 1; n < 1000; n++) {
        if (!name)
            name = "client";
        snprintf(nname, sizeof(nname) - 3, "%s", name); // - space for number
        for (int i = 0; nname[i]; i++)
            nname[i] = mp_isalnum(nname[i]) ? nname[i] : '_';
        if (n > 1)
            mp_snprintf_cat(nname, sizeof(nname), "%d", n);
        if (!find_client(clients, nname))
            break;
        nname[0] = '\0';
    }

    if (!nname[0] || clients->shutting_down) {
        pthread_mutex_unlock(&clients->lock);
        return NULL;
    }

    int num_events = 1000;

    struct mpv_handle *client = talloc_ptrtype(NULL, client);
    *client = (struct mpv_handle){
        .log = mp_log_new(client, clients->mpctx->log, nname),
        .mpctx = clients->mpctx,
        .clients = clients,
        .id = ++(clients->id_alloc),
        .cur_event = talloc_zero(client, struct mpv_event),
        .events = talloc_array(client, mpv_event, num_events),
        .max_events = num_events,
        .event_mask = (1ULL << INTERNAL_EVENT_BASE) - 1, // exclude internal events
        .wakeup_pipe = {-1, -1},
    };
    pthread_mutex_init(&client->lock, NULL);
    pthread_mutex_init(&client->wakeup_lock, NULL);
    pthread_cond_init(&client->wakeup, NULL);

    snprintf(client->name, sizeof(client->name), "%s", nname);

    clients->clients_list_change_ts += 1;
    MP_TARRAY_APPEND(clients, clients->clients, clients->num_clients, client);

    if (clients->num_clients == 1 && !clients->mpctx->is_cli)
        client->fuzzy_initialized = true;

    pthread_mutex_unlock(&clients->lock);

    mpv_request_event(client, MPV_EVENT_TICK, 0);

    return client;
}

void mp_client_set_weak(struct mpv_handle *ctx)
{
    pthread_mutex_lock(&ctx->lock);
    ctx->is_weak = true;
    pthread_mutex_unlock(&ctx->lock);
}

const char *mpv_client_name(mpv_handle *ctx)
{
    return ctx->name;
}

int64_t mpv_client_id(mpv_handle *ctx)
{
    return ctx->id;
}

struct mp_log *mp_client_get_log(struct mpv_handle *ctx)
{
    return ctx->log;
}

struct mpv_global *mp_client_get_global(struct mpv_handle *ctx)
{
    return ctx->mpctx->global;
}

static void wakeup_client(struct mpv_handle *ctx)
{
    pthread_mutex_lock(&ctx->wakeup_lock);
    if (!ctx->need_wakeup) {
        ctx->need_wakeup = true;
        pthread_cond_broadcast(&ctx->wakeup);
        if (ctx->wakeup_cb)
            ctx->wakeup_cb(ctx->wakeup_cb_ctx);
        if (ctx->wakeup_pipe[0] != -1)
            (void)write(ctx->wakeup_pipe[1], &(char){0}, 1);
    }
    pthread_mutex_unlock(&ctx->wakeup_lock);
}

// Note: the caller has to deal with sporadic wakeups.
static int wait_wakeup(struct mpv_handle *ctx, int64_t end)
{
    int r = 0;
    pthread_mutex_unlock(&ctx->lock);
    pthread_mutex_lock(&ctx->wakeup_lock);
    if (!ctx->need_wakeup) {
        struct timespec ts = mp_time_us_to_timespec(end);
        r = pthread_cond_timedwait(&ctx->wakeup, &ctx->wakeup_lock, &ts);
    }
    if (r == 0)
        ctx->need_wakeup = false;
    pthread_mutex_unlock(&ctx->wakeup_lock);
    pthread_mutex_lock(&ctx->lock);
    return r;
}

void mpv_set_wakeup_callback(mpv_handle *ctx, void (*cb)(void *d), void *d)
{
    pthread_mutex_lock(&ctx->wakeup_lock);
    ctx->wakeup_cb = cb;
    ctx->wakeup_cb_ctx = d;
    if (ctx->wakeup_cb)
        ctx->wakeup_cb(ctx->wakeup_cb_ctx);
    pthread_mutex_unlock(&ctx->wakeup_lock);
}

void mpv_suspend(mpv_handle *ctx)
{
    MP_ERR(ctx, "mpv_suspend() is deprecated and does nothing.\n");
}

void mpv_resume(mpv_handle *ctx)
{
}

static void lock_core(mpv_handle *ctx)
{
    mp_dispatch_lock(ctx->mpctx->dispatch);
}

static void unlock_core(mpv_handle *ctx)
{
    mp_dispatch_unlock(ctx->mpctx->dispatch);
}

void mpv_wait_async_requests(mpv_handle *ctx)
{
    pthread_mutex_lock(&ctx->lock);
    while (ctx->reserved_events || ctx->async_counter)
        wait_wakeup(ctx, INT64_MAX);
    pthread_mutex_unlock(&ctx->lock);
}

// Send abort signal to all matching work items.
// If type==0, destroy all of the matching ctx.
// If ctx==0, destroy all.
static void abort_async(struct MPContext *mpctx, mpv_handle *ctx,
                        int type, uint64_t id)
{
    pthread_mutex_lock(&mpctx->abort_lock);

    // Destroy all => ensure any newly appearing work is aborted immediately.
    if (ctx == NULL)
        mpctx->abort_all = true;

    for (int n = 0; n < mpctx->num_abort_list; n++) {
        struct mp_abort_entry *abort = mpctx->abort_list[n];
        if (!ctx || (abort->client == ctx && (!type ||
            (abort->client_work_type == type && abort->client_work_id == id))))
        {
            mp_abort_trigger_locked(mpctx, abort);
        }
    }

    pthread_mutex_unlock(&mpctx->abort_lock);
}

static void get_thread(void *ptr)
{
    *(pthread_t *)ptr = pthread_self();
}

static void mp_destroy_client(mpv_handle *ctx, bool terminate)
{
    if (!ctx)
        return;

    struct MPContext *mpctx = ctx->mpctx;
    struct mp_client_api *clients = ctx->clients;

    MP_DBG(ctx, "Exiting...\n");

    if (terminate)
        mpv_command(ctx, (const char*[]){"quit", NULL});

    pthread_mutex_lock(&ctx->lock);

    ctx->destroying = true;

    for (int n = 0; n < ctx->num_properties; n++)
        prop_unref(ctx->properties[n]);
    ctx->num_properties = 0;
    ctx->properties_change_ts += 1;

    prop_unref(ctx->cur_property);
    ctx->cur_property = NULL;

    pthread_mutex_unlock(&ctx->lock);

    abort_async(mpctx, ctx, 0, 0);

    // reserved_events equals the number of asynchronous requests that weren't
    // yet replied. In order to avoid that trying to reply to a removed client
    // causes a crash, block until all asynchronous requests were served.
    mpv_wait_async_requests(ctx);

    osd_set_external_remove_owner(mpctx->osd, ctx);
    mp_input_remove_sections_by_owner(mpctx->input, ctx->name);

    pthread_mutex_lock(&clients->lock);

    for (int n = 0; n < clients->num_clients; n++) {
        if (clients->clients[n] == ctx) {
            clients->clients_list_change_ts += 1;
            MP_TARRAY_REMOVE_AT(clients->clients, clients->num_clients, n);
            while (ctx->num_events) {
                talloc_free(ctx->events[ctx->first_event].data);
                ctx->first_event = (ctx->first_event + 1) % ctx->max_events;
                ctx->num_events--;
            }
            mp_msg_log_buffer_destroy(ctx->messages);
            pthread_cond_destroy(&ctx->wakeup);
            pthread_mutex_destroy(&ctx->wakeup_lock);
            pthread_mutex_destroy(&ctx->lock);
            if (ctx->wakeup_pipe[0] != -1) {
                close(ctx->wakeup_pipe[0]);
                close(ctx->wakeup_pipe[1]);
            }
            talloc_free(ctx);
            ctx = NULL;
            break;
        }
    }
    assert(!ctx);

    if (mpctx->is_cli) {
        terminate = false;
    } else {
        // If the last strong mpv_handle got destroyed, destroy the core.
        bool has_strong_ref = false;
        for (int n = 0; n < clients->num_clients; n++)
            has_strong_ref |= !clients->clients[n]->is_weak;
        if (!has_strong_ref)
            terminate = true;

        // Reserve the right to destroy mpctx for us.
        if (clients->have_terminator)
            terminate = false;
        clients->have_terminator |= terminate;
    }

    // mp_shutdown_clients() sleeps to avoid wasting CPU.
    // mp_hook_test_completion() also relies on this a bit.
    mp_wakeup_core(mpctx);

    pthread_mutex_unlock(&clients->lock);

    // Note that even if num_clients==0, having set have_terminator keeps mpctx
    // and the core thread alive.
    if (terminate) {
        // Make sure the core stops playing files etc. Being able to lock the
        // dispatch queue requires that the core thread is still active.
        mp_dispatch_lock(mpctx->dispatch);
        mpctx->stop_play = PT_QUIT;
        mp_dispatch_unlock(mpctx->dispatch);

        pthread_t playthread;
        mp_dispatch_run(mpctx->dispatch, get_thread, &playthread);

        // Ask the core thread to stop.
        pthread_mutex_lock(&clients->lock);
        clients->terminate_core_thread = true;
        pthread_mutex_unlock(&clients->lock);
        mp_wakeup_core(mpctx);

        // Blocking wait for all clients and core thread to terminate.
        pthread_join(playthread, NULL);

        mp_destroy(mpctx);
    }
}

void mpv_destroy(mpv_handle *ctx)
{
    mp_destroy_client(ctx, false);
}

void mpv_detach_destroy(mpv_handle *ctx)
{
    mpv_destroy(ctx);
}

void mpv_terminate_destroy(mpv_handle *ctx)
{
    mp_destroy_client(ctx, true);
}

// Can be called on the core thread only. Idempotent.
// Also happens to take care of shutting down any async work.
void mp_shutdown_clients(struct MPContext *mpctx)
{
    struct mp_client_api *clients = mpctx->clients;

    // Forcefully abort async work after 2 seconds of waiting.
    double abort_time = mp_time_sec() + 2;

    pthread_mutex_lock(&clients->lock);

    // Prevent that new clients can appear.
    clients->shutting_down = true;

    // Wait until we can terminate.
    while (clients->num_clients || mpctx->outstanding_async ||
           !(mpctx->is_cli || clients->terminate_core_thread))
    {
        pthread_mutex_unlock(&clients->lock);

        double left = abort_time - mp_time_sec();
        if (left >= 0) {
            mp_set_timeout(mpctx, left);
        } else {
            // Forcefully abort any ongoing async work. This is quite rude and
            // probably not what everyone wants, so it happens only after a
            // timeout.
            abort_async(mpctx, NULL, 0, 0);
        }

        mp_client_broadcast_event(mpctx, MPV_EVENT_SHUTDOWN, NULL);
        mp_wait_events(mpctx);

        pthread_mutex_lock(&clients->lock);
    }

    pthread_mutex_unlock(&clients->lock);
}

bool mp_is_shutting_down(struct MPContext *mpctx)
{
    struct mp_client_api *clients = mpctx->clients;
    pthread_mutex_lock(&clients->lock);
    bool res = clients->shutting_down;
    pthread_mutex_unlock(&clients->lock);
    return res;
}

static void *core_thread(void *p)
{
    struct MPContext *mpctx = p;

    mpthread_set_name("mpv core");

    while (!mpctx->initialized && mpctx->stop_play != PT_QUIT)
        mp_idle(mpctx);

    if (mpctx->initialized)
        mp_play_files(mpctx);

    // This actually waits until all clients are gone before actually
    // destroying mpctx. Actual destruction is done by whatever destroys
    // the last mpv_handle.
    mp_shutdown_clients(mpctx);

    return NULL;
}

mpv_handle *mpv_create(void)
{
    struct MPContext *mpctx = mp_create();
    if (!mpctx)
        return NULL;

    m_config_set_profile(mpctx->mconfig, "libmpv", 0);

    mpv_handle *ctx = mp_new_client(mpctx->clients, "main");
    if (!ctx) {
        mp_destroy(mpctx);
        return NULL;
    }

    pthread_t thread;
    if (pthread_create(&thread, NULL, core_thread, mpctx) != 0) {
        ctx->clients->have_terminator = true; // avoid blocking
        mpv_terminate_destroy(ctx);
        mp_destroy(mpctx);
        return NULL;
    }

    return ctx;
}

mpv_handle *mpv_create_client(mpv_handle *ctx, const char *name)
{
    if (!ctx)
        return mpv_create();
    mpv_handle *new = mp_new_client(ctx->mpctx->clients, name);
    if (new)
        mpv_wait_event(new, 0); // set fuzzy_initialized
    return new;
}

mpv_handle *mpv_create_weak_client(mpv_handle *ctx, const char *name)
{
    mpv_handle *new = mpv_create_client(ctx, name);
    if (new)
        mp_client_set_weak(new);
    return new;
}

int mpv_initialize(mpv_handle *ctx)
{
    lock_core(ctx);
    int res = mp_initialize(ctx->mpctx, NULL) ? MPV_ERROR_INVALID_PARAMETER : 0;
    mp_wakeup_core(ctx->mpctx);
    unlock_core(ctx);
    return res;
}

// set ev->data to a new copy of the original data
// (done only for message types that are broadcast)
static void dup_event_data(struct mpv_event *ev)
{
    switch (ev->event_id) {
    case MPV_EVENT_CLIENT_MESSAGE: {
        struct mpv_event_client_message *src = ev->data;
        struct mpv_event_client_message *msg =
            talloc_zero(NULL, struct mpv_event_client_message);
        for (int n = 0; n < src->num_args; n++) {
            MP_TARRAY_APPEND(msg, msg->args, msg->num_args,
                             talloc_strdup(msg, src->args[n]));
        }
        ev->data = msg;
        break;
    }
    case MPV_EVENT_START_FILE:
        ev->data = talloc_memdup(NULL, ev->data, sizeof(mpv_event_start_file));
        break;
    case MPV_EVENT_END_FILE:
        ev->data = talloc_memdup(NULL, ev->data, sizeof(mpv_event_end_file));
        break;
    default:
        // Doesn't use events with memory allocation.
        if (ev->data)
            abort();
    }
}

// Reserve an entry in the ring buffer. This can be used to guarantee that the
// reply can be made, even if the buffer becomes congested _after_ sending
// the request.
// Returns an error code if the buffer is full.
static int reserve_reply(struct mpv_handle *ctx)
{
    int res = MPV_ERROR_EVENT_QUEUE_FULL;
    pthread_mutex_lock(&ctx->lock);
    if (ctx->reserved_events + ctx->num_events < ctx->max_events && !ctx->choked)
    {
        ctx->reserved_events++;
        res = 0;
    }
    pthread_mutex_unlock(&ctx->lock);
    return res;
}

static int append_event(struct mpv_handle *ctx, struct mpv_event event, bool copy)
{
    if (ctx->num_events + ctx->reserved_events >= ctx->max_events)
        return -1;
    if (copy)
        dup_event_data(&event);
    ctx->events[(ctx->first_event + ctx->num_events) % ctx->max_events] = event;
    ctx->num_events++;
    wakeup_client(ctx);
    if (event.event_id == MPV_EVENT_SHUTDOWN)
        ctx->event_mask &= ctx->event_mask & ~(1ULL << MPV_EVENT_SHUTDOWN);
    return 0;
}

static int send_event(struct mpv_handle *ctx, struct mpv_event *event, bool copy)
{
    pthread_mutex_lock(&ctx->lock);
    uint64_t mask = 1ULL << event->event_id;
    if (ctx->property_event_masks & mask)
        notify_property_events(ctx, event->event_id);
    int r;
    if (!(ctx->event_mask & mask)) {
        r = 0;
    } else if (ctx->choked) {
        r = -1;
    } else {
        r = append_event(ctx, *event, copy);
        if (r < 0) {
            MP_ERR(ctx, "Too many events queued.\n");
            ctx->choked = true;
        }
    }
    pthread_mutex_unlock(&ctx->lock);
    return r;
}

// Send a reply; the reply must have been previously reserved with
// reserve_reply (otherwise, use send_event()).
static void send_reply(struct mpv_handle *ctx, uint64_t userdata,
                       struct mpv_event *event)
{
    event->reply_userdata = userdata;
    pthread_mutex_lock(&ctx->lock);
    // If this fails, reserve_reply() probably wasn't called.
    assert(ctx->reserved_events > 0);
    ctx->reserved_events--;
    if (append_event(ctx, *event, false) < 0)
        abort(); // not reached
    pthread_mutex_unlock(&ctx->lock);
}

void mp_client_broadcast_event(struct MPContext *mpctx, int event, void *data)
{
    struct mp_client_api *clients = mpctx->clients;

    pthread_mutex_lock(&clients->lock);

    for (int n = 0; n < clients->num_clients; n++) {
        struct mpv_event event_data = {
            .event_id = event,
            .data = data,
        };
        send_event(clients->clients[n], &event_data, true);
    }

    pthread_mutex_unlock(&clients->lock);
}

// Like mp_client_broadcast_event(), but can be called from any thread.
// Avoid using this.
void mp_client_broadcast_event_external(struct mp_client_api *api, int event,
                                        void *data)
{
    struct MPContext *mpctx = api->mpctx;

    mp_client_broadcast_event(mpctx, event, data);
    mp_wakeup_core(mpctx);
}

// If client_name == NULL, then broadcast and free the event.
int mp_client_send_event(struct MPContext *mpctx, const char *client_name,
                         uint64_t reply_userdata, int event, void *data)
{
    if (!client_name) {
        mp_client_broadcast_event(mpctx, event, data);
        talloc_free(data);
        return 0;
    }

    struct mp_client_api *clients = mpctx->clients;
    int r = 0;

    struct mpv_event event_data = {
        .event_id = event,
        .data = data,
        .reply_userdata = reply_userdata,
    };

    pthread_mutex_lock(&clients->lock);

    struct mpv_handle *ctx = find_client(clients, client_name);
    if (ctx) {
        r = send_event(ctx, &event_data, false);
    } else {
        r = -1;
        talloc_free(data);
    }

    pthread_mutex_unlock(&clients->lock);

    return r;
}

int mp_client_send_event_dup(struct MPContext *mpctx, const char *client_name,
                             int event, void *data)
{
    if (!client_name) {
        mp_client_broadcast_event(mpctx, event, data);
        return 0;
    }

    struct mpv_event event_data = {
        .event_id = event,
        .data = data,
    };

    dup_event_data(&event_data);
    return mp_client_send_event(mpctx, client_name, 0, event, event_data.data);
}

static bool deprecated_events[] = {
    [MPV_EVENT_TRACKS_CHANGED] = true,
    [MPV_EVENT_TRACK_SWITCHED] = true,
    [MPV_EVENT_IDLE] = true,
    [MPV_EVENT_PAUSE] = true,
    [MPV_EVENT_UNPAUSE] = true,
    [MPV_EVENT_TICK] = true,
    [MPV_EVENT_SCRIPT_INPUT_DISPATCH] = true,
    [MPV_EVENT_METADATA_UPDATE] = true,
    [MPV_EVENT_CHAPTER_CHANGE] = true,
};

int mpv_request_event(mpv_handle *ctx, mpv_event_id event, int enable)
{
    if (!mpv_event_name(event) || enable < 0 || enable > 1)
        return MPV_ERROR_INVALID_PARAMETER;
    if (event == MPV_EVENT_SHUTDOWN && !enable)
        return MPV_ERROR_INVALID_PARAMETER;
    assert(event < (int)INTERNAL_EVENT_BASE); // excluded above; they have no name
    pthread_mutex_lock(&ctx->lock);
    uint64_t bit = 1ULL << event;
    ctx->event_mask = enable ? ctx->event_mask | bit : ctx->event_mask & ~bit;
    if (enable && event < MP_ARRAY_SIZE(deprecated_events) &&
        deprecated_events[event])
    {
        MP_WARN(ctx, "The '%s' event is deprecated and will be removed.\n",
                mpv_event_name(event));
    }
    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

// Set waiting_for_hook==true for all possibly pending properties.
static void set_wait_for_hook_flags(mpv_handle *ctx)
{
    for (int n = 0; n < ctx->num_properties; n++) {
        struct observe_property *prop = ctx->properties[n];

        if (prop->value_ret_ts != prop->change_ts)
            prop->waiting_for_hook = true;
    }
}

// Return whether any property still has waiting_for_hook set.
static bool check_for_for_hook_flags(mpv_handle *ctx)
{
    for (int n = 0; n < ctx->num_properties; n++) {
        if (ctx->properties[n]->waiting_for_hook)
            return true;
    }
    return false;
}

mpv_event *mpv_wait_event(mpv_handle *ctx, double timeout)
{
    mpv_event *event = ctx->cur_event;

    pthread_mutex_lock(&ctx->lock);

    if (!ctx->fuzzy_initialized)
        mp_wakeup_core(ctx->clients->mpctx);
    ctx->fuzzy_initialized = true;

    if (timeout < 0)
        timeout = 1e20;

    int64_t deadline = mp_add_timeout(mp_time_us(), timeout);

    *event = (mpv_event){0};
    talloc_free_children(event);

    while (1) {
        if (ctx->queued_wakeup)
            deadline = 0;
        // Recover from overflow.
        if (ctx->choked && !ctx->num_events) {
            ctx->choked = false;
            event->event_id = MPV_EVENT_QUEUE_OVERFLOW;
            break;
        }
        struct mpv_event *ev =
            ctx->num_events ? &ctx->events[ctx->first_event] : NULL;
        if (ev && ev->event_id == MPV_EVENT_HOOK) {
            // Give old property notifications priority over hooks. This is a
            // guarantee given to clients to simplify their logic. New property
            // changes after this are treated normally, so
            if (!ctx->hook_pending) {
                ctx->hook_pending = true;
                set_wait_for_hook_flags(ctx);
            }
            if (check_for_for_hook_flags(ctx)) {
                ev = NULL; // delay
            } else {
                ctx->hook_pending = false;
            }
        }
        if (ev) {
            *event = *ev;
            ctx->first_event = (ctx->first_event + 1) % ctx->max_events;
            ctx->num_events--;
            talloc_steal(event, event->data);
            break;
        }
        // If there's a changed property, generate change event (never queued).
        if (gen_property_change_event(ctx))
            break;
        // Pop item from message queue, and return as event.
        if (gen_log_message_event(ctx))
            break;
        int r = wait_wakeup(ctx, deadline);
        if (r == ETIMEDOUT)
            break;
    }
    ctx->queued_wakeup = false;

    pthread_mutex_unlock(&ctx->lock);

    return event;
}

void mpv_wakeup(mpv_handle *ctx)
{
    pthread_mutex_lock(&ctx->lock);
    ctx->queued_wakeup = true;
    wakeup_client(ctx);
    pthread_mutex_unlock(&ctx->lock);
}

// map client API types to internal types
static const struct m_option type_conv[] = {
    [MPV_FORMAT_STRING]     = { .type = CONF_TYPE_STRING },
    [MPV_FORMAT_FLAG]       = { .type = CONF_TYPE_FLAG },
    [MPV_FORMAT_INT64]      = { .type = CONF_TYPE_INT64 },
    [MPV_FORMAT_DOUBLE]     = { .type = CONF_TYPE_DOUBLE },
    [MPV_FORMAT_NODE]       = { .type = CONF_TYPE_NODE },
};

static const struct m_option *get_mp_type(mpv_format format)
{
    if ((unsigned)format >= MP_ARRAY_SIZE(type_conv))
        return NULL;
    if (!type_conv[format].type)
        return NULL;
    return &type_conv[format];
}

// for read requests - MPV_FORMAT_OSD_STRING special handling
static const struct m_option *get_mp_type_get(mpv_format format)
{
    if (format == MPV_FORMAT_OSD_STRING)
        format = MPV_FORMAT_STRING; // it's string data, just other semantics
    return get_mp_type(format);
}

// move src->dst, and do implicit conversion if possible (conversions to or
// from strings are handled otherwise)
static bool conv_node_to_format(void *dst, mpv_format dst_fmt, mpv_node *src)
{
    if (dst_fmt == src->format) {
        const struct m_option *type = get_mp_type(dst_fmt);
        memcpy(dst, &src->u, type->type->size);
        return true;
    }
    if (dst_fmt == MPV_FORMAT_DOUBLE && src->format == MPV_FORMAT_INT64) {
        *(double *)dst = src->u.int64;
        return true;
    }
    if (dst_fmt == MPV_FORMAT_INT64 && src->format == MPV_FORMAT_DOUBLE) {
        if (src->u.double_ > (double)INT64_MIN &&
            src->u.double_ < (double)INT64_MAX)
        {
            *(int64_t *)dst = src->u.double_;
            return true;
        }
    }
    return false;
}

void mpv_free_node_contents(mpv_node *node)
{
    static const struct m_option type = { .type = CONF_TYPE_NODE };
    m_option_free(&type, node);
}

int mpv_set_option(mpv_handle *ctx, const char *name, mpv_format format,
                   void *data)
{
    const struct m_option *type = get_mp_type(format);
    if (!type)
        return MPV_ERROR_OPTION_FORMAT;
    struct mpv_node tmp;
    if (format != MPV_FORMAT_NODE) {
        tmp.format = format;
        memcpy(&tmp.u, data, type->type->size);
        data = &tmp;
    }
    lock_core(ctx);
    int err = m_config_set_option_node(ctx->mpctx->mconfig, bstr0(name), data, 0);
    unlock_core(ctx);
    switch (err) {
    case M_OPT_MISSING_PARAM:
    case M_OPT_INVALID:
        return MPV_ERROR_OPTION_ERROR;
    case M_OPT_OUT_OF_RANGE:
        return MPV_ERROR_OPTION_FORMAT;
    case M_OPT_UNKNOWN:
        return MPV_ERROR_OPTION_NOT_FOUND;
    default:
        if (err >= 0)
            return 0;
        return MPV_ERROR_OPTION_ERROR;
    }
}

int mpv_set_option_string(mpv_handle *ctx, const char *name, const char *data)
{
    return mpv_set_option(ctx, name, MPV_FORMAT_STRING, &data);
}

// Run a command in the playback thread.
static void run_locked(mpv_handle *ctx, void (*fn)(void *fn_data), void *fn_data)
{
    mp_dispatch_lock(ctx->mpctx->dispatch);
    fn(fn_data);
    mp_dispatch_unlock(ctx->mpctx->dispatch);
}

// Run a command asynchronously. It's the responsibility of the caller to
// actually send the reply. This helper merely saves a small part of the
// required boilerplate to do so.
//  fn: callback to execute the request
//  fn_data: opaque caller-defined argument for fn. This will be automatically
//           freed with talloc_free(fn_data).
static int run_async(mpv_handle *ctx, void (*fn)(void *fn_data), void *fn_data)
{
    int err = reserve_reply(ctx);
    if (err < 0) {
        talloc_free(fn_data);
        return err;
    }
    mp_dispatch_enqueue(ctx->mpctx->dispatch, fn, fn_data);
    return 0;
}

struct cmd_request {
    struct MPContext *mpctx;
    struct mp_cmd *cmd;
    int status;
    struct mpv_node *res;
    struct mp_waiter completion;
};

static void cmd_complete(struct mp_cmd_ctx *cmd)
{
    struct cmd_request *req = cmd->on_completion_priv;

    req->status = cmd->success ? 0 : MPV_ERROR_COMMAND;
    if (req->res) {
        *req->res = cmd->result;
        cmd->result = (mpv_node){0};
    }

    // Unblock the waiting thread (especially for async commands).
    mp_waiter_wakeup(&req->completion, 0);
}

static int run_client_command(mpv_handle *ctx, struct mp_cmd *cmd, mpv_node *res)
{
    if (!cmd)
        return MPV_ERROR_INVALID_PARAMETER;
    if (!ctx->mpctx->initialized) {
        talloc_free(cmd);
        return MPV_ERROR_UNINITIALIZED;
    }

    cmd->sender = ctx->name;

    struct cmd_request req = {
        .mpctx = ctx->mpctx,
        .cmd = cmd,
        .res = res,
        .completion = MP_WAITER_INITIALIZER,
    };

    bool async = cmd->flags & MP_ASYNC_CMD;

    lock_core(ctx);
    if (async) {
        run_command(ctx->mpctx, cmd, NULL, NULL, NULL);
    } else {
        struct mp_abort_entry *abort = NULL;
        if (cmd->def->can_abort) {
            abort = talloc_zero(NULL, struct mp_abort_entry);
            abort->client = ctx;
        }
        run_command(ctx->mpctx, cmd, abort, cmd_complete, &req);
    }
    unlock_core(ctx);

    if (!async)
        mp_waiter_wait(&req.completion);

    return req.status;
}

int mpv_command(mpv_handle *ctx, const char **args)
{
    return run_client_command(ctx, mp_input_parse_cmd_strv(ctx->log, args), NULL);
}

int mpv_command_node(mpv_handle *ctx, mpv_node *args, mpv_node *result)
{
    struct mpv_node rn = {.format = MPV_FORMAT_NONE};
    int r = run_client_command(ctx, mp_input_parse_cmd_node(ctx->log, args), &rn);
    if (result && r >= 0)
        *result = rn;
    return r;
}

int mpv_command_ret(mpv_handle *ctx, const char **args, mpv_node *result)
{
    struct mpv_node rn = {.format = MPV_FORMAT_NONE};
    int r = run_client_command(ctx, mp_input_parse_cmd_strv(ctx->log, args), &rn);
    if (result && r >= 0)
        *result = rn;
    return r;
}

int mpv_command_string(mpv_handle *ctx, const char *args)
{
    return run_client_command(ctx,
        mp_input_parse_cmd(ctx->mpctx->input, bstr0((char*)args), ctx->name), NULL);
}

struct async_cmd_request {
    struct MPContext *mpctx;
    struct mp_cmd *cmd;
    struct mpv_handle *reply_ctx;
    uint64_t userdata;
};

static void async_cmd_complete(struct mp_cmd_ctx *cmd)
{
    struct async_cmd_request *req = cmd->on_completion_priv;

    struct mpv_event_command *data = talloc_zero(NULL, struct mpv_event_command);
    data->result = cmd->result;
    cmd->result = (mpv_node){0};
    talloc_steal(data, node_get_alloc(&data->result));

    struct mpv_event reply = {
        .event_id = MPV_EVENT_COMMAND_REPLY,
        .data = data,
        .error = cmd->success ? 0 : MPV_ERROR_COMMAND,
    };
    send_reply(req->reply_ctx, req->userdata, &reply);

    talloc_free(req);
}

static void async_cmd_fn(void *data)
{
    struct async_cmd_request *req = data;

    struct mp_cmd *cmd = req->cmd;
    ta_set_parent(cmd, NULL);
    req->cmd = NULL;

    struct mp_abort_entry *abort = NULL;
    if (cmd->def->can_abort) {
        abort = talloc_zero(NULL, struct mp_abort_entry);
        abort->client = req->reply_ctx;
        abort->client_work_type = MPV_EVENT_COMMAND_REPLY;
        abort->client_work_id = req->userdata;
    }

    // This will synchronously or asynchronously call cmd_complete (depending
    // on the command).
    run_command(req->mpctx, cmd, abort, async_cmd_complete, req);
}

static int run_async_cmd(mpv_handle *ctx, uint64_t ud, struct mp_cmd *cmd)
{
    if (!cmd)
        return MPV_ERROR_INVALID_PARAMETER;
    if (!ctx->mpctx->initialized) {
        talloc_free(cmd);
        return MPV_ERROR_UNINITIALIZED;
    }

    cmd->sender = ctx->name;

    struct async_cmd_request *req = talloc_ptrtype(NULL, req);
    *req = (struct async_cmd_request){
        .mpctx = ctx->mpctx,
        .cmd = talloc_steal(req, cmd),
        .reply_ctx = ctx,
        .userdata = ud,
    };
    return run_async(ctx, async_cmd_fn, req);
}

int mpv_command_async(mpv_handle *ctx, uint64_t ud, const char **args)
{
    return run_async_cmd(ctx, ud, mp_input_parse_cmd_strv(ctx->log, args));
}

int mpv_command_node_async(mpv_handle *ctx, uint64_t ud, mpv_node *args)
{
    return run_async_cmd(ctx, ud, mp_input_parse_cmd_node(ctx->log, args));
}

void mpv_abort_async_command(mpv_handle *ctx, uint64_t reply_userdata)
{
    abort_async(ctx->mpctx, ctx, MPV_EVENT_COMMAND_REPLY, reply_userdata);
}

static int translate_property_error(int errc)
{
    switch (errc) {
    case M_PROPERTY_OK:                 return 0;
    case M_PROPERTY_ERROR:              return MPV_ERROR_PROPERTY_ERROR;
    case M_PROPERTY_UNAVAILABLE:        return MPV_ERROR_PROPERTY_UNAVAILABLE;
    case M_PROPERTY_NOT_IMPLEMENTED:    return MPV_ERROR_PROPERTY_ERROR;
    case M_PROPERTY_UNKNOWN:            return MPV_ERROR_PROPERTY_NOT_FOUND;
    case M_PROPERTY_INVALID_FORMAT:     return MPV_ERROR_PROPERTY_FORMAT;
    // shouldn't happen
    default:                            return MPV_ERROR_PROPERTY_ERROR;
    }
}

struct setproperty_request {
    struct MPContext *mpctx;
    const char *name;
    int format;
    void *data;
    int status;
    struct mpv_handle *reply_ctx;
    uint64_t userdata;
};

static void setproperty_fn(void *arg)
{
    struct setproperty_request *req = arg;
    const struct m_option *type = get_mp_type(req->format);

    struct mpv_node *node;
    struct mpv_node tmp;
    if (req->format == MPV_FORMAT_NODE) {
        node = req->data;
    } else {
        tmp.format = req->format;
        memcpy(&tmp.u, req->data, type->type->size);
        node = &tmp;
    }

    int err = mp_property_do(req->name, M_PROPERTY_SET_NODE, node, req->mpctx);

    req->status = translate_property_error(err);

    if (req->reply_ctx) {
        struct mpv_event reply = {
            .event_id = MPV_EVENT_SET_PROPERTY_REPLY,
            .error = req->status,
        };
        send_reply(req->reply_ctx, req->userdata, &reply);
        talloc_free(req);
    }
}

int mpv_set_property(mpv_handle *ctx, const char *name, mpv_format format,
                     void *data)
{
    if (!ctx->mpctx->initialized) {
        int r = mpv_set_option(ctx, name, format, data);
        if (r == MPV_ERROR_OPTION_NOT_FOUND &&
            mp_get_property_id(ctx->mpctx, name) >= 0)
            return MPV_ERROR_PROPERTY_UNAVAILABLE;
        switch (r) {
        case MPV_ERROR_SUCCESS:          return MPV_ERROR_SUCCESS;
        case MPV_ERROR_OPTION_FORMAT:    return MPV_ERROR_PROPERTY_FORMAT;
        case MPV_ERROR_OPTION_NOT_FOUND: return MPV_ERROR_PROPERTY_NOT_FOUND;
        default:                         return MPV_ERROR_PROPERTY_ERROR;
        }
    }
    if (!get_mp_type(format))
        return MPV_ERROR_PROPERTY_FORMAT;

    struct setproperty_request req = {
        .mpctx = ctx->mpctx,
        .name = name,
        .format = format,
        .data = data,
    };
    run_locked(ctx, setproperty_fn, &req);
    return req.status;
}

int mpv_set_property_string(mpv_handle *ctx, const char *name, const char *data)
{
    return mpv_set_property(ctx, name, MPV_FORMAT_STRING, &data);
}

static void free_prop_set_req(void *ptr)
{
    struct setproperty_request *req = ptr;
    const struct m_option *type = get_mp_type(req->format);
    m_option_free(type, req->data);
}

int mpv_set_property_async(mpv_handle *ctx, uint64_t ud, const char *name,
                           mpv_format format, void *data)
{
    const struct m_option *type = get_mp_type(format);
    if (!ctx->mpctx->initialized)
        return MPV_ERROR_UNINITIALIZED;
    if (!type)
        return MPV_ERROR_PROPERTY_FORMAT;

    struct setproperty_request *req = talloc_ptrtype(NULL, req);
    *req = (struct setproperty_request){
        .mpctx = ctx->mpctx,
        .name = talloc_strdup(req, name),
        .format = format,
        .data = talloc_zero_size(req, type->type->size),
        .reply_ctx = ctx,
        .userdata = ud,
    };

    m_option_copy(type, req->data, data);
    talloc_set_destructor(req, free_prop_set_req);

    return run_async(ctx, setproperty_fn, req);
}

struct getproperty_request {
    struct MPContext *mpctx;
    const char *name;
    mpv_format format;
    void *data;
    int status;
    struct mpv_handle *reply_ctx;
    uint64_t userdata;
};

static void free_prop_data(void *ptr)
{
    struct mpv_event_property *prop = ptr;
    const struct m_option *type = get_mp_type_get(prop->format);
    m_option_free(type, prop->data);
}

static void getproperty_fn(void *arg)
{
    struct getproperty_request *req = arg;
    const struct m_option *type = get_mp_type_get(req->format);

    union m_option_value xdata = {0};
    void *data = req->data ? req->data : &xdata;

    int err = -1;
    switch (req->format) {
    case MPV_FORMAT_OSD_STRING:
        err = mp_property_do(req->name, M_PROPERTY_PRINT, data, req->mpctx);
        break;
    case MPV_FORMAT_STRING: {
        char *s = NULL;
        err = mp_property_do(req->name, M_PROPERTY_GET_STRING, &s, req->mpctx);
        if (err == M_PROPERTY_OK)
            *(char **)data = s;
        break;
    }
    case MPV_FORMAT_NODE:
    case MPV_FORMAT_FLAG:
    case MPV_FORMAT_INT64:
    case MPV_FORMAT_DOUBLE: {
        struct mpv_node node = {{0}};
        err = mp_property_do(req->name, M_PROPERTY_GET_NODE, &node, req->mpctx);
        if (err == M_PROPERTY_NOT_IMPLEMENTED) {
            // Go through explicit string conversion. Same reasoning as on the
            // GET code path.
            char *s = NULL;
            err = mp_property_do(req->name, M_PROPERTY_GET_STRING, &s,
                                 req->mpctx);
            if (err != M_PROPERTY_OK)
                break;
            node.format = MPV_FORMAT_STRING;
            node.u.string = s;
        } else if (err <= 0)
            break;
        if (req->format == MPV_FORMAT_NODE) {
            *(struct mpv_node *)data = node;
        } else {
            if (!conv_node_to_format(data, req->format, &node)) {
                err = M_PROPERTY_INVALID_FORMAT;
                mpv_free_node_contents(&node);
            }
        }
        break;
    }
    default:
        abort();
    }

    req->status = translate_property_error(err);

    if (req->reply_ctx) {
        struct mpv_event_property *prop = talloc_ptrtype(NULL, prop);
        *prop = (struct mpv_event_property){
            .name = talloc_steal(prop, (char *)req->name),
            .format = req->format,
            .data = talloc_size(prop, type->type->size),
        };
        // move data
        memcpy(prop->data, &xdata, type->type->size);
        talloc_set_destructor(prop, free_prop_data);
        struct mpv_event reply = {
            .event_id = MPV_EVENT_GET_PROPERTY_REPLY,
            .data = prop,
            .error = req->status,
        };
        send_reply(req->reply_ctx, req->userdata, &reply);
        talloc_free(req);
    }
}

int mpv_get_property(mpv_handle *ctx, const char *name, mpv_format format,
                     void *data)
{
    if (!ctx->mpctx->initialized)
        return MPV_ERROR_UNINITIALIZED;
    if (!data)
        return MPV_ERROR_INVALID_PARAMETER;
    if (!get_mp_type_get(format))
        return MPV_ERROR_PROPERTY_FORMAT;

    struct getproperty_request req = {
        .mpctx = ctx->mpctx,
        .name = name,
        .format = format,
        .data = data,
    };
    run_locked(ctx, getproperty_fn, &req);
    return req.status;
}

char *mpv_get_property_string(mpv_handle *ctx, const char *name)
{
    char *str = NULL;
    mpv_get_property(ctx, name, MPV_FORMAT_STRING, &str);
    return str;
}

char *mpv_get_property_osd_string(mpv_handle *ctx, const char *name)
{
    char *str = NULL;
    mpv_get_property(ctx, name, MPV_FORMAT_OSD_STRING, &str);
    return str;
}

int mpv_get_property_async(mpv_handle *ctx, uint64_t ud, const char *name,
                           mpv_format format)
{
    if (!ctx->mpctx->initialized)
        return MPV_ERROR_UNINITIALIZED;
    if (!get_mp_type_get(format))
        return MPV_ERROR_PROPERTY_FORMAT;

    struct getproperty_request *req = talloc_ptrtype(NULL, req);
    *req = (struct getproperty_request){
        .mpctx = ctx->mpctx,
        .name = talloc_strdup(req, name),
        .format = format,
        .reply_ctx = ctx,
        .userdata = ud,
    };
    return run_async(ctx, getproperty_fn, req);
}

static void property_free(void *p)
{
    struct observe_property *prop = p;

    assert(prop->refcount == 0);

    if (prop->type) {
        m_option_free(prop->type, &prop->value);
        m_option_free(prop->type, &prop->value_ret);
    }
}

int mpv_observe_property(mpv_handle *ctx, uint64_t userdata,
                         const char *name, mpv_format format)
{
    const struct m_option *type = get_mp_type_get(format);
    if (format != MPV_FORMAT_NONE && !type)
        return MPV_ERROR_PROPERTY_FORMAT;
    // Explicitly disallow this, because it would require a special code path.
    if (format == MPV_FORMAT_OSD_STRING)
        return MPV_ERROR_PROPERTY_FORMAT;

    pthread_mutex_lock(&ctx->lock);
    assert(!ctx->destroying);
    struct observe_property *prop = talloc_ptrtype(ctx, prop);
    talloc_set_destructor(prop, property_free);
    *prop = (struct observe_property){
        .owner = ctx,
        .name = talloc_strdup(prop, name),
        .id = mp_get_property_id(ctx->mpctx, name),
        .event_mask = mp_get_property_event_mask(name),
        .reply_id = userdata,
        .format = format,
        .type = type,
        .change_ts = 1, // force initial event
        .refcount = 1,
    };
    ctx->properties_change_ts += 1;
    MP_TARRAY_APPEND(ctx, ctx->properties, ctx->num_properties, prop);
    ctx->property_event_masks |= prop->event_mask;
    ctx->new_property_events = true;
    ctx->cur_property_index = 0;
    ctx->has_pending_properties = true;
    pthread_mutex_unlock(&ctx->lock);
    mp_wakeup_core(ctx->mpctx);
    return 0;
}

int mpv_unobserve_property(mpv_handle *ctx, uint64_t userdata)
{
    pthread_mutex_lock(&ctx->lock);
    int count = 0;
    for (int n = ctx->num_properties - 1; n >= 0; n--) {
        struct observe_property *prop = ctx->properties[n];
        // Perform actual removal of the property lazily to avoid creating
        // dangling pointers and such.
        if (prop->reply_id == userdata) {
            prop_unref(prop);
            ctx->properties_change_ts += 1;
            MP_TARRAY_REMOVE_AT(ctx->properties, ctx->num_properties, n);
            ctx->cur_property_index = 0;
            count++;
        }
    }
    pthread_mutex_unlock(&ctx->lock);
    return count;
}

// Broadcast that a property has changed.
void mp_client_property_change(struct MPContext *mpctx, const char *name)
{
    struct mp_client_api *clients = mpctx->clients;
    int id = mp_get_property_id(mpctx, name);
    bool any_pending = false;

    pthread_mutex_lock(&clients->lock);

    for (int n = 0; n < clients->num_clients; n++) {
        struct mpv_handle *client = clients->clients[n];
        pthread_mutex_lock(&client->lock);
        for (int i = 0; i < client->num_properties; i++) {
            if (client->properties[i]->id == id) {
                client->properties[i]->change_ts += 1;
                client->has_pending_properties = true;
                any_pending = true;
            }
        }
        pthread_mutex_unlock(&client->lock);
    }

    pthread_mutex_unlock(&clients->lock);

    // If we're inside mp_dispatch_queue_process(), this will cause the playloop
    // to be re-run (to get mp_client_send_property_changes() called). If we're
    // inside the normal playloop, this does nothing, but the latter function
    // will be called at the end of the playloop anyway.
    if (any_pending)
        mp_dispatch_adjust_timeout(mpctx->dispatch, 0);
}

// Mark properties as changed in reaction to specific events.
// Called with ctx->lock held.
static void notify_property_events(struct mpv_handle *ctx, int event)
{
    uint64_t mask = 1ULL << event;
    for (int i = 0; i < ctx->num_properties; i++) {
        if (ctx->properties[i]->event_mask & mask) {
            ctx->properties[i]->change_ts += 1;
            ctx->has_pending_properties = true;
        }
    }

    // Same as in mp_client_property_change().
    if (ctx->has_pending_properties)
        mp_dispatch_adjust_timeout(ctx->mpctx->dispatch, 0);
}

// Call with ctx->lock held (only). May temporarily drop the lock.
static void send_client_property_changes(struct mpv_handle *ctx)
{
    uint64_t cur_ts = ctx->properties_change_ts;

    ctx->has_pending_properties = false;

    for (int n = 0; n < ctx->num_properties; n++) {
        struct observe_property *prop = ctx->properties[n];

        if (prop->value_ts == prop->change_ts)
            continue;

        bool changed = false;
        if (prop->format) {
            const struct m_option *type = prop->type;
            union m_option_value val = {0};
            struct getproperty_request req = {
                .mpctx = ctx->mpctx,
                .name = prop->name,
                .format = prop->format,
                .data = &val,
            };

            // Temporarily unlock and read the property. The very important
            // thing is that property getters can do whatever they want, _and_
            // that they may wait on the client API user thread (if vo_libmpv
            // or similar things are involved).
            prop->refcount += 1; // keep prop alive (esp. prop->name)
            ctx->async_counter += 1; // keep ctx alive
            pthread_mutex_unlock(&ctx->lock);
            getproperty_fn(&req);
            pthread_mutex_lock(&ctx->lock);
            ctx->async_counter -= 1;
            prop_unref(prop);

            // Set if observed properties was changed or something similar
            // => start over, retry next time.
            if (cur_ts != ctx->properties_change_ts || ctx->destroying) {
                m_option_free(type, &val);
                mp_wakeup_core(ctx->mpctx);
                ctx->has_pending_properties = true;
                break;
            }
            assert(prop->refcount > 0);

            bool val_valid = req.status >= 0;
            changed = prop->value_valid != val_valid;
            if (prop->value_valid && val_valid)
                changed = !equal_mpv_value(&prop->value, &val, prop->format);
            if (prop->value_ts == 0)
                changed = true; // initial event

            prop->value_valid = val_valid;
            if (changed && val_valid) {
                // move val to prop->value
                m_option_free(type, &prop->value);
                memcpy(&prop->value, &val, type->type->size);
                memset(&val, 0, type->type->size);
            }

            m_option_free(prop->type, &val);
        } else {
            changed = true;
        }

        if (prop->waiting_for_hook)
            ctx->new_property_events = true; // make sure to wakeup

        // Avoid retriggering the change event if the property didn't change,
        // and the previous value was actually returned to the client.
        if (!changed && prop->value_ret_ts == prop->value_ts) {
            prop->value_ret_ts = prop->change_ts; // no change => no event
            prop->waiting_for_hook = false;
        } else {
            ctx->new_property_events = true;
        }

        prop->value_ts = prop->change_ts;
    }

    if (ctx->destroying || ctx->new_property_events)
        wakeup_client(ctx);
}

void mp_client_send_property_changes(struct MPContext *mpctx)
{
    struct mp_client_api *clients = mpctx->clients;

    pthread_mutex_lock(&clients->lock);
    uint64_t cur_ts = clients->clients_list_change_ts;

    for (int n = 0; n < clients->num_clients; n++) {
        struct mpv_handle *ctx = clients->clients[n];

        pthread_mutex_lock(&ctx->lock);
        if (!ctx->has_pending_properties || ctx->destroying) {
            pthread_mutex_unlock(&ctx->lock);
            continue;
        }
        // Keep ctx->lock locked (unlock order does not matter).
        pthread_mutex_unlock(&clients->lock);
        send_client_property_changes(ctx);
        pthread_mutex_unlock(&ctx->lock);
        pthread_mutex_lock(&clients->lock);
        if (cur_ts != clients->clients_list_change_ts) {
            // List changed; need to start over. Do it in the next iteration.
            mp_wakeup_core(mpctx);
            break;
        }
    }

    pthread_mutex_unlock(&clients->lock);
}

// Set ctx->cur_event to a generated property change event, if there is any
// outstanding property.
static bool gen_property_change_event(struct mpv_handle *ctx)
{
    if (!ctx->mpctx->initialized)
        return false;

    while (1) {
        if (ctx->cur_property_index >= ctx->num_properties) {
            ctx->new_property_events &= ctx->num_properties > 0;
            if (!ctx->new_property_events)
                break;
            ctx->new_property_events = false;
            ctx->cur_property_index = 0;
        }

        struct observe_property *prop = ctx->properties[ctx->cur_property_index++];

        if (prop->value_ts == prop->change_ts &&    // not a stale value?
            prop->value_ret_ts != prop->value_ts)   // other value than last time?
        {
            prop->value_ret_ts = prop->value_ts;
            prop->waiting_for_hook = false;
            prop_unref(ctx->cur_property);
            ctx->cur_property = prop;
            prop->refcount += 1;

            if (prop->value_valid)
                m_option_copy(prop->type, &prop->value_ret, &prop->value);

            ctx->cur_property_event = (struct mpv_event_property){
                .name = prop->name,
                .format = prop->value_valid ? prop->format : 0,
                .data = prop->value_valid ? &prop->value_ret : NULL,
            };
            *ctx->cur_event = (struct mpv_event){
                .event_id = MPV_EVENT_PROPERTY_CHANGE,
                .reply_userdata = prop->reply_id,
                .data = &ctx->cur_property_event,
            };
            return true;
        }
    }

    return false;
}

int mpv_hook_add(mpv_handle *ctx, uint64_t reply_userdata,
                 const char *name, int priority)
{
    lock_core(ctx);
    mp_hook_add(ctx->mpctx, ctx->name, ctx->id, name, reply_userdata, priority);
    unlock_core(ctx);
    return 0;
}

int mpv_hook_continue(mpv_handle *ctx, uint64_t id)
{
    lock_core(ctx);
    int r = mp_hook_continue(ctx->mpctx, ctx->id, id);
    unlock_core(ctx);
    return r;
}

int mpv_load_config_file(mpv_handle *ctx, const char *filename)
{
    lock_core(ctx);
    int r = m_config_parse_config_file(ctx->mpctx->mconfig, filename, NULL, 0);
    unlock_core(ctx);
    if (r == 0)
        return MPV_ERROR_INVALID_PARAMETER;
    if (r < 0)
        return MPV_ERROR_OPTION_ERROR;
    return 0;
}

static void msg_wakeup(void *p)
{
    mpv_handle *ctx = p;
    wakeup_client(ctx);
}

// Undocumented: if min_level starts with "silent:", then log messages are not
// returned to the API user, but are stored until logging is enabled normally
// again by calling this without "silent:". (Using a different level will
// flush it, though.)
int mpv_request_log_messages(mpv_handle *ctx, const char *min_level)
{
    bstr blevel = bstr0(min_level);
    bool silent = bstr_eatstart0(&blevel, "silent:");

    int level = -1;
    for (int n = 0; n < MSGL_MAX + 1; n++) {
        if (mp_log_levels[n] && bstr_equals0(blevel, mp_log_levels[n])) {
            level = n;
            break;
        }
    }
    if (bstr_equals0(blevel, "terminal-default"))
        level = MP_LOG_BUFFER_MSGL_TERM;

    if (level < 0 && strcmp(min_level, "no") != 0)
        return MPV_ERROR_INVALID_PARAMETER;

    pthread_mutex_lock(&ctx->lock);
    if (level < 0 || level != ctx->messages_level) {
        mp_msg_log_buffer_destroy(ctx->messages);
        ctx->messages = NULL;
    }
    if (level >= 0) {
        if (!ctx->messages) {
            int size = level >= MSGL_V ? 10000 : 1000;
            ctx->messages = mp_msg_log_buffer_new(ctx->mpctx->global, size,
                                                  level, msg_wakeup, ctx);
            ctx->messages_level = level;
        }
        mp_msg_log_buffer_set_silent(ctx->messages, silent);
    }
    wakeup_client(ctx);
    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

// Set ctx->cur_event to a generated log message event, if any available.
static bool gen_log_message_event(struct mpv_handle *ctx)
{
    if (ctx->messages) {
        struct mp_log_buffer_entry *msg =
            mp_msg_log_buffer_read(ctx->messages);
        if (msg) {
            struct mpv_event_log_message *cmsg =
                talloc_ptrtype(ctx->cur_event, cmsg);
            talloc_steal(cmsg, msg);
            *cmsg = (struct mpv_event_log_message){
                .prefix = msg->prefix,
                .level = mp_log_levels[msg->level],
                .log_level = mp_mpv_log_levels[msg->level],
                .text = msg->text,
            };
            *ctx->cur_event = (struct mpv_event){
                .event_id = MPV_EVENT_LOG_MESSAGE,
                .data = cmsg,
            };
            return true;
        }
    }
    return false;
}

int mpv_get_wakeup_pipe(mpv_handle *ctx)
{
    pthread_mutex_lock(&ctx->wakeup_lock);
    if (ctx->wakeup_pipe[0] == -1) {
        if (mp_make_wakeup_pipe(ctx->wakeup_pipe) >= 0)
            (void)write(ctx->wakeup_pipe[1], &(char){0}, 1);
    }
    int fd = ctx->wakeup_pipe[0];
    pthread_mutex_unlock(&ctx->wakeup_lock);
    return fd;
}

unsigned long mpv_client_api_version(void)
{
    return MPV_CLIENT_API_VERSION;
}

int mpv_event_to_node(mpv_node *dst, mpv_event *event)
{
    *dst = (mpv_node){0};

    node_init(dst, MPV_FORMAT_NODE_MAP, NULL);
    node_map_add_string(dst, "event", mpv_event_name(event->event_id));

    if (event->error < 0)
        node_map_add_string(dst, "error", mpv_error_string(event->error));

    if (event->reply_userdata)
        node_map_add_int64(dst, "id", event->reply_userdata);

    switch (event->event_id) {

    case MPV_EVENT_START_FILE: {
        mpv_event_start_file *esf = event->data;

        node_map_add_int64(dst, "playlist_entry_id", esf->playlist_entry_id);
        break;
    }

    case MPV_EVENT_END_FILE: {
        mpv_event_end_file *eef = event->data;

        const char *reason;
        switch (eef->reason) {
        case MPV_END_FILE_REASON_EOF: reason = "eof"; break;
        case MPV_END_FILE_REASON_STOP: reason = "stop"; break;
        case MPV_END_FILE_REASON_QUIT: reason = "quit"; break;
        case MPV_END_FILE_REASON_ERROR: reason = "error"; break;
        case MPV_END_FILE_REASON_REDIRECT: reason = "redirect"; break;
        default:
            reason = "unknown";
        }
        node_map_add_string(dst, "reason", reason);

        node_map_add_int64(dst, "playlist_entry_id", eef->playlist_entry_id);

        if (eef->playlist_insert_id) {
            node_map_add_int64(dst, "playlist_insert_id", eef->playlist_insert_id);
            node_map_add_int64(dst, "playlist_insert_num_entries",
                               eef->playlist_insert_num_entries);
        }

        if (eef->reason == MPV_END_FILE_REASON_ERROR)
            node_map_add_string(dst, "file_error", mpv_error_string(eef->error));
        break;
    }

    case MPV_EVENT_LOG_MESSAGE: {
        mpv_event_log_message *msg = event->data;

        node_map_add_string(dst, "prefix", msg->prefix);
        node_map_add_string(dst, "level",  msg->level);
        node_map_add_string(dst, "text",   msg->text);
        break;
    }

    case MPV_EVENT_CLIENT_MESSAGE: {
        mpv_event_client_message *msg = event->data;

        struct mpv_node *args = node_map_add(dst, "args", MPV_FORMAT_NODE_ARRAY);
        for (int n = 0; n < msg->num_args; n++) {
            struct mpv_node *sn = node_array_add(args, MPV_FORMAT_NONE);
            sn->format = MPV_FORMAT_STRING;
            sn->u.string = (char *)msg->args[n];
        }
        break;
    }

    case MPV_EVENT_PROPERTY_CHANGE: {
        mpv_event_property *prop = event->data;

        node_map_add_string(dst, "name", prop->name);

        switch (prop->format) {
        case MPV_FORMAT_NODE:
            *node_map_add(dst, "data", MPV_FORMAT_NONE) =
                *(struct mpv_node *)prop->data;
            break;
        case MPV_FORMAT_DOUBLE:
            node_map_add_double(dst, "data", *(double *)prop->data);
            break;
        case MPV_FORMAT_FLAG:
            node_map_add_flag(dst, "data", *(int *)prop->data);
            break;
        case MPV_FORMAT_STRING:
            node_map_add_string(dst, "data", *(char **)prop->data);
            break;
        default: ;
        }
        break;
    }

    case MPV_EVENT_COMMAND_REPLY: {
        mpv_event_command *cmd = event->data;

        *node_map_add(dst, "result", MPV_FORMAT_NONE) = cmd->result;
        break;
    }

    case MPV_EVENT_HOOK: {
        mpv_event_hook *hook = event->data;

        node_map_add_int64(dst, "hook_id", hook->id);
        break;
    }

    }
    return 0;
}

static const char *const err_table[] = {
    [-MPV_ERROR_SUCCESS] = "success",
    [-MPV_ERROR_EVENT_QUEUE_FULL] = "event queue full",
    [-MPV_ERROR_NOMEM] = "memory allocation failed",
    [-MPV_ERROR_UNINITIALIZED] = "core not uninitialized",
    [-MPV_ERROR_INVALID_PARAMETER] = "invalid parameter",
    [-MPV_ERROR_OPTION_NOT_FOUND] = "option not found",
    [-MPV_ERROR_OPTION_FORMAT] = "unsupported format for accessing option",
    [-MPV_ERROR_OPTION_ERROR] = "error setting option",
    [-MPV_ERROR_PROPERTY_NOT_FOUND] = "property not found",
    [-MPV_ERROR_PROPERTY_FORMAT] = "unsupported format for accessing property",
    [-MPV_ERROR_PROPERTY_UNAVAILABLE] = "property unavailable",
    [-MPV_ERROR_PROPERTY_ERROR] = "error accessing property",
    [-MPV_ERROR_COMMAND] = "error running command",
    [-MPV_ERROR_LOADING_FAILED] = "loading failed",
    [-MPV_ERROR_AO_INIT_FAILED] = "audio output initialization failed",
    [-MPV_ERROR_VO_INIT_FAILED] = "video output initialization failed",
    [-MPV_ERROR_NOTHING_TO_PLAY] = "no audio or video data played",
    [-MPV_ERROR_UNKNOWN_FORMAT] = "unrecognized file format",
    [-MPV_ERROR_UNSUPPORTED] = "not supported",
    [-MPV_ERROR_NOT_IMPLEMENTED] = "operation not implemented",
    [-MPV_ERROR_GENERIC] = "something happened",
};

const char *mpv_error_string(int error)
{
    error = -error;
    if (error < 0)
        error = 0;
    const char *name = NULL;
    if (error < MP_ARRAY_SIZE(err_table))
        name = err_table[error];
    return name ? name : "unknown error";
}

static const char *const event_table[] = {
    [MPV_EVENT_NONE] = "none",
    [MPV_EVENT_SHUTDOWN] = "shutdown",
    [MPV_EVENT_LOG_MESSAGE] = "log-message",
    [MPV_EVENT_GET_PROPERTY_REPLY] = "get-property-reply",
    [MPV_EVENT_SET_PROPERTY_REPLY] = "set-property-reply",
    [MPV_EVENT_COMMAND_REPLY] = "command-reply",
    [MPV_EVENT_START_FILE] = "start-file",
    [MPV_EVENT_END_FILE] = "end-file",
    [MPV_EVENT_FILE_LOADED] = "file-loaded",
    [MPV_EVENT_TRACKS_CHANGED] = "tracks-changed",
    [MPV_EVENT_TRACK_SWITCHED] = "track-switched",
    [MPV_EVENT_IDLE] = "idle",
    [MPV_EVENT_PAUSE] = "pause",
    [MPV_EVENT_UNPAUSE] = "unpause",
    [MPV_EVENT_TICK] = "tick",
    [MPV_EVENT_SCRIPT_INPUT_DISPATCH] = "script-input-dispatch",
    [MPV_EVENT_CLIENT_MESSAGE] = "client-message",
    [MPV_EVENT_VIDEO_RECONFIG] = "video-reconfig",
    [MPV_EVENT_AUDIO_RECONFIG] = "audio-reconfig",
    [MPV_EVENT_METADATA_UPDATE] = "metadata-update",
    [MPV_EVENT_SEEK] = "seek",
    [MPV_EVENT_PLAYBACK_RESTART] = "playback-restart",
    [MPV_EVENT_PROPERTY_CHANGE] = "property-change",
    [MPV_EVENT_CHAPTER_CHANGE] = "chapter-change",
    [MPV_EVENT_QUEUE_OVERFLOW] = "event-queue-overflow",
    [MPV_EVENT_HOOK] = "hook",
};

const char *mpv_event_name(mpv_event_id event)
{
    if ((unsigned)event >= MP_ARRAY_SIZE(event_table))
        return NULL;
    return event_table[event];
}

void mpv_free(void *data)
{
    talloc_free(data);
}

int64_t mpv_get_time_us(mpv_handle *ctx)
{
    return mp_time_us();
}

#include "video/out/libmpv.h"

static void do_kill(void *ptr)
{
    struct MPContext *mpctx = ptr;

    struct track *track = mpctx->vo_chain ? mpctx->vo_chain->track : NULL;
    uninit_video_out(mpctx);
    if (track) {
        mpctx->error_playing = MPV_ERROR_VO_INIT_FAILED;
        error_on_track(mpctx, track);
    }
}

// Used by vo_libmpv to (a)synchronously uninitialize video.
void kill_video_async(struct mp_client_api *client_api)
{
    struct MPContext *mpctx = client_api->mpctx;
    mp_dispatch_enqueue(mpctx->dispatch, do_kill, mpctx);
}

// Used by vo_libmpv to set the current render context.
bool mp_set_main_render_context(struct mp_client_api *client_api,
                                struct mpv_render_context *ctx, bool active)
{
    assert(ctx);

    pthread_mutex_lock(&client_api->lock);
    bool is_set = !!client_api->render_context;
    bool is_same = client_api->render_context == ctx;
    // Can set if it doesn't remove another existing ctx.
    bool res = is_same || !is_set;
    if (res)
        client_api->render_context = active ? ctx : NULL;
    pthread_mutex_unlock(&client_api->lock);
    return res;
}

// Used by vo_libmpv. Relies on guarantees by mp_render_context_acquire().
struct mpv_render_context *
mp_client_api_acquire_render_context(struct mp_client_api *ca)
{
    struct mpv_render_context *res = NULL;
    pthread_mutex_lock(&ca->lock);
    if (ca->render_context && mp_render_context_acquire(ca->render_context))
        res = ca->render_context;
    pthread_mutex_unlock(&ca->lock);
    return res;
}

// Emulation of old opengl_cb API.

#include "libmpv/opengl_cb.h"
#include "libmpv/render_gl.h"

void mpv_opengl_cb_set_update_callback(mpv_opengl_cb_context *ctx,
                                       mpv_opengl_cb_update_fn callback,
                                       void *callback_ctx)
{
}

int mpv_opengl_cb_init_gl(mpv_opengl_cb_context *ctx, const char *exts,
                          mpv_opengl_cb_get_proc_address_fn get_proc_address,
                          void *get_proc_address_ctx)
{
    return MPV_ERROR_NOT_IMPLEMENTED;
}

int mpv_opengl_cb_draw(mpv_opengl_cb_context *ctx, int fbo, int w, int h)
{
    return MPV_ERROR_NOT_IMPLEMENTED;
}

int mpv_opengl_cb_report_flip(mpv_opengl_cb_context *ctx, int64_t time)
{
    return MPV_ERROR_NOT_IMPLEMENTED;
}

int mpv_opengl_cb_uninit_gl(mpv_opengl_cb_context *ctx)
{
    return 0;
}

int mpv_opengl_cb_render(mpv_opengl_cb_context *ctx, int fbo, int vp[4])
{
    return MPV_ERROR_NOT_IMPLEMENTED;
}

void *mpv_get_sub_api(mpv_handle *ctx, mpv_sub_api sub_api)
{
    if (!ctx->mpctx->initialized || sub_api != MPV_SUB_API_OPENGL_CB)
        return NULL;
    // Return something non-NULL, as I think most API users will not check
    // this properly. The other opengl_cb stubs do not use this value.
    MP_WARN(ctx, "The opengl_cb API is not supported anymore.\n"
                 "Use the similar API in render.h instead.\n");
    return "no";
}

// stream_cb

struct mp_custom_protocol {
    char *protocol;
    void *user_data;
    mpv_stream_cb_open_ro_fn open_fn;
};

int mpv_stream_cb_add_ro(mpv_handle *ctx, const char *protocol, void *user_data,
                         mpv_stream_cb_open_ro_fn open_fn)
{
    if (!open_fn)
        return MPV_ERROR_INVALID_PARAMETER;

    struct mp_client_api *clients = ctx->clients;
    int r = 0;
    pthread_mutex_lock(&clients->lock);
    for (int n = 0; n < clients->num_custom_protocols; n++) {
        struct mp_custom_protocol *proto = &clients->custom_protocols[n];
        if (strcmp(proto->protocol, protocol) == 0) {
            r = MPV_ERROR_INVALID_PARAMETER;
            break;
        }
    }
    if (stream_has_proto(protocol))
        r = MPV_ERROR_INVALID_PARAMETER;
    if (r >= 0) {
        struct mp_custom_protocol proto = {
            .protocol = talloc_strdup(clients, protocol),
            .user_data = user_data,
            .open_fn = open_fn,
        };
        MP_TARRAY_APPEND(clients, clients->custom_protocols,
                         clients->num_custom_protocols, proto);
    }
    pthread_mutex_unlock(&clients->lock);
    return r;
}

bool mp_streamcb_lookup(struct mpv_global *g, const char *protocol,
                        void **out_user_data, mpv_stream_cb_open_ro_fn *out_fn)
{
    struct mp_client_api *clients = g->client_api;
    bool found = false;
    pthread_mutex_lock(&clients->lock);
    for (int n = 0; n < clients->num_custom_protocols; n++) {
        struct mp_custom_protocol *proto = &clients->custom_protocols[n];
        if (strcmp(proto->protocol, protocol) == 0) {
            *out_user_data = proto->user_data;
            *out_fn = proto->open_fn;
            found = true;
            break;
        }
    }
    pthread_mutex_unlock(&clients->lock);
    return found;
}
