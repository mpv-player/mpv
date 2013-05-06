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

#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

#include "core/mp_msg.h"
#include "osdep/shmem.h"
#include "osdep/timer.h"
#include "network.h"
#include "stream.h"
#include "demux/demux.h"

#include "core/m_option.h"
#include "core/m_struct.h"

#include "cache2.h"

/// We keep these 2 for the gui atm, but they will be removed.
int vcd_track = 0;
char *cdrom_device = NULL;
char *dvd_device = NULL;
int dvd_title = 0;

struct input_ctx;
static int (*stream_check_interrupt_cb)(struct input_ctx *ctx, int time);
static struct input_ctx *stream_check_interrupt_ctx;

extern const stream_info_t stream_info_vcd;
extern const stream_info_t stream_info_cdda;
extern const stream_info_t stream_info_asf;
extern const stream_info_t stream_info_udp;
extern const stream_info_t stream_info_http1;
extern const stream_info_t stream_info_http2;
extern const stream_info_t stream_info_dvb;
extern const stream_info_t stream_info_tv;
extern const stream_info_t stream_info_radio;
extern const stream_info_t stream_info_pvr;
extern const stream_info_t stream_info_ftp;
extern const stream_info_t stream_info_vstream;
extern const stream_info_t stream_info_smb;

extern const stream_info_t stream_info_null;
extern const stream_info_t stream_info_mf;
extern const stream_info_t stream_info_ffmpeg;
extern const stream_info_t stream_info_avdevice;
extern const stream_info_t stream_info_file;
extern const stream_info_t stream_info_ifo;
extern const stream_info_t stream_info_dvd;
extern const stream_info_t stream_info_bluray;

static const stream_info_t *const auto_open_streams[] = {
#ifdef CONFIG_VCD
    &stream_info_vcd,
#endif
#ifdef CONFIG_CDDA
    &stream_info_cdda,
#endif
    &stream_info_ffmpeg, // use for rstp:// before http fallback
    &stream_info_avdevice,
#ifdef CONFIG_NETWORKING
    &stream_info_http1,
    &stream_info_asf,
    &stream_info_udp,
    &stream_info_http2,
#endif
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
#ifdef CONFIG_FTP
    &stream_info_ftp,
#endif
#ifdef CONFIG_VSTREAM
    &stream_info_vstream,
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

    &stream_info_null,
    &stream_info_mf,
    &stream_info_file,
    NULL
};

static stream_t *open_stream_plugin(const stream_info_t *sinfo,
                                    const char *filename,
                                    int mode, struct MPOpts *options,
                                    int *file_format, int *ret,
                                    char **redirected_url)
{
    void *arg = NULL;
    stream_t *s;
    m_struct_t *desc = (m_struct_t *)sinfo->opts;

    // Parse options
    if (desc) {
        arg = m_struct_alloc(desc);
        if (sinfo->opts_url) {
            m_option_t url_opt = { "stream url", arg, CONF_TYPE_CUSTOM_URL, 0,
                                   0, 0, (void *)sinfo->opts };
            if (m_option_parse(&url_opt, bstr0("stream url"), bstr0(filename),
                               arg) < 0)
            {
                mp_tmsg(MSGT_OPEN, MSGL_ERR, "URL parsing failed on url %s\n",
                        filename);
                m_struct_free(desc, arg);
                *ret = STREAM_ERROR;
                return NULL;
            }
        }
    }
    s = new_stream(-2, -2);
    s->opts = options;
    s->url = strdup(filename);
    s->flags |= mode;
    *ret = sinfo->open(s, mode, arg, file_format);
    if ((*ret) != STREAM_OK) {
#ifdef CONFIG_NETWORKING
        if (*ret == STREAM_REDIRECTED && redirected_url) {
            if (s->streaming_ctrl && s->streaming_ctrl->url
                && s->streaming_ctrl->url->url)
                *redirected_url = strdup(s->streaming_ctrl->url->url);
            else
                *redirected_url = NULL;
        }
        streaming_ctrl_free(s->streaming_ctrl);
#endif
        free(s->url);
        talloc_free(s);
        return NULL;
    }

    if (s->streaming && !s->cache_size) {
        // Set default cache size to use if user does not specify it.
        s->cache_size = 320;
    }

    if (s->type <= -2)
        mp_msg(MSGT_OPEN, MSGL_WARN, "Warning streams need a type !!!!\n");
    if (s->flags & MP_STREAM_SEEK && !s->seek)
        s->flags &= ~MP_STREAM_SEEK;
    if (s->seek && !(s->flags & MP_STREAM_SEEK))
        s->flags |= MP_STREAM_SEEK;

    s->mode = mode;

    mp_msg(MSGT_OPEN, MSGL_V, "STREAM: [%s] %s\n", sinfo->name, filename);
    mp_msg(MSGT_OPEN, MSGL_V, "STREAM: Description: %s\n", sinfo->info);
    mp_msg(MSGT_OPEN, MSGL_V, "STREAM: Author: %s\n", sinfo->author);
    mp_msg(MSGT_OPEN, MSGL_V, "STREAM: Comment: %s\n", sinfo->comment);

    if (s->mime_type)
        mp_msg(MSGT_OPEN, MSGL_V, "Mime-type: '%s'\n", s->mime_type);

    return s;
}


