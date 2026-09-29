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

#include <libavutil/aes_ctr.h>

#include <mpv/stream_cb.h>

#include "common/common.h"
#include "libmpv_common.h"

#define BLOCKSIZE 16
#define SOURCE_SIZE 300000

// The low half of the IV passes 32 bits within the source.
#define SOURCE_KEY "00112233445566778899aabbccddeeff"
#define SOURCE_IV "a0a1a2a3a4a5a6a700000000fffffff0"

struct vector {
    const char *name;
    const char *key;
    const char *iv;
    const char *cipher;
};

#define PLAIN \
    "6bc1bee22e409f96e93d7e117393172a" \
    "ae2d8a571e03ac9c9eb76fac45af8e51" \
    "30c81c46a35ce411e5fbc1191a0a52ef" \
    "f69f2445df4f9b17ad2b417be66c3710"

static const struct vector vectors[] = {
    // NIST SP 800-38A, F.5.1, F.5.3 and F.5.5
    {
        .name = "AES-128",
        .key = "2b7e151628aed2a6abf7158809cf4f3c",
        .iv = "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff",
        .cipher = "874d6191b620e3261bef6864990db6ce"
                  "9806f66b7970fdff8617187bb9fffdff"
                  "5ae4df3edbd5d35e5b4f09020db03eab"
                  "1e031dda2fbe03d1792170a0f3009cee",
    }, {
        .name = "AES-192",
        .key = "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
        .iv = "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff",
        .cipher = "1abc932417521ca24f2b0459fe7e6e0b"
                  "090339ec0aa6faefd5ccc2c6f4ce8e94"
                  "1e36b26bd1ebc670d1bd1d665620abf7"
                  "4f78a7f6d29809585a97daec58c6b050",
    }, {
        .name = "AES-256",
        .key = "603deb1015ca71be2b73aef0857d7781"
               "1f352c073b6108d72d9810a30914dff4",
        .iv = "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff",
        .cipher = "601ec313775789a5b7a7f504bbf3d228"
                  "f443e3ca4d62b59aca84e990cacaf5c5"
                  "2b0930daa23de94ce87017ba2d84988d"
                  "dfc9c58db67aada613c2dd08457941a6",
    },
    // Counters that carry past 64 bits, made with
    // openssl enc -aes-128-ctr -K <key> -iv <iv>
    {
        .name = "carry in the first block",
        .key = "2b7e151628aed2a6abf7158809cf4f3c",
        .iv = "0123456789abcdefffffffffffffffff",
        .cipher = "7fcd7b6e665aabaae97763d99fa22b8b"
                  "ee1fc6dc82dd9c6b1f4750dd948294da"
                  "0f60a620fc0281bfb28fda3955ad3ab8"
                  "6222f5587a513df59e92e701ee3e1b73",
    }, {
        .name = "carry in the second block",
        .key = "2b7e151628aed2a6abf7158809cf4f3c",
        .iv = "0000000000000000fffffffffffffffe",
        .cipher = "393993cf1e6593644880765e7f951dda"
                  "41aabde09dc756147830813822a8b13f"
                  "ecc227852555267e8ad1a2ba86fdbc7c"
                  "3374b251626cc364521c305ea579336c",
    }, {
        .name = "wrap around",
        .key = "2b7e151628aed2a6abf7158809cf4f3c",
        .iv = "ffffffffffffffffffffffffffffffff",
        .cipher = "e13338e36cb71962e00d020b4cedbd86"
                  "d3dae15b04bb352fa0f59febfcb4da3e"
                  "67da610697ed5aae4b0fa7a0dd783d29"
                  "61a00ab697367915d23c754bd99e2899",
    },
};

// Temporary output file
static const char *out_path;

static int num_checks;

static int unhex(const char *hex, uint8_t *dst, int size)
{
    int len = strlen(hex) / 2;
    if (len > size)
        fail("hex string too long\n");
    for (int i = 0; i < len; i++) {
        unsigned byte;
        if (sscanf(hex + 2 * i, "%2x", &byte) != 1)
            fail("invalid hex string\n");
        dst[i] = byte;
    }
    return len;
}

// Every byte of the source depends on its position only, so the expected
// output of a stream follows from the byte range it has to deliver.
static uint8_t source_byte(int64_t pos)
{
    return ((uint32_t)pos * 2654435761u) >> 24;
}

struct source {
    int64_t size;
    int64_t max_read;
    int64_t pos;
};

static int64_t source_read(void *cookie, char *buf, uint64_t nbytes)
{
    struct source *s = cookie;
    int64_t len = MPMIN(s->size - s->pos, s->max_read);
    len = MPCLAMP(len, 0, (int64_t)nbytes);
    for (int64_t i = 0; i < len; i++)
        buf[i] = source_byte(s->pos + i);
    s->pos += len;
    return len;
}

