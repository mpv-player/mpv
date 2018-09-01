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

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <strings.h>
#include <assert.h>

#include <libavutil/common.h>
#include "osdep/io.h"

#include "mpv_talloc.h"

#include "config.h"

#include "common/common.h"
#include "common/global.h"
#include "misc/bstr.h"
#include "misc/thread_tools.h"
#include "common/msg.h"
#include "options/options.h"
#include "options/path.h"
#include "osdep/timer.h"
#include "stream.h"

#include "options/m_option.h"
#include "options/m_config.h"

#define TOTAL_BUFFER_SIZE STREAM_MAX_BUFFER_SIZE

extern const stream_info_t stream_info_null;
extern const stream_info_t stream_info_memory;
extern const stream_info_t stream_info_mf;
extern const stream_info_t stream_info_ffmpeg;
extern const stream_info_t stream_info_ffmpeg_unsafe;
extern const stream_info_t stream_info_avdevice;
extern const stream_info_t stream_info_file;
extern const stream_info_t stream_info_edl;
extern const stream_info_t stream_info_libarchive;
extern const stream_info_t stream_info_cb;

static const stream_info_t *const stream_list[] = {
    &stream_info_ffmpeg,
    &stream_info_ffmpeg_unsafe,
    &stream_info_avdevice,
#if HAVE_LIBARCHIVE
    &stream_info_libarchive,
#endif
    &stream_info_memory,
    &stream_info_null,
    &stream_info_mf,
    &stream_info_edl,
    &stream_info_file,
    &stream_info_cb,
    NULL
};

static bool stream_seek_unbuffered(stream_t *s, int64_t newpos);

// return -1 if not hex char
static int hex2dec(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return 10 + c - 'A';
    if (c >= 'a' && c <= 'f')
        return 10 + c - 'a';
    return -1;
}

// Replace escape sequences in an URL (or a part of an URL)
void mp_url_unescape_inplace(char *url)
{
    for (int len = strlen(url), i = 0, o = 0; i <= len;) {
        if ((url[i] != '%') || (i > len - 3)) {  // %NN can't start after len-3
            url[o++] = url[i++];
            continue;
        }

        int msd = hex2dec(url[i + 1]),
            lsd = hex2dec(url[i + 2]);

        if (msd >= 0 && lsd >= 0) {
            url[o++] = 16 * msd + lsd;
            i += 3;
        } else {
            url[o++] = url[i++];
            url[o++] = url[i++];
            url[o++] = url[i++];
        }
    }
}

static const char hex_digits[] = "0123456789ABCDEF";


static const char url_default_ok[] = "abcdefghijklmnopqrstuvwxyz"
                                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                     "0123456789"
                                     "-._~";

// Escape according to http://tools.ietf.org/html/rfc3986#section-2.1
// Only unreserved characters are not escaped.
// The argument ok (if not NULL) is as follows:
//      ok[0] != '~': additional characters that are not escaped
//      ok[0] == '~': do not escape anything but these characters
//                    (can't override the unreserved characters, which are
//                     never escaped)
char *mp_url_escape(void *talloc_ctx, const char *url, const char *ok)
{
    char *rv = talloc_size(talloc_ctx, strlen(url) * 3 + 1);
    char *out = rv;
    bool negate = ok && ok[0] == '~';

    for (char c; (c = *url); url++) {
        bool as_is = negate ? !strchr(ok + 1, c)
                            : (strchr(url_default_ok, c) || (ok && strchr(ok, c)));
        if (as_is) {
            *out++ = c;
        } else {
            unsigned char v = c;
            *out++ = '%';
            *out++ = hex_digits[v / 16];
            *out++ = hex_digits[v % 16];
        }
    }

    *out = 0;
    return rv;
}

static stream_t *new_stream(void)
{
    return talloc_zero_size(NULL, sizeof(stream_t) + TOTAL_BUFFER_SIZE);
}

static const char *match_proto(const char *url, const char *proto)
{
    int l = strlen(proto);
    if (l > 0) {
        if (strncasecmp(url, proto, l) == 0 && strncmp("://", url + l, 3) == 0)
            return url + l + 3;
    } else if (!mp_is_url(bstr0(url))) {
        return url; // pure filenames
    }
    return NULL;
}

static int open_internal(const stream_info_t *sinfo, const char *url, int flags,
                         struct mp_cancel *c, struct mpv_global *global,
                         struct stream **ret)
{
    if (!sinfo->is_safe && (flags & STREAM_SAFE_ONLY))
        return STREAM_UNSAFE;
    if (!sinfo->is_network && (flags & STREAM_NETWORK_ONLY))
        return STREAM_UNSAFE;

