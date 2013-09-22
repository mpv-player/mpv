/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifndef __MINGW32__
#include <sys/ioctl.h>
#include <sys/wait.h>
#endif
#include <fcntl.h>
#include <strings.h>
#include <assert.h>

#include <libavutil/intreadwrite.h>
#include <libavutil/common.h>

#include "talloc.h"

#include "config.h"

#include "mpvcore/mp_common.h"
#include "mpvcore/bstr.h"
#include "mpvcore/mp_msg.h"
#include "mpvcore/path.h"
#include "osdep/timer.h"
#include "stream.h"
#include "demux/demux.h"

#include "mpvcore/m_option.h"
#include "mpvcore/m_config.h"

// Includes additional padding in case sizes get rounded up by sector size.
#define TOTAL_BUFFER_SIZE (STREAM_MAX_BUFFER_SIZE + STREAM_MAX_SECTOR_SIZE)

/// We keep these 2 for the gui atm, but they will be removed.
char *cdrom_device = NULL;
char *dvd_device = NULL;
int dvd_title = 0;

struct input_ctx;
static int (*stream_check_interrupt_cb)(struct input_ctx *ctx, int time);
static struct input_ctx *stream_check_interrupt_ctx;

extern const stream_info_t stream_info_vcd;
extern const stream_info_t stream_info_cdda;
extern const stream_info_t stream_info_dvb;
extern const stream_info_t stream_info_tv;
extern const stream_info_t stream_info_radio;
extern const stream_info_t stream_info_pvr;
extern const stream_info_t stream_info_smb;
extern const stream_info_t stream_info_null;
extern const stream_info_t stream_info_memory;
extern const stream_info_t stream_info_mf;
extern const stream_info_t stream_info_ffmpeg;
extern const stream_info_t stream_info_avdevice;
extern const stream_info_t stream_info_file;
extern const stream_info_t stream_info_ifo;
extern const stream_info_t stream_info_dvd;
extern const stream_info_t stream_info_bluray;
extern const stream_info_t stream_info_rar_filter;
extern const stream_info_t stream_info_rar_entry;

static const stream_info_t *const stream_list[] = {
#ifdef CONFIG_VCD
    &stream_info_vcd,
#endif
#ifdef CONFIG_CDDA
    &stream_info_cdda,
#endif
    &stream_info_ffmpeg,
    &stream_info_avdevice,
#ifdef CONFIG_DVBIN
    &stream_info_dvb,
#endif
#ifdef CONFIG_TV
    &stream_info_tv,
#endif
#ifdef CONFIG_RADIO
    &stream_info_radio,
#endif
#ifdef CONFIG_PVR
    &stream_info_pvr,
#endif
#ifdef CONFIG_LIBSMBCLIENT
    &stream_info_smb,
#endif
#ifdef CONFIG_DVDREAD
    &stream_info_ifo,
    &stream_info_dvd,
#endif
#ifdef CONFIG_LIBBLURAY
    &stream_info_bluray,
#endif

    &stream_info_memory,
    &stream_info_null,
    &stream_info_mf,
    &stream_info_rar_filter,
    &stream_info_rar_entry,
    &stream_info_file,
    NULL
};

static int stream_seek_unbuffered(stream_t *s, int64_t newpos);

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
//                     never escaped, and '%', which is always escaped)
char *mp_url_escape(void *talloc_ctx, const char *s, const char *ok)
{
    int len = strlen(s);
    char *buf = talloc_array(talloc_ctx, char, len * 3 + 1);
    int o = 0;
    for (int i = 0; i < len; i++) {
        unsigned char c = s[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || strchr("-._~", c) ||
            (ok && ((ok[0] != '~') == !!strchr(ok, c)) && c != '%'))
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
                mp_tmsg(MSGT_OPEN, MSGL_ERR, "Stream type '%s' accepts no '%s' "
                        "field in URLs.\n", st->info->name, f_names[n]);
                return false;
            }
            int r = m_config_set_option(config, bstr0(opt), f[n]);
            if (r < 0) {
                mp_tmsg(MSGT_OPEN, MSGL_ERR, "Error setting stream option: %s\n",
                        m_option_strerror(r));
                return false;
            }
        }
    }
    return true;
}

