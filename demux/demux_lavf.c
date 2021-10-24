/*
 * Copyright (C) 2004 Michael Niedermayer <michaelni@gmx.at>
 *
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

#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
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

#include "common/msg.h"
#include "common/tags.h"
#include "common/av_common.h"
#include "misc/bstr.h"
#include "misc/charset_conv.h"
#include "misc/thread_tools.h"

#include "stream/stream.h"
#include "demux.h"
#include "stheader.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "options/path.h"

#ifndef AV_DISPOSITION_TIMED_THUMBNAILS
#define AV_DISPOSITION_TIMED_THUMBNAILS 0
#endif
#ifndef AV_DISPOSITION_STILL_IMAGE
#define AV_DISPOSITION_STILL_IMAGE 0
#endif

#define INITIAL_PROBE_SIZE STREAM_BUFFER_SIZE
#define PROBE_BUF_SIZE (10 * 1024 * 1024)


// Should correspond to IO_BUFFER_SIZE in libavformat/aviobuf.c (not public)
// libavformat (almost) always reads data in blocks of this size.
#define BIO_BUFFER_SIZE 32768

#define OPT_BASE_STRUCT struct demux_lavf_opts
struct demux_lavf_opts {
    int probesize;
    int probeinfo;
    int probescore;
    float analyzeduration;
    int buffersize;
    int allow_mimetype;
    char *format;
    char **avopts;
    int hacks;
    char *sub_cp;
    int rtsp_transport;
    int linearize_ts;
    int propagate_opts;
};

const struct m_sub_options demux_lavf_conf = {
    .opts = (const m_option_t[]) {
        {"demuxer-lavf-probesize", OPT_INT(probesize), M_RANGE(32, INT_MAX)},
        {"demuxer-lavf-probe-info", OPT_CHOICE(probeinfo,
            {"no", 0}, {"yes", 1}, {"auto", -1}, {"nostreams", -2})},
        {"demuxer-lavf-format", OPT_STRING(format)},
        {"demuxer-lavf-analyzeduration", OPT_FLOAT(analyzeduration),
         M_RANGE(0, 3600)},
        {"demuxer-lavf-buffersize", OPT_INT(buffersize),
         M_RANGE(1, 10 * 1024 * 1024), OPTDEF_INT(BIO_BUFFER_SIZE)},
        {"demuxer-lavf-allow-mimetype", OPT_FLAG(allow_mimetype)},
        {"demuxer-lavf-probescore", OPT_INT(probescore),
         M_RANGE(1, AVPROBE_SCORE_MAX)},
        {"demuxer-lavf-hacks", OPT_FLAG(hacks)},
        {"demuxer-lavf-o", OPT_KEYVALUELIST(avopts)},
        {"sub-codepage", OPT_STRING(sub_cp)},
        {"rtsp-transport", OPT_CHOICE(rtsp_transport,
            {"lavf", 0},
            {"udp", 1},
            {"tcp", 2},
            {"http", 3},
            {"udp_multicast", 4})},
        {"demuxer-lavf-linearize-timestamps", OPT_CHOICE(linearize_ts,
            {"no", 0}, {"auto", -1}, {"yes", 1})},
        {"demuxer-lavf-propagate-opts", OPT_FLAG(propagate_opts)},
        {0}
    },
    .size = sizeof(struct demux_lavf_opts),
    .defaults = &(const struct demux_lavf_opts){
        .probeinfo = -1,
        .allow_mimetype = 1,
        .hacks = 1,
        // AVPROBE_SCORE_MAX/4 + 1 is the "recommended" limit. Below that, the
        // user is supposed to retry with larger probe sizes until a higher
        // value is reached.
        .probescore = AVPROBE_SCORE_MAX/4 + 1,
        .sub_cp = "auto",
        .rtsp_transport = 2,
        .linearize_ts = -1,
        .propagate_opts = 1,
    },
};

struct format_hack {
    const char *ff_name;
    const char *mime_type;
    int probescore;
    float analyzeduration;
    bool skipinfo : 1;          // skip avformat_find_stream_info()
    unsigned int if_flags;      // additional AVInputFormat.flags flags
    bool max_probe : 1;         // use probescore only if max. probe size reached
    bool ignore : 1;            // blacklisted
    bool no_stream : 1;         // do not wrap struct stream as AVIOContext
    bool use_stream_ids : 1;    // has a meaningful native stream IDs (export it)
    bool fully_read : 1;        // set demuxer.fully_read flag
    bool detect_charset : 1;    // format is a small text file, possibly not UTF8
    // Do not confuse player's position estimation (position is into external
    // segment, with e.g. HLS, player knows about the playlist main file only).
    bool clear_filepos : 1;
    bool linearize_audio_ts : 1;// compensate timestamp resets (audio only)
    bool fix_editlists : 1;
    bool is_network : 1;
    bool no_seek : 1;
    bool no_pcm_seek : 1;
    bool no_seek_on_no_duration : 1;
    bool readall_on_no_streamseek : 1;
};

#define BLACKLIST(fmt) {fmt, .ignore = true}
#define TEXTSUB(fmt) {fmt, .fully_read = true, .detect_charset = true}
#define TEXTSUB_UTF8(fmt) {fmt, .fully_read = true}

static const struct format_hack format_hacks[] = {
    // for webradios
    {"aac", "audio/aacp", 25, 0.5},
    {"aac", "audio/aac",  25, 0.5},

    // some mp3 files don't detect correctly (usually id3v2 too large)
    {"mp3", "audio/mpeg", 24, 0.5},
    {"mp3", NULL,         24, .max_probe = true},

    {"hls", .no_stream = true, .clear_filepos = true},
    {"dash", .no_stream = true, .clear_filepos = true},
    {"sdp", .clear_filepos = true, .is_network = true, .no_seek = true},
    {"mpeg", .use_stream_ids = true},
    {"mpegts", .use_stream_ids = true},
    {"mxf", .use_stream_ids = true},
    {"avi", .use_stream_ids = true},
    {"asf", .use_stream_ids = true},
    {"mp4", .skipinfo = true, .fix_editlists = true, .no_pcm_seek = true,
            .use_stream_ids = true},
    {"matroska", .skipinfo = true, .no_pcm_seek = true, .use_stream_ids = true},

    {"v4l2", .no_seek = true},
    {"rtsp", .no_seek_on_no_duration = true},

    // In theory, such streams might contain timestamps, but virtually none do.
    {"h264", .if_flags = AVFMT_NOTIMESTAMPS },
    {"hevc", .if_flags = AVFMT_NOTIMESTAMPS },

    // Some Ogg shoutcast streams are essentially concatenated OGG files. They
    // reset timestamps, which causes all sorts of problems.
    {"ogg", .linearize_audio_ts = true, .use_stream_ids = true},

    // At some point, FFmpeg lost the ability to read gif from unseekable
    // streams.
    {"gif", .readall_on_no_streamseek = true},

    TEXTSUB("aqtitle"), TEXTSUB("jacosub"), TEXTSUB("microdvd"),
    TEXTSUB("mpl2"), TEXTSUB("mpsub"), TEXTSUB("pjs"), TEXTSUB("realtext"),
    TEXTSUB("sami"), TEXTSUB("srt"), TEXTSUB("stl"), TEXTSUB("subviewer"),
    TEXTSUB("subviewer1"), TEXTSUB("vplayer"), TEXTSUB("ass"),

    TEXTSUB_UTF8("webvtt"),

    // Useless non-sense, sometimes breaks MLP2 subreader.c fallback
    BLACKLIST("tty"),
    // Let's open files with extremely generic extensions (.bin) with a
    // demuxer that doesn't have a probe function! NO.
    BLACKLIST("bin"),
    // Useless, does not work with custom streams.
    BLACKLIST("image2"),
    {0}
};

struct nested_stream {
    AVIOContext *id;
    int64_t last_bytes;
};

struct stream_info {
    struct sh_stream *sh;
    double last_key_pts;
    double highest_pts;
    double ts_offset;
};

typedef struct lavf_priv {
    struct stream *stream;
    bool own_stream;
    char *filename;
    struct format_hack format_hack;
    const AVInputFormat *avif;
    int avif_flags;
    AVFormatContext *avfc;
    AVIOContext *pb;
    struct stream_info **streams; // NULL for unknown streams
    int num_streams;
    char *mime_type;
    double seek_delay;

    struct demux_lavf_opts *opts;
    double mf_fps;

    bool pcm_seek_hack_disabled;
    AVStream *pcm_seek_hack;
    int pcm_seek_hack_packet_size;

    int linearize_ts;
    bool any_ts_fixed;

    int retry_counter;

    AVDictionary *av_opts;

    // Proxying nested streams.
    struct nested_stream *nested;
    int num_nested;
    int (*default_io_open)(struct AVFormatContext *s, AVIOContext **pb,
                           const char *url, int flags, AVDictionary **options);
    void (*default_io_close)(struct AVFormatContext *s, AVIOContext *pb);
} lavf_priv_t;

static void update_read_stats(struct demuxer *demuxer)
{
#if !HAVE_FFMPEG_AVIOCONTEXT_BYTES_READ
    return;
#else
    lavf_priv_t *priv = demuxer->priv;

    for (int n = 0; n < priv->num_nested; n++) {
        struct nested_stream *nest = &priv->nested[n];

        int64_t cur = nest->id->bytes_read;
        int64_t new = cur - nest->last_bytes;
        nest->last_bytes = cur;
        demux_report_unbuffered_read_bytes(demuxer, new);
    }
#endif
}

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
    lavf_priv_t *priv = demuxer->priv;
    struct stream *stream = priv->stream;
    if (!stream)
        return 0;

    int ret = stream_read_partial(stream, buf, size);

    MP_TRACE(demuxer, "%d=mp_read(%p, %p, %d), pos: %"PRId64", eof:%d\n",
             ret, stream, buf, size, stream_tell(stream), stream->eof);
    return ret ? ret : AVERROR_EOF;
}

static int64_t mp_seek(void *opaque, int64_t pos, int whence)
{
    struct demuxer *demuxer = opaque;
    lavf_priv_t *priv = demuxer->priv;
    struct stream *stream = priv->stream;
    if (!stream)
        return -1;

    MP_TRACE(demuxer, "mp_seek(%p, %"PRId64", %s)\n", stream, pos,
             whence == SEEK_END ? "end" :
             whence == SEEK_CUR ? "cur" :
             whence == SEEK_SET ? "set" : "size");
    if (whence == SEEK_END || whence == AVSEEK_SIZE) {
        int64_t end = stream_get_size(stream);
        if (end < 0)
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

    int64_t current_pos = stream_tell(stream);
    if (stream_seek(stream, pos) == 0) {
        stream_seek(stream, current_pos);
        return -1;
    }

    return pos;
}

static int64_t mp_read_seek(void *opaque, int stream_idx, int64_t ts, int flags)
{
    struct demuxer *demuxer = opaque;
    lavf_priv_t *priv = demuxer->priv;
    struct stream *stream = priv->stream;

    struct stream_avseek cmd = {
        .stream_index = stream_idx,
        .timestamp = ts,
        .flags = flags,
    };

    if (stream && stream_control(stream, STREAM_CTRL_AVSEEK, &cmd) == STREAM_OK) {
        stream_drop_buffers(stream);
        return 0;
    }
    return AVERROR(ENOSYS);
}

static void list_formats(struct demuxer *demuxer)
{
    MP_INFO(demuxer, "Available lavf input formats:\n");
    const AVInputFormat *fmt;
    void *iter = NULL;
    while ((fmt = av_demuxer_iterate(&iter)))
        MP_INFO(demuxer, "%15s : %s\n", fmt->name, fmt->long_name);
}

static void convert_charset(struct demuxer *demuxer)
{
    lavf_priv_t *priv = demuxer->priv;
    char *cp = priv->opts->sub_cp;
    if (!cp || !cp[0] || mp_charset_is_utf8(cp))
        return;
    bstr data = stream_read_complete(priv->stream, NULL, 128 * 1024 * 1024);
    if (!data.start) {
        MP_WARN(demuxer, "File too big (or error reading) - skip charset probing.\n");
        return;
    }
    void *alloc = data.start;
    cp = (char *)mp_charset_guess(priv, demuxer->log, data, cp, 0);
    if (cp && !mp_charset_is_utf8(cp))
        MP_INFO(demuxer, "Using subtitle charset: %s\n", cp);
    // libavformat transparently converts UTF-16 to UTF-8
    if (!mp_charset_is_utf16(cp) && !mp_charset_is_utf8(cp)) {
        bstr conv = mp_iconv_to_utf8(demuxer->log, data, cp, MP_ICONV_VERBOSE);
        if (conv.start && conv.start != data.start)
            talloc_steal(alloc, conv.start);
        if (conv.start)
            data = conv;
    }
    if (data.start) {
        priv->stream = stream_memory_open(demuxer->global, data.start, data.len);
        priv->own_stream = true;
    }
    talloc_free(alloc);
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
    lavf_priv_t *priv = demuxer->priv;
    struct demux_lavf_opts *lavfdopts = priv->opts;
    struct stream *s = priv->stream;

    priv->filename = remove_prefix(s->url, prefixes);

    char *avdevice_format = NULL;
    if (s->info && strcmp(s->info->name, "avdevice") == 0) {
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

    char *mime_type = s->mime_type;
    if (!lavfdopts->allow_mimetype || !mime_type)
        mime_type = "";

    const AVInputFormat *forced_format = NULL;
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
        .buf = av_mallocz(PROBE_BUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE),
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
            nsize = stream_read_peek(s, avpd.buf, nsize);
            if (nsize <= avpd.buf_size)
                final_probe = true;
            avpd.buf_size = nsize;

            priv->avif = av_probe_input_format2(&avpd, avpd.buf_size > 0, &score);
        }

        if (priv->avif) {
            MP_VERBOSE(demuxer, "Found '%s' at score=%d size=%d%s.\n",
                       priv->avif->name, score, avpd.buf_size,
                       forced_format ? " (forced)" : "");

            for (int n = 0; lavfdopts->hacks && format_hacks[n].ff_name; n++) {
                const struct format_hack *entry = &format_hacks[n];
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

    if (lavfdopts->hacks)
        priv->avif_flags = priv->avif->flags | priv->format_hack.if_flags;

    priv->linearize_ts = lavfdopts->linearize_ts;
    if (priv->linearize_ts < 0 && !priv->format_hack.linearize_audio_ts)
        priv->linearize_ts = 0;

    demuxer->filetype = priv->avif->name;

    if (priv->format_hack.detect_charset)
        convert_charset(demuxer);

    return 0;
}

static char *replace_idx_ext(void *ta_ctx, bstr f)
{
    if (f.len < 4 || f.start[f.len - 4] != '.')
        return NULL;
    char *ext = bstr_endswith0(f, "IDX") ? "SUB" : "sub"; // match case
    return talloc_asprintf(ta_ctx, "%.*s.%s", BSTR_P(bstr_splice(f, 0, -4)), ext);
}

static void guess_and_set_vobsub_name(struct demuxer *demuxer, AVDictionary **d)
{
    lavf_priv_t *priv = demuxer->priv;
    if (!matches_avinputformat_name(priv, "vobsub"))
        return;

    void *tmp = talloc_new(NULL);
    bstr bfilename = bstr0(priv->filename);
    char *subname = NULL;
    if (mp_is_url(bfilename)) {
        // It might be a http URL, which has additional parameters after the
        // end of the actual file path.
        bstr start, end;
        if (bstr_split_tok(bfilename, "?", &start, &end)) {
            subname = replace_idx_ext(tmp, start);
            if (subname)
                subname = talloc_asprintf(tmp, "%s?%.*s", subname, BSTR_P(end));
        }
    }
    if (!subname)
        subname = replace_idx_ext(tmp, bfilename);
    if (!subname)
        subname = talloc_asprintf(tmp, "%.*s.sub", BSTR_P(bfilename));

    MP_VERBOSE(demuxer, "Assuming associated .sub file: %s\n", subname);
    av_dict_set(d, "sub_name", subname, 0);
    talloc_free(tmp);
}

static void select_tracks(struct demuxer *demuxer, int start)
{
    lavf_priv_t *priv = demuxer->priv;
    for (int n = start; n < priv->num_streams; n++) {
        struct sh_stream *stream = priv->streams[n]->sh;
        AVStream *st = priv->avfc->streams[n];
        bool selected = stream && demux_stream_is_selected(stream) &&
                        !stream->attached_picture;
        st->discard = selected ? AVDISCARD_DEFAULT : AVDISCARD_ALL;
    }
}

static void export_replaygain(demuxer_t *demuxer, struct sh_stream *sh,
                              AVStream *st)
{
    for (int i = 0; i < st->nb_side_data; i++) {
        AVReplayGain *av_rgain;
        struct replaygain_data *rgain;
        AVPacketSideData *src_sd = &st->side_data[i];

        if (src_sd->type != AV_PKT_DATA_REPLAYGAIN)
            continue;

        av_rgain = (AVReplayGain*)src_sd->data;
        rgain    = talloc_ptrtype(demuxer, rgain);
        rgain->track_gain = rgain->album_gain = 0;
        rgain->track_peak = rgain->album_peak = 1;

        // Set values in *rgain, using track gain as a fallback for album gain
        // if the latter is not present. This behavior matches that in
        // demux/demux.c's decode_rgain; if you change this, please make
        // equivalent changes there too.
        if (av_rgain->track_gain != INT32_MIN && av_rgain->track_peak != 0.0) {
            // Track gain is defined.
            rgain->track_gain = av_rgain->track_gain / 100000.0f;
            rgain->track_peak = av_rgain->track_peak / 100000.0f;

            if (av_rgain->album_gain != INT32_MIN &&
                av_rgain->album_peak != 0.0)
            {
                // Album gain is also defined.
                rgain->album_gain = av_rgain->album_gain / 100000.0f;
                rgain->album_peak = av_rgain->album_peak / 100000.0f;
            } else {
                // Album gain is undefined; fall back to track gain.
                rgain->album_gain = rgain->track_gain;
                rgain->album_peak = rgain->track_peak;
            }
        }

        // This must be run only before the stream was added, otherwise there
        // will be race conditions with accesses from the user thread.
        assert(!sh->ds);
        sh->codec->replaygain_data = rgain;
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

static void handle_new_stream(demuxer_t *demuxer, int i)
{
    lavf_priv_t *priv = demuxer->priv;
    AVFormatContext *avfc = priv->avfc;
    AVStream *st = avfc->streams[i];
    struct sh_stream *sh = NULL;
    AVCodecParameters *codec = st->codecpar;
    int lavc_delay = codec->initial_padding;

    switch (codec->codec_type) {
    case AVMEDIA_TYPE_AUDIO: {
        sh = demux_alloc_sh_stream(STREAM_AUDIO);

        // probably unneeded
        mp_chmap_set_unknown(&sh->codec->channels, codec->channels);
        if (codec->channel_layout)
            mp_chmap_from_lavc(&sh->codec->channels, codec->channel_layout);
        sh->codec->samplerate = codec->sample_rate;
        sh->codec->bitrate = codec->bit_rate;

        double delay = 0;
        if (codec->sample_rate > 0)
            delay = lavc_delay / (double)codec->sample_rate;
        priv->seek_delay = MPMAX(priv->seek_delay, delay);

        export_replaygain(demuxer, sh, st);

        sh->seek_preroll = delay;

        break;
    }
    case AVMEDIA_TYPE_VIDEO: {
        sh = demux_alloc_sh_stream(STREAM_VIDEO);

        if ((st->disposition & AV_DISPOSITION_ATTACHED_PIC) &&
            !(st->disposition & AV_DISPOSITION_TIMED_THUMBNAILS))
        {
            sh->attached_picture =
                new_demux_packet_from_avpacket(&st->attached_pic);
            if (sh->attached_picture) {
                sh->attached_picture->pts = 0;
                talloc_steal(sh, sh->attached_picture);
                sh->attached_picture->keyframe = true;
            }
        }

        if (!sh->attached_picture) {
            // A real video stream probably means it's a packet based format.
            priv->pcm_seek_hack_disabled = true;
            priv->pcm_seek_hack = NULL;
            // Also, we don't want to do this shit for ogv videos.
            if (priv->linearize_ts < 0)
                priv->linearize_ts = 0;
        }

        sh->codec->disp_w = codec->width;
        sh->codec->disp_h = codec->height;
        if (st->avg_frame_rate.num)
            sh->codec->fps = av_q2d(st->avg_frame_rate);
        if (st->nb_frames <= 1 && (
                sh->attached_picture ||
                bstr_endswith0(bstr0(priv->avif->name), "_pipe") ||
                strcmp(priv->avif->name, "alias_pix") == 0 ||
                strcmp(priv->avif->name, "gif") == 0 ||
                strcmp(priv->avif->name, "image2pipe") == 0
            )) {
            MP_VERBOSE(demuxer, "Assuming this is an image format.\n");
            sh->image = true;
            sh->codec->fps = priv->mf_fps;
        }
        sh->codec->par_w = st->sample_aspect_ratio.num;
        sh->codec->par_h = st->sample_aspect_ratio.den;

        uint8_t *sd = av_stream_get_side_data(st, AV_PKT_DATA_DISPLAYMATRIX, NULL);
        if (sd) {
            double r = av_display_rotation_get((uint32_t *)sd);
            if (!isnan(r))
                sh->codec->rotate = (((int)(-r) % 360) + 360) % 360;
        }

        // This also applies to vfw-muxed mkv, but we can't detect these easily.
        sh->codec->avi_dts = matches_avinputformat_name(priv, "avi");

        break;
    }
    case AVMEDIA_TYPE_SUBTITLE: {
        sh = demux_alloc_sh_stream(STREAM_SUB);

        if (codec->extradata_size) {
            sh->codec->extradata = talloc_size(sh, codec->extradata_size);
            memcpy(sh->codec->extradata, codec->extradata, codec->extradata_size);
            sh->codec->extradata_size = codec->extradata_size;
        }

        if (matches_avinputformat_name(priv, "microdvd")) {
            AVRational r;
            if (av_opt_get_q(avfc, "subfps", AV_OPT_SEARCH_CHILDREN, &r) >= 0) {
                // File headers don't have a FPS set.
                if (r.num < 1 || r.den < 1)
                    sh->codec->frame_based = 23.976; // default timebase
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
            demuxer_add_attachment(demuxer, filename, mimetype,
                                   codec->extradata, codec->extradata_size);
        }
        break;
    }
    default: ;
    }

    struct stream_info *info = talloc_zero(priv, struct stream_info);
    *info = (struct stream_info){
        .sh = sh,
        .last_key_pts = MP_NOPTS_VALUE,
        .highest_pts = MP_NOPTS_VALUE,
    };
    assert(priv->num_streams == i); // directly mapped
    MP_TARRAY_APPEND(priv, priv->streams, priv->num_streams, info);

    if (sh) {
        sh->ff_index = st->index;
        sh->codec->codec = mp_codec_from_av_codec_id(codec->codec_id);
        sh->codec->codec_tag = codec->codec_tag;
        sh->codec->lav_codecpar = avcodec_parameters_alloc();
        if (sh->codec->lav_codecpar)
            avcodec_parameters_copy(sh->codec->lav_codecpar, codec);
        sh->codec->native_tb_num = st->time_base.num;
        sh->codec->native_tb_den = st->time_base.den;

        if (st->disposition & AV_DISPOSITION_DEFAULT)
            sh->default_track = true;
        if (st->disposition & AV_DISPOSITION_FORCED)
            sh->forced_track = true;
        if (st->disposition & AV_DISPOSITION_DEPENDENT)
            sh->dependent_track = true;
        if (st->disposition & AV_DISPOSITION_VISUAL_IMPAIRED)
            sh->visual_impaired_track = true;
        if (st->disposition & AV_DISPOSITION_HEARING_IMPAIRED)
            sh->hearing_impaired_track = true;
        if (st->disposition & AV_DISPOSITION_STILL_IMAGE)
            sh->still_image = true;
        if (priv->format_hack.use_stream_ids)
            sh->demuxer_id = st->id;
        AVDictionaryEntry *title = av_dict_get(st->metadata, "title", NULL, 0);
        if (title && title->value)
            sh->title = talloc_strdup(sh, title->value);
        if (!sh->title && st->disposition & AV_DISPOSITION_VISUAL_IMPAIRED)
            sh->title = talloc_asprintf(sh, "visual impaired");
        if (!sh->title && st->disposition & AV_DISPOSITION_HEARING_IMPAIRED)
            sh->title = talloc_asprintf(sh, "hearing impaired");
        AVDictionaryEntry *lang = av_dict_get(st->metadata, "language", NULL, 0);
        if (lang && lang->value && strcmp(lang->value, "und") != 0)
            sh->lang = talloc_strdup(sh, lang->value);
        sh->hls_bitrate = dict_get_decimal(st->metadata, "variant_bitrate", 0);
        sh->missing_timestamps = !!(priv->avif_flags & AVFMT_NOTIMESTAMPS);
        mp_tags_copy_from_av_dictionary(sh->tags, st->metadata);
        demux_add_sh_stream(demuxer, sh);

        // Unfortunately, there is no better way to detect PCM codecs, other
        // than listing them all manually. (Or other "frameless" codecs. Or
        // rather, codecs with frames so small libavformat will put multiple of
        // them into a single packet, but not preserve these artificial packet
        // boundaries on seeking.)
        if (sh->codec->codec && strncmp(sh->codec->codec, "pcm_", 4) == 0 &&
            codec->block_align && !priv->pcm_seek_hack_disabled &&
            priv->opts->hacks && !priv->format_hack.no_pcm_seek &&
            st->time_base.num == 1 && st->time_base.den == codec->sample_rate)
        {
            if (priv->pcm_seek_hack) {
                // More than 1 audio stream => usually doesn't apply.
                priv->pcm_seek_hack_disabled = true;
                priv->pcm_seek_hack = NULL;
            } else {
                priv->pcm_seek_hack = st;
            }
        }
    }

    select_tracks(demuxer, i);
}

// Add any new streams that might have been added
static void add_new_streams(demuxer_t *demuxer)
{
    lavf_priv_t *priv = demuxer->priv;
    while (priv->num_streams < priv->avfc->nb_streams)
        handle_new_stream(demuxer, priv->num_streams);
}

static void update_metadata(demuxer_t *demuxer)
{
    lavf_priv_t *priv = demuxer->priv;
    if (priv->avfc->event_flags & AVFMT_EVENT_FLAG_METADATA_UPDATED) {
        mp_tags_copy_from_av_dictionary(demuxer->metadata, priv->avfc->metadata);
        priv->avfc->event_flags = 0;
        demux_metadata_changed(demuxer);
    }
}

static int interrupt_cb(void *ctx)
{
    struct demuxer *demuxer = ctx;
    return mp_cancel_test(demuxer->cancel);
}

static int block_io_open(struct AVFormatContext *s, AVIOContext **pb,
                         const char *url, int flags, AVDictionary **options)
{
    struct demuxer *demuxer = s->opaque;
    MP_ERR(demuxer, "Not opening '%s' due to --access-references=no.\n", url);
    return AVERROR(EACCES);
}

static int nested_io_open(struct AVFormatContext *s, AVIOContext **pb,
                          const char *url, int flags, AVDictionary **options)
{
    struct demuxer *demuxer = s->opaque;
    lavf_priv_t *priv = demuxer->priv;

    if (priv->opts->propagate_opts) {
        // Copy av_opts to options, but only entries that are not present in
        // options. (Hope this will break less by not overwriting important
        // settings.)
        AVDictionaryEntry *cur = NULL;
        while ((cur = av_dict_get(priv->av_opts, "", cur, AV_DICT_IGNORE_SUFFIX)))
        {
            if (!*options || !av_dict_get(*options, cur->key, NULL, 0)) {
                MP_TRACE(demuxer, "Nested option: '%s'='%s'\n",
                         cur->key, cur->value);
                av_dict_set(options, cur->key, cur->value, 0);
            } else {
                MP_TRACE(demuxer, "Skipping nested option: '%s'\n", cur->key);
            }
        }
    }

    int r = priv->default_io_open(s, pb, url, flags, options);
    if (r >= 0) {
        if (options)
            mp_avdict_print_unset(demuxer->log, MSGL_TRACE, *options);
        struct nested_stream nest = {
            .id = *pb,
        };
        MP_TARRAY_APPEND(priv, priv->nested, priv->num_nested, nest);
    }
    return r;
}

static void nested_io_close(struct AVFormatContext *s, AVIOContext *pb)
{
    struct demuxer *demuxer = s->opaque;
    lavf_priv_t *priv = demuxer->priv;

    for (int n = 0; n < priv->num_nested; n++) {
        if (priv->nested[n].id == pb) {
            MP_TARRAY_REMOVE_AT(priv->nested, priv->num_nested, n);
            break;
        }
    }


    priv->default_io_close(s, pb);
}

static int demux_open_lavf(demuxer_t *demuxer, enum demux_check check)
{
    AVFormatContext *avfc;
    AVDictionaryEntry *t = NULL;
    float analyze_duration = 0;
    lavf_priv_t *priv = talloc_zero(NULL, lavf_priv_t);
    demuxer->priv = priv;
    priv->stream = demuxer->stream;

    priv->opts = mp_get_config_group(priv, demuxer->global, &demux_lavf_conf);
    struct demux_lavf_opts *lavfdopts = priv->opts;

    int index_mode;
    mp_read_option_raw(demuxer->global, "index", &m_option_type_choice,
                       &index_mode);
    mp_read_option_raw(demuxer->global, "mf-fps", &m_option_type_double,
                       &priv->mf_fps);

    if (lavf_check_file(demuxer, check) < 0)
        return -1;

    avfc = avformat_alloc_context();
    if (!avfc)
        return -1;

    if (index_mode != 1)
        avfc->flags |= AVFMT_FLAG_IGNIDX;

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

    if ((priv->avif_flags & AVFMT_NOFILE) || priv->format_hack.no_stream) {
        mp_setup_av_network_options(&dopts, priv->avif->name,
                                    demuxer->global, demuxer->log);
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
        if (stream_control(priv->stream, STREAM_CTRL_HAS_AVSEEK, NULL) > 0)
            demuxer->seekable = true;
        demuxer->seekable |= priv->format_hack.fully_read;
    }

    if (matches_avinputformat_name(priv, "rtsp")) {
        const char *transport = NULL;
        switch (lavfdopts->rtsp_transport) {
        case 1: transport = "udp";  break;
        case 2: transport = "tcp";  break;
        case 3: transport = "http"; break;
        case 4: transport = "udp_multicast"; break;
        }
        if (transport)
            av_dict_set(&dopts, "rtsp_transport", transport, 0);
    }

    guess_and_set_vobsub_name(demuxer, &dopts);

    if (priv->format_hack.fix_editlists)
        av_dict_set(&dopts, "advanced_editlist", "0", 0);

    avfc->interrupt_callback = (AVIOInterruptCB){
        .callback = interrupt_cb,
        .opaque = demuxer,
    };

    avfc->opaque = demuxer;
    if (demuxer->access_references) {
        priv->default_io_open = avfc->io_open;
        priv->default_io_close = avfc->io_close;
        avfc->io_open = nested_io_open;
        avfc->io_close = nested_io_close;
    } else {
        avfc->io_open = block_io_open;
    }

    mp_set_avdict(&dopts, lavfdopts->avopts);

    if (av_dict_copy(&priv->av_opts, dopts, 0) < 0) {
        av_dict_free(&dopts);
        return -1;
    }

    if (priv->format_hack.readall_on_no_streamseek && priv->pb &&
        !priv->pb->seekable)
    {
        MP_VERBOSE(demuxer, "Non-seekable demuxer pre-read hack...\n");
        // Read incremental to avoid unnecessary large buffer sizes.
        int r = 0;
        for (int n = 16; n < 29; n++) {
            r = stream_peek(priv->stream, 1 << n);
            if (r < (1 << n))
                break;
        }
        MP_VERBOSE(demuxer, "...did read %d bytes.\n", r);
    }

    if (avformat_open_input(&avfc, priv->filename, priv->avif, &dopts) < 0) {
        MP_ERR(demuxer, "avformat_open_input() failed\n");
        av_dict_free(&dopts);
        return -1;
    }

    mp_avdict_print_unset(demuxer->log, MSGL_V, dopts);
    av_dict_free(&dopts);

    priv->avfc = avfc;

    bool probeinfo = lavfdopts->probeinfo != 0;
    switch (lavfdopts->probeinfo) {
    case -2: probeinfo = priv->avfc->nb_streams == 0; break;
    case -1: probeinfo = !priv->format_hack.skipinfo; break;
    }
    if (demuxer->params && demuxer->params->skip_lavf_probing)
        probeinfo = false;
    if (probeinfo) {
        if (avformat_find_stream_info(avfc, NULL) < 0) {
            MP_ERR(demuxer, "av_find_stream_info() failed\n");
            return -1;
        }

        MP_VERBOSE(demuxer, "avformat_find_stream_info() finished after %"PRId64
                   " bytes.\n", stream_tell(priv->stream));
    }

    for (int i = 0; i < avfc->nb_chapters; i++) {
        AVChapter *c = avfc->chapters[i];
        t = av_dict_get(c->metadata, "title", NULL, 0);
        int index = demuxer_add_chapter(demuxer, t ? t->value : "",
                                        c->start * av_q2d(c->time_base), i);
        mp_tags_copy_from_av_dictionary(demuxer->chapters[index].metadata, c->metadata);
    }

    add_new_streams(demuxer);

    mp_tags_copy_from_av_dictionary(demuxer->metadata, avfc->metadata);

    demuxer->ts_resets_possible =
        priv->avif_flags & (AVFMT_TS_DISCONT | AVFMT_NOTIMESTAMPS);

    if (avfc->start_time != AV_NOPTS_VALUE)
        demuxer->start_time = avfc->start_time / (double)AV_TIME_BASE;

    demuxer->fully_read = priv->format_hack.fully_read;

#ifdef AVFMTCTX_UNSEEKABLE
    if (avfc->ctx_flags & AVFMTCTX_UNSEEKABLE)
        demuxer->seekable = false;
#endif

    demuxer->is_network |= priv->format_hack.is_network;
    demuxer->seekable &= !priv->format_hack.no_seek;

    if (priv->avfc->duration > 0) {
        demuxer->duration = (double)priv->avfc->duration / AV_TIME_BASE;
    } else {
        double total_duration = 0;
        double av_duration = 0;
        for (int n = 0; n < priv->avfc->nb_streams; n++) {
            AVStream *st = priv->avfc->streams[n];
            if (st->duration <= 0)
                continue;
            double f_duration = st->duration * av_q2d(st->time_base);
            total_duration = MPMAX(total_duration, f_duration);
            if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO ||
                st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
                av_duration = MPMAX(av_duration, f_duration);
        }
        double duration = av_duration > 0 ? av_duration : total_duration;
        if (duration > 0)
            demuxer->duration = duration;
    }

    if (demuxer->duration < 0 && priv->format_hack.no_seek_on_no_duration)
        demuxer->seekable = false;

    // In some cases, libavformat will export bogus bullshit timestamps anyway,
    // such as with mjpeg.
    if (priv->avif_flags & AVFMT_NOTIMESTAMPS) {
        MP_WARN(demuxer,
                "This format is marked by FFmpeg as having no timestamps!\n"
                "FFmpeg will likely make up its own broken timestamps. For\n"
                "video streams you can correct this with:\n"
                "    --no-correct-pts --fps=VALUE\n"
                "with VALUE being the real framerate of the stream. You can\n"
                "expect seeking and buffering estimation to be generally\n"
                "broken as well.\n");
    }

    if (demuxer->fully_read) {
        demux_close_stream(demuxer);
        if (priv->own_stream)
            free_stream(priv->stream);
        priv->own_stream = false;
        priv->stream = NULL;
    }

    return 0;
}

static bool demux_lavf_read_packet(struct demuxer *demux,
                                   struct demux_packet **mp_pkt)
{
    lavf_priv_t *priv = demux->priv;

    AVPacket *pkt = &(AVPacket){0};
    int r = av_read_frame(priv->avfc, pkt);
    update_read_stats(demux);
    if (r < 0) {
        av_packet_unref(pkt);
        if (r == AVERROR_EOF)
            return false;
        MP_WARN(demux, "error reading packet: %s.\n", av_err2str(r));
        if (priv->retry_counter >= 10) {
            MP_ERR(demux, "...treating it as fatal error.\n");
            return false;
        }
        priv->retry_counter += 1;
        return true;
    }
    priv->retry_counter = 0;

    add_new_streams(demux);
    update_metadata(demux);

    assert(pkt->stream_index >= 0 && pkt->stream_index < priv->num_streams);
    struct stream_info *info = priv->streams[pkt->stream_index];
    struct sh_stream *stream = info->sh;
    AVStream *st = priv->avfc->streams[pkt->stream_index];

    if (!demux_stream_is_selected(stream)) {
        av_packet_unref(pkt);
        return true; // don't signal EOF if skipping a packet
    }

    struct demux_packet *dp = new_demux_packet_from_avpacket(pkt);
    if (!dp) {
        av_packet_unref(pkt);
        return true;
    }

    if (priv->pcm_seek_hack == st && !priv->pcm_seek_hack_packet_size)
        priv->pcm_seek_hack_packet_size = pkt->size;

    dp->pts = mp_pts_from_av(pkt->pts, &st->time_base);
    dp->dts = mp_pts_from_av(pkt->dts, &st->time_base);
    dp->duration = pkt->duration * av_q2d(st->time_base);
    dp->pos = pkt->pos;
    dp->keyframe = pkt->flags & AV_PKT_FLAG_KEY;
    if (pkt->flags & AV_PKT_FLAG_DISCARD)
        MP_ERR(demux, "Edit lists are not correctly supported (FFmpeg issue).\n");
    av_packet_unref(pkt);

    if (priv->format_hack.clear_filepos)
        dp->pos = -1;

    dp->stream = stream->index;

    if (priv->linearize_ts) {
        dp->pts = MP_ADD_PTS(dp->pts, info->ts_offset);
        dp->dts = MP_ADD_PTS(dp->dts, info->ts_offset);

        double pts = MP_PTS_OR_DEF(dp->pts, dp->dts);
        if (pts != MP_NOPTS_VALUE) {
            if (dp->keyframe) {
                if (pts < info->highest_pts) {
                    MP_WARN(demux, "Linearizing discontinuity: %f -> %f\n",
                            pts, info->highest_pts);
                    // Note: introduces a small discontinuity by a frame size.
                    double diff = info->highest_pts - pts;
                    dp->pts = MP_ADD_PTS(dp->pts, diff);
                    dp->dts = MP_ADD_PTS(dp->dts, diff);
                    pts += diff;
                    info->ts_offset += diff;
                    priv->any_ts_fixed = true;
                }
                info->last_key_pts = pts;
            }
            info->highest_pts = MP_PTS_MAX(info->highest_pts, pts);
        }
    }

    if (st->event_flags & AVSTREAM_EVENT_FLAG_METADATA_UPDATED) {
        st->event_flags = 0;
        struct mp_tags *tags = talloc_zero(NULL, struct mp_tags);
        mp_tags_copy_from_av_dictionary(tags, st->metadata);
        double pts = MP_PTS_OR_DEF(dp->pts, dp->dts);
        demux_stream_tags_changed(demux, stream, tags, pts);
    }

    *mp_pkt = dp;
    return true;
}

static void demux_seek_lavf(demuxer_t *demuxer, double seek_pts, int flags)
{
    lavf_priv_t *priv = demuxer->priv;
    int avsflags = 0;
    int64_t seek_pts_av = 0;
    int seek_stream = -1;

    if (priv->any_ts_fixed)  {
        // helpful message to piss of users
        MP_WARN(demuxer, "Some timestamps returned by the demuxer were linearized. "
                         "A low level seek was requested; this won't work due to "
                         "restrictions in libavformat's API. You may have more "
                         "luck by enabling or enlarging the mpv cache.\n");
    }

    if (priv->linearize_ts < 0)
        priv->linearize_ts = 0;

    if (!(flags & SEEK_FORWARD))
        avsflags = AVSEEK_FLAG_BACKWARD;

    if (flags & SEEK_FACTOR) {
        struct stream *s = priv->stream;
        int64_t end = s ? stream_get_size(s) : -1;
        if (end > 0 && demuxer->ts_resets_possible &&
            !(priv->avif_flags & AVFMT_NO_BYTE_SEEK))
        {
            avsflags |= AVSEEK_FLAG_BYTE;
            seek_pts_av = end * seek_pts;
        } else if (priv->avfc->duration != 0 &&
                   priv->avfc->duration != AV_NOPTS_VALUE)
        {
            seek_pts_av = seek_pts * priv->avfc->duration;
        }
    } else {
        if (!(flags & SEEK_FORWARD))
            seek_pts -= priv->seek_delay;
        seek_pts_av = seek_pts * AV_TIME_BASE;
    }

    // Hack to make wav seeking "deterministic". Without this, features like
    // backward playback won't work.
    if (priv->pcm_seek_hack && !priv->pcm_seek_hack_packet_size) {
        // This might for example be the initial seek. Fuck it up like the
        // bullshit it is.
        AVPacket pkt = {0};
        if (av_read_frame(priv->avfc, &pkt) >= 0)
            priv->pcm_seek_hack_packet_size = pkt.size;
        av_packet_unref(&pkt);
        add_new_streams(demuxer);
    }
    if (priv->pcm_seek_hack && priv->pcm_seek_hack_packet_size &&
        !(avsflags & AVSEEK_FLAG_BYTE))
    {
        int samples = priv->pcm_seek_hack_packet_size /
                      priv->pcm_seek_hack->codecpar->block_align;
        if (samples > 0) {
            MP_VERBOSE(demuxer, "using bullshit libavformat PCM seek hack\n");
            double pts = seek_pts_av / (double)AV_TIME_BASE;
            seek_pts_av = pts / av_q2d(priv->pcm_seek_hack->time_base);
            int64_t align = seek_pts_av % samples;
            seek_pts_av -= align;
            seek_stream = priv->pcm_seek_hack->index;
        }
    }

    int r = av_seek_frame(priv->avfc, seek_stream, seek_pts_av, avsflags);
    if (r < 0 && (avsflags & AVSEEK_FLAG_BACKWARD)) {
        // When seeking before the beginning of the file, and seeking fails,
        // try again without the backwards flag to make it seek to the
        // beginning.
        avsflags &= ~AVSEEK_FLAG_BACKWARD;
        r = av_seek_frame(priv->avfc, seek_stream, seek_pts_av, avsflags);
    }

    if (r < 0) {
        char buf[180];
        av_strerror(r, buf, sizeof(buf));
        MP_VERBOSE(demuxer, "Seek failed (%s)\n", buf);
    }

    update_read_stats(demuxer);
}

static void demux_lavf_switched_tracks(struct demuxer *demuxer)
{
    select_tracks(demuxer, 0);
}

static void demux_close_lavf(demuxer_t *demuxer)
{
    lavf_priv_t *priv = demuxer->priv;
    if (priv) {
        // This will be a dangling pointer; but see below.
        AVIOContext *leaking = priv->avfc ? priv->avfc->pb : NULL;
        avformat_close_input(&priv->avfc);
        // The ffmpeg garbage breaks its own API yet again: hls.c will call
        // io_open on the main playlist, but never calls io_close. This happens
        // to work out for us (since we don't really use custom I/O), but it's
        // still weird. Compensate.
        if (priv->num_nested == 1 && priv->nested[0].id == leaking)
            priv->num_nested = 0;
        if (priv->num_nested) {
            MP_WARN(demuxer, "Leaking %d nested connections (FFmpeg bug).\n",
                    priv->num_nested);
        }
        if (priv->pb)
            av_freep(&priv->pb->buffer);
        av_freep(&priv->pb);
        for (int n = 0; n < priv->num_streams; n++) {
            struct stream_info *info = priv->streams[n];
            if (info->sh)
                avcodec_parameters_free(&info->sh->codec->lav_codecpar);
        }
        if (priv->own_stream)
            free_stream(priv->stream);
        if (priv->av_opts)
            av_dict_free(&priv->av_opts);
        talloc_free(priv);
        demuxer->priv = NULL;
    }
}


const demuxer_desc_t demuxer_desc_lavf = {
    .name = "lavf",
    .desc = "libavformat",
    .read_packet = demux_lavf_read_packet,
    .open = demux_open_lavf,
    .close = demux_close_lavf,
    .seek = demux_seek_lavf,
    .switched_tracks = demux_lavf_switched_tracks,
};
