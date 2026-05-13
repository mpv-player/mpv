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

#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include <curl/curl.h>

#include <libavformat/avio.h>
#include <libavformat/version.h>
#include <libavutil/avstring.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>

#include "stream.h"
#include "stream_curl.h"

#include "common/common.h"
#include "common/global.h"
#include "common/msg.h"
#include "cookies.h"
#include "demux/demux.h"
#include "misc/bstr.h"
#include "misc/dispatch.h"
#include "misc/path_utils.h"
#include "misc/thread_tools.h"
#include "mpv_talloc.h"
#include "network.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "options/path.h"
#include "osdep/threads.h"
#include "osdep/timer.h"

enum curl_proto {
    MP_CURL_PROTO_HTTP,
    MP_CURL_PROTO_FTP,
};

struct curl_scheme {
    bstr scheme;
    enum curl_proto proto;
};

static const struct curl_scheme curl_schemes[] = {
    {bstr0_lit("http"), MP_CURL_PROTO_HTTP},
    {bstr0_lit("https"), MP_CURL_PROTO_HTTP},
    {bstr0_lit("ftp"), MP_CURL_PROTO_FTP},
    {bstr0_lit("ftps"), MP_CURL_PROTO_FTP},
};

struct curl_opts {
    bool enabled;
    int http_version;
    int max_redirects;
    int max_retries;
    double connect_timeout;
    int64_t buffer_size;
    int64_t max_request_size;
};

#ifndef CURL_HTTP_VERSION_3
#define CURL_HTTP_VERSION_3 CURL_HTTP_VERSION_NONE
#endif

#ifndef CURL_HTTP_VERSION_3ONLY
#define CURL_HTTP_VERSION_3ONLY CURL_HTTP_VERSION_NONE
#endif

#define OPT_BASE_STRUCT struct curl_opts
const struct m_sub_options curl_conf = {
    .opts = (const struct m_option[]) {
        {"enabled", OPT_BOOL(enabled)},
        {"http-version", OPT_CHOICE(http_version,
            {"auto", CURL_HTTP_VERSION_NONE},
            {"1.0", CURL_HTTP_VERSION_1_0},
            {"1.1", CURL_HTTP_VERSION_1_1},
            {"2", CURL_HTTP_VERSION_2},
            {"2tls", CURL_HTTP_VERSION_2TLS},
            {"2-prior-knowledge", CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE},
            {"3", CURL_HTTP_VERSION_3},
            {"3only", CURL_HTTP_VERSION_3ONLY}
            )},
        {"max-redirects", OPT_INT(max_redirects), M_RANGE(0, 100)},
        {"max-retries", OPT_INT(max_retries), M_RANGE(0, 100)},
        {"connect-timeout", OPT_DOUBLE(connect_timeout), M_RANGE(0, 600)},
        {"buffer-size", OPT_BYTE_SIZE(buffer_size),
            M_RANGE(2 * CURL_MAX_WRITE_SIZE, M_MAX_MEM_BYTES)},
        {"max-request-size", OPT_BYTE_SIZE(max_request_size),
            M_RANGE(0, M_MAX_MEM_BYTES)},
        {0}
    },
    .defaults = &(const struct curl_opts) {
        // Older lavf has a bug with nested IO cleanup, disable by default.
        // <https://code.ffmpeg.org/FFmpeg/FFmpeg/pulls/23082>
        .enabled = LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(62, 15, 101),
        .http_version = CURL_HTTP_VERSION_NONE,
        .max_redirects = 16,
        .max_retries = 5,
        .connect_timeout = 30,
        .buffer_size = 4 << 20, // 4 MiB
        .max_request_size = 0,
    },
    .size = sizeof(struct curl_opts),
};

static const struct curl_scheme *curl_scheme_lookup(bstr url)
{
    bstr scheme = mp_split_proto(url, NULL);
    for (int i = 0; i < MP_ARRAY_SIZE(curl_schemes); i++) {
        if (bstrcasecmp(scheme, curl_schemes[i].scheme) == 0)
            return &curl_schemes[i];
    }
    return NULL;
}

struct curl_ctx {
    mp_thread thread;
    struct mp_dispatch_queue *dispatch;
    CURLM *multi;
    bool exit;
};

// Per-stream state, owned by the curl thread.
struct priv {
    struct mp_log *log;
    struct mpv_global *global;
    struct curl_ctx *ctx;
    struct stream *s;

    struct curl_opts *opts;
    struct mp_network_opts *net_opts;

    CURL *curl;
    struct curl_slist *headers;
    char *url;
    const struct curl_scheme *scheme;

    // Stream parameters
    bool seekable;
    int64_t content_size; // -1 if unknown

