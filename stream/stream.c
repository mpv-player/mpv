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

#include "osdep/io.h"

#include "mpv_talloc.h"

#include "config.h"

#include "common/common.h"
#include "common/global.h"
#include "misc/bstr.h"
#include "misc/thread_tools.h"
#include "common/msg.h"
#include "options/m_config.h"
#include "options/options.h"
#include "options/path.h"
#include "osdep/timer.h"
#include "stream.h"

#include "options/m_option.h"
#include "options/m_config.h"

extern const stream_info_t stream_info_cdda;
extern const stream_info_t stream_info_dvb;
extern const stream_info_t stream_info_null;
extern const stream_info_t stream_info_memory;
extern const stream_info_t stream_info_mf;
extern const stream_info_t stream_info_ffmpeg;
extern const stream_info_t stream_info_ffmpeg_unsafe;
extern const stream_info_t stream_info_avdevice;
extern const stream_info_t stream_info_file;
extern const stream_info_t stream_info_slice;
extern const stream_info_t stream_info_fd;
extern const stream_info_t stream_info_ifo_dvdnav;
extern const stream_info_t stream_info_dvdnav;
extern const stream_info_t stream_info_bdmv_dir;
extern const stream_info_t stream_info_bluray;
extern const stream_info_t stream_info_bdnav;
extern const stream_info_t stream_info_edl;
extern const stream_info_t stream_info_libarchive;
extern const stream_info_t stream_info_cb;

static const stream_info_t *const stream_list[] = {
#if HAVE_CDDA
    &stream_info_cdda,
#endif
    &stream_info_ffmpeg,
    &stream_info_ffmpeg_unsafe,
    &stream_info_avdevice,
#if HAVE_DVBIN
    &stream_info_dvb,
#endif
#if HAVE_DVDNAV
    &stream_info_ifo_dvdnav,
    &stream_info_dvdnav,
#endif
#if HAVE_LIBBLURAY
    &stream_info_bdmv_dir,
    &stream_info_bluray,
    &stream_info_bdnav,
#endif
#if HAVE_LIBARCHIVE
    &stream_info_libarchive,
#endif
    &stream_info_memory,
    &stream_info_null,
    &stream_info_mf,
    &stream_info_edl,
    &stream_info_file,
    &stream_info_slice,
    &stream_info_fd,
    &stream_info_cb,
    NULL
};

// Because of guarantees documented on STREAM_BUFFER_SIZE.
// Half the buffer is used as forward buffer, the other for seek-back.
#define STREAM_MIN_BUFFER_SIZE (STREAM_BUFFER_SIZE * 2)
// Sort of arbitrary; keep *2 of it comfortably within integer limits.
// Must be power of 2.
#define STREAM_MAX_BUFFER_SIZE (512 * 1024 * 1024)

struct stream_opts {
    int64_t buffer_size;
    int load_unsafe_playlists;
};

#define OPT_BASE_STRUCT struct stream_opts

const struct m_sub_options stream_conf = {
    .opts = (const struct m_option[]){
        {"stream-buffer-size", OPT_BYTE_SIZE(buffer_size),
            M_RANGE(STREAM_MIN_BUFFER_SIZE, STREAM_MAX_BUFFER_SIZE)},
        {"load-unsafe-playlists", OPT_FLAG(load_unsafe_playlists)},
        {0}
    },
    .size = sizeof(struct stream_opts),
    .defaults = &(const struct stream_opts){
        .buffer_size = 128 * 1024,
    },
};

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

// src and new are both STREAM_ORIGIN_* values. This checks whether a stream
// with flags "new" can be opened from the "src". On success, return
// new origin, on incompatibility return 0.
static int check_origin(int src, int new)
{
    switch (src) {
    case STREAM_ORIGIN_DIRECT:
    case STREAM_ORIGIN_UNSAFE:
        // Allow anything, but constrain it to the new origin.
        return new;
    case STREAM_ORIGIN_FS:
        // From unix FS, allow all but unsafe.
        if (new == STREAM_ORIGIN_FS || new == STREAM_ORIGIN_NET)
            return new;
        break;
    case STREAM_ORIGIN_NET:
        // Allow only other network links.
        if (new == STREAM_ORIGIN_NET)
            return new;
        break;
    }
    return 0;
}

