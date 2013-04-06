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

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>

#include <libavutil/common.h>
#include <libavutil/lzo.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/avstring.h>

#include "config.h"

#if CONFIG_ZLIB
#include <zlib.h>
#endif

#include "talloc.h"
#include "core/options.h"
#include "core/bstr.h"
#include "stream/stream.h"
#include "demux.h"
#include "stheader.h"
#include "ebml.h"
#include "matroska.h"
//#include "demux_real.h"

#include "core/mp_msg.h"

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

typedef struct mkv_content_encoding {
    uint64_t order, type, scope;
    uint64_t comp_algo;
    uint8_t *comp_settings;
    int comp_settings_len;
} mkv_content_encoding_t;

typedef struct mkv_track {
    int tnum;
    char *name;
    int id;  // -aid / -sid / -vid option value

    char *codec_id;
    int ms_compat;
    char *language;

    int type;

    uint32_t v_width, v_height, v_dwidth, v_dheight;
    bool v_dwidth_set, v_dheight_set;
    double v_frate;
    uint32_t colorspace;

    uint32_t a_formattag;
    uint32_t a_channels, a_bps;
    float a_sfreq;
    float a_osfreq;

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
    int64_t segment_start;

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

    int64_t *parsed_pos;
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

    uint64_t skip_to_timecode;
    int v_skip_to_keyframe, a_skip_to_keyframe;
    bool subtitle_preroll;

    int num_audio_tracks;
    int num_video_tracks;
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

static bool is_parsed_header(struct mkv_demuxer *mkv_d, int64_t pos)
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
        mkv_d->parsed_pos = talloc_realloc(mkv_d, mkv_d->parsed_pos, int64_t,
                                       mkv_d->num_parsed_pos + 32);
    mkv_d->num_parsed_pos++;
    for (int i = mkv_d->num_parsed_pos - 1; i > low; i--)
        mkv_d->parsed_pos[i] = mkv_d->parsed_pos[i - 1];
    mkv_d->parsed_pos[low] = pos;
    return false;
}

static mkv_track_t *find_track_by_num(struct mkv_demuxer *d, int n, int type)
{
    for (int i = 0; i < d->num_tracks; i++)
        if (d->tracks[i] != NULL && d->tracks[i]->type == type)
            if (d->tracks[i]->id == n)
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
            talloc_free(src);
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
                *dest = talloc_realloc_size(NULL, *dest, *size);
                zstream.next_out = (Bytef *) (*dest + zstream.total_out);
                result = inflate(&zstream, Z_NO_FLUSH);
                if (result != Z_OK && result != Z_STREAM_END) {
                    mp_tmsg(MSGT_DEMUX, MSGL_WARN,
                            "[mkv] zlib decompression failed.\n");
                    talloc_free(*dest);
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
            int out_avail;
            int dstlen = *size * 3;

            *dest = NULL;
            while (1) {
                int srclen = *size;
                *dest = talloc_realloc_size(NULL, *dest,
                                            dstlen + AV_LZO_OUTPUT_PADDING);
                out_avail = dstlen;
                int result = av_lzo1x_decode(*dest, &out_avail, src, &srclen);
                if (result == 0)
                    break;
                if (!(result & AV_LZO_OUTPUT_FULL)) {
                    mp_tmsg(MSGT_DEMUX, MSGL_WARN,
                            "[mkv] lzo decompression failed.\n");
                    talloc_free(*dest);
                    *dest = NULL;
                    goto error;
                }
                mp_msg(MSGT_DEMUX, MSGL_DBG2,
                       "[mkv] lzo decompression buffer too small.\n");
                dstlen *= 2;
            }
            *size = dstlen - out_avail;
        } else if (enc->comp_algo == 3) {
            *dest = talloc_size(NULL, *size + enc->comp_settings_len);
            memcpy(*dest, enc->comp_settings, enc->comp_settings_len);
            memcpy(*dest + enc->comp_settings_len, src, *size);
            *size += enc->comp_settings_len;
        }
    }

 error:
    if (src != *dest && src != orig_src)
        talloc_free(src);
}


static int demux_mkv_read_info(demuxer_t *demuxer)
{
    mkv_demuxer_t *mkv_d = demuxer->priv;
    stream_t *s = demuxer->stream;
    int res = 0;

    mkv_d->tc_scale = 1000000;
    mkv_d->duration = 0;

    struct ebml_info info = {};
    struct ebml_parse_ctx parse_ctx = {};
    if (ebml_read_element(s, &parse_ctx, &info, &ebml_info_desc) < 0)
        return -1;
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
    if (demuxer->params && demuxer->params->matroska_wanted_uids) {
        unsigned char (*uids)[16] = demuxer->params->matroska_wanted_uids;
        if (!info.n_segment_uid)
            uids = NULL;
        for (int i = 0; i < MP_TALLOC_ELEMS(uids); i++) {
            if (!memcmp(info.segment_uid.start, uids[i], 16))
                goto out;
        }
        mp_tmsg(MSGT_DEMUX, MSGL_INFO,
                "[mkv] This is not one of the wanted files. "
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
                    "but mpv has not been compiled\n"
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
    if (audio->n_output_sampling_frequency) {
        track->a_osfreq = audio->output_sampling_frequency;
        mp_msg(MSGT_DEMUX, MSGL_V,
               "[mkv] |   + Output sampling frequency: %f\n", track->a_osfreq);
    } else
        track->a_osfreq = track->a_sfreq;
    // Something creates files with osfreq incorrectly set
    if (track->a_sfreq == 44100 && track->a_osfreq == 96000) {
        mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] Audio track has codec frequency "
               "%.1f and playback frequency %.1f.\n[mkv] This looks wrong. "
               "Assuming this file is corrupt and ignoring the latter.\n",
               track->a_sfreq, track->a_osfreq);
        track->a_osfreq = track->a_sfreq;
    }
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
        track->v_dwidth_set = true;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Display width: %u\n",
               track->v_dwidth);
    }
    if (video->n_display_height) {
        track->v_dheight = video->display_height;
        track->v_dheight_set = true;
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
    if (video->n_colour_space && video->colour_space.len == 4) {
        uint8_t *d = (uint8_t *)&video->colour_space.start[0];
        track->colorspace = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |   + Colorspace: %#x\n",
               (unsigned int)track->colorspace);
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
            track->subtitle_type = 'v';
        else if (!strcmp(track->codec_id, MKV_S_TEXTSSA)
                 || !strcmp(track->codec_id, MKV_S_TEXTASS)
                 || !strcmp(track->codec_id, MKV_S_SSA)
                 || !strcmp(track->codec_id, MKV_S_ASS))
            track->subtitle_type = 'a';
        else if (!strcmp(track->codec_id, MKV_S_TEXTASCII)
                   || !strcmp(track->codec_id, MKV_S_TEXTUTF8))
            track->subtitle_type = 't';
        else if (!strcmp(track->codec_id, MKV_S_PGS))
            track->subtitle_type = 'p';
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
        return -1;

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
        return -1;
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
        return -1;

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
            chapter.start = ca->chapter_time_start;
            chapter.end = ca->chapter_time_end;

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

            if (idx == selected_edition){
                demuxer_add_chapter(demuxer, name, chapter.start, chapter.end);
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

    demuxer->num_editions = num_editions;
    demuxer->edition = selected_edition;

    talloc_free(parse_ctx.talloc_ctx);
    mp_msg(MSGT_DEMUX, MSGL_V,
           "[mkv] \\---- [ parsing chapters ] ---------\n");
    return 0;
}

static int demux_mkv_read_tags(demuxer_t *demuxer)
{
    stream_t *s = demuxer->stream;

    struct ebml_parse_ctx parse_ctx = {};
    struct ebml_tags           tags = {};
    if (ebml_read_element(s, &parse_ctx, &tags, &ebml_tags_desc) < 0)
        return -1;

    for (int i = 0; i < tags.n_tag; i++) {
        struct ebml_tag tag = tags.tag[i];
        if (tag.targets.target_track_uid  || tag.targets.target_edition_uid ||
            tag.targets.target_chapter_uid || tag.targets.target_attachment_uid)
            continue;

        for (int j = 0; j < tag.n_simple_tag; j++)
            demux_info_add_bstr(demuxer, tag.simple_tag[j].tag_name, tag.simple_tag[j].tag_string);
    }

    talloc_free(parse_ctx.talloc_ctx);
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
        return -1;

    for (int i = 0; i < attachments.n_attached_file; i++) {
        struct ebml_attached_file *attachment = &attachments.attached_file[i];
        if (!attachment->n_file_name || !attachment->n_file_mime_type
            || !attachment->n_file_data) {
            mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] Malformed attachment\n");
            continue;
        }
        struct bstr name = attachment->file_name;
        struct bstr mime = attachment->file_mime_type;
        demuxer_add_attachment(demuxer, name, mime, attachment->file_data);
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] Attachment: %.*s, %.*s, %zu bytes\n",
               BSTR_P(name), BSTR_P(mime), attachment->file_data.len);
    }

    talloc_free(parse_ctx.talloc_ctx);
    mp_msg(MSGT_DEMUX, MSGL_V,
           "[mkv] \\---- [ parsing attachments ] ---------\n");
    return 0;
}

