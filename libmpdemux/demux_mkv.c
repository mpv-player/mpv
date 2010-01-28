/*
 * Matroska demuxer
 * Copyright (C) 2004 Aurelien Jacobs <aurel@gnuage.org>
 * Based on the one written by Ronald Bultje for gstreamer
 * and on demux_mkv.cpp from Moritz Bunkus.
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>

#include "talloc.h"
#include "options.h"
#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "ebml.h"
#include "matroska.h"

#include "mp_msg.h"
#include "help_mp.h"

#include "vobsub.h"
#include "subreader.h"
#include "libvo/sub.h"

#include "ass_mp.h"

#include "libavutil/common.h"

#ifdef CONFIG_QTX_CODECS
#include "loader/qtx/qtxsdk/components.h"
#endif

#if CONFIG_ZLIB
#include <zlib.h>
#endif

#include "libavutil/lzo.h"
#include "ffmpeg_files/intreadwrite.h"
#include "libavutil/avstring.h"

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
static const int atrc_fl2bps[ATRC_FLAVORS] =
    { 8269, 11714, 13092, 16538, 18260, 22050, 33075, 44100 };
static const int cook_fl2bps[COOK_FLAVORS] = {
    1000, 1378, 2024, 2584, 4005, 5513, 8010, 4005, 750, 2498,
    4048, 5513, 8010, 11973, 8010, 2584, 4005, 2067, 2584, 2584,
    4005, 4005, 5513, 5513, 8010, 12059, 1550, 8010, 12059, 5513,
    12016, 16408, 22911, 33506
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

    char *codec_id;
    int ms_compat;
    char *language;

    int type;

    uint32_t v_width, v_height, v_dwidth, v_dheight;
    double v_frate;

    uint32_t a_formattag;
    uint32_t a_channels, a_bps;
    float a_sfreq;

    double default_duration;

    int default_track;

    unsigned char *private_data;
    unsigned int private_size;

    /* stuff for realmedia */
    int realmedia;
    int64_t rv_kf_base;
    int rv_kf_pts;
    double rv_pts;              /* previous video timestamp */
    double ra_pts;              /* previous audio timestamp */

  /** realaudio descrambling */
    int sub_packet_size;        ///< sub packet size, per stream
    int sub_packet_h;           ///< number of coded frames per block
    int coded_framesize;        ///< coded frame size, per stream
    int audiopk_size;           ///< audio packet size
    unsigned char *audio_buf;   ///< place to store reordered audio data
    double *audio_timestamp;    ///< timestamp for each audio packet
    int sub_packet_cnt;         ///< number of subpacket already received
    int audio_filepos;          ///< file position of first audio packet in block

    /* stuff for quicktime */
    int fix_i_bps;
    double qt_last_a_pts;

    int subtitle_type;

    /* The timecodes of video frames might have to be reordered if they're
       in display order (the timecodes, not the frames themselves!). In this
       case demux packets have to be cached with the help of these variables. */
    int reorder_timecodes;
    demux_packet_t **cached_dps;
    int num_cached_dps, num_allocated_dps;
    double max_pts;

    /* generic content encoding support */
    mkv_content_encoding_t *encodings;
    int num_encodings;

    /* For VobSubs and SSA/ASS */
    sh_sub_t *sh_sub;
} mkv_track_t;

typedef struct mkv_index {
    int tnum;
    uint64_t timecode, filepos;
} mkv_index_t;

typedef struct mkv_demuxer {
    off_t segment_start;

    double duration, last_pts;
    uint64_t last_filepos;

    mkv_track_t **tracks;
    int num_tracks;

    uint64_t tc_scale, cluster_tc;

    uint64_t cluster_start;
    uint64_t cluster_size;
    uint64_t blockgroup_size;

    mkv_index_t *indexes;
    int num_indexes;

    off_t *parsed_pos;
    int num_parsed_pos;
    bool parsed_info;
    bool parsed_tracks;
    bool parsed_tags;
    bool parsed_chapters;
    bool parsed_attachments;

    struct cluster_pos {
        uint64_t filepos;
        uint64_t timecode;
    } *cluster_positions;
    int num_cluster_pos;

    int64_t skip_to_timecode;
    int v_skip_to_keyframe, a_skip_to_keyframe;

    int last_aid;
    int audio_tracks[MAX_A_STREAMS];
} mkv_demuxer_t;

#define REALHEADER_SIZE    16
#define RVPROPERTIES_SIZE  34
#define RAPROPERTIES4_SIZE 56
#define RAPROPERTIES5_SIZE 70

/**
 * \brief ensures there is space for at least one additional element
 * \param array array to grow
 * \param nelem current number of elements in array
 * \param elsize size of one array element
 */
static void *grow_array(void *array, int nelem, size_t elsize)
{
    if (!(nelem & 31))
        array = realloc(array, (nelem + 32) * elsize);
    return array;
}

static bool is_parsed_header(struct mkv_demuxer *mkv_d, off_t pos)
{
    int low = 0;
    int high = mkv_d->num_parsed_pos;
    while (high > low + 1) {
        int mid = high + low >> 1;
        if (mkv_d->parsed_pos[mid] > pos)
            high = mid;
        else
            low = mid;
    }
    if (mkv_d->num_parsed_pos && mkv_d->parsed_pos[low] == pos)
        return true;
    if (!(mkv_d->num_parsed_pos & 31))
        mkv_d->parsed_pos = talloc_realloc(mkv_d, mkv_d->parsed_pos, off_t,
                                       mkv_d->num_parsed_pos + 32);
    mkv_d->num_parsed_pos++;
    for (int i = mkv_d->num_parsed_pos - 1; i > low; i--)
        mkv_d->parsed_pos[i] = mkv_d->parsed_pos[i - 1];
    mkv_d->parsed_pos[low] = pos;
    return false;
}

static mkv_track_t *demux_mkv_find_track_by_num(mkv_demuxer_t *d, int n,
                                                int type)
{
    int i, id;

    for (i = 0, id = 0; i < d->num_tracks; i++)
        if (d->tracks[i] != NULL && d->tracks[i]->type == type)
            if (id++ == n)
                return d->tracks[i];

    return NULL;
}

static void add_cluster_position(mkv_demuxer_t *mkv_d, uint64_t filepos,
                                 uint64_t timecode)
{
    if (mkv_d->indexes)
        return;

    int n = mkv_d->num_cluster_pos;
    if (n > 0 && mkv_d->cluster_positions[n-1].filepos >= filepos)
        return;

    mkv_d->cluster_positions =
        grow_array(mkv_d->cluster_positions, mkv_d->num_cluster_pos,
                   sizeof(*mkv_d->cluster_positions));
    mkv_d->cluster_positions[mkv_d->num_cluster_pos++] = (struct cluster_pos){
        .filepos = filepos,
        .timecode = timecode,
    };
}


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

/** \brief Free cached demux packets
 *
 * Reordering the timecodes requires caching of demux packets. This function
 * frees all these cached packets and the memory for the cached pointers
 * itself.
 *
 * \param demuxer The demuxer for which the cache is to be freed.
 */
static void free_cached_dps(demuxer_t *demuxer)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    mkv_track_t *track;
    int i, k;

    for (k = 0; k < mkv_d->num_tracks; k++) {
        track = mkv_d->tracks[k];
        for (i = 0; i < track->num_cached_dps; i++)
            free_demux_packet(track->cached_dps[i]);
        free(track->cached_dps);
        track->cached_dps = NULL;
        track->num_cached_dps = 0;
        track->num_allocated_dps = 0;
        track->max_pts = 0;
    }
}

static void demux_mkv_decode(mkv_track_t *track, uint8_t *src,
                             uint8_t **dest, uint32_t *size, uint32_t type)
{
    uint8_t *orig_src = src;

    *dest = src;

    for (int i = 0; i < track->num_encodings; i++) {
        struct mkv_content_encoding *enc = track->encodings + i;
        if (!(enc->scope & type))
            continue;

        if (src != *dest && src != orig_src)
            free(src);
        src = *dest;  // output from last iteration is new source

        if (enc->comp_algo == 0) {
#if CONFIG_ZLIB
            /* zlib encoded track */

            if (*size == 0)
                continue;

            z_stream zstream;

            zstream.zalloc = (alloc_func) 0;
            zstream.zfree = (free_func) 0;
            zstream.opaque = (voidpf) 0;
            if (inflateInit(&zstream) != Z_OK) {
                mp_tmsg(MSGT_DEMUX, MSGL_WARN,
                        "[mkv] zlib initialization failed.\n");
                goto error;
            }
            zstream.next_in = (Bytef *) src;
            zstream.avail_in = *size;

            *dest = NULL;
            zstream.avail_out = *size;
            int result;
            do {
                *size += 4000;
                *dest = realloc(*dest, *size);
                zstream.next_out = (Bytef *) (*dest + zstream.total_out);
                result = inflate(&zstream, Z_NO_FLUSH);
                if (result != Z_OK && result != Z_STREAM_END) {
                    mp_tmsg(MSGT_DEMUX, MSGL_WARN,
                            "[mkv] zlib decompression failed.\n");
                    free(*dest);
                    *dest = NULL;
                    inflateEnd(&zstream);
                    goto error;
                }
                zstream.avail_out += 4000;
            } while (zstream.avail_out == 4000 && zstream.avail_in != 0
                     && result != Z_STREAM_END);

            *size = zstream.total_out;
            inflateEnd(&zstream);
#endif
        } else if (enc->comp_algo == 2) {
            /* lzo encoded track */
            int dstlen = *size * 3;

            *dest = NULL;
            while (1) {
                int srclen = *size;
                if (dstlen > SIZE_MAX - AV_LZO_OUTPUT_PADDING)
                    goto lzo_fail;
                *dest = realloc(*dest, dstlen + AV_LZO_OUTPUT_PADDING);
                int result = av_lzo1x_decode(*dest, &dstlen, src, &srclen);
                if (result == 0)
                    break;
                if (!(result & AV_LZO_OUTPUT_FULL)) {
                  lzo_fail:
                    mp_tmsg(MSGT_DEMUX, MSGL_WARN,
                            "[mkv] lzo decompression failed.\n");
                    free(*dest);
                    *dest = NULL;
                    goto error;
                }
                mp_msg(MSGT_DEMUX, MSGL_DBG2,
                       "[mkv] lzo decompression buffer too small.\n");
                dstlen *= 2;
            }
            *size = dstlen;
        } else if (enc->comp_algo == 3) {
            *dest = malloc(*size + enc->comp_settings_len);
            memcpy(*dest, enc->comp_settings, enc->comp_settings_len);
            memcpy(*dest + enc->comp_settings_len, src, *size);
            *size += enc->comp_settings_len;
        }
    }

 error:
    if (src != *dest && src != orig_src)
        free(src);
}


static int demux_mkv_read_info(demuxer_t *demuxer)
{
    mkv_demuxer_t *mkv_d = demuxer->priv;
    stream_t *s = demuxer->stream;

    mkv_d->tc_scale = 1000000;
    mkv_d->duration = 0;

    struct ebml_info info = {};
    struct ebml_parse_ctx parse_ctx = {};
    if (ebml_read_element(s, &parse_ctx, &info, &ebml_info_desc) < 0)
        return 1;
    if (info.n_timecode_scale) {
        mkv_d->tc_scale = info.timecode_scale;
        mp_msg(MSGT_DEMUX, MSGL_V,
               "[mkv] | + timecode scale: %" PRIu64 "\n", mkv_d->tc_scale);
    }
    if (info.n_duration) {
        mkv_d->duration = info.duration * mkv_d->tc_scale / 1e9;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] | + duration: %.3fs\n",
               mkv_d->duration);
    }
    if (info.n_segment_uid) {
        int len = info.segment_uid.len;
        if (len != sizeof(demuxer->matroska_data.segment_uid)) {
            mp_msg(MSGT_DEMUX, MSGL_INFO,
                   "[mkv] segment uid invalid length %d\n", len);
        } else {
            memcpy(demuxer->matroska_data.segment_uid, info.segment_uid.start,
                   len);
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] | + segment uid");
            for (int i = 0; i < len; i++)
                mp_msg(MSGT_DEMUX, MSGL_V, " %02x",
                       demuxer->matroska_data.segment_uid[i]);
            mp_msg(MSGT_DEMUX, MSGL_V, "\n");
        }
    }
    talloc_free(parse_ctx.talloc_ctx);
    return 0;
}