    const char *path = url;
    for (int n = 0; sinfo->protocols && sinfo->protocols[n]; n++) {
        path = match_proto(url, sinfo->protocols[n]);
        if (path)
            break;
    }

    if (!path)
        return STREAM_NO_MATCH;

    stream_t *s = new_stream();
    s->log = mp_log_new(s, global->log, sinfo->name);
    s->info = sinfo;
    s->cancel = c;
    s->global = global;
    s->url = talloc_strdup(s, url);
    s->path = talloc_strdup(s, path);
    s->is_network = sinfo->is_network;
    s->mode = flags & (STREAM_READ | STREAM_WRITE);

    if (global->config) {
        int opt;
        mp_read_option_raw(global, "access-references", &m_option_type_flag, &opt);
        s->access_references = opt;
    }

    MP_VERBOSE(s, "Opening %s\n", url);

    if ((s->mode & STREAM_WRITE) && !sinfo->can_write) {
        MP_DBG(s, "No write access implemented.\n");
        talloc_free(s);
        return STREAM_NO_MATCH;
    }

    int r = (sinfo->open)(s);
    if (r != STREAM_OK) {
        talloc_free(s);
        return r;
    }

    if (!s->read_chunk)
        s->read_chunk = 4 * STREAM_BUFFER_SIZE;

    assert(s->seekable == !!s->seek);

    if (s->mime_type)
        MP_VERBOSE(s, "Mime-type: '%s'\n", s->mime_type);

    MP_DBG(s, "Stream opened successfully.\n");

    *ret = s;
    return STREAM_OK;
}

struct stream *stream_create(const char *url, int flags,
                             struct mp_cancel *c, struct mpv_global *global)
{
    struct mp_log *log = mp_log_new(NULL, global->log, "!stream");
    struct stream *s = NULL;
    assert(url);

    if (strlen(url) > INT_MAX / 8)
        goto done;

    // Open stream proper
    bool unsafe = false;
    for (int i = 0; stream_list[i]; i++) {
        int r = open_internal(stream_list[i], url, flags, c, global, &s);
        if (r == STREAM_OK)
            break;
        if (r == STREAM_NO_MATCH || r == STREAM_UNSUPPORTED)
            continue;
        if (r == STREAM_UNSAFE) {
            unsafe = true;
            continue;
        }
        if (r != STREAM_OK) {
            if (!mp_cancel_test(c))
                mp_err(log, "Failed to open %s.\n", url);
            goto done;
        }
    }

    if (!s && unsafe) {
        mp_err(log, "\nRefusing to load potentially unsafe URL from a playlist.\n"
               "Use --playlist=file or the --load-unsafe-playlists option to "
               "load it anyway.\n\n");
        goto done;
    }

    if (!s) {
        mp_err(log, "No protocol handler found to open URL %s\n", url);
        mp_err(log, "The protocol is either unsupported, or was disabled "
                    "at compile-time.\n");
        goto done;
    }

done:
    talloc_free(log);
    return s;
}

struct stream *stream_open(const char *filename, struct mpv_global *global)
{
    return stream_create(filename, STREAM_READ, NULL, global);
}

stream_t *open_output_stream(const char *filename, struct mpv_global *global)
{
    return stream_create(filename, STREAM_WRITE, NULL, global);
}

// Read function bypassing the local stream buffer. This will not write into
// s->buffer, but into buf[0..len] instead.
// Returns 0 on error or EOF, and length of bytes read on success.
// Partial reads are possible, even if EOF is not reached.
static int stream_read_unbuffered(stream_t *s, void *buf, int len)
{
    int res = 0;
    s->buf_pos = s->buf_len = 0;
    // we will retry even if we already reached EOF previously.
    if (s->fill_buffer && !mp_cancel_test(s->cancel))
        res = s->fill_buffer(s, buf, len);
    if (res <= 0) {
        s->eof = 1;
        return 0;
    }
    // When reading succeeded we are obviously not at eof.
    s->eof = 0;
    s->pos += res;
    return res;
}

static int stream_fill_buffer_by(stream_t *s, int64_t len)
{
    len = MPMIN(len, s->read_chunk);
    len = MPMAX(len, STREAM_BUFFER_SIZE);
    len = stream_read_unbuffered(s, s->buffer, len);
    s->buf_pos = 0;
    s->buf_len = len;
    return s->buf_len;
}

int stream_fill_buffer(stream_t *s)
{
    return stream_fill_buffer_by(s, STREAM_BUFFER_SIZE);
}