static int read_header_element(struct demuxer *demuxer, uint32_t id,
                               int64_t at_filepos);

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
        res = -1;
        goto out;
    }
    /* off now holds the position of the next element after the seek head. */
    int64_t off = stream_tell(s);
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
        int r = read_header_element(demuxer, seek->seek_id, pos);
        if (r <= -2) {
            res = r;
            goto out;
        }
    }
    if (!stream_seek(s, off)) {
        mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] Couldn't seek back after "
               "SeekHead??\n");
        res = -1;
    }
 out:
    mp_msg(MSGT_DEMUX, MSGL_V,
           "[mkv] \\---- [ parsing seek head ] ---------\n");
    talloc_free(parse_ctx.talloc_ctx);
    return res;
}

static bool seek_pos_id(struct stream *s, int64_t pos, uint32_t id)
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
                               int64_t at_filepos)
{
    struct mkv_demuxer *mkv_d = demuxer->priv;
    stream_t *s = demuxer->stream;
    int64_t pos = stream_tell(s) - 4;
    int res = 1;

    switch(id) {
    case MATROSKA_ID_INFO:
        if (mkv_d->parsed_info)
            break;
        if (at_filepos && !seek_pos_id(s, at_filepos, id))
            return -1;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |+ segment information...\n");
        mkv_d->parsed_info = true;
        return demux_mkv_read_info(demuxer);

    case MATROSKA_ID_TRACKS:
        if (mkv_d->parsed_tracks)
            break;
        if (at_filepos && !seek_pos_id(s, at_filepos, id))
            return -1;
        mkv_d->parsed_tracks = true;
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |+ segment tracks...\n");
        return demux_mkv_read_tracks(demuxer);

    case MATROSKA_ID_CUES:
        if (is_parsed_header(mkv_d, pos))
            break;
        if (at_filepos && !seek_pos_id(s, at_filepos, id))
            return -1;
        return demux_mkv_read_cues(demuxer);

    case MATROSKA_ID_TAGS:
        if (mkv_d->parsed_tags)
            break;
        if (at_filepos && !seek_pos_id(s, at_filepos, id))
            return -1;
        mkv_d->parsed_tags = true;
        return demux_mkv_read_tags(demuxer);

    case MATROSKA_ID_SEEKHEAD:
        if (is_parsed_header(mkv_d, pos))
            break;
        if (at_filepos && !seek_pos_id(s, at_filepos, id))
            return -1;
        return demux_mkv_read_seekhead(demuxer);

    case MATROSKA_ID_CHAPTERS:
        if (mkv_d->parsed_chapters)
            break;
        if (at_filepos && !seek_pos_id(s, at_filepos, id))
            return -1;
        mkv_d->parsed_chapters = true;
        return demux_mkv_read_chapters(demuxer);

    case MATROSKA_ID_ATTACHMENTS:
        if (mkv_d->parsed_attachments)
            break;
        if (at_filepos && !seek_pos_id(s, at_filepos, id))
            return -1;
        mkv_d->parsed_attachments = true;
        return demux_mkv_read_attachments(demuxer);

    case EBML_ID_VOID:
        break;

    default:
        res = 2;
    }
    if (!at_filepos && id != EBML_ID_INVALID)
        ebml_read_skip(s, NULL);
    return res;
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
            mkv_d->tracks[i]->id = -1;
            if (vid == MAX_V_STREAMS)
                break;
            mkv_d->tracks[i]->id = vid;
            demux_mkv_open_video(demuxer, mkv_d->tracks[i], vid);
            if (mkv_d->tracks[i]->name)
                mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VID_%d_NAME=%s\n", vid,
                       mkv_d->tracks[i]->name);
            sprintf(str, "-vid %u", vid++);
            break;
        case MATROSKA_TRACK_AUDIO:
            type = "audio";
            mkv_d->tracks[i]->id = -1;
            if (aid == MAX_A_STREAMS)
                break;
            mkv_d->tracks[i]->id = aid;
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
            mkv_d->tracks[i]->id = -1;
            if (sid == MAX_S_STREAMS)
                break;
            mkv_d->tracks[i]->id = sid;
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
            mp_tmsg(MSGT_DEMUX, MSGL_V,
                    "[mkv] Track ID %u: %s (%s) \"%s\", %s\n",
                    mkv_d->tracks[i]->tnum, type, mkv_d->tracks[i]->codec_id,
                    mkv_d->tracks[i]->name, str);
        else
            mp_tmsg(MSGT_DEMUX, MSGL_V, "[mkv] Track ID %u: %s (%s), %s\n",
                    mkv_d->tracks[i]->tnum, type, mkv_d->tracks[i]->codec_id,
                    str);
    }
    mkv_d->num_audio_tracks = aid;
    mkv_d->num_video_tracks = vid;
}