static void parse_trackencodings(struct demuxer *demuxer,
                                 struct mkv_track *track,
                                 struct ebml_content_encodings *encodings)
{
    // initial allocation to be a non-NULL context before realloc
    mkv_content_encoding_t *ce = talloc_size(track, 1);

    for (int n_enc = 0; n_enc < encodings->n_content_encoding; n_enc++) {
        struct ebml_content_encoding *enc = encodings->content_encoding + n_enc;
        struct mkv_content_encoding e = {};
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
            mp_tmsg(MSGT_DEMUX, MSGL_WARN, "[mkv] Track "
                    "number %u has been encrypted and "
                    "decryption has not yet been\n"
                    "[mkv] implemented. Skipping track.\n",
                    track->tnum);
        } else if (e.type != 0) {
            mp_tmsg(MSGT_DEMUX, MSGL_WARN,
                    "[mkv] Unknown content encoding type for "
                    "track %u. Skipping track.\n",
                    track->tnum);
        } else if (e.comp_algo != 0 && e.comp_algo != 2 && e.comp_algo != 3) {
            mp_tmsg(MSGT_DEMUX, MSGL_WARN,
                    "[mkv] Track %u has been compressed with "
                    "an unknown/unsupported compression\n"
                    "[mkv] algorithm (%" PRIu64 "). Skipping track.\n",
                    track->tnum, e.comp_algo);
        }
#if !CONFIG_ZLIB
        else if (e.comp_algo == 0) {
            mp_tmsg(MSGT_DEMUX, MSGL_WARN,
                    "[mkv] Track %u was compressed with zlib "
                    "but mplayer has not been compiled\n"
                    "[mkv] with support for zlib compression. "
                    "Skipping track.\n",
                    track->tnum);
        }
#endif
        int i;
        for (i = 0; i < n_enc; i++)
            if (e.order >= ce[i].order)
                break;
        ce = talloc_realloc_size(track, ce, (n_enc + 1) * sizeof(*ce));
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
        mp_msg(MSGT_DEMUX, MSGL_V,
               "[mkv] |   + Sampling frequency: %f\n", track->a_sfreq);
    } else
        track->a_sfreq = 8000;
    if (audio->n_bit_depth) {
        track->a_bps = audio->bit_depth;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Bit depth: %u\n",
               track->a_bps);
    }
    if (audio->n_channels) {
        track->a_channels = audio->channels;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Channels: %u\n",
               track->a_channels);
    } else
        track->a_channels = 1;
}

static void parse_trackvideo(struct demuxer *demuxer, struct mkv_track *track,
                             struct ebml_video *video)
{
    if (video->n_frame_rate) {
        track->v_frate = video->frame_rate;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Frame rate: %f\n",
               track->v_frate);
        if (track->v_frate > 0)
            track->default_duration = 1 / track->v_frate;
    }
    if (video->n_display_width) {
        track->v_dwidth = video->display_width;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Display width: %u\n",
               track->v_dwidth);
    }
    if (video->n_display_height) {
        track->v_dheight = video->display_height;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Display height: %u\n",
               track->v_dheight);
    }
    if (video->n_pixel_width) {
        track->v_width = video->pixel_width;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Pixel width: %u\n",
               track->v_width);
    }
    if (video->n_pixel_height) {
        track->v_height = video->pixel_height;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Pixel height: %u\n",
               track->v_height);
    }
}

/**
 * \brief free any data associated with given track
 * \param track track of which to free data
 */
static void demux_mkv_free_trackentry(mkv_track_t *track)
{
    free(track->audio_buf);
    free(track->audio_timestamp);
    talloc_free(track);
}

static void parse_trackentry(struct demuxer *demuxer,
                             struct ebml_track_entry *entry)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    struct mkv_track *track = talloc_zero_size(NULL, sizeof(*track));

    track->tnum = entry->track_number;
    if (track->tnum)
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Track number: %u\n",
               track->tnum);
    else
        mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] Missing track number!\n");

    if (entry->n_name) {
        track->name = talloc_strndup(track, entry->name.start,
                                     entry->name.len);
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Name: %s\n",
               track->name);
    }

    track->type = entry->track_type;
    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Track type: ");
    switch (track->type) {
    case MATROSKA_TRACK_AUDIO:
        mp_msg(MSGT_DEMUX, MSGL_V, "Audio\n");
        break;
    case MATROSKA_TRACK_VIDEO:
        mp_msg(MSGT_DEMUX, MSGL_V, "Video\n");
        break;
    case MATROSKA_TRACK_SUBTITLE:
        mp_msg(MSGT_DEMUX, MSGL_V, "Subtitle\n");
        break;
    default:
        mp_msg(MSGT_DEMUX, MSGL_V, "unknown\n");
        break;
    }

    if (entry->n_audio) {
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Audio track\n");
        parse_trackaudio(demuxer, track, &entry->audio);
    }

    if (entry->n_video) {
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Video track\n");
        parse_trackvideo(demuxer, track, &entry->video);
    }

    if (entry->n_codec_id) {
        track->codec_id = talloc_strndup(track, entry->codec_id.start,
                                         entry->codec_id.len);
        if (!strcmp(track->codec_id, MKV_V_MSCOMP)
            || !strcmp(track->codec_id, MKV_A_ACM))
            track->ms_compat = 1;
        else if (!strcmp(track->codec_id, MKV_S_VOBSUB))
            track->subtitle_type = MATROSKA_SUBTYPE_VOBSUB;
        else if (!strcmp(track->codec_id, MKV_S_TEXTSSA)
                 || !strcmp(track->codec_id, MKV_S_TEXTASS)
                 || !strcmp(track->codec_id, MKV_S_SSA)
                 || !strcmp(track->codec_id, MKV_S_ASS)) {
            track->subtitle_type = MATROSKA_SUBTYPE_SSA;
        } else if (!strcmp(track->codec_id, MKV_S_TEXTASCII))
            track->subtitle_type = MATROSKA_SUBTYPE_TEXT;
        if (!strcmp(track->codec_id, MKV_S_TEXTUTF8)) {
            track->subtitle_type = MATROSKA_SUBTYPE_TEXT;
        }
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Codec ID: %s\n",
               track->codec_id);
    } else
        mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] Missing codec ID!\n");

    if (entry->n_codec_private) {
        int len = entry->codec_private.len;
        track->private_data = talloc_size(track, len + AV_LZO_INPUT_PADDING);
        memcpy(track->private_data, entry->codec_private.start, len);
        track->private_size = len;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + CodecPrivate, length %u\n",
               track->private_size);
    }

    if (entry->n_language) {
        track->language = talloc_strndup(track, entry->language.start,
                                         entry->language.len);
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Language: %s\n",
               track->language);
    } else
        track->language = talloc_strdup(track, "eng");

    if (entry->n_flag_default) {
        track->default_track = entry->flag_default;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Default flag: %u\n",
               track->default_track);
    } else
        track->default_track = 1;

    if (entry->n_default_duration) {
        track->default_duration = entry->default_duration / 1e9;
        if (entry->default_duration == 0)
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |  + Default duration: 0");
        else {
            if (!track->v_frate)
                track->v_frate = 1e9 / entry->default_duration;
            mp_msg(MSGT_DEMUX, MSGL_V,
                   "[mkv] |  + Default duration: %.3fms ( = %.3f fps)\n",
                   entry->default_duration / 1000000.0, track->v_frate);
        }
    }

    if (entry->n_content_encodings)
        parse_trackencodings(demuxer, track, &entry->content_encodings);

    mkv_d->tracks[mkv_d->num_tracks++] = track;
}

static int demux_mkv_read_tracks(demuxer_t *demuxer)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    stream_t *s = demuxer->stream;

    struct ebml_tracks tracks = {};
    struct ebml_parse_ctx parse_ctx = {};
    if (ebml_read_element(s, &parse_ctx, &tracks, &ebml_tracks_desc) < 0)
        return 1;

    mkv_d->tracks = talloc_size(mkv_d,
                                tracks.n_track_entry * sizeof(*mkv_d->tracks));
    for (int i = 0; i < tracks.n_track_entry; i++) {
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] | + a track...\n");
        parse_trackentry(demuxer, &tracks.track_entry[i]);
    }
    talloc_free(parse_ctx.talloc_ctx);
    return 0;
}

static int demux_mkv_read_cues(demuxer_t *demuxer)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    stream_t *s = demuxer->stream;

    if (index_mode == 0 || index_mode == 2) {
        ebml_read_skip(s, NULL);
        return 0;
    }

    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] /---- [ parsing cues ] -----------\n");
    struct ebml_cues cues = {};
    struct ebml_parse_ctx parse_ctx = {};
    if (ebml_read_element(s, &parse_ctx, &cues, &ebml_cues_desc) < 0)
        goto out;
    for (int i = 0; i < cues.n_cue_point; i++) {
        struct ebml_cue_point *cuepoint = &cues.cue_point[i];
        if (cuepoint->n_cue_time != 1 || !cuepoint->n_cue_track_positions) {
            mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] Malformed CuePoint element\n");
            continue;
        }
        uint64_t time = cuepoint->cue_time;
        for (int i = 0; i < cuepoint->n_cue_track_positions; i++) {
            struct ebml_cue_track_positions *trackpos =
                &cuepoint->cue_track_positions[i];
            uint64_t track = trackpos->cue_track;
            uint64_t pos = trackpos->cue_cluster_position;
            mkv_d->indexes =
                grow_array(mkv_d->indexes, mkv_d->num_indexes,
                           sizeof(mkv_index_t));
            mkv_d->indexes[mkv_d->num_indexes].tnum = track;
            mkv_d->indexes[mkv_d->num_indexes].timecode = time;
            mkv_d->indexes[mkv_d->num_indexes].filepos =
                mkv_d->segment_start + pos;
            mp_msg(MSGT_DEMUX, MSGL_DBG2,
                   "[mkv] |+ found cue point for track %" PRIu64
                   ": timecode %" PRIu64 ", filepos: %" PRIu64 "\n", track,
                   time, mkv_d->segment_start + pos);
            mkv_d->num_indexes++;
        }
    }

 out:
    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] \\---- [ parsing cues ] -----------\n");
    talloc_free(parse_ctx.talloc_ctx);
    return 0;
}

