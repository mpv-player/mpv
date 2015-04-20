/*
 * Copyright (C) 2004 Michael Niedermayer <michaelni@gmx.at>
 *
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

#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

#include "config.h"

#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/avstring.h>
#include <libavutil/mathematics.h>
#include <libavutil/replaygain.h>
#include <libavutil/display.h>
#include <libavutil/opt.h>

#include "options/options.h"
#include "common/msg.h"
#include "common/tags.h"
#include "common/av_common.h"
#include "misc/bstr.h"

#include "stream/stream.h"
#include "demux.h"
#include "stheader.h"
#include "options/m_option.h"


#define INITIAL_PROBE_SIZE STREAM_BUFFER_SIZE
#define PROBE_BUF_SIZE FFMIN(STREAM_MAX_BUFFER_SIZE, 2 * 1024 * 1024)


// Should correspond to IO_BUFFER_SIZE in libavformat/aviobuf.c (not public)
// libavformat (almost) always reads data in blocks of this size.
#define BIO_BUFFER_SIZE 32768

#define OPT_BASE_STRUCT struct demux_lavf_opts
struct demux_lavf_opts {
    int probesize;
    int probescore;
    float analyzeduration;
    int buffersize;
    int allow_mimetype;
    char *format;
    char *cryptokey;
    char **avopts;
    int hacks;
    int genptsmode;
};

const struct m_sub_options demux_lavf_conf = {
    .opts = (const m_option_t[]) {
        OPT_INTRANGE("probesize", probesize, 0, 32, INT_MAX),
        OPT_STRING("format", format, 0),
        OPT_FLOATRANGE("analyzeduration", analyzeduration, 0, 0, 3600),
        OPT_INTRANGE("buffersize", buffersize, 0, 1, 10 * 1024 * 1024,
                     OPTDEF_INT(BIO_BUFFER_SIZE)),
        OPT_FLAG("allow-mimetype", allow_mimetype, 0),
        OPT_INTRANGE("probescore", probescore, 0, 1, AVPROBE_SCORE_MAX),
        OPT_STRING("cryptokey", cryptokey, 0),
        OPT_FLAG("hacks", hacks, 0),
        OPT_CHOICE("genpts-mode", genptsmode, 0,
                   ({"lavf", 1}, {"no", 0})),
        OPT_KEYVALUELIST("o", avopts, 0),
        {0}
    },
    .size = sizeof(struct demux_lavf_opts),
    .defaults = &(const struct demux_lavf_opts){
        .allow_mimetype = 1,
        .hacks = 1,
        // AVPROBE_SCORE_MAX/4 + 1 is the "recommended" limit. Below that, the
        // user is supposed to retry with larger probe sizes until a higher
        // value is reached.
        .probescore = AVPROBE_SCORE_MAX/4 + 1,
    },
};

struct format_hack {
    const char *ff_name;
    const char *mime_type;
    int probescore;
    float analyzeduration;
    unsigned int if_flags;      // additional AVInputFormat.flags flags
    bool max_probe : 1;         // use probescore only if max. probe size reached
    bool ignore : 1;            // blacklisted
    bool no_stream : 1;         // do not wrap struct stream as AVIOContext
    bool use_stream_ids : 1;    // export the native stream IDs
    bool fully_read : 1;        // set demuxer.fully_read flag
    bool image_format : 1;      // expected to contain exactly 1 frame
    // Do not confuse player's position estimation (position is into external
    // segment, with e.g. HLS, player knows about the playlist main file only).
    bool clear_filepos : 1;
};

#define BLACKLIST(fmt) {fmt, .ignore = true}
#define TEXTSUB(fmt) {fmt, .fully_read = true}
#define IMAGEFMT(fmt) {fmt, .image_format = true}

static const struct format_hack format_hacks[] = {
    // for webradios
    {"aac", "audio/aacp", 25, 0.5},
    {"aac", "audio/aac",  25, 0.5},

    // some mp3 files don't detect correctly (usually id3v2 too large)
    {"mp3", "audio/mpeg", 24, 0.5},
    {"mp3", NULL,         24, .max_probe = true},

    {"hls", .no_stream = true, .clear_filepos = true},
    {"mpeg", .use_stream_ids = true},
    {"mpegts", .use_stream_ids = true},

    // In theory, such streams might contain timestamps, but virtually none do.
    {"h264", .if_flags = AVFMT_NOTIMESTAMPS },
    {"hevc", .if_flags = AVFMT_NOTIMESTAMPS },

    TEXTSUB("aqtitle"), TEXTSUB("ass"), TEXTSUB("jacosub"), TEXTSUB("microdvd"),
    TEXTSUB("mpl2"), TEXTSUB("mpsub"), TEXTSUB("pjs"), TEXTSUB("realtext"),
    TEXTSUB("sami"), TEXTSUB("srt"), TEXTSUB("stl"), TEXTSUB("subviewer"),
    TEXTSUB("subviewer1"), TEXTSUB("vplayer"), TEXTSUB("webvtt"),

    // Useless non-sense, sometimes breaks MLP2 subreader.c fallback
    BLACKLIST("tty"),
    // Let's open files with extremely generic extensions (.bin) with a
    // demuxer that doesn't have a probe function! NO.
    BLACKLIST("bin"),
    // Useless, does not work with custom streams.
    BLACKLIST("image2"),
    // Image demuxers ("<name>_pipe" is detected explicitly)
    IMAGEFMT("image2pipe"),
    {0}
};

typedef struct lavf_priv {
    char *filename;
    struct format_hack format_hack;
    AVInputFormat *avif;
    int avif_flags;
    AVFormatContext *avfc;
    AVIOContext *pb;
    int64_t last_pts;
    struct sh_stream **streams; // NULL for unknown streams
    int num_streams;
    int cur_program;
    char *mime_type;
    bool merge_track_metadata;
} lavf_priv_t;

// At least mp4 has name="mov,mp4,m4a,3gp,3g2,mj2", so we split the name
// on "," in general.
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

static int mp_read(void *opaque, uint8_t *buf, int size)
{
    struct demuxer *demuxer = opaque;
    struct stream *stream = demuxer->stream;
    int ret;

    ret = stream_read(stream, buf, size);

    MP_TRACE(demuxer, "%d=mp_read(%p, %p, %d), pos: %"PRId64", eof:%d\n",
             ret, stream, buf, size, stream_tell(stream), stream->eof);
    return ret;
}

static int64_t mp_seek(void *opaque, int64_t pos, int whence)
{
    struct demuxer *demuxer = opaque;
    struct stream *stream = demuxer->stream;
    int64_t current_pos;
    MP_TRACE(demuxer, "mp_seek(%p, %"PRId64", %d)\n", stream, pos, whence);
    if (whence == SEEK_END || whence == AVSEEK_SIZE) {
        int64_t end;
        if (stream_control(stream, STREAM_CTRL_GET_SIZE, &end) != STREAM_OK)
            return -1;
        if (whence == AVSEEK_SIZE)
            return end;
        pos += end;
    } else if (whence == SEEK_CUR) {
        pos += stream_tell(stream);
    } else if (whence != SEEK_SET) {
        return -1;
    }

    if (pos < 0)
        return -1;
    current_pos = stream_tell(stream);
    if (stream_seek(stream, pos) == 0) {
        stream_seek(stream, current_pos);
        return -1;
    }

    return pos;
}

static int64_t mp_read_seek(void *opaque, int stream_idx, int64_t ts, int flags)
{
    struct demuxer *demuxer = opaque;
    struct stream *stream = demuxer->stream;

    struct stream_avseek cmd = {
        .stream_index = stream_idx,
        .timestamp = ts,
        .flags = flags,
    };

    if (stream_control(stream, STREAM_CTRL_AVSEEK, &cmd) == STREAM_OK) {
        stream_drop_buffers(stream);
        return 0;
    }
    return AVERROR(ENOSYS);
}

static void list_formats(struct demuxer *demuxer)
{
    MP_INFO(demuxer, "Available lavf input formats:\n");
    AVInputFormat *fmt = NULL;
    while ((fmt = av_iformat_next(fmt)))
        MP_INFO(demuxer, "%15s : %s\n", fmt->name, fmt->long_name);
}

static char *remove_prefix(char *s, const char *const *prefixes)
{
    for (int n = 0; prefixes[n]; n++) {
        int len = strlen(prefixes[n]);
        if (strncmp(s, prefixes[n], len) == 0)
            return s + len;
    }
    return s;
}

static const char *const prefixes[] =
    {"ffmpeg://", "lavf://", "avdevice://", "av://", NULL};

static int lavf_check_file(demuxer_t *demuxer, enum demux_check check)
{
    struct MPOpts *opts = demuxer->opts;
    struct demux_lavf_opts *lavfdopts = opts->demux_lavf;
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
    if (s->uncached_type == STREAMTYPE_AVDEVICE) {
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

    AVInputFormat *forced_format = NULL;
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
        forced_format = av_find_input_format(format);
        if (!forced_format) {
            MP_FATAL(demuxer, "Unknown lavf format %s\n", format);
            return -1;
        }
    }

    AVProbeData avpd = {
        // Disable file-extension matching with normal checks
        .filename = check <= DEMUX_CHECK_REQUEST ? priv->filename : "",
        .buf_size = 0,
        .buf = av_mallocz(PROBE_BUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE),
    };
    if (!avpd.buf)
        return -1;

    bool final_probe = false;
    do {
        int score = 0;

        if (forced_format) {
            priv->avif = forced_format;
            score = AVPROBE_SCORE_MAX;
        } else {
            int nsize = av_clip(avpd.buf_size * 2, INITIAL_PROBE_SIZE,
                                PROBE_BUF_SIZE);
            bstr buf = stream_peek(s, nsize);
            if (buf.len <= avpd.buf_size)
                final_probe = true;
            memcpy(avpd.buf, buf.start, buf.len);
            avpd.buf_size = buf.len;

            priv->avif = av_probe_input_format2(&avpd, avpd.buf_size > 0, &score);
        }

        if (priv->avif) {
            MP_VERBOSE(demuxer, "Found '%s' at score=%d size=%d%s.\n",
                       priv->avif->name, score, avpd.buf_size,
                       forced_format ? " (forced)" : "");

            for (int n = 0; format_hacks[n].ff_name; n++) {
                const struct format_hack *entry = &format_hacks[n];
                if (!lavfdopts->hacks)
                    continue;
                if (!matches_avinputformat_name(priv, entry->ff_name))
                    continue;
                if (entry->mime_type && strcasecmp(entry->mime_type, mime_type) != 0)
                    continue;
                priv->format_hack = *entry;
                break;
            }

            if (score >= lavfdopts->probescore)
                break;

            if (priv->format_hack.probescore &&
                score >= priv->format_hack.probescore &&
                (!priv->format_hack.max_probe || final_probe))
                break;
        }

        priv->avif = NULL;
        priv->format_hack = (struct format_hack){0};
    } while (!final_probe);

    av_free(avpd.buf);

    if (priv->avif && !forced_format && priv->format_hack.ignore) {
        MP_VERBOSE(demuxer, "Format blacklisted.\n");
        priv->avif = NULL;
    }

    if (!priv->avif) {
        MP_VERBOSE(demuxer, "No format found, try lowering probescore or forcing the format.\n");
        return -1;
    }

    if (bstr_endswith0(bstr0(priv->avif->name), "_pipe")) {
        MP_VERBOSE(demuxer, "Assuming this is an image format.\n");
        priv->format_hack.image_format = true;
    }

    priv->avif_flags = priv->avif->flags | priv->format_hack.if_flags;

    demuxer->filetype = priv->avif->name;

    return 0;
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
        bool selected = stream && demux_stream_is_selected(stream) &&
                        !stream->attached_picture;
        st->discard = selected ? AVDISCARD_DEFAULT : AVDISCARD_ALL;
    }
}

static void export_replaygain(demuxer_t *demuxer, sh_audio_t *sh, AVStream *st)
{
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

        sh->replaygain_data = rgain;
    }
}

// Return a dictionary entry as (decimal) integer.
static int dict_get_decimal(AVDictionary *dict, const char *entry, int def)
{
    AVDictionaryEntry *e = av_dict_get(dict, entry, NULL, 0);
    if (e && e->value) {
        char *end = NULL;
        long int r = strtol(e->value, &end, 10);
        if (end && !end[0] && r >= INT_MIN && r <= INT_MAX)
            return r;
    }
    return def;
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
        mp_chmap_set_unknown(&sh_audio->channels, codec->channels);
        if (codec->channel_layout)
            mp_chmap_from_lavc(&sh_audio->channels, codec->channel_layout);
        sh_audio->samplerate = codec->sample_rate;
        sh_audio->bitrate = codec->bit_rate;

        export_replaygain(demuxer, sh_audio, st);

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
            if (sh->attached_picture) {
                sh->attached_picture->pts = 0;
                talloc_steal(sh, sh->attached_picture);
                sh->attached_picture->keyframe = true;
            }
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
        if (priv->format_hack.image_format)
            sh_video->fps = demuxer->opts->mf_fps;
        if (st->sample_aspect_ratio.num)
            sh_video->aspect = codec->width  * st->sample_aspect_ratio.num
                    / (float)(codec->height * st->sample_aspect_ratio.den);
        else
            sh_video->aspect = codec->width  * codec->sample_aspect_ratio.num
                    / (float)(codec->height * codec->sample_aspect_ratio.den);

        uint8_t *sd = av_stream_get_side_data(st, AV_PKT_DATA_DISPLAYMATRIX, NULL);
        if (sd)
            sh_video->rotate = -av_display_rotation_get((uint32_t *)sd);
        sh_video->rotate = ((sh_video->rotate % 360) + 360) % 360;

        // This also applies to vfw-muxed mkv, but we can't detect these easily.
        sh_video->avi_dts = matches_avinputformat_name(priv, "avi");

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
        AVDictionaryEntry *ftag = av_dict_get(st->metadata, "filename", NULL, 0);
        char *filename = ftag ? ftag->value : NULL;
        AVDictionaryEntry *mt = av_dict_get(st->metadata, "mimetype", NULL, 0);
        char *mimetype = mt ? mt->value : NULL;
        if (mimetype) {
            demuxer_add_attachment(demuxer, bstr0(filename), bstr0(mimetype),
                                   (struct bstr){codec->extradata,
                                                 codec->extradata_size});
        }
        break;
    }
    default: ;
    }

    assert(priv->num_streams == i); // directly mapped
    MP_TARRAY_APPEND(priv, priv->streams, priv->num_streams, sh);

    if (sh) {
        sh->ff_index = st->index;
        sh->codec = mp_codec_from_av_codec_id(codec->codec_id);
        sh->lav_headers = codec;

        if (st->disposition & AV_DISPOSITION_DEFAULT)
            sh->default_track = 1;
        if (priv->format_hack.use_stream_ids)
            sh->demuxer_id = st->id;
        AVDictionaryEntry *title = av_dict_get(st->metadata, "title", NULL, 0);
        if (title && title->value)
            sh->title = talloc_strdup(sh, title->value);
        AVDictionaryEntry *lang = av_dict_get(st->metadata, "language", NULL, 0);
        if (lang && lang->value)
            sh->lang = talloc_strdup(sh, lang->value);
        sh->hls_bitrate = dict_get_decimal(st->metadata, "variant_bitrate", 0);
        if (!sh->title && sh->hls_bitrate > 0)
            sh->title = talloc_asprintf(sh, "bitrate %d", sh->hls_bitrate);
        sh->missing_timestamps = !!(priv->avif_flags & AVFMT_NOTIMESTAMPS);
    }

    select_tracks(demuxer, i);
    demux_changed(demuxer, DEMUX_EVENT_STREAMS);
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
    lavf_priv_t *priv = demuxer->priv;
    if (priv->avfc->event_flags & AVFMT_EVENT_FLAG_METADATA_UPDATED) {
        mp_tags_copy_from_av_dictionary(demuxer->metadata, priv->avfc->metadata);
        priv->avfc->event_flags = 0;
        demux_changed(demuxer, DEMUX_EVENT_METADATA);
    }
    if (priv->merge_track_metadata) {
        for (int n = 0; n < priv->num_streams; n++) {
            AVStream *st = priv->streams[n] ? priv->avfc->streams[n] : NULL;
            if (st && st->event_flags & AVSTREAM_EVENT_FLAG_METADATA_UPDATED) {
                mp_tags_copy_from_av_dictionary(demuxer->metadata, st->metadata);
                st->event_flags = 0;
                demux_changed(demuxer, DEMUX_EVENT_METADATA);
            }
        }
    }
}

static int interrupt_cb(void *ctx)
{
    struct demuxer *demuxer = ctx;
    return mp_cancel_test(demuxer->stream->cancel);
}

static int demux_open_lavf(demuxer_t *demuxer, enum demux_check check)
{
    struct MPOpts *opts = demuxer->opts;
    struct demux_lavf_opts *lavfdopts = opts->demux_lavf;
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
    if (!avfc)
        return -1;

    if (lavfdopts->cryptokey)
        parse_cryptokey(avfc, lavfdopts->cryptokey);
    if (lavfdopts->genptsmode)
        avfc->flags |= AVFMT_FLAG_GENPTS;
    if (opts->index_mode != 1)
        avfc->flags |= AVFMT_FLAG_IGNIDX;

#if LIBAVFORMAT_VERSION_MICRO >= 100
    /* Keep side data as side data instead of mashing it into the packet
     * stream.
     * Note: Libav doesn't have this horrible insanity. */
    av_opt_set(avfc, "fflags", "+keepside", 0);