static stream_t *new_stream(void)
{
    stream_t *s = talloc_size(NULL, sizeof(stream_t) + TOTAL_BUFFER_SIZE);
    memset(s, 0, sizeof(stream_t));
    return s;
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

static int open_internal(const stream_info_t *sinfo, struct stream *underlying,
                         const char *url, int flags, struct MPOpts *options,
                         struct stream **ret)
{
    if (sinfo->stream_filter != !!underlying)
        return STREAM_NO_MATCH;
    if (sinfo->stream_filter && (flags & STREAM_NO_FILTERS))
        return STREAM_NO_MATCH;

    const char *path = NULL;
    // Stream filters use the original URL, with no protocol matching at all.
    if (!sinfo->stream_filter) {
        for (int n = 0; sinfo->protocols && sinfo->protocols[n]; n++) {
            path = match_proto(url, sinfo->protocols[n]);
            if (path)
                break;
        }

        if (!path)
            return STREAM_NO_MATCH;
    }

    stream_t *s = new_stream();
    s->info = sinfo;
    s->opts = options;
    s->url = talloc_strdup(s, url);
    s->path = talloc_strdup(s, path);
    s->source = underlying;

    // Parse options
    if (sinfo->priv_size) {
        struct m_obj_desc desc = {
            .priv_size = sinfo->priv_size,
            .priv_defaults = sinfo->priv_defaults,
            .options = sinfo->options,
        };
        struct m_config *config = m_config_from_obj_desc(s, &desc);
        s->priv = config->optstruct;
        if (s->info->url_options && !parse_url(s, config)) {
            mp_tmsg(MSGT_OPEN, MSGL_ERR, "URL parsing failed on url %s\n", url);
            talloc_free(s);
            return STREAM_ERROR;
        }
    }

    s->flags = 0;
    s->mode = flags & (STREAM_READ | STREAM_WRITE);
    int r = sinfo->open(s, s->mode);
    if (r != STREAM_OK) {
        talloc_free(s);
        return r;
    }

    if (!s->read_chunk)
        s->read_chunk = 4 * (s->sector_size ? s->sector_size : STREAM_BUFFER_SIZE);

    if (!s->seek)
        s->flags &= ~MP_STREAM_SEEK;
    if (s->seek && !(s->flags & MP_STREAM_SEEK))
        s->flags |= MP_STREAM_SEEK;

    if (s->flags & MP_STREAM_FAST_SKIPPING)
        s->flags |= MP_STREAM_SEEK_FW;

    s->uncached_type = s->type;

    mp_msg(MSGT_OPEN, MSGL_V, "[stream] [%s] %s\n", sinfo->name, url);

    if (s->mime_type)
        mp_msg(MSGT_OPEN, MSGL_V, "Mime-type: '%s'\n", s->mime_type);

    *ret = s;
    return STREAM_OK;
}

struct stream *stream_create(const char *url, int flags, struct MPOpts *options)
{
    struct stream *s = NULL;
    assert(url);

    // Open stream proper
    for (int i = 0; stream_list[i]; i++) {
        int r = open_internal(stream_list[i], NULL, url, flags, options, &s);
        if (r == STREAM_OK)
            break;
        if (r == STREAM_NO_MATCH || r == STREAM_UNSUPPORTED)
            continue;
        if (r != STREAM_OK) {
            mp_tmsg(MSGT_OPEN, MSGL_ERR, "Failed to open %s.\n", url);
            return NULL;
        }
    }

    if (!s) {
        mp_tmsg(MSGT_OPEN, MSGL_ERR, "No stream found to handle url %s\n", url);
        return NULL;
    }

    // Open stream filters
    for (;;) {
        struct stream *new = NULL;
        for (int i = 0; stream_list[i]; i++) {
            int r = open_internal(stream_list[i], s, s->url, flags, options, &new);
            if (r == STREAM_OK)
                break;
        }
        if (!new)
            break;
        s = new;
    }

    return s;
}

struct stream *stream_open(const char *filename, struct MPOpts *options)
{
    return stream_create(filename, STREAM_READ, options);
}

stream_t *open_output_stream(const char *filename, struct MPOpts *options)
{
    return stream_create(filename, STREAM_WRITE, options);
}

static int stream_reconnect(stream_t *s)
{
#define MAX_RECONNECT_RETRIES 5
#define RECONNECT_SLEEP_MS 1000
    if (!s->streaming)
        return 0;
    int64_t pos = s->pos;
    for (int retry = 0; retry < MAX_RECONNECT_RETRIES; retry++) {
        mp_msg(MSGT_STREAM, MSGL_WARN,
               "Connection lost! Attempting to reconnect (%d)...\n", retry + 1);

        if (stream_check_interrupt(retry ? RECONNECT_SLEEP_MS : 0))
            return 0;

        s->eof = 1;
        s->pos = 0;
        s->buf_pos = s->buf_len = 0;

        int r = stream_control(s, STREAM_CTRL_RECONNECT, NULL);
        if (r == STREAM_UNSUPPORTED)
            return 0;
        if (r != STREAM_OK)
            continue;

        if (stream_seek_unbuffered(s, pos) < 0 && s->pos == pos)
            return 1;
    }
    return 0;
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
            s->capture_file = fopen(filename, "wb");
            if (s->capture_file) {
                s->capture_filename = talloc_strdup(NULL, filename);
            } else {
                mp_tmsg(MSGT_GLOBAL, MSGL_ERR,
                        "Error opening capture file: %s\n", strerror(errno));
            }
        }
    }
}

