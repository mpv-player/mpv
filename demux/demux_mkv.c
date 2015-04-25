/*
 * Matroska demuxer
 * Copyright (C) 2004 Aurelien Jacobs <aurel@gnuage.org>
 * Based on the one written by Ronald Bultje for gstreamer
 * and on demux_mkv.cpp from Moritz Bunkus.
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
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>

#include <libavutil/common.h>
#include <libavutil/lzo.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/avstring.h>

#include <libavcodec/avcodec.h>
#include <libavcodec/version.h>

#include "config.h"

#if HAVE_ZLIB
#include <zlib.h>
#endif

#include "talloc.h"
#include "common/av_common.h"
#include "options/options.h"
#include "options/m_option.h"
#include "misc/bstr.h"
#include "stream/stream.h"
#include "video/csputils.h"
#include "demux.h"
#include "stheader.h"
#include "ebml.h"
#include "matroska.h"
#include "codec_tags.h"
#include "video/img_fourcc.h"

#include "common/msg.h"

static const unsigned char sipr_swaps[38][2] = {
    {0,63},{1,22},{2,44},{3,90},{5,81},{7,31},{8,86},{9,58},{10,36},{12,68},
    {13,39},{14,73},{15,53},{16,69},{17,57},{19,88},{20,34},{21,71},{24,46},
    {25,94},{26,54},{28,75},{29,50},{32,70},{33,92},{35,74},{38,85},{40,56},
    {42,87},{43,65},{45,59},{48,79},{49,93},{51,89},{55,95},{61,76},{67,83},
    {77,80}
};

// Map flavour to bytes per second
#define SIPR_FLAVORS 4
#define ATRC_FLAVORS 8
#define COOK_FLAVORS 34
static const int sipr_fl2bps[SIPR_FLAVORS] = { 813, 1062, 625, 2000 };
static const int atrc_fl2bps[ATRC_FLAVORS] = {
    8269, 11714, 13092, 16538, 18260, 22050, 33075, 44100 };
static const int cook_fl2bps[COOK_FLAVORS] = {
    1000, 1378, 2024, 2584, 4005, 5513, 8010, 4005, 750, 2498,
    4048, 5513, 8010, 11973, 8010, 2584, 4005, 2067, 2584, 2584,
    4005, 4005, 5513, 5513, 8010, 12059, 1550, 8010, 12059, 5513,
    12016, 16408, 22911, 33506
};

enum {
    MAX_NUM_LACES = 256,
};

typedef struct mkv_content_encoding {
    uint64_t order, type, scope;
    uint64_t comp_algo;
    uint8_t *comp_settings;
    int comp_settings_len;
} mkv_content_encoding_t;

typedef struct mkv_track {
    int tnum;
    char *name;
    struct sh_stream *stream;

    char *codec_id;
    int ms_compat;
    char *language;

    int type;

    uint32_t v_width, v_height, v_dwidth, v_dheight;
    bool v_dwidth_set, v_dheight_set;
    double v_frate;
    uint32_t colorspace;
    int stereo_mode;

    uint32_t a_formattag;
    uint32_t a_channels, a_bps;
    float a_sfreq;
    float a_osfreq;

    double default_duration;
    double codec_delay;

    int default_track;

    unsigned char *private_data;
    unsigned int private_size;

    bool parse;
    int64_t parse_timebase;
    void *parser_tmp;
    AVCodecParserContext *av_parser;
    AVCodecContext *av_parser_codec;

    /* stuff for realaudio braincancer */
    double ra_pts;              /* previous audio timestamp */
    uint32_t sub_packet_size;   ///< sub packet size, per stream
    uint32_t sub_packet_h;      ///< number of coded frames per block
    uint32_t coded_framesize;   ///< coded frame size, per stream
    uint32_t audiopk_size;      ///< audio packet size
    unsigned char *audio_buf;   ///< place to store reordered audio data
    double *audio_timestamp;    ///< timestamp for each audio packet
    uint32_t sub_packet_cnt;    ///< number of subpacket already received

    /* generic content encoding support */
    mkv_content_encoding_t *encodings;
    int num_encodings;

    /* latest added index entry for this track */
    size_t last_index_entry;
} mkv_track_t;

typedef struct mkv_index {
    int tnum;
    uint64_t timecode, duration;
    uint64_t filepos; // position of the cluster which contains the packet
} mkv_index_t;

typedef struct mkv_demuxer {
    int64_t segment_start, segment_end;

    double duration, last_pts;

    mkv_track_t **tracks;
    int num_tracks;

    struct ebml_tags *tags;

    uint64_t tc_scale, cluster_tc;

    uint64_t cluster_start;
    uint64_t cluster_end;

    mkv_index_t *indexes;
    size_t num_indexes;
    bool index_complete;

    struct header_elem {
        int32_t id;
        int64_t pos;
        bool parsed;
    } *headers;
    int num_headers;

    uint64_t skip_to_timecode;
    int v_skip_to_keyframe, a_skip_to_keyframe;
    int a_skip_preroll;
    int subtitle_preroll;

    bool index_has_durations;

    bool eof_warning;
} mkv_demuxer_t;

#define OPT_BASE_STRUCT struct demux_mkv_opts
struct demux_mkv_opts {
    int subtitle_preroll;
    double subtitle_preroll_secs;
    int probe_duration;
    int fix_timestamps;
};

const struct m_sub_options demux_mkv_conf = {
    .opts = (const m_option_t[]) {
        OPT_FLAG("subtitle-preroll", subtitle_preroll, 0),
        OPT_DOUBLE("subtitle-preroll-secs", subtitle_preroll_secs,
                   M_OPT_MIN, .min = 0),
        OPT_FLAG("probe-video-duration", probe_duration, 0),
        OPT_FLAG("fix-timestamps", fix_timestamps, 0),
        {0}
    },
    .size = sizeof(struct demux_mkv_opts),
    .defaults = &(const struct demux_mkv_opts){
        .subtitle_preroll_secs = 1.0,
        .fix_timestamps = 1,
    },
};

#define REALHEADER_SIZE    16
#define RVPROPERTIES_SIZE  34
#define RAPROPERTIES4_SIZE 56
#define RAPROPERTIES5_SIZE 70

// Maximum number of subtitle packets that are accepted for pre-roll.
// (Subtitle packets added before first A/V keyframe packet is found with seek.)
#define NUM_SUB_PREROLL_PACKETS 500

static void probe_last_timestamp(struct demuxer *demuxer);

#define AAC_SYNC_EXTENSION_TYPE 0x02b7
static int aac_get_sample_rate_index(uint32_t sample_rate)
{
    static const int srates[] = {
        92017, 75132, 55426, 46009, 37566, 27713,
        23004, 18783, 13856, 11502, 9391, 0
    };
    int i = 0;
    while (sample_rate < srates[i])
        i++;
    return i;
}

static bstr demux_mkv_decode(struct mp_log *log, mkv_track_t *track,
                             bstr data, uint32_t type)
{
    uint8_t *src = data.start;
    uint8_t *orig_src = src;
    uint8_t *dest = src;
    uint32_t size = data.len;

    for (int i = 0; i < track->num_encodings; i++) {
        struct mkv_content_encoding *enc = track->encodings + i;
        if (!(enc->scope & type))
            continue;

        if (src != dest && src != orig_src)
            talloc_free(src);
        src = dest;  // output from last iteration is new source

        if (enc->comp_algo == 0) {
#if HAVE_ZLIB
            /* zlib encoded track */

            if (size == 0)
                continue;

            z_stream zstream;

            zstream.zalloc = (alloc_func) 0;
            zstream.zfree = (free_func) 0;
            zstream.opaque = (voidpf) 0;
            if (inflateInit(&zstream) != Z_OK) {
                mp_warn(log, "zlib initialization failed.\n");
                goto error;
            }
            zstream.next_in = (Bytef *) src;
            zstream.avail_in = size;

            dest = NULL;
            zstream.avail_out = size;
            int result;
            do {
                if (size >= INT_MAX - 4000) {
                    talloc_free(dest);
                    dest = NULL;
                    inflateEnd(&zstream);
                    goto error;
                }
                size += 4000;
                dest = talloc_realloc_size(track->parser_tmp, dest, size);
                zstream.next_out = (Bytef *) (dest + zstream.total_out);
                result = inflate(&zstream, Z_NO_FLUSH);
                if (result != Z_OK && result != Z_STREAM_END) {
                    mp_warn(log, "zlib decompression failed.\n");
                    talloc_free(dest);
                    dest = NULL;
                    inflateEnd(&zstream);
                    goto error;
                }
                zstream.avail_out += 4000;
            } while (zstream.avail_out == 4000 && zstream.avail_in != 0
                     && result != Z_STREAM_END);

            size = zstream.total_out;
            inflateEnd(&zstream);
#endif
        } else if (enc->comp_algo == 2) {
            /* lzo encoded track */
            int out_avail;
            int maxlen = INT_MAX - AV_LZO_OUTPUT_PADDING;
            if (size >= maxlen / 3)
                goto error;
            int dstlen = size * 3;

            dest = NULL;
            while (1) {
                int srclen = size;
                dest = talloc_realloc_size(track->parser_tmp, dest,
                                           dstlen + AV_LZO_OUTPUT_PADDING);
                out_avail = dstlen;
                int result = av_lzo1x_decode(dest, &out_avail, src, &srclen);
                if (result == 0)
                    break;
                if (!(result & AV_LZO_OUTPUT_FULL)) {
                    mp_warn(log, "lzo decompression failed.\n");
                    talloc_free(dest);
                    dest = NULL;
                    goto error;
                }
                mp_dbg(log, "lzo decompression buffer too small.\n");
                if (dstlen >= maxlen / 2) {
                    talloc_free(dest);
                    dest = NULL;
                    goto error;
                }
                dstlen = MPMAX(1, 2 * dstlen);
            }
            size = dstlen - out_avail;
        } else if (enc->comp_algo == 3) {
            dest = talloc_size(track->parser_tmp, size + enc->comp_settings_len);
            memcpy(dest, enc->comp_settings, enc->comp_settings_len);
            memcpy(dest + enc->comp_settings_len, src, size);
            size += enc->comp_settings_len;
        }
    }

 error:
    if (src != dest && src != orig_src)
        talloc_free(src);
    if (!size)
        dest = NULL;
    return (bstr){dest, size};
}


static int demux_mkv_read_info(demuxer_t *demuxer)
{
    mkv_demuxer_t *mkv_d = demuxer->priv;
    stream_t *s = demuxer->stream;
    int res = 0;

    MP_VERBOSE(demuxer, "|+ segment information...\n");

    mkv_d->tc_scale = 1000000;
    mkv_d->duration = 0;

    struct ebml_info info = {0};
    struct ebml_parse_ctx parse_ctx = {demuxer->log};
    if (ebml_read_element(s, &parse_ctx, &info, &ebml_info_desc) < 0)
        return -1;
    if (info.n_timecode_scale) {
        mkv_d->tc_scale = info.timecode_scale;
        MP_VERBOSE(demuxer, "| + timecode scale: %" PRIu64 "\n", mkv_d->tc_scale);
        if (mkv_d->tc_scale < 1 || mkv_d->tc_scale > INT_MAX) {
            res = -1;
            goto out;
        }
    }
    if (info.n_duration) {
        mkv_d->duration = info.duration * mkv_d->tc_scale / 1e9;
        MP_VERBOSE(demuxer, "| + duration: %.3fs\n",
               mkv_d->duration);
    }
    if (info.n_title) {
        mp_tags_set_bstr(demuxer->metadata, bstr0("TITLE"), info.title);
    }
    if (info.n_segment_uid) {
        int len = info.segment_uid.len;
        if (len != sizeof(demuxer->matroska_data.uid.segment)) {
            MP_INFO(demuxer, "segment uid invalid length %d\n", len);
        } else {
            memcpy(demuxer->matroska_data.uid.segment, info.segment_uid.start,
                   len);
            MP_VERBOSE(demuxer, "| + segment uid");
            for (int i = 0; i < len; i++)
                MP_VERBOSE(demuxer, " %02x",
                       demuxer->matroska_data.uid.segment[i]);
            MP_VERBOSE(demuxer, "\n");
        }
    }
    if (demuxer->params && demuxer->params->matroska_wanted_uids) {
        if (info.n_segment_uid) {
            for (int i = 0; i < demuxer->params->matroska_num_wanted_uids; i++) {
                struct matroska_segment_uid *uid = demuxer->params->matroska_wanted_uids + i;
                if (!memcmp(info.segment_uid.start, uid->segment, 16)) {
                    demuxer->matroska_data.uid.edition = uid->edition;
                    goto out;
                }
            }
        }
        MP_VERBOSE(demuxer, "This is not one of the wanted files. "
                "Stopping attempt to open.\n");
        res = -2;
    }
 out:
    talloc_free(parse_ctx.talloc_ctx);
    return res;
}

static void parse_trackencodings(struct demuxer *demuxer,
                                 struct mkv_track *track,
                                 struct ebml_content_encodings *encodings)
{
    // initial allocation to be a non-NULL context before realloc
    mkv_content_encoding_t *ce = talloc_size(track, 1);