#endif

    if (lavfdopts->probesize) {
        if (av_opt_set_int(avfc, "probesize", lavfdopts->probesize, 0) < 0)
            MP_ERR(demuxer, "couldn't set option probesize to %u\n",
                   lavfdopts->probesize);
    }

    if (priv->format_hack.analyzeduration)
        analyze_duration = priv->format_hack.analyzeduration;
    if (lavfdopts->analyzeduration)
        analyze_duration = lavfdopts->analyzeduration;
    if (analyze_duration > 0) {
        if (av_opt_set_int(avfc, "analyzeduration",
                           analyze_duration * AV_TIME_BASE, 0) < 0)
            MP_ERR(demuxer, "demux_lavf, couldn't set option "
                   "analyzeduration to %f\n", analyze_duration);
    }

    AVDictionary *dopts = NULL;

    if ((priv->avif_flags & AVFMT_NOFILE) ||
        demuxer->stream->type == STREAMTYPE_AVDEVICE ||
        priv->format_hack.no_stream)
    {
        mp_setup_av_network_options(&dopts, demuxer->global, demuxer->log, opts);
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
        if (stream_control(demuxer->stream, STREAM_CTRL_HAS_AVSEEK, NULL) > 0)
            demuxer->seekable = true;
    }

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

    avfc->interrupt_callback = (AVIOInterruptCB){
        .callback = interrupt_cb,
        .opaque = demuxer,
    };

    mp_set_avdict(&dopts, lavfdopts->avopts);

    if (avformat_open_input(&avfc, priv->filename, priv->avif, &dopts) < 0) {
        MP_ERR(demuxer, "avformat_open_input() failed\n");
        av_dict_free(&dopts);
        return -1;
    }

    mp_avdict_print_unset(demuxer->log, MSGL_V, dopts);
    av_dict_free(&dopts);

    priv->avfc = avfc;
    if (avformat_find_stream_info(avfc, NULL) < 0) {
        MP_ERR(demuxer, "av_find_stream_info() failed\n");
        return -1;
    }

    MP_VERBOSE(demuxer, "avformat_find_stream_info() finished after %"PRId64
               " bytes.\n", stream_tell(demuxer->stream));

    for (i = 0; i < avfc->nb_chapters; i++) {
        AVChapter *c = avfc->chapters[i];
        t = av_dict_get(c->metadata, "title", NULL, 0);
        int index = demuxer_add_chapter(demuxer, t ? bstr0(t->value) : bstr0(""),
                                        c->start * av_q2d(c->time_base), i);
        mp_tags_copy_from_av_dictionary(demuxer->chapters[index].metadata, c->metadata);
    }

    add_new_streams(demuxer);

    // Often useful with OGG audio-only files, which have metadata in the audio
    // track metadata instead of the main metadata.
    if (demuxer->num_streams == 1) {
        priv->merge_track_metadata = true;
        for (int n = 0; n < priv->num_streams; n++) {
            if (priv->streams[n])
                mp_tags_copy_from_av_dictionary(demuxer->metadata, avfc->streams[n]->metadata);
        }
    }

    mp_tags_copy_from_av_dictionary(demuxer->metadata, avfc->metadata);
    update_metadata(demuxer, NULL);

    demuxer->ts_resets_possible =
        priv->avif_flags & (AVFMT_TS_DISCONT | AVFMT_NOTIMESTAMPS);

    demuxer->start_time = priv->avfc->start_time == AV_NOPTS_VALUE ?
                          0 : (double)priv->avfc->start_time / AV_TIME_BASE;

    demuxer->allow_refresh_seeks = matches_avinputformat_name(priv, "mp4");
    demuxer->fully_read = priv->format_hack.fully_read;

    return 0;
}