static int64_t source_seek(void *cookie, int64_t offset)
{
    struct source *s = cookie;
    s->pos = offset;
    return offset;
}

static int64_t source_size(void *cookie)
{
    struct source *s = cookie;
    return s->size;
}

static void source_close(void *cookie)
{
    free(cookie);
}

// source://size/max_read/seekable serves size bytes, at most max_read of them
// per read.
static int source_open(void *user_data, char *uri, mpv_stream_cb_info *info)
{
    struct source *s = calloc(1, sizeof(*s));
    if (!s)
        return MPV_ERROR_NOMEM;
    int seekable;
    if (sscanf(uri, "source://%" SCNd64 "/%" SCNd64 "/%d",
               &s->size, &s->max_read, &seekable) != 3)
    {
        free(s);
        return MPV_ERROR_LOADING_FAILED;
    }
    info->cookie = s;
    info->read_fn = source_read;
    info->seek_fn = seekable ? source_seek : NULL;
    info->size_fn = source_size;
    info->close_fn = source_close;
    return 0;
}

// Reference for data that has no published vector. libavutil implements the
// mode with a counter of 64 bits.
static void reference_crypt(const char *key_hex, const char *iv_hex,
                            uint8_t *data, int len)
{
    uint8_t key[BLOCKSIZE], iv[BLOCKSIZE];
    if (unhex(key_hex, key, sizeof(key)) != BLOCKSIZE ||
        unhex(iv_hex, iv, sizeof(iv)) != BLOCKSIZE)
        fail("the reference needs a key and an IV of 128 bits\n");

    struct AVAESCTR *ctr = av_aes_ctr_alloc();
    if (!ctr || av_aes_ctr_init(ctr, key) < 0)
        fail("could not set up the reference\n");
    av_aes_ctr_set_full_iv(ctr, iv);
    av_aes_ctr_crypt(ctr, data, data, len);
    av_aes_ctr_free(ctr);
}

static void compare(const char *name, const uint8_t *data, int64_t data_len,
                    const uint8_t *expected, int64_t len)
{
    for (int64_t i = 0; i < data_len && i < len; i++) {
        if (data[i] != expected[i]) {
            fail("%s: byte %" PRId64 " is 0x%02x, expected 0x%02x\n",
                 name, i, data[i], expected[i]);
        }
    }
    if (data_len != len) {
        fail("%s: got %" PRId64 " bytes, expected %" PRId64 "\n",
             name, data_len, len);
    }
    num_checks++;
}

// Load the URL and return the reason it ended with.
static mpv_end_file_reason load(const char *url, bool fail_on_error)
{
    const char *cmd[] = {"loadfile", url, NULL};
    command(cmd);
    while (1) {
        mpv_event *event = wait_event(fail_on_error);
        if (event->event_id == MPV_EVENT_END_FILE)
            return ((mpv_event_end_file *)event->data)->reason;
    }
}

// Dump the stream and compare it with the expected data.
static void check_stream(const char *url, const uint8_t *expected, int64_t len)
{
    if (load(url, true) != MPV_END_FILE_REASON_EOF)
        fail("%s: dumping failed\n", url);

    // One byte more than expected shows a stream that is too long.
    uint8_t *data = malloc(len + 1);
    FILE *fp = fopen(out_path, "rb");
    if (!data || !fp)
        fail("%s: no output\n", url);
    int64_t data_len = fread(data, 1, len + 1, fp);
    fclose(fp);
    compare(url, data, data_len, expected, len);
    free(data);
}

// Require that the stream fails to open.
static void check_failure(const char *url)
{
    if (load(url, false) != MPV_END_FILE_REASON_ERROR)
        fail("%s: did not fail\n", url);
    num_checks++;
}

// Check the stream of the first len bytes of the input.
static void check_vector(const struct vector *v, const char *input,
                         const uint8_t *expected, int len)
{
    char url[512];
    snprintf(url, sizeof(url), "aes-ctr://%s:%s@hex://%.*s",
             v->key, v->iv, 2 * len, input);
    check_stream(url, expected, len);
}

static void test_vectors(void)
{
    for (int n = 0; n < MP_ARRAY_SIZE(vectors); n++) {
        const struct vector *v = &vectors[n];
        uint8_t plain[64], cipher[64];
        int len = unhex(PLAIN, plain, sizeof(plain));
        if (unhex(v->cipher, cipher, sizeof(cipher)) != len)
            fail("%s: broken vector\n", v->name);

        check_vector(v, v->cipher, plain, len);
        check_vector(v, PLAIN, cipher, len);
        // A stream that ends within a block
        check_vector(v, v->cipher, plain, 37);
    }
}