// Read len bytes from the start position, and wrap around as needed. Limit the
// actually read data to the size of the buffer. Return amount of copied bytes.
//  len: max bytes to copy to dst
//  pos: index into s->buffer[], e.g. s->buf_start is byte 0
//  returns: bytes copied to dst (limited by len and available buffered data)
static int ring_copy(struct stream *s, void *dst, int len, int pos)
{
    assert(len >= 0);

    if (pos < s->buf_start || pos > s->buf_end)
        return 0;

    int copied = 0;
    len = MPMIN(len, s->buf_end - pos);

    if (len && pos <= s->buffer_mask) {
        int copy = MPMIN(len, s->buffer_mask + 1 - pos);
        memcpy(dst, &s->buffer[pos], copy);
        copied += copy;
        len -= copy;
        pos += copy;
    }

    if (len) {
        memcpy((char *)dst + copied, &s->buffer[pos & s->buffer_mask], len);
        copied += len;
    }

    return copied;
}

// Resize the current stream buffer. Uses a larger size if needed to keep data.
// Does nothing if the size is adequate. Calling this with 0 ensures it uses the
// default buffer size if possible.
// The caller must check whether enough data was really allocated.
//  keep: keep at least [buf_end-keep, buf_end] (used for assert()s only)
//  new: new total size of buffer
//  returns: false if buffer allocation failed, true if reallocated or size ok
static bool stream_resize_buffer(struct stream *s, int keep, int new)
{
    assert(keep >= s->buf_end - s->buf_cur);
    assert(keep <= new);

    new = MPMAX(new, s->requested_buffer_size);
    new = MPMIN(new, STREAM_MAX_BUFFER_SIZE);
    new = mp_round_next_power_of_2(new);

    assert(keep <= new); // can't fail (if old buffer size was valid)

    if (new == s->buffer_mask + 1)
        return true;

    int old_pos = s->buf_cur - s->buf_start;
    int old_used_len = s->buf_end - s->buf_start;
    int skip = old_used_len > new ? old_used_len - new : 0;

    MP_DBG(s, "resize stream to %d bytes, drop %d bytes\n", new, skip);

    void *nbuf = ta_alloc_size(s, new);
    if (!nbuf)
        return false; // oom; tolerate it, caller needs to check if required

    int new_len = 0;
    if (s->buffer)
        new_len = ring_copy(s, nbuf, new, s->buf_start + skip);
    assert(new_len == old_used_len - skip);
    assert(old_pos >= skip); // "keep" too low
    assert(old_pos - skip <= new_len);
    s->buf_start = 0;
    s->buf_cur = old_pos - skip;
    s->buf_end = new_len;

    ta_free(s->buffer);

    s->buffer = nbuf;
    s->buffer_mask = new - 1;

    return true;
}

static int stream_create_instance(const stream_info_t *sinfo,
                                  struct stream_open_args *args,
                                  struct stream **ret)
{
    const char *url = args->url;
    int flags = args->flags;

    *ret = NULL;

    const char *path = url;

    if (flags & STREAM_LOCAL_FS_ONLY) {
        if (!sinfo->local_fs)
            return STREAM_NO_MATCH;
    } else {
        for (int n = 0; sinfo->protocols && sinfo->protocols[n]; n++) {
            path = match_proto(url, sinfo->protocols[n]);
            if (path)
                break;
        }

        if (!path)
            return STREAM_NO_MATCH;
    }

    stream_t *s = talloc_zero(NULL, stream_t);
    s->global = args->global;
    struct stream_opts *opts = mp_get_config_group(s, s->global, &stream_conf);
    if (flags & STREAM_SILENT) {
        s->log = mp_null_log;
    } else {
        s->log = mp_log_new(s, s->global->log, sinfo->name);
    }
    s->info = sinfo;
    s->cancel = args->cancel;
    s->url = talloc_strdup(s, url);
    s->path = talloc_strdup(s, path);
    s->mode = flags & (STREAM_READ | STREAM_WRITE);
    s->requested_buffer_size = opts->buffer_size;

    if (flags & STREAM_LESS_NOISE)
        mp_msg_set_max_level(s->log, MSGL_WARN);

    int opt;
    mp_read_option_raw(s->global, "access-references", &m_option_type_flag, &opt);
    s->access_references = opt;

    MP_VERBOSE(s, "Opening %s\n", url);

    if (strlen(url) > INT_MAX / 8) {
        MP_ERR(s, "URL too large.\n");
        talloc_free(s);
        return STREAM_ERROR;
    }

    if ((s->mode & STREAM_WRITE) && !sinfo->can_write) {
        MP_DBG(s, "No write access implemented.\n");
        talloc_free(s);
        return STREAM_NO_MATCH;
    }

