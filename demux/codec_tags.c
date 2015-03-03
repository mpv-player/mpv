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

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/common.h>
#include "codec_tags.h"
#include "stheader.h"
#include "common/av_common.h"

struct mp_codec_tag {
    uint32_t tag;
    const char *codec;
};

static const struct mp_codec_tag mp_codec_tags[] = {
    // Made-up tags used by demux_mkv.c to map codecs.
    // (This is a leftover from MPlayer's codecs.conf mechanism.)
    {MKTAG('p', 'r', '0', '0'), "prores"},
    {MKTAG('H', 'E', 'V', 'C'), "hevc"},
    {MKTAG('R', 'V', '2', '0'), "rv20"},
    {MKTAG('R', 'V', '3', '0'), "rv30"},
    {MKTAG('R', 'V', '4', '0'), "rv40"},
    {MKTAG('R', 'V', '1', '0'), "rv10"},
    {MKTAG('R', 'V', '1', '3'), "rv10"},
    {MKTAG('E', 'A', 'C', '3'), "eac3"},
    {MKTAG('M', 'P', '4', 'A'), "aac"},     // also the QT tag
    {MKTAG('v', 'r', 'b', 's'), "vorbis"},
    {MKTAG('O', 'p', 'u', 's'), "opus"},
    {MKTAG('W', 'V', 'P', 'K'), "wavpack"},
    {MKTAG('T', 'R', 'H', 'D'), "truehd"},
    {MKTAG('f', 'L', 'a', 'C'), "flac"},
    {MKTAG('a', 'L', 'a', 'C'), "alac"},    // also the QT tag
    {MKTAG('2', '8', '_', '8'), "ra_288"},
    {MKTAG('a', 't', 'r', 'c'), "atrac3"},
    {MKTAG('c', 'o', 'o', 'k'), "cook"},
    {MKTAG('d', 'n', 'e', 't'), "ac3"},
    {MKTAG('s', 'i', 'p', 'r'), "sipr"},
    {MKTAG('T', 'T', 'A', '1'), "tta"},
    // Fringe codecs, occur in the wild, but not mapped in FFmpeg.
    {MKTAG('B', 'I', 'K', 'b'), "binkvideo"},
    {MKTAG('B', 'I', 'K', 'f'), "binkvideo"},
    {MKTAG('B', 'I', 'K', 'g'), "binkvideo"},
    {MKTAG('B', 'I', 'K', 'h'), "binkvideo"},
    {MKTAG('B', 'I', 'K', 'i'), "binkvideo"},
    {0}
};

#define HAVE_QT_TAGS (LIBAVFORMAT_VERSION_MICRO >= 100)

static const char *lookup_tag(int type, uint32_t tag)
{
    for (int n = 0; mp_codec_tags[n].codec; n++) {
        if (mp_codec_tags[n].tag == tag)
            return mp_codec_tags[n].codec;
    }

    const struct AVCodecTag *av_tags[3] = {0};
    switch (type) {
    case STREAM_VIDEO: {
        av_tags[0] = avformat_get_riff_video_tags();
#if HAVE_QT_TAGS
        av_tags[1] = avformat_get_mov_video_tags();
#endif
        break;
    }
    case STREAM_AUDIO: {
        av_tags[0] = avformat_get_riff_audio_tags();
#if HAVE_QT_TAGS
        av_tags[1] = avformat_get_mov_audio_tags();
#endif
        break;
    }
    }

    int id = av_codec_get_id(av_tags, tag);
    return id == AV_CODEC_ID_NONE ? NULL : mp_codec_from_av_codec_id(id);
}

static const char *const pcm_le[] = {"pcm_u8", "pcm_s16le", "pcm_s24le", "pcm_s32le"};
static const char *const pcm_be[] = {"pcm_s8", "pcm_s16be", "pcm_s24be", "pcm_s32be"};

static const char *map_audio_pcm_tag(uint32_t tag, int bits)
{
    int bytes = (bits + 7) / 8;
    switch (tag) {
    case 0x0:       // Microsoft PCM
    case 0x1:
    case 0xfffe:    // MS PCM, Extended
        return bytes >= 1 && bytes <= 4 ? pcm_le[bytes - 1] : NULL;
    case 0x3:       // IEEE float
        return bits == 64 ? "pcm_f64le" : "pcm_f32le";
    case 0x20776172:// 'raw '
        return bits == 8 ? "pcm_u8" : "pcm_s16be";
    case MKTAG('t', 'w', 'o', 's'): // demux_mkv.c internal
        return bytes >= 1 && bytes <= 4 ? pcm_be[bytes - 1] : NULL;
    default:
        return NULL;
    }
}

void mp_set_codec_from_tag(struct sh_stream *sh)
{
    sh->codec = lookup_tag(sh->type, sh->format);

    if (sh->audio && sh->audio->bits_per_coded_sample) {
        const char *codec =
            map_audio_pcm_tag(sh->format, sh->audio->bits_per_coded_sample);
        if (codec)
            sh->codec = codec;
    }
}

void mp_set_pcm_codec(struct sh_stream *sh, bool sign, bool is_float, int bits,
                      bool is_be)
{
    // This uses libavcodec pcm codec names, e.g. "pcm_u16le".
    char codec[64] = "pcm_";
    if (is_float) {
        mp_snprintf_cat(codec, sizeof(codec), "f");
    } else {
        mp_snprintf_cat(codec, sizeof(codec), sign ? "s" : "u");
    }
    mp_snprintf_cat(codec, sizeof(codec), "%d", bits);
    if (bits != 8)
        mp_snprintf_cat(codec, sizeof(codec), is_be ? "be" : "le");
    sh->codec = talloc_strdup(sh->audio, codec);
}

static const char *const mimetype_to_codec[][2] = {
    {"image/jpeg",      "mjpeg"},
    {"image/png",       "png"},
    {0}
};

const char *mp_map_mimetype_to_video_codec(const char *mimetype)
{
    if (mimetype) {
        for (int n = 0; mimetype_to_codec[n][0]; n++) {
            if (strcmp(mimetype_to_codec[n][0], mimetype) == 0)
                return mimetype_to_codec[n][1];
        }
    }
    return NULL;
}
