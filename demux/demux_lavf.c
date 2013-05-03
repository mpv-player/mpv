/*
 * Copyright (C) 2004 Michael Niedermayer <michaelni@gmx.at>
 *
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

// #include <stdio.h>
#include <stdlib.h>
// #include <unistd.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/avstring.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include "compat/libav.h"

#include "config.h"
#include "core/options.h"
#include "core/mp_msg.h"
#include "core/av_opts.h"
#include "core/av_common.h"
#include "core/bstr.h"

#include "stream/stream.h"
#include "demux.h"
#include "stheader.h"
#include "core/m_option.h"

#define INITIAL_PROBE_SIZE STREAM_BUFFER_SIZE
#define PROBE_BUF_SIZE (2 * 1024 * 1024)

#define OPT_BASE_STRUCT struct MPOpts

const m_option_t lavfdopts_conf[] = {
    OPT_INTRANGE("probesize", lavfdopts.probesize, 0, 32, INT_MAX),
    OPT_STRING("format", lavfdopts.format, 0),
    OPT_INTRANGE("analyzeduration", lavfdopts.analyzeduration, 0, 0, INT_MAX),
    OPT_INTRANGE("probescore", lavfdopts.probescore, 0, 0, 100),
    OPT_STRING("cryptokey", lavfdopts.cryptokey, 0),
    OPT_STRING("o", lavfdopts.avopt, 0),
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

#define BIO_BUFFER_SIZE 32768

typedef struct lavf_priv {
    char *filename;
    AVInputFormat *avif;
    AVFormatContext *avfc;
    AVIOContext *pb;
    uint8_t buffer[BIO_BUFFER_SIZE];
    int autoselect_sub;
    int64_t last_pts;
    struct sh_stream **streams; // NULL for unknown streams
    int num_streams;
    int cur_program;
    bool use_dts;
    bool seek_by_bytes;
    int bitrate;
    char *mime_type;
} lavf_priv_t;

static const char *map_demuxer_mime_type[][2] = {
    {"audio/aacp", "aac"},
    {0}
};

static const char *find_demuxer_from_mime_type(char *mime_type)
{
    for (int n = 0; map_demuxer_mime_type[n][0]; n++) {
        if (strcasecmp(map_demuxer_mime_type[n][0], mime_type) == 0)
            return map_demuxer_mime_type[n][1];
    }
    return NULL;
}

static int mp_read(void *opaque, uint8_t *buf, int size)
{
    struct demuxer *demuxer = opaque;
    struct stream *stream = demuxer->stream;
    int ret;

    ret = stream_read(stream, buf, size);

    mp_msg(MSGT_HEADER, MSGL_DBG2,
           "%d=mp_read(%p, %p, %d), pos: %"PRId64", eof:%d\n",
           ret, stream, buf, size, stream_tell(stream), stream->eof);
    return ret;
}

static int64_t mp_seek(void *opaque, int64_t pos, int whence)
{
    struct demuxer *demuxer = opaque;
    struct stream *stream = demuxer->stream;
    int64_t current_pos;
    mp_msg(MSGT_HEADER, MSGL_DBG2, "mp_seek(%p, %"PRId64", %d)\n",
           stream, pos, whence);
    if (whence == SEEK_CUR)
        pos += stream_tell(stream);
    else if (whence == SEEK_END && stream->end_pos > 0)
        pos += stream->end_pos;
    else if (whence == SEEK_SET)
        pos += stream->start_pos;
    else if (whence == AVSEEK_SIZE && stream->end_pos > 0) {
        stream_update_size(stream);
        return stream->end_pos - stream->start_pos;
    } else
        return -1;

    if (pos < 0)
        return -1;
    current_pos = stream_tell(stream);
    if (stream_seek(stream, pos) == 0) {
        stream_reset(stream);
        stream_seek(stream, current_pos);
        return -1;
    }

    return pos - stream->start_pos;
}

static int64_t mp_read_seek(void *opaque, int stream_idx, int64_t ts, int flags)
{
    struct demuxer *demuxer = opaque;
    struct stream *stream = demuxer->stream;
    struct lavf_priv *priv = demuxer->priv;

    AVStream *st = priv->avfc->streams[stream_idx];
    double pts = (double)ts * st->time_base.num / st->time_base.den;
    int ret = stream_control(stream, STREAM_CTRL_SEEK_TO_TIME, &pts);
    if (ret < 0)
        ret = AVERROR(ENOSYS);
    return ret;
}

static void list_formats(void)
{
    mp_msg(MSGT_DEMUX, MSGL_INFO, "Available lavf input formats:\n");
    AVInputFormat *fmt = NULL;
    while ((fmt = av_iformat_next(fmt)))
        mp_msg(MSGT_DEMUX, MSGL_INFO, "%15s : %s\n", fmt->name, fmt->long_name);
}

static char *remove_prefix(char *s, const char **prefixes)
{
    for (int n = 0; prefixes[n]; n++) {
        int len = strlen(prefixes[n]);
        if (strncmp(s, prefixes[n], len) == 0)
            return s + len;
    }
    return s;
}

static const char *prefixes[] =
    {"ffmpeg://", "lavf://", "avdevice://", "av://", NULL};

static int lavf_check_file(demuxer_t *demuxer)
{
    struct MPOpts *opts = demuxer->opts;
    struct lavfdopts *lavfdopts = &opts->lavfdopts;
    AVProbeData avpd;
    lavf_priv_t *priv;
    int probe_data_size = 0;
    int read_size = INITIAL_PROBE_SIZE;
    int score;

    assert(!demuxer->priv);
    demuxer->priv = talloc_zero(NULL, lavf_priv_t);
    priv = demuxer->priv;
    priv->autoselect_sub = -1;

    priv->filename = demuxer->stream->url;
    if (!priv->filename) {
        priv->filename = "mp:unknown";
        mp_msg(MSGT_DEMUX, MSGL_WARN, "Stream url is not set!\n");
    }

    priv->filename = remove_prefix(priv->filename, prefixes);

    char *avdevice_format = NULL;
    if (demuxer->stream->type == STREAMTYPE_AVDEVICE) {
        // always require filename in the form "format:filename"
        char *sep = strchr(priv->filename, ':');
        if (!sep) {
            mp_msg(MSGT_DEMUX, MSGL_FATAL,
                   "Must specify filename in 'format:filename' form\n");
            return 0;
        }
        avdevice_format = talloc_strndup(priv, priv->filename,
                                         sep - priv->filename);
        priv->filename = sep + 1;
    }

    char *format = lavfdopts->format;
    if (!format)
        format = demuxer->stream->lavf_type;
    if (!format)
        format = avdevice_format;
    if (!format && demuxer->stream->mime_type)
        format = (char *)find_demuxer_from_mime_type(demuxer->stream->mime_type);
    if (format) {
        if (strcmp(format, "help") == 0) {
            list_formats();
            return 0;
        }
        priv->avif = av_find_input_format(format);
        if (!priv->avif) {
            mp_msg(MSGT_DEMUX, MSGL_FATAL, "Unknown lavf format %s\n", format);
            return 0;
        }
        mp_msg(MSGT_DEMUX, MSGL_INFO, "Forced lavf %s demuxer\n",
               priv->avif->long_name);
        goto success;
    }

    // AVPROBE_SCORE_MAX/4 + 1 is the "recommended" limit. Below that, the user
    // is supposed to retry with larger probe sizes until a higher value is
    // reached.
    int min_probe = AVPROBE_SCORE_MAX/4 + 1;
    if (lavfdopts->probescore)
        min_probe = lavfdopts->probescore;

    avpd.buf = av_mallocz(FFMAX(BIO_BUFFER_SIZE, PROBE_BUF_SIZE) +
                          FF_INPUT_BUFFER_PADDING_SIZE);
    do {
        read_size = stream_read(demuxer->stream, avpd.buf + probe_data_size,
                                read_size);
        if (read_size < 0) {
            av_free(avpd.buf);
            return 0;
        }
        probe_data_size += read_size;
        avpd.filename = priv->filename;
        avpd.buf_size = probe_data_size;

        score = 0;
        priv->avif = av_probe_input_format2(&avpd, probe_data_size > 0, &score);

        if (priv->avif) {
            mp_msg(MSGT_HEADER, MSGL_V, "Found '%s' at score=%d size=%d.\n",
                   priv->avif->name, score, probe_data_size);
        }

        if (priv->avif && score >= min_probe)
            break;

        priv->avif = NULL;
        read_size = FFMIN(2 * read_size, PROBE_BUF_SIZE - probe_data_size);
    } while (read_size > 0 && probe_data_size < PROBE_BUF_SIZE);
    av_free(avpd.buf);

    if (!priv->avif) {
        mp_msg(MSGT_HEADER, MSGL_V,
               "No format found, try lowering probescore.\n");
        return 0;
    }

success:

    demuxer->filetype = priv->avif->long_name;
    if (!demuxer->filetype)
        demuxer->filetype = priv->avif->name;

    return DEMUXER_TYPE_LAVF;
}

static bool matches_avinputformat_name(struct lavf_priv *priv,
                                       const char *name)
{
    const char *avifname = priv->avif->name;
    while (1) {
        const char *next = strchr(avifname, ',');
        if (!next)
            return !strcmp(avifname, name);
        int len = next - avifname;
        if (len == strlen(name) && !memcmp(avifname, name, len))
            return true;
        avifname = next + 1;
    }
}

static uint8_t char2int(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static void parse_cryptokey(AVFormatContext *avfc, const char *str)
{
    int len = strlen(str) / 2;
    uint8_t *key = av_mallocz(len);
    int i;
    avfc->keylen = len;
    avfc->key = key;
    for (i = 0; i < len; i++, str += 2)
        *key++ = (char2int(str[0]) << 4) | char2int(str[1]);
}

static void handle_stream(demuxer_t *demuxer, int i)
{
    lavf_priv_t *priv = demuxer->priv;
    AVFormatContext *avfc = priv->avfc;
    AVStream *st = avfc->streams[i];
    AVCodecContext *codec = st->codec;
    struct sh_stream *sh = NULL;

    st->discard = AVDISCARD_ALL;

    switch (codec->codec_type) {
    case AVMEDIA_TYPE_AUDIO: {
        sh = new_sh_stream(demuxer, STREAM_AUDIO);
        if (!sh)
            break;
        sh_audio_t *sh_audio = sh->audio;

        sh_audio->format = codec->codec_tag;

        // probably unneeded
        sh_audio->channels = codec->channels;
        sh_audio->samplerate = codec->sample_rate;
        sh_audio->i_bps = codec->bit_rate / 8;

        break;
    }
    case AVMEDIA_TYPE_VIDEO: {
        sh = new_sh_stream(demuxer, STREAM_VIDEO);
        if (!sh)
            break;
        sh_video_t *sh_video = sh->video;

        if (st->disposition & AV_DISPOSITION_ATTACHED_PIC)
            sh_video->gsh->attached_picture = true;

        sh_video->format = codec->codec_tag;
        sh_video->disp_w = codec->width;
        sh_video->disp_h = codec->height;
        /* Try to make up some frame rate value, even if it's not reliable.
         * FPS information is needed to support subtitle formats which base
         * timing on frame numbers.
         * Libavformat seems to report no "reliable" FPS value for AVI files,
         * while they are typically constant enough FPS that the value this
         * heuristic makes up works with subtitles in practice.
         */
        double fps;
        if (st->avg_frame_rate.num)
            fps = av_q2d(st->avg_frame_rate);
        else
            fps = 1.0 / FFMAX(av_q2d(st->time_base),
                              av_q2d(st->codec->time_base) *
                              st->codec->ticks_per_frame);
        sh_video->fps = fps;
        sh_video->frametime = 1 / fps;
        if (st->sample_aspect_ratio.num)
            sh_video->aspect = codec->width  * st->sample_aspect_ratio.num
                    / (float)(codec->height * st->sample_aspect_ratio.den);
        else
            sh_video->aspect = codec->width  * codec->sample_aspect_ratio.num
                    / (float)(codec->height * codec->sample_aspect_ratio.den);
        sh_video->i_bps = codec->bit_rate / 8;

        mp_msg(MSGT_DEMUX, MSGL_DBG2, "aspect= %d*%d/(%d*%d)\n",
               codec->width, codec->sample_aspect_ratio.num,
               codec->height, codec->sample_aspect_ratio.den);
        break;
    }
    case AVMEDIA_TYPE_SUBTITLE: {
        sh_sub_t *sh_sub;
        sh = new_sh_stream(demuxer, STREAM_SUB);
        if (!sh)
            break;
        sh_sub = sh->sub;

        if (codec->extradata_size) {
            sh_sub->extradata = malloc(codec->extradata_size);
            memcpy(sh_sub->extradata, codec->extradata, codec->extradata_size);
            sh_sub->extradata_len = codec->extradata_size;
        }
        st->discard = AVDISCARD_DEFAULT;
        break;
    }
    case AVMEDIA_TYPE_ATTACHMENT: {
        AVDictionaryEntry *ftag = av_dict_get(st->metadata, "filename",
                                              NULL, 0);
        char *filename = ftag ? ftag->value : NULL;
        if (st->codec->codec_id == AV_CODEC_ID_TTF)
            demuxer_add_attachment(demuxer, bstr0(filename),
                                   bstr0("application/x-truetype-font"),
                                   (struct bstr){codec->extradata,
                                                 codec->extradata_size});
        break;
    }
    default: ;
    }

    assert(priv->num_streams == i); // directly mapped
    MP_TARRAY_APPEND(priv, priv->streams, priv->num_streams, sh);

    if (sh) {
        sh->codec = mp_codec_from_av_codec_id(codec->codec_id);
        sh->lav_headers = avcodec_alloc_context3(codec->codec);
        assert(sh->lav_headers);
        avcodec_copy_context(sh->lav_headers, codec);

        if (st->disposition & AV_DISPOSITION_DEFAULT)
            sh->default_track = 1;
        if (matches_avinputformat_name(priv, "mpeg"))
            sh->demuxer_id = st->id;
        AVDictionaryEntry *title = av_dict_get(st->metadata, "title", NULL, 0);
        if (title && title->value)
            sh->title = talloc_strdup(sh, title->value);
        AVDictionaryEntry *lang = av_dict_get(st->metadata, "language", NULL, 0);
        if (lang && lang->value)
            sh->lang = talloc_strdup(sh, lang->value);
    }
}