static int demux_mkv_read_chapters(struct demuxer *demuxer)
{
    struct MPOpts *opts = demuxer->opts;
    stream_t *s = demuxer->stream;

    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] /---- [ parsing chapters ] ---------\n");
    struct ebml_chapters file_chapters = {};
    struct ebml_parse_ctx parse_ctx = {};
    if (ebml_read_element(s, &parse_ctx, &file_chapters,
                          &ebml_chapters_desc) < 0)
        goto out;

    int selected_edition = 0;
    int num_editions = file_chapters.n_edition_entry;
    struct ebml_edition_entry *editions = file_chapters.edition_entry;
    if (opts->edition_id >= 0 && opts->edition_id < num_editions) {
        selected_edition = opts->edition_id;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] User-specified edition: %d\n",
               selected_edition);
    } else
        for (int i = 0; i < num_editions; i++)
            if (editions[i].edition_flag_default) {
                selected_edition = i;
                mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] Default edition: %d\n", i);
                break;
            }
    struct matroska_chapter *m_chapters = NULL;
    if (editions[selected_edition].edition_flag_ordered) {
        int count = editions[selected_edition].n_chapter_atom;
        m_chapters = talloc_array_ptrtype(demuxer, m_chapters, count);
        demuxer->matroska_data.ordered_chapters = m_chapters;
        demuxer->matroska_data.num_ordered_chapters = count;
    }

    for (int idx = 0; idx < num_editions; idx++) {
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] New edition %d\n", idx);
        int warn_level = idx == selected_edition ? MSGL_WARN : MSGL_V;
        if (editions[idx].n_edition_flag_default)
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] Default edition flag: %"PRIu64
                   "\n", editions[idx].edition_flag_default);
        if (editions[idx].n_edition_flag_ordered)
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] Ordered chapter flag: %"PRIu64
                   "\n", editions[idx].edition_flag_ordered);
        for (int i = 0; i < editions[idx].n_chapter_atom; i++) {
            struct ebml_chapter_atom *ca = editions[idx].chapter_atom + i;
            struct matroska_chapter chapter = { };
            struct bstr name = { "(unnamed)", 9 };

            if (!ca->n_chapter_time_start)
                mp_msg(MSGT_DEMUX, warn_level,
                       "[mkv] Chapter lacks start time\n");
            chapter.start = ca->chapter_time_start / 1000000;
            chapter.end = ca->chapter_time_end / 1000000;

            if (ca->n_chapter_display) {
                if (ca->n_chapter_display > 1)
                    mp_msg(MSGT_DEMUX, warn_level, "[mkv] Multiple chapter "
                           "names not supported, picking first\n");
                if (!ca->chapter_display[0].n_chap_string)
                    mp_msg(MSGT_DEMUX, warn_level, "[mkv] Malformed chapter "
                           "name entry\n");
                else
                    name = ca->chapter_display[0].chap_string;
            }

            if (ca->n_chapter_segment_uid) {
                chapter.has_segment_uid = true;
                int len = ca->chapter_segment_uid.len;
                if (len != sizeof(chapter.segment_uid))
                    mp_msg(MSGT_DEMUX, warn_level,
                           "[mkv] Chapter segment uid bad length %d\n", len);
                else if (ca->n_chapter_segment_edition_uid) {
                    mp_tmsg(MSGT_DEMUX, warn_level, "[mkv] Warning: "
                            "unsupported edition recursion in chapter; "
                            "will skip on playback!\n");
                } else {
                    memcpy(chapter.segment_uid, ca->chapter_segment_uid.start,
                           len);
                    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] Chapter segment uid ");
                    for (int i = 0; i < len; i++)
                        mp_msg(MSGT_DEMUX, MSGL_V, "%02x ",
                               chapter.segment_uid[i]);
                    mp_msg(MSGT_DEMUX, MSGL_V, "\n");
                }
            }

            mp_msg(MSGT_DEMUX, MSGL_V,
                   "[mkv] Chapter %u from %02d:%02d:%02d.%03d "
                   "to %02d:%02d:%02d.%03d, %.*s\n", idx,
                   (int) (chapter.start / 60 / 60 / 1000),
                   (int) ((chapter.start / 60 / 1000) % 60),
                   (int) ((chapter.start / 1000) % 60),
                   (int) (chapter.start % 1000),
                   (int) (chapter.end / 60 / 60 / 1000),
                   (int) ((chapter.end / 60 / 1000) % 60),
                   (int) ((chapter.end / 1000) % 60),
                   (int) (chapter.end % 1000),
                   name.len, name.start);

            if (idx == selected_edition){
                demuxer_add_chapter(demuxer, name.start, name.len,
                                    chapter.start, chapter.end);
                if (editions[idx].edition_flag_ordered) {
                    chapter.name = talloc_strndup(m_chapters, name.start,
                                                  name.len);
                    m_chapters[i] = chapter;
                }
            }
        }
    }
    if (num_editions > 1)
        mp_msg(MSGT_DEMUX, MSGL_INFO,
               "[mkv] Found %d editions, will play #%d (first is 0).\n",
               num_editions, selected_edition);

 out:
    talloc_free(parse_ctx.talloc_ctx);
    mp_msg(MSGT_DEMUX, MSGL_V,
           "[mkv] \\---- [ parsing chapters ] ---------\n");
    return 0;
}

static int demux_mkv_read_tags(demuxer_t *demuxer)
{
    ebml_read_skip(demuxer->stream, NULL);
    return 0;
}

static int demux_mkv_read_attachments(demuxer_t *demuxer)
{
    stream_t *s = demuxer->stream;

    mp_msg(MSGT_DEMUX, MSGL_V,
           "[mkv] /---- [ parsing attachments ] ---------\n");

    struct ebml_attachments attachments = {};
    struct ebml_parse_ctx parse_ctx = {};
    if (ebml_read_element(s, &parse_ctx, &attachments,
                          &ebml_attachments_desc) < 0)
        goto out;

    for (int i = 0; i < attachments.n_attached_file; i++) {
        struct ebml_attached_file *attachment = &attachments.attached_file[i];
        if (!attachment->n_file_name || !attachment->n_file_mime_type
            || !attachment->n_file_data) {
            mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] Malformed attachment\n");
            continue;
        }
        struct bstr name = attachment->file_name;
        struct bstr mime = attachment->file_mime_type;
        char *data = attachment->file_data.start;
        int data_size = attachment->file_data.len;
        demuxer_add_attachment(demuxer, name.start, name.len, mime.start,
                               mime.len, data, data_size);
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] Attachment: %.*s, %.*s, %u bytes\n",
               name.len, name.start, mime.len, mime.start, data_size);
    }

 out:
    talloc_free(parse_ctx.talloc_ctx);
    mp_msg(MSGT_DEMUX, MSGL_V,
           "[mkv] \\---- [ parsing attachments ] ---------\n");
    return 0;
}

static int read_header_element(struct demuxer *demuxer, uint32_t id,
                               off_t at_filepos);

static int demux_mkv_read_seekhead(demuxer_t *demuxer)
{
    struct mkv_demuxer *mkv_d = demuxer->priv;
    struct stream *s = demuxer->stream;
    int res = 0;
    struct ebml_seek_head seekhead = {};
    struct ebml_parse_ctx parse_ctx = {};

    mp_msg(MSGT_DEMUX, MSGL_V,
           "[mkv] /---- [ parsing seek head ] ---------\n");
    if (ebml_read_element(s, &parse_ctx, &seekhead, &ebml_seek_head_desc) < 0) {
        res = 1;
        goto out;
    }
    /* off now holds the position of the next element after the seek head. */
    off_t off = stream_tell(s);
    for (int i = 0; i < seekhead.n_seek; i++) {
        struct ebml_seek *seek = &seekhead.seek[i];
        if (seek->n_seek_id != 1 || seek->n_seek_position != 1) {
            mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] Invalid SeekHead entry\n");
            continue;
        }
        uint64_t pos = seek->seek_position + mkv_d->segment_start;
        if (pos >= demuxer->movi_end) {
            mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] SeekHead position beyond "
                   "end of file - incomplete file?\n");
            continue;
        }
        read_header_element(demuxer, seek->seek_id, pos);
    }
    if (!stream_seek(s, off)) {
        mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] Couldn't seek back after "
               "SeekHead??\n");
        res = 1;
    }
 out:
    mp_msg(MSGT_DEMUX, MSGL_V,
           "[mkv] \\---- [ parsing seek head ] ---------\n");
    talloc_free(parse_ctx.talloc_ctx);
    return res;
}

static bool seek_pos_id(struct stream *s, off_t pos, uint32_t id)
{
    if (!stream_seek(s, pos)) {
        mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] Failed to seek in file\n");
        return false;
    }
    if (ebml_read_id(s, NULL) != id) {
        mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] Expected element not found\n");
        return false;
    }
    return true;
}

static int read_header_element(struct demuxer *demuxer, uint32_t id,
                               off_t at_filepos)
{
    struct mkv_demuxer *mkv_d = demuxer->priv;
    stream_t *s = demuxer->stream;
    off_t pos = stream_tell(s) - 4;

    switch(id) {
    case MATROSKA_ID_INFO:
        if (mkv_d->parsed_info)
            break;
        if (at_filepos && !seek_pos_id(s, at_filepos, id))
            return -1;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |+ segment information...\n");
        mkv_d->parsed_info = true;
        return demux_mkv_read_info(demuxer) ? -1 : 1;

    case MATROSKA_ID_TRACKS:
        if (mkv_d->parsed_tracks)
            break;
        if (at_filepos && !seek_pos_id(s, at_filepos, id))
            return -1;
        mkv_d->parsed_tracks = true;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |+ segment tracks...\n");
        return demux_mkv_read_tracks(demuxer) ? -1 : 1;

    case MATROSKA_ID_CUES:
        if (is_parsed_header(mkv_d, pos))
            break;
        if (at_filepos && !seek_pos_id(s, at_filepos, id))
            return -1;
        return demux_mkv_read_cues(demuxer) ? -1 : 1;

    case MATROSKA_ID_TAGS:
        if (mkv_d->parsed_tags)
            break;
        if (at_filepos && !seek_pos_id(s, at_filepos, id))
            return -1;
        mkv_d->parsed_tags = true;
        return demux_mkv_read_tags(demuxer) ? -1 : 1;

    case MATROSKA_ID_SEEKHEAD:
        if (is_parsed_header(mkv_d, pos))
            break;
        if (at_filepos && !seek_pos_id(s, at_filepos, id))
            return -1;
        return demux_mkv_read_seekhead(demuxer) ? -1 : 1;

    case MATROSKA_ID_CHAPTERS:
        if (mkv_d->parsed_chapters)
            break;
        if (at_filepos && !seek_pos_id(s, at_filepos, id))
            return -1;
        mkv_d->parsed_chapters = true;
        return demux_mkv_read_chapters(demuxer) ? -1 : 1;

    case MATROSKA_ID_ATTACHMENTS:
        if (mkv_d->parsed_attachments)
            break;
        if (at_filepos && !seek_pos_id(s, at_filepos, id))
            return -1;
        mkv_d->parsed_attachments = true;
        return demux_mkv_read_attachments(demuxer) ? -1 : 1;

    default:
        if (!at_filepos)
            ebml_read_skip(s, NULL);
        return 0;
    }
    if (!at_filepos)
        ebml_read_skip(s, NULL);
    return 1;
}



static int demux_mkv_open_video(demuxer_t *demuxer, mkv_track_t *track,
                                int vid);
static int demux_mkv_open_audio(demuxer_t *demuxer, mkv_track_t *track,
                                int aid);
static int demux_mkv_open_sub(demuxer_t *demuxer, mkv_track_t *track,
                              int sid);

static void display_create_tracks(demuxer_t *demuxer)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    int i, vid = 0, aid = 0, sid = 0;

    for (i = 0; i < mkv_d->num_tracks; i++) {
        char *type = "unknown", str[32];
        *str = '\0';
        switch (mkv_d->tracks[i]->type) {
        case MATROSKA_TRACK_VIDEO:
            type = "video";
            demux_mkv_open_video(demuxer, mkv_d->tracks[i], vid);
            if (mkv_d->tracks[i]->name)
                mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VID_%d_NAME=%s\n", vid,
                       mkv_d->tracks[i]->name);
            sprintf(str, "-vid %u", vid++);
            break;
        case MATROSKA_TRACK_AUDIO:
            type = "audio";
            demux_mkv_open_audio(demuxer, mkv_d->tracks[i], aid);
            if (mkv_d->tracks[i]->name)
                mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AID_%d_NAME=%s\n", aid,
                       mkv_d->tracks[i]->name);
            mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AID_%d_LANG=%s\n", aid,
                   mkv_d->tracks[i]->language);
            sprintf(str, "-aid %u, -alang %.5s", aid++,
                    mkv_d->tracks[i]->language);
            break;
        case MATROSKA_TRACK_SUBTITLE:
            type = "subtitles";
            demux_mkv_open_sub(demuxer, mkv_d->tracks[i], sid);
            if (mkv_d->tracks[i]->name)
                mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_SID_%d_NAME=%s\n", sid,
                       mkv_d->tracks[i]->name);
            mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_SID_%d_LANG=%s\n", sid,
                   mkv_d->tracks[i]->language);
            sprintf(str, "-sid %u, -slang %.5s", sid++,
                    mkv_d->tracks[i]->language);
            break;
        }
        if (mkv_d->tracks[i]->name)
            mp_tmsg(MSGT_DEMUX, MSGL_INFO,
                    "[mkv] Track ID %u: %s (%s) \"%s\", %s\n",
                    mkv_d->tracks[i]->tnum, type, mkv_d->tracks[i]->codec_id,
                    mkv_d->tracks[i]->name, str);
        else
            mp_tmsg(MSGT_DEMUX, MSGL_INFO, "[mkv] Track ID %u: %s (%s), %s\n",
                    mkv_d->tracks[i]->tnum, type, mkv_d->tracks[i]->codec_id,
                    str);
    }
}