// Read between 1..buf_size bytes of data, return how much data has been read.
// Return 0 on EOF, error, or if buf_size was 0.
int stream_read_partial(stream_t *s, char *buf, int buf_size)
{
    assert(s->buf_pos <= s->buf_len);
    assert(buf_size >= 0);
    if (s->buf_pos == s->buf_len && buf_size > 0) {
        s->buf_pos = s->buf_len = 0;
        // Do a direct read
        // Also, small reads will be more efficient with buffering & copying
        if (buf_size >= STREAM_BUFFER_SIZE)
            return stream_read_unbuffered(s, buf, buf_size);
        if (!stream_fill_buffer(s))
            return 0;
    }
    int len = FFMIN(buf_size, s->buf_len - s->buf_pos);
    memcpy(buf, &s->buffer[s->buf_pos], len);
    s->buf_pos += len;
    if (len > 0)
        s->eof = 0;
    return len;
}

int stream_read(stream_t *s, char *mem, int total)
{
    int len = total;
    while (len > 0) {
        int read = stream_read_partial(s, mem, len);
        if (read <= 0)
            break; // EOF
        mem += read;
        len -= read;
    }
    total -= len;
    if (total > 0)
        s->eof = 0;
    return total;
}

// Read ahead at most len bytes without changing the read position. Return a
// pointer to the internal buffer, starting from the current read position.
// Can read ahead at most STREAM_MAX_BUFFER_SIZE bytes.
// The returned buffer becomes invalid on the next stream call, and you must
// not write to it.
struct bstr stream_peek(stream_t *s, int len)
{
    assert(len >= 0);
    assert(len <= STREAM_MAX_BUFFER_SIZE);
    if (s->buf_len - s->buf_pos < len) {
        // Move to front to guarantee we really can read up to max size.
        int buf_valid = s->buf_len - s->buf_pos;
        memmove(s->buffer, &s->buffer[s->buf_pos], buf_valid);
        // Fill rest of the buffer.
        while (buf_valid < len) {
            int chunk = MPMAX(len - buf_valid, STREAM_BUFFER_SIZE);
            assert(buf_valid + chunk <= TOTAL_BUFFER_SIZE);
            int read = stream_read_unbuffered(s, &s->buffer[buf_valid], chunk);
            if (read == 0)
                break; // EOF
            buf_valid += read;
        }
        s->buf_pos = 0;
        s->buf_len = buf_valid;
        if (s->buf_len)
            s->eof = 0;
    }
    return (bstr){.start = &s->buffer[s->buf_pos],
                  .len = FFMIN(len, s->buf_len - s->buf_pos)};
}

int stream_write_buffer(stream_t *s, unsigned char *buf, int len)
{
    int rd;
    if (!s->write_buffer)
        return -1;
    rd = s->write_buffer(s, buf, len);
    if (rd < 0)
        return -1;
    s->pos += rd;
    assert(rd == len && "stream_write_buffer(): unexpected short write");
    return rd;
}

// Drop len bytes form input, possibly reading more until all is skipped. If
// EOF or an error was encountered before all could be skipped, return false,
// otherwise return true.
static bool stream_skip_read(struct stream *s, int64_t len)
{
    while (len > 0) {
        unsigned int left = s->buf_len - s->buf_pos;
        if (!left) {
            if (!stream_fill_buffer_by(s, left))
                return false;
            continue;
        }
        unsigned skip = MPMIN(len, left);
        s->buf_pos += skip;
        len -= skip;
    }
    return true;
}

// Drop the internal buffer. Note that this will advance the stream position
// (as seen by stream_tell()), because the real stream position is ahead of the
// logical stream position by the amount of buffered but not yet read data.
void stream_drop_buffers(stream_t *s)
{
    s->pos = stream_tell(s);
    s->buf_pos = s->buf_len = 0;
    s->eof = 0;
}

// Seek function bypassing the local stream buffer.
static bool stream_seek_unbuffered(stream_t *s, int64_t newpos)
{
    if (newpos != s->pos) {
        if (newpos > s->pos && !s->seekable) {
            MP_ERR(s, "Cannot seek forward in this stream\n");
            return false;
        }
        if (newpos < s->pos && !s->seekable) {
            MP_ERR(s, "Cannot seek backward in linear streams!\n");
            return false;
        }
        if (s->seek(s, newpos) <= 0) {
            MP_ERR(s, "Seek failed\n");
            return false;
        }
        stream_drop_buffers(s);
        s->pos = newpos;
    }
    return true;
}

