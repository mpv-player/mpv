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

#include <stdint.h>
#include <string.h>

#include <libavutil/aes.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/mem.h>

#include "common/common.h"
#include "misc/bstr.h"
#include "stream.h"

#define BLOCKSIZE 16
// Number of key stream blocks generated at once.
#define MAX_BLOCKS 256

struct priv {
    struct stream *inner;
    struct AVAES *aes;
    uint8_t iv[BLOCKSIZE];
};

static void priv_destructor(void *ptr)
{
    struct priv *p = ptr;
    av_free(p->aes);
}

// XOR the key stream onto data that starts at byte pos of the stream. The
// counter block is the IV plus the index of the block, as a 128 bit big
// endian number.
static void apply_keystream(struct priv *p, uint64_t pos, uint8_t *data, int len)
{
    uint8_t keystream[MAX_BLOCKS * BLOCKSIZE];
    uint64_t iv_high = AV_RB64(p->iv);
    uint64_t iv_low = AV_RB64(p->iv + 8);
    uint64_t index = pos / BLOCKSIZE;
    int offset = pos % BLOCKSIZE;

    while (len > 0) {
        int n = MPMIN(len, (int)sizeof(keystream) - offset);
        int blocks = MP_DIV_UP(offset + n, BLOCKSIZE);

        for (int i = 0; i < blocks; i++) {
            uint64_t low = iv_low + index + i;
            AV_WB64(&keystream[i * BLOCKSIZE], iv_high + (low < iv_low));
            AV_WB64(&keystream[i * BLOCKSIZE + 8], low);
        }
        av_aes_crypt(p->aes, keystream, keystream, blocks, NULL, 0);

        for (int i = 0; i < n; i++)
            data[i] ^= keystream[offset + i];

        data += n;
        len -= n;
        index += blocks;
        offset = 0;
    }
}

static int fill_buffer(struct stream *s, void *buffer, int len)
{
    struct priv *p = s->priv;
    int64_t pos = stream_tell(p->inner);
    int r = stream_read_partial(p->inner, buffer, len);
    if (r > 0)
        apply_keystream(p, pos, buffer, r);
    return r;
}

static int seek(struct stream *s, int64_t newpos)
{
    struct priv *p = s->priv;
    return stream_seek(p->inner, newpos);
}

static int64_t get_size(struct stream *s)
{
    struct priv *p = s->priv;
    return stream_get_size(p->inner);
}

static void s_close(struct stream *s)
{
    struct priv *p = s->priv;
    free_stream(p->inner);
}

static int parse_url(struct stream *stream)
{
    struct priv *p = stream->priv;

    struct bstr params, inner_url;
    if (!bstr_split_tok(bstr0(stream->path), "@", &params, &inner_url) ||
        !inner_url.len)
    {
        MP_ERR(stream, "Expected aes-ctr://key:iv@URL: '%s'\n", stream->url);
        return STREAM_ERROR;
    }

    struct bstr key, iv;
    bstr_split_tok(params, ":", &key, &iv);
    if ((key.len != 32 && key.len != 48 && key.len != 64) ||
        !bstr_decode_hex(stream, key, &key))
    {
        MP_ERR(stream, "The key must have 32, 48 or 64 hex digits: '%s'\n",
               stream->url);
        return STREAM_ERROR;
    }
    if (iv.len != 2 * BLOCKSIZE || !bstr_decode_hex(stream, iv, &iv)) {
        MP_ERR(stream, "The IV must have 32 hex digits: '%s'\n", stream->url);
        return STREAM_ERROR;
    }

    p->aes = av_aes_alloc();
    if (!p->aes || av_aes_init(p->aes, key.start, key.len * 8, 0) < 0)
        return STREAM_ERROR;
    memcpy(p->iv, iv.start, BLOCKSIZE);

    stream->path = bstrto0(stream, inner_url);

    return STREAM_OK;
}

static int open2(struct stream *stream, const struct stream_open_args *args)
{
    struct priv *p = talloc_zero(stream, struct priv);
    talloc_set_destructor(p, priv_destructor);
    stream->priv = p;

    stream->fill_buffer = fill_buffer;
    stream->get_size = get_size;
    stream->close = s_close;

    int parse_ret = parse_url(stream);
    if (parse_ret != STREAM_OK)
        return parse_ret;

    struct stream_open_args args2 = *args;
    args2.url = stream->path;
    int inner_ret = stream_create_with_args(&args2, &p->inner);
    if (inner_ret != STREAM_OK)
        return inner_ret;

    if (p->inner->is_directory) {
        MP_FATAL(stream, "Inner stream '%s' is a directory\n", p->inner->url);
        free_stream(p->inner);
        return STREAM_ERROR;
    }

    if (p->inner->seekable) {
        stream->seek = seek;
        stream->seekable = true;
    }
    stream->fast_skip = p->inner->fast_skip;
    stream->streaming = p->inner->streaming;
    stream->is_network = p->inner->is_network;
    stream->stream_origin = p->inner->stream_origin;

    return STREAM_OK;
}

const stream_info_t stream_info_aes_ctr = {
    .name = "aes-ctr",
    .open2 = open2,
    .protocols = (const char*const[]){ "aes-ctr", NULL },
    .can_write = false,
};