// The reference has to agree with a vector before it is used.
static void test_reference(void)
{
    const struct vector *v = &vectors[0];
    uint8_t plain[64], data[64];
    int len = unhex(PLAIN, plain, sizeof(plain));
    unhex(v->cipher, data, sizeof(data));
    reference_crypt(v->key, v->iv, data, len);
    compare("reference", data, len, plain, len);
}

// The decrypted source
static uint8_t source_plain[SOURCE_SIZE];

// Check the part of the decrypted source that begins at start.
static void check_source(int64_t start, int64_t max_read, int seekable)
{
    char url[256];
    int pos = 0;
    if (start)
        pos = snprintf(url, sizeof(url), "slice://%" PRId64 "@", start);
    snprintf(url + pos, sizeof(url) - pos,
             "aes-ctr://" SOURCE_KEY ":" SOURCE_IV "@source://%d/%" PRId64
             "/%d", SOURCE_SIZE, max_read, seekable);
    check_stream(url, source_plain + start, SOURCE_SIZE - start);
}

static void test_source(void)
{
    static const int64_t starts[] = {
        0, 1, 15, 16, 17, 4095, 4096, 4097, 131071, 131072, 150001,
        SOURCE_SIZE - 17, SOURCE_SIZE - 16, SOURCE_SIZE - 1,
    };
    static const int64_t max_reads[] = {SOURCE_SIZE, 4099, 7};
    static const char *const buffer_sizes[] = {"4KiB", "128KiB"};

    for (int64_t i = 0; i < SOURCE_SIZE; i++)
        source_plain[i] = source_byte(i);
    reference_crypt(SOURCE_KEY, SOURCE_IV, source_plain, SOURCE_SIZE);

    for (int b = 0; b < MP_ARRAY_SIZE(buffer_sizes); b++) {
        set_property_string("stream-buffer-size", buffer_sizes[b]);
        for (int m = 0; m < MP_ARRAY_SIZE(max_reads); m++) {
            for (int s = 0; s < MP_ARRAY_SIZE(starts); s++)
                check_source(starts[s], max_reads[m], 1);
            check_source(0, max_reads[m], 0);
        }
    }
}

static void test_failures(void)
{
    static const char *const urls[] = {
        // No or empty inner URL
        "aes-ctr://" SOURCE_KEY ":" SOURCE_IV,
        "aes-ctr://" SOURCE_KEY ":" SOURCE_IV "@",
        // Inner stream that does not open
        "aes-ctr://" SOURCE_KEY ":" SOURCE_IV "@source://none",
        // Key of 120 and of 136 bits, and one that is not hex
        "aes-ctr://00112233445566778899aabbccddee:" SOURCE_IV "@hex://00",
        "aes-ctr://00112233445566778899aabbccddeeff00:" SOURCE_IV "@hex://00",
        "aes-ctr://00112233445566778899aabbccddeefg:" SOURCE_IV "@hex://00",
        // Odd number of digits
        "aes-ctr://00112233445566778899aabbccddeeff0:" SOURCE_IV "@hex://00",
        "aes-ctr://" SOURCE_KEY ":a0a1a2a3a4a5a6a700000000fffffff00@hex://00",
        // No IV, and one of 64 bits
        "aes-ctr://" SOURCE_KEY "@hex://00",
        "aes-ctr://" SOURCE_KEY ":a0a1a2a3a4a5a6a7@hex://00",
        // A stream without seeking cannot be sliced
        "slice://16@aes-ctr://" SOURCE_KEY ":" SOURCE_IV "@source://64/64/0",
    };

    for (int n = 0; n < MP_ARRAY_SIZE(urls); n++)
        check_failure(urls[n]);
}

static void cleanup(void)
{
    exit_cleanup();
    if (out_path && *out_path)
        unlink(out_path);
}

int main(int argc, char *argv[])
{
    ctx = mpv_create();
    if (!ctx)
        return 1;

    atexit(cleanup);

    out_path = temp_path();
    set_property_string("stream-dump", out_path);
    initialize();

    if (mpv_stream_cb_add_ro(ctx, "source", NULL, source_open) < 0)
        fail("could not register the source\n");

    const char *fmt = "================ TEST: %s ================\n";
    printf(fmt, "test_reference");
    test_reference();
    printf(fmt, "test_vectors");
    test_vectors();
    printf(fmt, "test_source");
    test_source();
    // Last, the errors it logs may arrive late.
    printf(fmt, "test_failures");
    test_failures();
    printf("================ SHUTDOWN ================\n");
    printf("%d checks\n", num_checks);

    command_string("quit");
    while (wait_event(false)->event_id != MPV_EVENT_SHUTDOWN) {}

    return 0;
}
