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

static const char *lookup_tag(int type, uint32_t tag)
{
    const struct AVCodecTag *av_tags[3] = {0};
    switch (type) {
    case STREAM_VIDEO: {
        av_tags[0] = avformat_get_riff_video_tags();
        av_tags[1] = avformat_get_mov_video_tags();
        break;
    }
    case STREAM_AUDIO: {
        av_tags[0] = avformat_get_riff_audio_tags();
        av_tags[1] = avformat_get_mov_audio_tags();
        break;
    }
    }

    int id = av_codec_get_id(av_tags, tag);
    return id == AV_CODEC_ID_NONE ? NULL : mp_codec_from_av_codec_id(id);
}


/* 
 * As seen in the following page:
 * 
 * https://web.archive.org/web/20220406060153/
 * http://dream.cs.bath.ac.uk/researchdev/wave-ex/bformat.html
 * 
 * Note that the GUID struct in the above citation has its
 * integers encoded in little-endian format, which means that
 * the unsigned short and unsigned long entries need to be
 * byte-flipped for this encoding.
 * 
 * In theory only the first element of this array should be used,
 * however some encoders incorrectly encoded the GUID byte-for-byte
 * and thus the second one exists as a fallback.
 */
static const unsigned char guid_ext_base[][16] = {
    // MEDIASUBTYPE_BASE_GUID
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
     0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71},
     // SUBTYPE_AMBISONIC_B_FORMAT_PCM
    {0x01, 0x00, 0x00, 0x00, 0x21, 0x07, 0xD3, 0x11,
     0x86, 0x44, 0xC8, 0xC1, 0xCA, 0x00, 0x00, 0x00}
};

struct mp_waveformatex_guid {
    const char *codec;
    const unsigned char guid[16];
};

static const struct mp_waveformatex_guid guid_ext_other[] = {
    {"ac3",
        {0x2C, 0x80, 0x6D, 0xE0, 0x46, 0xDB, 0xCF, 0x11,
         0xB4, 0xD1, 0x00, 0x80, 0x5F, 0x6C, 0xBB, 0xEA}},
    {"adpcm_agm",
        {0x82, 0xEC, 0x1F, 0x6A, 0xCA, 0xDB, 0x19, 0x45,
         0xBD, 0xE7, 0x56, 0xD3, 0xB3, 0xEF, 0x98, 0x1D}},
    {"atrac3p",
        {0xBF, 0xAA, 0x23, 0xE9, 0x58, 0xCB, 0x71, 0x44,
         0xA1, 0x19, 0xFF, 0xFA, 0x01, 0xE4, 0xCE, 0x62}},
    {"atrac9",
        {0xD2, 0x42, 0xE1, 0x47, 0xBA, 0x36, 0x8D, 0x4D,
         0x88, 0xFC, 0x61, 0x65, 0x4F, 0x8C, 0x83, 0x6C}},
    {"dfpwm",
        {0x3A, 0xC1, 0xFA, 0x38, 0x81, 0x1D, 0x43, 0x61,
         0xA4, 0x0D, 0xCE, 0x53, 0xCA, 0x60, 0x7C, 0xD1}},
    {"eac3",
        {0xAF, 0x87, 0xFB, 0xA7, 0x02, 0x2D, 0xFB, 0x42,
         0xA4, 0xD4, 0x05, 0xCD, 0x93, 0x84, 0x3B, 0xDD}},
    {"mp2",
        {0x2B, 0x80, 0x6D, 0xE0, 0x46, 0xDB, 0xCF, 0x11,
         0xB4, 0xD1, 0x00, 0x80, 0x5F, 0x6C, 0xBB, 0xEA}}
};

static void map_audio_pcm_tag(struct mp_codec_params *c)
{
    // MS PCM, Extended
    if (c->codec_tag == 0xfffe && c->extradata_size >= 22) {
        // WAVEFORMATEXTENSIBLE.wBitsPerSample
        int bits_per_sample = AV_RL16(c->extradata);
        if (bits_per_sample)
            c->bits_per_coded_sample = bits_per_sample;

        // WAVEFORMATEXTENSIBLE.dwChannelMask
        uint64_t chmask = AV_RL32(c->extradata + 2);
        struct mp_chmap chmap;
        mp_chmap_from_waveext(&chmap, chmask);
        if (c->channels.num == chmap.num)
            c->channels = chmap;

        // WAVEFORMATEXTENSIBLE.SubFormat
        unsigned char *subformat = c->extradata + 6;
        for (int i = 0; i < MP_ARRAY_SIZE(guid_ext_base); i++) {
            if (memcmp(subformat + 4, guid_ext_base[i] + 4, 12) == 0) {
                c->codec_tag = AV_RL32(subformat);
                c->codec = lookup_tag(c->type, c->codec_tag);
                break;
            }
        }

        // extra subformat, not a base one
        if (c->codec_tag == 0xfffe) {
            for (int i = 0; i < MP_ARRAY_SIZE(guid_ext_other); i++) {
                if (memcmp(subformat, &guid_ext_other[i].guid, 16) == 0) {
                    c->codec = guid_ext_other[i].codec;
                    c->codec_tag = mp_codec_to_av_codec_id(c->codec);
                    break;
                }
            }
        }

        // Compressed formats might use this.
        c->extradata += 22;
        c->extradata_size -= 22;
    }

    int bits = c->bits_per_coded_sample;
    if (!bits)
        return;

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
    if (c->type == STREAM_AUDIO)
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
    {"image/apng",      "apng"},
    {"image/avif",      "av1"},
    {"image/bmp",       "bmp"},
    {"image/gif",       "gif"},
    {"image/jpeg",      "mjpeg"},
    {"image/jxl",       "jpegxl"},
    {"image/png",       "png"},
    {"image/tiff",      "tiff"},
    {"image/webp",      "webp"},
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