    // Producer state. Only touched by the curl thread.
    uint64_t request_start;    // absolute byte position of next request
    uint64_t request_received; // bytes received in the current request
    int retry_count;           // consecutive failed attempts at request_start
    bool active;               // handle is currently active in the multi
    bool finished;             // current request has reached EOF

    // Probe state. Set on the curl thread read by curl_open after.
    bool probed;
    bool stream_ok;

    // Shared state, protected by mtx.
    mp_mutex mtx;
    mp_cond cond;
    uint8_t *buffer;
    size_t buffer_size;
    size_t head, tail, count;
    bool paused;         // write callback paused due to a full buffer
    bool stream_eof;     // producer has delivered all data
    bool stream_error;   // unrecoverable error
    atomic_bool aborted; // canceled by user (mp_cancel)
};

// Curl thread

enum cmd_kind {
    CMD_ADD,
    CMD_REMOVE,
    CMD_SEEK,
    CMD_UNPAUSE,
    CMD_EXIT,
};

struct cmd {
    enum cmd_kind kind;
    struct curl_ctx *ctx;
    struct priv *p;
    int64_t pos;
    bool drop;
};

static void start_request(struct priv *p);
static void on_done(struct priv *p, CURLcode code);

static void run_cmd(void *arg)
{
    struct cmd *c = arg;
    struct curl_ctx *ctx = c->ctx;
    switch (c->kind) {
    case CMD_ADD:
        MP_TRACE(c->p, "starting curl request at %" PRIu64 "\n", c->p->request_start);
        start_request(c->p);
        break;
    case CMD_REMOVE:
        if (c->p->active) {
            MP_TRACE(c->p, "removing curl handle\n");
            curl_multi_remove_handle(ctx->multi, c->p->curl);
            c->p->active = false;
        }
        break;
    case CMD_UNPAUSE:
        // The consumer freed enough buffer space. Clear the pause flag and
        // resume the transfer.
        mp_mutex_lock(&c->p->mtx);
        MP_TRACE(c->p, "resuming curl transfer\n");
        c->p->paused = false;
        mp_mutex_unlock(&c->p->mtx);
        curl_easy_pause(c->p->curl, CURLPAUSE_CONT);
        break;
    case CMD_SEEK:
        MP_TRACE(c->p, "seeking to %" PRIu64 "\n", c->pos);
        if (c->p->active) {
            curl_multi_remove_handle(ctx->multi, c->p->curl);
            c->p->active = false;
        }
        mp_mutex_lock(&c->p->mtx);
        if (c->drop)
            c->p->head = c->p->tail = c->p->count = 0;
        c->p->paused = false;
        c->p->stream_eof = false;
        c->p->stream_error = false;
        mp_cond_broadcast(&c->p->cond);
        mp_mutex_unlock(&c->p->mtx);
        c->p->request_start = c->pos;
        c->p->request_received = 0;
        c->p->retry_count = 0;
        c->p->finished = false;
        start_request(c->p);
        break;
    case CMD_EXIT:
        ctx->exit = true;
        break;
    }
}

static void cmd_async(struct priv *p, enum cmd_kind kind)
{
    struct cmd *c = talloc_zero(NULL, struct cmd);
    c->kind = kind;
    c->ctx = p->ctx;
    c->p = p;
    mp_dispatch_enqueue_autofree(p->ctx->dispatch, run_cmd, c);
}

static void cmd_sync(struct priv *p, enum cmd_kind kind, int64_t pos, bool drop)
{
    struct cmd c = {
        .kind = kind,
        .ctx = p->ctx,
        .p = p,
        .pos = pos,
        .drop = drop,
    };
    mp_dispatch_run(p->ctx->dispatch, run_cmd, &c);
}

static void curl_wakeup(void *arg)
{
    struct curl_ctx *ctx = arg;
    curl_multi_wakeup(ctx->multi);
}

static MP_THREAD_VOID curl_thread(void *arg)
{
    mp_thread_set_name("curl");
    struct curl_ctx *ctx = arg;

    curl_global_init(CURL_GLOBAL_ALL);
    ctx->multi = curl_multi_init();
    curl_multi_setopt(ctx->multi, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);

    mp_dispatch_set_wakeup_fn(ctx->dispatch, curl_wakeup, ctx);

    while (!ctx->exit) {
        mp_dispatch_queue_process(ctx->dispatch, 0);

        // Stop early to avoid delays, this happens only when player is closing.
        if (ctx->exit)
            break;

        int running = 0;
        CURLMcode mres = curl_multi_perform(ctx->multi, &running);
        if (mres != CURLM_OK && mres != CURLM_CALL_MULTI_PERFORM)
            break;

        CURLMsg *msg;
        int left = 0;
        while ((msg = curl_multi_info_read(ctx->multi, &left))) {
            if (msg->msg != CURLMSG_DONE)
                continue;
            struct priv *p = NULL;
            curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &p);
            mp_assert(p);
            curl_multi_remove_handle(ctx->multi, msg->easy_handle);
            p->active = false;
            on_done(p, msg->data.result);
        }

        curl_multi_poll(ctx->multi, NULL, 0, 1000, NULL);
    }

    curl_multi_cleanup(ctx->multi);
    curl_global_cleanup();
    MP_THREAD_RETURN();
}