typedef struct {
    char *id;
    int fourcc;
    int extradata;
} videocodec_info_t;

static const videocodec_info_t vinfo[] = {
    {MKV_V_MJPEG,     mmioFOURCC('m', 'j', 'p', 'g'), 1},
    {MKV_V_MPEG1,     mmioFOURCC('m', 'p', 'g', '1'), 0},
    {MKV_V_MPEG2,     mmioFOURCC('m', 'p', 'g', '2'), 0},
    {MKV_V_MPEG4_SP,  mmioFOURCC('m', 'p', '4', 'v'), 1},
    {MKV_V_MPEG4_ASP, mmioFOURCC('m', 'p', '4', 'v'), 1},
    {MKV_V_MPEG4_AP,  mmioFOURCC('m', 'p', '4', 'v'), 1},
    {MKV_V_MPEG4_AVC, mmioFOURCC('a', 'v', 'c', '1'), 1},
    {MKV_V_THEORA,    mmioFOURCC('t', 'h', 'e', 'o'), 1},
    {MKV_V_VP8,       mmioFOURCC('V', 'P', '8', '0'), 0},
    {NULL, 0, 0}
};

static int demux_mkv_open_video(demuxer_t *demuxer, mkv_track_t *track,
                                int vid)
{
    BITMAPINFOHEADER *bih;
    sh_video_t *sh_v;
    bool raw = false;

    if (track->ms_compat) {     /* MS compatibility mode */
        BITMAPINFOHEADER *src;

        if (track->private_data == NULL
            || track->private_size < sizeof(*bih))
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
        memcpy(bih + 1,
               src + 1,
               track->private_size - sizeof(*bih));

        if (track->v_width == 0)
            track->v_width = bih->biWidth;
        if (track->v_height == 0)
            track->v_height = bih->biHeight;
    } else {
        bih = calloc(1, sizeof(*bih));
        bih->biSize = sizeof(*bih);
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
            bih = realloc(bih, sizeof(*bih) + 8 + cnt);
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
            memcpy(dst, src - 8, 8 + cnt);
            track->realmedia = 1;
        } else if (strcmp(track->codec_id, MKV_V_UNCOMPRESSED) == 0) {
            // raw video, "like AVI" - this is a FourCC
            bih->biCompression = track->colorspace;
            raw = true;
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

    sh_v = new_sh_video(demuxer, vid);
    sh_v->gsh->demuxer_id = track->tnum;
    sh_v->gsh->title = talloc_strdup(sh_v, track->name);
    sh_v->bih = bih;
    sh_v->format = sh_v->bih->biCompression;
    if (raw) {
        sh_v->gsh->codec = "rawvideo";
    } else {
        mp_set_video_codec_from_tag(sh_v);
        sh_v->format = mp_video_fourcc_alias(sh_v->format);
    }
    if (track->v_frate == 0.0)
        track->v_frate = 25.0;
    sh_v->fps = track->v_frate;
    sh_v->frametime = 1 / track->v_frate;
    sh_v->aspect = 0;
    if (!track->realmedia) {
        sh_v->disp_w = track->v_width;
        sh_v->disp_h = track->v_height;
        uint32_t dw = track->v_dwidth_set ? track->v_dwidth : track->v_width;
        uint32_t dh = track->v_dheight_set ? track->v_dheight : track->v_height;
        if (dw && dh)
            sh_v->aspect = (double) dw / dh;
    } else {
        // vd_realvid.c will set aspect to disp_w/disp_h and rederive
        // disp_w and disp_h from the RealVideo stream contents returned
        // by the Real DLLs. If DisplayWidth/DisplayHeight was not set in
        // the Matroska file then it has already been set to PixelWidth/Height
        // by check_track_information.
        sh_v->disp_w = track->v_dwidth;
        sh_v->disp_h = track->v_dheight;
    }
    mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] Aspect: %f\n", sh_v->aspect);

    sh_v->ds = demuxer->video;
    return 0;
}

static struct mkv_audio_tag {
    char *id;   bool prefix;   uint32_t formattag;
} mkv_audio_tags[] = {
    { MKV_A_MP2,       0, 0x0055 },
    { MKV_A_MP3,       0, 0x0055 },
    { MKV_A_AC3,       1, 0x2000 },
    { MKV_A_EAC3,      1, mmioFOURCC('E', 'A', 'C', '3') },
    { MKV_A_DTS,       0, 0x2001 },
    { MKV_A_PCM,       0, 0x0001 },
    { MKV_A_PCM_BE,    0, 0x0001 },
    { MKV_A_AAC_2MAIN, 0, mmioFOURCC('M', 'P', '4', 'A') },
    { MKV_A_AAC_2LC,   1, mmioFOURCC('M', 'P', '4', 'A') },
    { MKV_A_AAC_2SSR,  0, mmioFOURCC('M', 'P', '4', 'A') },
    { MKV_A_AAC_4MAIN, 0, mmioFOURCC('M', 'P', '4', 'A') },
    { MKV_A_AAC_4LC,   1, mmioFOURCC('M', 'P', '4', 'A') },
    { MKV_A_AAC_4SSR,  0, mmioFOURCC('M', 'P', '4', 'A') },
    { MKV_A_AAC_4LTP,  0, mmioFOURCC('M', 'P', '4', 'A') },
    { MKV_A_AAC,       0, mmioFOURCC('M', 'P', '4', 'A') },
    { MKV_A_VORBIS,    0, mmioFOURCC('v', 'r', 'b', 's') },
    { MKV_A_OPUS,      0, mmioFOURCC('O', 'p', 'u', 's') },
    { MKV_A_OPUS_EXP,  0, mmioFOURCC('O', 'p', 'u', 's') },
    { MKV_A_QDMC,      0, mmioFOURCC('Q', 'D', 'M', 'C') },
    { MKV_A_QDMC2,     0, mmioFOURCC('Q', 'D', 'M', '2') },
    { MKV_A_WAVPACK,   0, mmioFOURCC('W', 'V', 'P', 'K') },
    { MKV_A_TRUEHD,    0, mmioFOURCC('T', 'R', 'H', 'D') },
    { MKV_A_FLAC,      0, mmioFOURCC('f', 'L', 'a', 'C') },
    { MKV_A_ALAC,      0, mmioFOURCC('a', 'L', 'a', 'C') },
    { MKV_A_REAL28,    0, mmioFOURCC('2', '8', '_', '8') },
    { MKV_A_REALATRC,  0, mmioFOURCC('a', 't', 'r', 'c') },
    { MKV_A_REALCOOK,  0, mmioFOURCC('c', 'o', 'o', 'k') },
    { MKV_A_REALDNET,  0, mmioFOURCC('d', 'n', 'e', 't') },
    { MKV_A_REALSIPR,  0, mmioFOURCC('s', 'i', 'p', 'r') },
    { MKV_A_TTA1,      0, mmioFOURCC('T', 'T', 'A', '1') },
    { NULL },
};


