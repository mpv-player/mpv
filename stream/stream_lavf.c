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

#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/opt.h>

#include "config.h"
#include "options/options.h"
#include "options/path.h"
#include "common/msg.h"
#include "stream.h"
#include "options/m_option.h"

#include "cookies.h"

#include "bstr/bstr.h"
#include "talloc.h"

static int open_f(stream_t *stream, int mode);
static char **read_icy(stream_t *stream);

static int fill_buffer(stream_t *s, char *buffer, int max_len)
{
    AVIOContext *avio = s->priv;
    if (!avio)
        return -1;
    int r = avio_read(avio, buffer, max_len);
    return (r <= 0) ? -1 : r;
}

static int write_buffer(stream_t *s, char *buffer, int len)
{
    AVIOContext *avio = s->priv;
    if (!avio)
        return -1;
    avio_write(avio, buffer, len);
    avio_flush(avio);
    if (avio->error)
        return -1;
    return len;
}

static int seek(stream_t *s, int64_t newpos)
{
    AVIOContext *avio = s->priv;
    if (!avio)
        return -1;
    if (avio_seek(avio, newpos, SEEK_SET) < 0) {
        return 0;
    }
    return 1;
}

static void close_f(stream_t *stream)
{
    AVIOContext *avio = stream->priv;
    /* NOTE: As of 2011 write streams must be manually flushed before close.
     * Currently write_buffer() always flushes them after writing.
     * avio_close() could return an error, but we have no way to return that
     * with the current stream API.
     */
    if (avio)
        avio_close(avio);
}

static int control(stream_t *s, int cmd, void *arg)
{
    AVIOContext *avio = s->priv;
    if (!avio && cmd != STREAM_CTRL_RECONNECT)
        return -1;
    int64_t size, ts;
    double pts;
    switch(cmd) {
    case STREAM_CTRL_GET_SIZE:
        size = avio_size(avio);
        if (size >= 0) {
            *(int64_t *)arg = size;
            return 1;
        }
        break;
    case STREAM_CTRL_SEEK_TO_TIME:
        pts = *(double *)arg;
        ts = pts * AV_TIME_BASE;
        ts = avio_seek_time(avio, -1, ts, 0);
        if (ts >= 0)
            return 1;
        break;
    case STREAM_CTRL_GET_METADATA: {
        *(char ***)arg = read_icy(s);
        if (!*(char ***)arg)
            break;
        return 1;
    }
    case STREAM_CTRL_RECONNECT: {
        if (avio && avio->write_flag)
            break; // don't bother with this
        // avio doesn't seem to support this - emulate it by reopening
        close_f(s);
        s->priv = NULL;
        return open_f(s, STREAM_READ);
    }
    }
    return STREAM_UNSUPPORTED;
}

static int interrupt_cb(void *ctx)
{
    struct stream *stream = ctx;
    return stream_check_interrupt(stream);
}

static const char * const prefix[] = { "lavf://", "ffmpeg://" };