    for (int n_enc = 0; n_enc < encodings->n_content_encoding; n_enc++) {
        struct ebml_content_encoding *enc = encodings->content_encoding + n_enc;
        struct mkv_content_encoding e = {0};
        e.order = enc->content_encoding_order;
        if (enc->n_content_encoding_scope)
            e.scope = enc->content_encoding_scope;
        else
            e.scope = 1;
        e.type = enc->content_encoding_type;

        if (enc->n_content_compression) {
            struct ebml_content_compression *z = &enc->content_compression;
            e.comp_algo = z->content_comp_algo;
            if (z->n_content_comp_settings) {
                int sz = z->content_comp_settings.len;
                e.comp_settings = talloc_size(ce, sz);
                memcpy(e.comp_settings, z->content_comp_settings.start, sz);
                e.comp_settings_len = sz;
            }
        }

        if (e.type == 1) {
            MP_WARN(demuxer, "Track "
                    "number %u has been encrypted and "
                    "decryption has not yet been\n"
                    "implemented. Skipping track.\n",
                    track->tnum);
        } else if (e.type != 0) {
            MP_WARN(demuxer, "Unknown content encoding type for "
                    "track %u. Skipping track.\n",
                    track->tnum);
        } else if (e.comp_algo != 0 && e.comp_algo != 2 && e.comp_algo != 3) {
            MP_WARN(demuxer, "Track %u has been compressed with "
                    "an unknown/unsupported compression\n"
                    "algorithm (%" PRIu64 "). Skipping track.\n",
                    track->tnum, e.comp_algo);
        }
#if !HAVE_ZLIB
        else if (e.comp_algo == 0) {
            MP_WARN(demuxer, "Track %u was compressed with zlib "
                    "but mpv has not been compiled\n"
                    "with support for zlib compression. "
                    "Skipping track.\n",
                    track->tnum);
        }
#endif
        int i;
        for (i = 0; i < n_enc; i++) {
            if (e.order >= ce[i].order)
                break;
        }
        ce = talloc_realloc(track, ce, mkv_content_encoding_t, n_enc + 1);
        memmove(ce + i + 1, ce + i, (n_enc - i) * sizeof(*ce));
        memcpy(ce + i, &e, sizeof(e));
    }

    track->encodings = ce;
    track->num_encodings = encodings->n_content_encoding;
}

static void parse_trackaudio(struct demuxer *demuxer, struct mkv_track *track,
                             struct ebml_audio *audio)
{
    if (audio->n_sampling_frequency) {
        track->a_sfreq = audio->sampling_frequency;
        MP_VERBOSE(demuxer, "|   + Sampling frequency: %f\n", track->a_sfreq);
    } else {
        track->a_sfreq = 8000;
    }
    if (audio->n_output_sampling_frequency) {
        track->a_osfreq = audio->output_sampling_frequency;
        MP_VERBOSE(demuxer, "|   + Output sampling frequency: %f\n", track->a_osfreq);
    } else {
        track->a_osfreq = track->a_sfreq;
    }
    if (audio->n_bit_depth) {
        track->a_bps = audio->bit_depth;
        MP_VERBOSE(demuxer, "|   + Bit depth: %u\n", track->a_bps);
    }
    if (audio->n_channels) {
        track->a_channels = audio->channels;
        MP_VERBOSE(demuxer, "|   + Channels: %u\n", track->a_channels);
    } else {
        track->a_channels = 1;
    }
}

static void parse_trackvideo(struct demuxer *demuxer, struct mkv_track *track,
                             struct ebml_video *video)
{
    if (video->n_frame_rate) {
        track->v_frate = video->frame_rate;
        MP_VERBOSE(demuxer, "|   + Frame rate: %f\n", track->v_frate);
        if (track->v_frate > 0)
            track->default_duration = 1 / track->v_frate;
    }
    if (video->n_display_width) {
        track->v_dwidth = video->display_width;
        track->v_dwidth_set = true;
        MP_VERBOSE(demuxer, "|   + Display width: %u\n", track->v_dwidth);
    }
    if (video->n_display_height) {
        track->v_dheight = video->display_height;
        track->v_dheight_set = true;
        MP_VERBOSE(demuxer, "|   + Display height: %u\n", track->v_dheight);
    }
    if (video->n_pixel_width) {
        track->v_width = video->pixel_width;
        MP_VERBOSE(demuxer, "|   + Pixel width: %u\n", track->v_width);
    }
    if (video->n_pixel_height) {
        track->v_height = video->pixel_height;
        MP_VERBOSE(demuxer, "|   + Pixel height: %u\n", track->v_height);
    }
    if (video->n_colour_space && video->colour_space.len == 4) {
        uint8_t *d = (uint8_t *)&video->colour_space.start[0];
        track->colorspace = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
        MP_VERBOSE(demuxer, "|   + Colorspace: %#x\n",
                   (unsigned int)track->colorspace);
    }
    if (video->n_stereo_mode) {
        const char *name = MP_STEREO3D_NAME(video->stereo_mode);
        if (name) {
            track->stereo_mode = video->stereo_mode;
            MP_VERBOSE(demuxer, "|   + StereoMode: %s\n", name);
        } else {
            MP_WARN(demuxer, "Unknown StereoMode: %d\n", (int)video->stereo_mode);
        }
    }
}

/**
 * \brief free any data associated with given track
 * \param track track of which to free data
 */
static void demux_mkv_free_trackentry(mkv_track_t *track)
{
    talloc_free(track->parser_tmp);
    talloc_free(track);
}

static void parse_trackentry(struct demuxer *demuxer,
                             struct ebml_track_entry *entry)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    struct mkv_track *track = talloc_zero(NULL, struct mkv_track);
    track->last_index_entry = (size_t)-1;
    track->parser_tmp = talloc_new(track);

    track->tnum = entry->track_number;
    if (track->tnum) {
        MP_VERBOSE(demuxer, "|  + Track number: %u\n", track->tnum);
    } else {
        MP_ERR(demuxer, "Missing track number!\n");
    }

    if (entry->n_name) {
        track->name = talloc_strndup(track, entry->name.start,
                                     entry->name.len);
        MP_VERBOSE(demuxer, "|  + Name: %s\n", track->name);
    }

    track->type = entry->track_type;
    MP_VERBOSE(demuxer, "|  + Track type: ");
    switch (track->type) {
    case MATROSKA_TRACK_AUDIO:
        MP_VERBOSE(demuxer, "Audio\n");
        break;
    case MATROSKA_TRACK_VIDEO:
        MP_VERBOSE(demuxer, "Video\n");
        break;
    case MATROSKA_TRACK_SUBTITLE:
        MP_VERBOSE(demuxer, "Subtitle\n");
        break;
    default:
        MP_VERBOSE(demuxer, "unknown\n");
        break;
    }

    if (entry->n_audio) {
        MP_VERBOSE(demuxer, "|  + Audio track\n");
        parse_trackaudio(demuxer, track, &entry->audio);
    }

    if (entry->n_video) {
        MP_VERBOSE(demuxer, "|  + Video track\n");
        parse_trackvideo(demuxer, track, &entry->video);
    }

    if (entry->n_codec_id) {
        track->codec_id = talloc_strndup(track, entry->codec_id.start,
                                         entry->codec_id.len);
        if (!strcmp(track->codec_id, MKV_V_MSCOMP)
            || !strcmp(track->codec_id, MKV_A_ACM))
            track->ms_compat = 1;
        MP_VERBOSE(demuxer, "|  + Codec ID: %s\n", track->codec_id);
    } else {
        MP_ERR(demuxer, "Missing codec ID!\n");
        track->codec_id = "";
    }

    if (entry->n_codec_private && entry->codec_private.len <= 0x10000000) {
        int len = entry->codec_private.len;
        track->private_data = talloc_size(track, len + AV_LZO_INPUT_PADDING);
        memcpy(track->private_data, entry->codec_private.start, len);
        track->private_size = len;
        MP_VERBOSE(demuxer, "|  + CodecPrivate, length %u\n", track->private_size);
    }

    if (entry->n_language) {
        track->language = talloc_strndup(track, entry->language.start,
                                         entry->language.len);
        MP_VERBOSE(demuxer, "|  + Language: %s\n", track->language);
    } else {
        track->language = talloc_strdup(track, "eng");
    }

    if (entry->n_flag_default) {
        track->default_track = entry->flag_default;
        MP_VERBOSE(demuxer, "|  + Default flag: %u\n", track->default_track);
    } else {
        track->default_track = 1;
    }

    if (entry->n_default_duration) {
        track->default_duration = entry->default_duration / 1e9;
        if (entry->default_duration == 0) {
            MP_VERBOSE(demuxer, "|  + Default duration: 0");
        } else {
            if (!track->v_frate)
                track->v_frate = 1e9 / entry->default_duration;
            MP_VERBOSE(demuxer, "|  + Default duration: %.3fms ( = %.3f fps)\n",
                       entry->default_duration / 1000000.0, track->v_frate);
        }
    }

    if (entry->n_content_encodings)
        parse_trackencodings(demuxer, track, &entry->content_encodings);

    if (entry->n_codec_delay)
        track->codec_delay = entry->codec_delay / 1e9;

    mkv_d->tracks[mkv_d->num_tracks++] = track;
}

static int demux_mkv_read_tracks(demuxer_t *demuxer)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    stream_t *s = demuxer->stream;

    MP_VERBOSE(demuxer, "|+ segment tracks...\n");

    struct ebml_tracks tracks = {0};
    struct ebml_parse_ctx parse_ctx = {demuxer->log};
    if (ebml_read_element(s, &parse_ctx, &tracks, &ebml_tracks_desc) < 0)
        return -1;

    mkv_d->tracks = talloc_zero_array(mkv_d, struct mkv_track*,
                                      tracks.n_track_entry);
    for (int i = 0; i < tracks.n_track_entry; i++) {
        MP_VERBOSE(demuxer, "| + a track...\n");
        parse_trackentry(demuxer, &tracks.track_entry[i]);
    }
    talloc_free(parse_ctx.talloc_ctx);
    return 0;
}

static void cue_index_add(demuxer_t *demuxer, int track_id, uint64_t filepos,
                          uint64_t timecode, uint64_t duration)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;

    MP_TARRAY_GROW(mkv_d, mkv_d->indexes, mkv_d->num_indexes);

    mkv_d->indexes[mkv_d->num_indexes] = (mkv_index_t) {
        .tnum = track_id,
        .filepos = filepos,
        .timecode = timecode,
        .duration = duration,
    };

    mkv_d->num_indexes++;
}

static void add_block_position(demuxer_t *demuxer, struct mkv_track *track,
                               uint64_t filepos,
                               uint64_t timecode, uint64_t duration)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;

    if (mkv_d->index_complete || !track)
        return;

    mkv_d->index_has_durations = true;

    if (track->last_index_entry != (size_t)-1) {
        mkv_index_t *index = &mkv_d->indexes[track->last_index_entry];
        // Never add blocks which are already covered by the index.
        if (index->timecode >= timecode)
            return;
    }
    cue_index_add(demuxer, track->tnum, filepos, timecode, duration);
    track->last_index_entry = mkv_d->num_indexes - 1;
}

static int demux_mkv_read_cues(demuxer_t *demuxer)
{
    struct MPOpts *opts = demuxer->opts;
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    stream_t *s = demuxer->stream;

    if (opts->index_mode != 1) {
        ebml_read_skip(demuxer->log, -1, s);
        return 0;
    }

    MP_VERBOSE(demuxer, "/---- [ parsing cues ] -----------\n");
    struct ebml_cues cues = {0};
    struct ebml_parse_ctx parse_ctx = {demuxer->log};
    if (ebml_read_element(s, &parse_ctx, &cues, &ebml_cues_desc) < 0)
        return -1;

    mkv_d->num_indexes = 0;
    mkv_d->index_has_durations = false;

    for (int i = 0; i < cues.n_cue_point; i++) {
        struct ebml_cue_point *cuepoint = &cues.cue_point[i];
        if (cuepoint->n_cue_time != 1 || !cuepoint->n_cue_track_positions) {
            MP_WARN(demuxer, "Malformed CuePoint element\n");
            continue;
        }
        uint64_t time = cuepoint->cue_time;
        for (int c = 0; c < cuepoint->n_cue_track_positions; c++) {
            struct ebml_cue_track_positions *trackpos =
                &cuepoint->cue_track_positions[c];
            uint64_t pos = mkv_d->segment_start + trackpos->cue_cluster_position;
            cue_index_add(demuxer, trackpos->cue_track, pos,
                          time, trackpos->cue_duration);
            mkv_d->index_has_durations |= trackpos->n_cue_duration > 0;
            MP_DBG(demuxer, "|+ found cue point for track %" PRIu64
                   ": timecode %" PRIu64 ", filepos: %" PRIu64
                   " offset %" PRIu64 ", duration %" PRIu64 "\n",
                   trackpos->cue_track, time, pos,
                   trackpos->cue_relative_position, trackpos->cue_duration);
        }
    }

    // Do not attempt to create index on the fly.
    mkv_d->index_complete = true;

    MP_VERBOSE(demuxer, "\\---- [ parsing cues ] -----------\n");
    talloc_free(parse_ctx.talloc_ctx);
    return 0;
}