static int demux_mkv_open_audio(demuxer_t *demuxer, mkv_track_t *track,
                                int aid)
{
    sh_audio_t *sh_a = new_sh_audio(demuxer, aid);
    if (!sh_a)
        return 1;

    if (track->language && (strcmp(track->language, "und") != 0))
        sh_a->gsh->lang = talloc_strdup(sh_a, track->language);
    sh_a->gsh->demuxer_id = track->tnum;
    sh_a->gsh->title = talloc_strdup(sh_a, track->name);
    sh_a->gsh->default_track = track->default_track;
    sh_a->ds = demuxer->audio;
    if (track->ms_compat) {
        if (track->private_size < sizeof(*sh_a->wf))
            goto error;
        WAVEFORMATEX *wf = (WAVEFORMATEX *) track->private_data;
        sh_a->wf = calloc(1, track->private_size);
        sh_a->wf->wFormatTag = le2me_16(wf->wFormatTag);
        sh_a->wf->nChannels = le2me_16(wf->nChannels);
        sh_a->wf->nSamplesPerSec = le2me_32(wf->nSamplesPerSec);
        sh_a->wf->nAvgBytesPerSec = le2me_32(wf->nAvgBytesPerSec);
        sh_a->wf->nBlockAlign = le2me_16(wf->nBlockAlign);
        sh_a->wf->wBitsPerSample = le2me_16(wf->wBitsPerSample);
        sh_a->wf->cbSize = track->private_size - sizeof(*sh_a->wf);
        memcpy(sh_a->wf + 1, wf + 1,
               track->private_size - sizeof(*sh_a->wf));
        if (track->a_sfreq == 0.0)
            track->a_sfreq = sh_a->wf->nSamplesPerSec;
        if (track->a_channels == 0)
            track->a_channels = sh_a->wf->nChannels;
        if (track->a_bps == 0)
            track->a_bps = sh_a->wf->wBitsPerSample;
        track->a_formattag = sh_a->wf->wFormatTag;
    } else {
        sh_a->wf = calloc(1, sizeof(*sh_a->wf));
        for (int i = 0; ; i++) {
            struct mkv_audio_tag *t = mkv_audio_tags + i;
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

    sh_a->format = track->a_formattag;
    sh_a->wf->wFormatTag = track->a_formattag;
    mp_chmap_from_channels(&sh_a->channels, track->a_channels);
    sh_a->wf->nChannels = track->a_channels;
    sh_a->samplerate = (uint32_t) track->a_sfreq;
    sh_a->container_out_samplerate = track->a_osfreq;
    sh_a->wf->nSamplesPerSec = (uint32_t) track->a_sfreq;
    if (track->a_bps == 0)
        sh_a->wf->wBitsPerSample = 16;
    else
        sh_a->wf->wBitsPerSample = track->a_bps;
    if (track->a_formattag == 0x0055) { /* MP3 || MP2 */
        sh_a->wf->nAvgBytesPerSec = 16000;
        sh_a->wf->nBlockAlign = 1152;
    } else if ((track->a_formattag == 0x2000)           /* AC3 */
               || track->a_formattag == mmioFOURCC('E', 'A', 'C', '3')
               || (track->a_formattag == 0x2001)) {        /* DTS */
        free(sh_a->wf);
        sh_a->wf = NULL;
    } else if (track->a_formattag == 0x0001) {  /* PCM || PCM_BE */
        sh_a->wf->nAvgBytesPerSec = sh_a->channels.num * sh_a->samplerate * 2;
        sh_a->wf->nBlockAlign = sh_a->wf->nAvgBytesPerSec;
        if (!strcmp(track->codec_id, MKV_A_PCM_BE))
            sh_a->format = mmioFOURCC('t', 'w', 'o', 's');
    } else if (!strcmp(track->codec_id, MKV_A_QDMC)
               || !strcmp(track->codec_id, MKV_A_QDMC2)) {
        sh_a->wf->nAvgBytesPerSec = 16000;
        sh_a->wf->nBlockAlign = 1486;
        track->fix_i_bps = 1;
        track->qt_last_a_pts = 0.0;
        goto copy_private_data;
    } else if (track->a_formattag == mmioFOURCC('M', 'P', '4', 'A')) {
        int profile, srate_idx;

        sh_a->wf->nAvgBytesPerSec = 16000;
        sh_a->wf->nBlockAlign = 1024;

        if (!strcmp(track->codec_id, MKV_A_AAC) && track->private_data)
            goto copy_private_data;

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
    } else if (track->a_formattag == mmioFOURCC('v', 'r', 'b', 's')) {
        /* VORBIS */
        if (track->private_size == 0 || track->ms_compat && !sh_a->wf->cbSize)
            goto error;
        if (!track->ms_compat) {
            sh_a->wf->cbSize = track->private_size;
            sh_a->wf = realloc(sh_a->wf, sizeof(*sh_a->wf) + sh_a->wf->cbSize);
            memcpy((unsigned char *) (sh_a->wf + 1), track->private_data,
                   sh_a->wf->cbSize);
        }
    } else if (!strcmp(track->codec_id, MKV_A_OPUS)
               || !strcmp(track->codec_id, MKV_A_OPUS_EXP)) {
        sh_a->format = mmioFOURCC('O', 'p', 'u', 's');
    } else if (!strncmp(track->codec_id, MKV_A_REALATRC, 7)) {
        if (track->private_size < RAPROPERTIES4_SIZE)
            goto error;
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
        sh_a->wf = realloc(sh_a->wf, sizeof(*sh_a->wf) + sh_a->wf->cbSize);
        memcpy(((char *) (sh_a->wf + 1)), src, codecdata_length);

        switch (track->a_formattag) {
        case mmioFOURCC('a', 't', 'r', 'c'):
            sh_a->wf->nAvgBytesPerSec = atrc_fl2bps[flavor];
            sh_a->wf->nBlockAlign = track->sub_packet_size;
            goto audiobuf;
        case mmioFOURCC('c', 'o', 'o', 'k'):
            sh_a->wf->nAvgBytesPerSec = cook_fl2bps[flavor];
            sh_a->wf->nBlockAlign = track->sub_packet_size;
            goto audiobuf;
        case mmioFOURCC('s', 'i', 'p', 'r'):
            sh_a->wf->nAvgBytesPerSec = sipr_fl2bps[flavor];
            sh_a->wf->nBlockAlign = track->coded_framesize;
            goto audiobuf;
        case mmioFOURCC('2', '8', '_', '8'):
            sh_a->wf->nAvgBytesPerSec = 3600;
            sh_a->wf->nBlockAlign = track->coded_framesize;
        audiobuf:
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

        if (!track->ms_compat) {
            ptr = track->private_data;
            size = track->private_size;
        } else {
            sh_a->format = mmioFOURCC('f', 'L', 'a', 'C');
            ptr = track->private_data + sizeof(*sh_a->wf);
            size = track->private_size - sizeof(*sh_a->wf);
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
    } else if (!strcmp(track->codec_id, MKV_A_ALAC)) {
        if (track->private_size && track->private_size < 10000000) {
            sh_a->codecdata_len = track->private_size + 12;
            sh_a->codecdata = malloc(sh_a->codecdata_len);
            char *data = sh_a->codecdata;
            AV_WB32(data + 0, sh_a->codecdata_len);
            memcpy(data + 4, "alac", 4);
            AV_WB32(data + 8, 0);
            memcpy(data + 12, track->private_data, track->private_size);
        }
    } else if (track->a_formattag == mmioFOURCC('W', 'V', 'P', 'K') ||
               track->a_formattag == mmioFOURCC('T', 'R', 'H', 'D')) {
    copy_private_data:
        if (!track->ms_compat && track->private_size) {
            sh_a->codecdata = malloc(track->private_size);
            sh_a->codecdata_len = track->private_size;
            memcpy(sh_a->codecdata, track->private_data, track->private_size);
        }
    } else if (track->a_formattag == mmioFOURCC('T', 'T', 'A', '1')) {
        sh_a->codecdata_len = 30;
        sh_a->codecdata = calloc(1, sh_a->codecdata_len);
        if (!sh_a->codecdata)
            goto error;
        char *data = sh_a->codecdata;
        memcpy(data + 0, "TTA1", 4);
        AV_WL16(data + 4, 1);
        AV_WL16(data + 6, sh_a->channels.num);
        AV_WL16(data + 8, sh_a->wf->wBitsPerSample);
        AV_WL32(data + 10, sh_a->samplerate);
        // Bogus: last frame won't be played.
        AV_WL32(data + 14, 0);
    } else if (!track->ms_compat) {
        goto error;
    }

    mp_set_audio_codec_from_tag(sh_a);

    return 0;

 error:
    mp_tmsg(MSGT_DEMUX, MSGL_WARN, "[mkv] Unknown/unsupported audio "
            "codec ID '%s' for track %u or missing/faulty\n[mkv] "
            "private codec data.\n", track->codec_id, track->tnum);
    return 1;
}

static int demux_mkv_open_sub(demuxer_t *demuxer, mkv_track_t *track,
                              int sid)
{
    if (track->subtitle_type) {
        int size;
        uint8_t *buffer;
        sh_sub_t *sh = new_sh_sub(demuxer, sid);
        sh->gsh->demuxer_id = track->tnum;
        track->sh_sub = sh;
        sh->type = track->subtitle_type;
        size = track->private_size;
        demux_mkv_decode(track, track->private_data, &buffer, &size, 2);
        if (buffer && buffer != track->private_data) {
            talloc_free(track->private_data);
            talloc_steal(track, buffer);
            track->private_data = buffer;
            track->private_size = size;
        }
        sh->extradata = malloc(track->private_size);
        memcpy(sh->extradata, track->private_data, track->private_size);
        sh->extradata_len = track->private_size;
        if (track->language && (strcmp(track->language, "und") != 0))
            sh->gsh->lang = talloc_strdup(sh, track->language);
        sh->gsh->title = talloc_strdup(sh, track->name);
        sh->gsh->default_track = track->default_track;
    } else {
        mp_tmsg(MSGT_DEMUX, MSGL_ERR,
                "[mkv] Subtitle type '%s' is not supported.\n",
                track->codec_id);
        return 1;
    }

    return 0;
}

static void mkv_free(struct demuxer *demuxer)
{
    struct mkv_demuxer *mkv_d = demuxer->priv;
    if (!mkv_d)
        return;
    for (int i = 0; i < mkv_d->num_tracks; i++)
        demux_mkv_free_trackentry(mkv_d->tracks[i]);
    free(mkv_d->indexes);
    free(mkv_d->cluster_positions);
}

static int demux_mkv_open(demuxer_t *demuxer)
{
    stream_t *s = demuxer->stream;
    mkv_demuxer_t *mkv_d;
    mkv_track_t *track;

    stream_seek(s, s->start_pos);
    if (ebml_read_id(s, NULL) != EBML_ID_EBML)
        return 0;
    struct ebml_ebml ebml_master = {};
    struct ebml_parse_ctx parse_ctx = { .no_error_messages = true };
    if (ebml_read_element(s, &parse_ctx, &ebml_master, &ebml_ebml_desc) < 0)
        return 0;
    if (ebml_master.doc_type.start == NULL) {
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] File has EBML header but no doctype."
               " Assuming \"matroska\".\n");
    } else if (bstrcmp(ebml_master.doc_type, bstr0("matroska")) != 0
        && bstrcmp(ebml_master.doc_type, bstr0("webm")) != 0) {
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

    while (1) {
        uint32_t id = ebml_read_id(s, NULL);
        if (s->eof) {
            mp_tmsg(MSGT_DEMUX, MSGL_ERR, "[mkv] Unexpected end of file\n");
            return 0;
        }
        if (id == MATROSKA_ID_CLUSTER) {
            mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] |+ found cluster, headers are "
                   "parsed completely :)\n");
            stream_seek(s, stream_tell(s) - 4);
            break;
        }
        int res = read_header_element(demuxer, id, 0);
        if (res <= -2)
            return 0;
        if (res < 0)
            break;
    }

    display_create_tracks(demuxer);

    /* select video track */
    track = NULL;
    if (demuxer->video->id == -1) {     /* automatically select a video track */
        /* search for a video track that has the 'default' flag set */
        for (int i = 0; i < mkv_d->num_tracks; i++)
            if (mkv_d->tracks[i]->type == MATROSKA_TRACK_VIDEO
                && mkv_d->tracks[i]->default_track) {
                track = mkv_d->tracks[i];
                break;
            }

        if (track == NULL)
            /* no track has the 'default' flag set */
            /* let's take the first video track */
            for (int i = 0; i < mkv_d->num_tracks; i++)
                if (mkv_d->tracks[i]->type == MATROSKA_TRACK_VIDEO
                    && mkv_d->tracks[i]->id >= 0) {
                    track = mkv_d->tracks[i];
                    break;
                }
    } else if (demuxer->video->id != -2)        /* -2 = no video at all */
        track = find_track_by_num(mkv_d, demuxer->video->id,
                                  MATROSKA_TRACK_VIDEO);

    if (track && demuxer->v_streams[track->id]) {
        mp_tmsg(MSGT_DEMUX, MSGL_V, "[mkv] Will play video track %u.\n",
                track->tnum);
        demuxer->video->id = track->id;
        demuxer->video->sh = demuxer->v_streams[track->id];
    } else {
        mp_tmsg(MSGT_DEMUX, MSGL_INFO, "[mkv] No video track found/wanted.\n");
        demuxer->video->id = -2;
    }

    demuxer->audio->id = -2;  // wait for higher-level code to select track

    if (s->end_pos == 0)
        demuxer->seekable = 0;
    else {
        demuxer->movi_start = s->start_pos;
        demuxer->movi_end = s->end_pos;
        demuxer->seekable = 1;
    }

    demuxer->accurate_seek = true;

    return DEMUXER_TYPE_MATROSKA;
}

static int demux_mkv_read_block_lacing(uint8_t *buffer, uint64_t *size,
                                       uint8_t *laces,
                                       uint32_t **all_lace_sizes)
{
    uint32_t total = 0;
    uint32_t *lace_size = NULL;
    uint8_t flags;
    int i;

    *all_lace_sizes = NULL;
    /* lacing flags */
    if (*size < 1)
        goto error;
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
        if (*size < 1)
            goto error;
        *laces = *buffer++;
        (*size)--;
        (*laces)++;
        lace_size = calloc(*laces, sizeof(uint32_t));

        switch ((flags & 0x06) >> 1) {
        case 1:                /* xiph lacing */
            for (i = 0; i < *laces - 1; i++) {
                lace_size[i] = 0;
                do {
                    if (!*size)
                        goto error;
                    lace_size[i] += *buffer;
                    (*size)--;
                } while (*buffer++ == 0xFF);
                if (lace_size[i] > *size - total || total > *size)
                    goto error;
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
            if (num == EBML_UINT_INVALID)
                goto error;
            buffer += l;
            if (*size < l)
                goto error;
            *size -= l;
            if (num > *size)
                goto error;

            total = lace_size[0] = num;
            for (i = 1; i < *laces - 1; i++) {
                int64_t snum = ebml_read_vlen_int(buffer, &l);
                if (snum == EBML_INT_INVALID)
                    goto error;
                buffer += l;
                if (*size < l)
                    goto error;
                *size -= l;
                lace_size[i] = lace_size[i - 1] + snum;
                if (lace_size[i] > *size - total || total > *size)
                    goto error;
                total += lace_size[i];
            }
            lace_size[i] = *size - total;
            break;
        }
        break;
    }
    *all_lace_sizes = lace_size;
    return 0;

 error:
    free(lace_size);
    mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] Bad input [lacing]\n");
    return 1;
}

