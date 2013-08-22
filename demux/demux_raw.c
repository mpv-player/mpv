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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "mpvcore/m_option.h"

#include "stream/stream.h"
#include "demux.h"
#include "stheader.h"
#include "audio/format.h"

#include "video/img_format.h"
#include "video/img_fourcc.h"

struct priv {
    int frame_size;
    int read_frames;
    double frame_rate;
};

static struct mp_chmap channels = MP_CHMAP_INIT_STEREO;
static int samplerate = 44100;
static int aformat = AF_FORMAT_S16_NE;

const m_option_t demux_rawaudio_opts[] = {
    { "channels", &channels, &m_option_type_chmap, CONF_MIN, 1 },
    { "rate", &samplerate, CONF_TYPE_INT, CONF_RANGE, 1000, 8 * 48000, NULL },
    { "format", &aformat, CONF_TYPE_AFMT, 0, 0, 0, NULL },
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

static int vformat = MP_FOURCC_I420;
static int mp_format;
static char *codec;
static int width = 0;
static int height = 0;
static float fps = 25;
static int imgsize = 0;

const m_option_t demux_rawvideo_opts[] = {
    // size:
    { "w", &width, CONF_TYPE_INT, CONF_RANGE, 1, 8192, NULL },
    { "h", &height, CONF_TYPE_INT, CONF_RANGE, 1, 8192, NULL },
    // format:
    { "format", &vformat, CONF_TYPE_FOURCC, 0, 0, 0, NULL },
    { "mp-format", &mp_format, CONF_TYPE_IMGFMT, 0, 0, 0, NULL },
    { "codec", &codec, CONF_TYPE_STRING, 0, 0, 0, NULL },
    // misc:
    { "fps", &fps, CONF_TYPE_FLOAT, CONF_RANGE, 0.001, 1000, NULL },
    { "size", &imgsize, CONF_TYPE_INT, CONF_RANGE, 1, 8192 * 8192 * 4, NULL },

    {NULL, NULL, 0, 0, 0, 0, NULL}
};

static int demux_rawaudio_open(demuxer_t *demuxer, enum demux_check check)
{
    struct sh_stream *sh;
    sh_audio_t *sh_audio;
    WAVEFORMATEX *w;

    if (check != DEMUX_CHECK_REQUEST && check != DEMUX_CHECK_FORCE)
        return -1;

    if ((aformat & AF_FORMAT_SPECIAL_MASK) != 0)
        return -1;

    sh = new_sh_stream(demuxer, STREAM_AUDIO);
    sh_audio = sh->audio;
    sh_audio->gsh->codec = "mp-pcm";
    sh_audio->format = aformat;
    sh_audio->wf = w = malloc(sizeof(*w));
    w->wFormatTag = 0;
    sh_audio->channels = channels;
    w->nChannels = sh_audio->channels.num;
    w->nSamplesPerSec = sh_audio->samplerate = samplerate;
    int samplesize = (af_fmt2bits(aformat) + 7) / 8;
    w->nAvgBytesPerSec = samplerate * samplesize * w->nChannels;
    w->nBlockAlign = w->nChannels * samplesize;
    w->wBitsPerSample = 8 * samplesize;
    w->cbSize = 0;

    demuxer->movi_start = demuxer->stream->start_pos;
    demuxer->movi_end = demuxer->stream->end_pos;

    struct priv *p = talloc_ptrtype(demuxer, p);
    demuxer->priv = p;
    *p = (struct priv) {
        .frame_size = samplesize * sh_audio->channels.num,
        .frame_rate = samplerate,
        .read_frames = samplerate,
    };

    return 0;
}

static int demux_rawvideo_open(demuxer_t *demuxer, enum demux_check check)
{
    struct sh_stream *sh;
    sh_video_t *sh_video;

    if (check != DEMUX_CHECK_REQUEST && check != DEMUX_CHECK_FORCE)
        return -1;

    if (!width || !height) {
        mp_msg(MSGT_DEMUX, MSGL_ERR, "rawvideo: width or height not specified!\n");
        return -1;
    }

    const char *decoder = "rawvideo";
    int imgfmt = vformat;
    if (mp_format) {
        decoder = "mp-rawvideo";
        imgfmt = mp_format;
        if (!imgsize) {
            struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(mp_format);
            for (int p = 0; p < desc.num_planes; p++) {
                imgsize += ((width >> desc.xs[p]) * (height >> desc.ys[p]) *
                            desc.bpp[p] + 7) / 8;
            }
        }
    } else if (codec && codec[0])
        decoder = talloc_strdup(demuxer, codec);

    if (!imgsize) {
        int bpp = 0;
        switch (vformat) {
        case MP_FOURCC_I420: case MP_FOURCC_IYUV:
        case MP_FOURCC_NV12: case MP_FOURCC_NV21:
        case MP_FOURCC_HM12:
        case MP_FOURCC_YV12:
            bpp = 12;
            break;
        case MP_FOURCC_RGB12: case MP_FOURCC_BGR12:
        case MP_FOURCC_RGB15: case MP_FOURCC_BGR15:
        case MP_FOURCC_RGB16: case MP_FOURCC_BGR16:
        case MP_FOURCC_YUY2:  case MP_FOURCC_UYVY:
            bpp = 16;
            break;
        case MP_FOURCC_RGB8: case MP_FOURCC_BGR8:
        case MP_FOURCC_Y800: case MP_FOURCC_Y8:
            bpp = 8;
            break;
        case MP_FOURCC_RGB24: case MP_FOURCC_BGR24:
            bpp = 24;
            break;
        case MP_FOURCC_RGB32: case MP_FOURCC_BGR32:
            bpp = 32;
            break;
        }
        if (!bpp) {
            mp_msg(MSGT_DEMUX, MSGL_ERR,
                   "rawvideo: img size not specified and unknown format!\n");
            return -1;
        }
        imgsize = width * height * bpp / 8;
    }

    sh = new_sh_stream(demuxer, STREAM_VIDEO);
    sh_video = sh->video;
    sh_video->gsh->codec = decoder;
    sh_video->format = imgfmt;
    sh_video->fps = fps;
    sh_video->disp_w = width;
    sh_video->disp_h = height;
    sh_video->i_bps = fps * imgsize;

    demuxer->movi_start = demuxer->stream->start_pos;
    demuxer->movi_end = demuxer->stream->end_pos;

    struct priv *p = talloc_ptrtype(demuxer, p);
    demuxer->priv = p;
    *p = (struct priv) {
        .frame_size = imgsize,
        .frame_rate = fps,
        .read_frames = 1,
    };

    return 0;
}

static int raw_fill_buffer(demuxer_t *demuxer)
{
    struct priv *p = demuxer->priv;

    if (demuxer->stream->eof)
        return 0;

    struct demux_packet *dp = new_demux_packet(p->frame_size * p->read_frames);
    dp->pos = stream_tell(demuxer->stream) - demuxer->movi_start;
    dp->pts = (dp->pos  / p->frame_size) / p->frame_rate;

    int len = stream_read(demuxer->stream, dp->buffer, dp->len);
    resize_demux_packet(dp, len);
    demuxer_add_packet(demuxer, demuxer->streams[0], dp);

    return 1;
}

static void raw_seek(demuxer_t *demuxer, float rel_seek_secs, int flags)
{
    struct priv *p = demuxer->priv;
    stream_t *s = demuxer->stream;
    stream_update_size(s);
    int64_t start = s->start_pos;
    int64_t end = s->end_pos;
    int64_t pos = (flags & SEEK_ABSOLUTE) ? start : stream_tell(s);
    if (flags & SEEK_FACTOR)
        pos += (end - start) * rel_seek_secs;
    else
        pos += rel_seek_secs * p->frame_rate * p->frame_size;
    if (pos < 0)
        pos = 0;
    if (end && pos > end)
        pos = end;
    stream_seek(s, (pos / p->frame_size) * p->frame_size);
}

static int raw_control(demuxer_t *demuxer, int cmd, void *arg)
{
    struct priv *p = demuxer->priv;

    switch (cmd) {
    case DEMUXER_CTRL_GET_TIME_LENGTH: {
        stream_t *s = demuxer->stream;
        stream_update_size(s);
        int64_t start = s->start_pos;
        int64_t end = s->end_pos;
        if (!end)
            return DEMUXER_CTRL_DONTKNOW;

        *((double *) arg) = ((end - start) / p->frame_size) / p->frame_rate;
        return DEMUXER_CTRL_OK;
    }
    default:
        return DEMUXER_CTRL_NOTIMPL;
    }
}

const demuxer_desc_t demuxer_desc_rawaudio = {
    .name = "rawaudio",
    .desc = "Uncompressed audio",
    .open = demux_rawaudio_open,
    .fill_buffer = raw_fill_buffer,
    .seek = raw_seek,
    .control = raw_control,
};

const demuxer_desc_t demuxer_desc_rawvideo = {
    .name = "rawvideo",
    .desc = "Uncompressed video",
    .open = demux_rawvideo_open,
    .fill_buffer = raw_fill_buffer,
    .seek = raw_seek,
    .control = raw_control,
};
