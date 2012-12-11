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
#include "core/mp_msg.h"
#include "stream.h"
#include "core/m_option.h"
#include "core/m_struct.h"
#include "demux/demux.h"

static int fill_buffer(stream_t *s, char *buffer, int max_len)
{
    AVIOContext *avio = s->priv;
    int r = avio_read(avio, buffer, max_len);
    return (r <= 0) ? -1 : r;
}

static int write_buffer(stream_t *s, char *buffer, int len)
{
    AVIOContext *avio = s->priv;
    avio_write(avio, buffer, len);
    avio_flush(avio);
    if (avio->error)
        return -1;
    return len;
}

static int seek(stream_t *s, int64_t newpos)
{
    AVIOContext *avio = s->priv;
    s->pos = newpos;
    if (avio_seek(avio, s->pos, SEEK_SET) < 0) {
        s->eof = 1;
        return 0;
    }
    return 1;
}

static int control(stream_t *s, int cmd, void *arg)
{
    AVIOContext *avio = s->priv;
    int64_t size, ts;
    double pts;
    switch(cmd) {
    case STREAM_CTRL_GET_SIZE:
        size = avio_size(avio);
        if(size >= 0) {
            *(uint64_t *)arg = size;
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
    }
    return STREAM_UNSUPPORTED;
}

static void close_f(stream_t *stream)
{
    AVIOContext *avio = stream->priv;
    /* NOTE: As of 2011 write streams must be manually flushed before close.
     * Currently write_buffer() always flushes them after writing.
     * avio_close() could return an error, but we have no way to return that
     * with the current stream API.
     */
    avio_close(avio);
}

static const char * const prefix[] = { "lavf://", "ffmpeg://" };

static int open_f(stream_t *stream, int mode, void *opts, int *file_format)
{
    int flags = 0;
    AVIOContext *avio = NULL;
    int res = STREAM_ERROR;
    void *temp = talloc_new(NULL);

    if (mode == STREAM_READ)
        flags = AVIO_FLAG_READ;
    else if (mode == STREAM_WRITE)
        flags = AVIO_FLAG_WRITE;
    else {
        mp_msg(MSGT_OPEN, MSGL_ERR, "[ffmpeg] Unknown open mode %d\n", mode);
        res = STREAM_UNSUPPORTED;
        goto out;
    }

    const char *filename = stream->url;
    if (!filename) {
        mp_msg(MSGT_OPEN, MSGL_ERR, "[ffmpeg] No URL\n");
        goto out;
    }
    for (int i = 0; i < sizeof(prefix) / sizeof(prefix[0]); i++)
        if (!strncmp(filename, prefix[i], strlen(prefix[i])))
            filename += strlen(prefix[i]);
    if (!strncmp(filename, "rtsp:", 5)) {
        /* This is handled as a special demuxer, without a separate
         * stream layer. demux_lavf will do all the real work.
         */
        stream->type = STREAMTYPE_STREAM;
        stream->seek = NULL;
        *file_format = DEMUXER_TYPE_LAVF;
        stream->lavf_type = "rtsp";
        return STREAM_OK;
    }
    mp_msg(MSGT_OPEN, MSGL_V, "[ffmpeg] Opening %s\n", filename);

    // Replace "mms://" with "mmsh://", so that most mms:// URLs just work.
    bstr b_filename = bstr0(filename);
    if (bstr_eatstart0(&b_filename, "mms://") ||
        bstr_eatstart0(&b_filename, "mmshttp://"))
    {
        filename = talloc_asprintf(temp, "mmsh://%.*s", BSTR_P(b_filename));
    }

    if (avio_open(&avio, filename, flags) < 0)
        goto out;

#if LIBAVFORMAT_VERSION_MICRO >= 100
    if (avio->av_class) {
        uint8_t *mt = NULL;
        if (av_opt_get(avio, "mime_type", AV_OPT_SEARCH_CHILDREN, &mt) >= 0)
            stream->mime_type = talloc_strdup(stream, mt);
    }
#endif

    char *rtmp[] = {"rtmp:", "rtmpt:", "rtmpe:", "rtmpte:", "rtmps:"};
    for (int i = 0; i < FF_ARRAY_ELEMS(rtmp); i++)
        if (!strncmp(filename, rtmp[i], strlen(rtmp[i]))) {
            *file_format = DEMUXER_TYPE_LAVF;
            stream->lavf_type = "flv";
        }
    stream->priv = avio;
    int64_t size = avio_size(avio);
    if (size >= 0)
        stream->end_pos = size;
    stream->type = STREAMTYPE_FILE;
    stream->seek = seek;
    if (!avio->seekable) {
        stream->type = STREAMTYPE_STREAM;
        stream->seek = NULL;
    }
    stream->fill_buffer = fill_buffer;
    stream->write_buffer = write_buffer;
    stream->control = control;
    stream->close = close_f;
    // enable cache (should be avoided for files, but no way to detect this)
    stream->streaming = true;
    res = STREAM_OK;

out:
    talloc_free(temp);
    return res;
}

const stream_info_t stream_info_ffmpeg = {
  "FFmpeg",
  "ffmpeg",
  "",
  "",
  open_f,
  { "lavf", "ffmpeg", "rtmp", "rtsp", "http", "https", "mms", "mmst", "mmsh",
    "mmshttp", NULL },
  NULL,
  1 // Urls are an option string
};
