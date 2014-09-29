/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdint.h>

#include "osdep/io.h"

#include "common/common.h"
#include "common/msg.h"

#include "options/options.h"

#include "stream.h"

#define BLOCK_SIZE 1024LL
#define BLOCK_ALIGN(p) ((p) & ~(BLOCK_SIZE - 1))

struct priv {
    struct stream *original;
    FILE *cache_file;
    uint8_t *block_bits;    // 1 bit for each BLOCK_SIZE, whether block was read
    int64_t size;           // currently known size
    int64_t max_size;       // max. size for block_bits and cache_file
};

static bool test_bit(struct priv *p, int64_t pos)
{
    if (pos < 0 || pos >= p->size)
        return false;
    size_t block = pos / BLOCK_SIZE;
    return p->block_bits[block / 8] & (1 << (block % 8));
}

static void set_bit(struct priv *p, int64_t pos, bool bit)
{
    if (pos < 0 || pos >= p->size)
        return;
    size_t block = pos / BLOCK_SIZE;
    unsigned int m = (1 << (block % 8));
    p->block_bits[block / 8] = (p->block_bits[block / 8] & ~m) | (bit ? m : 0);
}

static int fill_buffer(stream_t *s, char *buffer, int max_len)
{
    struct priv *p = s->priv;
    if (s->pos < 0)
        return -1;
    if (s->pos >= p->max_size) {
        if (stream_seek(p->original, s->pos) < 1)
            return -1;
        return stream_read(p->original, buffer, max_len);
    }
    // Size of file changes -> invalidate last block
    if (s->pos >= p->size - BLOCK_SIZE) {
        int64_t new_size = -1;
        stream_control(s, STREAM_CTRL_GET_SIZE, &new_size);
        if (p->size >= 0 && new_size != p->size)
            set_bit(p, BLOCK_ALIGN(p->size), 0);
        p->size = MPMIN(p->max_size, new_size);
    }
    int64_t aligned = BLOCK_ALIGN(s->pos);
    if (!test_bit(p, aligned)) {
        char tmp[BLOCK_SIZE];
        stream_seek(p->original, aligned);
        int r = stream_read(p->original, tmp, BLOCK_SIZE);
        if (r < BLOCK_SIZE) {
            if (p->size < 0) {
                MP_WARN(s, "suspected EOF\n");
            } else if (aligned + r < p->size) {
                MP_ERR(s, "unexpected EOF\n");
                return -1;
            }
        }
        if (fseeko(p->cache_file, aligned, SEEK_SET))
            return -1;
        if (fwrite(tmp, r, 1, p->cache_file) != 1)
            return -1;
        set_bit(p, aligned, 1);
    }
    if (fseeko(p->cache_file, s->pos, SEEK_SET))
        return -1;
    // align/limit to blocks
    max_len = MPMIN(max_len, BLOCK_SIZE - (s->pos % BLOCK_SIZE));
    // Limit to max. known file size
    if (p->size >= 0)
        max_len = MPMIN(max_len, p->size - s->pos);
    return fread(buffer, 1, max_len, p->cache_file);
}

static int seek(stream_t *s, int64_t newpos)
{
    return 1;
}

static int control(stream_t *s, int cmd, void *arg)
{
    struct priv *p = s->priv;
    return stream_control(p->original, cmd, arg);
}

static void s_close(stream_t *s)
{
    struct priv *p = s->priv;
    if (p->cache_file)
        fclose(p->cache_file);
    talloc_free(p);
}

// return 1 on success, 0 if disabled, -1 on error
int stream_file_cache_init(stream_t *cache, stream_t *stream,
                           struct mp_cache_opts *opts)
{
    if (!opts->file || !opts->file[0] || opts->file_max < 1)
        return 0;

    if (!stream->seekable) {
        MP_ERR(cache, "can't cache unseekable stream\n");
        return -1;
    }

    bool use_anon_file = strcmp(opts->file, "TMP") == 0;
    FILE *file = use_anon_file ? tmpfile() : fopen(opts->file, "wb+");
    if (!file) {
        MP_ERR(cache, "can't open cache file '%s'\n", opts->file);
        return -1;
    }

    struct priv *p = talloc_zero(NULL, struct priv);

    cache->priv = p;
    p->original = stream;
    p->cache_file = file;
    p->max_size = opts->file_max * 1024LL;

    // file_max can be INT_MAX, so this is at most about 256MB
    p->block_bits = talloc_zero_size(p, (p->max_size / BLOCK_SIZE + 1) / 8 + 1);

    cache->seek = seek;
    cache->fill_buffer = fill_buffer;
    cache->control = control;
    cache->close = s_close;

    return 1;
}