static stream_t *open_stream_full(const char *filename, int mode,
                                  struct MPOpts *options, int *file_format)
{
    int i, j, l, r;
    const stream_info_t *sinfo;
    stream_t *s;
    char *redirected_url = NULL;

    assert(filename);

    int dummy;
    if (!file_format)
        file_format = &dummy;

    *file_format = DEMUXER_TYPE_UNKNOWN;

    for (i = 0; auto_open_streams[i]; i++) {
        sinfo = auto_open_streams[i];
        if (!sinfo->protocols) {
            mp_msg(MSGT_OPEN, MSGL_WARN,
                   "Stream type %s has protocols == NULL, it's a bug\n",
                   sinfo->name);
            continue;
        }
        for (j = 0; sinfo->protocols[j]; j++) {
            l = strlen(sinfo->protocols[j]);
            // l == 0 => Don't do protocol matching (ie network and filenames)
            if ((l == 0 && !strstr(filename, "://")) ||
                ((strncasecmp(sinfo->protocols[j], filename, l) == 0) &&
                 (strncmp("://", filename + l, 3) == 0))) {
                *file_format = DEMUXER_TYPE_UNKNOWN;
                s =
                    open_stream_plugin(sinfo, filename, mode, options,
                                       file_format,
                                       &r,
                                       &redirected_url);
                if (s)
                    return s;
                if (r == STREAM_REDIRECTED && redirected_url) {
                    mp_msg(MSGT_OPEN, MSGL_V, "[%s] open %s redirected to %s\n",
                           sinfo->info, filename, redirected_url);
                    s = open_stream_full(redirected_url, mode, options,
                                         file_format);
                    free(redirected_url);
                    return s;
                } else if (r != STREAM_UNSUPPORTED) {
                    mp_tmsg(MSGT_OPEN, MSGL_ERR, "Failed to open %s.\n",
                            filename);
                    return NULL;
                }
                break;
            }
        }
    }

    mp_tmsg(MSGT_OPEN, MSGL_ERR, "No stream found to handle url %s\n", filename);
    return NULL;
}

stream_t *open_stream(const char *filename, struct MPOpts *options,
                      int *file_format)
{
    return open_stream_full(filename, STREAM_READ, options, file_format);
}

stream_t *open_output_stream(const char *filename, struct MPOpts *options)
{
    return open_stream_full(filename, STREAM_WRITE, options, NULL);
}

//=================== STREAMER =========================

static int stream_reconnect(stream_t *s)
{
#define MAX_RECONNECT_RETRIES 5
#define RECONNECT_SLEEP_MS 1000
    if (!s->streaming)
        return 0;
    int64_t pos = s->pos;
    for (int retry = 0; retry < MAX_RECONNECT_RETRIES; retry++) {
        mp_msg(MSGT_STREAM, MSGL_WARN,
               "Connection lost! Attempting to reconnect...\n");

        if (retry)
            usec_sleep(RECONNECT_SLEEP_MS * 1000);

        s->eof = 1;
        stream_reset(s);

        // Some streams (internal http.c) don't support STREAM_CTRL_RECONNECT,
        // but do it when trying to seek.
        if (s->control) {
            if (s->control(s, STREAM_CTRL_RECONNECT, NULL) == STREAM_ERROR)
                continue;
        }

        if (stream_seek_internal(s, pos) < 0 && s->pos == pos)
            return 1;
    }
    return 0;
}

