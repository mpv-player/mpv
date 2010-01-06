#include "config.h"

#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "mp_msg.h"
#include "stream.h"
#include "m_option.h"
#include "m_struct.h"

static int fill_buffer(stream_t *s, char *buffer, int max_len)
{
    int r = url_read_complete(s->priv, buffer, max_len);
    return (r <= 0) ? -1 : r;
}

static int write_buffer(stream_t *s, char *buffer, int len)
{
    int r = url_write(s->priv, buffer, len);
    return (r <= 0) ? -1 : r;
}

static int seek(stream_t *s, off_t newpos)
{
    s->pos = newpos;
    if (url_seek(s->priv, s->pos, SEEK_SET) < 0) {
        s->eof = 1;
        return 0;
    }
    return 1;
}

static int control(stream_t *s, int cmd, void *arg)
{
    int64_t size;
    switch(cmd) {
    case STREAM_CTRL_GET_SIZE:
        size = url_filesize(s->priv);
        if(size >= 0) {
            *(off_t *)arg = size;
            return 1;
        }
    }
    return STREAM_UNSUPPORTED;
}

static void close_f(stream_t *stream)
{
    url_close(stream->priv);
}

static const char prefix[] = "ffmpeg://";

static int open_f(stream_t *stream, int mode, void *opts, int *file_format)
{
    int flags = 0;
    const char *filename;
    URLContext *ctx = NULL;
    int res = STREAM_ERROR;
    int64_t size;

    av_register_all();
    if (mode == STREAM_READ)
        flags = URL_RDONLY;
    else if (mode == STREAM_WRITE)
        flags = URL_WRONLY;
    else {
        mp_msg(MSGT_OPEN, MSGL_ERR, "[ffmpeg] Unknown open mode %d\n", mode);
        res = STREAM_UNSUPPORTED;
        goto out;
    }

    if (stream->url)
        filename = stream->url;
    else {
        mp_msg(MSGT_OPEN, MSGL_ERR, "[ffmpeg] No URL\n");
        goto out;
    }
    if (!strncmp(filename, prefix, strlen(prefix)))
        filename += strlen(prefix);
    mp_msg(MSGT_OPEN, MSGL_V, "[ffmpeg] Opening %s\n", filename);

    if (url_open(&ctx, filename, flags) < 0)
        goto out;

    stream->priv = ctx;
    size = url_filesize(ctx);
    if (size >= 0)
        stream->end_pos = size;
    stream->type = STREAMTYPE_FILE;
    stream->seek = seek;
    if (ctx->is_streamed) {
        stream->type = STREAMTYPE_STREAM;
        stream->seek = NULL;
    }
    stream->fill_buffer = fill_buffer;
    stream->write_buffer = write_buffer;
    stream->control = control;
    stream->close = close_f;
    res = STREAM_OK;

out:
    return res;
}

const stream_info_t stream_info_ffmpeg = {
  "FFmpeg",
  "ffmpeg",
  "",
  "",
  open_f,
  { "ffmpeg", "rtmp", NULL },
  NULL,
  1 // Urls are an option string
};