static void mp_curl_destroy(void *ptr)
{
    struct curl_ctx *ctx = ptr;
    struct cmd c = { .kind = CMD_EXIT, .ctx = ctx };
    mp_dispatch_run(ctx->dispatch, run_cmd, &c);
    mp_thread_join(ctx->thread);
}

void mp_curl_global_init(struct mpv_global *global)
{
    struct curl_ctx *ctx = talloc_zero(global, struct curl_ctx);
    talloc_set_destructor(ctx, mp_curl_destroy);
    ctx->dispatch = mp_dispatch_create(ctx);
    global->curl = ctx;
    mp_require(!mp_thread_create(&ctx->thread, curl_thread, ctx));
}

// Curl callbacks

static bool is_http_success(long resp)
{
    return resp >= 200 && resp < 300;
}

// Called per chunk of body data.
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct priv *p = userdata;
    size_t bytes = size * nmemb;

    // header_callback validated the response and logged any error status,
    // we don't care about error body.
    if (!p->stream_ok)
        return CURL_WRITEFUNC_ERROR;

    if (atomic_load_explicit(&p->aborted, memory_order_relaxed))
        return CURL_WRITEFUNC_ERROR;

    mp_mutex_lock(&p->mtx);

    if (p->buffer_size - p->count < bytes) {
        // No room in the buffer. Pause the transfer and wait for the consumer.
        p->paused = true;
        mp_mutex_unlock(&p->mtx);
        MP_TRACE(p, "pausing curl transfer, buffer full (%zu bytes)\n", p->count);
        return CURL_WRITEFUNC_PAUSE;
    }

    size_t tail_chunk = MPMIN(p->buffer_size - p->tail, bytes);
    memcpy(p->buffer + p->tail, ptr, tail_chunk);
    memcpy(p->buffer, ptr + tail_chunk, bytes - tail_chunk);
    p->tail = (p->tail + bytes) % p->buffer_size;
    p->count += bytes;
    p->paused = false;

    p->request_received += bytes;

    mp_cond_broadcast(&p->cond);
    mp_mutex_unlock(&p->mtx);

    return bytes;
}

static int xferinfo_callback(void *userdata, curl_off_t dl_total, curl_off_t dl_now,
                             curl_off_t ul_total, curl_off_t ul_now)
{
    struct priv *p = userdata;
    return atomic_load_explicit(&p->aborted, memory_order_relaxed);
}

static int64_t parse_content_range_total(const char *value)
{
    if (!value)
        return -1;
    bstr after;
    if (!bstr_split_tok(bstr0(value), "/", &(bstr){0}, &after))
        return -1;
    bstr rest;
    long long total = bstrtoll(after, &rest, 10);
    return (rest.len == 0 && total > 0) ? (int64_t)total : -1;
}

static const char *header_value(CURL *c, const char *name)
{
    struct curl_header *h = NULL;
    if (curl_easy_header(c, name, 0, CURLH_HEADER, -1, &h) == CURLHE_OK)
        return h->value;
    return NULL;
}

static void finalize_probe(struct priv *p)
{
    if (mp_msg_test(p->log, MSGL_DEBUG)) {
        long resp = 0;
        char *ctype = NULL;
        curl_easy_getinfo(p->curl, CURLINFO_RESPONSE_CODE, &resp);
        curl_easy_getinfo(p->curl, CURLINFO_CONTENT_TYPE, &ctype);
        MP_DBG(p, "proto=%.*s ok=%d code=%ld size=%" PRId64 " seekable=%d type=%s\n",
               BSTR_P(p->scheme->scheme), p->stream_ok, resp,
               p->content_size, p->seekable, ctype ? ctype : "-");
    }

    mp_mutex_lock(&p->mtx);
    p->probed = true;
    mp_cond_broadcast(&p->cond);
    mp_mutex_unlock(&p->mtx);
}