static int open_f(stream_t *stream, int mode)
{
    struct MPOpts *opts = stream->opts;
    int flags = 0;
    AVIOContext *avio = NULL;
    int res = STREAM_ERROR;
    AVDictionary *dict = NULL;
    void *temp = talloc_new(NULL);

    stream->seek = NULL;
    stream->seekable = false;

    if (mode == STREAM_READ)
        flags = AVIO_FLAG_READ;
    else if (mode == STREAM_WRITE)
        flags = AVIO_FLAG_WRITE;
    else {
        MP_ERR(stream, "[ffmpeg] Unknown open mode %d\n", mode);
        res = STREAM_UNSUPPORTED;
        goto out;
    }

    const char *filename = stream->url;
    if (!filename) {
        MP_ERR(stream, "[ffmpeg] No URL\n");
        goto out;
    }
    for (int i = 0; i < sizeof(prefix) / sizeof(prefix[0]); i++)
        if (!strncmp(filename, prefix[i], strlen(prefix[i])))
            filename += strlen(prefix[i]);
    if (!strncmp(filename, "rtsp:", 5)) {
        /* This is handled as a special demuxer, without a separate
         * stream layer. demux_lavf will do all the real work. Note
         * that libavformat doesn't even provide a protocol entry for
         * this (the rtsp demuxer's probe function checks for a "rtsp:"
         * filename prefix), so it has to be handled specially here.
         */
        stream->demuxer = "lavf";
        stream->lavf_type = "rtsp";
        talloc_free(temp);
        return STREAM_OK;
    }
    MP_VERBOSE(stream, "[ffmpeg] Opening %s\n", filename);

    // Replace "mms://" with "mmsh://", so that most mms:// URLs just work.
    bstr b_filename = bstr0(filename);
    if (bstr_eatstart0(&b_filename, "mms://") ||
        bstr_eatstart0(&b_filename, "mmshttp://"))
    {
        filename = talloc_asprintf(temp, "mmsh://%.*s", BSTR_P(b_filename));
    }

    // HTTP specific options (other protocols ignore them)
    if (opts->network_useragent)
        av_dict_set(&dict, "user-agent", opts->network_useragent, 0);
    if (opts->network_cookies_enabled) {
        char *file = opts->network_cookies_file;
        if (file && file[0])
            file = mp_get_user_path(temp, stream->global, file);
        char *cookies = cookies_lavf(temp, stream->log, file);
        if (cookies && cookies[0])
            av_dict_set(&dict, "cookies", cookies, 0);
    }
    av_dict_set(&dict, "tls_verify", opts->network_tls_verify ? "1" : "0", 0);
    if (opts->network_tls_ca_file)
        av_dict_set(&dict, "ca_file", opts->network_tls_ca_file, 0);
    char *cust_headers = talloc_strdup(temp, "");
    if (opts->network_referrer) {
        cust_headers = talloc_asprintf_append(cust_headers, "Referer: %s\r\n",
                                              opts->network_referrer);
    }
    if (opts->network_http_header_fields) {
        for (int n = 0; opts->network_http_header_fields[n]; n++) {
            cust_headers = talloc_asprintf_append(cust_headers, "%s\r\n",
                                                  opts->network_http_header_fields[n]);
        }
    }
    if (strlen(cust_headers))
        av_dict_set(&dict, "headers", cust_headers, 0);
    av_dict_set(&dict, "icy", "1", 0);

    AVIOInterruptCB cb = {
        .callback = interrupt_cb,
        .opaque = stream,
    };

    int err = avio_open2(&avio, filename, flags, &cb, &dict);
    if (err < 0) {
        if (err == AVERROR_PROTOCOL_NOT_FOUND)
            MP_ERR(stream, "[ffmpeg] Protocol not found. Make sure"
                   " ffmpeg/Libav is compiled with networking support.\n");
        goto out;
    }

    AVDictionaryEntry *t = NULL;
    while ((t = av_dict_get(dict, "", t, AV_DICT_IGNORE_SUFFIX))) {
        MP_VERBOSE(stream, "[ffmpeg] Could not set stream option %s=%s\n",
               t->key, t->value);
    }

    if (avio->av_class) {
        uint8_t *mt = NULL;
        if (av_opt_get(avio, "mime_type", AV_OPT_SEARCH_CHILDREN, &mt) >= 0) {
            stream->mime_type = talloc_strdup(stream, mt);
            av_free(mt);
        }
    }

    if (strncmp(filename, "rtmp", 4) == 0) {
        stream->demuxer = "lavf";
        stream->lavf_type = "flv";
    }
    stream->priv = avio;
    stream->seekable = avio->seekable;
    stream->seek = stream->seekable ? seek : NULL;
    stream->fill_buffer = fill_buffer;
    stream->write_buffer = write_buffer;
    stream->control = control;
    stream->close = close_f;
    // enable cache (should be avoided for files, but no way to detect this)
    stream->streaming = true;
    res = STREAM_OK;

out:
    av_dict_free(&dict);
    talloc_free(temp);
    return res;
}

static void append_meta(char ***info, int *num_info, bstr name, bstr val)
{
    if (name.len && val.len) {
        char *cname = talloc_asprintf(*info, "icy-%.*s", BSTR_P(name));
        char *cval = talloc_asprintf(*info, "%.*s", BSTR_P(val));
        MP_TARRAY_APPEND(NULL, *info, *num_info, cname);
        MP_TARRAY_APPEND(NULL, *info, *num_info, cval);
    }
}

static char **read_icy(stream_t *s)
{
    AVIOContext *avio = s->priv;

    if (!avio->av_class)
        return NULL;

    uint8_t *icy_header = NULL;
    if (av_opt_get(avio, "icy_metadata_headers", AV_OPT_SEARCH_CHILDREN,
                   &icy_header) < 0)
        icy_header = NULL;

    uint8_t *icy_packet;
    if (av_opt_get(avio, "icy_metadata_packet", AV_OPT_SEARCH_CHILDREN,
                   &icy_packet) < 0)
        icy_packet = NULL;

    char **res = NULL;

    if ((!icy_header || !icy_header[0]) && (!icy_packet || !icy_packet[0]))
        goto done;

    res = talloc_new(NULL);
    int num_res = 0;
    bstr header = bstr0(icy_header);
    while (header.len) {
        bstr line = bstr_strip_linebreaks(bstr_getline(header, &header));
        bstr name, val;
        if (bstr_split_tok(line, ": ", &name, &val)) {
            bstr_eatstart0(&name, "icy-");
            append_meta(&res, &num_res, name, val);
        }
    }

    bstr packet = bstr0(icy_packet);
    bstr head = bstr0("StreamTitle='");
    int i = bstr_find(packet, head);
    if (i >= 0) {
        packet = bstr_cut(packet, i + head.len);
        int end = bstrchr(packet, '\'');
        packet = bstr_splice(packet, 0, end);
        append_meta(&res, &num_res, bstr0("title"), packet);
    }

    if (res) {
        MP_TARRAY_APPEND(NULL, res, num_res, NULL);
        MP_TARRAY_APPEND(NULL, res, num_res, NULL);
    }

done:
    av_free(icy_header);
    av_free(icy_packet);
    return res;
}

const stream_info_t stream_info_ffmpeg = {
  .name = "ffmpeg",
  .open = open_f,
  .protocols = (const char*[]){
     "lavf", "ffmpeg", "rtmp", "rtsp", "http", "https", "mms", "mmst", "mmsh",
     "mmshttp", "udp", "ftp", "rtp", "httpproxy", "hls", "rtmpe", "rtmps",
     "rtmpt", "rtmpte", "rtmpts", "srtp", "tcp", "udp", "tls", "unix", "sftp",
     "md5",
     NULL },
};