typedef struct {
    char *id;
    int fourcc;
    int extradata;
} videocodec_info_t;

static const videocodec_info_t vinfo[] = {
    {MKV_V_MPEG1,     mmioFOURCC('m', 'p', 'g', '1'), 0},
    {MKV_V_MPEG2,     mmioFOURCC('m', 'p', 'g', '2'), 0},
    {MKV_V_MPEG4_SP,  mmioFOURCC('m', 'p', '4', 'v'), 1},
    {MKV_V_MPEG4_ASP, mmioFOURCC('m', 'p', '4', 'v'), 1},
    {MKV_V_MPEG4_AP,  mmioFOURCC('m', 'p', '4', 'v'), 1},
    {MKV_V_MPEG4_AVC, mmioFOURCC('a', 'v', 'c', '1'), 1},
    {MKV_V_THEORA,    mmioFOURCC('t', 'h', 'e', 'o'), 1},
    {NULL, 0, 0}
};

static int demux_mkv_open_video(demuxer_t *demuxer, mkv_track_t *track,
                                int vid)
{
    struct MPOpts *opts = demuxer->opts;
    BITMAPINFOHEADER *bih;
    void *ImageDesc = NULL;
    sh_video_t *sh_v;

    if (track->ms_compat) {     /* MS compatibility mode */
        BITMAPINFOHEADER *src;

        if (track->private_data == NULL
            || track->private_size < sizeof(BITMAPINFOHEADER))
            return 1;

        src = (BITMAPINFOHEADER *) track->private_data;
        bih = calloc(1, track->private_size);
        bih->biSize = le2me_32(src->biSize);
        bih->biWidth = le2me_32(src->biWidth);
        bih->biHeight = le2me_32(src->biHeight);
        bih->biPlanes = le2me_16(src->biPlanes);
        bih->biBitCount = le2me_16(src->biBitCount);
        bih->biCompression = le2me_32(src->biCompression);
        bih->biSizeImage = le2me_32(src->biSizeImage);
        bih->biXPelsPerMeter = le2me_32(src->biXPelsPerMeter);
        bih->biYPelsPerMeter = le2me_32(src->biYPelsPerMeter);
        bih->biClrUsed = le2me_32(src->biClrUsed);
        bih->biClrImportant = le2me_32(src->biClrImportant);
        memcpy((char *) bih + sizeof(BITMAPINFOHEADER),
               (char *) src + sizeof(BITMAPINFOHEADER),
               track->private_size - sizeof(BITMAPINFOHEADER));

        if (track->v_width == 0)
            track->v_width = bih->biWidth;
        if (track->v_height == 0)
            track->v_height = bih->biHeight;
    } else {
        bih = calloc(1, sizeof(BITMAPINFOHEADER));
        bih->biSize = sizeof(BITMAPINFOHEADER);
        bih->biWidth = track->v_width;
        bih->biHeight = track->v_height;
        bih->biBitCount = 24;
        bih->biSizeImage = bih->biWidth * bih->biHeight * bih->biBitCount / 8;

        if (track->private_size >= RVPROPERTIES_SIZE
            && (!strcmp(track->codec_id, MKV_V_REALV10)
                || !strcmp(track->codec_id, MKV_V_REALV20)
                || !strcmp(track->codec_id, MKV_V_REALV30)
                || !strcmp(track->codec_id, MKV_V_REALV40))) {
            unsigned char *dst, *src;
            uint32_t type2;
            unsigned int cnt;

            src = (uint8_t *) track->private_data + RVPROPERTIES_SIZE;

            cnt = track->private_size - RVPROPERTIES_SIZE;
            bih = realloc(bih, sizeof(BITMAPINFOHEADER) + 8 + cnt);
            bih->biSize = 48 + cnt;
            bih->biPlanes = 1;
            type2 = AV_RB32(src - 4);
            if (type2 == 0x10003000 || type2 == 0x10003001)
                bih->biCompression = mmioFOURCC('R', 'V', '1', '3');
            else
                bih->biCompression =
                    mmioFOURCC('R', 'V', track->codec_id[9], '0');
            dst = (unsigned char *) (bih + 1);
            // copy type1 and type2 info from rv properties
            memcpy(dst, src - 8, 8);
            stream_read(demuxer->stream, dst + 8, cnt);
            track->realmedia = 1;

#ifdef CONFIG_QTX_CODECS
        } else if (track->private_size >= sizeof(ImageDescription)
                   && !strcmp(track->codec_id, MKV_V_QUICKTIME)) {
            ImageDescriptionPtr idesc;

            idesc = (ImageDescriptionPtr) track->private_data;
            idesc->idSize = be2me_32(idesc->idSize);
            idesc->cType = be2me_32(idesc->cType);
            idesc->version = be2me_16(idesc->version);
            idesc->revisionLevel = be2me_16(idesc->revisionLevel);
            idesc->vendor = be2me_32(idesc->vendor);
            idesc->temporalQuality = be2me_32(idesc->temporalQuality);
            idesc->spatialQuality = be2me_32(idesc->spatialQuality);
            idesc->width = be2me_16(idesc->width);
            idesc->height = be2me_16(idesc->height);
            idesc->hRes = be2me_32(idesc->hRes);
            idesc->vRes = be2me_32(idesc->vRes);
            idesc->dataSize = be2me_32(idesc->dataSize);
            idesc->frameCount = be2me_16(idesc->frameCount);
            idesc->depth = be2me_16(idesc->depth);
            idesc->clutID = be2me_16(idesc->clutID);
            bih->biPlanes = 1;
            bih->biCompression = idesc->cType;
            ImageDesc = idesc;
#endif                          /* CONFIG_QTX_CODECS */

        } else {
            const videocodec_info_t *vi = vinfo;
            while (vi->id && strcmp(vi->id, track->codec_id))
                vi++;
            bih->biCompression = vi->fourcc;
            if (vi->extradata && track->private_data
                && (track->private_size > 0)) {
                bih->biSize += track->private_size;
                bih = realloc(bih, bih->biSize);
                memcpy(bih + 1, track->private_data, track->private_size);
            }
            track->reorder_timecodes = opts->user_correct_pts == 0;
            if (!vi->id) {
                mp_tmsg(MSGT_DEMUX, MSGL_WARN, "[mkv] Unknown/unsupported "
                        "CodecID (%s) or missing/bad CodecPrivate\n"
                        "[mkv] data (track %u).\n",
                        track->codec_id, track->tnum);
                free(bih);
                return 1;
            }
        }
    }

    sh_v = new_sh_video_vid(demuxer, track->tnum, vid);
    sh_v->bih = bih;
    sh_v->format = sh_v->bih->biCompression;
    if (track->v_frate == 0.0)
        track->v_frate = 25.0;
    sh_v->fps = track->v_frate;
    sh_v->frametime = 1 / track->v_frate;
    sh_v->aspect = 0;
    if (!track->realmedia) {
        sh_v->disp_w = track->v_width;
        sh_v->disp_h = track->v_height;
        if (track->v_dheight)
            sh_v->aspect = (double) track->v_dwidth / track->v_dheight;
    } else {
        // vd_realvid.c will set aspect to disp_w/disp_h and rederive
        // disp_w and disp_h from the RealVideo stream contents returned
        // by the Real DLLs. If DisplayWidth/DisplayHeight was not set in
        // the Matroska file then it has already been set to PixelWidth/Height
        // by check_track_information.
        sh_v->disp_w = track->v_dwidth;
        sh_v->disp_h = track->v_dheight;
    }
    sh_v->ImageDesc = ImageDesc;
    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] Aspect: %f\n", sh_v->aspect);

    sh_v->ds = demuxer->video;
    return 0;
}