// Empty line is the end of the header. Skip intermediate 1xx and 3xx responses,
// we care about the final one.
static void probe_http(struct priv *p, struct bstr line)
{
    if (line.len > 0)
        return;

    long resp = 0;
    curl_easy_getinfo(p->curl, CURLINFO_RESPONSE_CODE, &resp);
    if (resp < 200 || (resp >= 300 && resp < 400))
        return;

    if (!is_http_success(resp)) {
        MP_ERR(p, "HTTP error %ld\n", resp);
        goto done;
    }

    // Compressed responses are byte-addressed in the encoded representation,
    // which our caller can't translate, so they are non-seekable.
    const char *ce = header_value(p->curl, "Content-Encoding");
    bool compressed = ce && ce[0] && strcasecmp(ce, "identity") != 0;
    const char *ar = header_value(p->curl, "Accept-Ranges");
    bool accept_ranges = ar && strcasecmp(ar, "bytes") == 0;

    // Some servers reply 200 to an open-ended "Range: 0-" but 206 to explicit
    // byte ranges, so trust either.
    p->seekable = !compressed && (resp == 206 || accept_ranges);

    if (p->seekable) {
        // Content-Range carries the full size on a partial response. On any
        // non-206 success code use Content-Length.
        int64_t total = parse_content_range_total(header_value(p->curl, "Content-Range"));
        if (total < 0 && resp != 206) {
            curl_off_t cl = -1;
            if (curl_easy_getinfo(p->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T,
                                  &cl) == CURLE_OK && cl >= 0)
                total = cl;
        }
        p->content_size = total;
    }
    p->stream_ok = true;
done:
    finalize_probe(p);
}

static void probe_ftp(struct priv *p, struct bstr line)
{
    if (line.len < 4 || line.start[3] != ' ')
        return;
    // Parse the line directly: libcurl only stamps CURLINFO_RESPONSE_CODE after
    // a reply is fully processed, so polling it from header_callback returns
    // the previous code.
    struct bstr code = {line.start, 3};
    if (!bstr_equals0(code, "150") && !bstr_equals0(code, "125"))
        return;

    curl_off_t cl = -1;
    if (curl_easy_getinfo(p->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T,
                          &cl) == CURLE_OK && cl >= 0)
        p->content_size = cl;

    p->seekable = p->content_size > 0;
    p->stream_ok = true;
    finalize_probe(p);
}

// Called per header line.
static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    struct priv *p = userdata;
    size_t bytes = size * nitems;

    if (p->probed)
        return bytes;

    struct bstr line = bstr_strip_linebreaks((bstr){buffer, bytes});
    switch (p->scheme->proto) {
    case MP_CURL_PROTO_HTTP:
        probe_http(p, line);
        break;
    case MP_CURL_PROTO_FTP:
        probe_ftp(p, line);
        break;
    default:
        break;
    }

    return bytes;
}

static int debug_callback(CURL *handle, curl_infotype type, char *data, size_t size,
                          void *userdata)
{
    struct priv *p = userdata;
    const char *prefix;
    switch (type) {
    case CURLINFO_TEXT:       prefix = "* "; break;
    case CURLINFO_HEADER_IN:  prefix = "< "; break;
    case CURLINFO_HEADER_OUT: prefix = "> "; break;
    default:                  return 0;
    }
    bstr msg = bstr_strip_linebreaks((bstr){data, size});
    MP_TRACE(p, "%s%.*s\n", prefix, BSTR_P(msg));
    return 0;
}

// Request handling

static bool is_recoverable_error(CURLcode code)
{
    switch (code) {
    case CURLE_RECV_ERROR:
    case CURLE_SEND_ERROR:
    case CURLE_PARTIAL_FILE:
    case CURLE_OPERATION_TIMEDOUT:
    case CURLE_GOT_NOTHING:
    case CURLE_COULDNT_CONNECT:
    case CURLE_COULDNT_RESOLVE_HOST:
    case CURLE_HTTP2:
    case CURLE_HTTP2_STREAM:
        return true;
    default:
        return false;
    }
}