static int demux_lavf_fill_buffer(demuxer_t *demux)
{
    lavf_priv_t *priv = demux->priv;

    AVPacket *pkt = &(AVPacket){0};
    int r = av_read_frame(priv->avfc, pkt);
    if (r < 0) {
        av_free_packet(pkt);
        if (r == AVERROR(EAGAIN))
            return 1;
        if (r == AVERROR_EOF)
            return 0;
        MP_WARN(demux, "error reading packet.\n");
        return -1;
    }

    add_new_streams(demux);
    update_metadata(demux, pkt);

    assert(pkt->stream_index >= 0 && pkt->stream_index < priv->num_streams);
    struct sh_stream *stream = priv->streams[pkt->stream_index];
    AVStream *st = priv->avfc->streams[pkt->stream_index];

    if (!demux_stream_is_selected(stream)) {
        av_free_packet(pkt);
        return 1; // don't signal EOF if skipping a packet
    }

    struct demux_packet *dp = new_demux_packet_from_avpacket(pkt);
    if (!dp) {
        av_free_packet(pkt);
        return 1;
    }

    if (pkt->pts != AV_NOPTS_VALUE)
        dp->pts = pkt->pts * av_q2d(st->time_base);
    if (pkt->dts != AV_NOPTS_VALUE)
        dp->dts = pkt->dts * av_q2d(st->time_base);
    dp->duration = pkt->duration * av_q2d(st->time_base);
    if (pkt->convergence_duration > 0)
        dp->duration = pkt->convergence_duration * av_q2d(st->time_base);
    dp->pos = pkt->pos;
    dp->keyframe = pkt->flags & AV_PKT_FLAG_KEY;
    if (dp->pts != MP_NOPTS_VALUE) {
        priv->last_pts = dp->pts * AV_TIME_BASE;
    } else if (dp->dts != MP_NOPTS_VALUE) {
        priv->last_pts = dp->dts * AV_TIME_BASE;
    }
    av_free_packet(pkt);

    if (priv->format_hack.clear_filepos)
        dp->pos = -1;

    demux_add_packet(stream, dp);
    return 1;
}