static int demux_mkv_read_chapters(struct demuxer *demuxer)
{
    struct MPOpts *opts = demuxer->opts;
    stream_t *s = demuxer->stream;
    int wanted_edition = opts->edition_id;
    uint64_t wanted_edition_uid = demuxer->matroska_data.uid.edition;

    /* A specific edition UID was requested; ignore the user option which is
     * only applicable to the top-level file. */
    if (wanted_edition_uid)
        wanted_edition = -1;

    MP_VERBOSE(demuxer, "/---- [ parsing chapters ] ---------\n");
    struct ebml_chapters file_chapters = {0};
    struct ebml_parse_ctx parse_ctx = {demuxer->log};
    if (ebml_read_element(s, &parse_ctx, &file_chapters,
                          &ebml_chapters_desc) < 0)
        return -1;

    int selected_edition = -1;
    int num_editions = file_chapters.n_edition_entry;
    struct ebml_edition_entry *editions = file_chapters.edition_entry;
    for (int i = 0; i < num_editions; i++) {
        struct demux_edition new = {
            .demuxer_id = editions[i].edition_uid,
            .default_edition = editions[i].edition_flag_default,
            .metadata = talloc_zero(demuxer, struct mp_tags),
        };
        MP_TARRAY_APPEND(demuxer, demuxer->editions, demuxer->num_editions, new);
    }
    if (wanted_edition >= 0 && wanted_edition < num_editions) {
        selected_edition = wanted_edition;
        MP_VERBOSE(demuxer, "User-specified edition: %d\n", selected_edition);
    } else {
        for (int i = 0; i < num_editions; i++) {
            if (wanted_edition_uid &&
                editions[i].edition_uid == wanted_edition_uid) {
                selected_edition = i;
                break;
            } else if (editions[i].edition_flag_default) {
                selected_edition = i;
                MP_VERBOSE(demuxer, "Default edition: %d\n", i);
                break;
            }
        }
    }
    if (selected_edition < 0) {
        if (wanted_edition_uid) {
            MP_ERR(demuxer, "Unable to find expected edition uid: %"PRIu64"\n",
                   wanted_edition_uid);
            return -1;
        } else {
            selected_edition = 0;
        }
    }

    for (int idx = 0; idx < num_editions; idx++) {
        MP_VERBOSE(demuxer, "New edition %d\n", idx);
        int warn_level = idx == selected_edition ? MSGL_WARN : MSGL_V;
        if (editions[idx].n_edition_flag_default)
            MP_VERBOSE(demuxer, "Default edition flag: %"PRIu64
                       "\n", editions[idx].edition_flag_default);
        if (editions[idx].n_edition_flag_ordered)
            MP_VERBOSE(demuxer, "Ordered chapter flag: %"PRIu64
                       "\n", editions[idx].edition_flag_ordered);

        int chapter_count = editions[idx].n_chapter_atom;

        struct matroska_chapter *m_chapters = NULL;
        if (idx == selected_edition && editions[idx].edition_flag_ordered) {
            m_chapters = talloc_array_ptrtype(demuxer, m_chapters, chapter_count);
            demuxer->matroska_data.ordered_chapters = m_chapters;
            demuxer->matroska_data.num_ordered_chapters = chapter_count;
        }

        for (int i = 0; i < chapter_count; i++) {
            struct ebml_chapter_atom *ca = editions[idx].chapter_atom + i;
            struct matroska_chapter chapter = {0};
            struct bstr name = { "(unnamed)", 9 };

            if (!ca->n_chapter_time_start)
                MP_MSG(demuxer, warn_level, "Chapter lacks start time\n");
            chapter.start = ca->chapter_time_start;
            chapter.end = ca->chapter_time_end;

            if (ca->n_chapter_display) {
                if (ca->n_chapter_display > 1)
                    MP_MSG(demuxer, warn_level, "Multiple chapter "
                           "names not supported, picking first\n");
                if (!ca->chapter_display[0].n_chap_string)
                    MP_MSG(demuxer, warn_level, "Malformed chapter name entry\n");
                else
                    name = ca->chapter_display[0].chap_string;
            }

            if (ca->n_chapter_segment_uid) {
                chapter.has_segment_uid = true;
                int len = ca->chapter_segment_uid.len;
                if (len != sizeof(chapter.uid.segment))
                    MP_MSG(demuxer, warn_level,
                           "Chapter segment uid bad length %d\n", len);
                else {
                    memcpy(chapter.uid.segment, ca->chapter_segment_uid.start,
                           len);
                    if (ca->n_chapter_segment_edition_uid)
                        chapter.uid.edition = ca->chapter_segment_edition_uid;
                    else
                        chapter.uid.edition = 0;
                    MP_VERBOSE(demuxer, "Chapter segment uid ");
                    for (int n = 0; n < len; n++)
                        MP_VERBOSE(demuxer, "%02x ",
                               chapter.uid.segment[n]);
                    MP_VERBOSE(demuxer, "\n");
                }
            }

            MP_VERBOSE(demuxer, "Chapter %u from %02d:%02d:%02d.%03d "
                   "to %02d:%02d:%02d.%03d, %.*s\n", i,
                   (int) (chapter.start / 60 / 60 / 1000000000),
                   (int) ((chapter.start / 60 / 1000000000) % 60),
                   (int) ((chapter.start / 1000000000) % 60),
                   (int) (chapter.start % 1000000000),
                   (int) (chapter.end / 60 / 60 / 1000000000),
                   (int) ((chapter.end / 60 / 1000000000) % 60),
                   (int) ((chapter.end / 1000000000) % 60),
                   (int) (chapter.end % 1000000000),
                   BSTR_P(name));

            if (idx == selected_edition) {
                demuxer_add_chapter(demuxer, name, chapter.start / 1e9,
                                    ca->chapter_uid);
            }
            if (m_chapters) {
                chapter.name = talloc_strndup(m_chapters, name.start, name.len);
                m_chapters[i] = chapter;
            }
        }
    }

    demuxer->num_editions = num_editions;
    demuxer->edition = selected_edition;

    talloc_free(parse_ctx.talloc_ctx);
    MP_VERBOSE(demuxer, "\\---- [ parsing chapters ] ---------\n");
    return 0;
}

static int demux_mkv_read_tags(demuxer_t *demuxer)
{
    struct mkv_demuxer *mkv_d = demuxer->priv;
    stream_t *s = demuxer->stream;

    struct ebml_parse_ctx parse_ctx = {demuxer->log};
    struct ebml_tags           tags = {0};
    if (ebml_read_element(s, &parse_ctx, &tags, &ebml_tags_desc) < 0)
        return -1;

    mkv_d->tags = talloc_memdup(mkv_d, &tags, sizeof(tags));
    talloc_steal(mkv_d->tags, parse_ctx.talloc_ctx);
    return 0;
}

static void process_tags(demuxer_t *demuxer)
{
    struct mkv_demuxer *mkv_d = demuxer->priv;
    struct ebml_tags *tags = mkv_d->tags;

    if (!tags)
        return;

    for (int i = 0; i < tags->n_tag; i++) {
        struct ebml_tag tag = tags->tag[i];
        if (tag.targets.target_track_uid  || tag.targets.target_attachment_uid)
            continue;

        struct mp_tags *dst = NULL;

        if (tag.targets.target_chapter_uid) {
            for (int n = 0; n < demuxer->num_chapters; n++) {
                if (demuxer->chapters[n].demuxer_id ==
                    tag.targets.target_chapter_uid)
                {
                    dst = demuxer->chapters[n].metadata;
                    break;
                }
            }
        } else if (tag.targets.target_edition_uid) {
            for (int n = 0; n < demuxer->num_editions; n++) {
                if (demuxer->editions[n].demuxer_id ==
                    tag.targets.target_edition_uid)
                {
                    dst = demuxer->editions[n].metadata;
                    break;
                }
            }
        } else {
            dst = demuxer->metadata;
        }

        if (dst) {
            for (int j = 0; j < tag.n_simple_tag; j++) {
                mp_tags_set_bstr(dst, tag.simple_tag[j].tag_name,
                                      tag.simple_tag[j].tag_string);
            }
        }
    }
}

static int demux_mkv_read_attachments(demuxer_t *demuxer)
{
    stream_t *s = demuxer->stream;

    MP_VERBOSE(demuxer, "/---- [ parsing attachments ] ---------\n");

    struct ebml_attachments attachments = {0};
    struct ebml_parse_ctx parse_ctx = {demuxer->log};
    if (ebml_read_element(s, &parse_ctx, &attachments,
                          &ebml_attachments_desc) < 0)
        return -1;

    for (int i = 0; i < attachments.n_attached_file; i++) {
        struct ebml_attached_file *attachment = &attachments.attached_file[i];
        if (!attachment->n_file_name || !attachment->n_file_mime_type
            || !attachment->n_file_data) {
            MP_WARN(demuxer, "Malformed attachment\n");
            continue;
        }
        struct bstr name = attachment->file_name;
        struct bstr mime = attachment->file_mime_type;
        demuxer_add_attachment(demuxer, name, mime, attachment->file_data);
        MP_VERBOSE(demuxer, "Attachment: %.*s, %.*s, %zu bytes\n",
                   BSTR_P(name), BSTR_P(mime), attachment->file_data.len);
    }

    talloc_free(parse_ctx.talloc_ctx);
    MP_VERBOSE(demuxer, "\\---- [ parsing attachments ] ---------\n");
    return 0;
}

static struct header_elem *get_header_element(struct demuxer *demuxer,
                                              uint32_t id,
                                              int64_t element_filepos)
{
    struct mkv_demuxer *mkv_d = demuxer->priv;

    // Note that some files in fact contain a SEEKHEAD with a list of all
    // clusters - we have no use for that.
    if (!ebml_is_mkv_level1_id(id) || id == MATROSKA_ID_CLUSTER)
        return NULL;

    for (int n = 0; n < mkv_d->num_headers; n++) {
        struct header_elem *elem = &mkv_d->headers[n];
        // SEEKHEAD is the only element that can happen multiple times.
        // Other elements might be duplicated (or attempted to be read twice,
        // even if it's only once in the file), but only the first is used.
        if (elem->id == id && (id != MATROSKA_ID_SEEKHEAD ||
                               elem->pos == element_filepos))
            return elem;
    }
    struct header_elem elem = { .id = id, .pos = element_filepos };
    MP_TARRAY_APPEND(mkv_d, mkv_d->headers, mkv_d->num_headers, elem);
    return &mkv_d->headers[mkv_d->num_headers - 1];
}

// Mark the level 1 element with the given id as read. Return whether it
// was marked read before (e.g. for checking whether it was already read).
// element_filepos refers to the file position of the element ID.
static bool test_header_element(struct demuxer *demuxer, uint32_t id,
                                int64_t element_filepos)
{
    struct header_elem *elem = get_header_element(demuxer, id, element_filepos);
    if (!elem)
        return false;
    if (elem->parsed)
        return true;
    elem->parsed = true;
    return false;
}

static int demux_mkv_read_seekhead(demuxer_t *demuxer)
{
    struct mkv_demuxer *mkv_d = demuxer->priv;
    struct stream *s = demuxer->stream;
    int res = 0;
    struct ebml_seek_head seekhead = {0};
    struct ebml_parse_ctx parse_ctx = {demuxer->log};

    MP_VERBOSE(demuxer, "/---- [ parsing seek head ] ---------\n");
    if (ebml_read_element(s, &parse_ctx, &seekhead, &ebml_seek_head_desc) < 0) {
        res = -1;
        goto out;
    }
    for (int i = 0; i < seekhead.n_seek; i++) {
        struct ebml_seek *seek = &seekhead.seek[i];
        if (seek->n_seek_id != 1 || seek->n_seek_position != 1) {
            MP_WARN(demuxer, "Invalid SeekHead entry\n");
            continue;
        }
        uint64_t pos = seek->seek_position + mkv_d->segment_start;
        MP_DBG(demuxer, "Element 0x%x at %"PRIu64".\n",
               (unsigned)seek->seek_id, pos);
        get_header_element(demuxer, seek->seek_id, pos);
    }
 out:
    MP_VERBOSE(demuxer, "\\---- [ parsing seek head ] ---------\n");
    talloc_free(parse_ctx.talloc_ctx);
    return res;
}

static int read_header_element(struct demuxer *demuxer, uint32_t id,
                               int64_t start_filepos)
{
    if (id == EBML_ID_INVALID)
        return 0;

    if (test_header_element(demuxer, id, start_filepos))
        goto skip;

    switch(id) {
    case MATROSKA_ID_INFO:
        return demux_mkv_read_info(demuxer);
    case MATROSKA_ID_TRACKS:
        return demux_mkv_read_tracks(demuxer);
    case MATROSKA_ID_CUES:
        return demux_mkv_read_cues(demuxer);
    case MATROSKA_ID_TAGS:
        return demux_mkv_read_tags(demuxer);
    case MATROSKA_ID_SEEKHEAD:
        return demux_mkv_read_seekhead(demuxer);
    case MATROSKA_ID_CHAPTERS:
        return demux_mkv_read_chapters(demuxer);
    case MATROSKA_ID_ATTACHMENTS:
        return demux_mkv_read_attachments(demuxer);
    }
skip:
    ebml_read_skip(demuxer->log, -1, demuxer->stream);
    return 0;
}

static int read_deferred_element(struct demuxer *demuxer,
                                 struct header_elem *elem)
{
    stream_t *s = demuxer->stream;

    if (elem->parsed)
        return 0;
    elem->parsed = true;
    MP_VERBOSE(demuxer, "Seeking to %"PRIu64" to read header element 0x%x.\n",
               elem->pos, (unsigned)elem->id);
    if (!stream_seek(s, elem->pos)) {
        MP_WARN(demuxer, "Failed to seek when reading header element.\n");
        return 0;
    }
    if (ebml_read_id(s) != elem->id) {
        MP_ERR(demuxer, "Expected element 0x%x not found\n",
               (unsigned int)elem->id);
        return 0;
    }
    elem->parsed = false; // don't make read_header_element skip it
    return read_header_element(demuxer, elem->id, elem->pos);
}

