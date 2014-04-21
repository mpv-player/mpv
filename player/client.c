/* Permission to use, copy, modify, and/or distribute this software for any
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
#include <assert.h>

#include "common/common.h"
#include "common/msg.h"
#include "common/msg_control.h"
#include "input/input.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "options/m_property.h"
#include "osdep/threads.h"
#include "osdep/timer.h"
#include "osdep/io.h"

#include "command.h"
#include "core.h"
#include "client.h"

#include "config.h"

/*
 * Locking hierarchy:
 *
 *  MPContext > mp_client_api.lock > mpv_handle.lock
 *
 * MPContext strictly speaking has no locks, and instead implicitly managed
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
};

struct observe_property {
    char *name;
    int64_t reply_id;
    mpv_format format;
    bool changed;           // property change should be signaled to user
    bool need_new_value;    // a new value should be retrieved
    bool updating;          // a new value is being retrieved
    bool dead;              // property unobserved while retrieving value
    bool value_valid;
    union m_option_value value;
    struct mpv_handle *client;
};

struct mpv_handle {
    // -- immmutable
    char *name;
    struct mp_log *log;
    struct MPContext *mpctx;
    struct mp_client_api *clients;

    // -- not thread-safe
    struct mpv_event *cur_event;
    struct mpv_event_property cur_property_event;

    pthread_mutex_t lock;
    pthread_cond_t wakeup;

    // -- protected by lock

    uint64_t event_mask;
    bool queued_wakeup;
    bool shutdown;
    bool choke_warning;
    void (*wakeup_cb)(void *d);
    void *wakeup_cb_ctx;
    int wakeup_pipe[2];

    mpv_event *events;      // ringbuffer of max_events entries
    int max_events;         // allocated number of entries in events
    int first_event;        // events[first_event] is the first readable event
    int num_events;         // number of readable events
    int reserved_events;    // number of entries reserved for replies

    struct observe_property **properties;
    int num_properties;
    int lowest_changed;
    int properties_updating;

    struct mp_log_buffer *messages;
    int messages_level;
};

static bool gen_property_change_event(struct mpv_handle *ctx);

void mp_clients_init(struct MPContext *mpctx)
{
    mpctx->clients = talloc_ptrtype(NULL, mpctx->clients);
    *mpctx->clients = (struct mp_client_api) {
        .mpctx = mpctx,
    };
    pthread_mutex_init(&mpctx->clients->lock, NULL);
}

void mp_clients_destroy(struct MPContext *mpctx)
{
    if (!mpctx->clients)
        return;
    assert(mpctx->clients->num_clients == 0);
    pthread_mutex_destroy(&mpctx->clients->lock);
    talloc_free(mpctx->clients);
    mpctx->clients = NULL;
}

int mp_clients_num(struct MPContext *mpctx)
{
    pthread_mutex_lock(&mpctx->clients->lock);
    int num_clients = mpctx->clients->num_clients;
    pthread_mutex_unlock(&mpctx->clients->lock);
    return num_clients;
}

static struct mpv_handle *find_client(struct mp_client_api *clients,
                                      const char *name)
{
    for (int n = 0; n < clients->num_clients; n++) {
        if (strcmp(clients->clients[n]->name, name) == 0)
            return clients->clients[n];
    }
    return NULL;
}

struct mpv_handle *mp_new_client(struct mp_client_api *clients, const char *name)
{
    pthread_mutex_lock(&clients->lock);

    char *unique_name = NULL;
    if (find_client(clients, name)) {
        for (int n = 2; n < 1000; n++) {
            unique_name = talloc_asprintf(NULL, "%s%d", name, n);
            if (!find_client(clients, unique_name))
                break;
            talloc_free(unique_name);
            unique_name = NULL;
        }
        if (!unique_name) {
            pthread_mutex_unlock(&clients->lock);
            return NULL;
        }
    }
    if (!unique_name)
        unique_name = talloc_strdup(NULL, name);

    int num_events = 1000;

    struct mpv_handle *client = talloc_ptrtype(NULL, client);
    *client = (struct mpv_handle){
        .name = talloc_steal(client, unique_name),
        .log = mp_log_new(client, clients->mpctx->log, unique_name),
        .mpctx = clients->mpctx,
        .clients = clients,
        .cur_event = talloc_zero(client, struct mpv_event),
        .events = talloc_array(client, mpv_event, num_events),
        .max_events = num_events,
        .event_mask = ((uint64_t)-1) & ~(1ULL << MPV_EVENT_TICK),
        .wakeup_pipe = {-1, -1},
    };
    pthread_mutex_init(&client->lock, NULL);
    pthread_cond_init(&client->wakeup, NULL);

    MP_TARRAY_APPEND(clients, clients->clients, clients->num_clients, client);

    pthread_mutex_unlock(&clients->lock);

    return client;
}

const char *mpv_client_name(mpv_handle *ctx)
{
    return ctx->name;
}

struct mp_log *mp_client_get_log(struct mpv_handle *ctx)
{
    return ctx->log;
}

static void wakeup_client(struct mpv_handle *ctx)
{
    pthread_cond_signal(&ctx->wakeup);
    if (ctx->wakeup_cb)
        ctx->wakeup_cb(ctx->wakeup_cb_ctx);
    if (ctx->wakeup_pipe[0] == -1)
        write(ctx->wakeup_pipe[0], &(char){0}, 1);
}

void mpv_set_wakeup_callback(mpv_handle *ctx, void (*cb)(void *d), void *d)
{
    pthread_mutex_lock(&ctx->lock);
    ctx->wakeup_cb = cb;
    ctx->wakeup_cb_ctx = d;
    pthread_mutex_unlock(&ctx->lock);
}

void mpv_suspend(mpv_handle *ctx)
{
    mp_dispatch_suspend(ctx->mpctx->dispatch);
}

void mpv_resume(mpv_handle *ctx)
{
    mp_dispatch_resume(ctx->mpctx->dispatch);
}

void mpv_destroy(mpv_handle *ctx)
{
    if (!ctx)
        return;

    pthread_mutex_lock(&ctx->lock);
    // reserved_events equals the number of asynchronous requests that weren't
    // yet replied. In order to avoid that trying to reply to a removed client
    // causes a crash, block until all asynchronous requests were served.
    ctx->event_mask = 0;
    while (ctx->reserved_events || ctx->properties_updating)
        pthread_cond_wait(&ctx->wakeup, &ctx->lock);
    pthread_mutex_unlock(&ctx->lock);

    struct mp_client_api *clients = ctx->clients;

    pthread_mutex_lock(&clients->lock);
    for (int n = 0; n < clients->num_clients; n++) {
        if (clients->clients[n] == ctx) {
            MP_TARRAY_REMOVE_AT(clients->clients, clients->num_clients, n);
            while (ctx->num_events) {
                talloc_free(ctx->events[ctx->first_event].data);
                ctx->first_event = (ctx->first_event + 1) % ctx->max_events;
                ctx->num_events--;
            }
            mp_msg_log_buffer_destroy(ctx->messages);
            pthread_cond_destroy(&ctx->wakeup);
            pthread_mutex_destroy(&ctx->lock);
            if (ctx->wakeup_pipe[0] != -1) {
                close(ctx->wakeup_pipe[0]);
                close(ctx->wakeup_pipe[1]);
            }
            talloc_free(ctx);
            ctx = NULL;
            // shutdown_clients() sleeps to avoid wasting CPU
            if (clients->mpctx->input)
                mp_input_wakeup(clients->mpctx->input);
            // TODO: make core quit if there are no clients
            break;
        }
    }
    pthread_mutex_unlock(&clients->lock);
    assert(!ctx);
}

mpv_handle *mpv_create(void)
{
    struct MPContext *mpctx = mp_create();
    mpv_handle *ctx = mp_new_client(mpctx->clients, "main");
    if (ctx) {
        // Set some defaults.
        mpv_set_option_string(ctx, "idle", "yes");
        mpv_set_option_string(ctx, "terminal", "no");
        mpv_set_option_string(ctx, "osc", "no");
        mpv_set_option_string(ctx, "input-default-bindings", "no");
    } else {
        mp_destroy(mpctx);
    }
    return ctx;
}

static void *playback_thread(void *p)
{
    struct MPContext *mpctx = p;

    pthread_detach(pthread_self());

    mp_play_files(mpctx);

    // This actually waits until all clients are gone before actually
    // destroying mpctx.
    mp_destroy(mpctx);

    return NULL;
}

int mpv_initialize(mpv_handle *ctx)
{
    if (mp_initialize(ctx->mpctx) < 0)
        return MPV_ERROR_INVALID_PARAMETER;

    pthread_t thread;
    if (pthread_create(&thread, NULL, playback_thread, ctx->mpctx) != 0)
        return MPV_ERROR_NOMEM;

    return 0;
}

// Reserve an entry in the ring buffer. This can be used to guarantee that the
// reply can be made, even if the buffer becomes congested _after_ sending
// the request.
// Returns an error code if the buffer is full.
static int reserve_reply(struct mpv_handle *ctx)
{
    int res = MPV_ERROR_EVENT_QUEUE_FULL;
    pthread_mutex_lock(&ctx->lock);
    if (ctx->reserved_events + ctx->num_events < ctx->max_events) {
        ctx->reserved_events++;
        res = 0;
    }
    pthread_mutex_unlock(&ctx->lock);
    return res;
}

static int append_event(struct mpv_handle *ctx, struct mpv_event *event)
{
    if (ctx->num_events + ctx->reserved_events >= ctx->max_events)
        return -1;
    ctx->events[(ctx->first_event + ctx->num_events) % ctx->max_events] = *event;
    ctx->num_events++;
    wakeup_client(ctx);
    return 0;
}

static int send_event(struct mpv_handle *ctx, struct mpv_event *event)
{
    pthread_mutex_lock(&ctx->lock);
    if (!(ctx->event_mask & (1ULL << event->event_id))) {
        pthread_mutex_unlock(&ctx->lock);
        return 0;
    }
    int r = append_event(ctx, event);
    if (r < 0 && !ctx->choke_warning) {
        mp_err(ctx->log, "Too many events queued.\n");
        ctx->choke_warning = true;
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
    if (append_event(ctx, event) < 0)
        abort();
    pthread_mutex_unlock(&ctx->lock);
}

static void status_reply(struct mpv_handle *ctx, int event,
                         uint64_t userdata, int status)
{
    struct mpv_event reply = {
        .event_id = event,
        .error = status,
    };
    send_reply(ctx, userdata, &reply);
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
    case MPV_EVENT_END_FILE:
        ev->data = talloc_memdup(NULL, ev->data, sizeof(mpv_event_end_file));
        break;
    default:
        // Doesn't use events with memory allocation.
        if (ev->data)
            abort();
    }
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
        dup_event_data(&event_data);
        send_event(clients->clients[n], &event_data);
    }

    pthread_mutex_unlock(&clients->lock);
}

int mp_client_send_event(struct MPContext *mpctx, const char *client_name,
                         int event, void *data)
{
    struct mp_client_api *clients = mpctx->clients;
    int r = 0;

    struct mpv_event event_data = {
        .event_id = event,
        .data = data,
    };

    pthread_mutex_lock(&clients->lock);

    struct mpv_handle *ctx = find_client(clients, client_name);
    if (ctx) {
        r = send_event(ctx, &event_data);
    } else {
        r = -1;
        talloc_free(data);
    }

    pthread_mutex_unlock(&clients->lock);

    return r;
}

int mpv_request_event(mpv_handle *ctx, mpv_event_id event, int enable)
{
    if (!mpv_event_name(event) || enable < 0 || enable > 1)
        return MPV_ERROR_INVALID_PARAMETER;
    pthread_mutex_lock(&ctx->lock);
    uint64_t bit = 1LLU << event;
    ctx->event_mask = enable ? ctx->event_mask | bit : ctx->event_mask & ~bit;
    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

mpv_event *mpv_wait_event(mpv_handle *ctx, double timeout)
{
    mpv_event *event = ctx->cur_event;

    struct timespec deadline = mpthread_get_deadline(timeout);

    pthread_mutex_lock(&ctx->lock);

    *event = (mpv_event){0};
    talloc_free_children(event);

    while (1) {
        if (ctx->num_events) {
            *event = ctx->events[ctx->first_event];
            ctx->first_event = (ctx->first_event + 1) % ctx->max_events;
            ctx->num_events--;
            talloc_steal(event, event->data);
            break;
        }
        if (gen_property_change_event(ctx))
            break;
        if (ctx->shutdown) {
            event->event_id = MPV_EVENT_SHUTDOWN;
            break;
        }
        if (ctx->messages) {
            // Poll the log message queue. Currently we can't/don't do better.
            struct mp_log_buffer_entry *msg =
                mp_msg_log_buffer_read(ctx->messages);
            if (msg) {
                event->event_id = MPV_EVENT_LOG_MESSAGE;
                struct mpv_event_log_message *cmsg = talloc_ptrtype(event, cmsg);
                *cmsg = (struct mpv_event_log_message){
                    .prefix = talloc_steal(event, msg->prefix),
                    .level = mp_log_levels[msg->level],
                    .text = talloc_steal(event, msg->text),
                };
                event->data = cmsg;
                talloc_free(msg);
                break;
            }
        }
        if (ctx->queued_wakeup)
            break;
        if (timeout <= 0)
            break;
        int r = pthread_cond_timedwait(&ctx->wakeup, &ctx->lock, &deadline);
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
    if (format < 0 || format >= MP_ARRAY_SIZE(type_conv))
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
    return false;
}

// Note: for MPV_FORMAT_NODE_MAP, this (incorrectly) takes the order into
//       account, instead of treating it as set.
static bool compare_value(void *a, void *b, mpv_format format)
{
    switch (format) {
    case MPV_FORMAT_NONE:
        return true;
    case MPV_FORMAT_STRING:
    case MPV_FORMAT_OSD_STRING:
        return strcmp(*(char **)a, *(char **)b) == 0;
    case MPV_FORMAT_FLAG:
        return *(int *)a == *(int *)b;
    case MPV_FORMAT_INT64:
        return *(int64_t *)a == *(int64_t *)b;
    case MPV_FORMAT_DOUBLE:
        return *(double *)a == *(double *)b;
    case MPV_FORMAT_NODE: {
        struct mpv_node *a_n = a, *b_n = b;
        if (a_n->format != b_n->format)
            return false;
        return compare_value(&a_n->u, &b_n->u, a_n->format);
    }
    case MPV_FORMAT_NODE_ARRAY:
    case MPV_FORMAT_NODE_MAP:
    {
        mpv_node_list *l_a = *(mpv_node_list **)a, *l_b = *(mpv_node_list **)b;
        if (l_a->num != l_b->num)
            return false;
        for (int n = 0; n < l_a->num; n++) {
            if (!compare_value(&l_a->values[n], &l_b->values[n], MPV_FORMAT_NODE))
                return false;
            if (format == MPV_FORMAT_NODE_MAP) {
                if (strcmp(l_a->keys[n], l_b->keys[n]) != 0)
                    return false;
            }
        }
        return true;
    }
    }
    abort();
}

void mpv_free_node_contents(mpv_node *node)
{
    static const struct m_option type = { .type = CONF_TYPE_NODE };
    m_option_free(&type, node);
}

int mpv_set_option(mpv_handle *ctx, const char *name, mpv_format format,
                   void *data)
{
    if (ctx->mpctx->initialized) {
        char prop[100];
        snprintf(prop, sizeof(prop), "options/%s", name);
        int err = mpv_set_property(ctx, prop, format, data);
        switch (err) {
        case MPV_ERROR_PROPERTY_UNAVAILABLE:
        case MPV_ERROR_PROPERTY_ERROR:
            return MPV_ERROR_OPTION_ERROR;
        case MPV_ERROR_PROPERTY_FORMAT:
            return MPV_ERROR_OPTION_FORMAT;
        case MPV_ERROR_PROPERTY_NOT_FOUND:
            return MPV_ERROR_OPTION_NOT_FOUND;
        default:
            return err;
        }
    } else {
        const struct m_option *type = get_mp_type(format);
        if (!type)
            return MPV_ERROR_OPTION_FORMAT;
        struct mpv_node tmp;
        if (format != MPV_FORMAT_NODE) {
            tmp.format = format;
            memcpy(&tmp.u, data, type->type->size);
            format = MPV_FORMAT_NODE;
            data = &tmp;
        }
        int err = m_config_set_option_node(ctx->mpctx->mconfig, bstr0(name),
                                           data);
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
}

int mpv_set_option_string(mpv_handle *ctx, const char *name, const char *data)
{
    return mpv_set_option(ctx, name, MPV_FORMAT_STRING, &data);
}

// Run a command in the playback thread.
// Note: once some things are fixed (like vo_opengl not being safe to be
//       called from any thread other than the playback thread), this can
//       be replaced by a simpler method.
static void run_locked(mpv_handle *ctx, void (*fn)(void *fn_data), void *fn_data)
{
    mp_dispatch_run(ctx->mpctx->dispatch, fn, fn_data);
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
    mp_dispatch_enqueue_autofree(ctx->mpctx->dispatch, fn, fn_data);
    return 0;
}

struct cmd_request {
    struct MPContext *mpctx;
    struct mp_cmd *cmd;
    int status;
    struct mpv_handle *reply_ctx;
    uint64_t userdata;
};

static void cmd_fn(void *data)
{
    struct cmd_request *req = data;
    run_command(req->mpctx, req->cmd);
    req->status = 0;
    talloc_free(req->cmd);
    if (req->reply_ctx) {
        status_reply(req->reply_ctx, MPV_EVENT_COMMAND_REPLY,
                     req->userdata, req->status);
    }
}

static int run_client_command(mpv_handle *ctx, struct mp_cmd *cmd)
{
    if (!ctx->mpctx->initialized)
        return MPV_ERROR_UNINITIALIZED;
    if (!cmd)
        return MPV_ERROR_INVALID_PARAMETER;

    struct cmd_request req = {
        .mpctx = ctx->mpctx,
        .cmd = cmd,
    };
    run_locked(ctx, cmd_fn, &req);
    return req.status;
}

int mpv_command(mpv_handle *ctx, const char **args)
{
    return run_client_command(ctx, mp_input_parse_cmd_strv(ctx->log, 0, args,
                                                           ctx->name));
}

int mpv_command_string(mpv_handle *ctx, const char *args)
{
    return run_client_command(ctx,
        mp_input_parse_cmd(ctx->mpctx->input, bstr0((char*)args), ctx->name));
}

int mpv_command_async(mpv_handle *ctx, uint64_t ud, const char **args)
{
    if (!ctx->mpctx->initialized)
        return MPV_ERROR_UNINITIALIZED;

    struct mp_cmd *cmd = mp_input_parse_cmd_strv(ctx->log, 0, args, "<client>");
    if (!cmd)
        return MPV_ERROR_INVALID_PARAMETER;

    struct cmd_request *req = talloc_ptrtype(NULL, req);
    *req = (struct cmd_request){
        .mpctx = ctx->mpctx,
        .cmd = cmd,
        .reply_ctx = ctx,
        .userdata = ud,
    };
    return run_async(ctx, cmd_fn, req);
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

    int err;
    switch (req->format) {
    case MPV_FORMAT_STRING: {
        // Go through explicit string conversion. M_PROPERTY_SET_NODE doesn't
        // do this, because it tries to be somewhat type-strict. But the client
        // needs a way to set everything by string.
        char *s = *(char **)req->data;
        err = mp_property_do(req->name, M_PROPERTY_SET_STRING, s, req->mpctx);
        break;
    }
    case MPV_FORMAT_NODE:
    case MPV_FORMAT_FLAG:
    case MPV_FORMAT_INT64:
    case MPV_FORMAT_DOUBLE: {
        struct mpv_node node;
        if (req->format == MPV_FORMAT_NODE) {
            node = *(struct mpv_node *)req->data;
        } else {
            // These are basically emulated via mpv_node.
            node.format = req->format;
            memcpy(&node.u, req->data, type->type->size);
        }
        err = mp_property_do(req->name, M_PROPERTY_SET_NODE, &node, req->mpctx);
        break;
    }
    default:
        abort();
    }

    req->status = translate_property_error(err);

    if (req->reply_ctx) {
        status_reply(req->reply_ctx, MPV_EVENT_SET_PROPERTY_REPLY,
                     req->userdata, req->status);
    }
}

int mpv_set_property(mpv_handle *ctx, const char *name, mpv_format format,
                     void *data)
{
    if (!ctx->mpctx->initialized)
        return MPV_ERROR_UNINITIALIZED;
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
            *(char **)req->data = s;
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
            .reply_userdata = req->userdata,
        };
        send_reply(req->reply_ctx, req->userdata, &reply);
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
    const struct m_option *type = get_mp_type_get(prop->format);
    if (type)
        m_option_free(type, &prop->value);
}

int mpv_observe_property(mpv_handle *ctx, uint64_t userdata,
                         const char *name, mpv_format format)
{
    if (format != MPV_FORMAT_NONE && !get_mp_type_get(format))
        return MPV_ERROR_PROPERTY_FORMAT;
    // Explicitly disallow this, because it would require a special code path.
    if (format == MPV_FORMAT_OSD_STRING)
        return MPV_ERROR_PROPERTY_FORMAT;

    pthread_mutex_lock(&ctx->lock);
    struct observe_property *prop = talloc_ptrtype(ctx, prop);
    talloc_set_destructor(prop, property_free);
    *prop = (struct observe_property){
        .client = ctx,
        .name = talloc_strdup(prop, name),
        .reply_id = userdata,
        .format = format,
        .changed = true,
        .need_new_value = true,
    };
    MP_TARRAY_APPEND(ctx, ctx->properties, ctx->num_properties, prop);
    ctx->lowest_changed = 0;
    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

int mpv_unobserve_property(mpv_handle *ctx, uint64_t userdata)
{
    pthread_mutex_lock(&ctx->lock);
    int count = 0;
    for (int n = ctx->num_properties - 1; n >= 0; n--) {
        struct observe_property *prop = ctx->properties[n];
        if (prop->reply_id == userdata) {
            if (prop->updating) {
                prop->dead = true;
            } else {
                // In case mpv_unobserve_property() is called after mpv_wait_event()
                // returned, and the mpv_event still references the name somehow,
                // make sure it's not freed while in use. The same can happen
                // with the value update mechanism.
                talloc_steal(ctx->cur_event, prop);
            }
            MP_TARRAY_REMOVE_AT(ctx->properties, ctx->num_properties, n);
            count++;
        }
    }
    ctx->lowest_changed = 0;
    pthread_mutex_unlock(&ctx->lock);
    return count;
}

static int prefix_len(const char *p)
{
    const char *end = strchr(p, '/');
    return end ? end - p : strlen(p);
}

static bool match_property(const char *a, const char *b)
{
    if (strcmp(b, "*") == 0)
        return true;
    int len_a = prefix_len(a);
    int len_b = prefix_len(b);
    return strncmp(a, b, MPMIN(len_a, len_b)) == 0;
}

// Broadcast that properties have changed.
void mp_client_property_change(struct MPContext *mpctx, const char **list)
{
    struct mp_client_api *clients = mpctx->clients;

    pthread_mutex_lock(&clients->lock);

    for (int n = 0; n < clients->num_clients; n++) {
        struct mpv_handle *client = clients->clients[n];
        pthread_mutex_lock(&client->lock);

        client->lowest_changed = client->num_properties;
        for (int i = 0; i < client->num_properties; i++) {
            struct observe_property *prop = client->properties[i];
            if (!prop->changed && !prop->need_new_value) {
                for (int x = 0; list && list[x]; x++) {
                    if (match_property(prop->name, list[x])) {
                        prop->changed = prop->need_new_value = true;
                        break;
                    }
                }
            }
            if ((prop->changed || prop->updating) && i < client->lowest_changed)
                client->lowest_changed = i;
        }
        if (client->lowest_changed < client->num_properties)
            wakeup_client(client);
        pthread_mutex_unlock(&client->lock);
    }

    pthread_mutex_unlock(&clients->lock);
}

static void update_prop(void *p)
{
    struct observe_property *prop = p;
    struct mpv_handle *ctx = prop->client;

    const struct m_option *type = get_mp_type_get(prop->format);
    union m_option_value val = {0};

    struct getproperty_request req = {
        .mpctx = ctx->mpctx,
        .name = prop->name,
        .format = prop->format,
        .data = &val,
    };

    getproperty_fn(&req);

    pthread_mutex_lock(&ctx->lock);
    ctx->properties_updating--;
    prop->updating = false;
    bool new_value_valid = req.status >= 0;
    if (prop->value_valid != new_value_valid) {
        prop->changed = true;
    } else if (prop->value_valid && new_value_valid) {
        if (!compare_value(&prop->value, &val, prop->format))
            prop->changed = true;
    }
    m_option_free(type, &prop->value);
    if (new_value_valid)
        memcpy(&prop->value, &val, type->type->size);
    prop->value_valid = new_value_valid;
    if (prop->dead)
        talloc_steal(ctx->cur_event, prop);
    wakeup_client(ctx);
    pthread_mutex_unlock(&ctx->lock);
}

// Set ctx->cur_event to a generated property change event, if there is any
// outstanding property.
static bool gen_property_change_event(struct mpv_handle *ctx)
{
    int start = ctx->lowest_changed;
    ctx->lowest_changed = ctx->num_properties;
    for (int n = start; n < ctx->num_properties; n++) {
        struct observe_property *prop = ctx->properties[n];
        if ((prop->changed || prop->updating) && n < ctx->lowest_changed)
            ctx->lowest_changed = n;
        if (prop->changed) {
            bool new_val = prop->need_new_value;
            prop->changed = prop->need_new_value = false;
            if (prop->format && new_val) {
                ctx->properties_updating++;
                prop->updating = true;
                mp_dispatch_enqueue(ctx->mpctx->dispatch, update_prop, prop);
            } else {
                ctx->cur_property_event = (struct mpv_event_property){
                    .name = prop->name,
                    .format = prop->value_valid ? prop->format : 0,
                };
                if (prop->value_valid)
                    ctx->cur_property_event.data = &prop->value;
                *ctx->cur_event = (struct mpv_event){
                    .event_id = MPV_EVENT_PROPERTY_CHANGE,
                    .reply_userdata = prop->reply_id,
                    .data = &ctx->cur_property_event,
                };
                return true;
            }
        }
    }
    return false;
}

int mpv_request_log_messages(mpv_handle *ctx, const char *min_level)
{
    int level = -1;
    for (int n = 0; n < MSGL_MAX + 1; n++) {
        if (mp_log_levels[n] && strcmp(min_level, mp_log_levels[n]) == 0) {
            level = n;
            break;
        }
    }
    if (level < 0 && strcmp(min_level, "no") != 0)
        return MPV_ERROR_INVALID_PARAMETER;

    pthread_mutex_lock(&ctx->lock);

    if (!ctx->messages)
        ctx->messages_level = -1;

    if (ctx->messages_level != level) {
        mp_msg_log_buffer_destroy(ctx->messages);
        ctx->messages = NULL;
        if (level >= 0) {
            ctx->messages =
                mp_msg_log_buffer_new(ctx->mpctx->global, 1000, level);
        }
        ctx->messages_level = level;
    }

    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

int mpv_get_wakeup_pipe(mpv_handle *ctx)
{
    pthread_mutex_lock(&ctx->lock);
#if defined(F_SETFL)
    if (ctx->wakeup_pipe[0] == -1) {
        if (pipe(ctx->wakeup_pipe) != 0)
            goto fail;
        for (int i = 0; i < 2; i++) {
            mp_set_cloexec(ctx->wakeup_pipe[i]);
            int ret = fcntl(ctx->wakeup_pipe[i], F_GETFL);
            fcntl(ctx->wakeup_pipe[i], F_SETFL, ret | O_NONBLOCK);
        }
    }
fail:
#endif
    pthread_mutex_unlock(&ctx->lock);
    return ctx->wakeup_pipe[1];
}

unsigned long mpv_client_api_version(void)
{
    return MPV_CLIENT_API_VERSION;
}

static const char *err_table[] = {
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

static const char *event_table[] = {
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
};

const char *mpv_event_name(mpv_event_id event)
{
    if (event < 0 || event >= MP_ARRAY_SIZE(event_table))
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