static void demux_seek_lavf(demuxer_t *demuxer, double rel_seek_secs, int flags)
{
    lavf_priv_t *priv = demuxer->priv;
    int avsflags = 0;

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
        int64_t end = 0;
        stream_control(s, STREAM_CTRL_GET_SIZE, &end);
        if (end > 0 && demuxer->ts_resets_possible &&
            !(priv->avif_flags & AVFMT_NO_BYTE_SEEK))
        {
            avsflags |= AVSEEK_FLAG_BYTE;
            priv->last_pts = end * rel_seek_secs;
        } else if (priv->avfc->duration != 0 &&
                   priv->avfc->duration != AV_NOPTS_VALUE)
        {
            priv->last_pts = rel_seek_secs * priv->avfc->duration;
        }
    } else {
        priv->last_pts += rel_seek_secs * AV_TIME_BASE;
    }

    int r;
    if (!priv->avfc->iformat->read_seek2) {
        // Normal seeking.
        r = av_seek_frame(priv->avfc, -1, priv->last_pts, avsflags);
        if (r < 0 && (avsflags & AVSEEK_FLAG_BACKWARD)) {
            // When seeking before the beginning of the file, and seeking fails,
            // try again without the backwards flag to make it seek to the
            // beginning.
            avsflags &= ~AVSEEK_FLAG_BACKWARD;
            r = av_seek_frame(priv->avfc, -1, priv->last_pts, avsflags);
        }
    } else {
        // av_seek_frame() won't work. Use "new" seeking API. We don't use this
        // API by default, because there are some major issues.
        // Set max_ts==ts, so that demuxing starts from an earlier position in
        // the worst case.
        r = avformat_seek_file(priv->avfc, -1, INT64_MIN,
                               priv->last_pts, priv->last_pts, avsflags);
        // Similar issue as in the normal seeking codepath.
        if (r < 0) {
            r = avformat_seek_file(priv->avfc, -1, INT64_MIN,
                                   priv->last_pts, INT64_MAX, avsflags);
        }
    }
    if (r < 0) {
        char buf[180];
        av_strerror(r, buf, sizeof(buf));
        MP_VERBOSE(demuxer, "Seek failed (%s)\n", buf);
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
        // avio_flush() is designed for write-only streams, and does the wrong
        // thing when reading. Flush it manually instead.
        stream_drop_buffers(demuxer->stream);
        priv->avfc->pb->buf_ptr = priv->avfc->pb->buf_end = priv->avfc->pb->buffer;
        priv->avfc->pb->pos = stream_tell(demuxer->stream);
        av_seek_frame(priv->avfc, 0, stream_tell(demuxer->stream),
                      AVSEEK_FLAG_BYTE);
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