#define SKIP_BITS(n) buffer<<=n
#define SHOW_BITS(n) ((buffer)>>(32-(n)))

static double real_fix_timestamp(unsigned char *buf, unsigned int timestamp, unsigned int format, int64_t *kf_base, int *kf_pts, double *pts){
  double v_pts;
  unsigned char *s = buf + 1 + (*buf+1)*8;
  uint32_t buffer= (s[0]<<24) + (s[1]<<16) + (s[2]<<8) + s[3];
  unsigned int kf=timestamp;
  int pict_type;

  if(format==mmioFOURCC('R','V','3','0') || format==mmioFOURCC('R','V','4','0')){
    if(format==mmioFOURCC('R','V','3','0')){
      SKIP_BITS(3);
      pict_type= SHOW_BITS(2);
      SKIP_BITS(2 + 7);
    }else{
      SKIP_BITS(1);
      pict_type= SHOW_BITS(2);
      SKIP_BITS(2 + 7 + 3);
    }
    kf= SHOW_BITS(13);  //    kf= 2*SHOW_BITS(12);
//    if(pict_type==0)
    if(pict_type<=1){
      // I frame, sync timestamps:
      *kf_base=(int64_t)timestamp-kf;
      mp_msg(MSGT_DEMUX, MSGL_DBG2,"\nTS: base=%08"PRIX64"\n",*kf_base);
      kf=timestamp;
    } else {
      // P/B frame, merge timestamps:
      int64_t tmp=(int64_t)timestamp-*kf_base;
      kf|=tmp&(~0x1fff);        // combine with packet timestamp
      if(kf<tmp-4096) kf+=8192; else // workaround wrap-around problems
      if(kf>tmp+4096) kf-=8192;
      kf+=*kf_base;
    }
    if(pict_type != 3){ // P || I  frame -> swap timestamps
        unsigned int tmp=kf;
        kf=*kf_pts;
        *kf_pts=tmp;
//      if(kf<=tmp) kf=0;
    }
  }
    v_pts=kf*0.001f;
//    if(pts && (v_pts<*pts || !kf)) v_pts=*pts+frametime;
    if(pts) *pts=v_pts;
    return v_pts;
}