static int demux_mkv_open_audio(demuxer_t *demuxer, mkv_track_t *track,
                                int aid)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    sh_audio_t *sh_a = new_sh_audio_aid(demuxer, track->tnum, aid);
    if (!sh_a)
        return 1;
    mkv_d->audio_tracks[mkv_d->last_aid] = track->tnum;

    if (track->language && (strcmp(track->language, "und") != 0))
        sh_a->lang = strdup(track->language);
    sh_a->default_track = track->default_track;
    sh_a->ds = demuxer->audio;
    sh_a->wf = malloc(sizeof(WAVEFORMATEX));
    if (track->ms_compat && (track->private_size >= sizeof(WAVEFORMATEX))) {
        WAVEFORMATEX *wf = (WAVEFORMATEX *) track->private_data;
        sh_a->wf = realloc(sh_a->wf, track->private_size);
        sh_a->wf->wFormatTag = le2me_16(wf->wFormatTag);
        sh_a->wf->nChannels = le2me_16(wf->nChannels);
        sh_a->wf->nSamplesPerSec = le2me_32(wf->nSamplesPerSec);
        sh_a->wf->nAvgBytesPerSec = le2me_32(wf->nAvgBytesPerSec);
        sh_a->wf->nBlockAlign = le2me_16(wf->nBlockAlign);
        sh_a->wf->wBitsPerSample = le2me_16(wf->wBitsPerSample);
        sh_a->wf->cbSize = track->private_size - sizeof(WAVEFORMATEX);
        memcpy(sh_a->wf + 1, wf + 1,
               track->private_size - sizeof(WAVEFORMATEX));
        if (track->a_sfreq == 0.0)
            track->a_sfreq = sh_a->wf->nSamplesPerSec;
        if (track->a_channels == 0)
            track->a_channels = sh_a->wf->nChannels;
        if (track->a_bps == 0)
            track->a_bps = sh_a->wf->wBitsPerSample;
        track->a_formattag = sh_a->wf->wFormatTag;
    } else {
        memset(sh_a->wf, 0, sizeof(WAVEFORMATEX));
        if (!strcmp(track->codec_id, MKV_A_MP3)
            || !strcmp(track->codec_id, MKV_A_MP2))
            track->a_formattag = 0x0055;
        else if (!strncmp(track->codec_id, MKV_A_AC3, strlen(MKV_A_AC3)))
            track->a_formattag = 0x2000;
        else if (!strcmp(track->codec_id, MKV_A_DTS))
            track->a_formattag = 0x2001;
        else if (!strcmp(track->codec_id, MKV_A_PCM)
                 || !strcmp(track->codec_id, MKV_A_PCM_BE))
            track->a_formattag = 0x0001;
        else if (!strcmp(track->codec_id, MKV_A_AAC_2MAIN)
                 || !strncmp(track->codec_id, MKV_A_AAC_2LC,
                             strlen(MKV_A_AAC_2LC))
                 || !strcmp(track->codec_id, MKV_A_AAC_2SSR)
                 || !strcmp(track->codec_id, MKV_A_AAC_4MAIN)
                 || !strncmp(track->codec_id, MKV_A_AAC_4LC,
                             strlen(MKV_A_AAC_4LC))
                 || !strcmp(track->codec_id, MKV_A_AAC_4SSR)
                 || !strcmp(track->codec_id, MKV_A_AAC_4LTP)
                 || !strcmp(track->codec_id, MKV_A_AAC))
            track->a_formattag = mmioFOURCC('M', 'P', '4', 'A');
        else if (!strcmp(track->codec_id, MKV_A_VORBIS)) {
            if (track->private_data == NULL)
                return 1;
            track->a_formattag = mmioFOURCC('v', 'r', 'b', 's');
        } else if (!strcmp(track->codec_id, MKV_A_QDMC))
            track->a_formattag = mmioFOURCC('Q', 'D', 'M', 'C');
        else if (!strcmp(track->codec_id, MKV_A_QDMC2))
            track->a_formattag = mmioFOURCC('Q', 'D', 'M', '2');
        else if (!strcmp(track->codec_id, MKV_A_WAVPACK))
            track->a_formattag = mmioFOURCC('W', 'V', 'P', 'K');
        else if (!strcmp(track->codec_id, MKV_A_TRUEHD))
            track->a_formattag = mmioFOURCC('T', 'R', 'H', 'D');
        else if (!strcmp(track->codec_id, MKV_A_FLAC)) {
            if (track->private_data == NULL || track->private_size == 0) {
                mp_tmsg(MSGT_DEMUX, MSGL_WARN,
                        "[mkv] FLAC track does not contain valid headers.\n");
                return 1;
            }
            track->a_formattag = mmioFOURCC('f', 'L', 'a', 'C');
        } else if (track->private_size >= RAPROPERTIES4_SIZE) {
            if (!strcmp(track->codec_id, MKV_A_REAL28))
                track->a_formattag = mmioFOURCC('2', '8', '_', '8');
            else if (!strcmp(track->codec_id, MKV_A_REALATRC))
                track->a_formattag = mmioFOURCC('a', 't', 'r', 'c');
            else if (!strcmp(track->codec_id, MKV_A_REALCOOK))
                track->a_formattag = mmioFOURCC('c', 'o', 'o', 'k');
            else if (!strcmp(track->codec_id, MKV_A_REALDNET))
                track->a_formattag = mmioFOURCC('d', 'n', 'e', 't');
            else if (!strcmp(track->codec_id, MKV_A_REALSIPR))
                track->a_formattag = mmioFOURCC('s', 'i', 'p', 'r');
        } else {
            mp_tmsg(MSGT_DEMUX, MSGL_WARN, "[mkv] Unknown/unsupported audio "
                    "codec ID '%s' for track %u or missing/faulty\n[mkv] "
                    "private codec data.\n", track->codec_id, track->tnum);
            free_sh_audio(demuxer, track->tnum);
            return 1;
        }
    }

    sh_a->format = track->a_formattag;
    sh_a->wf->wFormatTag = track->a_formattag;
    sh_a->channels = track->a_channels;
    sh_a->wf->nChannels = track->a_channels;
    sh_a->samplerate = (uint32_t) track->a_sfreq;
    sh_a->wf->nSamplesPerSec = (uint32_t) track->a_sfreq;
    if (track->a_bps == 0) {
        sh_a->samplesize = 2;
        sh_a->wf->wBitsPerSample = 16;
    } else {
        sh_a->samplesize = track->a_bps / 8;
        sh_a->wf->wBitsPerSample = track->a_bps;
    }
    if (track->a_formattag == 0x0055) { /* MP3 || MP2 */
        sh_a->wf->nAvgBytesPerSec = 16000;
        sh_a->wf->nBlockAlign = 1152;
    } else if ((track->a_formattag == 0x2000)           /* AC3 */
               || (track->a_formattag == 0x2001)) {        /* DTS */
        free(sh_a->wf);
        sh_a->wf = NULL;
    } else if (track->a_formattag == 0x0001) {  /* PCM || PCM_BE */
        sh_a->wf->nAvgBytesPerSec = sh_a->channels * sh_a->samplerate * 2;
        sh_a->wf->nBlockAlign = sh_a->wf->nAvgBytesPerSec;
        if (!strcmp(track->codec_id, MKV_A_PCM_BE))
            sh_a->format = mmioFOURCC('t', 'w', 'o', 's');
    } else if (!strcmp(track->codec_id, MKV_A_QDMC)
               || !strcmp(track->codec_id, MKV_A_QDMC2)) {
        sh_a->wf->nAvgBytesPerSec = 16000;
        sh_a->wf->nBlockAlign = 1486;
        track->fix_i_bps = 1;
        track->qt_last_a_pts = 0.0;
        if (track->private_data != NULL) {
            sh_a->codecdata = malloc(track->private_size);
            memcpy(sh_a->codecdata, track->private_data, track->private_size);
            sh_a->codecdata_len = track->private_size;
        }
    } else if (track->a_formattag == mmioFOURCC('M', 'P', '4', 'A')) {
        int profile, srate_idx;

        sh_a->wf->nAvgBytesPerSec = 16000;
        sh_a->wf->nBlockAlign = 1024;

        if (!strcmp(track->codec_id, MKV_A_AAC)
            && (NULL != track->private_data)) {
            sh_a->codecdata = malloc(track->private_size);
            memcpy(sh_a->codecdata, track->private_data, track->private_size);
            sh_a->codecdata_len = track->private_size;
            return 0;
        }

        /* Recreate the 'private data' */
        /* which faad2 uses in its initialization */
        srate_idx = aac_get_sample_rate_index(sh_a->samplerate);
        if (!strncmp(&track->codec_id[12], "MAIN", 4))
            profile = 0;
        else if (!strncmp(&track->codec_id[12], "LC", 2))
            profile = 1;
        else if (!strncmp(&track->codec_id[12], "SSR", 3))
            profile = 2;
        else
            profile = 3;
        sh_a->codecdata = malloc(5);
        sh_a->codecdata[0] = ((profile + 1) << 3) | ((srate_idx & 0xE) >> 1);
        sh_a->codecdata[1] =
            ((srate_idx & 0x1) << 7) | (track->a_channels << 3);

        if (strstr(track->codec_id, "SBR") != NULL) {
            /* HE-AAC (aka SBR AAC) */
            sh_a->codecdata_len = 5;

            sh_a->samplerate *= 2;
            sh_a->wf->nSamplesPerSec *= 2;
            srate_idx = aac_get_sample_rate_index(sh_a->samplerate);
            sh_a->codecdata[2] = AAC_SYNC_EXTENSION_TYPE >> 3;
            sh_a->codecdata[3] = ((AAC_SYNC_EXTENSION_TYPE & 0x07) << 5) | 5;
            sh_a->codecdata[4] = (1 << 7) | (srate_idx << 3);
            track->default_duration = 1024.0 / (sh_a->samplerate / 2);
        } else {
            sh_a->codecdata_len = 2;
            track->default_duration = 1024.0 / sh_a->samplerate;
        }
    } else if (track->a_formattag == mmioFOURCC('v', 'r', 'b', 's')) {  /* VORBIS */
        sh_a->wf->cbSize = track->private_size;
        sh_a->wf = realloc(sh_a->wf, sizeof(WAVEFORMATEX) + sh_a->wf->cbSize);
        memcpy((unsigned char *) (sh_a->wf + 1), track->private_data,
               sh_a->wf->cbSize);
    } else if (track->private_size >= RAPROPERTIES4_SIZE
               && !strncmp(track->codec_id, MKV_A_REALATRC, 7)) {
        /* Common initialization for all RealAudio codecs */
        unsigned char *src = track->private_data;
        int codecdata_length, version;
        int flavor;

        sh_a->wf->nAvgBytesPerSec = 0;  /* FIXME !? */

        version = AV_RB16(src + 4);
        flavor = AV_RB16(src + 22);
        track->coded_framesize = AV_RB32(src + 24);
        track->sub_packet_h = AV_RB16(src + 40);
        sh_a->wf->nBlockAlign = track->audiopk_size = AV_RB16(src + 42);
        track->sub_packet_size = AV_RB16(src + 44);
        if (version == 4) {
            src += RAPROPERTIES4_SIZE;
            src += src[0] + 1;
            src += src[0] + 1;
        } else
            src += RAPROPERTIES5_SIZE;

        src += 3;
        if (version == 5)
            src++;
        codecdata_length = AV_RB32(src);
        src += 4;
        sh_a->wf->cbSize = codecdata_length;
        sh_a->wf = realloc(sh_a->wf, sizeof(WAVEFORMATEX) + sh_a->wf->cbSize);
        memcpy(((char *) (sh_a->wf + 1)), src, codecdata_length);

        switch (track->a_formattag) {
        case mmioFOURCC('a', 't', 'r', 'c'):
            sh_a->wf->nAvgBytesPerSec = atrc_fl2bps[flavor];
            sh_a->wf->nBlockAlign = track->sub_packet_size;
            track->audio_buf =
                malloc(track->sub_packet_h * track->audiopk_size);
            track->audio_timestamp =
                malloc(track->sub_packet_h * sizeof(double));
            break;
        case mmioFOURCC('c', 'o', 'o', 'k'):
            sh_a->wf->nAvgBytesPerSec = cook_fl2bps[flavor];
            sh_a->wf->nBlockAlign = track->sub_packet_size;
            track->audio_buf =
                malloc(track->sub_packet_h * track->audiopk_size);
            track->audio_timestamp =
                malloc(track->sub_packet_h * sizeof(double));
            break;
        case mmioFOURCC('s', 'i', 'p', 'r'):
            sh_a->wf->nAvgBytesPerSec = sipr_fl2bps[flavor];
            sh_a->wf->nBlockAlign = track->coded_framesize;
            track->audio_buf =
                malloc(track->sub_packet_h * track->audiopk_size);
            track->audio_timestamp =
                malloc(track->sub_packet_h * sizeof(double));
            break;
        case mmioFOURCC('2', '8', '_', '8'):
            sh_a->wf->nAvgBytesPerSec = 3600;
            sh_a->wf->nBlockAlign = track->coded_framesize;
            track->audio_buf =
                malloc(track->sub_packet_h * track->audiopk_size);
            track->audio_timestamp =
                malloc(track->sub_packet_h * sizeof(double));
            break;
        }

        track->realmedia = 1;
    } else if (!strcmp(track->codec_id, MKV_A_FLAC)
               || (track->a_formattag == 0xf1ac)) {
        unsigned char *ptr;
        int size;
        free(sh_a->wf);
        sh_a->wf = NULL;

        if (track->a_formattag == mmioFOURCC('f', 'L', 'a', 'C')) {
            ptr = track->private_data;
            size = track->private_size;
        } else {
            sh_a->format = mmioFOURCC('f', 'L', 'a', 'C');
            ptr = track->private_data + sizeof(WAVEFORMATEX);
            size = track->private_size - sizeof(WAVEFORMATEX);
        }
        if (size < 4 || ptr[0] != 'f' || ptr[1] != 'L' || ptr[2] != 'a'
            || ptr[3] != 'C') {
            sh_a->codecdata = malloc(4);
            sh_a->codecdata_len = 4;
            memcpy(sh_a->codecdata, "fLaC", 4);
        } else {
            sh_a->codecdata = malloc(size);
            sh_a->codecdata_len = size;
            memcpy(sh_a->codecdata, ptr, size);
        }
    } else if (track->a_formattag == mmioFOURCC('W', 'V', 'P', 'K') || track->a_formattag == mmioFOURCC('T', 'R', 'H', 'D')) {  /* do nothing, still works */
    } else if (!track->ms_compat
               || (track->private_size < sizeof(WAVEFORMATEX))) {
        free_sh_audio(demuxer, track->tnum);
        return 1;
    }

    return 0;
}

static int demux_mkv_open_sub(demuxer_t *demuxer, mkv_track_t *track,
                              int sid)
{
    if (track->subtitle_type != MATROSKA_SUBTYPE_UNKNOWN) {
        int size;
        uint8_t *buffer;
        sh_sub_t *sh = new_sh_sub_sid(demuxer, track->tnum, sid);
        track->sh_sub = sh;
        sh->type = 't';
        if (track->subtitle_type == MATROSKA_SUBTYPE_VOBSUB)
            sh->type = 'v';
        if (track->subtitle_type == MATROSKA_SUBTYPE_SSA)
            sh->type = 'a';
        size = track->private_size;
        demux_mkv_decode(track, track->private_data, &buffer, &size, 2);
        if (buffer && buffer != track->private_data) {
            free(track->private_data);
            track->private_data = buffer;
            track->private_size = size;
        }
        sh->extradata = malloc(track->private_size);
        memcpy(sh->extradata, track->private_data, track->private_size);
        sh->extradata_len = track->private_size;
        if (track->language && (strcmp(track->language, "und") != 0))
            sh->lang = strdup(track->language);
        sh->default_track = track->default_track;
    } else {
        mp_tmsg(MSGT_DEMUX, MSGL_ERR,
                "[mkv] Subtitle type '%s' is not supported.\n",
                track->codec_id);
        return 1;
    }

    return 0;
}

