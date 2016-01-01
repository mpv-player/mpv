#include "config.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "osdep/io.h"

#include "common/common.h"
#include "common/msg.h"
#include "common/global.h"
#include "stream.h"
#include "options/m_option.h"
#include "options/path.h"
#include "player/client.h"
#include "libmpv/stream_cb.h"

struct mpv_stream_cb_context {
    void *user_data;
    mpv_stream_cb_open_fn open_fn;
    mpv_stream_cb_read_fn read_fn;
    mpv_stream_cb_seek_fn seek_fn;
    mpv_stream_cb_size_fn size_fn;
    mpv_stream_cb_close_fn close_fn;
};

struct mpv_stream_cb_context *mp_stream_cb_fetch(struct mpv_global *g,
                                                 struct mp_client_api *client_api)
{
    return g->stream_cb_ctx;
}

void mpv_stream_cb_init(mpv_stream_cb_context *ctx, void *user_data)
{
    ctx->user_data = user_data;
}

void mpv_stream_cb_set_open_fn(mpv_stream_cb_context *ctx,
                               mpv_stream_cb_open_fn fn)
{
    ctx->open_fn = fn;
}

void mpv_stream_cb_set_read_fn(mpv_stream_cb_context *ctx,
                               mpv_stream_cb_read_fn fn)
{
    ctx->read_fn = fn;
}

void mpv_stream_cb_set_seek_fn(mpv_stream_cb_context *ctx,
                               mpv_stream_cb_seek_fn fn)
{
    ctx->seek_fn = fn;
}

void mpv_stream_cb_set_size_fn(mpv_stream_cb_context *ctx,
                               mpv_stream_cb_size_fn fn)
{
    ctx->size_fn = fn;
}

void mpv_stream_cb_set_close_fn(mpv_stream_cb_context *ctx,
                                mpv_stream_cb_close_fn fn)
{
    ctx->close_fn = fn;
}

struct priv {
    void *cookie;
    mpv_stream_cb_context *stream_cb_ctx;
};

static int fill_buffer(stream_t *s, char *buffer, int max_len)
{
    struct priv *p = s->priv;
    return (int)p->stream_cb_ctx->read_fn(p->cookie, buffer, (size_t)max_len);
}

static int seek(stream_t *s, int64_t newpos)
{
    struct priv *p = s->priv;
    return (int)p->stream_cb_ctx->seek_fn(p->cookie, (off_t)newpos, SEEK_SET) != -1;
}

static int control(stream_t *s, int cmd, void *arg)
{
    struct priv *p = s->priv;
    switch (cmd) {
    case STREAM_CTRL_GET_SIZE: {
        if (!p->stream_cb_ctx->size_fn)
            break;
        off_t size = p->stream_cb_ctx->size_fn(p->cookie);
        if (size != (off_t)-1) {
            *(int64_t *)arg = size;
            return 1;
        }
        break;
    }
    }
    return STREAM_UNSUPPORTED;
}

static void s_close(stream_t *s)
{
    struct priv *p = s->priv;
    if (p->stream_cb_ctx->close_fn)
        p->stream_cb_ctx->close_fn(p->cookie);
}

static int open_cb(stream_t *stream)
{
    struct priv *p = talloc_ptrtype(stream, p);
    stream->priv = p;
    stream->type = STREAMTYPE_CB;

    if (strncmp(stream->url, "cb://", 5) == 0) {
        p->stream_cb_ctx = stream->global->stream_cb_ctx;
        if (!p->stream_cb_ctx || !p->stream_cb_ctx->open_fn || !p->stream_cb_ctx->read_fn) {
            MP_ERR(stream, "Missing callbacks, call mpv_stream_cb_set_{open,read}_fn() first.\n");
            return STREAM_ERROR;
        }

        p->cookie = p->stream_cb_ctx->open_fn(p->stream_cb_ctx->user_data, stream->url);
        if (!p->cookie)
            return STREAM_ERROR;
    } else {
        return STREAM_ERROR;
    }

    if (p->stream_cb_ctx->seek_fn) {
        stream->seek = seek;
        stream->seekable = true;
    }
    stream->fast_skip = true;
    stream->fill_buffer = fill_buffer;
    stream->control = control;
    stream->read_chunk = 64 * 1024;
    stream->close = s_close;

    return STREAM_OK;
}

const stream_info_t stream_info_cb = {
    .name = "stream_callback",
    .open = open_cb,
    .protocols = (const char*const[]){ "cb", NULL },
};