static void handle_realvideo(demuxer_t *demuxer, mkv_track_t *track,
                             uint8_t *buffer, uint32_t size, bool keyframe)
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
    dp->keyframe = keyframe;

    ds_add_packet(demuxer->video, dp);
}

static void handle_realaudio(demuxer_t *demuxer, mkv_track_t *track,
                             uint8_t *buffer, uint32_t size, bool keyframe)
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
                ((sh_audio_t *) demuxer->audio->sh)->wf->nBlockAlign;
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
                dp->keyframe = !x;   // Mark first packet as keyframe
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
        dp->keyframe = keyframe;
        ds_add_packet(demuxer->audio, dp);
    }
}

static int handle_block(demuxer_t *demuxer, uint8_t *block, uint64_t length,
                        uint64_t block_duration, bool keyframe,
                        bool simpleblock)
{
    mkv_demuxer_t *mkv_d = (mkv_demuxer_t *) demuxer->priv;
    mkv_track_t *track = NULL;
    demux_stream_t *ds = NULL;
    uint64_t old_length;
    uint64_t tc;
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
    if (simpleblock)
        keyframe = flags & 0x80;
    if (demux_mkv_read_block_lacing(block, &length, &laces, &lace_size))
        return 0;
    block += old_length - length;

    tc = time * mkv_d->tc_scale + mkv_d->cluster_tc;
    current_pts = tc / 1e9;

    for (i = 0; i < mkv_d->num_tracks; i++)
        if (mkv_d->tracks[i]->tnum == num) {
            track = mkv_d->tracks[i];
            break;
        }
    if (track == NULL) {
        free(lace_size);
        return 1;
    }
    if (track->type == MATROSKA_TRACK_AUDIO
        && track->id == demuxer->audio->id) {
        ds = demuxer->audio;

        if (mkv_d->a_skip_to_keyframe)
            use_this_block = keyframe;
        if (mkv_d->v_skip_to_keyframe)
            use_this_block = 0;

        if (track->fix_i_bps && use_this_block) {
            sh_audio_t *sh = (sh_audio_t *) ds->sh;

            if (block_duration != 0) {
                sh->i_bps = length * 1e9 / block_duration;
                track->fix_i_bps = 0;
            } else if (track->qt_last_a_pts == 0.0)
                track->qt_last_a_pts = current_pts;
            else if (track->qt_last_a_pts != current_pts) {
                sh->i_bps = length / (current_pts - track->qt_last_a_pts);
                track->fix_i_bps = 0;
            }
        }
    } else if (track->type == MATROSKA_TRACK_SUBTITLE
               && track->id == demuxer->sub->id) {
        if (tc < mkv_d->skip_to_timecode && !mkv_d->subtitle_preroll)
            use_this_block = 0;
        if (use_this_block) {
            ds = demuxer->sub;
            if (laces > 1) {
                mp_msg(MSGT_DEMUX, MSGL_WARN, "[mkv] Subtitles use Matroska "
                    "lacing. This is abnormal and not supported.\n");
                use_this_block = 0;
            }
        }
    } else if (tc < mkv_d->skip_to_timecode)
        use_this_block = 0;
    else if (track->type == MATROSKA_TRACK_VIDEO
             && track->id == demuxer->video->id) {
        ds = demuxer->video;
        if (mkv_d->v_skip_to_keyframe)
            use_this_block = keyframe;
    } else
        use_this_block = 0;

    if (use_this_block) {
        mkv_d->last_pts = current_pts;
        mkv_d->last_filepos = demuxer->filepos;

        for (i = 0; i < laces; i++) {
            if (ds == demuxer->video && track->realmedia)
                handle_realvideo(demuxer, track, block, lace_size[i],
                                 keyframe);
            else if (ds == demuxer->audio && track->realmedia)
                handle_realaudio(demuxer, track, block, lace_size[i],
                                 keyframe);
            else {
                int size = lace_size[i];
                demux_packet_t *dp;
                uint8_t *buffer;
                demux_mkv_decode(track, block, &buffer, &size, 1);
                if (buffer) {
                    dp = new_demux_packet(size);
                    memcpy(dp->buffer, buffer, size);
                    if (buffer != block)
                        talloc_free(buffer);
                    dp->keyframe = keyframe;
                    /* If default_duration is 0, assume no pts value is known
                     * for packets after the first one (rather than all pts
                     * values being the same) */
                    if (i == 0 || track->default_duration)
                        dp->pts =
                            mkv_d->last_pts + i * track->default_duration;
                    dp->duration = block_duration / 1e9;
                    ds_add_packet(ds, dp);
                }
            }
            block += lace_size[i];
        }

        if (ds == demuxer->video) {
            mkv_d->v_skip_to_keyframe = 0;
            mkv_d->skip_to_timecode = 0;
            mkv_d->subtitle_preroll = false;
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
            bool keyframe = true;
            uint8_t *block = NULL;

            while (mkv_d->blockgroup_size > 0) {
                switch (ebml_read_id(s, &il)) {
                case MATROSKA_ID_BLOCKDURATION:
                    block_duration = ebml_read_uint(s, &l);
                    if (block_duration == EBML_UINT_INVALID) {
                        free(block);
                        return 0;
                    }
                    block_duration *= mkv_d->tc_scale;
                    break;

                case MATROSKA_ID_BLOCK:
                    block_length = ebml_read_length(s, &tmp);
                    free(block);
                    if (block_length > 500000000)
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
                    if (num)
                        keyframe = false;
                    break;

                case EBML_ID_INVALID:
                    free(block);
                    ebml_resync_cluster(s);
                    goto find_next_cluster;

                default:
                    if (ebml_read_skip_or_resync_cluster(s, &l) != 0)
                        goto find_next_cluster;
                    break;
                }
                mkv_d->blockgroup_size -= l + il;
                mkv_d->cluster_size -= l + il;
            }

            if (block) {
                int res = handle_block(demuxer, block, block_length,
                                       block_duration, keyframe, false);
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
                    if (block_length > 500000000)
                        return 0;
                    block = malloc(block_length);
                    demuxer->filepos = stream_tell(s);
                    if (stream_read(s, block, block_length) !=
                        (int) block_length) {
                        free(block);
                        return 0;
                    }
                    l = tmp + block_length;
                    res = handle_block(demuxer, block, block_length,
                                       block_duration, false, true);
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
                    ebml_resync_cluster(s);
                    goto find_next_cluster;

                default: ;
                    if (ebml_read_skip_or_resync_cluster(s, &l) != 0)
                        goto find_next_cluster;
                    break;
                }
                mkv_d->cluster_size -= l + il;
            }
        }

    find_next_cluster:
        for (;;) {
            uint32_t id = ebml_read_id(s, &il);
            if (id == MATROSKA_ID_CLUSTER)
                break;
            if (s->eof)
                return 0;
            ebml_read_skip_or_resync_cluster(s, NULL);
        }
        mkv_d->cluster_start = stream_tell(s) - il;
        mkv_d->cluster_size = ebml_read_length(s, NULL);
    }

    return 0;
}