static void start_request(struct priv *p)
{
    if (p->finished) {
        mp_mutex_lock(&p->mtx);
        p->stream_eof = true;
        mp_cond_broadcast(&p->cond);
        mp_mutex_unlock(&p->mtx);
        return;
    }

    uint64_t start = p->request_start;

    bool ranged = !p->probed || p->seekable;
    bool chunked = ranged && p->opts->max_request_size > 0;

    if (p->seekable && p->content_size > 0 && start >= p->content_size) {
        p->finished = true;
        mp_mutex_lock(&p->mtx);
        p->stream_eof = true;
        mp_cond_broadcast(&p->cond);
        mp_mutex_unlock(&p->mtx);
        return;
    }

    char range[64];
    if (chunked) {
        uint64_t end = start + p->opts->max_request_size - 1;
        if (p->content_size > 0)
            end = MPMIN(end, p->content_size - 1);
        snprintf(range, sizeof(range), "%" PRIu64 "-%" PRIu64, start, end);
        curl_easy_setopt(p->curl, CURLOPT_RANGE, range);
    } else if (ranged) {
        snprintf(range, sizeof(range), "%" PRIu64 "-", start);
        curl_easy_setopt(p->curl, CURLOPT_RANGE, range);
    } else {
        curl_easy_setopt(p->curl, CURLOPT_RANGE, NULL);
    }

    p->request_received = 0;
    p->active = true;
    curl_multi_add_handle(p->ctx->multi, p->curl);
}

static void on_done(struct priv *p, CURLcode code)
{
    bool aborted = atomic_load_explicit(&p->aborted, memory_order_relaxed);

    if (!p->probed) {
        // Connection died before any headers arrived.
        if (code != CURLE_OK && !aborted)
            MP_ERR(p, "error: %s\n", curl_easy_strerror(code));
        mp_mutex_lock(&p->mtx);
        p->probed = true;
        mp_cond_broadcast(&p->cond);
        mp_mutex_unlock(&p->mtx);
        return;
    }

    // Roll completed bytes into request_start so retries and chunk
    // continuations resume at the next missing byte.
    p->request_start += p->request_received;
    p->request_received = 0;

    if (code == CURLE_OK && !aborted) {
        p->retry_count = 0;

        bool chunked = p->seekable && p->opts->max_request_size > 0;
        if (chunked && (p->content_size <= 0 || p->request_start < p->content_size)) {
            start_request(p);
            return;
        }

        p->finished = true;
        mp_mutex_lock(&p->mtx);
        p->stream_eof = true;
        mp_cond_broadcast(&p->cond);
        mp_mutex_unlock(&p->mtx);
        return;
    }

    // Try to recover if the stream is seekable and the failure looks
    // recoverable.
    bool recoverable = !aborted && p->seekable &&
                       is_recoverable_error(code) &&
                       p->retry_count < p->opts->max_retries;
    if (recoverable) {
        p->retry_count++;
        MP_WARN(p, "%s, retrying (#%d) from %" PRIu64 "\n",
                curl_easy_strerror(code), p->retry_count, p->request_start);
        start_request(p);
        return;
    }

    if (!aborted)
        MP_ERR(p, "transfer failed: %s\n", curl_easy_strerror(code));

    mp_mutex_lock(&p->mtx);
    p->stream_error = true;
    mp_cond_broadcast(&p->cond);
    mp_mutex_unlock(&p->mtx);
}

static void on_cancel(void *ctx)
{
    struct priv *p = ctx;
    atomic_store(&p->aborted, true);
    mp_mutex_lock(&p->mtx);
    mp_cond_broadcast(&p->cond);
    mp_mutex_unlock(&p->mtx);
    if (p->ctx && p->ctx->multi)
        curl_multi_wakeup(p->ctx->multi);
}

// Configuration and initial setup

static struct curl_slist *build_header_list(struct priv *p)
{
    struct curl_slist *list = NULL;
    if (p->net_opts->referrer && p->net_opts->referrer[0]) {
        char *h = talloc_asprintf(NULL, "Referer: %s", p->net_opts->referrer);
        list = curl_slist_append(list, h);
        talloc_free(h);
    }
    if (p->net_opts->http_header_fields) {
        for (int i = 0; p->net_opts->http_header_fields[i]; i++)
            list = curl_slist_append(list, p->net_opts->http_header_fields[i]);
    }
    return list;
}