    s->stream_origin = flags & STREAM_ORIGIN_MASK; // pass through by default
    if (opts->load_unsafe_playlists) {
        s->stream_origin = STREAM_ORIGIN_DIRECT;
    } else if (sinfo->stream_origin) {
        s->stream_origin = check_origin(s->stream_origin, sinfo->stream_origin);
    }

    if (!s->stream_origin) {
        talloc_free(s);
        return STREAM_UNSAFE;
    }

    int r = STREAM_UNSUPPORTED;
    if (sinfo->open2) {
        r = sinfo->open2(s, args);
    } else if (!args->special_arg) {
        r = (sinfo->open)(s);
    }
    if (r != STREAM_OK) {
        talloc_free(s);
        return r;
    }

    if (!stream_resize_buffer(s, 0, 0)) {
        free_stream(s);
        return STREAM_ERROR;
    }

    assert(s->seekable == !!s->seek);

    if (s->mime_type)
        MP_VERBOSE(s, "Mime-type: '%s'\n", s->mime_type);

    MP_DBG(s, "Stream opened successfully.\n");

    *ret = s;
    return STREAM_OK;
}

int stream_create_with_args(struct stream_open_args *args, struct stream **ret)

{
    assert(args->url);

    int r = STREAM_NO_MATCH;
    *ret = NULL;

    // Open stream proper
    if (args->sinfo) {
        r = stream_create_instance(args->sinfo, args, ret);
    } else {
        for (int i = 0; stream_list[i]; i++) {
            r = stream_create_instance(stream_list[i], args, ret);
            if (r == STREAM_OK)
                break;
            if (r == STREAM_NO_MATCH || r == STREAM_UNSUPPORTED)
                continue;
            if (r == STREAM_UNSAFE)
                continue;
            break;
        }
    }

    if (!*ret && !(args->flags & STREAM_SILENT) && !mp_cancel_test(args->cancel))
    {
        struct mp_log *log = mp_log_new(NULL, args->global->log, "!stream");

        if (r == STREAM_UNSAFE) {
            mp_err(log, "\nRefusing to load potentially unsafe URL from a playlist.\n"
                   "Use the --load-unsafe-playlists option to load it anyway.\n\n");
        } else if (r == STREAM_NO_MATCH || r == STREAM_UNSUPPORTED) {
            mp_err(log, "No protocol handler found to open URL %s\n", args->url);
            mp_err(log, "The protocol is either unsupported, or was disabled "
                        "at compile-time.\n");
        } else {
            mp_err(log, "Failed to open %s.\n", args->url);
        }

        talloc_free(log);
    }

    return r;
}

struct stream *stream_create(const char *url, int flags,
                             struct mp_cancel *c, struct mpv_global *global)
{
    struct stream_open_args args = {
        .global = global,
        .cancel = c,
        .flags = flags,
        .url = url,
    };
    struct stream *s;
    stream_create_with_args(&args, &s);
    return s;
}

stream_t *open_output_stream(const char *filename, struct mpv_global *global)
{
    return stream_create(filename, STREAM_ORIGIN_DIRECT | STREAM_WRITE,
                         NULL, global);
}

// Read function bypassing the local stream buffer. This will not write into
// s->buffer, but into buf[0..len] instead.
// Returns 0 on error or EOF, and length of bytes read on success.
// Partial reads are possible, even if EOF is not reached.
static int stream_read_unbuffered(stream_t *s, void *buf, int len)
{
    assert(len >= 0);
    if (len <= 0)
        return 0;

    int res = 0;
    // we will retry even if we already reached EOF previously.
    if (s->fill_buffer && !mp_cancel_test(s->cancel))
        res = s->fill_buffer(s, buf, len);
    if (res <= 0) {
        s->eof = 1;
        return 0;
    }
    assert(res <= len);
    // When reading succeeded we are obviously not at eof.
    s->eof = 0;
    s->pos += res;
    s->total_unbuffered_read_bytes += res;
    return res;
}

