#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libavformat/avio.h>
#include <libavutil/aes.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>

#include "demux/avio_crypto.h"
#include "misc/bstr.h"
#include "test_utils.h"

#define BLOCKSIZE 16

static const uint8_t test_key[BLOCKSIZE] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
};
static const uint8_t test_iv[BLOCKSIZE] = {
    0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08,
    0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,
};

struct mem_reader {
    const uint8_t *data;
    size_t size;
    size_t pos;
    int chunk;
};

static int mem_read(void *opaque, uint8_t *buf, int size)
{
    struct mem_reader *r = opaque;
    if (r->pos >= r->size)
        return AVERROR_EOF;
    int avail = (int)(r->size - r->pos);
    int n = size < avail ? size : avail;
    if (r->chunk > 0 && n > r->chunk)
        n = r->chunk;
    memcpy(buf, r->data + r->pos, n);
    r->pos += n;
    return n;
}

static AVIOContext *make_inner(struct mem_reader *r)
{
    void *buf = av_malloc(4096);
    mp_require(buf);
    AVIOContext *pb = avio_alloc_context(buf, 4096, 0, r, mem_read, NULL, NULL);
    mp_require(pb);
    pb->seekable = 0;
    return pb;
}

static void free_inner(AVIOContext **pb)
{
    av_freep(&(*pb)->buffer);
    avio_context_free(pb);
}

static uint8_t *encrypt_pkcs7(const uint8_t *key, const uint8_t *iv,
                              const uint8_t *plain, size_t plain_len, size_t *coded_len)
{
    size_t pad = BLOCKSIZE - (plain_len % BLOCKSIZE);
    size_t total = plain_len + pad;
    uint8_t *padded = malloc(total);
    mp_require(padded);
    memcpy(padded, plain, plain_len);
    memset(padded + plain_len, (int)pad, pad);

    struct AVAES *aes = av_aes_alloc();
    mp_require(aes);
    mp_require(av_aes_init(aes, key, BLOCKSIZE * 8, 0) >= 0);

    uint8_t iv_copy[BLOCKSIZE];
    memcpy(iv_copy, iv, BLOCKSIZE);
    uint8_t *out = malloc(total);
    mp_require(out);
    av_aes_crypt(aes, out, padded, (int)(total / BLOCKSIZE), iv_copy, 0);
    av_free(aes);
    free(padded);

    *coded_len = total;
    return out;
}

static void test_roundtrip(const uint8_t *plain, size_t plain_len,
                           int chunk_in, int chunk_out)
{
    size_t coded_len;
    uint8_t *coded = encrypt_pkcs7(test_key, test_iv, plain, plain_len, &coded_len);

    struct mem_reader mr = { .data = coded, .size = coded_len, .chunk = chunk_in };
    AVIOContext *inner = make_inner(&mr);

    AVIOContext *wrapper = NULL;
    bstr kb = { (uint8_t *)test_key, BLOCKSIZE };
    bstr ib = { (uint8_t *)test_iv, BLOCKSIZE };
    assert_int_equal(mp_avio_crypto_open(&wrapper, inner, kb, ib), 0);

    size_t cap = plain_len + 64;
    uint8_t *out = malloc(cap);
    mp_require(out);
    size_t got = 0;
    while (got < cap) {
        int want = chunk_out > 0 ? chunk_out : (int)(cap - got);
        if ((size_t)want > cap - got)
            want = (int)(cap - got);
        int n = avio_read(wrapper, out + got, want);
        if (n <= 0)
            break;
        got += n;
    }

    assert_int_equal(got, plain_len);
    if (plain_len)
        assert_memcmp(out, plain, plain_len);

    free(out);
    mp_avio_crypto_close(&wrapper);
    free_inner(&inner);
    free(coded);
}