// Add any new streams that might have been added
static void add_new_streams(demuxer_t *demuxer)
{
    lavf_priv_t *priv = demuxer->priv;
    while (priv->num_streams < priv->avfc->nb_streams)
        handle_stream(demuxer, priv->num_streams);
}

static demuxer_t *demux_open_lavf(demuxer_t *demuxer)
{
    struct MPOpts *opts = demuxer->opts;
    struct lavfdopts *lavfdopts = &opts->lavfdopts;
    AVFormatContext *avfc;
    AVDictionaryEntry *t = NULL;
    lavf_priv_t *priv = demuxer->priv;
    int i;

    // do not allow forcing the demuxer
    if (!priv->avif)
        return NULL;

    stream_seek(demuxer->stream, 0);

    avfc = avformat_alloc_context();

    if (lavfdopts->cryptokey)
        parse_cryptokey(avfc, lavfdopts->cryptokey);
    if (matches_avinputformat_name(priv, "avi")) {
        /* for avi libavformat returns the avi timestamps in .dts,
         * some made-up stuff that's not really pts in .pts */
        priv->use_dts = true;
        demuxer->timestamp_type = TIMESTAMP_TYPE_SORT;
    } else {
        if (opts->user_correct_pts != 0)
            avfc->flags |= AVFMT_FLAG_GENPTS;
    }
    if (index_mode == 0)
        avfc->flags |= AVFMT_FLAG_IGNIDX;

    if (lavfdopts->probesize) {
        if (av_opt_set_int(avfc, "probesize", lavfdopts->probesize, 0) < 0)
            mp_msg(MSGT_HEADER, MSGL_ERR,
                   "demux_lavf, couldn't set option probesize to %u\n",
                   lavfdopts->probesize);
    }
    if (lavfdopts->analyzeduration) {
        if (av_opt_set_int(avfc, "analyzeduration",
                           lavfdopts->analyzeduration * AV_TIME_BASE, 0) < 0)
            mp_msg(MSGT_HEADER, MSGL_ERR, "demux_lavf, couldn't set option "
                   "analyzeduration to %u\n", lavfdopts->analyzeduration);
    }

    if (lavfdopts->avopt) {
        if (parse_avopts(avfc, lavfdopts->avopt) < 0) {
            mp_msg(MSGT_HEADER, MSGL_ERR,
                   "Your options /%s/ look like gibberish to me pal\n",
                   lavfdopts->avopt);
            return NULL;
        }
    }

    if (!(priv->avif->flags & AVFMT_NOFILE) &&
        demuxer->stream->type != STREAMTYPE_AVDEVICE)
    {
        priv->pb = avio_alloc_context(priv->buffer, BIO_BUFFER_SIZE, 0,
                                      demuxer, mp_read, NULL, mp_seek);
        priv->pb->read_seek = mp_read_seek;
        priv->pb->seekable = demuxer->stream->end_pos
                 && (demuxer->stream->flags & MP_STREAM_SEEK) == MP_STREAM_SEEK
                ? AVIO_SEEKABLE_NORMAL : 0;
        avfc->pb = priv->pb;
    }

    if (avformat_open_input(&avfc, priv->filename, priv->avif, NULL) < 0) {
        mp_msg(MSGT_HEADER, MSGL_ERR,
               "LAVF_header: avformat_open_input() failed\n");
        return NULL;
    }

    priv->avfc = avfc;
    if (avformat_find_stream_info(avfc, NULL) < 0) {
        mp_msg(MSGT_HEADER, MSGL_ERR,
               "LAVF_header: av_find_stream_info() failed\n");
        return NULL;
    }
    /* Add metadata. */
    while ((t = av_dict_get(avfc->metadata, "", t,
                            AV_DICT_IGNORE_SUFFIX)))
        demux_info_add(demuxer, t->key, t->value);

    for (i = 0; i < avfc->nb_chapters; i++) {
        AVChapter *c = avfc->chapters[i];
        uint64_t start = av_rescale_q(c->start, c->time_base,
                                      (AVRational){1, 1000000000});
        uint64_t end   = av_rescale_q(c->end, c->time_base,
                                      (AVRational){1, 1000000000});
        t = av_dict_get(c->metadata, "title", NULL, 0);
        demuxer_add_chapter(demuxer, t ? bstr0(t->value) : bstr0(NULL),
                            start, end);
    }

    add_new_streams(demuxer);

    if (avfc->nb_programs) {
        int p;
        for (p = 0; p < avfc->nb_programs; p++) {
            AVProgram *program = avfc->programs[p];
            t = av_dict_get(program->metadata, "title", NULL, 0);
            mp_msg(MSGT_HEADER, MSGL_INFO, "LAVF: Program %d %s\n",
                   program->id, t ? t->value : "");
            mp_msg(MSGT_IDENTIFY, MSGL_V, "PROGRAM_ID=%d\n", program->id);
        }
    }

    mp_msg(MSGT_HEADER, MSGL_V, "LAVF: build %d\n", LIBAVFORMAT_BUILD);

    demuxer->ts_resets_possible = priv->avif->flags & AVFMT_TS_DISCONT;

    // disabled because unreliable per-stream bitrate values returned
    // by libavformat trigger this heuristic incorrectly and break things
#if 0
    /* libavformat sets bitrate for mpeg based on pts at start and end
     * of file, which fails for files with pts resets. So calculate our
     * own bitrate estimate. */
    if (priv->avif->flags & AVFMT_TS_DISCONT) {
        for (int i = 0; i < avfc->nb_streams; i++)
            priv->bitrate += avfc->streams[i]->codec->bit_rate;
        /* pts-based is more accurate if there are no resets; try to make
         * a somewhat reasonable guess */
        if (!avfc->duration || avfc->duration == AV_NOPTS_VALUE
            || priv->bitrate && (avfc->bit_rate < priv->bitrate / 2
                                 || avfc->bit_rate > priv->bitrate * 2))
            priv->seek_by_bytes = true;
        if (!priv->bitrate)
            priv->bitrate = 1440000;
    }
#endif
    demuxer->accurate_seek = !priv->seek_by_bytes;

    return demuxer;
}

