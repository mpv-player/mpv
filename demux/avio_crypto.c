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

#include <string.h>

#include <libavformat/avio.h>
#include <libavutil/aes.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>

#include "avio_crypto.h"

#include "common/common.h"
#include "misc/bstr.h"
#include "mpv_talloc.h"

#define BLOCKSIZE         16
// In-place decrypt scratch. Holds unconsumed plaintext followed by at most one
// held-back ciphertext block. Any value >= 2*BLOCKSIZE works, this is just a
// reasonable batch size.
#define IO_BUFFER_SIZE    (4 * 1024)
#define AVIO_BUFFER_SIZE  (64 * 1024)

struct crypto_priv {
    AVIOContext *inner;
    struct AVAES *aes;
    uint8_t iv[BLOCKSIZE];

    // [0          .. plain_pos)  consumed plaintext (free space)
    // [plain_pos  .. plain_end)  plaintext to deliver
    // [plain_end  .. ct_end)     held-back ciphertext (< BLOCKSIZE)
    uint8_t buf[IO_BUFFER_SIZE];
    int plain_pos;
    int plain_end;
    int ct_end;
    bool inner_eof;
};

static void priv_destructor(void *ptr)
{
    struct crypto_priv *p = ptr;
    av_free(p->aes);
}

static int crypto_read(void *opaque, uint8_t *buf, int size)
{
    struct crypto_priv *p = opaque;

    if (size <= 0)
        return 0;

    while (p->plain_pos == p->plain_end) {
        // Compact held-back ciphertext to the start of the buffer.
        int held = p->ct_end - p->plain_end;
        if (held > 0 && p->plain_end > 0)
            memmove(p->buf, p->buf + p->plain_end, held);
        p->plain_pos = p->plain_end = 0;
        p->ct_end = held;

        // Refill until we have two blocks pending (so the final one can stay
        // held back for PKCS#7 stripping) or hit EOF.
        while (!p->inner_eof && p->ct_end < 2 * BLOCKSIZE) {
            int n = avio_read(p->inner, p->buf + p->ct_end,
                              sizeof(p->buf) - p->ct_end);
            if (n <= 0) {
                p->inner_eof = true;
                break;
            }
            p->ct_end += n;
        }

        int blocks = p->ct_end / BLOCKSIZE;
        if (!p->inner_eof)
            blocks--;
        if (blocks <= 0)
            return AVERROR_EOF;

        av_aes_crypt(p->aes, p->buf, p->buf, blocks, p->iv, 1);
        p->plain_end = blocks * BLOCKSIZE;

        // Final batch: drop PKCS#7 padding bytes from the tail.
        if (p->inner_eof && p->plain_end == p->ct_end) {
            int padding = p->buf[p->plain_end - 1];
            if (padding < 1 || padding > BLOCKSIZE)
                return AVERROR_INVALIDDATA;
            p->plain_end -= padding;
        }
    }

    int copy = MPMIN(p->plain_end - p->plain_pos, size);
    memcpy(buf, p->buf + p->plain_pos, copy);
    p->plain_pos += copy;
    return copy;
}

int mp_avio_crypto_open(AVIOContext **out_pb, AVIOContext *inner,
                        bstr key, bstr iv)
{
    if (!out_pb || !inner || key.len != BLOCKSIZE || iv.len != BLOCKSIZE)
        return AVERROR(EINVAL);

    int r;
    void *avio_buf = NULL;
    struct crypto_priv *p = talloc_zero(NULL, struct crypto_priv);
    talloc_set_destructor(p, priv_destructor);

    p->inner = inner;
    p->aes = av_aes_alloc();
    if (!p->aes) {
        r = AVERROR(ENOMEM);
        goto fail;
    }
    if (av_aes_init(p->aes, key.start, BLOCKSIZE * 8, 1) < 0) {
        r = AVERROR(EINVAL);
        goto fail;
    }
    memcpy(p->iv, iv.start, BLOCKSIZE);

    avio_buf = av_malloc(AVIO_BUFFER_SIZE);
    if (!avio_buf) {
        r = AVERROR(ENOMEM);
        goto fail;
    }

    AVIOContext *pb = avio_alloc_context(avio_buf, AVIO_BUFFER_SIZE, 0, p,
                                         crypto_read, NULL, NULL);
    if (!pb) {
        r = AVERROR(ENOMEM);
        goto fail;
    }
    pb->seekable = 0;

    *out_pb = pb;
    return 0;

fail:
    av_free(avio_buf);
    talloc_free(p);
    return r;
}

void mp_avio_crypto_close(AVIOContext **pb)
{
    if (!pb || !*pb)
        return;
    struct crypto_priv *p = (*pb)->opaque;
    av_freep(&(*pb)->buffer);
    avio_context_free(pb);
    talloc_free(p);
}