static void read_deferred_cues(demuxer_t *demuxer)
{
    struct MPOpts *opts = demuxer->opts;
    mkv_demuxer_t *mkv_d = demuxer->priv;

    if (mkv_d->index_complete || opts->index_mode != 1)
        return;

    for (int n = 0; n < mkv_d->num_headers; n++) {
        struct header_elem *elem = &mkv_d->headers[n];

        if (elem->id == MATROSKA_ID_CUES)
            read_deferred_element(demuxer, elem);
    }
}

static void add_coverart(struct demuxer *demuxer)
{
    for (int n = 0; n < demuxer->num_attachments; n++) {
        struct demux_attachment *att = &demuxer->attachments[n];
        const char *codec = mp_map_mimetype_to_video_codec(att->type);
        if (!codec)
            continue;
        struct sh_stream *sh = new_sh_stream(demuxer, STREAM_VIDEO);
        if (!sh)
            break;
        sh->demuxer_id = -1 - sh->index; // don't clash with mkv IDs
        sh->codec = codec;
        sh->attached_picture = new_demux_packet_from(att->data, att->data_size);
        if (sh->attached_picture) {
            sh->attached_picture->pts = 0;
            talloc_steal(sh, sh->attached_picture);
            sh->attached_picture->keyframe = true;
        }
        sh->title = att->name;
    }
}

static int demux_mkv_open_video(demuxer_t *demuxer, mkv_track_t *track);
static int demux_mkv_open_audio(demuxer_t *demuxer, mkv_track_t *track);
static int demux_mkv_open_sub(demuxer_t *demuxer, mkv_track_t *track);

static void display_create_tracks(demuxer_t *demuxer)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;

    for (int i = 0; i < mkv_d->num_tracks; i++) {
        switch (mkv_d->tracks[i]->type) {
        case MATROSKA_TRACK_VIDEO:
            demux_mkv_open_video(demuxer, mkv_d->tracks[i]);
            break;
        case MATROSKA_TRACK_AUDIO:
            demux_mkv_open_audio(demuxer, mkv_d->tracks[i]);
            break;
        case MATROSKA_TRACK_SUBTITLE:
            demux_mkv_open_sub(demuxer, mkv_d->tracks[i]);
            break;
        }
    }
}

typedef struct {
    char *id;
    int fourcc;
    int extradata;
} videocodec_info_t;

static const videocodec_info_t vinfo[] = {
    {MKV_V_MJPEG,     MP_FOURCC('m', 'j', 'p', 'g'), 1},
    {MKV_V_MPEG1,     MP_FOURCC('m', 'p', 'g', '1'), 0},
    {MKV_V_MPEG2,     MP_FOURCC('m', 'p', 'g', '2'), 0},
    {MKV_V_MPEG4_SP,  MP_FOURCC('m', 'p', '4', 'v'), 1},
    {MKV_V_MPEG4_ASP, MP_FOURCC('m', 'p', '4', 'v'), 1},
    {MKV_V_MPEG4_AP,  MP_FOURCC('m', 'p', '4', 'v'), 1},
    {MKV_V_MPEG4_AVC, MP_FOURCC('a', 'v', 'c', '1'), 1},
    {MKV_V_THEORA,    MP_FOURCC('t', 'h', 'e', 'o'), 1},
    {MKV_V_VP8,       MP_FOURCC('V', 'P', '8', '0'), 0},
    {MKV_V_VP9,       MP_FOURCC('V', 'P', '9', '0'), 0},
    {MKV_V_DIRAC,     MP_FOURCC('d', 'r', 'a', 'c'), 0},
    {MKV_V_PRORES,    MP_FOURCC('p', 'r', '0', '0'), 0},
    {MKV_V_HEVC,      MP_FOURCC('H', 'E', 'V', 'C'), 1},
    {0}
};

static int demux_mkv_open_video(demuxer_t *demuxer, mkv_track_t *track)
{
    unsigned char *extradata = NULL;
    unsigned int extradata_size = 0;
    struct sh_stream *sh = new_sh_stream(demuxer, STREAM_VIDEO);
    if (!sh)
        return 1;
    track->stream = sh;
    sh_video_t *sh_v = sh->video;
    sh->demuxer_id = track->tnum;
    sh->title = talloc_strdup(sh_v, track->name);

    if (track->ms_compat) {     /* MS compatibility mode */
        // The private_data contains a BITMAPINFOHEADER struct
        if (track->private_data == NULL || track->private_size < 40)
            return 1;

        unsigned char *h = track->private_data;
        if (track->v_width == 0)
            track->v_width = AV_RL32(h + 4);        // biWidth
        if (track->v_height == 0)
            track->v_height = AV_RL32(h + 8);       // biHeight
        sh_v->bits_per_coded_sample = AV_RL16(h + 14); // biBitCount
        sh->format = AV_RL32(h + 16);               // biCompression

        extradata = track->private_data + 40;
        extradata_size = track->private_size - 40;
        mp_set_codec_from_tag(sh);
    } else {
        sh_v->bits_per_coded_sample = 24;

        if (track->private_size >= RVPROPERTIES_SIZE
            && (!strcmp(track->codec_id, MKV_V_REALV10)
                || !strcmp(track->codec_id, MKV_V_REALV20)
                || !strcmp(track->codec_id, MKV_V_REALV30)
                || !strcmp(track->codec_id, MKV_V_REALV40))) {
            unsigned char *src;
            uint32_t type2;
            unsigned int cnt;

            src = (uint8_t *) track->private_data + RVPROPERTIES_SIZE;

            cnt = track->private_size - RVPROPERTIES_SIZE;
            type2 = AV_RB32(src - 4);
            if (type2 == 0x10003000 || type2 == 0x10003001)
                sh->format = MP_FOURCC('R', 'V', '1', '3');
            else
                sh->format = MP_FOURCC('R', 'V', track->codec_id[9], '0');
            // copy type1 and type2 info from rv properties
            extradata_size = cnt + 8;
            extradata = src - 8;
            track->parse = true;
            track->parse_timebase = 1e3;
            mp_set_codec_from_tag(sh);
        } else if (strcmp(track->codec_id, MKV_V_UNCOMPRESSED) == 0) {
            // raw video, "like AVI" - this is a FourCC
            sh->format = track->colorspace;
            sh->codec = "rawvideo";
        } else if (strcmp(track->codec_id, MKV_V_QUICKTIME) == 0) {
            uint32_t fourcc1 = 0, fourcc2 = 0;
            if (track->private_size >= 8) {
                fourcc1 = AV_RL32(track->private_data + 0);
                fourcc2 = AV_RL32(track->private_data + 4);
            }
            if (fourcc1 == MP_FOURCC('S', 'V', 'Q', '3') ||
                fourcc2 == MP_FOURCC('S', 'V', 'Q', '3'))
            {
                sh->codec = "svq3";
                extradata = track->private_data;
                extradata_size = track->private_size;
            }
        } else {
            const videocodec_info_t *vi = vinfo;
            while (vi->id && strcmp(vi->id, track->codec_id))
                vi++;
            if (vi->id) {
                sh->format = vi->fourcc;
                mp_set_codec_from_tag(sh);
            }
            if (vi->extradata && track->private_data && track->private_size > 0)
            {
                extradata = track->private_data;
                extradata_size = track->private_size;
            }
        }
    }

    if (sh->format == MP_FOURCC('V', 'P', '9', '0')) {
        track->parse = true;
        track->parse_timebase = 1e9;
    }

    if (extradata_size > 0x1000000) {
        MP_WARN(demuxer, "Invalid CodecPrivate\n");
        return 1;
    }

    sh_v->extradata = talloc_memdup(sh_v, extradata, extradata_size);
    sh_v->extradata_len = extradata_size;
    if (!sh->codec) {
        MP_WARN(demuxer, "Unknown/unsupported CodecID (%s) or missing/bad "
                "CodecPrivate data (track %u).\n",
                track->codec_id, track->tnum);
    }
    if (track->v_frate == 0.0)
        track->v_frate = 25.0;
    sh_v->fps = track->v_frate;
    sh_v->disp_w = track->v_width;
    sh_v->disp_h = track->v_height;
    uint32_t dw = track->v_dwidth_set ? track->v_dwidth : track->v_width;
    uint32_t dh = track->v_dheight_set ? track->v_dheight : track->v_height;
    sh_v->aspect = (dw && dh) ? (double) dw / dh : 0;
    MP_VERBOSE(demuxer, "Aspect: %f\n", sh_v->aspect);
    sh_v->avi_dts = track->ms_compat;
    sh_v->stereo_mode = track->stereo_mode;

    return 0;
}

static const struct mkv_audio_tag {
    char *id;   bool prefix;   uint32_t formattag;
} mkv_audio_tags[] = {
    { MKV_A_MP2,       0, 0x0055 },
    { MKV_A_MP3,       0, 0x0055 },
    { MKV_A_AC3,       1, 0x2000 },
    { MKV_A_EAC3,      1, MP_FOURCC('E', 'A', 'C', '3') },
    { MKV_A_DTS,       0, 0x2001 },
    { MKV_A_PCM,       0, 0x0001 },
    { MKV_A_PCM_BE,    0, 0x0001 },
    { MKV_A_PCM_FLT,   0, 0x0003 },
    { MKV_A_AAC_2MAIN, 0, MP_FOURCC('M', 'P', '4', 'A') },
    { MKV_A_AAC_2LC,   1, MP_FOURCC('M', 'P', '4', 'A') },
    { MKV_A_AAC_2SSR,  0, MP_FOURCC('M', 'P', '4', 'A') },
    { MKV_A_AAC_4MAIN, 0, MP_FOURCC('M', 'P', '4', 'A') },
    { MKV_A_AAC_4LC,   1, MP_FOURCC('M', 'P', '4', 'A') },
    { MKV_A_AAC_4SSR,  0, MP_FOURCC('M', 'P', '4', 'A') },
    { MKV_A_AAC_4LTP,  0, MP_FOURCC('M', 'P', '4', 'A') },
    { MKV_A_AAC,       0, MP_FOURCC('M', 'P', '4', 'A') },
    { MKV_A_VORBIS,    0, MP_FOURCC('v', 'r', 'b', 's') },
    { MKV_A_OPUS,      0, MP_FOURCC('O', 'p', 'u', 's') },
    { MKV_A_OPUS_EXP,  0, MP_FOURCC('O', 'p', 'u', 's') },
    { MKV_A_QDMC,      0, MP_FOURCC('Q', 'D', 'M', 'C') },
    { MKV_A_QDMC2,     0, MP_FOURCC('Q', 'D', 'M', '2') },
    { MKV_A_WAVPACK,   0, MP_FOURCC('W', 'V', 'P', 'K') },
    { MKV_A_TRUEHD,    0, MP_FOURCC('T', 'R', 'H', 'D') },
    { MKV_A_FLAC,      0, MP_FOURCC('f', 'L', 'a', 'C') },
    { MKV_A_ALAC,      0, MP_FOURCC('a', 'L', 'a', 'C') },
    { MKV_A_REAL28,    0, MP_FOURCC('2', '8', '_', '8') },
    { MKV_A_REALATRC,  0, MP_FOURCC('a', 't', 'r', 'c') },
    { MKV_A_REALCOOK,  0, MP_FOURCC('c', 'o', 'o', 'k') },
    { MKV_A_REALDNET,  0, MP_FOURCC('d', 'n', 'e', 't') },
    { MKV_A_REALSIPR,  0, MP_FOURCC('s', 'i', 'p', 'r') },
    { MKV_A_TTA1,      0, MP_FOURCC('T', 'T', 'A', '1') },
    { NULL },
};