static int destroy_avpacket(void *pkt)
{
    av_free_packet(pkt);
    return 0;
}

static int demux_lavf_fill_buffer(demuxer_t *demux, demux_stream_t *dsds)
{
    lavf_priv_t *priv = demux->priv;
    demux_packet_t *dp;
    mp_msg(MSGT_DEMUX, MSGL_DBG2, "demux_lavf_fill_buffer()\n");

    demux->filepos = stream_tell(demux->stream);

    AVPacket *pkt = talloc(NULL, AVPacket);
    if (av_read_frame(priv->avfc, pkt) < 0) {
        talloc_free(pkt);
        return 0;
    }
    talloc_set_destructor(pkt, destroy_avpacket);

    add_new_streams(demux);

    assert(pkt->stream_index >= 0 && pkt->stream_index < priv->num_streams);
    AVStream *st = priv->avfc->streams[pkt->stream_index];
    struct sh_stream *stream = priv->streams[pkt->stream_index];

    if (stream && stream->type == STREAM_SUB && demux->sub->id < 0 &&
        stream->demuxer_id == priv->autoselect_sub)
    {
        priv->autoselect_sub = -1;
        demux->sub->id = stream->stream_index;
    }

    if (!demuxer_stream_is_selected(demux, stream)) {
        talloc_free(pkt);
        return 1;
    }

    // If the packet has pointers to temporary fields that could be
    // overwritten/freed by next av_read_frame(), copy them to persistent
    // allocations so we can safely queue the packet for any length of time.
    if (av_dup_packet(pkt) < 0)
        abort();
    dp = new_demux_packet_fromdata(pkt->data, pkt->size);
    dp->avpacket = pkt;

    int64_t ts = priv->use_dts ? pkt->dts : pkt->pts;
    if (ts == AV_NOPTS_VALUE && (st->disposition & AV_DISPOSITION_ATTACHED_PIC))
        ts = 0;
    if (ts != AV_NOPTS_VALUE) {
        dp->pts = ts * av_q2d(st->time_base);
        priv->last_pts = dp->pts * AV_TIME_BASE;
        // always set duration for subtitles, even if AV_PKT_FLAG_KEY isn't set,
        // otherwise they will stay on screen to long if e.g. ASS is demuxed
        // from mkv
        if ((stream->type == STREAM_SUB || (pkt->flags & AV_PKT_FLAG_KEY)) &&
            pkt->convergence_duration > 0)
            dp->duration = pkt->convergence_duration * av_q2d(st->time_base);
    }
    dp->pos = demux->filepos;
    dp->keyframe = pkt->flags & AV_PKT_FLAG_KEY;
    // Use only one stream for stream_pts, otherwise PTS might be jumpy.
    if (stream->type == STREAM_VIDEO) {
        double pts;
        if (stream_control(demux->stream, STREAM_CTRL_GET_CURRENT_TIME, &pts) > 0)
            dp->stream_pts = pts;
    }
    demuxer_add_packet(demux, stream, dp);
    return 1;
}