static int seek_creating_index(struct demuxer *demuxer, float rel_seek_secs,
                               int flags)
{
    struct mkv_demuxer *mkv_d = demuxer->priv;
    struct stream *s = demuxer->stream;
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
        if ((int64_t) max_filepos > stream_tell(s))
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
                    if (ebml_read_id(s, NULL) == MATROSKA_ID_TIMECODE) {
                        uint64_t tc = ebml_read_uint(s, NULL);
                        tc *= mkv_d->tc_scale;
                        add_cluster_position(mkv_d, start, tc);
                        if (tc >= target_tc_ns)
                            goto enough_index;
                        break;
                    }
                }
            }
            if (s->eof)
                break;
            stream_seek(s, end);
        }
    enough_index:
        if (s->eof)
            stream_reset(s);
    }
    if (!mkv_d->num_cluster_pos) {
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] no target for seek found\n");
        return -1;
    }
    uint64_t cluster_pos = mkv_d->cluster_positions[0].filepos;
    /* Let's find the nearest cluster */
    int64_t min_diff = 0xFFFFFFFFFFFFFFF;
    for (int i = 0; i < mkv_d->num_cluster_pos; i++) {
        int64_t diff = mkv_d->cluster_positions[i].timecode - target_tc_ns;
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
    return 0;
}