static int demux_mkv_open_audio(demuxer_t *demuxer, mkv_track_t *track)
{
    struct sh_stream *sh = new_sh_stream(demuxer, STREAM_AUDIO);
    if (!sh)
        return 1;
    track->stream = sh;
    sh_audio_t *sh_a = sh->audio;

    if (track->private_size > 0x1000000)
        goto error;

    unsigned char *extradata = track->private_data;
    unsigned int extradata_len = track->private_size;

    if (track->language && (strcmp(track->language, "und") != 0))
        sh->lang = talloc_strdup(sh_a, track->language);
    sh->demuxer_id = track->tnum;
    sh->title = talloc_strdup(sh_a, track->name);
    sh->default_track = track->default_track;
    if (!track->a_osfreq)
        track->a_osfreq = track->a_sfreq;

    if (track->ms_compat) {
        // The private_data contains a WAVEFORMATEX struct
        if (track->private_size < 18)
            goto error;
        MP_VERBOSE(demuxer, "track with MS compat audio.\n");
        unsigned char *h = track->private_data;
        track->a_formattag = AV_RL16(h + 0);    // wFormatTag
        if (track->a_channels == 0)
            track->a_channels = AV_RL16(h + 2); // nChannels
        if (track->a_osfreq == 0.0)
            track->a_osfreq = AV_RL32(h + 4);   // nSamplesPerSec
        sh_a->bitrate = AV_RL32(h + 8) * 8;     // nAvgBytesPerSec
        sh_a->block_align = AV_RL16(h + 12);    // nBlockAlign
        if (track->a_bps == 0)
            track->a_bps = AV_RL16(h + 14);     // wBitsPerSample
        extradata = track->private_data + 18;
        extradata_len = track->private_size - 18;
    } else {
        for (int i = 0; ; i++) {
            const struct mkv_audio_tag *t = mkv_audio_tags + i;
            if (t->id == NULL)
                goto error;
            if (t->prefix) {
                if (!bstr_startswith0(bstr0(track->codec_id), t->id))
                    continue;
            } else {
                if (strcmp(track->codec_id, t->id))
                    continue;
            }
            track->a_formattag = t->formattag;
            break;
        }
    }

    sh->format = track->a_formattag;
    mp_chmap_set_unknown(&sh_a->channels, track->a_channels);
    sh_a->samplerate = (uint32_t) track->a_osfreq;
    sh_a->bits_per_coded_sample = track->a_bps ? track->a_bps : 16;
    if (track->a_formattag == 0x0055) { /* MP3 || MP2 */
        sh_a->bitrate = 16000 * 8;
        sh_a->block_align = 1152;
        track->parse = true;
    } else if ((track->a_formattag == 0x2000)           /* AC3 */
               || track->a_formattag == MP_FOURCC('E', 'A', 'C', '3')
               || (track->a_formattag == 0x2001)) {        /* DTS */
        sh_a->bits_per_coded_sample = 0;
        sh_a->bitrate = 0;
        sh_a->block_align = 0;
    } else if (track->a_formattag == 0x0001) {  /* PCM || PCM_BE */
        if (!strcmp(track->codec_id, MKV_A_PCM_BE))
            sh->format = MP_FOURCC('t', 'w', 'o', 's');
    } else if (track->a_formattag == 0x0003) {  /* PCM_FLT */
        /* ok */
    } else if (!strcmp(track->codec_id, MKV_A_QDMC)
               || !strcmp(track->codec_id, MKV_A_QDMC2)) {
        sh_a->bitrate = 16000 * 8;
        sh_a->block_align = 1486;
    } else if (track->a_formattag == MP_FOURCC('M', 'P', '4', 'A')) {
        sh_a->bitrate = 16000 * 8;
        sh_a->block_align = 1024;

        if (strcmp(track->codec_id, MKV_A_AAC)) {
            /* Recreate the 'private data' */
            int srate_idx = aac_get_sample_rate_index(track->a_sfreq);
            const char *tail = "";
            if (strlen(track->codec_id) >= 12)
                tail = &track->codec_id[12];
            int profile = 3;
            if (!strncmp(tail, "MAIN", 4))
                profile = 0;
            else if (!strncmp(tail, "LC", 2))
                profile = 1;
            else if (!strncmp(tail, "SSR", 3))
                profile = 2;
            sh_a->codecdata = talloc_size(sh_a, 5);
            sh_a->codecdata[0] = ((profile + 1) << 3) | ((srate_idx & 0xE) >> 1);
            sh_a->codecdata[1] =
                ((srate_idx & 0x1) << 7) | (track->a_channels << 3);

            if (strstr(track->codec_id, "SBR") != NULL) {
                /* HE-AAC (aka SBR AAC) */
                sh_a->codecdata_len = 5;

                srate_idx = aac_get_sample_rate_index(sh_a->samplerate);
                sh_a->codecdata[2] = AAC_SYNC_EXTENSION_TYPE >> 3;
                sh_a->codecdata[3] = ((AAC_SYNC_EXTENSION_TYPE & 0x07) << 5) | 5;
                sh_a->codecdata[4] = (1 << 7) | (srate_idx << 3);
                track->default_duration = 1024.0 / (sh_a->samplerate / 2);
            } else {
                sh_a->codecdata_len = 2;
                track->default_duration = 1024.0 / sh_a->samplerate;
            }
        }
    } else if (track->a_formattag == MP_FOURCC('v', 'r', 'b', 's')) {
        /* VORBIS */
    } else if (!strcmp(track->codec_id, MKV_A_OPUS)
               || !strcmp(track->codec_id, MKV_A_OPUS_EXP)) {
        sh->format = MP_FOURCC('O', 'p', 'u', 's');
    } else if (!strncmp(track->codec_id, MKV_A_REALATRC, 7)) {
        if (track->private_size < RAPROPERTIES4_SIZE)
            goto error;
        /* Common initialization for all RealAudio codecs */
        unsigned char *src = track->private_data;

        sh_a->bitrate = 0;  /* FIXME !? */

        int version = AV_RB16(src + 4);
        unsigned int flavor = AV_RB16(src + 22);
        track->coded_framesize = AV_RB32(src + 24);
        track->sub_packet_h = AV_RB16(src + 40);
        sh_a->block_align = track->audiopk_size = AV_RB16(src + 42);
        track->sub_packet_size = AV_RB16(src + 44);
        if (version == 4) {
            src += RAPROPERTIES4_SIZE;
            src += src[0] + 1;
            src += src[0] + 1;
        } else {
            src += RAPROPERTIES5_SIZE;
        }

        if (track->audiopk_size == 0 || track->sub_packet_size == 0 ||
            track->sub_packet_h == 0 || track->coded_framesize == 0)
            goto error;
        if (track->coded_framesize > 0x40000000)
            goto error;

        src += 3;
        if (version == 5)
            src++;
        uint32_t codecdata_length = AV_RB32(src);
        if (codecdata_length > 0x1000000)
            goto error;
        src += 4;
        extradata_len = codecdata_length;
        extradata = src;

        switch (track->a_formattag) {
        case MP_FOURCC('a', 't', 'r', 'c'):
            if (flavor >= MP_ARRAY_SIZE(atrc_fl2bps))
                goto error;
            sh_a->bitrate = atrc_fl2bps[flavor] * 8;
            sh_a->block_align = track->sub_packet_size;
            break;
        case MP_FOURCC('c', 'o', 'o', 'k'):
            if (flavor >= MP_ARRAY_SIZE(cook_fl2bps))
                goto error;
            sh_a->bitrate = cook_fl2bps[flavor] * 8;
            sh_a->block_align = track->sub_packet_size;
            break;
        case MP_FOURCC('s', 'i', 'p', 'r'):
            if (flavor >= MP_ARRAY_SIZE(sipr_fl2bps))
                goto error;
            sh_a->bitrate = sipr_fl2bps[flavor] * 8;
            sh_a->block_align = track->coded_framesize;
            break;
        case MP_FOURCC('2', '8', '_', '8'):
            sh_a->bitrate = 3600 * 8;
            sh_a->block_align = track->coded_framesize;
            break;
        }

        track->audio_buf =
            talloc_array_size(track, track->sub_packet_h, track->audiopk_size);
        track->audio_timestamp =
            talloc_array(track, double, track->sub_packet_h);

    } else if (!strcmp(track->codec_id, MKV_A_FLAC)
               || (track->a_formattag == 0xf1ac)) {
        sh_a->bits_per_coded_sample = 0;
        sh_a->bitrate = 0;
        sh_a->block_align = 0;

        if (track->ms_compat)
            sh->format = MP_FOURCC('f', 'L', 'a', 'C');
        unsigned char *ptr = extradata;
        unsigned int size = extradata_len;
        if (size < 4 || ptr[0] != 'f' || ptr[1] != 'L' || ptr[2] != 'a'
            || ptr[3] != 'C') {
            sh_a->codecdata = talloc_size(sh_a, 4);
            sh_a->codecdata_len = 4;
            memcpy(sh_a->codecdata, "fLaC", 4);
        } else {
            sh_a->codecdata = talloc_size(sh_a, size);
            sh_a->codecdata_len = size;
            memcpy(sh_a->codecdata, ptr, size);
        }
    } else if (!strcmp(track->codec_id, MKV_A_ALAC)) {
        if (track->private_size) {
            sh_a->codecdata_len = track->private_size + 12;
            sh_a->codecdata = talloc_size(sh_a, sh_a->codecdata_len);
            char *data = sh_a->codecdata;
            AV_WB32(data + 0, sh_a->codecdata_len);
            memcpy(data + 4, "alac", 4);
            AV_WB32(data + 8, 0);
            memcpy(data + 12, track->private_data, track->private_size);
        }
    } else if (track->a_formattag == MP_FOURCC('W', 'V', 'P', 'K')) {
        /* ok */
    } else if (track->a_formattag == MP_FOURCC('T', 'R', 'H', 'D')) {
        track->parse = true;
    } else if (track->a_formattag == MP_FOURCC('T', 'T', 'A', '1')) {
        sh_a->codecdata_len = 30;
        sh_a->codecdata = talloc_zero_size(sh_a, sh_a->codecdata_len);
        if (!sh_a->codecdata)
            goto error;
        char *data = sh_a->codecdata;
        memcpy(data + 0, "TTA1", 4);
        AV_WL16(data + 4, 1);
        AV_WL16(data + 6, sh_a->channels.num);
        AV_WL16(data + 8, sh_a->bits_per_coded_sample);
        AV_WL32(data + 10, track->a_osfreq);
        // Bogus: last frame won't be played.
        AV_WL32(data + 14, 0);
    } else if (!track->ms_compat) {
        goto error;
    }

    // Some files have broken default DefaultDuration set, which will lead to
    // audio packets with incorrect timestamps. This follows FFmpeg commit
    // 6158a3b, sample see FFmpeg ticket 2508.
    if (sh_a->samplerate == 8000 && strcmp(track->codec_id, MKV_A_AC3) == 0)
        track->default_duration = 0;

    mp_set_codec_from_tag(sh);

    if (!sh_a->codecdata && extradata_len) {
        sh_a->codecdata = talloc_memdup(sh_a, extradata, extradata_len);
        sh_a->codecdata_len = extradata_len;
    }

    return 0;

 error:
    MP_WARN(demuxer, "Unknown/unsupported audio "
            "codec ID '%s' for track %u or missing/faulty\n"
            "private codec data.\n", track->codec_id, track->tnum);
    return 1;
}

static const char *const mkv_sub_tag[][2] = {
    { MKV_S_VOBSUB,     "dvd_subtitle" },
    { MKV_S_TEXTSSA,    "ass"},
    { MKV_S_TEXTASS,    "ass"},
    { MKV_S_SSA,        "ass"},
    { MKV_S_ASS,        "ass"},
    { MKV_S_TEXTASCII,  "subrip"},
    { MKV_S_TEXTUTF8,   "subrip"},
    { MKV_S_PGS,        "hdmv_pgs_subtitle"},
    { MKV_S_WEBVTT_S,   "webvtt-webm"},
    { MKV_S_WEBVTT_C,   "webvtt-webm"},
    { MKV_S_DVB,        "dvb_subtitle"},
    {0}
};

static int demux_mkv_open_sub(demuxer_t *demuxer, mkv_track_t *track)
{
    const char *subtitle_type = NULL;
    for (int n = 0; mkv_sub_tag[n][0]; n++) {
        if (strcmp(track->codec_id, mkv_sub_tag[n][0]) == 0) {
            subtitle_type = mkv_sub_tag[n][1];
            break;
        }
    }

    if (track->private_size > 0x10000000)
        return 1;

    struct sh_stream *sh = new_sh_stream(demuxer, STREAM_SUB);
    if (!sh)
        return 1;
    track->stream = sh;
    sh_sub_t *sh_s = sh->sub;
    sh->demuxer_id = track->tnum;
    sh->codec = subtitle_type;
    bstr in = (bstr){track->private_data, track->private_size};
    bstr buffer = demux_mkv_decode(demuxer->log, track, in, 2);
    if (buffer.start && buffer.start != track->private_data) {
        talloc_free(track->private_data);
        talloc_steal(track, buffer.start);
        track->private_data = buffer.start;
        track->private_size = buffer.len;
    }
    sh_s->extradata = talloc_size(sh, track->private_size);
    memcpy(sh_s->extradata, track->private_data, track->private_size);
    sh_s->extradata_len = track->private_size;
    if (track->language && (strcmp(track->language, "und") != 0))
        sh->lang = talloc_strdup(sh, track->language);
    sh->title = talloc_strdup(sh, track->name);
    sh->default_track = track->default_track;

    if (!subtitle_type)
        MP_ERR(demuxer, "Subtitle type '%s' is not supported.\n", track->codec_id);

    return 0;
}

static int read_ebml_header(demuxer_t *demuxer)
{
    stream_t *s = demuxer->stream;

    if (ebml_read_id(s) != EBML_ID_EBML)
        return 0;
    struct ebml_ebml ebml_master = {{0}};
    struct ebml_parse_ctx parse_ctx = { demuxer->log, .no_error_messages = true };
    if (ebml_read_element(s, &parse_ctx, &ebml_master, &ebml_ebml_desc) < 0)
        return 0;
    if (ebml_master.doc_type.start == NULL) {
        MP_VERBOSE(demuxer, "File has EBML header but no doctype."
                   " Assuming \"matroska\".\n");
    } else if (bstrcmp(ebml_master.doc_type, bstr0("matroska")) != 0
        && bstrcmp(ebml_master.doc_type, bstr0("webm")) != 0) {
        MP_DBG(demuxer, "no head found\n");
        talloc_free(parse_ctx.talloc_ctx);
        return 0;
    }
    if (ebml_master.doc_type_read_version > 2) {
        MP_WARN(demuxer, "This looks like a Matroska file, "
                "but we don't support format version %"PRIu64"\n",
                ebml_master.doc_type_read_version);
        talloc_free(parse_ctx.talloc_ctx);
        return 0;
    }
    if ((ebml_master.n_ebml_read_version
         && ebml_master.ebml_read_version != EBML_VERSION)
        || (ebml_master.n_ebml_max_size_length
            && ebml_master.ebml_max_size_length > 8)
        || (ebml_master.n_ebml_max_id_length
            && ebml_master.ebml_max_id_length != 4))
    {
        MP_WARN(demuxer, "This looks like a Matroska file, "
                "but the header has bad parameters\n");
        talloc_free(parse_ctx.talloc_ctx);
        return 0;
    }
    talloc_free(parse_ctx.talloc_ctx);

    return 1;
}