static void stream_capture_write(stream_t *s, void *buf, size_t len)
{
    if (s->capture_file && len > 0) {
        if (fwrite(buf, len, 1, s->capture_file) < 1) {
            mp_tmsg(MSGT_GLOBAL, MSGL_ERR, "Error writing capture file: %s\n",
                    strerror(errno));
            stream_set_capture_file(s, NULL);
        }
    }
}

// Read function bypassing the local stream buffer. This will not write into
// s->buffer, but into buf[0..len] instead.
// Returns < 0 on error, 0 on EOF, and length of bytes read on success.
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
        // do not retry if this looks like proper eof
        if (s->eof || (s->end_pos && s->pos == s->end_pos))
            goto eof_out;

        // just in case this is an error e.g. due to network
        // timeout reset and retry
        if (!stream_reconnect(s))
            goto eof_out;
        // make sure EOF is set to ensure no endless loops
        s->eof = 1;
        return stream_read_unbuffered(s, buf, orig_len);

eof_out:
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
                chunk = STREAM_BUFFER_SIZE;
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

static int stream_skip_read(struct stream *s, int64_t len)
{
    while (len > 0) {
        int x = s->buf_len - s->buf_pos;
        if (x == 0) {
            if (!stream_fill_buffer_by(s, len))
                return 0; // EOF
            x = s->buf_len - s->buf_pos;
        }
        if (x > len)
            x = len;
        s->buf_pos += x;
        len -= x;
    }
    return 1;
}

