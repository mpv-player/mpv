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

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/common.h>
#include <libavutil/intreadwrite.h>

#include "codec_tags.h"
#include "stheader.h"
#include "common/av_common.h"

#define HAVE_QT_TAGS (LIBAVFORMAT_VERSION_MICRO >= 100)

static const char *lookup_tag(int type, uint32_t tag)
{
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

static const unsigned char guid_pcm[16] =
    {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};
static const unsigned char guid_float[16] =
    {0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};
// Corresponds to FF_MEDIASUBTYPE_BASE_GUID (plus 4 bytes of padding).
static const unsigned char guid_ffext[16] =
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
     0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};

static void map_audio_pcm_tag(struct mp_codec_params *c)
{
    // MS PCM, Extended
    if (c->codec_tag == 0xfffe && c->extradata_size >= 22) {
        // WAVEFORMATEXTENSIBLE.dwChannelMask
        uint64_t chmask = AV_RL32(c->extradata + 2);
        struct mp_chmap chmap;
        mp_chmap_from_waveext(&chmap, chmask);
        if (c->channels.num == chmap.num)
            c->channels = chmap;

        // WAVEFORMATEXTENSIBLE.SubFormat
        unsigned char *subformat = c->extradata + 6;
        if (memcmp(subformat + 4, guid_ffext + 4, 12) == 0) {
            c->codec_tag = AV_RL32(subformat);
            c->codec = lookup_tag(c->type, c->codec_tag);
        }
        if (memcmp(subformat, guid_pcm, 16) == 0)
            c->codec_tag = 0x0;
        if (memcmp(subformat, guid_float, 16) == 0)
            c->codec_tag = 0x3;

        // Compressed formats might use this.
        c->extradata += 22;
        c->extradata_size += 22;
    }

    int bits = c->bits_per_coded_sample;
    int bytes = (bits + 7) / 8;
    switch (c->codec_tag) {
    case 0x0:       // Microsoft PCM
    case 0x1:
        if (bytes >= 1 && bytes <= 4)
            mp_set_pcm_codec(c, bytes > 1, false, bytes * 8, false);
        break;
    case 0x3:       // IEEE float
        mp_set_pcm_codec(c, true, true, bits == 64 ? 64 : 32, false);
        break;
    }
}

void mp_set_codec_from_tag(struct mp_codec_params *c)
{
    c->codec = lookup_tag(c->type, c->codec_tag);

    if (c->type == STREAM_AUDIO && c->bits_per_coded_sample)
        map_audio_pcm_tag(c);
}

void mp_set_pcm_codec(struct mp_codec_params *c, bool sign, bool is_float,
                      int bits, bool is_be)
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
    c->codec = talloc_strdup(c, codec);
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
            if (strcasecmp(mimetype_to_codec[n][0], mimetype) == 0)
                return mimetype_to_codec[n][1];
        }
    }
    return NULL;
}