static int read_mkv_segment_header(demuxer_t *demuxer, int64_t *segment_end)
{
    stream_t *s = demuxer->stream;
    int num_skip = 0;
    if (demuxer->params)
        num_skip = demuxer->params->matroska_wanted_segment;

    while (!s->eof) {
        if (ebml_read_id(s) != MATROSKA_ID_SEGMENT) {
            MP_VERBOSE(demuxer, "segment not found\n");
            return 0;
        }
        MP_VERBOSE(demuxer, "+ a segment...\n");
        uint64_t len = ebml_read_length(s);
        *segment_end = (len == EBML_UINT_INVALID) ? 0 : stream_tell(s) + len;
        if (num_skip <= 0)
            return 1;
        num_skip--;
        MP_VERBOSE(demuxer, "  (skipping)\n");
        if (*segment_end <= 0)
            break;
        int64_t end = 0;
        stream_control(s, STREAM_CTRL_GET_SIZE, &end);
        if (*segment_end >= end)
            return 0;
        if (!stream_seek(s, *segment_end)) {
            MP_WARN(demuxer, "Failed to seek in file\n");
            return 0;
        }
        // Segments are like concatenated Matroska files
        if (!read_ebml_header(demuxer))
            return 0;
    }

    MP_VERBOSE(demuxer, "End of file, no further segments.\n");
    return 0;
}

static int demux_mkv_open(demuxer_t *demuxer, enum demux_check check)
{
    stream_t *s = demuxer->stream;
    struct MPOpts *opts = demuxer->opts;
    mkv_demuxer_t *mkv_d;
    int64_t start_pos;
    int64_t end_pos;

    bstr start = stream_peek(s, 4);
    uint32_t start_id = 0;
    for (int n = 0; n < start.len; n++)
        start_id = (start_id << 8) | start.start[n];
    if (start_id != EBML_ID_EBML)
        return -1;

    if (!read_ebml_header(demuxer))
        return -1;
    MP_VERBOSE(demuxer, "Found the head...\n");

    if (!read_mkv_segment_header(demuxer, &end_pos))
        return -1;

    mkv_d = talloc_zero(demuxer, struct mkv_demuxer);
    demuxer->priv = mkv_d;
    mkv_d->tc_scale = 1000000;
    mkv_d->segment_start = stream_tell(s);
    mkv_d->segment_end = end_pos;
    mkv_d->a_skip_preroll = 1;

    if (demuxer->params && demuxer->params->matroska_was_valid)
        *demuxer->params->matroska_was_valid = true;

    while (1) {
        start_pos = stream_tell(s);
        stream_peek(s, 4); // make sure we can always seek back
        uint32_t id = ebml_read_id(s);
        if (s->eof) {
            MP_WARN(demuxer, "Unexpected end of file (no clusters found)\n");
            break;
        }
        if (id == MATROSKA_ID_CLUSTER) {
            MP_VERBOSE(demuxer, "|+ found cluster\n");
            break;
        }
        int res = read_header_element(demuxer, id, start_pos);
        if (res < 0)
            return -1;
    }

    int64_t end = 0;
    stream_control(s, STREAM_CTRL_GET_SIZE, &end);

    // Read headers that come after the first cluster (i.e. require seeking).
    // Note: reading might increase ->num_headers.
    //       Likewise, ->headers might be reallocated.
    for (int n = 0; n < mkv_d->num_headers; n++) {
        struct header_elem *elem = &mkv_d->headers[n];
        if (elem->parsed)
            continue;
        // Warn against incomplete files and skip headers outside of range.
        if (elem->pos >= end) {
            elem->parsed = true; // don't bother if file is incomplete
            if (!mkv_d->eof_warning) {
                MP_WARN(demuxer, "SeekHead position beyond "
                        "end of file - incomplete file?\n");
                mkv_d->eof_warning = true;
            }
            continue;
        }
        if (elem->id == MATROSKA_ID_CUES) {
            // Read cues when they are needed, to avoid seeking on opening.
            MP_VERBOSE(demuxer, "Deferring reading cues.\n");
            continue;
        }
        if (read_deferred_element(demuxer, elem) < 0)
            return -1;
    }
    if (!stream_seek(s, start_pos)) {
        MP_ERR(demuxer, "Couldn't seek back after reading headers?\n");
        return -1;
    }

    MP_VERBOSE(demuxer, "All headers are parsed!\n");

    process_tags(demuxer);
    display_create_tracks(demuxer);
    add_coverart(demuxer);
    demuxer->allow_refresh_seeks = true;

    if (opts->demux_mkv->probe_duration)
        probe_last_timestamp(demuxer);

    return 0;
}

static bool bstr_read_u8(bstr *buffer, uint8_t *out_u8)
{
    if (buffer->len > 0) {
        *out_u8 = buffer->start[0];
        buffer->len -= 1;
        buffer->start += 1;
        return true;
    } else {
        return false;
    }
}

static int demux_mkv_read_block_lacing(bstr *buffer, int *laces,
                                       uint32_t lace_size[MAX_NUM_LACES])
{
    uint32_t total = 0;
    uint8_t flags, t;
    int i;

    /* lacing flags */
    if (!bstr_read_u8(buffer, &flags))
        goto error;

    int type = (flags >> 1) & 0x03;
    if (type == 0) {           /* no lacing */
        *laces = 1;
        lace_size[0] = buffer->len;
    } else {
        if (!bstr_read_u8(buffer, &t))
            goto error;
        *laces = t + 1;

        switch (type) {
        case 1:                /* xiph lacing */
            for (i = 0; i < *laces - 1; i++) {
                lace_size[i] = 0;
                do {
                    if (!bstr_read_u8(buffer, &t))
                        goto error;
                    lace_size[i] += t;
                } while (t == 0xFF);
                total += lace_size[i];
            }
            lace_size[i] = buffer->len - total;
            break;

        case 2:                /* fixed-size lacing */
            for (i = 0; i < *laces; i++)
                lace_size[i] = buffer->len / *laces;
            break;

        case 3:;                /* EBML lacing */
            uint64_t num = ebml_read_vlen_uint(buffer);
            if (num == EBML_UINT_INVALID)
                goto error;
            if (num > buffer->len)
                goto error;

            total = lace_size[0] = num;
            for (i = 1; i < *laces - 1; i++) {
                int64_t snum = ebml_read_vlen_int(buffer);
                if (snum == EBML_INT_INVALID)
                    goto error;
                lace_size[i] = lace_size[i - 1] + snum;
                total += lace_size[i];
            }
            lace_size[i] = buffer->len - total;
            break;

        default: abort();
        }
    }

    total = buffer->len;
    for (i = 0; i < *laces; i++) {
        if (lace_size[i] > total)
            goto error;
        total -= lace_size[i];
    }
    if (total != 0)
        goto error;

    return 0;

 error:
    return 1;
}

// Return whether the packet was handled & freed.
static bool handle_realaudio(demuxer_t *demuxer, mkv_track_t *track,
                             struct demux_packet *orig)
{
    uint32_t sps = track->sub_packet_size;
    uint32_t sph = track->sub_packet_h;
    uint32_t cfs = track->coded_framesize; // restricted to [1,0x40000000]
    uint32_t w = track->audiopk_size;
    uint32_t spc = track->sub_packet_cnt;
    uint8_t *buffer = orig->buffer;
    uint32_t size = orig->len;
    demux_packet_t *dp;
    // track->audio_buf allocation size
    size_t audiobuf_size = sph * w;

    if (!track->audio_buf || !track->audio_timestamp)
        return false;

    switch (track->a_formattag) {
    case MP_FOURCC('2', '8', '_', '8'):
        for (int x = 0; x < sph / 2; x++) {
            uint64_t dst_offset = x * 2 * w + spc * (uint64_t)cfs;
            if (dst_offset + cfs > audiobuf_size)
                goto error;
            uint64_t src_offset = x * (uint64_t)cfs;
            if (src_offset + cfs > size)
                goto error;
            memcpy(track->audio_buf + dst_offset, buffer + src_offset, cfs);
        }
        break;
    case MP_FOURCC('c', 'o', 'o', 'k'):
    case MP_FOURCC('a', 't', 'r', 'c'):
        for (int x = 0; x < w / sps; x++) {
            uint32_t dst_offset =
                sps * (sph * x + ((sph + 1) / 2) * (spc & 1) + (spc >> 1));
            if (dst_offset + sps > audiobuf_size)
                goto error;
            uint32_t src_offset = sps * x;
            if (src_offset + sps > size)
                goto error;
            memcpy(track->audio_buf + dst_offset, buffer + src_offset, sps);
        }
        break;
    case MP_FOURCC('s', 'i', 'p', 'r'):
        if (spc * w + w > audiobuf_size || w > size)
            goto error;
        memcpy(track->audio_buf + spc * w, buffer, w);
        if (spc == sph - 1) {
            int n;
            int bs = sph * w * 2 / 96;      // nibbles per subpacket
            // Perform reordering
            for (n = 0; n < 38; n++) {
                unsigned int i = bs * sipr_swaps[n][0]; // 77 max
                unsigned int o = bs * sipr_swaps[n][1]; // 95 max
                // swap nibbles of block 'i' with 'o'
                for (int j = 0; j < bs; j++) {
                    if (i / 2 >= audiobuf_size || o / 2 >= audiobuf_size)
                        goto error;
                    uint8_t iv = track->audio_buf[i / 2];
                    uint8_t ov = track->audio_buf[o / 2];
                    int x = (i & 1) ? iv >> 4 : iv & 0x0F;
                    int y = (o & 1) ? ov >> 4 : ov & 0x0F;
                    track->audio_buf[o / 2] = (ov & 0x0F) | (o & 1 ? x << 4 : x);
                    track->audio_buf[i / 2] = (iv & 0x0F) | (i & 1 ? y << 4 : y);
                    i++;
                    o++;
                }
            }
        }
        break;
    default:
        // Not a codec that requires reordering
        return false;
    }

    track->audio_timestamp[track->sub_packet_cnt] =
        track->ra_pts == orig->pts ? 0 : orig->pts;
    track->ra_pts = orig->pts;

    if (++(track->sub_packet_cnt) == sph) {
        track->sub_packet_cnt = 0;
        // apk_usize has same range as coded_framesize in worst case
        uint32_t apk_usize = track->stream->audio->block_align;
        if (apk_usize > audiobuf_size)
            goto error;
        // Release all the audio packets
        for (int x = 0; x < sph * w / apk_usize; x++) {
            dp = new_demux_packet_from(track->audio_buf + x * apk_usize,
                                        apk_usize);
            if (!dp)
                goto error;
            /* Put timestamp only on packets that correspond to original
             * audio packets in file */
            dp->pts = (x * apk_usize % w) ? MP_NOPTS_VALUE :
                track->audio_timestamp[x * apk_usize / w];
            dp->pos = orig->pos + x;
            dp->keyframe = !x;   // Mark first packet as keyframe
            demux_add_packet(track->stream, dp);
        }
    }

error:
    talloc_free(orig);
    return true;
}

static void mkv_seek_reset(demuxer_t *demuxer)
{
    mkv_demuxer_t *mkv_d = demuxer->priv;

    for (int i = 0; i < mkv_d->num_tracks; i++) {
        mkv_track_t *track = mkv_d->tracks[i];
        if (track->av_parser)
            av_parser_close(track->av_parser);
        track->av_parser = NULL;
        if (track->av_parser_codec) {
            avcodec_close(track->av_parser_codec);
            av_free(track->av_parser_codec);
        }
        track->av_parser_codec = NULL;
    }
}

// Copied from libavformat/matroskadec.c (FFmpeg 310f9dd / 2013-05-30)
// Originally added with Libav commit 9b6f47c
// License: LGPL v2.1 or later
// Author header: The FFmpeg Project (this function still came from Libav)
// Modified to use talloc, removed ffmpeg/libav specific error codes.
static int libav_parse_wavpack(mkv_track_t *track, uint8_t *src,
                               uint8_t **pdst, int *size)
{
    uint8_t *dst = NULL;
    int dstlen   = 0;
    int srclen   = *size;
    uint32_t samples;
    uint16_t ver;
    int offset = 0;

    if (srclen < 12 || track->private_size < 2)
        return -1;

    ver = AV_RL16(track->private_data);

    samples = AV_RL32(src);
    src    += 4;
    srclen -= 4;

    while (srclen >= 8) {
        int multiblock;
        uint32_t blocksize;
        uint8_t *tmp;

        uint32_t flags = AV_RL32(src);
        uint32_t crc   = AV_RL32(src + 4);
        src    += 8;
        srclen -= 8;

        multiblock = (flags & 0x1800) != 0x1800;
        if (multiblock) {
            if (srclen < 4)
                goto fail;
            blocksize = AV_RL32(src);
            src    += 4;
            srclen -= 4;
        } else {
            blocksize = srclen;
        }

        if (blocksize > srclen)
            goto fail;

        if (dstlen > 0x10000000 || blocksize > 0x10000000)
            goto fail;

        tmp = talloc_realloc(track->parser_tmp, dst, uint8_t,
                             dstlen + blocksize + 32);
        if (!tmp)
            goto fail;
        dst     = tmp;
        dstlen += blocksize + 32;

        AV_WL32(dst + offset,      MKTAG('w', 'v', 'p', 'k')); // tag
        AV_WL32(dst + offset + 4,  blocksize + 24);         // blocksize - 8
        AV_WL16(dst + offset + 8,  ver);                    // version
        AV_WL16(dst + offset + 10, 0);                      // track/index_no
        AV_WL32(dst + offset + 12, 0);                      // total samples
        AV_WL32(dst + offset + 16, 0);                      // block index
        AV_WL32(dst + offset + 20, samples);                // number of samples
        AV_WL32(dst + offset + 24, flags);                  // flags
        AV_WL32(dst + offset + 28, crc);                    // crc
        memcpy (dst + offset + 32, src, blocksize);         // block data

        src    += blocksize;
        srclen -= blocksize;
        offset += blocksize + 32;
    }

    *pdst = dst;
    *size = dstlen;

    return 0;

fail:
    talloc_free(dst);
    return -1;
}

