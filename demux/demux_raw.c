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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "options/m_option.h"
#include "options/options.h"

#include "stream/stream.h"
#include "demux.h"
#include "stheader.h"
#include "codec_tags.h"

#include "video/img_format.h"
#include "video/img_fourcc.h"

#include "osdep/endian.h"

struct demux_rawaudio_opts {
    struct mp_chmap channels;
    int samplerate;
    int aformat;
};

// Ad-hoc schema to systematically encode the format as int
#define PCM(sign, is_float, bits, is_be) \
    ((sign) | ((is_float) << 1) | ((is_be) << 2) | ((bits) << 3))
#define NE (BYTE_ORDER == BIG_ENDIAN)

#define OPT_BASE_STRUCT struct demux_rawaudio_opts
const struct m_sub_options demux_rawaudio_conf = {
    .opts = (const m_option_t[]) {
        OPT_CHMAP("channels", channels, CONF_MIN, .min = 1),
        OPT_INTRANGE("rate", samplerate, 0, 1000, 8 * 48000),
        OPT_CHOICE("format", aformat, 0,
                   ({"u8",      PCM(0, 0,  8, 0)},
                    {"s8",      PCM(1, 0,  8, 0)},
                    {"u16le",   PCM(0, 0, 16, 0)}, {"u16be",    PCM(0, 0, 16, 1)},
                    {"s16le",   PCM(1, 0, 16, 0)}, {"u16be",    PCM(1, 0, 16, 1)},
                    {"u24le",   PCM(0, 0, 24, 0)}, {"u24be",    PCM(0, 0, 24, 1)},
                    {"s24le",   PCM(1, 0, 24, 0)}, {"s24be",    PCM(1, 0, 24, 1)},
                    {"u32le",   PCM(0, 0, 32, 0)}, {"u32be",    PCM(0, 0, 32, 1)},
                    {"s32le",   PCM(1, 0, 32, 0)}, {"s32be",    PCM(1, 0, 32, 1)},
                    {"floatle", PCM(0, 1, 32, 0)}, {"floatbe",  PCM(0, 1, 32, 1)},
                    {"doublele",PCM(0, 1, 64, 0)}, {"doublebe", PCM(0, 1, 64, 1)},
                    {"u16",     PCM(0, 0, 16, NE)},
                    {"s16",     PCM(1, 0, 16, NE)},
                    {"u24",     PCM(0, 0, 24, NE)},
                    {"s24",     PCM(1, 0, 24, NE)},
                    {"u32",     PCM(0, 0, 32, NE)},
                    {"s32",     PCM(1, 0, 32, NE)},
                    {"float",   PCM(0, 1, 32, NE)},
                    {"double",  PCM(0, 1, 64, NE)})),
        {0}
    },
    .size = sizeof(struct demux_rawaudio_opts),
    .defaults = &(const struct demux_rawaudio_opts){
        // Note that currently, stream_cdda expects exactly these parameters!
        .channels = MP_CHMAP_INIT_STEREO,
        .samplerate = 44100,
        .aformat = PCM(1, 0, 16, 0), // s16le
    },
};

#undef PCM
#undef NE

struct demux_rawvideo_opts {
    int vformat;
    int mp_format;
    char *codec;
    int width;
    int height;
    float fps;
    int imgsize;
};

#undef OPT_BASE_STRUCT
#define OPT_BASE_STRUCT struct demux_rawvideo_opts
const struct m_sub_options demux_rawvideo_conf = {
    .opts = (const m_option_t[]) {
        OPT_INTRANGE("w", width, 0, 1, 8192),
        OPT_INTRANGE("h", height, 0, 1, 8192),
        OPT_GENERAL(int, "format", vformat, 0, .type = &m_option_type_fourcc),
        OPT_IMAGEFORMAT("mp-format", mp_format, 0),
        OPT_STRING("codec", codec, 0),
        OPT_FLOATRANGE("fps", fps, 0, 0.001, 1000),
        OPT_INTRANGE("size", imgsize, 0, 1, 8192 * 8192 * 4),
        {0}
    },
    .size = sizeof(struct demux_rawvideo_opts),
    .defaults = &(const struct demux_rawvideo_opts){
        .vformat = MP_FOURCC_I420,
        .width = 1280,
        .height = 720,
        .fps = 25,
    },
};

struct priv {
    int frame_size;
    int read_frames;
    double frame_rate;
};

