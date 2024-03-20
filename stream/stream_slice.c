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
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include "common/common.h"
#include "options/m_option.h"
#include "options/path.h"
#include "stream.h"

struct priv {
    int64_t slice_start;
    int64_t slice_max_end; // 0 for no limit
    struct stream *inner;
};

static int fill_buffer(struct stream *s, void *buffer, int len)
{
    struct priv *p = s->priv;

    if (p->slice_max_end) {
        // We don't simply use (s->pos >= size) to avoid early return
        // if the file is still being appended to.
        if (s->pos + p->slice_start >= p->slice_max_end)
            return -1;
        // Avoid rading beyond p->slice_max_end
        len = MPMIN(len, p->slice_max_end - s->pos);
    }

    return stream_read_partial(p->inner, buffer, len);
}

static int seek(struct stream *s, int64_t newpos)
{
    struct priv *p = s->priv;
    return stream_seek(p->inner, newpos + p->slice_start);
}

static int64_t get_size(struct stream *s)
{
    struct priv *p = s->priv;
    int64_t size = stream_get_size(p->inner);
    if (size <= 0)
        return size;
    if (size <= p->slice_start)
        return 0;
    if (p->slice_max_end)
        size = MPMIN(size, p->slice_max_end);
    return size - p->slice_start;
}

static void s_close(struct stream *s)
{
    struct priv *p = s->priv;
    free_stream(p->inner);
}

static int parse_slice_range(stream_t *stream)
{
    struct priv *p = stream->priv;

    struct bstr b_url = bstr0(stream->url);
    struct bstr proto_with_range, inner_url;

    bool has_at = bstr_split_tok(b_url, "@", &proto_with_range, &inner_url);

    if (!has_at) {
        MP_ERR(stream, "Expected slice://start[-end]@URL: '%s'\n", stream->url);
        return STREAM_ERROR;
    }

    if (!inner_url.len) {
        MP_ERR(stream, "URL expected to follow 'slice://start[-end]@': '%s'.\n", stream->url);
        return STREAM_ERROR;
    }
    stream->path = bstrto0(stream, inner_url);

    mp_split_proto(proto_with_range, &proto_with_range);
    struct bstr range = proto_with_range;

    struct bstr start, end;
    bool has_end = bstr_split_tok(range, "-", &start, &end);

    if (!start.len) {
        MP_ERR(stream, "The byte range must have a start, and it can't be negative: '%s'\n", stream->url);
        return STREAM_ERROR;
    }

    if (has_end && !end.len) {
        MP_ERR(stream, "The byte range end can be omitted, but it can't be empty: '%s'\n", stream->url);
        return STREAM_ERROR;
    }

    const struct m_option opt = {
        .type = &m_option_type_byte_size,
    };

    if (m_option_parse(stream->log, &opt, bstr0("slice_start"), start, &p->slice_start) < 0)
        return STREAM_ERROR;

    bool max_end_is_offset = bstr_startswith0(end, "+");
    if (has_end) {
        if (m_option_parse(stream->log, &opt, bstr0("slice_max_end"), end, &p->slice_max_end) < 0)
            return STREAM_ERROR;
    }

    if (max_end_is_offset)
        p->slice_max_end += p->slice_start;

    if (p->slice_max_end && p->slice_max_end < p->slice_start) {
        MP_ERR(stream, "The byte range end (%"PRId64") can't be smaller than the start (%"PRId64"): '%s'\n",
                p->slice_max_end,
                p->slice_start,
                stream->url);
        return STREAM_ERROR;
    }

    return STREAM_OK;
}

static int open2(struct stream *stream, const struct stream_open_args *args)
{
    struct priv *p = talloc_zero(stream, struct priv);
    stream->priv = p;

    stream->fill_buffer = fill_buffer;
    stream->seek = seek;
    stream->get_size = get_size;
    stream->close = s_close;

    int parse_ret = parse_slice_range(stream);
    if (parse_ret != STREAM_OK) {
        return parse_ret;
    }

    struct stream_open_args args2 = *args;
    args2.url = stream->path;
    int inner_ret = stream_create_with_args(&args2, &p->inner);
    if (inner_ret != STREAM_OK) {
        return inner_ret;
    }

    if (!p->inner->seekable) {
        MP_FATAL(stream, "Non-seekable stream '%s' can't be used with 'slice://'\n", p->inner->url);
        free_stream(p->inner);
        return STREAM_ERROR;
    }

    stream->seekable = 1;
    stream->stream_origin = p->inner->stream_origin;

    if (p->slice_start)
        seek(stream, 0);

    return STREAM_OK;
}

const stream_info_t stream_info_slice = {
    .name = "slice",
    .open2 = open2,
    .protocols = (const char*const[]){ "slice", NULL },
    .can_write = false,
};