static void demux_seek_lavf(demuxer_t *demuxer, float rel_seek_secs,
                            float audio_delay, int flags)
{
    lavf_priv_t *priv = demuxer->priv;
    int avsflags = 0;
    mp_msg(MSGT_DEMUX, MSGL_DBG2, "demux_seek_lavf(%p, %f, %f, %d)\n",
           demuxer, rel_seek_secs, audio_delay, flags);

    if (priv->seek_by_bytes) {
        int64_t pos = demuxer->filepos;
        rel_seek_secs *= priv->bitrate / 8;
        pos += rel_seek_secs;
        av_seek_frame(priv->avfc, -1, pos, AVSEEK_FLAG_BYTE);
        return;
    }

    if (flags & SEEK_ABSOLUTE)
        priv->last_pts = 0;
    else if (rel_seek_secs < 0)
        avsflags = AVSEEK_FLAG_BACKWARD;

    if (flags & SEEK_FORWARD)
        avsflags = 0;
    else if (flags & SEEK_BACKWARD)
        avsflags = AVSEEK_FLAG_BACKWARD;

    if (flags & SEEK_FACTOR) {
        if (demuxer->movi_end > 0 && demuxer->ts_resets_possible &&
            !(priv->avif->flags & AVFMT_NO_BYTE_SEEK))
        {
            avsflags |= AVSEEK_FLAG_BYTE;
            priv->last_pts = (demuxer->movi_end - demuxer->movi_start) *
                             rel_seek_secs;
        } else if (priv->avfc->duration != 0 &&
                   priv->avfc->duration != AV_NOPTS_VALUE)
        {
            priv->last_pts = rel_seek_secs * priv->avfc->duration;
        }
    } else {
        priv->last_pts += rel_seek_secs * AV_TIME_BASE;
    }

    if (!priv->avfc->iformat->read_seek2) {
        // Normal seeking.
        int r = av_seek_frame(priv->avfc, -1, priv->last_pts, avsflags);
        if (r < 0 && (avsflags & AVSEEK_FLAG_BACKWARD)) {
            // When seeking before the beginning of the file, and seeking fails,
            // try again without the backwards flag to make it seek to the
            // beginning.
            avsflags &= ~AVSEEK_FLAG_BACKWARD;
            av_seek_frame(priv->avfc, -1, priv->last_pts, avsflags);
        }
    } else {
        // av_seek_frame() won't work. Use "new" seeking API. We don't use this
        // API by default, because there are some major issues.
        // Set max_ts==ts, so that demuxing starts from an earlier position in
        // the worst case.
        int r = avformat_seek_file(priv->avfc, -1, INT64_MIN,
                                   priv->last_pts, priv->last_pts, avsflags);
        // Similar issue as in the normal seeking codepath.
        if (r < 0) {
            avformat_seek_file(priv->avfc, -1, INT64_MIN,
                               priv->last_pts, INT64_MAX, avsflags);
        }
    }
}

