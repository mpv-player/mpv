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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavutil/common.h>

#include "common/av_common.h"

#include "options/m_config.h"
#include "options/m_option.h"

#include "stream/stream.h"
#include "demux.h"
#include "stheader.h"
#include "codec_tags.h"

#include "video/fmt-conversion.h"
#include "video/img_format.h"

#include "osdep/endian.h"

struct demux_rawaudio_opts {
    struct m_channels channels;
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
        {"channels", OPT_CHANNELS(channels), .flags = M_OPT_CHANNELS_LIMITED},
        {"rate", OPT_INT(samplerate), M_RANGE(1000, 8 * 48000)},
        {"format", OPT_CHOICE(aformat,
            {"u8",      PCM(0, 0,  8, 0)},
            {"s8",      PCM(1, 0,  8, 0)},
            {"u16le",   PCM(0, 0, 16, 0)},  {"u16be",    PCM(0, 0, 16, 1)},
            {"s16le",   PCM(1, 0, 16, 0)},  {"s16be",    PCM(1, 0, 16, 1)},
            {"u24le",   PCM(0, 0, 24, 0)},  {"u24be",    PCM(0, 0, 24, 1)},
            {"s24le",   PCM(1, 0, 24, 0)},  {"s24be",    PCM(1, 0, 24, 1)},
            {"u32le",   PCM(0, 0, 32, 0)},  {"u32be",    PCM(0, 0, 32, 1)},
            {"s32le",   PCM(1, 0, 32, 0)},  {"s32be",    PCM(1, 0, 32, 1)},
            {"floatle", PCM(0, 1, 32, 0)},  {"floatbe",  PCM(0, 1, 32, 1)},
            {"doublele",PCM(0, 1, 64, 0)},  {"doublebe", PCM(0, 1, 64, 1)},
            {"u16",     PCM(0, 0, 16, NE)},
            {"s16",     PCM(1, 0, 16, NE)},
            {"u24",     PCM(0, 0, 24, NE)},
            {"s24",     PCM(1, 0, 24, NE)},
            {"u32",     PCM(0, 0, 32, NE)},
            {"s32",     PCM(1, 0, 32, NE)},
            {"float",   PCM(0, 1, 32, NE)},
            {"double",  PCM(0, 1, 64, NE)})},
        {0}
    },
    .size = sizeof(struct demux_rawaudio_opts),
    .defaults = &(const struct demux_rawaudio_opts){
        // Note that currently, stream_cdda expects exactly these parameters!
        .channels = {
            .set = 1,
            .chmaps = (struct mp_chmap[]){ MP_CHMAP_INIT_STEREO, },
            .num_chmaps = 1,
        },
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
        {"w", OPT_INT(width), M_RANGE(1, 8192)},
        {"h", OPT_INT(height), M_RANGE(1, 8192)},
        {"format", OPT_FOURCC(vformat)},
        {"mp-format", OPT_IMAGEFORMAT(mp_format)},
        {"codec", OPT_STRING(codec)},
        {"fps", OPT_FLOAT(fps), M_RANGE(0.001, 1000)},
        {"size", OPT_INT(imgsize), M_RANGE(1, 8192 * 8192 * 4)},
        {0}
    },
    .size = sizeof(struct demux_rawvideo_opts),
    .defaults = &(const struct demux_rawvideo_opts){
        .vformat = MKTAG('I', '4', '2', '0'),
        .width = 1280,
        .height = 720,
        .fps = 25,
    },
};

struct priv {
    struct sh_stream *sh;
    int frame_size;
    int read_frames;
    double frame_rate;
};

static int generic_open(struct demuxer *demuxer)
{
    struct stream *s = demuxer->stream;
    struct priv *p = demuxer->priv;

    int64_t end = stream_get_size(s);
    if (end >= 0)
        demuxer->duration = (end / p->frame_size) / p->frame_rate;

    return 0;
}

static int demux_rawaudio_open(demuxer_t *demuxer, enum demux_check check)
{
    struct demux_rawaudio_opts *opts =
        mp_get_config_group(demuxer, demuxer->global, &demux_rawaudio_conf);

    if (check != DEMUX_CHECK_REQUEST && check != DEMUX_CHECK_FORCE)
        return -1;

    if (opts->channels.num_chmaps != 1) {
        MP_ERR(demuxer, "Invalid channels option given.\n");
        return -1;
    }

    struct sh_stream *sh = demux_alloc_sh_stream(STREAM_AUDIO);
    struct mp_codec_params *c = sh->codec;
    c->channels = opts->channels.chmaps[0];
    c->force_channels = true;
    c->samplerate = opts->samplerate;

    c->native_tb_num = 1;
    c->native_tb_den = c->samplerate;

    int f = opts->aformat;
    // See PCM():               sign   float  bits    endian
    mp_set_pcm_codec(sh->codec, f & 1, f & 2, f >> 3, f & 4);
    int samplesize = ((f >> 3) + 7) / 8;

    demux_add_sh_stream(demuxer, sh);

    struct priv *p = talloc_ptrtype(demuxer, p);
    demuxer->priv = p;
    *p = (struct priv) {
        .sh = sh,
        .frame_size = samplesize * c->channels.num,
        .frame_rate = c->samplerate,
        .read_frames = c->samplerate / 8,
    };

    return generic_open(demuxer);
}