bool stream_seek(stream_t *s, int64_t pos)
{
    MP_TRACE(s, "seek to %lld\n", (long long)pos);

    s->eof = 0; // eof should be set only on read; seeking always clears it

    if (pos == stream_tell(s))
        return true;

    if (pos < 0) {
        MP_ERR(s, "Invalid seek to negative position %lld!\n", (long long)pos);
        pos = 0;
    }
    if (pos < s->pos) {
        int64_t x = pos - (s->pos - (int)s->buf_len);
        if (x >= 0) {
            s->buf_pos = x;
            assert(s->buf_pos <= s->buf_len);
            return true;
        }
    }

    if (s->mode == STREAM_WRITE)
        return s->seekable && s->seek(s, pos);

    int64_t newpos = pos;

    MP_TRACE(s, "Seek from %" PRId64 " to %" PRId64
             " (with offset %d)\n", s->pos, pos, (int)(pos - newpos));

    if (pos >= s->pos && !s->seekable && s->fast_skip) {
        // skipping is handled by generic code below
    } else if (!stream_seek_unbuffered(s, newpos)) {
        return false;
    }

    return stream_skip_read(s, pos - stream_tell(s));
}

bool stream_skip(stream_t *s, int64_t len)
{
    int64_t target = stream_tell(s) + len;
    if (len < 0)
        return stream_seek(s, target);
    if (len > 2 * STREAM_BUFFER_SIZE && s->seekable) {
        // Seek to 1 byte before target - this is the only way to distinguish
        // skip-to-EOF and skip-past-EOF in general. Successful seeking means
        // absolutely nothing, so test by doing a real read of the last byte.
        if (!stream_seek(s, target - 1))
            return false;
        stream_read_char(s);
        return !stream_eof(s) && stream_tell(s) == target;
    }
    return stream_skip_read(s, len);
}

int stream_control(stream_t *s, int cmd, void *arg)
{
    return s->control ? s->control(s, cmd, arg) : STREAM_UNSUPPORTED;
}

// Return the current size of the stream, or a negative value if unknown.
int64_t stream_get_size(stream_t *s)
{
    int64_t size = -1;
    if (stream_control(s, STREAM_CTRL_GET_SIZE, &size) != STREAM_OK)
        size = -1;
    return size;
}

void free_stream(stream_t *s)
{
    if (!s)
        return;

    if (s->close)
        s->close(s);
    talloc_free(s);
}

stream_t *open_memory_stream(void *data, int len)
{
    assert(len >= 0);
    struct mpv_global *dummy = talloc_zero(NULL, struct mpv_global);
    dummy->log = mp_null_log;
    stream_t *s = stream_open("memory://", dummy);
    assert(s);
    talloc_steal(s, dummy);
    stream_control(s, STREAM_CTRL_SET_CONTENTS, &(bstr){data, len});
    return s;
}

static uint16_t stream_read_word_endian(stream_t *s, bool big_endian)
{
    unsigned int y = stream_read_char(s);
    y = (y << 8) | stream_read_char(s);
    if (!big_endian)
        y = ((y >> 8) & 0xFF) | (y << 8);
    return y;
}

// Read characters until the next '\n' (including), or until the buffer in s is
// exhausted.
static int read_characters(stream_t *s, uint8_t *dst, int dstsize, int utf16)
{
    if (utf16 == 1 || utf16 == 2) {
        uint8_t *cur = dst;
        while (1) {
            if ((cur - dst) + 8 >= dstsize) // PUT_UTF8 writes max. 8 bytes
                return -1; // line too long
            uint32_t c;
            uint8_t tmp;
            GET_UTF16(c, stream_read_word_endian(s, utf16 == 2), return -1;)
            if (s->eof)
                break; // legitimate EOF; ignore the case of partial reads
            PUT_UTF8(c, tmp, *cur++ = tmp;)
            if (c == '\n')
                break;
        }
        return cur - dst;
    } else {
        if (s->buf_pos >= s->buf_len)
            stream_fill_buffer(s);
        uint8_t *src = s->buffer + s->buf_pos;
        int src_len = s->buf_len - s->buf_pos;
        uint8_t *end = memchr(src, '\n', src_len);
        int len = end ? end - src + 1 : src_len;
        if (len > dstsize)
            return -1; // line too long
        memcpy(dst, src, len);
        s->buf_pos += len;
        return len;
    }
}