static void setup_curl(struct priv *p)
{
    CURL *c = p->curl;

    curl_easy_setopt(c, CURLOPT_URL, p->url);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_PRIVATE, p);

    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, p);
    curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(c, CURLOPT_HEADERDATA, p);
    // enable progress callback, so we can cancel transfer at any point
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, xferinfo_callback);
    curl_easy_setopt(c, CURLOPT_XFERINFODATA, p);

    // Enable verbose output with trace level logging.
    curl_easy_setopt(c, CURLOPT_VERBOSE, mp_msg_test(p->log, MSGL_TRACE) ? 1L : 0L);
    curl_easy_setopt(c, CURLOPT_DEBUGFUNCTION, debug_callback);
    curl_easy_setopt(c, CURLOPT_DEBUGDATA, p);

    curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS, (long)p->opts->max_redirects);
    curl_easy_setopt(c, CURLOPT_HTTP_VERSION, (long)p->opts->http_version);
    curl_easy_setopt(c, CURLOPT_HSTS_CTRL, (long)CURLHSTS_ENABLE);
    curl_easy_setopt(c, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT_MS,
                     (long)(p->opts->connect_timeout * 1000));

    if (p->net_opts->useragent && p->net_opts->useragent[0])
        curl_easy_setopt(c, CURLOPT_USERAGENT, p->net_opts->useragent);
    if (p->net_opts->http_proxy && p->net_opts->http_proxy[0])
        curl_easy_setopt(c, CURLOPT_PROXY, p->net_opts->http_proxy);

    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, p->net_opts->tls_verify ? 1L : 0L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, p->net_opts->tls_verify ? 2L : 0L);
    if (p->net_opts->tls_ca_file) {
        char *path = mp_get_user_path(p, p->global, p->net_opts->tls_ca_file);
        curl_easy_setopt(c, CURLOPT_CAINFO, path);
    }
    if (p->net_opts->tls_cert_file) {
        char *path = mp_get_user_path(p, p->global, p->net_opts->tls_cert_file);
        curl_easy_setopt(c, CURLOPT_SSLCERT, path);
    }
    if (p->net_opts->tls_key_file) {
        char *path = mp_get_user_path(p, p->global, p->net_opts->tls_key_file);
        curl_easy_setopt(c, CURLOPT_SSLKEY, path);
    }

    if (p->net_opts->cookies_enabled) {
        curl_easy_setopt(c, CURLOPT_COOKIEFILE, "");
        char *file = p->net_opts->cookies_file;
        if (file && file[0]) {
            void *tmp = talloc_new(NULL);
            char *path = mp_get_user_path(tmp, p->global, file);
            bstr data = stream_read_file2(path, tmp,
                STREAM_READ_FILE_FLAGS_DEFAULT & ~STREAM_LOCAL_FS_ONLY,
                p->global, 1000000);
            bstr buf = data;
            while (buf.len) {
                bstr line = bstr_strip_linebreaks(bstr_getline(buf, &buf));
                if (!line.len)
                    continue;
                char *line_str = bstrto0(tmp, line);
                curl_easy_setopt(c, CURLOPT_COOKIELIST, line_str);
            }
            talloc_free(tmp);
        }
    }

    p->headers = build_header_list(p);
    if (p->headers)
        curl_easy_setopt(c, CURLOPT_HTTPHEADER, p->headers);
}

// stream_curl implementation

static int curl_fill_buffer(struct stream *s, void *buffer, int max_len)
{
    struct priv *p = s->priv;
    if (max_len <= 0)
        return 0;

    mp_mutex_lock(&p->mtx);

    while (p->count == 0 && !p->stream_eof && !p->stream_error &&
           !atomic_load_explicit(&p->aborted, memory_order_relaxed))
    {
        mp_cond_wait(&p->cond, &p->mtx);
    }

    size_t copy = MPMIN((size_t)max_len, p->count);
    if (copy > 0) {
        size_t head_chunk = MPMIN(p->buffer_size - p->head, copy);
        memcpy(buffer, p->buffer + p->head, head_chunk);
        memcpy((char *)buffer + head_chunk, p->buffer, copy - head_chunk);
        p->head = (p->head + copy) % p->buffer_size;
        p->count -= copy;
    }

    bool unpause = p->paused && !p->stream_eof && !p->stream_error &&
                   p->buffer_size - p->count >= p->buffer_size / 2;

    mp_mutex_unlock(&p->mtx);

    if (unpause)
        cmd_async(p, CMD_UNPAUSE);

    return copy;
}

static int curl_seek(struct stream *s, int64_t pos)
{
    struct priv *p = s->priv;
    if (pos < 0)
        return 0;
    cmd_sync(p, CMD_SEEK, pos, true);
    return 1;
}

static int64_t curl_get_size(struct stream *s)
{
    struct priv *p = s->priv;
    return p->content_size;
}

static void priv_destructor(void *ptr)
{
    struct priv *p = ptr;
    mp_cancel_set_cb(p->s->cancel, NULL, NULL);
    if (p->curl) {
        cmd_sync(p, CMD_REMOVE, 0, false);
        curl_easy_cleanup(p->curl);
    }
    if (p->headers)
        curl_slist_free_all(p->headers);
    mp_mutex_destroy(&p->mtx);
    mp_cond_destroy(&p->cond);
}

static void curl_close(struct stream *s)
{
    struct priv *p = s->priv;
    if (!p)
        return;
    mp_cancel_set_cb(s->cancel, NULL, NULL);
    if (p->curl) {
        cmd_sync(p, CMD_REMOVE, 0, false);
        curl_easy_cleanup(p->curl);
        p->curl = NULL;
    }
}