int stream_read_internal(stream_t *s, void *buf, int len)
{
    int orig_len = len;
    // we will retry even if we already reached EOF previously.
    switch (s->type) {
    case STREAMTYPE_STREAM:
        if (s->streaming_ctrl != NULL && s->streaming_ctrl->streaming_read) {
            len = s->streaming_ctrl->streaming_read(s->fd, buf, len,
                                                    s->streaming_ctrl);
            if (s->streaming_ctrl->status == streaming_stopped_e &&
                (!s->end_pos || s->pos == s->end_pos))
                s->eof = 1;
        } else {
            if (s->fill_buffer)
                len = s->fill_buffer(s, buf, len);
            else
                len = read(s->fd, buf, len);
        }
        break;

    default:
        len = s->fill_buffer ? s->fill_buffer(s, buf, len) : 0;
    }
    if (len <= 0) {
        // do not retry if this looks like proper eof
        if (s->eof || (s->end_pos && s->pos == s->end_pos))
            goto eof_out;

        // just in case this is an error e.g. due to network
        // timeout reset and retry
        if (!stream_reconnect(s))
            goto eof_out;
        // make sure EOF is set to ensure no endless loops
        s->eof = 1;
        return stream_read_internal(s, buf, orig_len);

eof_out:
        s->eof = 1;
        return 0;
    }
    // When reading succeeded we are obviously not at eof.
    // This e.g. avoids issues with eof getting stuck when lavf seeks in MPEG-TS
    s->eof = 0;
    s->pos += len;
    return len;
}