// Seek function bypassing the local stream buffer.
static int stream_seek_unbuffered(stream_t *s, int64_t newpos)
{
    if (newpos != s->pos) {
        if (!s->seek || !(s->flags & MP_STREAM_SEEK)) {
            mp_tmsg(MSGT_STREAM, MSGL_ERR, "Can not seek in this stream\n");
            return 0;
        }
        if (newpos < s->pos && !(s->flags & MP_STREAM_SEEK_BW)) {
            mp_tmsg(MSGT_STREAM, MSGL_ERR,
                    "Cannot seek backward in linear streams!\n");
            return 1;
        }
        if (s->seek(s, newpos) <= 0) {
            mp_tmsg(MSGT_STREAM, MSGL_ERR, "Seek failed\n");
            return 0;
        }
    }
    s->pos = newpos;
    s->eof = 0; // EOF reset when seek succeeds.
    return -1;
}

// Unlike stream_seek, does not try to seek within local buffer.
// Unlike stream_seek_unbuffered(), it still fills the local buffer.
static int stream_seek_long(stream_t *s, int64_t pos)
{
    s->buf_pos = s->buf_len = 0;
    s->eof = 0;

    if (s->mode == STREAM_WRITE) {
        if (!s->seek || !s->seek(s, pos))
            return 0;
        return 1;
    }

    int64_t newpos = pos;
    if (s->sector_size)
        newpos = (pos / s->sector_size) * s->sector_size;

    mp_msg(MSGT_STREAM, MSGL_DBG3, "Seek from %" PRId64 " to %" PRId64
           " (with offset %d)\n", s->pos, pos, (int)(pos - newpos));

    if (!s->seek && (s->flags & MP_STREAM_FAST_SKIPPING) && pos >= s->pos) {
        // skipping is handled by generic code below
    } else if (stream_seek_unbuffered(s, newpos) >= 0) {
        return 0;
    }

    if (pos >= s->pos && stream_skip_read(s, pos - s->pos) > 0)
        return 1; // success

    // Fill failed, but seek still is a success (partially).
    s->buf_pos = 0;
    s->buf_len = 0;
    s->eof = 0; // eof should be set only on read

    mp_msg(MSGT_STREAM, MSGL_V,
           "stream_seek: Seek to/past EOF: no buffer preloaded.\n");
    return 1;
}

int stream_seek(stream_t *s, int64_t pos)
{

    mp_dbg(MSGT_DEMUX, MSGL_DBG3, "seek to 0x%llX\n", (long long)pos);

    if (pos < 0) {
        mp_msg(MSGT_DEMUX, MSGL_ERR, "Invalid seek to negative position %llx!\n",
               (long long)pos);
        pos = 0;
    }
    if (pos < s->pos) {
        int64_t x = pos - (s->pos - (int)s->buf_len);
        if (x >= 0) {
            s->buf_pos = x;
            s->eof = 0;
            return 1;
        }
    }

    return stream_seek_long(s, pos);
}

int stream_skip(stream_t *s, int64_t len)
{
    int64_t target = stream_tell(s) + len;
    if (len < 0)
        return stream_seek(s, target);
    if (len > 2 * STREAM_BUFFER_SIZE && (s->flags & MP_STREAM_SEEK_FW)) {
        // Seek to 1 byte before target - this is the only way to distinguish
        // skip-to-EOF and skip-past-EOF in general. Successful seeking means
        // absolutely nothing, so test by doing a real read of the last byte.
        int r = stream_seek(s, target - 1);
        if (r) {
            stream_read_char(s);
            return !stream_eof(s) && stream_tell(s) == target;
        }
        return r;
    }
    return stream_skip_read(s, len);
}

int stream_control(stream_t *s, int cmd, void *arg)
{
    if (!s->control)
        return STREAM_UNSUPPORTED;
    return s->control(s, cmd, arg);
}

void stream_update_size(stream_t *s)
{
    uint64_t size;
    if (stream_control(s, STREAM_CTRL_GET_SIZE, &size) == STREAM_OK) {
        if (size > s->end_pos)
            s->end_pos = size;
    }
}

void free_stream(stream_t *s)
{
    if (!s)
        return;

    stream_set_capture_file(s, NULL);

    if (s->close)
        s->close(s);
    free_stream(s->uncached_stream);
    free_stream(s->source);
    talloc_free(s);
}

