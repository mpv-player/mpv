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
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include <strings.h>
#include <assert.h>

#include <libavutil/common.h>
#include "osdep/atomics.h"
#include "osdep/io.h"

#include "talloc.h"

#include "config.h"

#include "common/common.h"
#include "common/global.h"
#include "misc/bstr.h"
#include "common/msg.h"
#include "options/options.h"
#include "options/path.h"
#include "osdep/timer.h"
#include "stream.h"

#include "options/m_option.h"
#include "options/m_config.h"

#ifdef __MINGW32__
#include <windows.h>
#else
#include <poll.h>
#endif

// Includes additional padding in case sizes get rounded up by sector size.
#define TOTAL_BUFFER_SIZE (STREAM_MAX_BUFFER_SIZE + STREAM_MAX_SECTOR_SIZE)

extern const stream_info_t stream_info_cdda;
extern const stream_info_t stream_info_dvb;
extern const stream_info_t stream_info_tv;
extern const stream_info_t stream_info_pvr;
extern const stream_info_t stream_info_smb;
extern const stream_info_t stream_info_null;
extern const stream_info_t stream_info_memory;
extern const stream_info_t stream_info_mf;
extern const stream_info_t stream_info_ffmpeg;
extern const stream_info_t stream_info_ffmpeg_unsafe;
extern const stream_info_t stream_info_avdevice;
extern const stream_info_t stream_info_file;
extern const stream_info_t stream_info_ifo;
extern const stream_info_t stream_info_ifo_dvdnav;
extern const stream_info_t stream_info_dvd;
extern const stream_info_t stream_info_dvdnav;
extern const stream_info_t stream_info_bdmv_dir;
extern const stream_info_t stream_info_bluray;
extern const stream_info_t stream_info_bdnav;
extern const stream_info_t stream_info_rar;
extern const stream_info_t stream_info_edl;

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
#if HAVE_TV
    &stream_info_tv,
#endif
#if HAVE_PVR
    &stream_info_pvr,
#endif
#if HAVE_LIBSMBCLIENT
    &stream_info_smb,
#endif
#if HAVE_DVDREAD
    &stream_info_ifo,
    &stream_info_dvd,
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

    &stream_info_memory,
    &stream_info_null,
    &stream_info_mf,
    &stream_info_edl,
    &stream_info_rar,
    &stream_info_file,
    NULL
};

static bool stream_seek_unbuffered(stream_t *s, int64_t newpos);

static int from_hex(unsigned char c)
{
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= '0' && c <= '9')
        return c - '0';
    return -1;
}

// Replace escape sequences in an URL (or a part of an URL)
void mp_url_unescape_inplace(char *buf)
{
    int len = strlen(buf);
    int o = 0;
    for (int i = 0; i < len; i++) {
        unsigned char c = buf[i];
        if (c == '%' && i + 2 < len) { //must have 2 more chars
            int c1 = from_hex(buf[i + 1]);
            int c2 = from_hex(buf[i + 2]);
            if (c1 >= 0 && c2 >= 0) {
                c = c1 * 16 + c2;
                i = i + 2; //only skip next 2 chars if valid esc
            }
        }
        buf[o++] = c;
    }
    buf[o++] = '\0';
}

// Escape according to http://tools.ietf.org/html/rfc3986#section-2.1
// Only unreserved characters are not escaped.
// The argument ok (if not NULL) is as follows:
//      ok[0] != '~': additional characters that are not escaped
//      ok[0] == '~': do not escape anything but these characters
//                    (can't override the unreserved characters, which are
//                     never escaped)
char *mp_url_escape(void *talloc_ctx, const char *s, const char *ok)
{
    int len = strlen(s);
    char *buf = talloc_array(talloc_ctx, char, len * 3 + 1);
    int o = 0;
    for (int i = 0; i < len; i++) {
        unsigned char c = s[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || strchr("-._~", c) ||
            (ok && ((ok[0] != '~') == !!strchr(ok, c))))
        {
            buf[o++] = c;
        } else {
            const char hex[] = "0123456789ABCDEF";
            buf[o++] = '%';
            buf[o++] = hex[c / 16];
            buf[o++] = hex[c % 16];
        }
    }
    buf[o++] = '\0';
    return buf;
}