static int curl_open(stream_t *s, const struct stream_open_args *args)
{
    if (s->mode != STREAM_READ)
        return STREAM_UNSUPPORTED;
    if (!s->global || !s->global->curl) {
        MP_ERR(s, "curl backend not initialized\n");
        return STREAM_ERROR;
    }

    struct curl_opts *opts = mp_get_config_group(s, s->global, &curl_conf);
    if (!opts->enabled)
        return STREAM_NO_MATCH;

    struct priv *p = talloc_zero(s, struct priv);
    s->priv = p;
    talloc_set_destructor(p, priv_destructor);

    p->log = s->log;
    p->global = s->global;
    p->ctx = s->global->curl;
    p->s = s;
    p->opts = talloc_steal(p, opts);
    p->net_opts = mp_get_config_group(p, s->global, &mp_network_conf);
    p->url = talloc_strdup(p, s->url);
    p->scheme = curl_scheme_lookup(bstr0(p->url));
    // Only supported URLs are supposed to reach here.
    mp_assert(p->scheme);
    p->content_size = -1;
    p->buffer_size = p->opts->buffer_size;
    p->buffer = talloc_size(p, p->buffer_size);

    mp_mutex_init(&p->mtx);
    mp_cond_init(&p->cond);
    p->aborted = false;

    p->curl = curl_easy_init();
    if (!p->curl) {
        MP_ERR(s, "curl_easy_init failed\n");
        return STREAM_ERROR;
    }

    setup_curl(p);
    mp_cancel_set_cb(s->cancel, on_cancel, p);

    cmd_sync(p, CMD_ADD, 0, false);

    mp_mutex_lock(&p->mtx);
    while (!p->probed && !atomic_load_explicit(&p->aborted, memory_order_relaxed))
        mp_cond_wait(&p->cond, &p->mtx);
    mp_mutex_unlock(&p->mtx);

    if (!p->stream_ok || atomic_load(&p->aborted))
        return STREAM_ERROR;

    char *content_type = NULL;
    curl_easy_getinfo(p->curl, CURLINFO_CONTENT_TYPE, &content_type);
    if (content_type && content_type[0])
        s->mime_type = talloc_strdup(s, content_type);

    s->seekable = p->seekable;
    s->is_network = true;
    s->streaming = true;
    s->fast_skip = true;
    s->fill_buffer = curl_fill_buffer;
    s->seek = p->seekable ? curl_seek : NULL;
    s->get_size = curl_get_size;
    s->close = curl_close;

    return STREAM_OK;
}

static bool curl_has_proto(bstr scheme)
{
    curl_version_info_data *info = curl_version_info(CURLVERSION_NOW);
    mp_require(info && info->protocols);
    return bstr_in_list0(scheme, (char **)info->protocols);
}

static char **curl_get_protocols(void)
{
    int num = 0;
    char **protocols = NULL;
    for (int i = 0; i < MP_ARRAY_SIZE(curl_schemes); i++) {
        bstr scheme = curl_schemes[i].scheme;
        if (curl_has_proto(scheme))
            MP_TARRAY_APPEND(NULL, protocols, num, bstrdup0(protocols, scheme));
    }
    MP_TARRAY_APPEND(NULL, protocols, num, NULL);
    return protocols;
}

const stream_info_t stream_info_curl = {
    .name = "curl",
    .open2 = curl_open,
    .get_protocols = curl_get_protocols,
    .stream_origin = STREAM_ORIGIN_NET,
};

// FFmpeg AVIOContext implementation
// Allows demuxers to use our stream_curl in nested io and sub-demuxers. This
// should route all traffic through our implementation.

struct curl_avio_cookie {
    struct stream *stream;
    struct mp_cancel *cancel;
};

static bool is_protocol_allowed(struct mp_log *log, bstr scheme,
                                const char *whitelist, const char *blacklist)
{
    // `scheme` is required to be wrapped null-terminated string literal.
    // This is UB otherwise, see curl_schemes.
    mp_assert(scheme.len && scheme.start[scheme.len] == '\0');
    if (whitelist && av_match_list(scheme.start, whitelist, ',') <= 0) {
        mp_err(log, "Protocol '%.*s' not on whitelist '%s'!\n", BSTR_P(scheme), whitelist);
        return false;
    }
    if (blacklist && av_match_list(scheme.start, blacklist, ',') > 0) {
        mp_err(log, "Protocol '%.*s' on blacklist '%s'!\n", BSTR_P(scheme), blacklist);
        return false;
    }
    return true;
}