static int demux_mkv_open(demuxer_t *demuxer)
{
    stream_t *s = demuxer->stream;
    mkv_demuxer_t *mkv_d;
    mkv_track_t *track;
    int i, cont = 0;

    stream_seek(s, s->start_pos);
    if (ebml_read_id(s, NULL) != EBML_ID_EBML)
        return 0;
    struct ebml_ebml ebml_master = {};
    struct ebml_parse_ctx parse_ctx = { .no_error_messages = true };
    if (ebml_read_element(s, &parse_ctx, &ebml_master, &ebml_ebml_desc) < 0)
        return 0;
    if (ebml_master.doc_type.len != 8 || strncmp(ebml_master.doc_type.start,
                                                 "matroska", 8)) {
        mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] no head found\n");
        talloc_free(parse_ctx.talloc_ctx);
        return 0;
    }
    if (ebml_master.doc_type_read_version > 2) {
        mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] This looks like a Matroska file, "
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
            && ebml_master.ebml_max_id_length != 4)) {
        mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] This looks like a Matroska file, "
               "but the header has bad parameters\n");
        talloc_free(parse_ctx.talloc_ctx);
        return 0;
    }
    talloc_free(parse_ctx.talloc_ctx);

    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] Found the head...\n");

    if (ebml_read_id(s, NULL) != MATROSKA_ID_SEGMENT) {
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] but no segment :(\n");
        return 0;
    }
    ebml_read_length(s, NULL);  /* return bytes number until EOF */

    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] + a segment...\n");

    mkv_d = talloc_zero(demuxer, struct mkv_demuxer);
    demuxer->priv = mkv_d;
    mkv_d->tc_scale = 1000000;
    mkv_d->segment_start = stream_tell(s);

    while (!cont) {
        uint32_t id = ebml_read_id(s, NULL);
        switch (id) {
        case MATROSKA_ID_CLUSTER:
            mp_msg(MSGT_DEMUX, MSGL_V,
                   "[mkv] |+ found cluster, headers are "
                   "parsed completely :)\n");
            stream_seek(s, stream_tell(s) - 4);
            cont = 1;
            break;

        default:
            cont = read_header_element(demuxer, id, 0) < 1;
            break;
        case EBML_ID_VOID:
            ebml_read_skip(s, NULL);
            break;
        }
    }

    display_create_tracks(demuxer);

    /* select video track */
    track = NULL;
    if (demuxer->video->id == -1) {     /* automatically select a video track */
        /* search for a video track that has the 'default' flag set */
        for (i = 0; i < mkv_d->num_tracks; i++)
            if (mkv_d->tracks[i]->type == MATROSKA_TRACK_VIDEO
                && mkv_d->tracks[i]->default_track) {
                track = mkv_d->tracks[i];
                break;
            }

        if (track == NULL)
            /* no track has the 'default' flag set */
            /* let's take the first video track */
            for (i = 0; i < mkv_d->num_tracks; i++)
                if (mkv_d->tracks[i]->type == MATROSKA_TRACK_VIDEO) {
                    track = mkv_d->tracks[i];
                    break;
                }
    } else if (demuxer->video->id != -2)        /* -2 = no video at all */
        track =
            demux_mkv_find_track_by_num(mkv_d, demuxer->video->id,
                                        MATROSKA_TRACK_VIDEO);

    if (track && demuxer->v_streams[track->tnum]) {
        mp_tmsg(MSGT_DEMUX, MSGL_INFO, "[mkv] Will play video track %u.\n",
                track->tnum);
        demuxer->video->id = track->tnum;
        demuxer->video->sh = demuxer->v_streams[track->tnum];
    } else {
        mp_tmsg(MSGT_DEMUX, MSGL_INFO, "[mkv] No video track found/wanted.\n");
        demuxer->video->id = -2;
    }

    /* select audio track */
    track = NULL;
    if (track == NULL)
        /* search for an audio track that has the 'default' flag set */
        for (i = 0; i < mkv_d->num_tracks; i++)
            if (mkv_d->tracks[i]->type == MATROSKA_TRACK_AUDIO
                && mkv_d->tracks[i]->default_track) {
                track = mkv_d->tracks[i];
                break;
            }

    if (track == NULL)
        /* no track has the 'default' flag set */
        /* let's take the first audio track */
        for (i = 0; i < mkv_d->num_tracks; i++)
            if (mkv_d->tracks[i]->type == MATROSKA_TRACK_AUDIO) {
                track = mkv_d->tracks[i];
                break;
            }

    if (track && demuxer->a_streams[track->tnum]) {
        demuxer->audio->id = track->tnum;
        demuxer->audio->sh = demuxer->a_streams[track->tnum];
    } else {
        mp_tmsg(MSGT_DEMUX, MSGL_INFO, "[mkv] No audio track found/wanted.\n");
        demuxer->audio->id = -2;
    }


    if (demuxer->audio->id != -2)
        for (i = 0; i < mkv_d->num_tracks; i++) {
            if (mkv_d->tracks[i]->type != MATROSKA_TRACK_AUDIO)
                continue;
            if (demuxer->a_streams[track->tnum]) {
                mkv_d->last_aid++;
                if (mkv_d->last_aid == MAX_A_STREAMS)
                    break;
            }
        }

    if (s->end_pos == 0 || (mkv_d->indexes == NULL && index_mode < 0))
        demuxer->seekable = 0;
    else {
        demuxer->movi_start = s->start_pos;
        demuxer->movi_end = s->end_pos;
        demuxer->seekable = 1;
    }

    demuxer->accurate_seek = true;

    return DEMUXER_TYPE_MATROSKA;
}

static void demux_close_mkv(demuxer_t *demuxer)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;

    if (mkv_d) {
        int i;
        free_cached_dps(demuxer);
        if (mkv_d->tracks) {
            for (i = 0; i < mkv_d->num_tracks; i++)
                demux_mkv_free_trackentry(mkv_d->tracks[i]);
        }
        free(mkv_d->indexes);
        free(mkv_d->cluster_positions);
    }
}

static int demux_mkv_read_block_lacing(uint8_t *buffer, uint64_t *size,
                                       uint8_t *laces,
                                       uint32_t **all_lace_sizes)
{
    uint32_t total = 0, *lace_size;
    uint8_t flags;
    int i;

    *all_lace_sizes = NULL;
    lace_size = NULL;
    /* lacing flags */
    flags = *buffer++;
    (*size)--;

    switch ((flags & 0x06) >> 1) {
    case 0:                    /* no lacing */
        *laces = 1;
        lace_size = calloc(*laces, sizeof(uint32_t));
        lace_size[0] = *size;
        break;

    case 1:                    /* xiph lacing */
    case 2:                    /* fixed-size lacing */
    case 3:                    /* EBML lacing */
        *laces = *buffer++;
        (*size)--;
        (*laces)++;
        lace_size = calloc(*laces, sizeof(uint32_t));

        switch ((flags & 0x06) >> 1) {
        case 1:                /* xiph lacing */
            for (i = 0; i < *laces - 1; i++) {
                lace_size[i] = 0;
                do {
                    lace_size[i] += *buffer;
                    (*size)--;
                } while (*buffer++ == 0xFF);
                total += lace_size[i];
            }
            lace_size[i] = *size - total;
            break;

        case 2:                /* fixed-size lacing */
            for (i = 0; i < *laces; i++)
                lace_size[i] = *size / *laces;
            break;

        case 3:;                /* EBML lacing */
            int l;
            uint64_t num = ebml_read_vlen_uint(buffer, &l);
            if (num == EBML_UINT_INVALID) {
                free(lace_size);
                return 1;
            }
            buffer += l;
            *size -= l;

            total = lace_size[0] = num;
            for (i = 1; i < *laces - 1; i++) {
                int64_t snum;
                snum = ebml_read_vlen_int(buffer, &l);
                if (snum == EBML_INT_INVALID) {
                    free(lace_size);
                    return 1;
                }
                buffer += l;
                *size -= l;
                lace_size[i] = lace_size[i - 1] + snum;
                total += lace_size[i];
            }
            lace_size[i] = *size - total;
            break;
        }
        break;
    }
    *all_lace_sizes = lace_size;
    return 0;
}

static void handle_subtitles(demuxer_t *demuxer, mkv_track_t *track,
                             char *block, int64_t size,
                             uint64_t block_duration, uint64_t timecode)
{
    demux_packet_t *dp;

    if (block_duration == 0) {
        mp_msg(MSGT_DEMUX, MSGL_WARN,
               "[mkv] Warning: No BlockDuration for subtitle track found.\n");
        return;
    }

    sub_utf8 = 1;
    dp = new_demux_packet(size);
    memcpy(dp->buffer, block, size);
    dp->pts = timecode / 1000.0;
    dp->endpts = (timecode + block_duration) / 1000.0;
    ds_add_packet(demuxer->sub, dp);
}

double real_fix_timestamp(unsigned char *buf, unsigned int timestamp,
                          unsigned int format, int64_t *kf_base, int *kf_pts,
                          double *pts);

static void handle_realvideo(demuxer_t *demuxer, mkv_track_t *track,
                             uint8_t *buffer, uint32_t size, int block_bref)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    demux_packet_t *dp;
    uint32_t timestamp = mkv_d->last_pts * 1000;

    dp = new_demux_packet(size);
    memcpy(dp->buffer, buffer, size);

    if (mkv_d->v_skip_to_keyframe) {
        dp->pts = mkv_d->last_pts;
        track->rv_kf_base = 0;
        track->rv_kf_pts = timestamp;
    } else
        dp->pts =
            real_fix_timestamp(dp->buffer, timestamp,
                               ((sh_video_t *) demuxer->video->sh)->bih->
                               biCompression, &track->rv_kf_base,
                               &track->rv_kf_pts, NULL);
    dp->pos = demuxer->filepos;
    dp->flags = block_bref ? 0 : 0x10;

    ds_add_packet(demuxer->video, dp);
}