void stream_set_interrupt_callback(int (*cb)(struct input_ctx *, int),
                                   struct input_ctx *ctx)
{
    stream_check_interrupt_cb = cb;
    stream_check_interrupt_ctx = ctx;
}

int stream_check_interrupt(int time)
{
    if (!stream_check_interrupt_cb) {
        mp_sleep_us(time * 1000);
        return 0;
    }
    return stream_check_interrupt_cb(stream_check_interrupt_ctx, time);
}

stream_t *open_memory_stream(void *data, int len)
{
    assert(len >= 0);
    stream_t *s = stream_open("memory://", NULL);
    assert(s);
    stream_control(s, STREAM_CTRL_SET_CONTENTS, &(bstr){data, len});
    return s;
}

static int stream_enable_cache(stream_t **stream, int64_t size, int64_t min,
                               int64_t seek_limit);

/**
 * \return 1 on success, 0 if the function was interrupted and -1 on error, or
 *         if the cache is disabled
 */
int stream_enable_cache_percent(stream_t **stream, int64_t stream_cache_size,
                                int64_t stream_cache_def_size,
                                float stream_cache_min_percent,
                                float stream_cache_seek_min_percent)
{
    if (stream_cache_size == -1)
        stream_cache_size = (*stream)->streaming ? stream_cache_def_size : 0;

    stream_cache_size = stream_cache_size * 1024; // input is in KiB
    return stream_enable_cache(stream, stream_cache_size,
                               stream_cache_size *
                               (stream_cache_min_percent / 100.0),
                               stream_cache_size *
                               (stream_cache_seek_min_percent / 100.0));
}

static int stream_enable_cache(stream_t **stream, int64_t size, int64_t min,
                               int64_t seek_limit)
{
    stream_t *orig = *stream;

    if (orig->mode != STREAM_READ)
        return 1;

    stream_t *cache = new_stream();
    cache->uncached_type = orig->type;
    cache->uncached_stream = orig;
    cache->flags |= MP_STREAM_SEEK;
    cache->mode = STREAM_READ;
    cache->read_chunk = 4 * STREAM_BUFFER_SIZE;

    cache->url = talloc_strdup(cache, orig->url);
    cache->mime_type = talloc_strdup(cache, orig->mime_type);
    cache->demuxer = talloc_strdup(cache, orig->demuxer);
    cache->lavf_type = talloc_strdup(cache, orig->lavf_type);
    cache->safe_origin = orig->safe_origin;
    cache->opts = orig->opts;
    cache->start_pos = orig->start_pos;
    cache->end_pos = orig->end_pos;

    int res = -1;

#ifdef CONFIG_STREAM_CACHE
    res = stream_cache_init(cache, orig, size, min, seek_limit);
#endif

    if (res <= 0) {
        cache->uncached_stream = NULL; // don't free original stream
        free_stream(cache);
    } else {
        *stream = cache;
    }
    return res;
}

/**
 * Helper function to read 16 bits little-endian and advance pointer
 */
static uint16_t get_le16_inc(const uint8_t **buf)
{
    uint16_t v = AV_RL16(*buf);
    *buf += 2;
    return v;
}

/**
 * Helper function to read 16 bits big-endian and advance pointer
 */
static uint16_t get_be16_inc(const uint8_t **buf)
{
    uint16_t v = AV_RB16(*buf);
    *buf += 2;
    return v;
}

/**
 * Find a newline character in buffer
 * \param buf buffer to search
 * \param len amount of bytes to search in buffer, may not overread
 * \param utf16 chose between UTF-8/ASCII/other and LE and BE UTF-16
 *              0 = UTF-8/ASCII/other, 1 = UTF-16-LE, 2 = UTF-16-BE
 */