static int demux_lavf_control(demuxer_t *demuxer, int cmd, void *arg)
{
    lavf_priv_t *priv = demuxer->priv;

    switch (cmd) {
    case DEMUXER_CTRL_CORRECT_PTS:
        return DEMUXER_CTRL_OK;
    case DEMUXER_CTRL_GET_TIME_LENGTH:
        if (priv->seek_by_bytes) {
            /* Our bitrate estimate may be better than would be used in
             * otherwise similar fallback code at higher level */
            if (demuxer->movi_end <= 0)
                return DEMUXER_CTRL_DONTKNOW;
            *(double *)arg = (demuxer->movi_end - demuxer->movi_start) * 8 /
                             priv->bitrate;
            return DEMUXER_CTRL_GUESS;
        }
        if (priv->avfc->duration == 0 || priv->avfc->duration == AV_NOPTS_VALUE)
            return DEMUXER_CTRL_DONTKNOW;

        *((double *)arg) = (double)priv->avfc->duration / AV_TIME_BASE;
        return DEMUXER_CTRL_OK;

    case DEMUXER_CTRL_GET_START_TIME:
        *((double *)arg) = priv->avfc->start_time == AV_NOPTS_VALUE ?
                           0 : (double)priv->avfc->start_time / AV_TIME_BASE;
        return DEMUXER_CTRL_OK;

    case DEMUXER_CTRL_SWITCHED_TRACKS:
    {
        for (int n = 0; n < priv->num_streams; n++) {
            struct sh_stream *stream = priv->streams[n];
            AVStream *st = priv->avfc->streams[n];
            if (stream && stream->type != STREAM_SUB) {
                bool selected = demuxer_stream_is_selected(demuxer, stream);
                st->discard = selected ? AVDISCARD_NONE : AVDISCARD_ALL;
            }
        }
        return DEMUXER_CTRL_OK;
    }
    case DEMUXER_CTRL_AUTOSELECT_SUBTITLE:
    {
        demuxer->sub->id = -1;
        priv->autoselect_sub = *((int *)arg);
        return DEMUXER_CTRL_OK;
    }
    case DEMUXER_CTRL_IDENTIFY_PROGRAM:
    {
        demux_program_t *prog = arg;
        AVProgram *program;
        int p, i;
        int start;

        add_new_streams(demuxer);

        prog->vid = prog->aid = prog->sid = -2;
        if (priv->avfc->nb_programs < 1)
            return DEMUXER_CTRL_DONTKNOW;

        if (prog->progid == -1) {
            p = 0;
            while (p < priv->avfc->nb_programs && priv->avfc->programs[p]->id != priv->cur_program)
                p++;
            p = (p + 1) % priv->avfc->nb_programs;
        } else {
            for (i = 0; i < priv->avfc->nb_programs; i++)
                if (priv->avfc->programs[i]->id == prog->progid)
                    break;
            if (i == priv->avfc->nb_programs)
                return DEMUXER_CTRL_DONTKNOW;
            p = i;
        }
        start = p;
redo:
        prog->vid = prog->aid = prog->sid = -2;
        program = priv->avfc->programs[p];
        for (i = 0; i < program->nb_stream_indexes; i++) {
            struct sh_stream *stream = priv->streams[program->stream_index[i]];
            if (stream) {
                switch (stream->type) {
                case STREAM_VIDEO:
                    if (prog->vid == -2)
                        prog->vid = stream->demuxer_id;
                    break;
                case STREAM_AUDIO:
                    if (prog->aid == -2)
                        prog->aid = stream->demuxer_id;
                    break;
                case STREAM_SUB:
                    if (prog->sid == -2)
                        prog->sid = stream->demuxer_id;
                    break;
                }
            }
        }
        if (prog->progid == -1 && prog->vid == -2 && prog->aid == -2) {
            p = (p + 1) % priv->avfc->nb_programs;
            if (p == start)
                return DEMUXER_CTRL_DONTKNOW;
            goto redo;
        }
        priv->cur_program = prog->progid = program->id;
        return DEMUXER_CTRL_OK;
    }
    case DEMUXER_CTRL_RESYNC:
        /* NOTE:
         *
         * We actually want to call ff_read_frame_flush() here, but it is
         * internal.
         *
         * This function call seems to do the same for now.
         *
         * Once ff_read_frame_flush() is exported in some way, change this to
         * call the new API instead of relying on av_seek_frame() to do this
         * for us.
         */
        avio_flush(priv->avfc->pb);
        av_seek_frame(priv->avfc, 0, avio_tell(priv->avfc->pb),
                      AVSEEK_FLAG_BYTE);
        avio_flush(priv->avfc->pb);
        return DEMUXER_CTRL_OK;
    default:
        return DEMUXER_CTRL_NOTIMPL;
    }
}

static void demux_close_lavf(demuxer_t *demuxer)
{
    lavf_priv_t *priv = demuxer->priv;
    if (priv) {
        if (priv->avfc) {
            av_freep(&priv->avfc->key);
            avformat_close_input(&priv->avfc);
        }
        av_freep(&priv->pb);
        talloc_free(priv);
        demuxer->priv = NULL;
    }
}


const demuxer_desc_t demuxer_desc_lavf = {
    "libavformat demuxer",
    "lavf",
    "libavformat",
    "Michael Niedermayer",
    "supports many formats, requires libavformat",
    DEMUXER_TYPE_LAVF,
    1,
    lavf_check_file,
    demux_lavf_fill_buffer,
    demux_open_lavf,
    demux_close_lavf,
    demux_seek_lavf,
    demux_lavf_control
};