int stream_fill_buffer(stream_t *s)
{
    int len = stream_read_internal(s, s->buffer, STREAM_BUFFER_SIZE);
    if (len <= 0)
        return 0;
    s->buf_pos = 0;
    s->buf_len = len;
//  printf("[%d]",len);fflush(stdout);
    return len;
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

int stream_seek_internal(stream_t *s, int64_t newpos)
{
    if (newpos == 0 || newpos != s->pos) {
        switch (s->type) {
        case STREAMTYPE_STREAM:
            //s->pos=newpos; // real seek
            // Some streaming protocol allow to seek backward and forward
            // A function call that return -1 can tell that the protocol
            // doesn't support seeking.
#ifdef CONFIG_NETWORKING
            if (s->seek) { // new stream seek is much cleaner than streaming_ctrl one
                if (!s->seek(s, newpos)) {
                    mp_tmsg(MSGT_STREAM, MSGL_ERR, "Seek failed\n");
                    return 0;
                }
                break;
            }

            if (s->streaming_ctrl != NULL &&
                s->streaming_ctrl->streaming_seek) {
                if (s->streaming_ctrl->streaming_seek(s->fd, newpos,
                                                      s->streaming_ctrl) < 0) {
                    mp_tmsg(MSGT_STREAM, MSGL_INFO, "Stream not seekable!\n");
                    return 1;
                }
                break;
            }
#endif
            if (newpos < s->pos) {
                mp_tmsg(MSGT_STREAM, MSGL_INFO,
                        "Cannot seek backward in linear streams!\n");
                return 1;
            }
            break;
        default:
            // This should at the beginning as soon as all streams are converted
            if (!s->seek)
                return 0;
            // Now seek
            if (!s->seek(s, newpos)) {
                mp_tmsg(MSGT_STREAM, MSGL_ERR, "Seek failed\n");
                return 0;
            }
        }
//   putchar('.');fflush(stdout);
//} else {
//   putchar('%');fflush(stdout);
    }
    return -1;
}

int stream_seek_long(stream_t *s, int64_t pos)
{
    int res;
    int64_t newpos = 0;

//  if( mp_msg_test(MSGT_STREAM,MSGL_DBG3) ) printf("seek_long to 0x%X\n",(unsigned int)pos);

    s->buf_pos = s->buf_len = 0;

    if (s->mode == STREAM_WRITE) {
        if (!s->seek || !s->seek(s, pos))
            return 0;
        return 1;
    }

    if (s->sector_size)
        newpos = (pos / s->sector_size) * s->sector_size;
    else
        newpos = pos & (~((int64_t)STREAM_BUFFER_SIZE - 1));

    if (mp_msg_test(MSGT_STREAM, MSGL_DBG3)) {
        mp_msg(
            MSGT_STREAM, MSGL_DBG3,
            "s->pos=%" PRIX64 "  newpos=%" PRIX64 "  new_bufpos=%" PRIX64
            "  buflen=%X  \n",
            (int64_t)s->pos, (int64_t)newpos, (int64_t)pos, s->buf_len);
    }
    pos -= newpos;

    res = stream_seek_internal(s, newpos);
    if (res >= 0)
        return res;

    while (s->pos < newpos) {
        if (stream_fill_buffer(s) <= 0)
            break;                      // EOF
    }

    s->eof = 0; // EOF reset when seek succeeds.
    while (stream_fill_buffer(s) > 0) {
        if (pos <= s->buf_len) {
            s->buf_pos = pos; // byte position in sector
            return 1;
        }
        pos -= s->buf_len;
    }
// Fill failed, but seek still is a success.
    s->pos += pos;
    s->buf_pos = 0;
    s->buf_len = 0;

    mp_msg(MSGT_STREAM, MSGL_V,
           "stream_seek: Seek to/past EOF: no buffer preloaded.\n");
    return 1;
}


void stream_reset(stream_t *s)
{
    if (s->eof) {
        s->pos = 0;
        s->buf_pos = s->buf_len = 0;
        s->eof = 0;
    }
    if (s->control)
        s->control(s, STREAM_CTRL_RESET, NULL);
    //stream_seek(s,0);
}

int stream_control(stream_t *s, int cmd, void *arg)
{
#ifdef CONFIG_STREAM_CACHE
    if (s->cache_pid)
        return cache_do_control(s, cmd, arg);
#endif
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

stream_t *new_memory_stream(unsigned char *data, int len)
{
    stream_t *s;

    if (len < 0)
        return NULL;
    s = calloc(1, sizeof(stream_t) + len);
    s->fd = -1;
    s->type = STREAMTYPE_MEMORY;
    s->buf_pos = 0;
    s->buf_len = len;
    s->start_pos = 0;
    s->end_pos = len;
    stream_reset(s);
    s->pos = len;
    memcpy(s->buffer, data, len);
    return s;
}

stream_t *new_stream(int fd, int type)
{
    stream_t *s = talloc_zero(NULL, stream_t);

#if HAVE_WINSOCK2_H
    {
        WSADATA wsdata;
        int temp = WSAStartup(0x0202, &wsdata); // there might be a better place for this (-> later)
        mp_msg(MSGT_STREAM, MSGL_V, "WINSOCK2 init: %i\n", temp);
    }
#endif

    s->fd = fd;
    s->type = type;
    stream_reset(s);
    return s;
}

void free_stream(stream_t *s)
{
//  printf("\n*** free_stream() called ***\n");
#ifdef CONFIG_STREAM_CACHE
    cache_uninit(s);
#endif

    if (s->close)
        s->close(s);
    if (s->fd > 0) {
        /* on unix we define closesocket to close
           on windows however we have to distinguish between
           network socket and file */
        if (s->url && strstr(s->url, "://"))
            closesocket(s->fd);
        else
            close(s->fd);
    }
#if HAVE_WINSOCK2_H
    mp_msg(MSGT_STREAM, MSGL_V, "WINSOCK2 uninit\n");
    WSACleanup(); // there might be a better place for this (-> later)
#endif
    free(s->url);
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
        usec_sleep(time * 1000);
        return 0;
    }
    return stream_check_interrupt_cb(stream_check_interrupt_ctx, time);
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
    if (max < 1)
        return NULL;
    max--; // reserve one for 0-termination
    do {
        len = s->buf_len - s->buf_pos;
        // try to fill the buffer
        if (len <= 0 &&
            (!cache_stream_fill_buffer(s) ||
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

struct bstr stream_read_complete(struct stream *s, void *talloc_ctx,
                                 int max_size, int padding_bytes)
{
    if (max_size > 1000000000)
        abort();

    int bufsize;
    int total_read = 0;
    int padding = FFMAX(padding_bytes, 1);
    char *buf = NULL;
    if (s->end_pos > max_size)
        return (struct bstr){
                   NULL, 0
        };
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
            return (struct bstr){
                       NULL, 0
            };
        }
        bufsize = FFMIN(bufsize + (bufsize >> 1), max_size + padding);
    }
    buf = talloc_realloc_size(talloc_ctx, buf, total_read + padding);
    return (struct bstr){
               buf, total_read
    };
}

bool stream_manages_timeline(struct stream *s)
{
    return stream_control(s, STREAM_CTRL_MANAGES_TIMELINE, NULL) == STREAM_OK;
}