static const uint8_t *find_newline(const uint8_t *buf, int len, int utf16)
{
    uint32_t c;
    const uint8_t *end = buf + len;
    switch (utf16) {
    case 0:
        return (uint8_t *)memchr(buf, '\n', len);
    case 1:
        while (buf < end - 1) {
            GET_UTF16(c, buf < end - 1 ? get_le16_inc(&buf) : 0, return NULL;)
            if (buf <= end && c == '\n')
                return buf - 1;
        }
        break;
    case 2:
        while (buf < end - 1) {
            GET_UTF16(c, buf < end - 1 ? get_be16_inc(&buf) : 0, return NULL;)
            if (buf <= end && c == '\n')
                return buf - 1;
        }
        break;
    }
    return NULL;
}

#define EMPTY_STMT do{}while(0);

/**
 * Copy a number of bytes, converting to UTF-8 if input is UTF-16
 * \param dst buffer to copy to
 * \param dstsize size of dst buffer
 * \param src buffer to copy from
 * \param len amount of bytes to copy from src
 * \param utf16 chose between UTF-8/ASCII/other and LE and BE UTF-16
 *              0 = UTF-8/ASCII/other, 1 = UTF-16-LE, 2 = UTF-16-BE
 */
static int copy_characters(uint8_t *dst, int dstsize,
                           const uint8_t *src, int *len, int utf16)
{
    uint32_t c;
    uint8_t *dst_end = dst + dstsize;
    const uint8_t *end = src + *len;
    switch (utf16) {
    case 0:
        if (*len > dstsize)
            *len = dstsize;
        memcpy(dst, src, *len);
        return *len;
    case 1:
        while (src < end - 1 && dst_end - dst > 8) {
            uint8_t tmp;
            GET_UTF16(c, src < end - 1 ? get_le16_inc(&src) : 0, EMPTY_STMT)
            PUT_UTF8(c, tmp, *dst++ = tmp; EMPTY_STMT)
        }
        *len -= end - src;
        return dstsize - (dst_end - dst);
    case 2:
        while (src < end - 1 && dst_end - dst > 8) {
            uint8_t tmp;
            GET_UTF16(c, src < end - 1 ? get_be16_inc(&src) : 0, EMPTY_STMT)
            PUT_UTF8(c, tmp, *dst++ = tmp; EMPTY_STMT)
        }
        *len -= end - src;
        return dstsize - (dst_end - dst);
    }
    return 0;
}

unsigned char *stream_read_line(stream_t *s, unsigned char *mem, int max,
                                int utf16)
{
    int len;
    const unsigned char *end;
    unsigned char *ptr = mem;
    if (utf16 == -1)
        utf16 = 0;
    if (max < 1)
        return NULL;
    max--; // reserve one for 0-termination
    do {
        len = s->buf_len - s->buf_pos;
        // try to fill the buffer
        if (len <= 0 &&
            (!stream_fill_buffer(s) ||
             (len = s->buf_len - s->buf_pos) <= 0))
            break;
        end = find_newline(s->buffer + s->buf_pos, len, utf16);
        if (end)
            len = end - (s->buffer + s->buf_pos) + 1;
        if (len > 0 && max > 0) {
            int l = copy_characters(ptr, max, s->buffer + s->buf_pos, &len,
                                    utf16);
            max -= l;
            ptr += l;
            if (!len)
                break;
        }
        s->buf_pos += len;
    } while (!end);
    ptr[0] = 0;
    if (s->eof && ptr == mem)
        return NULL;
    return mem;
}

static const char *bom[3] = {"\xEF\xBB\xBF", "\xFF\xFE", "\xFE\xFF"};

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
    if (s->end_pos > max_size)
        return (struct bstr){NULL, 0};
    if (s->end_pos > 0)
        bufsize = s->end_pos + padding;
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

bool stream_manages_timeline(struct stream *s)
{
    return stream_control(s, STREAM_CTRL_MANAGES_TIMELINE, NULL) == STREAM_OK;
}