// On error, or if the line is larger than max-1, return NULL and unset s->eof.
// On EOF, return NULL, and s->eof will be set.
// Otherwise, return the line (including \n or \r\n at the end of the line).
// If the return value is non-NULL, it's always the same as mem.
// utf16: 0: UTF8 or 8 bit legacy, 1: UTF16-LE, 2: UTF16-BE
unsigned char *stream_read_line(stream_t *s, unsigned char *mem, int max,
                                int utf16)
{
    if (max < 1)
        return NULL;
    int read = 0;
    while (1) {
        // Reserve 1 byte of ptr for terminating \0.
        int l = read_characters(s, &mem[read], max - read - 1, utf16);
        if (l < 0 || memchr(&mem[read], '\0', l)) {
            MP_WARN(s, "error reading line\n");
            s->eof = false;
            return NULL;
        }
        read += l;
        if (l == 0 || (read > 0 && mem[read - 1] == '\n'))
            break;
    }
    mem[read] = '\0';
    if (s->eof && read == 0) // legitimate EOF
        return NULL;
    return mem;
}

static const char *const bom[3] = {"\xEF\xBB\xBF", "\xFF\xFE", "\xFE\xFF"};

// Return utf16 argument for stream_read_line
int stream_skip_bom(struct stream *s)
{
    bstr data = stream_peek(s, 4);
    for (int n = 0; n < 3; n++) {
        if (bstr_startswith0(data, bom[n])) {
            stream_skip(s, strlen(bom[n]));
            return n;
        }
    }
    return -1; // default to 8 bit codepages
}

// Read the rest of the stream into memory (current pos to EOF), and return it.
//  talloc_ctx: used as talloc parent for the returned allocation
//  max_size: must be set to >0. If the file is larger than that, it is treated
//            as error. This is a minor robustness measure.
//  returns: stream contents, or .start/.len set to NULL on error
// If the file was empty, but no error happened, .start will be non-NULL and
// .len will be 0.
// For convenience, the returned buffer is padded with a 0 byte. The padding
// is not included in the returned length.
struct bstr stream_read_complete(struct stream *s, void *talloc_ctx,
                                 int max_size)
{
    if (max_size > 1000000000)
        abort();

    int bufsize;
    int total_read = 0;
    int padding = 1;
    char *buf = NULL;
    int64_t size = stream_get_size(s) - stream_tell(s);
    if (size > max_size)
        return (struct bstr){NULL, 0};
    if (size > 0)
        bufsize = size + padding;
    else
        bufsize = 1000;
    while (1) {
        buf = talloc_realloc_size(talloc_ctx, buf, bufsize);
        int readsize = stream_read(s, buf + total_read, bufsize - total_read);
        total_read += readsize;
        if (total_read < bufsize)
            break;
        if (bufsize > max_size) {
            talloc_free(buf);
            return (struct bstr){NULL, 0};
        }
        bufsize = FFMIN(bufsize + (bufsize >> 1), max_size + padding);
    }
    buf = talloc_realloc_size(talloc_ctx, buf, total_read + padding);
    memset(&buf[total_read], 0, padding);
    return (struct bstr){buf, total_read};
}

struct bstr stream_read_file(const char *filename, void *talloc_ctx,
                             struct mpv_global *global, int max_size)
{
    struct bstr res = {0};
    char *fname = mp_get_user_path(NULL, global, filename);
    stream_t *s = stream_open(fname, global);
    if (s) {
        res = stream_read_complete(s, talloc_ctx, max_size);
        free_stream(s);
    }
    talloc_free(fname);
    return res;
}

char **stream_get_proto_list(void)
{
    char **list = NULL;
    int num = 0;
    for (int i = 0; stream_list[i]; i++) {
        const stream_info_t *stream_info = stream_list[i];

        if (!stream_info->protocols)
            continue;

        for (int j = 0; stream_info->protocols[j]; j++) {
            if (*stream_info->protocols[j] == '\0')
               continue;

            MP_TARRAY_APPEND(NULL, list, num,
                                talloc_strdup(NULL, stream_info->protocols[j]));
        }
    }
    MP_TARRAY_APPEND(NULL, list, num, NULL);
    return list;
}

void stream_print_proto_list(struct mp_log *log)
{
    int count = 0;

    mp_info(log, "Protocols:\n\n");
    char **list = stream_get_proto_list();
    for (int i = 0; list[i]; i++) {
        mp_info(log, " %s://\n", list[i]);
        count++;
        talloc_free(list[i]);
    }
    talloc_free(list);
    mp_info(log, "\nTotal: %d protocols\n", count);
}

bool stream_has_proto(const char *proto)
{
    for (int i = 0; stream_list[i]; i++) {
        const stream_info_t *stream_info = stream_list[i];

        for (int j = 0; stream_info->protocols && stream_info->protocols[j]; j++) {
            if (strcmp(stream_info->protocols[j], proto) == 0)
                return true;
        }
    }

    return false;
}