static void mkv_parse_and_add_packet(demuxer_t *demuxer, mkv_track_t *track,
                                     struct demux_packet *dp)
{
    struct sh_stream *stream = track->stream;

    if (stream->type == STREAM_AUDIO && handle_realaudio(demuxer, track, dp))
        return;

    if (track->a_formattag == MP_FOURCC('W', 'V', 'P', 'K')) {
        int size = dp->len;
        uint8_t *parsed;
        if (libav_parse_wavpack(track, dp->buffer, &parsed, &size) >= 0) {
            struct demux_packet *new = new_demux_packet_from(parsed, size);
            if (new) {
                demux_packet_copy_attribs(new, dp);
                talloc_free(dp);
                demux_add_packet(stream, new);
                return;
            }
        }
    }

    if (track->codec_id && strcmp(track->codec_id, MKV_V_PRORES) == 0) {
        size_t newlen = dp->len + 8;
        struct demux_packet *new = new_demux_packet(newlen);
        if (new) {
            AV_WB32(new->buffer + 0, newlen);
            AV_WB32(new->buffer + 4, MKBETAG('i', 'c', 'p', 'f'));
            memcpy(new->buffer + 8, dp->buffer, dp->len);
            demux_packet_copy_attribs(new, dp);
            talloc_free(dp);
            demux_add_packet(stream, new);
            return;
        }
    }

    if (track->parse && !track->av_parser) {
        int id = mp_codec_to_av_codec_id(track->stream->codec);
        const AVCodec *codec = avcodec_find_decoder(id);
        track->av_parser = av_parser_init(id);
        if (codec)
            track->av_parser_codec = avcodec_alloc_context3(codec);
    }

    if (!track->parse || !track->av_parser || !track->av_parser_codec) {
        demux_add_packet(stream, dp);
        return;
    }

    double tb = track->parse_timebase;
    int64_t pts = dp->pts == MP_NOPTS_VALUE ? AV_NOPTS_VALUE : dp->pts * tb;
    int64_t dts = dp->dts == MP_NOPTS_VALUE ? AV_NOPTS_VALUE : dp->dts * tb;

    while (dp->len) {
        uint8_t *data = NULL;
        int size = 0;
        int len = av_parser_parse2(track->av_parser, track->av_parser_codec,
                                   &data, &size, dp->buffer, dp->len,
                                   pts, dts, 0);
        if (len < 0 || len > dp->len)
            break;
        dp->buffer += len;
        dp->len -= len;
        dp->pos += len;
        if (size) {
            struct demux_packet *new = new_demux_packet_from(data, size);
            if (!new)
                break;
            demux_packet_copy_attribs(new, dp);
            if (track->parse_timebase) {
                new->pts = track->av_parser->pts == AV_NOPTS_VALUE
                         ? MP_NOPTS_VALUE : track->av_parser->pts / tb;
                new->dts = track->av_parser->dts == AV_NOPTS_VALUE
                         ? MP_NOPTS_VALUE : track->av_parser->dts / tb;
            }
            demux_add_packet(stream, new);
        }
        pts = dts = AV_NOPTS_VALUE;
    }

    if (dp->len) {
        demux_add_packet(stream, dp);
    } else {
        talloc_free(dp);
    }
}

struct block_info {
    uint64_t duration, discardpadding;
    bool simple, keyframe;
    uint64_t timecode;
    mkv_track_t *track;
    bstr data;
    void *alloc;
    int64_t filepos;
};

static void free_block(struct block_info *block)
{
    free(block->alloc);
    block->alloc = NULL;
    block->data = (bstr){0};
}

static void index_block(demuxer_t *demuxer, struct block_info *block)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    if (block->keyframe) {
        add_block_position(demuxer, block->track, mkv_d->cluster_start,
                           block->timecode / mkv_d->tc_scale,
                           block->duration / mkv_d->tc_scale);
    }
}

static int read_block(demuxer_t *demuxer, int64_t end, struct block_info *block)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    stream_t *s = demuxer->stream;
    uint64_t num;
    int16_t time;
    uint64_t length;
    int res = -1;

    free_block(block);
    length = ebml_read_length(s);
    if (length > 500000000 || stream_tell(s) + length > (uint64_t)end)
        goto exit;
    block->alloc = malloc(length + AV_LZO_INPUT_PADDING);
    if (!block->alloc)
        goto exit;
    block->data = (bstr){block->alloc, length};
    block->filepos = stream_tell(s);
    if (stream_read(s, block->data.start, block->data.len) != block->data.len)
        goto exit;

    // Parse header of the Block element
    /* first byte(s): track num */
    num = ebml_read_vlen_uint(&block->data);
    if (num == EBML_UINT_INVALID)
        goto exit;
    /* time (relative to cluster time) */
    if (block->data.len < 3)
        goto exit;
    time = block->data.start[0] << 8 | block->data.start[1];
    block->data.start += 2;
    block->data.len -= 2;
    if (block->simple)
        block->keyframe = block->data.start[0] & 0x80;
    block->timecode = time * mkv_d->tc_scale + mkv_d->cluster_tc;
    for (int i = 0; i < mkv_d->num_tracks; i++) {
        if (mkv_d->tracks[i]->tnum == num) {
            block->track = mkv_d->tracks[i];
            break;
        }
    }
    if (!block->track) {
        res = 0;
        goto exit;
    }

    res = 1;
exit:
    if (res <= 0)
        free_block(block);
    return res;
}

static double fix_timestamp(demuxer_t *demuxer, mkv_track_t *track, double ts)
{
    mkv_demuxer_t *mkv_d = demuxer->priv;
    if (demuxer->opts->demux_mkv->fix_timestamps && track->default_duration > 0) {
        // Assume that timestamps have been rounded to the timecode scale.
        double quant = MPMIN(mkv_d->tc_scale / 1e9, 0.001);
        double rts = rint(ts / track->default_duration) * track->default_duration;
        if (fabs(rts - ts) < quant)
            ts = rts;
    }
    return ts;
}

static int handle_block(demuxer_t *demuxer, struct block_info *block_info)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    int laces;
    double current_pts;
    bstr data = block_info->data;
    bool keyframe = block_info->keyframe;
    uint64_t block_duration = block_info->duration;
    uint64_t tc = block_info->timecode;
    mkv_track_t *track = block_info->track;
    struct sh_stream *stream = track->stream;
    uint32_t lace_size[MAX_NUM_LACES];
    bool use_this_block = tc >= mkv_d->skip_to_timecode;

    if (!demux_stream_is_selected(stream))
        return 0;

    if (demux_mkv_read_block_lacing(&data, &laces, lace_size)) {
        MP_ERR(demuxer, "Bad input [lacing]\n");
        return 0;
    }

    current_pts = fix_timestamp(demuxer, track, tc / 1e9) - track->codec_delay;

    if (track->type == MATROSKA_TRACK_AUDIO) {
        if (mkv_d->a_skip_to_keyframe)
            use_this_block &= keyframe;
    } else if (track->type == MATROSKA_TRACK_SUBTITLE) {
        if (!use_this_block && mkv_d->subtitle_preroll)
            use_this_block = 1;
        if (use_this_block) {
            if (mkv_d->subtitle_preroll) {
                mkv_d->subtitle_preroll--;
            } else {
                // This could overflow the demuxer queue.
                if (mkv_d->a_skip_to_keyframe || mkv_d->v_skip_to_keyframe)
                    use_this_block = 0;
            }
        }
        if (use_this_block) {
            if (laces > 1) {
                MP_WARN(demuxer, "Subtitles use Matroska "
                       "lacing. This is abnormal and not supported.\n");
                use_this_block = 0;
            }
        }
    } else if (track->type == MATROSKA_TRACK_VIDEO) {
        if (mkv_d->v_skip_to_keyframe)
            use_this_block &= keyframe;
    }

    if (use_this_block) {
        uint64_t filepos = block_info->filepos;
        mkv_d->last_pts = current_pts;

        for (int i = 0; i < laces; i++) {
            bstr block = bstr_splice(data, 0, lace_size[i]);
            data = bstr_cut(data, lace_size[i]);

            block = demux_mkv_decode(demuxer->log, track, block, 1);

            demux_packet_t *dp = new_demux_packet_from(block.start, block.len);
            if (!dp)
                break;
            dp->keyframe = keyframe;
            dp->pos = filepos;
            /* If default_duration is 0, assume no pts value is known
             * for packets after the first one (rather than all pts
             * values being the same). Also, don't use it for extra
             * packets resulting from parsing. */
            if (i == 0 || track->default_duration)
                dp->pts = mkv_d->last_pts + i * track->default_duration;
            if (track->ms_compat)
                MPSWAP(double, dp->pts, dp->dts);
            if (i == 0)
                dp->duration = block_duration / 1e9;
            if (stream->type == STREAM_AUDIO) {
                unsigned int srate = track->a_sfreq;
                demux_packet_set_padding(dp,
                    mkv_d->a_skip_preroll ? track->codec_delay * srate : 0,
                    block_info->discardpadding / 1e9 * srate);
                mkv_d->a_skip_preroll = 0;
            }

            mkv_parse_and_add_packet(demuxer, track, dp);
            talloc_free_children(track->parser_tmp);
            filepos += block.len;
        }

        if (stream->type == STREAM_VIDEO) {
            mkv_d->v_skip_to_keyframe = 0;
            mkv_d->skip_to_timecode = 0;
            mkv_d->subtitle_preroll = 0;
        } else if (stream->type == STREAM_AUDIO) {
            mkv_d->a_skip_to_keyframe = 0;
        }

        return 1;
    }

    return 0;
}

static int read_block_group(demuxer_t *demuxer, int64_t end,
                            struct block_info *block)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    stream_t *s = demuxer->stream;
    *block = (struct block_info){ .keyframe = true };

    while (stream_tell(s) < end) {
        switch (ebml_read_id(s)) {
        case MATROSKA_ID_BLOCKDURATION:
            block->duration = ebml_read_uint(s);
            if (block->duration == EBML_UINT_INVALID)
                goto error;
            block->duration *= mkv_d->tc_scale;
            break;

        case MATROSKA_ID_DISCARDPADDING:
            block->discardpadding = ebml_read_uint(s);
            if (block->discardpadding == EBML_UINT_INVALID)
                goto error;
            break;

        case MATROSKA_ID_BLOCK:
            if (read_block(demuxer, end, block) < 0)
                goto error;
            break;

        case MATROSKA_ID_REFERENCEBLOCK:;
            int64_t num = ebml_read_int(s);
            if (num == EBML_INT_INVALID)
                goto error;
            if (num)
                block->keyframe = false;
            break;

        case MATROSKA_ID_CLUSTER:
        case EBML_ID_INVALID:
            goto error;

        default:
            if (ebml_read_skip(demuxer->log, end, s) != 0)
                goto error;
            break;
        }
    }

    return block->data.start ? 1 : 0;

error:
    free_block(block);
    return -1;
}

static int read_next_block(demuxer_t *demuxer, struct block_info *block)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    stream_t *s = demuxer->stream;

    while (1) {
        while (stream_tell(s) < mkv_d->cluster_end) {
            int64_t start_filepos = stream_tell(s);
            switch (ebml_read_id(s)) {
            case MATROSKA_ID_TIMECODE: {
                uint64_t num = ebml_read_uint(s);
                if (num == EBML_UINT_INVALID)
                    goto find_next_cluster;
                mkv_d->cluster_tc = num * mkv_d->tc_scale;
                break;
            }

            case MATROSKA_ID_BLOCKGROUP: {
                int64_t end = ebml_read_length(s);
                end += stream_tell(s);
                if (end > mkv_d->cluster_end)
                    goto find_next_cluster;
                int res = read_block_group(demuxer, end, block);
                if (res < 0)
                    goto find_next_cluster;
                if (res > 0)
                    return 1;
                break;
            }

            case MATROSKA_ID_SIMPLEBLOCK: {
                *block = (struct block_info){ .simple = true };
                int res = read_block(demuxer, mkv_d->cluster_end, block);
                if (res < 0)
                    goto find_next_cluster;
                if (res > 0)
                    return 1;
                break;
            }

            case MATROSKA_ID_CLUSTER:
                mkv_d->cluster_start = start_filepos;
                goto next_cluster;

            case EBML_ID_INVALID:
                goto find_next_cluster;

            default: ;
                if (ebml_read_skip(demuxer->log, mkv_d->cluster_end, s) != 0)
                    goto find_next_cluster;
                break;
            }
        }

    find_next_cluster:
        mkv_d->cluster_end = 0;
        for (;;) {
            mkv_d->cluster_start = stream_tell(s);
            uint32_t id = ebml_read_id(s);
            if (id == MATROSKA_ID_CLUSTER)
                break;
            if (s->eof)
                return -1;
            if (demux_cancel_test(demuxer))
                return -1;
            if (id == EBML_ID_EBML && stream_tell(s) >= mkv_d->segment_end) {
                // Appended segment - don't use its clusters, consider this EOF.
                stream_seek(s, stream_tell(s) - 4);
                return -1;
            }
            // For the sake of robustness, consider even unknown level 1
            // elements the same as unknown/broken IDs.
            if (!ebml_is_mkv_level1_id(id) ||
                ebml_read_skip(demuxer->log, -1, s) != 0)
            {
                ebml_resync_cluster(demuxer->log, s);
            }
        }
    next_cluster:
        mkv_d->cluster_end = ebml_read_length(s);
        // mkv files for "streaming" can have this legally
        if (mkv_d->cluster_end != EBML_UINT_INVALID)
            mkv_d->cluster_end += stream_tell(s);
    }
}

