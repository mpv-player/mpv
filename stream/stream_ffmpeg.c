#include "config.h"

#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "mp_msg.h"
#include "stream.h"
#include "m_option.h"
#include "m_struct.h"

static struct stream_priv_s {
    char *filename;
    char *filename2;
} stream_priv_dflts = {
    NULL, NULL
};

#define ST_OFF(f) M_ST_OFF(struct stream_priv_s,f)
/// URL definition
static const m_option_t stream_opts_fields[] = {
    {"string",   ST_OFF(filename),  CONF_TYPE_STRING, 0, 0 ,0, NULL},
    {"filename", ST_OFF(filename2), CONF_TYPE_STRING, 0, 0 ,0, NULL},
    {NULL}
};

static const struct m_struct_st stream_opts = {
    "ffmpeg",
    sizeof(struct stream_priv_s),
    &stream_priv_dflts,
    stream_opts_fields
};

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
    struct stream_priv_s *p = opts;
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

    if (p->filename)
        filename = p->filename;
    else if (p->filename2)
        filename = p->filename2;
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
    m_struct_free(&stream_opts,opts);
    return res;
}

const stream_info_t stream_info_ffmpeg = {
  "FFmpeg",
  "ffmpeg",
  "",
  "",
  open_f,
  { "ffmpeg", NULL },
  &stream_opts,
  1 // Urls are an option string
};
