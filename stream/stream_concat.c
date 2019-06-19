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

#include <libavutil/common.h>

#include "common/common.h"
#include "stream.h"

struct priv {
    struct stream **streams;
    int num_streams;

    int64_t size;

    int cur; // streams[cur] is the stream for current stream.pos
};

static int fill_buffer(struct stream *s, char* buffer, int len)
{
    struct priv *p = s->priv;

    while (1) {
        int res = stream_read_partial(p->streams[p->cur], buffer, len);
        if (res || p->cur == p->num_streams - 1)
            return res;

        p->cur += 1;
        if (s->seekable)
            stream_seek(p->streams[p->cur], 0);
    }
}

static int seek(struct stream *s, int64_t newpos)
{
    struct priv *p = s->priv;

    int64_t next_pos = 0;
    int64_t base_pos = 0;

    // Look for the last stream whose start position is <= newpos.
    // Note that the last stream's size is essentially ignored. The last
    // stream is allowed to have an unknown size.
    for (int n = 0; n < p->num_streams; n++) {
        if (next_pos > newpos)
            break;

        base_pos = next_pos;
        p->cur = n;

        int64_t size = stream_get_size(p->streams[n]);
        if (size < 0)
            break;

        next_pos = base_pos + size;
    }

    int ok = stream_seek(p->streams[p->cur], newpos - base_pos);
    s->pos = base_pos + stream_tell(p->streams[p->cur]);
    return ok;
}

static int control(struct stream *s, int cmd, void *arg)
{
    struct priv *p = s->priv;
    if (cmd == STREAM_CTRL_GET_SIZE && p->size >= 0) {
        *(int64_t *)arg = p->size;
        return 1;
    }
    return STREAM_UNSUPPORTED;
}

static void s_close(struct stream *s)
{
    struct priv *p = s->priv;

    for (int n = 0; n < p->num_streams; n++)
        free_stream(p->streams[n]);
}

static int open2(struct stream *stream, void *arg)
{
    struct priv *p = talloc_zero(stream, struct priv);
    stream->priv = p;

    stream->fill_buffer = fill_buffer;
    stream->control = control;
    stream->close = s_close;

    stream->seekable = true;

    struct priv *list = arg;
    if (!list || !list->num_streams) {
        MP_FATAL(stream, "No sub-streams.\n");
        return STREAM_ERROR;
    }

    for (int n = 0; n < list->num_streams; n++) {
        struct stream *sub = list->streams[n];

        stream->read_chunk = MPMAX(stream->read_chunk, sub->read_chunk);

        int64_t size = stream_get_size(sub);
        if (n != list->num_streams - 1 && size < 0) {
            MP_WARN(stream, "Sub stream %d has unknown size.\n", n);
            stream->seekable = false;
            p->size = -1;
        } else if (size >= 0 && p->size >= 0) {
            p->size += size;
        }

        if (!sub->seekable)
            stream->seekable = false;

        MP_TARRAY_APPEND(p, p->streams, p->num_streams, sub);
    }

    if (stream->seekable)
        stream->seek = seek;

    return STREAM_OK;
}

static const stream_info_t stream_info_concat = {
    .name = "concat",
    .open2 = open2,
    .protocols = (const char*const[]){ "concat", NULL },
};

// Create a stream with a concatenated view on streams[]. Copies the streams
// array. Takes over ownership of every stream passed to it (it will free them
// if the concat stream is closed).
// If an error happens, NULL is returned, and the streams are not freed.
struct stream *stream_concat_open(struct mpv_global *global, struct mp_cancel *c,
                                  struct stream **streams, int num_streams)
{
    // (struct priv is blatantly abused to pass in the stream list)
    struct priv arg = {
        .streams = streams,
        .num_streams = num_streams,
    };

    struct stream *s = NULL;
    stream_create_instance(&stream_info_concat, "concat://",
                           STREAM_READ | STREAM_SILENT, c, global, &arg, &s);
    return s;
}