static int demux_rawaudio_open(demuxer_t *demuxer, enum demux_check check)
{
    struct demux_rawaudio_opts *opts = demuxer->opts->demux_rawaudio;
    struct sh_stream *sh;
    sh_audio_t *sh_audio;

    if (check != DEMUX_CHECK_REQUEST && check != DEMUX_CHECK_FORCE)
        return -1;

    sh = new_sh_stream(demuxer, STREAM_AUDIO);
    sh_audio = sh->audio;
    sh_audio->channels = opts->channels;
    sh_audio->force_channels = true;
    sh_audio->samplerate = opts->samplerate;

    int f = opts->aformat;
    // See PCM():        sign   float  bits    endian
    mp_set_pcm_codec(sh, f & 1, f & 2, f >> 3, f & 4);
    int samplesize = ((f >> 3) + 7) / 8;

    struct priv *p = talloc_ptrtype(demuxer, p);
    demuxer->priv = p;
    *p = (struct priv) {
        .frame_size = samplesize * sh_audio->channels.num,
        .frame_rate = sh_audio->samplerate,
        .read_frames = sh_audio->samplerate / 8,
    };

    return 0;
}

static int demux_rawvideo_open(demuxer_t *demuxer, enum demux_check check)
{
    struct demux_rawvideo_opts *opts = demuxer->opts->demux_rawvideo;
    struct sh_stream *sh;
    sh_video_t *sh_video;

    if (check != DEMUX_CHECK_REQUEST && check != DEMUX_CHECK_FORCE)
        return -1;

    int width = opts->width;
    int height = opts->height;

    if (!width || !height) {
        MP_ERR(demuxer, "rawvideo: width or height not specified!\n");
        return -1;
    }

    const char *decoder = "rawvideo";
    int imgfmt = opts->vformat;
    int imgsize = opts->imgsize;
    if (opts->mp_format && !IMGFMT_IS_HWACCEL(opts->mp_format)) {
        decoder = "mp-rawvideo";
        imgfmt = opts->mp_format;
        if (!imgsize) {
            struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(opts->mp_format);
            for (int p = 0; p < desc.num_planes; p++) {
                imgsize += ((width >> desc.xs[p]) * (height >> desc.ys[p]) *
                            desc.bpp[p] + 7) / 8;
            }
        }
    } else if (opts->codec && opts->codec[0])
        decoder = talloc_strdup(demuxer, opts->codec);

    if (!imgsize) {
        int bpp = 0;
        switch (imgfmt) {
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
            MP_ERR(demuxer, "rawvideo: img size not specified and unknown format!\n");
            return -1;
        }
        imgsize = width * height * bpp / 8;
    }

    sh = new_sh_stream(demuxer, STREAM_VIDEO);
    sh_video = sh->video;
    sh->codec = decoder;
    sh->format = imgfmt;
    sh_video->fps = opts->fps;
    sh_video->disp_w = width;
    sh_video->disp_h = height;

    struct priv *p = talloc_ptrtype(demuxer, p);
    demuxer->priv = p;
    *p = (struct priv) {
        .frame_size = imgsize,
        .frame_rate = sh_video->fps,
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
    if (!dp) {
        MP_ERR(demuxer, "Can't read packet.\n");
        return 1;
    }

    dp->pos = stream_tell(demuxer->stream);
    dp->pts = (dp->pos  / p->frame_size) / p->frame_rate;

    int len = stream_read(demuxer->stream, dp->buffer, dp->len);
    demux_packet_shorten(dp, len);
    demux_add_packet(demuxer->streams[0], dp);

    return 1;
}

static void raw_seek(demuxer_t *demuxer, double rel_seek_secs, int flags)
{
    struct priv *p = demuxer->priv;
    stream_t *s = demuxer->stream;
    int64_t end = 0;
    stream_control(s, STREAM_CTRL_GET_SIZE, &end);
    int64_t pos = (flags & SEEK_ABSOLUTE) ? 0 : stream_tell(s);
    if (flags & SEEK_FACTOR)
        pos += end * rel_seek_secs;
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
        int64_t end = 0;
        if (stream_control(s, STREAM_CTRL_GET_SIZE, &end) != STREAM_OK)
            return DEMUXER_CTRL_DONTKNOW;

        *((double *) arg) = (end / p->frame_size) / p->frame_rate;
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