// Feed a single ciphertext block whose plaintext last byte is `bad`. The
// decrypt path treats that byte as the PKCS#7 length and rejects it.
static void test_invalid_padding(uint8_t bad)
{
    uint8_t plain[BLOCKSIZE] = { 0 };
    plain[BLOCKSIZE - 1] = bad;

    struct AVAES *aes = av_aes_alloc();
    mp_require(aes);
    mp_require(av_aes_init(aes, test_key, BLOCKSIZE * 8, 0) >= 0);
    uint8_t iv_copy[BLOCKSIZE];
    memcpy(iv_copy, test_iv, BLOCKSIZE);
    uint8_t ct[BLOCKSIZE];
    av_aes_crypt(aes, ct, plain, 1, iv_copy, 0);
    av_free(aes);

    struct mem_reader mr = { .data = ct, .size = BLOCKSIZE };
    AVIOContext *inner = make_inner(&mr);

    AVIOContext *wrapper = NULL;
    bstr kb = { (uint8_t *)test_key, BLOCKSIZE };
    bstr ib = { (uint8_t *)test_iv, BLOCKSIZE };
    assert_int_equal(mp_avio_crypto_open(&wrapper, inner, kb, ib), 0);

    uint8_t out[BLOCKSIZE];
    int n = avio_read(wrapper, out, sizeof(out));
    assert_int_equal(n, AVERROR_INVALIDDATA);

    mp_avio_crypto_close(&wrapper);
    free_inner(&inner);
}

static void test_invalid_args(void)
{
    struct mem_reader empty = {0};
    AVIOContext *dummy = make_inner(&empty);

    AVIOContext *out = NULL;
    bstr kb = { (uint8_t *)test_key, BLOCKSIZE };
    bstr ib = { (uint8_t *)test_iv, BLOCKSIZE };
    bstr kshort = { (uint8_t *)test_key, 8 };
    bstr ishort = { (uint8_t *)test_iv, 8 };

    assert_int_equal(mp_avio_crypto_open(&out, dummy, kshort, ib),
                     AVERROR(EINVAL));
    assert_int_equal(mp_avio_crypto_open(&out, dummy, kb, ishort),
                     AVERROR(EINVAL));
    assert_int_equal(mp_avio_crypto_open(NULL, dummy, kb, ib),
                     AVERROR(EINVAL));
    assert_int_equal(mp_avio_crypto_open(&out, NULL, kb, ib),
                     AVERROR(EINVAL));
    assert_true(out == NULL);

    free_inner(&dummy);
}

int main(void)
{
    static const uint8_t lorem[] =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do "
        "eiusmod tempor incididunt ut labore et dolore magna aliqua.";

    // Sizes that exercise sub-block, exact-block, and odd-length cases.
    static const size_t sizes[] = { 0, 1, 15, 16, 17, 31, 32, 33, 100,
                                    sizeof(lorem) - 1 };

    uint8_t big[8192];
    for (size_t i = 0; i < sizeof(big); i++)
        big[i] = (uint8_t)(i * 31u + 7u);

    for (size_t k = 0; k < MP_ARRAY_SIZE(sizes); k++) {
        size_t sz = sizes[k];
        const uint8_t *src = sz <= sizeof(lorem) - 1 ? lorem : big;
        test_roundtrip(src, sz, 0,  0);
        test_roundtrip(src, sz, 1,  0);  // inner trickles 1 byte at a time
        test_roundtrip(src, sz, 0,  1);  // caller reads 1 byte at a time
        test_roundtrip(src, sz, 1,  1);
        test_roundtrip(src, sz, 13, 7);
    }

    // Long input that crosses many internal refill boundaries.
    test_roundtrip(big, sizeof(big), 0, 0);
    test_roundtrip(big, sizeof(big), 5, 11);

    test_invalid_padding(0x00);          // zero is invalid
    test_invalid_padding(BLOCKSIZE + 1); // > BLOCKSIZE is invalid
    test_invalid_padding(0xff);

    test_invalid_args();
    return 0;
}