static int demux_mkv_fill_buffer(demuxer_t *demuxer)
{
    for (;;) {
        int res;
        struct block_info block;
        res = read_next_block(demuxer, &block);
        if (res < 0)
            return 0;
        if (res > 0) {
            index_block(demuxer, &block);
            res = handle_block(demuxer, &block);
            free_block(&block);
            if (res > 0)
                return 1;
        }
    }
}

static mkv_index_t *get_highest_index_entry(struct demuxer *demuxer)
{
    struct mkv_demuxer *mkv_d = demuxer->priv;
    assert(!mkv_d->index_complete); // would require separate code

    mkv_index_t *index = NULL;
    for (int n = 0; n < mkv_d->num_tracks; n++) {
        int n_index = mkv_d->tracks[n]->last_index_entry;
        if (n_index >= 0) {
            mkv_index_t *index2 = &mkv_d->indexes[n_index];
            if (!index || index2->filepos > index->filepos)
                index = index2;
        }
    }
    return index;
}

static int create_index_until(struct demuxer *demuxer, uint64_t timecode)
{
    struct mkv_demuxer *mkv_d = demuxer->priv;
    struct stream *s = demuxer->stream;

    read_deferred_cues(demuxer);

    if (mkv_d->index_complete)
        return 0;

    mkv_index_t *index = get_highest_index_entry(demuxer);

    if (!index || index->timecode * mkv_d->tc_scale < timecode) {
        if (index)
            stream_seek(s, index->filepos);
        MP_VERBOSE(demuxer, "creating index until TC %" PRIu64 "\n", timecode);
        for (;;) {
            int res;
            struct block_info block;
            res = read_next_block(demuxer, &block);
            if (res < 0)
                break;
            if (res > 0) {
                index_block(demuxer, &block);
                free_block(&block);
            }
            index = get_highest_index_entry(demuxer);
            if (index && index->timecode * mkv_d->tc_scale >= timecode)
                break;
        }
    }
    if (!mkv_d->indexes) {
        MP_WARN(demuxer, "no target for seek found\n");
        return -1;
    }
    return 0;
}

#define FLAG_BACKWARD 1
#define FLAG_SUBPREROLL 2
static struct mkv_index *seek_with_cues(struct demuxer *demuxer, int seek_id,
                                        int64_t target_timecode, int flags)
{
    struct MPOpts *opts = demuxer->opts;
    struct mkv_demuxer *mkv_d = demuxer->priv;
    struct mkv_index *index = NULL;

    int64_t min_diff = INT64_MIN;
    for (size_t i = 0; i < mkv_d->num_indexes; i++) {
        if (seek_id < 0 || mkv_d->indexes[i].tnum == seek_id) {
            int64_t diff =
                target_timecode -
                (int64_t) (mkv_d->indexes[i].timecode * mkv_d->tc_scale);
            if (flags & FLAG_BACKWARD)
                diff = -diff;
            if (min_diff != INT64_MIN) {
                if (diff <= 0) {
                    if (min_diff <= 0 && diff <= min_diff)
                        continue;
                } else if (diff >= min_diff)
                    continue;
            }
            min_diff = diff;
            index = mkv_d->indexes + i;
        }
    }

    if (index) {        /* We've found an entry. */
        uint64_t seek_pos = index->filepos;
        if (flags & FLAG_SUBPREROLL) {
            // Find the cluster with the highest filepos, that has a timestamp
            // still lower than min_tc.
            double secs = opts->demux_mkv->subtitle_preroll_secs;
            uint64_t pre = MPMIN(INT64_MAX, secs * 1e9 / mkv_d->tc_scale);
            uint64_t min_tc = pre < index->timecode ? index->timecode - pre : 0;
            uint64_t prev_target = 0;
            uint64_t prev_tc = 0;
            for (size_t i = 0; i < mkv_d->num_indexes; i++) {
                if (seek_id < 0 || mkv_d->indexes[i].tnum == seek_id) {
                    struct mkv_index *cur = &mkv_d->indexes[i];
                    if (cur->timecode <= min_tc && cur->timecode >= prev_tc) {
                        prev_tc = cur->timecode;
                        prev_target = cur->filepos;
                    }
                }
            }
            if (mkv_d->index_has_durations) {
                // Find the earliest cluster that is not before prev_target,
                // but contains subtitle packets overlapping with the cluster
                // at seek_pos.
                uint64_t target = seek_pos;
                for (size_t i = 0; i < mkv_d->num_indexes; i++) {
                    struct mkv_index *cur = &mkv_d->indexes[i];
                    if (cur->timecode <= index->timecode &&
                        cur->timecode + cur->duration > index->timecode &&
                        cur->filepos >= prev_target &&
                        cur->filepos < target)
                    {
                        target = cur->filepos;
                    }
                }
                prev_target = target;
            }
            if (prev_target)
                seek_pos = prev_target;
        }

        mkv_d->cluster_end = 0;
        stream_seek(demuxer->stream, seek_pos);
    }
    return index;
}

static void demux_mkv_seek(demuxer_t *demuxer, double rel_seek_secs, int flags)
{
    mkv_demuxer_t *mkv_d = demuxer->priv;
    int64_t old_pos = stream_tell(demuxer->stream);
    uint64_t v_tnum = -1;
    uint64_t a_tnum = -1;
    bool st_active[STREAM_TYPE_COUNT] = {0};
    mkv_seek_reset(demuxer);
    for (int i = 0; i < mkv_d->num_tracks; i++) {
        mkv_track_t *track = mkv_d->tracks[i];
        if (demux_stream_is_selected(track->stream)) {
            st_active[track->stream->type] = true;
            if (track->type == MATROSKA_TRACK_VIDEO)
                v_tnum = track->tnum;
            if (track->type == MATROSKA_TRACK_AUDIO)
                a_tnum = track->tnum;
        }
    }

    int cueflags = (flags & SEEK_BACKWARD) ? FLAG_BACKWARD : 0;

    mkv_d->subtitle_preroll = NUM_SUB_PREROLL_PACKETS;
    if (((flags & SEEK_HR) || demuxer->opts->demux_mkv->subtitle_preroll) &&
        st_active[STREAM_SUB] && st_active[STREAM_VIDEO])
        cueflags |= FLAG_SUBPREROLL;

    // Adjust the target a little bit to catch cases where the target position
    // specifies a keyframe with high, but not perfect, precision.
    rel_seek_secs += flags & SEEK_FORWARD ? -0.005 : 0.005;

    if (!(flags & SEEK_FACTOR)) {       /* time in secs */
        mkv_index_t *index = NULL;

        if (!(flags & SEEK_ABSOLUTE))   /* relative seek */
            rel_seek_secs += mkv_d->last_pts;
        rel_seek_secs = FFMAX(rel_seek_secs, 0);
        int64_t target_timecode = rel_seek_secs * 1e9 + 0.5;

        if (create_index_until(demuxer, target_timecode) >= 0) {
            int seek_id = st_active[STREAM_VIDEO] ? v_tnum : a_tnum;
            index = seek_with_cues(demuxer, seek_id, target_timecode, cueflags);
            if (!index)
                index = seek_with_cues(demuxer, -1, target_timecode, cueflags);
        }

        if (!index)
            stream_seek(demuxer->stream, old_pos);

        if (flags & SEEK_FORWARD) {
            mkv_d->skip_to_timecode = target_timecode;
        } else {
            mkv_d->skip_to_timecode = index ? index->timecode * mkv_d->tc_scale
                                            : 0;
        }
    } else {
        stream_t *s = demuxer->stream;

        read_deferred_cues(demuxer);

        int64_t size = 0;
        stream_control(s, STREAM_CTRL_GET_SIZE, &size);

        int64_t target_filepos = size * MPCLAMP(rel_seek_secs, 0, 1);

        mkv_index_t *index = NULL;
        if (mkv_d->index_complete) {
            for (size_t i = 0; i < mkv_d->num_indexes; i++) {
                if (mkv_d->indexes[i].tnum == v_tnum) {
                    if ((index == NULL)
                        || ((mkv_d->indexes[i].filepos >= target_filepos)
                            && ((index->filepos < target_filepos)
                                || (mkv_d->indexes[i].filepos < index->filepos))))
                        index = &mkv_d->indexes[i];
                }
            }
        }

        mkv_d->cluster_end = 0;

        if (index) {
            stream_seek(s, index->filepos);
            mkv_d->skip_to_timecode = index->timecode * mkv_d->tc_scale;
        } else {
            stream_seek(s, target_filepos);
            if (ebml_resync_cluster(mp_null_log, s) < 0) {
                // Assume EOF
                mkv_d->cluster_end = size;
            }
        }
    }

    mkv_d->v_skip_to_keyframe = st_active[STREAM_VIDEO];
    mkv_d->a_skip_to_keyframe = st_active[STREAM_AUDIO];
    mkv_d->a_skip_preroll = mkv_d->a_skip_to_keyframe;

    demux_mkv_fill_buffer(demuxer);
}

static void probe_last_timestamp(struct demuxer *demuxer)
{
    mkv_demuxer_t *mkv_d = demuxer->priv;
    int64_t old_pos = stream_tell(demuxer->stream);

    if (!demuxer->seekable)
        return;

    // Pick some arbitrary video track
    int v_tnum = -1;
    for (int n = 0; n < mkv_d->num_tracks; n++) {
        if (mkv_d->tracks[n]->type == MATROSKA_TRACK_VIDEO) {
            v_tnum = mkv_d->tracks[n]->tnum;
            break;
        }
    }
    if (v_tnum < 0)
        return;

    read_deferred_cues(demuxer);

    if (!mkv_d->index_complete)
        return;

    // Find last cluster that still has video packets
    int64_t target = 0;
    for (size_t i = 0; i < mkv_d->num_indexes; i++) {
        struct mkv_index *cur = &mkv_d->indexes[i];
        if (cur->tnum == v_tnum)
            target = MPMAX(target, cur->filepos);
    }
    if (!target)
        return;

    if (!stream_seek(demuxer->stream, target))
        return;

    int64_t last_ts[STREAM_TYPE_COUNT] = {0};
    while (1) {
        struct block_info block;
        int res = read_next_block(demuxer, &block);
        if (res < 0)
            break;
        if (res > 0) {
            if (block.track && block.track->stream) {
                enum stream_type type = block.track->stream->type;
                uint64_t endtime = block.timecode + block.duration;
                if (last_ts[type] < endtime)
                    last_ts[type] = endtime;
            }
            free_block(&block);
        }
    }

    if (last_ts[STREAM_VIDEO])
        mkv_d->duration = last_ts[STREAM_VIDEO] / 1e9;

    stream_seek(demuxer->stream, old_pos);
    mkv_d->cluster_start = mkv_d->cluster_end = 0;
}

static int demux_mkv_control(demuxer_t *demuxer, int cmd, void *arg)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;

    switch (cmd) {
    case DEMUXER_CTRL_GET_TIME_LENGTH:
        if (mkv_d->duration == 0)
            return DEMUXER_CTRL_DONTKNOW;

        *((double *) arg) = (double) mkv_d->duration;
        return DEMUXER_CTRL_OK;
    default:
        return DEMUXER_CTRL_NOTIMPL;
    }
}

static void mkv_free(struct demuxer *demuxer)
{
    struct mkv_demuxer *mkv_d = demuxer->priv;
    if (!mkv_d)
        return;
    mkv_seek_reset(demuxer);
    for (int i = 0; i < mkv_d->num_tracks; i++)
        demux_mkv_free_trackentry(mkv_d->tracks[i]);
}

const demuxer_desc_t demuxer_desc_matroska = {
    .name = "mkv",
    .desc = "Matroska",
    .open = demux_mkv_open,
    .fill_buffer = demux_mkv_fill_buffer,
    .close = mkv_free,
    .seek = demux_mkv_seek,
    .control = demux_mkv_control,
    .load_timeline = build_ordered_chapter_timeline,
};

bool demux_matroska_uid_cmp(struct matroska_segment_uid *a,
                            struct matroska_segment_uid *b)
{
    return (!memcmp(a->segment, b->segment, 16) &&
            a->edition == b->edition);
}