static void handle_realaudio(demuxer_t *demuxer, mkv_track_t *track,
                             uint8_t *buffer, uint32_t size, int block_bref)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    int sps = track->sub_packet_size;
    int sph = track->sub_packet_h;
    int cfs = track->coded_framesize;
    int w = track->audiopk_size;
    int spc = track->sub_packet_cnt;
    demux_packet_t *dp;
    int x;

    if ((track->a_formattag == mmioFOURCC('2', '8', '_', '8'))
        || (track->a_formattag == mmioFOURCC('c', 'o', 'o', 'k'))
        || (track->a_formattag == mmioFOURCC('a', 't', 'r', 'c'))
        || (track->a_formattag == mmioFOURCC('s', 'i', 'p', 'r'))) {
//      if(!block_bref)
//        spc = track->sub_packet_cnt = 0;
        switch (track->a_formattag) {
        case mmioFOURCC('2', '8', '_', '8'):
            for (x = 0; x < sph / 2; x++)
                memcpy(track->audio_buf + x * 2 * w + spc * cfs,
                       buffer + cfs * x, cfs);
            break;
        case mmioFOURCC('c', 'o', 'o', 'k'):
        case mmioFOURCC('a', 't', 'r', 'c'):
            for (x = 0; x < w / sps; x++)
                memcpy(track->audio_buf +
                       sps * (sph * x + ((sph + 1) / 2) * (spc & 1) +
                              (spc >> 1)), buffer + sps * x, sps);
            break;
        case mmioFOURCC('s', 'i', 'p', 'r'):
            memcpy(track->audio_buf + spc * w, buffer, w);
            if (spc == sph - 1) {
                int n;
                int bs = sph * w * 2 / 96;      // nibbles per subpacket
                // Perform reordering
                for (n = 0; n < 38; n++) {
                    int j;
                    int i = bs * sipr_swaps[n][0];
                    int o = bs * sipr_swaps[n][1];
                    // swap nibbles of block 'i' with 'o'      TODO: optimize
                    for (j = 0; j < bs; j++) {
                        int x = (i & 1) ?
                            (track->audio_buf[i >> 1] >> 4) :
                            (track->audio_buf[i >> 1] & 0x0F);
                        int y = (o & 1) ?
                            (track->audio_buf[o >> 1] >> 4) :
                            (track->audio_buf[o >> 1] & 0x0F);
                        if (o & 1)
                            track->audio_buf[o >> 1] =
                                (track->audio_buf[o >> 1] & 0x0F) | (x << 4);
                        else
                            track->audio_buf[o >> 1] =
                                (track->audio_buf[o >> 1] & 0xF0) | x;
                        if (i & 1)
                            track->audio_buf[i >> 1] =
                                (track->audio_buf[i >> 1] & 0x0F) | (y << 4);
                        else
                            track->audio_buf[i >> 1] =
                                (track->audio_buf[i >> 1] & 0xF0) | y;
                        ++i;
                        ++o;
                    }
                }
            }
            break;
        }
        track->audio_timestamp[track->sub_packet_cnt] =
            (track->ra_pts == mkv_d->last_pts) ? 0 : (mkv_d->last_pts);
        track->ra_pts = mkv_d->last_pts;
        if (track->sub_packet_cnt == 0)
            track->audio_filepos = demuxer->filepos;
        if (++(track->sub_packet_cnt) == sph) {
            int apk_usize =
                ((WAVEFORMATEX *) ((sh_audio_t *) demuxer->audio->sh)->wf)->
                nBlockAlign;
            track->sub_packet_cnt = 0;
            // Release all the audio packets
            for (x = 0; x < sph * w / apk_usize; x++) {
                dp = new_demux_packet(apk_usize);
                memcpy(dp->buffer, track->audio_buf + x * apk_usize,
                       apk_usize);
                /* Put timestamp only on packets that correspond to original
                 * audio packets in file */
                dp->pts = (x * apk_usize % w) ? 0 :
                    track->audio_timestamp[x * apk_usize / w];
                dp->pos = track->audio_filepos; // all equal
                dp->flags = x ? 0 : 0x10;       // Mark first packet as keyframe
                ds_add_packet(demuxer->audio, dp);
            }
        }
    } else {                    // Not a codec that require reordering
        dp = new_demux_packet(size);
        memcpy(dp->buffer, buffer, size);
        if (track->ra_pts == mkv_d->last_pts && !mkv_d->a_skip_to_keyframe)
            dp->pts = 0;
        else
            dp->pts = mkv_d->last_pts;
        track->ra_pts = mkv_d->last_pts;

        dp->pos = demuxer->filepos;
        dp->flags = block_bref ? 0 : 0x10;
        ds_add_packet(demuxer->audio, dp);
    }
}

/** Reorder timecodes and add cached demux packets to the queues.
 *
 * Timecode reordering is needed if a video track contains B frames that
 * are timestamped in display order (e.g. MPEG-1, MPEG-2 or "native" MPEG-4).
 * MPlayer doesn't like timestamps in display order. This function adjusts
 * the timestamp of cached frames (which are exactly one I/P frame followed
 * by one or more B frames) so that they are in coding order again.
 *
 * Example: The track with 25 FPS contains four frames with the timecodes
 * I at 0ms, P at 120ms, B at 40ms and B at 80ms. As soon as the next I
 * or P frame arrives these timecodes can be changed to I at 0ms, P at 40ms,
 * B at 80ms and B at 120ms.
 *
 * This works for simple H.264 B-frame pyramids, but not for arbitrary orders.
 *
 * \param demuxer The Matroska demuxer struct for this instance.
 * \param track The track structure whose cache should be handled.
 */
static void flush_cached_dps(demuxer_t *demuxer, mkv_track_t *track)
{
    int i, ok;

    if (track->num_cached_dps == 0)
        return;

    do {
        ok = 1;
        for (i = 1; i < track->num_cached_dps; i++)
            if (track->cached_dps[i - 1]->pts > track->cached_dps[i]->pts) {
                double tmp_pts = track->cached_dps[i - 1]->pts;
                track->cached_dps[i - 1]->pts = track->cached_dps[i]->pts;
                track->cached_dps[i]->pts = tmp_pts;
                ok = 0;
            }
    } while (!ok);

    for (i = 0; i < track->num_cached_dps; i++)
        ds_add_packet(demuxer->video, track->cached_dps[i]);
    track->num_cached_dps = 0;
}

/** Cache video frames if timecodes have to be reordered.
 *
 * Timecode reordering is needed if a video track contains B frames that
 * are timestamped in display order (e.g. MPEG-1, MPEG-2 or "native" MPEG-4).
 * This function takes in a Matroska block read from the file, allocates a
 * demux packet for it, fills in its values, allocates space for storing
 * pointers to the cached demux packets and adds the packet to it. If
 * the packet contains an I or a P frame then ::flush_cached_dps is called
 * in order to send the old cached frames downstream.
 *
 * \param demuxer The Matroska demuxer struct for this instance.
 * \param track The packet is meant for this track.
 * \param buffer The actual frame contents.
 * \param size The frame size in bytes.
 * \param block_bref A relative timecode (backward reference). If it is \c 0
 *   then the frame is an I frame.
 * \param block_fref A relative timecode (forward reference). If it is \c 0
 *   then the frame is either an I frame or a P frame depending on the value
 *   of \a block_bref. Otherwise it's a B frame.
 */
static void handle_video_bframes(demuxer_t *demuxer, mkv_track_t *track,
                                 uint8_t *buffer, uint32_t size,
                                 int block_bref, int block_fref)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    demux_packet_t *dp;

    dp = new_demux_packet(size);
    memcpy(dp->buffer, buffer, size);
    dp->pos = demuxer->filepos;
    dp->pts = mkv_d->last_pts;
    if ((track->num_cached_dps > 0) && (dp->pts < track->max_pts))
        block_fref = 1;
    if (block_fref == 0)        /* I or P frame */
        flush_cached_dps(demuxer, track);
    if (block_bref != 0)        /* I frame, don't cache it */
        dp->flags = 0x10;
    if ((track->num_cached_dps + 1) > track->num_allocated_dps) {
        track->cached_dps = (demux_packet_t **)
            realloc(track->cached_dps,
                    (track->num_cached_dps + 10) * sizeof(demux_packet_t *));
        track->num_allocated_dps += 10;
    }
    track->cached_dps[track->num_cached_dps] = dp;
    track->num_cached_dps++;
    if (dp->pts > track->max_pts)
        track->max_pts = dp->pts;
}

static int handle_block(demuxer_t *demuxer, uint8_t *block, uint64_t length,
                        uint64_t block_duration, int64_t block_bref,
                        int64_t block_fref, uint8_t simpleblock)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    mkv_track_t *track = NULL;
    demux_stream_t *ds = NULL;
    uint64_t old_length;
    int64_t tc;
    uint32_t *lace_size;
    uint8_t laces, flags;
    int i, num, tmp, use_this_block = 1;
    double current_pts;
    int16_t time;

    /* first byte(s): track num */
    num = ebml_read_vlen_uint(block, &tmp);
    block += tmp;
    /* time (relative to cluster time) */
    time = block[0] << 8 | block[1];
    block += 2;
    length -= tmp + 2;
    old_length = length;
    flags = block[0];
    if (demux_mkv_read_block_lacing(block, &length, &laces, &lace_size))
        return 0;
    block += old_length - length;

    tc = (time * mkv_d->tc_scale + mkv_d->cluster_tc) / 1000000.0 + 0.5;
    if (tc < 0)
        tc = 0;
    current_pts = tc / 1000.0;

    for (i = 0; i < mkv_d->num_tracks; i++)
        if (mkv_d->tracks[i]->tnum == num) {
            track = mkv_d->tracks[i];
            break;
        }
    if (track == NULL) {
        free(lace_size);
        return 1;
    }
    if (num == demuxer->audio->id) {
        ds = demuxer->audio;

        if (mkv_d->a_skip_to_keyframe) {
            if (simpleblock) {
                if (!(flags & 0x80))    /*current frame isn't a keyframe */
                    use_this_block = 0;
            } else if (block_bref != 0)
                use_this_block = 0;
        } else if (mkv_d->v_skip_to_keyframe)
            use_this_block = 0;

        if (track->fix_i_bps && use_this_block) {
            sh_audio_t *sh = (sh_audio_t *) ds->sh;

            if (block_duration != 0) {
                sh->i_bps = length * 1000 / block_duration;
                track->fix_i_bps = 0;
            } else if (track->qt_last_a_pts == 0.0)
                track->qt_last_a_pts = current_pts;
            else if (track->qt_last_a_pts != current_pts) {
                sh->i_bps = length / (current_pts - track->qt_last_a_pts);
                track->fix_i_bps = 0;
            }
        }
    } else if (tc < mkv_d->skip_to_timecode)
        use_this_block = 0;
    else if (num == demuxer->video->id) {
        ds = demuxer->video;
        if (mkv_d->v_skip_to_keyframe) {
            if (simpleblock) {
                if (!(flags & 0x80))    /*current frame isn't a keyframe */
                    use_this_block = 0;
            } else if (block_bref != 0 || block_fref != 0)
                use_this_block = 0;
        }
    } else if (num == demuxer->sub->id) {
        ds = demuxer->sub;
        if (track->subtitle_type != MATROSKA_SUBTYPE_VOBSUB) {
            uint8_t *buffer;
            int size = length;
            demux_mkv_decode(track, block, &buffer, &size, 1);
            handle_subtitles(demuxer, track, buffer, size, block_duration, tc);
            if (buffer != block)
                free(buffer);
            use_this_block = 0;
        }
    } else
        use_this_block = 0;

    if (use_this_block) {
        mkv_d->last_pts = current_pts;
        mkv_d->last_filepos = demuxer->filepos;

        for (i = 0; i < laces; i++) {
            if (ds == demuxer->video && track->realmedia)
                handle_realvideo(demuxer, track, block, lace_size[i],
                                 block_bref);
            else if (ds == demuxer->audio && track->realmedia)
                handle_realaudio(demuxer, track, block, lace_size[i],
                                 block_bref);
            else if (ds == demuxer->video && track->reorder_timecodes)
                handle_video_bframes(demuxer, track, block, lace_size[i],
                                     block_bref, block_fref);
            else {
                int size = lace_size[i];
                demux_packet_t *dp;
                uint8_t *buffer;
                demux_mkv_decode(track, block, &buffer, &size, 1);
                if (buffer) {
                    dp = new_demux_packet(size);
                    memcpy(dp->buffer, buffer, size);
                    if (buffer != block)
                        free(buffer);
                    dp->flags = (block_bref == 0
                                 && block_fref == 0) ? 0x10 : 0;
                    /* If default_duration is 0, assume no pts value is known
                     * for packets after the first one (rather than all pts
                     * values being the same) */
                    if (i == 0 || track->default_duration)
                        dp->pts =
                            mkv_d->last_pts + i * track->default_duration;
                    ds_add_packet(ds, dp);
                }
            }
            block += lace_size[i];
        }

        if (ds == demuxer->video) {
            mkv_d->v_skip_to_keyframe = 0;
            mkv_d->skip_to_timecode = 0;
        } else if (ds == demuxer->audio)
            mkv_d->a_skip_to_keyframe = 0;

        free(lace_size);
        return 1;
    }

    free(lace_size);
    return 0;
}