static struct mkv_index *seek_with_cues(struct demuxer *demuxer, int seek_id,
                                        int64_t target_timecode, int flags)
{
    struct mkv_demuxer *mkv_d = demuxer->priv;
    struct mkv_index *index = NULL;

    /* Find the entry in the index closest to the target timecode in the
     * give direction. If there are no such entries - we're trying to seek
     * backward from a target time before the first entry or forward from a
     * target time after the last entry - then still seek to the first/last
     * entry if that's further in the direction wanted than mkv_d->last_pts.
     */
    int64_t min_diff = target_timecode - (int64_t)(mkv_d->last_pts * 1e9 + 0.5);
    if (flags & SEEK_BACKWARD)
        min_diff = -min_diff;
    min_diff = FFMAX(min_diff, 1);

    for (int i = 0; i < mkv_d->num_indexes; i++)
        if (seek_id < 0 || mkv_d->indexes[i].tnum == seek_id) {
            int64_t diff =
                target_timecode -
                (int64_t) (mkv_d->indexes[i].timecode * mkv_d->tc_scale);
            if (flags & SEEK_BACKWARD)
                diff = -diff;
            if (diff <= 0) {
                if (min_diff <= 0 && diff <= min_diff)
                    continue;
            } else if (diff >= min_diff)
                continue;
            min_diff = diff;
            index = mkv_d->indexes + i;
        }

    if (index) {        /* We've found an entry. */
        uint64_t seek_pos = index->filepos;
        if (mkv_d->subtitle_preroll) {
            uint64_t prev_target = 0;
            for (int i = 0; i < mkv_d->num_indexes; i++) {
                if (seek_id < 0 || mkv_d->indexes[i].tnum == seek_id) {
                    uint64_t index_pos = mkv_d->indexes[i].filepos;
                    if (index_pos > prev_target && index_pos < seek_pos)
                        prev_target = index_pos;
                }
            }
            if (prev_target)
                seek_pos = prev_target;
        }

        mkv_d->cluster_size = mkv_d->blockgroup_size = 0;
        stream_seek(demuxer->stream, seek_pos);
    }
    return index;
}

static void demux_mkv_seek(demuxer_t *demuxer, float rel_seek_secs,
                           float audio_delay, int flags)
{
    mkv_demuxer_t *mkv_d = demuxer->priv;
    uint64_t v_tnum = -1;
    if (demuxer->video->id >= 0)
        v_tnum = find_track_by_num(mkv_d, demuxer->video->id,
                                   MATROSKA_TRACK_VIDEO)->tnum;
    uint64_t a_tnum = -1;
    if (demuxer->audio->id >= 0)
        a_tnum = find_track_by_num(mkv_d, demuxer->audio->id,
                                   MATROSKA_TRACK_AUDIO)->tnum;
    mkv_d->subtitle_preroll = (flags & SEEK_SUBPREROLL) && demuxer->sub->id >= 0;
    if (!(flags & (SEEK_BACKWARD | SEEK_FORWARD))) {
        if (flags & SEEK_ABSOLUTE || rel_seek_secs < 0)
            flags |= SEEK_BACKWARD;
        else
            flags |= SEEK_FORWARD;
    }
    // Adjust the target a little bit to catch cases where the target position
    // specifies a keyframe with high, but not perfect, precision.
    rel_seek_secs += flags & SEEK_FORWARD ? -0.005 : 0.005;

    if (!(flags & SEEK_FACTOR)) {       /* time in secs */
        mkv_index_t *index = NULL;

        if (!(flags & SEEK_ABSOLUTE))   /* relative seek */
            rel_seek_secs += mkv_d->last_pts;
        rel_seek_secs = FFMAX(rel_seek_secs, 0);
        int64_t target_timecode = rel_seek_secs * 1e9 + 0.5;

        if (mkv_d->indexes == NULL) {   /* no index was found */
            if (seek_creating_index(demuxer, rel_seek_secs, flags) < 0)
                return;
        } else {
            int seek_id = (demuxer->video->id < 0) ?
                a_tnum : v_tnum;
            index = seek_with_cues(demuxer, seek_id, target_timecode, flags);
            if (!index)
                index = seek_with_cues(demuxer, -1, target_timecode, flags);
        }

        if (demuxer->video->id >= 0)
            mkv_d->v_skip_to_keyframe = 1;
        if (flags & SEEK_FORWARD)
            mkv_d->skip_to_timecode = target_timecode;
        else
            mkv_d->skip_to_timecode = index ? index->timecode * mkv_d->tc_scale
                                            : 0;
        mkv_d->a_skip_to_keyframe = 1;

        demux_mkv_fill_buffer(demuxer, NULL);
    } else if ((demuxer->movi_end <= 0) || !(flags & SEEK_ABSOLUTE))
        mp_msg(MSGT_DEMUX, MSGL_V, "[mkv] seek unsupported flags\n");
    else {
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
            if (mkv_d->indexes[i].tnum == v_tnum)
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
        mkv_d->skip_to_timecode = index->timecode * mkv_d->tc_scale;
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

    case DEMUXER_CTRL_SWITCH_AUDIO:;
        int new_aid = *(int *) arg;
        int current_aid = demuxer->audio->id;
        if (current_aid < 0)
            current_aid = -1;
        if (new_aid == -1)  {   // cycle to next
            new_aid = current_aid;
            while (1) {
                new_aid = (new_aid + 2) % (mkv_d->num_audio_tracks + 1) - 1;
                if (new_aid == -1 || demuxer->a_streams[new_aid])
                    break;
            }
        }
        if (new_aid < 0 || new_aid >= mkv_d->num_audio_tracks ||
                !demuxer->a_streams[new_aid])
            new_aid = -2;
        *(int *) arg = new_aid;
        if (current_aid != new_aid)
            ds_free_packs(demuxer->audio);
        demuxer->audio->id = new_aid;
        return DEMUXER_CTRL_OK;

    case DEMUXER_CTRL_SWITCH_VIDEO:;
        int new_vid = *(int *) arg;
        int current_vid = demuxer->video->id;
        if (current_vid < 0)
            current_vid = -1;
        if (new_vid == -1)  {   // cycle to next
            new_vid = current_vid;
            while (1) {
                new_vid = (new_vid + 2) % (mkv_d->num_video_tracks + 1) - 1;
                if (new_vid == -1 || demuxer->v_streams[new_vid])
                    break;
            }
        }
        if (new_vid < 0 || new_vid >= mkv_d->num_video_tracks ||
                !demuxer->v_streams[new_vid])
            new_vid = -2;
        *(int *) arg = new_vid;
        if (current_vid != new_vid)
            ds_free_packs(demuxer->video);
        demuxer->video->id = new_vid;
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
    mkv_free,
    demux_mkv_seek,
    demux_mkv_control
};