static int demux_rawvideo_open(demuxer_t *demuxer, enum demux_check check)
{
    struct demux_rawvideo_opts *opts =
        mp_get_config_group(demuxer, demuxer->global, &demux_rawvideo_conf);

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
    int mp_imgfmt = 0;
    if (opts->mp_format && !IMGFMT_IS_HWACCEL(opts->mp_format)) {
        mp_imgfmt = opts->mp_format;
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
        case MKTAG('Y', 'V', '1', '2'):
        case MKTAG('I', '4', '2', '0'):
        case MKTAG('I', 'Y', 'U', 'V'):
            bpp = 12;
            break;
        case MKTAG('U', 'Y', 'V', 'Y'):
        case MKTAG('Y', 'U', 'Y', '2'):
            bpp = 16;
            break;
        }
        if (!bpp) {
            MP_ERR(demuxer, "rawvideo: img size not specified and unknown format!\n");
            return -1;
        }
        imgsize = width * height * bpp / 8;
    }

    struct sh_stream *sh = demux_alloc_sh_stream(STREAM_VIDEO);
    struct mp_codec_params *c = sh->codec;
    c->codec = decoder;
    c->codec_tag = imgfmt;
    c->fps = opts->fps;
    c->reliable_fps = true;
    c->disp_w = width;
    c->disp_h = height;
    if (mp_imgfmt) {
        c->lav_codecpar = avcodec_parameters_alloc();
        if (!c->lav_codecpar)
            abort();
        c->lav_codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        c->lav_codecpar->codec_id = mp_codec_to_av_codec_id(decoder);
        c->lav_codecpar->format = imgfmt2pixfmt(mp_imgfmt);
        c->lav_codecpar->width = width;
        c->lav_codecpar->height = height;
    }
    demux_add_sh_stream(demuxer, sh);

    struct priv *p = talloc_ptrtype(demuxer, p);
    demuxer->priv = p;
    *p = (struct priv) {
        .sh = sh,
        .frame_size = imgsize,
        .frame_rate = c->fps,
        .read_frames = 1,
    };

    return generic_open(demuxer);
}

static bool raw_read_packet(struct demuxer *demuxer, struct demux_packet **pkt)
{
    struct priv *p = demuxer->priv;

    if (demuxer->stream->eof)
        return false;

    struct demux_packet *dp = new_demux_packet(p->frame_size * p->read_frames);
    if (!dp) {
        MP_ERR(demuxer, "Can't read packet.\n");
        return true;
    }

    dp->keyframe = true;
    dp->pos = stream_tell(demuxer->stream);
    dp->pts = (dp->pos  / p->frame_size) / p->frame_rate;

    int len = stream_read(demuxer->stream, dp->buffer, dp->len);
    demux_packet_shorten(dp, len);

    dp->stream = p->sh->index;
    *pkt = dp;

    return true;
}

static void raw_seek(demuxer_t *demuxer, double seek_pts, int flags)
{
    struct priv *p = demuxer->priv;
    stream_t *s = demuxer->stream;
    int64_t end = stream_get_size(s);
    int64_t frame_nr = seek_pts * p->frame_rate;
    frame_nr = frame_nr - (frame_nr % p->read_frames);
    int64_t pos = frame_nr * p->frame_size;
    if (flags & SEEK_FACTOR)
        pos = end * seek_pts;
    if (pos < 0)
        pos = 0;
    if (end > 0 && pos > end)
        pos = end;
    stream_seek(s, (pos / p->frame_size) * p->frame_size);
}

const demuxer_desc_t demuxer_desc_rawaudio = {
    .name = "rawaudio",
    .desc = "Uncompressed audio",
    .open = demux_rawaudio_open,
    .read_packet = raw_read_packet,
    .seek = raw_seek,
};

const demuxer_desc_t demuxer_desc_rawvideo = {
    .name = "rawvideo",
    .desc = "Uncompressed video",
    .open = demux_rawvideo_open,
    .read_packet = raw_read_packet,
    .seek = raw_seek,
};