static int curl_avio_read(void *opaque, uint8_t *buf, int size)
{
    struct curl_avio_cookie *c = opaque;
    int ret = stream_read_partial(c->stream, buf, size);
    return ret > 0 ? ret : AVERROR_EOF;
}

static int64_t curl_avio_seek(void *opaque, int64_t pos, int whence)
{
    struct curl_avio_cookie *c = opaque;
    if (whence == AVSEEK_SIZE) {
        int64_t end = stream_get_size(c->stream);
        return end >= 0 ? end : AVERROR(ENOSYS);
    }
    if (whence == SEEK_END) {
        int64_t end = stream_get_size(c->stream);
        if (end < 0)
            return AVERROR(EINVAL);
        pos += end;
    } else if (whence == SEEK_CUR) {
        pos += stream_tell(c->stream);
    } else if (whence != SEEK_SET) {
        return AVERROR(EINVAL);
    }
    if (pos < 0)
        return AVERROR(EINVAL);
    if (!stream_seek(c->stream, pos))
        return AVERROR(EIO);
    return pos;
}

int mp_curl_avio_open(struct demuxer *demuxer, AVIOContext **pb_out,
                      void **cookie_out, const char *url, int flags,
                      AVDictionary **options,
                      const char *whitelist, const char *blacklist)
{
    *pb_out = NULL;
    *cookie_out = NULL;

    if (flags & AVIO_FLAG_WRITE)
        return AVERROR(ENOSYS);

    // Check protocol early, to return ENOSYS and allow lavf to fallback.
    const struct curl_scheme *cs = curl_scheme_lookup(bstr0(url));
    if (!cs || !curl_has_proto(cs->scheme))
        return AVERROR(ENOSYS);

    struct curl_opts *opts = mp_get_config_group(NULL, demuxer->global, &curl_conf);
    bool enabled = opts->enabled;
    talloc_free(opts);
    if (!enabled)
        return AVERROR(ENOSYS);

    // The context is required to be initialized in global.
    mp_require(demuxer->global && demuxer->global->curl);

    // Nested IO plumbs whitelist/blacklist through the AVDictionary, use that
    // if set, same as FFmpeg's implementation.
    if (options && *options) {
        AVDictionaryEntry *e;
        if ((e = av_dict_get(*options, "protocol_whitelist", NULL, 0)))
            whitelist = e->value;
        if ((e = av_dict_get(*options, "protocol_blacklist", NULL, 0)))
            blacklist = e->value;
    }

    if (!is_protocol_allowed(demuxer->log, cs->scheme, whitelist, blacklist))
        return AVERROR(EINVAL);

    // Each nested stream gets its own mp_cancel slaved to the main demuxer,
    // so the http backend can install its own wake-up callback without
    // clobbering the top-level stream or any sibling nested stream.
    struct mp_cancel *cancel = mp_cancel_new(NULL);
    mp_cancel_set_parent(cancel, demuxer->cancel);

    struct stream_open_args args = {
        .global = demuxer->global,
        .cancel = cancel,
        .url = url,
        .flags = STREAM_READ | (demuxer->stream_origin & STREAM_ORIGIN_MASK),
        .sinfo = &stream_info_curl,
    };

    struct stream *s = NULL;
    int r = stream_create_with_args(&args, &s);
    if (r != STREAM_OK || !s) {
        talloc_free(cancel);
        return AVERROR(EIO);
    }

    struct curl_avio_cookie *c = talloc_zero(NULL, struct curl_avio_cookie);
    c->stream = s;
    c->cancel = cancel;

    void *buffer = av_malloc(64 * 1024);
    if (!buffer) {
        free_stream(s);
        talloc_free(cancel);
        talloc_free(c);
        return AVERROR(ENOMEM);
    }

    AVIOContext *pb = avio_alloc_context(buffer, 64 * 1024, 0, c,
                                         curl_avio_read, NULL,
                                         s->seekable ? curl_avio_seek : NULL);
    if (!pb) {
        av_free(buffer);
        free_stream(s);
        talloc_free(cancel);
        talloc_free(c);
        return AVERROR(ENOMEM);
    }
    pb->seekable = s->seekable ? AVIO_SEEKABLE_NORMAL : 0;

    *pb_out = pb;
    *cookie_out = c;
    return 0;
}

void mp_curl_avio_close(AVIOContext *pb, void *cookie)
{
    struct curl_avio_cookie *c = cookie;
    if (pb) {
        av_freep(&pb->buffer);
        avio_context_free(&pb);
    }
    if (c) {
        free_stream(c->stream);
        talloc_free(c->cancel);
        talloc_free(c);
    }
}