// Ask for having at most "forward" bytes ready to read in the buffer.
// To read everything, you may have to call this in a loop.
//  forward: desired amount of bytes in buffer after s->cur_pos
//  returns: progress (false on EOF or on OOM or if enough data was available)
static bool stream_read_more(struct stream *s, int forward)
{
    assert(forward >= 0);

    int forward_avail = s->buf_end - s->buf_cur;
    if (forward_avail >= forward)
        return false;

    // Avoid that many small reads will lead to many low-level read calls.
    forward = MPMAX(forward, s->requested_buffer_size / 2);
    assert(forward_avail < forward);

    // Keep guaranteed seek-back.
    int buf_old = MPMIN(s->buf_cur - s->buf_start, s->requested_buffer_size / 2);

    if (!stream_resize_buffer(s, buf_old + forward_avail, buf_old + forward))
        return false;

    int buf_alloc = s->buffer_mask + 1;

    assert(s->buf_start <= s->buf_cur);
    assert(s->buf_cur <= s->buf_end);
    assert(s->buf_cur < buf_alloc * 2);
    assert(s->buf_end < buf_alloc * 2);
    assert(s->buf_start < buf_alloc);

    // Note: read as much as possible, even if forward is much smaller. Do
    // this because the stream buffer is supposed to set an approx. minimum
    // read size on it.
    int read = buf_alloc - (buf_old + forward_avail); // free buffer past end

    int pos = s->buf_end & s->buffer_mask;
    read = MPMIN(read, buf_alloc - pos);

    // Note: if wrap-around happens, we need to make two calls. This may
    // affect latency (e.g. waiting for new data on a socket), so do only
    // 1 read call always.
    read = stream_read_unbuffered(s, &s->buffer[pos], read);

    s->buf_end += read;

    // May have overwritten old data.
    if (s->buf_end - s->buf_start >= buf_alloc) {
        assert(s->buf_end >= buf_alloc);

        s->buf_start = s->buf_end - buf_alloc;

        assert(s->buf_start <= s->buf_cur);
        assert(s->buf_cur <= s->buf_end);

        if (s->buf_start >= buf_alloc) {
            s->buf_start -= buf_alloc;
            s->buf_cur -= buf_alloc;
            s->buf_end -= buf_alloc;
        }
    }

    // Must not have overwritten guaranteed old data.
    assert(s->buf_cur - s->buf_start >= buf_old);

    if (s->buf_cur < s->buf_end)
        s->eof = 0;

    return !!read;
}

// Read between 1..buf_size bytes of data, return how much data has been read.
// Return 0 on EOF, error, or if buf_size was 0.
int stream_read_partial(stream_t *s, void *buf, int buf_size)
{
    assert(s->buf_cur <= s->buf_end);
    assert(buf_size >= 0);
    if (s->buf_cur == s->buf_end && buf_size > 0) {
        if (buf_size > (s->buffer_mask + 1) / 2) {
            // Direct read if the buffer is too small anyway.
            stream_drop_buffers(s);
            return stream_read_unbuffered(s, buf, buf_size);
        }
        stream_read_more(s, 1);
    }
    int res = ring_copy(s, buf, buf_size, s->buf_cur);
    s->buf_cur += res;
    return res;
}

// Slow version of stream_read_char(); called by it if the buffer is empty.
int stream_read_char_fallback(stream_t *s)
{
    uint8_t c;
    return stream_read_partial(s, &c, 1) ? c : -256;
}

int stream_read(stream_t *s, void *mem, int total)
{
    int len = total;
    while (len > 0) {
        int read = stream_read_partial(s, mem, len);
        if (read <= 0)
            break; // EOF
        mem = (char *)mem + read;
        len -= read;
    }
    total -= len;
    return total;
}

// Read ahead so that at least forward_size bytes are readable ahead. Returns
// the actual forward amount available (restricted by EOF or buffer limits).
int stream_peek(stream_t *s, int forward_size)
{
    while (stream_read_more(s, forward_size)) {}
    return s->buf_end - s->buf_cur;
}

// Like stream_read(), but do not advance the current position. This may resize
// the buffer to satisfy the read request.
int stream_read_peek(stream_t *s, void *buf, int buf_size)
{
    stream_peek(s, buf_size);
    return ring_copy(s, buf, buf_size, s->buf_cur);
}

int stream_write_buffer(stream_t *s, void *buf, int len)
{
    if (!s->write_buffer)
        return -1;
    int orig_len = len;
    while (len) {
        int w = s->write_buffer(s, buf, len);
        if (w <= 0)
            return -1;
        s->pos += w;
        buf = (char *)buf + w;
        len -= w;
    }
    return orig_len;
}