static int demux_mkv_fill_buffer(demuxer_t *demuxer, demux_stream_t *ds)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    stream_t *s = demuxer->stream;
    uint64_t l;
    int il, tmp;

    while (1) {
        while (mkv_d->cluster_size > 0) {
            uint64_t block_duration = 0, block_length = 0;
            int64_t block_bref = 0, block_fref = 0;
            uint8_t *block = NULL;

            while (mkv_d->blockgroup_size > 0) {
                switch (ebml_read_id(s, &il)) {
                case MATROSKA_ID_BLOCKDURATION:
                    block_duration = ebml_read_uint(s, &l);
                    if (block_duration == EBML_UINT_INVALID) {
                        free(block);
                        return 0;
                    }
                    block_duration =
                        block_duration * mkv_d->tc_scale / 1000000.0 + 0.5;
                    break;

                case MATROSKA_ID_BLOCK:
                    block_length = ebml_read_length(s, &tmp);
                    free(block);
                    if (block_length > SIZE_MAX - AV_LZO_INPUT_PADDING)
                        return 0;
                    block = malloc(block_length + AV_LZO_INPUT_PADDING);
                    demuxer->filepos = stream_tell(s);
                    if (stream_read(s, block, block_length) !=
                        (int) block_length) {
                        free(block);
                        return 0;
                    }
                    l = tmp + block_length;
                    break;

                case MATROSKA_ID_REFERENCEBLOCK:;
                    int64_t num = ebml_read_int(s, &l);
                    if (num == EBML_INT_INVALID) {
                        free(block);
                        return 0;
                    }
                    if (num <= 0)
                        block_bref = num;
                    else
                        block_fref = num;
                    break;

                case EBML_ID_INVALID:
                    free(block);
                    return 0;

                default:
                    ebml_read_skip(s, &l);
                    break;
                }
                mkv_d->blockgroup_size -= l + il;
                mkv_d->cluster_size -= l + il;
            }

            if (block) {
                int res = handle_block(demuxer, block, block_length,
                                       block_duration, block_bref, block_fref,
                                       0);
                free(block);
                if (res < 0)
                    return 0;
                if (res)
                    return 1;
            }

            if (mkv_d->cluster_size > 0) {
                switch (ebml_read_id(s, &il)) {
                case MATROSKA_ID_TIMECODE:;
                    uint64_t num = ebml_read_uint(s, &l);
                    if (num == EBML_UINT_INVALID)
                        return 0;
                    mkv_d->cluster_tc = num * mkv_d->tc_scale;
                    add_cluster_position(mkv_d, mkv_d->cluster_start,
                                         mkv_d->cluster_tc);
                    break;

                case MATROSKA_ID_BLOCKGROUP:
                    mkv_d->blockgroup_size = ebml_read_length(s, &tmp);
                    l = tmp;
                    break;

                case MATROSKA_ID_SIMPLEBLOCK:;
                    int res;
                    block_length = ebml_read_length(s, &tmp);
                    block = malloc(block_length);
                    demuxer->filepos = stream_tell(s);
                    if (stream_read(s, block, block_length) !=
                        (int) block_length) {
                        free(block);
                        return 0;
                    }
                    l = tmp + block_length;
                    res = handle_block(demuxer, block, block_length,
                                       block_duration, block_bref,
                                       block_fref, 1);
                    free(block);
                    mkv_d->cluster_size -= l + il;
                    if (res < 0)
                        return 0;
                    else if (res)
                        return 1;
                    else
                        mkv_d->cluster_size += l + il;
                    break;

                case EBML_ID_INVALID:
                    return 0;

                default:
                    ebml_read_skip(s, &l);
                    break;
                }
                mkv_d->cluster_size -= l + il;
            }
        }

        while (ebml_read_id(s, &il) != MATROSKA_ID_CLUSTER) {
            ebml_read_skip(s, NULL);
            if (s->eof)
                return 0;
        }
        mkv_d->cluster_start = stream_tell(s) - il;
        mkv_d->cluster_size = ebml_read_length(s, NULL);
    }

    return 0;
}

static void demux_mkv_seek(demuxer_t *demuxer, float rel_seek_secs,
                           float audio_delay, int flags)
{
    if (!(flags & (SEEK_BACKWARD | SEEK_FORWARD))) {
        if (flags & SEEK_ABSOLUTE || rel_seek_secs < 0)
            flags |= SEEK_BACKWARD;
        else
            flags |= SEEK_FORWARD;
    }
    // Adjust the target a little bit to catch cases where the target position
    // specifies a keyframe with high, but not perfect, precision.
    rel_seek_secs += flags & SEEK_FORWARD ? -0.005 : 0.005;

    free_cached_dps(demuxer);
    if (!(flags & SEEK_FACTOR)) {       /* time in secs */
        mkv_index_t *index = NULL;
        mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
        stream_t *s = demuxer->stream;
        int64_t target_timecode = 0, diff, min_diff = 0xFFFFFFFFFFFFFFFLL;
        int i;

        if (!(flags & SEEK_ABSOLUTE))   /* relative seek */
            target_timecode = (int64_t) (mkv_d->last_pts * 1000.0);
        target_timecode += (int64_t) (rel_seek_secs * 1000.0);
        if (target_timecode < 0)
            target_timecode = 0;

        if (mkv_d->indexes == NULL) {   /* no index was found */
            int64_t target_tc_ns = (int64_t) (rel_seek_secs * 1e9);
            if (target_tc_ns < 0)
                target_tc_ns = 0;
            uint64_t max_filepos = 0;
            int64_t max_tc = -1;
            int n = mkv_d->num_cluster_pos;
            if (n > 0) {
                max_filepos = mkv_d->cluster_positions[n - 1].filepos;
                max_tc = mkv_d->cluster_positions[n - 1].timecode;
            }

            if (target_tc_ns > max_tc) {
                if ((off_t) max_filepos > stream_tell(s))
                    stream_seek(s, max_filepos);
                else
                    stream_seek(s, stream_tell(s) + mkv_d->cluster_size);
                /* parse all the clusters upto target_filepos */
                while (!s->eof) {
                    uint64_t start = stream_tell(s);
                    uint32_t type = ebml_read_id(s, NULL);
                    uint64_t len = ebml_read_length(s, NULL);
                    uint64_t end = stream_tell(s) + len;
                    if (type == MATROSKA_ID_CLUSTER) {
                        while (!s->eof && stream_tell(s) < end) {
                            if (ebml_read_id(s, NULL)
                                == MATROSKA_ID_TIMECODE) {
                                uint64_t tc = ebml_read_uint(s, NULL);
                                tc *= mkv_d->tc_scale;
                                add_cluster_position(mkv_d, start, tc);
                                if (tc >= target_tc_ns)
                                    goto enough_index;
                                break;
                            }
                        }
                    }
                    stream_seek(s, end);
                }
            enough_index:
                if (s->eof)
                    stream_reset(s);
            }
            if (!mkv_d->num_cluster_pos) {
                mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] no target for seek found\n");
                return;
            }
            uint64_t cluster_pos = mkv_d->cluster_positions[0].filepos;
            /* Let's find the nearest cluster */
            for (i = 0; i < mkv_d->num_cluster_pos; i++) {
                diff = mkv_d->cluster_positions[i].timecode - target_tc_ns;
                if (flags & SEEK_BACKWARD && diff < 0 && -diff < min_diff) {
                    cluster_pos = mkv_d->cluster_positions[i].filepos;
                    min_diff = -diff;
                } else if (flags & SEEK_FORWARD
                           && (diff < 0 ? -1 * diff : diff) < min_diff) {
                    cluster_pos = mkv_d->cluster_positions[i].filepos;
                    min_diff = diff < 0 ? -1 * diff : diff;
                }
            }
            mkv_d->cluster_size = mkv_d->blockgroup_size = 0;
            stream_seek(s, cluster_pos);
        } else {
            int seek_id = (demuxer->video->id < 0) ?
                demuxer->audio->id : demuxer->video->id;

            /* let's find the entry in the indexes with the smallest */
            /* difference to the wanted timecode. */
            for (i = 0; i < mkv_d->num_indexes; i++)
                if (mkv_d->indexes[i].tnum == seek_id) {
                    diff =
                        target_timecode -
                        (int64_t) (mkv_d->indexes[i].timecode *
                                   mkv_d->tc_scale / 1000000.0 + 0.5);

                    if (flags & SEEK_BACKWARD) {
                        // Seek backward: find the last index position
                        // before target time
                        if (diff < 0 || diff >= min_diff)
                            continue;
                    } else {
                        // Seek forward: find the first index position
                        // after target time. If no such index exists, find last
                        // position between current position and target time.
                        if (diff <= 0) {
                            if (min_diff <= 0 && diff <= min_diff)
                                continue;
                        } else if (diff >=
                                   FFMIN(target_timecode - mkv_d->last_pts,
                                         min_diff))
                            continue;
                    }
                    min_diff = diff;
                    index = mkv_d->indexes + i;
                }

            if (index) {        /* We've found an entry. */
                mkv_d->cluster_size = mkv_d->blockgroup_size = 0;
                stream_seek(s, index->filepos);
            }
        }

        if (demuxer->video->id >= 0)
            mkv_d->v_skip_to_keyframe = 1;
        if (flags & SEEK_FORWARD)
            mkv_d->skip_to_timecode = target_timecode;
        else
            mkv_d->skip_to_timecode = index ? index->timecode : 0;
        mkv_d->a_skip_to_keyframe = 1;

        demux_mkv_fill_buffer(demuxer, NULL);
    } else if ((demuxer->movi_end <= 0) || !(flags & SEEK_ABSOLUTE))
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] seek unsupported flags\n");
    else {
        mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
        stream_t *s = demuxer->stream;
        uint64_t target_filepos;
        mkv_index_t *index = NULL;
        int i;

        if (mkv_d->indexes == NULL) {   /* not implemented without index */
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] seek unsupported flags\n");
            return;
        }

        target_filepos = (uint64_t) (demuxer->movi_end * rel_seek_secs);
        for (i = 0; i < mkv_d->num_indexes; i++)
            if (mkv_d->indexes[i].tnum == demuxer->video->id)
                if ((index == NULL)
                    || ((mkv_d->indexes[i].filepos >= target_filepos)
                        && ((index->filepos < target_filepos)
                            || (mkv_d->indexes[i].filepos < index->filepos))))
                    index = &mkv_d->indexes[i];

        if (!index)
            return;

        mkv_d->cluster_size = mkv_d->blockgroup_size = 0;
        stream_seek(s, index->filepos);

        if (demuxer->video->id >= 0)
            mkv_d->v_skip_to_keyframe = 1;
        mkv_d->skip_to_timecode = index->timecode;
        mkv_d->a_skip_to_keyframe = 1;

        demux_mkv_fill_buffer(demuxer, NULL);
    }
}

static int demux_mkv_control(demuxer_t *demuxer, int cmd, void *arg)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;

    switch (cmd) {
    case DEMUXER_CTRL_CORRECT_PTS:
        return DEMUXER_CTRL_OK;
    case DEMUXER_CTRL_GET_TIME_LENGTH:
        if (mkv_d->duration == 0)
            return DEMUXER_CTRL_DONTKNOW;

        *((double *) arg) = (double) mkv_d->duration;
        return DEMUXER_CTRL_OK;

    case DEMUXER_CTRL_GET_PERCENT_POS:
        if (mkv_d->duration == 0) {
            return DEMUXER_CTRL_DONTKNOW;
        }

        *((int *) arg) = (int) (100 * mkv_d->last_pts / mkv_d->duration);
        return DEMUXER_CTRL_OK;

    case DEMUXER_CTRL_SWITCH_AUDIO:
        if (demuxer->audio && demuxer->audio->sh) {
            sh_audio_t *sh = demuxer->a_streams[demuxer->audio->id];
            int aid = *(int *) arg;
            if (aid < 0)
                aid = (sh->aid + 1) % mkv_d->last_aid;
            if (aid != sh->aid) {
                mkv_track_t *track =
                    demux_mkv_find_track_by_num(mkv_d, aid,
                                                MATROSKA_TRACK_AUDIO);
                if (track) {
                    demuxer->audio->id = track->tnum;
                    sh = demuxer->a_streams[demuxer->audio->id];
                    ds_free_packs(demuxer->audio);
                }
            }
            *(int *) arg = sh->aid;
        } else
            *(int *) arg = -2;
        return DEMUXER_CTRL_OK;

    default:
        return DEMUXER_CTRL_NOTIMPL;
    }
}

const demuxer_desc_t demuxer_desc_matroska = {
    "Matroska demuxer",
    "mkv",
    "Matroska",
    "Aurelien Jacobs",
    "",
    DEMUXER_TYPE_MATROSKA,
    1,                          // safe autodetect
    demux_mkv_open,
    demux_mkv_fill_buffer,
    NULL,
    demux_close_mkv,
    demux_mkv_seek,
    demux_mkv_control
};
