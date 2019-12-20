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
#include "misc/thread_tools.h"

struct priv {
    mpv_stream_cb_info info;
    struct mp_cancel *cancel;
};

static int fill_buffer(stream_t *s, void *buffer, int max_len)
{
    struct priv *p = s->priv;
    return (int)p->info.read_fn(p->info.cookie, buffer, (size_t)max_len);
}

static int seek(stream_t *s, int64_t newpos)
{
    struct priv *p = s->priv;
    return p->info.seek_fn(p->info.cookie, newpos) >= 0;
}

static int64_t get_size(stream_t *s)
{
    struct priv *p = s->priv;

    if (p->info.size_fn) {
        int64_t size = p->info.size_fn(p->info.cookie);
        if (size >= 0)
            return size;
    }

    return -1;
}

static void s_close(stream_t *s)
{
    struct priv *p = s->priv;
    p->info.close_fn(p->info.cookie);
}

static int open_cb(stream_t *stream)
{
    struct priv *p = talloc_ptrtype(stream, p);
    stream->priv = p;

    bstr bproto = mp_split_proto(bstr0(stream->url), NULL);
    char *proto = bstrto0(stream, bproto);

    void *user_data;
    mpv_stream_cb_open_ro_fn open_fn;

    if (!mp_streamcb_lookup(stream->global, proto, &user_data, &open_fn))
        return STREAM_UNSUPPORTED;

    mpv_stream_cb_info info = {0};

    int r = open_fn(user_data, stream->url, &info);
    if (r < 0) {
        if (r != MPV_ERROR_LOADING_FAILED)
            MP_WARN(stream, "unknown error from user callback\n");
        return STREAM_ERROR;
    }

    if (!info.read_fn || !info.close_fn) {
        MP_FATAL(stream, "required read_fn or close_fn callbacks not set.\n");
        return STREAM_ERROR;
    }

    p->info = info;

    if (p->info.seek_fn && p->info.seek_fn(p->info.cookie, 0) >= 0) {
        stream->seek = seek;
        stream->seekable = true;
    }
    stream->fast_skip = true;
    stream->fill_buffer = fill_buffer;
    stream->get_size = get_size;
    stream->close = s_close;

    if (p->info.cancel_fn && stream->cancel) {
        p->cancel = mp_cancel_new(p);
        mp_cancel_set_parent(p->cancel, stream->cancel);
        mp_cancel_set_cb(p->cancel, p->info.cancel_fn, p->info.cookie);
    }

    return STREAM_OK;
}

const stream_info_t stream_info_cb = {
    .name = "stream_callback",
    .open = open_cb,
    .stream_origin = STREAM_ORIGIN_UNSAFE,
};