// Drop len bytes form input, possibly reading more until all is skipped. If
// EOF or an error was encountered before all could be skipped, return false,
// otherwise return true.
static bool stream_skip_read(struct stream *s, int64_t len)
{
    while (len > 0) {
        unsigned int left = s->buf_end - s->buf_cur;
        if (!left) {
            if (!stream_read_more(s, 1))
                return false;
            continue;
        }
        unsigned skip = MPMIN(len, left);
        s->buf_cur += skip;
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
    s->buf_start = s->buf_cur = s->buf_end = 0;
    s->eof = 0;
    stream_resize_buffer(s, 0, 0);
}

// Seek function bypassing the local stream buffer.
static bool stream_seek_unbuffered(stream_t *s, int64_t newpos)
{
    if (newpos != s->pos) {
        MP_VERBOSE(s, "stream level seek from %" PRId64 " to %" PRId64 "\n",
                   s->pos, newpos);

        s->total_stream_seeks++;

        if (newpos > s->pos && !s->seekable) {
            MP_ERR(s, "Cannot seek forward in this stream\n");
            return false;
        }
        if (newpos < s->pos && !s->seekable) {
            MP_ERR(s, "Cannot seek backward in linear streams!\n");
            return false;
        }
        if (s->seek(s, newpos) <= 0) {
            int level = mp_cancel_test(s->cancel) ? MSGL_V : MSGL_ERR;
            MP_MSG(s, level, "Seek failed (to %lld, size %lld)\n",
                   (long long)newpos, (long long)stream_get_size(s));
            return false;
        }
        stream_drop_buffers(s);
        s->pos = newpos;
    }
    return true;
}

bool stream_seek(stream_t *s, int64_t pos)
{
    MP_TRACE(s, "seek request from %" PRId64 " to %" PRId64 "\n",
             stream_tell(s), pos);

    s->eof = 0; // eof should be set only on read; seeking always clears it

    if (pos < 0) {
        MP_ERR(s, "Invalid seek to negative position %lld!\n", (long long)pos);
        pos = 0;
    }

    if (pos <= s->pos) {
        int64_t x = pos - (s->pos - (int)s->buf_end);
        if (x >= (int)s->buf_start) {
            s->buf_cur = x;
            assert(s->buf_cur >= s->buf_start);
            assert(s->buf_cur <= s->buf_end);
            return true;
        }
    }

    if (s->mode == STREAM_WRITE)
        return s->seekable && s->seek(s, pos);

    // Skip data instead of performing a seek in some cases.
    if (pos >= s->pos &&
        ((!s->seekable && s->fast_skip) ||
         pos - s->pos <= s->requested_buffer_size))
    {
        return stream_skip_read(s, pos - stream_tell(s));
    }

    return stream_seek_unbuffered(s, pos);
}

// Like stream_seek(), but strictly prefer skipping data instead of failing, if
// it's a forward-seek.
bool stream_seek_skip(stream_t *s, int64_t pos)
{
    uint64_t cur_pos = stream_tell(s);

    if (cur_pos == pos)
        return true;

    return !s->seekable && pos > cur_pos
        ? stream_skip_read(s, pos - cur_pos)
        : stream_seek(s, pos);
}

int stream_control(stream_t *s, int cmd, void *arg)
{
    return s->control ? s->control(s, cmd, arg) : STREAM_UNSUPPORTED;
}

// Return the current size of the stream, or a negative value if unknown.
int64_t stream_get_size(stream_t *s)
{
    return s->get_size ? s->get_size(s) : -1;
}

void free_stream(stream_t *s)
{
    if (!s)
        return;

    if (s->close)
        s->close(s);
    talloc_free(s);
}

static const char *const bom[3] = {"\xEF\xBB\xBF", "\xFF\xFE", "\xFE\xFF"};

// Return utf16 argument for stream_read_line
int stream_skip_bom(struct stream *s)
{
    char buf[4];
    int len = stream_read_peek(s, buf, sizeof(buf));
    bstr data = {buf, len};
    for (int n = 0; n < 3; n++) {
        if (bstr_startswith0(data, bom[n])) {
            stream_seek_skip(s, stream_tell(s) + strlen(bom[n]));
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
        bufsize = MPMIN(bufsize + (bufsize >> 1), max_size + padding);
    }
    buf = talloc_realloc_size(talloc_ctx, buf, total_read + padding);
    memset(&buf[total_read], 0, padding);
    return (struct bstr){buf, total_read};
}

struct bstr stream_read_file(const char *filename, void *talloc_ctx,
                             struct mpv_global *global, int max_size)
{
    struct bstr res = {0};
    int flags = STREAM_ORIGIN_DIRECT | STREAM_READ | STREAM_LOCAL_FS_ONLY |
                STREAM_LESS_NOISE;
    stream_t *s = stream_create(filename, flags, NULL, global);
    if (s) {
        res = stream_read_complete(s, talloc_ctx, max_size);
        free_stream(s);
    }
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
