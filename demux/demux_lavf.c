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

#include "config.h"

#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/avstring.h>
#include <libavutil/mathematics.h>
#if HAVE_AVCODEC_REPLAYGAIN_SIDE_DATA
# include <libavutil/replaygain.h>
#endif
#include <libavutil/opt.h>
#include "compat/libav.h"

#include "options/options.h"
#include "common/msg.h"
#include "common/tags.h"
#include "common/av_opts.h"
#include "common/av_common.h"
#include "bstr/bstr.h"

#include "stream/stream.h"
#include "demux.h"
#include "stheader.h"
#include "options/m_option.h"

#define INITIAL_PROBE_SIZE STREAM_BUFFER_SIZE
#define PROBE_BUF_SIZE FFMIN(STREAM_MAX_BUFFER_SIZE, 2 * 1024 * 1024)

#define OPT_BASE_STRUCT struct MPOpts

// Should correspond to IO_BUFFER_SIZE in libavformat/aviobuf.c (not public)
// libavformat (almost) always reads data in blocks of this size.
#define BIO_BUFFER_SIZE 32768

const m_option_t lavfdopts_conf[] = {
    OPT_INTRANGE("probesize", lavfdopts.probesize, 0, 32, INT_MAX),
    OPT_STRING("format", lavfdopts.format, 0),
    OPT_FLOATRANGE("analyzeduration", lavfdopts.analyzeduration, 0, 0, 3600),
    OPT_INTRANGE("buffersize", lavfdopts.buffersize, 0, 1, 10 * 1024 * 1024,
                 OPTDEF_INT(BIO_BUFFER_SIZE)),
    OPT_FLAG("allow-mimetype", lavfdopts.allow_mimetype, 0),
    OPT_INTRANGE("probescore", lavfdopts.probescore, 0, 0, 100),
    OPT_STRING("cryptokey", lavfdopts.cryptokey, 0),
    OPT_CHOICE("genpts-mode", lavfdopts.genptsmode, 0,
               ({"lavf", 1}, {"no", 0})),
    OPT_STRING("o", lavfdopts.avopt, 0),
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

#define MAX_PKT_QUEUE 50

typedef struct lavf_priv {
    char *filename;
    const struct format_hack *format_hack;
    AVInputFormat *avif;
    AVFormatContext *avfc;
    AVIOContext *pb;
    int64_t last_pts;
    struct sh_stream **streams; // NULL for unknown streams
    int num_streams;
    int cur_program;
    char *mime_type;
} lavf_priv_t;

struct format_hack {
    const char *ff_name;
    const char *mime_type;
    int probescore;
    float analyzeduration;
    bool max_probe;         // use probescore only if max. probe size reached
};

static const struct format_hack format_hacks[] = {
    // for webradios
    {"aac", "audio/aacp", 25, 0.5},
    {"aac", "audio/aac",  25, 0.5},
    {"mp3", "audio/mpeg", 25, 0.5},
    // some mp3 files don't detect correctly
    {"mp3", NULL,         24, .max_probe = true},
    {0}
};

static const char *format_blacklist[] = {
    "tty",      // Useless non-sense, sometimes breaks MLP2 subreader.c fallback
    0
};

static int mp_read(void *opaque, uint8_t *buf, int size)
{
    struct demuxer *demuxer = opaque;
    struct stream *stream = demuxer->stream;
    int ret;

    ret = stream_read(stream, buf, size);

    MP_DBG(demuxer, "%d=mp_read(%p, %p, %d), pos: %"PRId64", eof:%d\n",
           ret, stream, buf, size, stream_tell(stream), stream->eof);
    return ret;
}

static int64_t mp_seek(void *opaque, int64_t pos, int whence)
{
    struct demuxer *demuxer = opaque;
    struct stream *stream = demuxer->stream;
    int64_t current_pos;
    if (stream_manages_timeline(stream))
        return -1;
    MP_DBG(demuxer, "mp_seek(%p, %"PRId64", %d)\n",
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

static void list_formats(struct demuxer *demuxer)
{
    MP_INFO(demuxer, "Available lavf input formats:\n");
    AVInputFormat *fmt = NULL;
    while ((fmt = av_iformat_next(fmt)))
        MP_INFO(demuxer, "%15s : %s\n", fmt->name, fmt->long_name);
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

static int lavf_check_file(demuxer_t *demuxer, enum demux_check check)
{
    struct MPOpts *opts = demuxer->opts;
    struct lavfdopts *lavfdopts = &opts->lavfdopts;
    struct stream *s = demuxer->stream;
    lavf_priv_t *priv;

    assert(!demuxer->priv);
    demuxer->priv = talloc_zero(NULL, lavf_priv_t);
    priv = demuxer->priv;

    priv->filename = s->url;
    if (!priv->filename) {
        priv->filename = "mp:unknown";
        MP_WARN(demuxer, "Stream url is not set!\n");
    }

    priv->filename = remove_prefix(priv->filename, prefixes);

    char *avdevice_format = NULL;
    if (s->type == STREAMTYPE_AVDEVICE) {
        // always require filename in the form "format:filename"
        char *sep = strchr(priv->filename, ':');
        if (!sep) {
            MP_FATAL(demuxer, "Must specify filename in 'format:filename' form\n");
            return -1;
        }
        avdevice_format = talloc_strndup(priv, priv->filename,
                                         sep - priv->filename);
        priv->filename = sep + 1;
    }

    char *mime_type = demuxer->stream->mime_type;
    if (!lavfdopts->allow_mimetype || !mime_type)
        mime_type = "";

    const char *format = lavfdopts->format;
    if (!format)
        format = s->lavf_type;
    if (!format)
        format = avdevice_format;
    if (format) {
        if (strcmp(format, "help") == 0) {
            list_formats(demuxer);
            return -1;
        }
        priv->avif = av_find_input_format(format);
        if (!priv->avif) {
            MP_FATAL(demuxer, "Unknown lavf format %s\n", format);
            return -1;
        }
        MP_INFO(demuxer, "Forced lavf %s demuxer\n",
               priv->avif->long_name);
        goto success;
    }

    // AVPROBE_SCORE_MAX/4 + 1 is the "recommended" limit. Below that, the user
    // is supposed to retry with larger probe sizes until a higher value is
    // reached.
    int min_probe = AVPROBE_SCORE_MAX/4 + 1;
    if (lavfdopts->probescore)
        min_probe = lavfdopts->probescore;

    AVProbeData avpd = {
        // Disable file-extension matching with normal checks
        .filename = check <= DEMUX_CHECK_REQUEST ? priv->filename : "",
        .buf_size = 0,
        .buf = av_mallocz(PROBE_BUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE),
    };

    bool final_probe = false;
    do {
        int nsize = av_clip(avpd.buf_size * 2, INITIAL_PROBE_SIZE,
                            PROBE_BUF_SIZE);
        bstr buf = stream_peek(s, nsize);
        if (buf.len <= avpd.buf_size)
            final_probe = true;
        memcpy(avpd.buf, buf.start, buf.len);
        avpd.buf_size = buf.len;

        int score = 0;
        priv->avif = av_probe_input_format2(&avpd, avpd.buf_size > 0, &score);

        if (priv->avif) {
            MP_VERBOSE(demuxer, "Found '%s' at score=%d size=%d.\n",
                       priv->avif->name, score, avpd.buf_size);

            priv->format_hack = NULL;
            for (int n = 0; format_hacks[n].ff_name; n++) {
                const struct format_hack *entry = &format_hacks[n];
                if (strcmp(entry->ff_name, priv->avif->name) != 0)
                    continue;
                if (entry->mime_type && strcasecmp(entry->mime_type, mime_type) != 0)
                    continue;
                priv->format_hack = entry;
                break;
            }

            if (score >= min_probe)
                break;

            if (priv->format_hack) {
                if (score >= priv->format_hack->probescore &&
                    (!priv->format_hack->max_probe || final_probe))
                    break;
            }
        }

        priv->avif = NULL;
        priv->format_hack = NULL;
    } while (!final_probe);

    av_free(avpd.buf);

    if (priv->avif && !format) {
        for (int n = 0; format_blacklist[n]; n++) {
            if (strcmp(format_blacklist[n], priv->avif->name) == 0) {
                MP_VERBOSE(demuxer, "Format blacklisted.\n");
                priv->avif = NULL;
                break;
            }
        }
    }

    if (!priv->avif) {
        MP_VERBOSE(demuxer, "No format found, try lowering probescore or forcing the format.\n");
        return -1;
    }

success:

    demuxer->filetype = priv->avif->long_name;
    if (!demuxer->filetype)
        demuxer->filetype = priv->avif->name;

    return 0;
}

static bool matches_avinputformat_name(struct lavf_priv *priv,
                                       const char *name)
{
    // At least mp4 has name="mov,mp4,m4a,3gp,3g2,mj2", so we split the name
    // on "," in general.
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

static void select_tracks(struct demuxer *demuxer, int start)
{
    lavf_priv_t *priv = demuxer->priv;
    for (int n = start; n < priv->num_streams; n++) {
        struct sh_stream *stream = priv->streams[n];
        AVStream *st = priv->avfc->streams[n];
        bool selected = stream && demuxer_stream_is_selected(demuxer, stream) &&
                        !stream->attached_picture;
        st->discard = selected ? AVDISCARD_DEFAULT : AVDISCARD_ALL;
    }
}

static void export_replaygain(demuxer_t *demuxer, AVStream *st)
{
#if HAVE_AVCODEC_REPLAYGAIN_SIDE_DATA
    for (int i = 0; i < st->nb_side_data; i++) {
        AVReplayGain *av_rgain;
        struct replaygain_data *rgain;
        AVPacketSideData *src_sd = &st->side_data[i];

        if (src_sd->type != AV_PKT_DATA_REPLAYGAIN)
            continue;

        av_rgain = (AVReplayGain*)src_sd->data;
        rgain    = talloc_ptrtype(demuxer, rgain);

        rgain->track_gain = (av_rgain->track_gain != INT32_MIN) ?
            av_rgain->track_gain / 100000.0f : 0.0;

        rgain->track_peak = (av_rgain->track_peak != 0.0) ?
            av_rgain->track_peak / 100000.0f : 1.0;

        rgain->album_gain = (av_rgain->album_gain != INT32_MIN) ?
            av_rgain->album_gain / 100000.0f : 0.0;

        rgain->album_peak = (av_rgain->album_peak != 0.0) ?
            av_rgain->album_peak / 100000.0f : 1.0;

        demuxer->replaygain_data = rgain;
    }
#endif
}

static void handle_stream(demuxer_t *demuxer, int i)
{
    lavf_priv_t *priv = demuxer->priv;
    AVFormatContext *avfc = priv->avfc;
    AVStream *st = avfc->streams[i];
    AVCodecContext *codec = st->codec;
    struct sh_stream *sh = NULL;

    switch (codec->codec_type) {
    case AVMEDIA_TYPE_AUDIO: {
        sh = new_sh_stream(demuxer, STREAM_AUDIO);
        if (!sh)
            break;
        sh_audio_t *sh_audio = sh->audio;

        sh->format = codec->codec_tag;

        // probably unneeded
        mp_chmap_from_channels(&sh_audio->channels, codec->channels);
        if (codec->channel_layout)
            mp_chmap_from_lavc(&sh_audio->channels, codec->channel_layout);
        sh_audio->samplerate = codec->sample_rate;
        sh_audio->i_bps = codec->bit_rate / 8;

        export_replaygain(demuxer, st);

        break;
    }
    case AVMEDIA_TYPE_VIDEO: {
        sh = new_sh_stream(demuxer, STREAM_VIDEO);
        if (!sh)
            break;
        sh_video_t *sh_video = sh->video;

        if (st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
            sh->attached_picture = new_demux_packet_from(st->attached_pic.data,
                                                         st->attached_pic.size);
            sh->attached_picture->pts = 0;
            talloc_steal(sh, sh->attached_picture);
            sh->attached_picture->keyframe = true;
        }

        sh->format = codec->codec_tag;
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
        if (st->sample_aspect_ratio.num)
            sh_video->aspect = codec->width  * st->sample_aspect_ratio.num
                    / (float)(codec->height * st->sample_aspect_ratio.den);
        else
            sh_video->aspect = codec->width  * codec->sample_aspect_ratio.num
                    / (float)(codec->height * codec->sample_aspect_ratio.den);
        sh_video->i_bps = codec->bit_rate / 8;

        AVDictionaryEntry *rot = av_dict_get(st->metadata, "rotate", NULL, 0);
        if (rot && rot->value) {
            char *end = NULL;
            long int r = strtol(rot->value, &end, 10);
            if (end && !end[0])
                sh_video->rotate = ((r % 360) + 360) % 360;
        }

        // This also applies to vfw-muxed mkv, but we can't detect these easily.
        sh_video->avi_dts = matches_avinputformat_name(priv, "avi");

        MP_DBG(demuxer, "aspect= %d*%d/(%d*%d)\n",
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
            sh_sub->extradata = talloc_size(sh, codec->extradata_size);
            memcpy(sh_sub->extradata, codec->extradata, codec->extradata_size);
            sh_sub->extradata_len = codec->extradata_size;
        }
        sh_sub->w = codec->width;
        sh_sub->h = codec->height;

        if (matches_avinputformat_name(priv, "microdvd")) {
            AVRational r;
            if (av_opt_get_q(avfc, "subfps", AV_OPT_SEARCH_CHILDREN, &r) >= 0) {
                // File headers don't have a FPS set.
                if (r.num < 1 || r.den < 1)
                    sh_sub->frame_based = av_q2d(av_inv_q(codec->time_base));
            } else {
                // Older libavformat versions. If the FPS matches the microdvd
                // reader's default, assume it uses frame based timing.
                if (codec->time_base.num == 125 && codec->time_base.den == 2997)
                    sh_sub->frame_based = 23.976;
            }
        }
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
        sh->lav_headers = codec;

        if (st->disposition & AV_DISPOSITION_DEFAULT)
            sh->default_track = 1;
        if (matches_avinputformat_name(priv, "mpeg") ||
            matches_avinputformat_name(priv, "mpegts"))
            sh->demuxer_id = st->id;
        AVDictionaryEntry *title = av_dict_get(st->metadata, "title", NULL, 0);
        if (title && title->value)
            sh->title = talloc_strdup(sh, title->value);
        AVDictionaryEntry *lang = av_dict_get(st->metadata, "language", NULL, 0);
        if (lang && lang->value)
            sh->lang = talloc_strdup(sh, lang->value);
    }

    select_tracks(demuxer, i);
}

// Add any new streams that might have been added
static void add_new_streams(demuxer_t *demuxer)
{
    lavf_priv_t *priv = demuxer->priv;
    while (priv->num_streams < priv->avfc->nb_streams)
        handle_stream(demuxer, priv->num_streams);
}

static void update_metadata(demuxer_t *demuxer, AVPacket *pkt)
{
#if HAVE_AVCODEC_METADATA_UPDATE_SIDE_DATA
    int md_size;
    const uint8_t *md;
    md = av_packet_get_side_data(pkt, AV_PKT_DATA_METADATA_UPDATE, &md_size);
    if (md) {
        AVDictionary *dict = NULL;
        av_packet_unpack_dictionary(md, md_size, &dict);
        if (dict) {
            mp_tags_clear(demuxer->metadata);
            mp_tags_copy_from_av_dictionary(demuxer->metadata, dict);
            av_dict_free(&dict);
        }
    }
#endif
}

static int demux_open_lavf(demuxer_t *demuxer, enum demux_check check)
{
    struct MPOpts *opts = demuxer->opts;
    struct lavfdopts *lavfdopts = &opts->lavfdopts;
    AVFormatContext *avfc;
    AVDictionaryEntry *t = NULL;
    float analyze_duration = 0;
    int i;

    if (lavf_check_file(demuxer, check) < 0)
        return -1;

    lavf_priv_t *priv = demuxer->priv;
    if (!priv)
        return -1;

    avfc = avformat_alloc_context();

    if (lavfdopts->cryptokey)
        parse_cryptokey(avfc, lavfdopts->cryptokey);
    if (lavfdopts->genptsmode)
        avfc->flags |= AVFMT_FLAG_GENPTS;
    if (opts->index_mode == 0)
        avfc->flags |= AVFMT_FLAG_IGNIDX;

#if LIBAVFORMAT_VERSION_MICRO >= 100
    /* Keep side data as side data instead of mashing it into the packet
     * stream.
     * Note: Libav doesn't have this horrible insanity. */
    av_opt_set(avfc, "fflags", "+keepside", 0);
#endif

    if (lavfdopts->probesize) {
        if (av_opt_set_int(avfc, "probesize", lavfdopts->probesize, 0) < 0)
            MP_ERR(demuxer, "demux_lavf, couldn't set option probesize to %u\n",
                   lavfdopts->probesize);
    }

    if (priv->format_hack && priv->format_hack->analyzeduration)
        analyze_duration = priv->format_hack->analyzeduration;
    if (lavfdopts->analyzeduration)
        analyze_duration = lavfdopts->analyzeduration;
    if (analyze_duration > 0) {
        if (av_opt_set_int(avfc, "analyzeduration",
                           analyze_duration * AV_TIME_BASE, 0) < 0)
            MP_ERR(demuxer, "demux_lavf, couldn't set option "
                   "analyzeduration to %f\n", analyze_duration);
    }

    if (lavfdopts->avopt) {
        if (parse_avopts(avfc, lavfdopts->avopt) < 0) {
            MP_ERR(demuxer, "Your options /%s/ look like gibberish to me pal\n",
                   lavfdopts->avopt);
            return -1;
        }
    }

    if ((priv->avif->flags & AVFMT_NOFILE) ||
        demuxer->stream->type == STREAMTYPE_AVDEVICE)
    {
        // This might be incorrect.
        demuxer->seekable = true;
    } else {
        void *buffer = av_malloc(lavfdopts->buffersize);
        if (!buffer)
            return -1;
        priv->pb = avio_alloc_context(buffer, lavfdopts->buffersize, 0,
                                      demuxer, mp_read, NULL, mp_seek);
        if (!priv->pb) {
            av_free(buffer);
            return -1;
        }
        priv->pb->read_seek = mp_read_seek;
        priv->pb->seekable = demuxer->seekable ? AVIO_SEEKABLE_NORMAL : 0;
        avfc->pb = priv->pb;
    }

    AVDictionary *dopts = NULL;

    if (matches_avinputformat_name(priv, "rtsp")) {
        const char *transport = NULL;
        switch (opts->network_rtsp_transport) {
        case 1: transport = "udp";  break;
        case 2: transport = "tcp";  break;
        case 3: transport = "http"; break;
        }
        if (transport)
            av_dict_set(&dopts, "rtsp_transport", transport, 0);
    }

    if (avformat_open_input(&avfc, priv->filename, priv->avif, &dopts) < 0) {
        MP_ERR(demuxer, "LAVF_header: avformat_open_input() failed\n");
        av_dict_free(&dopts);
        return -1;
    }

    t = NULL;
    while ((t = av_dict_get(dopts, "", t, AV_DICT_IGNORE_SUFFIX))) {
        MP_VERBOSE(demuxer, "[lavf] Could not set demux option %s=%s\n",
               t->key, t->value);
    }
    av_dict_free(&dopts);

    priv->avfc = avfc;
    if (avformat_find_stream_info(avfc, NULL) < 0) {
        MP_ERR(demuxer, "LAVF_header: av_find_stream_info() failed\n");
        return -1;
    }

    MP_VERBOSE(demuxer, "demux_lavf: avformat_find_stream_info() "
           "finished after %"PRId64" bytes.\n", stream_tell(demuxer->stream));

    for (i = 0; i < avfc->nb_chapters; i++) {
        AVChapter *c = avfc->chapters[i];
        uint64_t start = av_rescale_q(c->start, c->time_base,
                                      (AVRational){1, 1000000000});
        uint64_t end   = av_rescale_q(c->end, c->time_base,
                                      (AVRational){1, 1000000000});
        t = av_dict_get(c->metadata, "title", NULL, 0);
        int index = demuxer_add_chapter(demuxer, t ? bstr0(t->value) : bstr0(""),
                                        start, end, i);
        mp_tags_copy_from_av_dictionary(demuxer->chapters[index].metadata, c->metadata);
    }

    add_new_streams(demuxer);

    mp_tags_copy_from_av_dictionary(demuxer->metadata, avfc->metadata);

    // Often useful with OGG audio-only files, which have metadata in the audio
    // track metadata instead of the main metadata.
    if (demuxer->num_streams == 1) {
        for (int n = 0; n < priv->num_streams; n++) {
            if (priv->streams[n])
                mp_tags_copy_from_av_dictionary(demuxer->metadata, avfc->streams[n]->metadata);
        }
    }

    if (avfc->nb_programs) {
        int p;
        for (p = 0; p < avfc->nb_programs; p++) {
            AVProgram *program = avfc->programs[p];
            t = av_dict_get(program->metadata, "title", NULL, 0);
            MP_INFO(demuxer, "LAVF: Program %d %s\n",
                   program->id, t ? t->value : "");
            MP_VERBOSE(demuxer, "PROGRAM_ID=%d\n", program->id);
        }
    }

    MP_VERBOSE(demuxer, "LAVF: build %d\n", LIBAVFORMAT_BUILD);

    demuxer->ts_resets_possible = priv->avif->flags & AVFMT_TS_DISCONT;

    return 0;
}

static void destroy_avpacket(void *pkt)
{
    av_free_packet(pkt);
}

static int demux_lavf_fill_buffer(demuxer_t *demux)
{
    lavf_priv_t *priv = demux->priv;
    demux_packet_t *dp;
    MP_DBG(demux, "demux_lavf_fill_buffer()\n");

    AVPacket *pkt = talloc(NULL, AVPacket);
    if (av_read_frame(priv->avfc, pkt) < 0) {
        talloc_free(pkt);
        return 0; // eof
    }
    talloc_set_destructor(pkt, destroy_avpacket);

    add_new_streams(demux);
    update_metadata(demux, pkt);

    assert(pkt->stream_index >= 0 && pkt->stream_index < priv->num_streams);
    struct sh_stream *stream = priv->streams[pkt->stream_index];
    AVStream *st = priv->avfc->streams[pkt->stream_index];

    if (!demuxer_stream_is_selected(demux, stream)) {
        talloc_free(pkt);
        return 1; // don't signal EOF if skipping a packet
    }

    // If the packet has pointers to temporary fields that could be
    // overwritten/freed by next av_read_frame(), copy them to persistent
    // allocations so we can safely queue the packet for any length of time.
    if (av_dup_packet(pkt) < 0)
        abort();

    dp = new_demux_packet_fromdata(pkt->data, pkt->size);
    dp->avpacket = talloc_steal(dp, pkt);

    if (pkt->pts != AV_NOPTS_VALUE)
        dp->pts = pkt->pts * av_q2d(st->time_base);
    if (pkt->dts != AV_NOPTS_VALUE)
        dp->dts = pkt->dts * av_q2d(st->time_base);
    dp->duration = pkt->duration * av_q2d(st->time_base);
    if (pkt->convergence_duration > 0)
        dp->duration = pkt->convergence_duration * av_q2d(st->time_base);
    dp->pos = pkt->pos;
    dp->keyframe = pkt->flags & AV_PKT_FLAG_KEY;
    // Use only one stream for stream_pts, otherwise PTS might be jumpy.
    if (stream->type == STREAM_VIDEO) {
        double pts;
        if (stream_control(demux->stream, STREAM_CTRL_GET_CURRENT_TIME, &pts) > 0)
            dp->stream_pts = pts;
    }
    if (dp->pts != MP_NOPTS_VALUE) {
        priv->last_pts = dp->pts * AV_TIME_BASE;
    } else if (dp->dts != MP_NOPTS_VALUE) {
        priv->last_pts = dp->dts * AV_TIME_BASE;
    }
    demuxer_add_packet(demux, stream, dp);
    return 1;
}

static void demux_seek_lavf(demuxer_t *demuxer, float rel_seek_secs, int flags)
{
    lavf_priv_t *priv = demuxer->priv;
    int avsflags = 0;
    MP_DBG(demuxer, "demux_seek_lavf(%p, %f, %d)\n",
           demuxer, rel_seek_secs, flags);

    if (flags & SEEK_ABSOLUTE)
        priv->last_pts = 0;
    else if (rel_seek_secs < 0)
        avsflags = AVSEEK_FLAG_BACKWARD;

    if (flags & SEEK_FORWARD)
        avsflags = 0;
    else if (flags & SEEK_BACKWARD)
        avsflags = AVSEEK_FLAG_BACKWARD;

    if (flags & SEEK_FACTOR) {
        struct stream *s = demuxer->stream;
        if (s->end_pos > 0 && demuxer->ts_resets_possible &&
            !(priv->avif->flags & AVFMT_NO_BYTE_SEEK))
        {
            avsflags |= AVSEEK_FLAG_BYTE;
            priv->last_pts = (s->end_pos - s->start_pos) * rel_seek_secs;
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
        // To make this horrible situation even worse, some lavf demuxers have
        // broken timebase handling (everything that uses
        // ff_subtitles_queue_seek()), and always uses the stream timebase. So
        // we use the timebase and stream index of the first enabled stream
        // (i.e. a stream which can participate in seeking).
        int stream_index = -1;
        AVRational time_base = {1, AV_TIME_BASE};
        for (int n = 0; n < priv->num_streams; n++) {
            struct sh_stream *stream = priv->streams[n];
            AVStream *st = priv->avfc->streams[n];
            if (stream && st->discard != AVDISCARD_ALL) {
                stream_index = n;
                time_base = st->time_base;
                break;
            }
        }
        int64_t pts = priv->last_pts;
        if (pts != AV_NOPTS_VALUE)
            pts = pts / (double)AV_TIME_BASE * av_q2d(av_inv_q(time_base));
        int r = avformat_seek_file(priv->avfc, stream_index, INT64_MIN,
                                   pts, pts, avsflags);
        // Similar issue as in the normal seeking codepath.
        if (r < 0) {
            avformat_seek_file(priv->avfc, stream_index, INT64_MIN,
                               pts, INT64_MAX, avsflags);
        }
    }
}

static int demux_lavf_control(demuxer_t *demuxer, int cmd, void *arg)
{
    lavf_priv_t *priv = demuxer->priv;

    switch (cmd) {
    case DEMUXER_CTRL_GET_TIME_LENGTH:
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
        select_tracks(demuxer, 0);
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
        stream_drop_buffers(demuxer->stream);
        avio_flush(priv->avfc->pb);
        av_seek_frame(priv->avfc, 0, stream_tell(demuxer->stream),
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
        if (priv->pb)
            av_freep(&priv->pb->buffer);
        av_freep(&priv->pb);
        talloc_free(priv);
        demuxer->priv = NULL;
    }
}


const demuxer_desc_t demuxer_desc_lavf = {
    .name = "lavf",
    .desc = "libavformat",
    .fill_buffer = demux_lavf_fill_buffer,
    .open = demux_open_lavf,
    .close = demux_close_lavf,
    .seek = demux_seek_lavf,
    .control = demux_lavf_control,
};