static const char *find_url_opt(struct stream *s, const char *opt)
{
    for (int n = 0; s->info->url_options && s->info->url_options[n]; n++) {
        const char *entry = s->info->url_options[n];
        const char *t = strchr(entry, '=');
        assert(t);
        if (strncmp(opt, entry, t - entry) == 0)
            return t + 1;
    }
    return NULL;
}

static bstr split_next(bstr *s, char end, const char *delim)
{
    int idx = bstrcspn(*s, delim);
    if (end && (idx >= s->len || s->start[idx] != end))
        return (bstr){0};
    bstr r = bstr_splice(*s, 0, idx);
    *s = bstr_cut(*s, idx + (end ? 1 : 0));
    return r;
}

// Parse the stream URL, syntax:
//  proto://  [<username>@]<hostname>[:<port>][/<filename>]
// (the proto:// part is already removed from s->path)
// This code originates from times when http code used this, but now it's
// just relict from other stream implementations reusing this code.
static bool parse_url(struct stream *st, struct m_config *config)
{
    bstr s = bstr0(st->path);
    const char *f_names[4] = {"username", "hostname", "port", "filename"};
    bstr f[4];
    f[0] = split_next(&s, '@', "@:/");
    f[1] = split_next(&s, 0, ":/");
    f[2] = bstr_eatstart0(&s, ":") ? split_next(&s, 0, "/") : (bstr){0};
    f[3] = bstr_eatstart0(&s, "/") ? s : (bstr){0};
    for (int n = 0; n < 4; n++) {
        if (f[n].len) {
            const char *opt = find_url_opt(st, f_names[n]);
            if (!opt) {
                MP_ERR(st, "Stream type '%s' accepts no '%s' field in URLs.\n",
                       st->info->name, f_names[n]);
                return false;
            }
            int r = m_config_set_option(config, bstr0(opt), f[n]);
            if (r < 0) {
                MP_ERR(st, "Error setting stream option: %s\n",
                       m_option_strerror(r));
                return false;
            }
        }
    }
    return true;
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

    const char *path = NULL;
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
    s->opts = global->opts;
    s->cancel = c;
    s->global = global;
    s->url = talloc_strdup(s, url);
    s->path = talloc_strdup(s, path);
    s->allow_caching = true;
    s->is_network = sinfo->is_network;
    s->mode = flags & (STREAM_READ | STREAM_WRITE);

    if ((s->mode & STREAM_WRITE) && !sinfo->can_write) {
        MP_VERBOSE(s, "No write access implemented.\n");
        talloc_free(s);
        return STREAM_NO_MATCH;
    }

    // Parse options
    if (sinfo->priv_size) {
        struct m_obj_desc desc = {
            .priv_size = sinfo->priv_size,
            .priv_defaults = sinfo->priv_defaults,
            .options = sinfo->options,
        };
        if (sinfo->get_defaults)
            desc.priv_defaults = sinfo->get_defaults(s);
        struct m_config *config = m_config_from_obj_desc(s, s->log, &desc);
        s->priv = config->optstruct;
        if (s->info->url_options && !parse_url(s, config)) {
            MP_ERR(s, "URL parsing failed on url %s\n", url);
            talloc_free(s);
            return STREAM_ERROR;
        }
    }

    int r = (sinfo->open)(s);
    if (r != STREAM_OK) {
        talloc_free(s);
        return r;
    }

    if (!s->read_chunk)
        s->read_chunk = 4 * (s->sector_size ? s->sector_size : STREAM_BUFFER_SIZE);

    if (!s->fill_buffer)
        s->allow_caching = false;

    assert(s->seekable == !!s->seek);

    s->uncached_type = s->type;

    MP_VERBOSE(s, "Opened: %s\n", url);

    if (s->mime_type)
        MP_VERBOSE(s, "Mime-type: '%s'\n", s->mime_type);

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

static bool stream_reconnect(stream_t *s)
{
    if (!s->streaming || s->uncached_stream || !s->seekable || !s->cancel)
        return false;

    int64_t pos = s->pos;
    double sleep_secs = 0;
    for (int retry = 0; retry < 6; retry++) {
        if (mp_cancel_wait(s->cancel, sleep_secs))
            break;

        int r = stream_control(s, STREAM_CTRL_RECONNECT, NULL);
        if (r == STREAM_UNSUPPORTED)
            break;
        if (r == STREAM_OK && stream_seek_unbuffered(s, pos) && s->pos == pos) {
            MP_WARN(s, "Reconnected successfully.\n");
            return true;
        }

        MP_WARN(s, "Connection lost! Attempting to reconnect (%d)...\n", retry + 1);

        sleep_secs = MPMAX(sleep_secs, 0.1);
        sleep_secs = MPMIN(sleep_secs * 4, 10.0);
    }
    return false;
}

static void stream_capture_write(stream_t *s, void *buf, size_t len)
{
    if (s->capture_file && len > 0) {
        if (fwrite(buf, len, 1, s->capture_file) < 1) {
            MP_ERR(s, "Error writing capture file: %s\n", mp_strerror(errno));
            stream_set_capture_file(s, NULL);
        }
    }
}

void stream_set_capture_file(stream_t *s, const char *filename)
{
    if (!bstr_equals(bstr0(s->capture_filename), bstr0(filename))) {
        if (s->capture_file)
            fclose(s->capture_file);
        talloc_free(s->capture_filename);
        s->capture_file = NULL;
        s->capture_filename = NULL;
        if (filename) {
            s->capture_file = fopen(filename, "ab");
            if (s->capture_file) {
                s->capture_filename = talloc_strdup(NULL, filename);
                if (s->buf_pos < s->buf_len)
                    stream_capture_write(s, s->buffer, s->buf_len);
            } else {
                MP_ERR(s, "Error opening capture file: %s\n", mp_strerror(errno));
            }
        }
    }
}

// Read function bypassing the local stream buffer. This will not write into
// s->buffer, but into buf[0..len] instead.
// Returns 0 on error or EOF, and length of bytes read on success.
// Partial reads are possible, even if EOF is not reached.
static int stream_read_unbuffered(stream_t *s, void *buf, int len)
{
    int orig_len = len;
    s->buf_pos = s->buf_len = 0;
    // we will retry even if we already reached EOF previously.
    len = s->fill_buffer ? s->fill_buffer(s, buf, len) : -1;
    if (len < 0)
        len = 0;
    if (len == 0) {
        // just in case this is an error e.g. due to network
        // timeout reset and retry
        // do not retry if this looks like proper eof
        int64_t size = -1;
        stream_control(s, STREAM_CTRL_GET_SIZE, &size);
        if (!s->eof && s->pos != size && stream_reconnect(s)) {
            s->eof = 1; // make sure EOF is set to ensure no endless recursion
            return stream_read_unbuffered(s, buf, orig_len);
        }

        s->eof = 1;
        return 0;
    }
    // When reading succeeded we are obviously not at eof.
    s->eof = 0;
    s->pos += len;
    stream_capture_write(s, buf, len);
    return len;
}

static int stream_fill_buffer_by(stream_t *s, int64_t len)
{
    len = MPMIN(len, s->read_chunk);
    len = MPMAX(len, STREAM_BUFFER_SIZE);
    if (s->sector_size)
        len = s->sector_size;
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
// Return 0 on EOF, error, of if buf_size was 0.
int stream_read_partial(stream_t *s, char *buf, int buf_size)
{
    assert(s->buf_pos <= s->buf_len);
    assert(buf_size >= 0);
    if (s->buf_pos == s->buf_len && buf_size > 0) {
        s->buf_pos = s->buf_len = 0;
        // Do a direct read, but only if there's no sector alignment requirement
        // Also, small reads will be more efficient with buffering & copying
        if (!s->sector_size && buf_size >= STREAM_BUFFER_SIZE)
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
            if (s->sector_size)
                chunk = s->sector_size;
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

static bool stream_skip_read(struct stream *s, int64_t len)
{
    while (len > 0) {
        int x = s->buf_len - s->buf_pos;
        if (x == 0) {
            if (!stream_fill_buffer_by(s, len))
                return false; // EOF
            x = s->buf_len - s->buf_pos;
        }
        if (x > len)
            x = len;
        s->buf_pos += x;
        len -= x;
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
            return true;
        }
    }

    if (s->mode == STREAM_WRITE)
        return s->seekable && s->seek(s, pos);

    int64_t newpos = pos;
    if (s->sector_size)
        newpos = (pos / s->sector_size) * s->sector_size;

    MP_TRACE(s, "Seek from %" PRId64 " to %" PRId64
             " (with offset %d)\n", s->pos, pos, (int)(pos - newpos));

    if (pos >= s->pos && !s->seekable && s->fast_skip) {
        // skipping is handled by generic code below
    } else if (!stream_seek_unbuffered(s, newpos)) {
        return false;
    }

    bool r = pos >= s->pos && stream_skip_read(s, pos - s->pos);
    if (!r)
        MP_VERBOSE(s, "Seek to/past EOF: no buffer preloaded.\n");
    s->eof = 0;
    return r;
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

void free_stream(stream_t *s)
{
    if (!s)
        return;

    stream_set_capture_file(s, NULL);

    if (s->close)
        s->close(s);
    free_stream(s->uncached_stream);
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

static stream_t *open_cache(stream_t *orig, const char *name)
{
    stream_t *cache = new_stream();
    cache->uncached_type = orig->uncached_type;
    cache->uncached_stream = orig;
    cache->seekable = true;
    cache->mode = STREAM_READ;
    cache->read_chunk = 4 * STREAM_BUFFER_SIZE;

    cache->url = talloc_strdup(cache, orig->url);
    cache->mime_type = talloc_strdup(cache, orig->mime_type);
    cache->demuxer = talloc_strdup(cache, orig->demuxer);
    cache->lavf_type = talloc_strdup(cache, orig->lavf_type);
    cache->streaming = orig->streaming,
    cache->is_network = orig->is_network;
    cache->opts = orig->opts;
    cache->cancel = orig->cancel;
    cache->global = orig->global;

    cache->log = mp_log_new(cache, cache->global->log, name);

    return cache;
}

static struct mp_cache_opts check_cache_opts(stream_t *stream,
                                             struct mp_cache_opts *opts)
{
    struct mp_cache_opts use_opts = *opts;
    if (use_opts.size == -1)
        use_opts.size = stream->streaming ? use_opts.def_size : 0;
    if (use_opts.size == -2)
        use_opts.size = use_opts.def_size;

    if (stream->mode != STREAM_READ || !stream->allow_caching || use_opts.size < 1)
        use_opts.size = 0;
    return use_opts;
}

bool stream_wants_cache(stream_t *stream, struct mp_cache_opts *opts)
{
    struct mp_cache_opts use_opts = check_cache_opts(stream, opts);
    return use_opts.size > 0;
}

// return 1 on success, 0 if the cache is disabled/not needed, and -1 on error
// or if the cache is disabled
int stream_enable_cache(stream_t **stream, struct mp_cache_opts *opts)
{
    stream_t *orig = *stream;
    struct mp_cache_opts use_opts = check_cache_opts(*stream, opts);

    if (use_opts.size < 1)
        return 0;

    stream_t *fcache = open_cache(orig, "file-cache");
    if (stream_file_cache_init(fcache, orig, &use_opts) <= 0) {
        fcache->uncached_stream = NULL; // don't free original stream
        free_stream(fcache);
        fcache = orig;
    }

    stream_t *cache = open_cache(fcache, "cache");

    int res = stream_cache_init(cache, fcache, &use_opts);
    if (res <= 0) {
        cache->uncached_stream = NULL; // don't free original stream
        free_stream(cache);
        if (fcache != orig)
            free_stream(fcache);
    } else {
        *stream = cache;
    }
    return res;
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
    int64_t size = 0;
    stream_control(s, STREAM_CTRL_GET_SIZE, &size);
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

struct mp_cancel {
    atomic_bool triggered;
#ifdef __MINGW32__
    HANDLE event;
#endif
    int wakeup_pipe[2];
};

static void cancel_destroy(void *p)
{
    struct mp_cancel *c = p;
#ifdef __MINGW32__
    CloseHandle(c->event);
#endif
    close(c->wakeup_pipe[0]);
    close(c->wakeup_pipe[1]);
}

struct mp_cancel *mp_cancel_new(void *talloc_ctx)
{
    struct mp_cancel *c = talloc_ptrtype(talloc_ctx, c);
    talloc_set_destructor(c, cancel_destroy);
    *c = (struct mp_cancel){.triggered = ATOMIC_VAR_INIT(false)};
#ifdef __MINGW32__
    c->event = CreateEventW(NULL, TRUE, FALSE, NULL);
#endif
    mp_make_wakeup_pipe(c->wakeup_pipe);
    return c;
}

// Request abort.
void mp_cancel_trigger(struct mp_cancel *c)
{
    atomic_store(&c->triggered, true);
#ifdef __MINGW32__
    SetEvent(c->event);
#endif
    write(c->wakeup_pipe[1], &(char){0}, 1);
}

// Restore original state. (Allows reusing a mp_cancel.)
void mp_cancel_reset(struct mp_cancel *c)
{
    atomic_store(&c->triggered, false);
#ifdef __MINGW32__
    ResetEvent(c->event);
#endif
    // Flush it fully.
    while (1) {
        int r = read(c->wakeup_pipe[0], &(char[256]){0}, 256);
        if (r < 0 && errno == EINTR)
            continue;
        if (r <= 0)
            break;
    }
}

// Return whether the caller should abort.
// For convenience, c==NULL is allowed.
bool mp_cancel_test(struct mp_cancel *c)
{
    return c ? atomic_load_explicit(&c->triggered, memory_order_relaxed) : false;
}

// Wait until the even is signaled. If the timeout (in seconds) expires, return
// false. timeout==0 polls, timeout<0 waits forever.
#ifdef __MINGW32__
bool mp_cancel_wait(struct mp_cancel *c, double timeout)
{
    return WaitForSingleObject(c->event, timeout < 0 ? INFINITE : timeout * 1000)
            == WAIT_OBJECT_0;
}
#else
bool mp_cancel_wait(struct mp_cancel *c, double timeout)
{
    struct pollfd fd = { .fd = c->wakeup_pipe[0], .events = POLLIN };
    poll(&fd, 1, timeout * 1000);
    return fd.revents & POLLIN;
}
#endif

#ifdef __MINGW32__
void *mp_cancel_get_event(struct mp_cancel *c)
{
    return c->event;
}
#endif

// The FD becomes readable if mp_cancel_test() would return true.
// Don't actually read from it, just use it for poll().
int mp_cancel_get_fd(struct mp_cancel *c)
{
    return c->wakeup_pipe[0];
}

void stream_print_proto_list(struct mp_log *log)
{
    int count = 0;

    mp_info(log, "Protocols:\n\n");
    for (int i = 0; stream_list[i]; i++) {
        const stream_info_t *stream_info = stream_list[i];

        if (!stream_info->protocols)
            continue;

        for (int j = 0; stream_info->protocols[j]; j++) {
            if (*stream_info->protocols[j] == '\0')
               continue;

            mp_info(log, " %s://\n", stream_info->protocols[j]);
            count++;
        }
    }
    mp_info(log, "\nTotal: %d protocols\n", count);
}
