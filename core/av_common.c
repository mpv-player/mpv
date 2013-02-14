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

#include <assert.h>

#include <libavutil/common.h>

#include "config.h"
#include "core/mp_talloc.h"
#include "av_common.h"
#include "codecs.h"


// Copy the codec-related fields from st into avctx. This does not set the
// codec itself, only codec related header data provided by libavformat.
// The goal is to initialize a new decoder with the header data provided by
// libavformat, and unlike avcodec_copy_context(), allow the user to create
// a clean AVCodecContext for a manually selected AVCodec.
// This is strictly for decoding only.
void mp_copy_lav_codec_headers(AVCodecContext *avctx, AVCodecContext *st)
{
    if (st->extradata_size) {
        av_free(avctx->extradata);
        avctx->extradata_size = 0;
        avctx->extradata =
            av_mallocz(st->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE);
        if (avctx->extradata) {
            avctx->extradata_size = st->extradata_size;
            memcpy(avctx->extradata, st->extradata, st->extradata_size);
        }
    }
    avctx->codec_tag                = st->codec_tag;
    avctx->stream_codec_tag         = st->stream_codec_tag;
    avctx->bit_rate                 = st->bit_rate;
    avctx->width                    = st->width;
    avctx->height                   = st->height;
    avctx->pix_fmt                  = st->pix_fmt;
    avctx->sample_aspect_ratio      = st->sample_aspect_ratio;
    avctx->chroma_sample_location   = st->chroma_sample_location;
    avctx->sample_rate              = st->sample_rate;
    avctx->channels                 = st->channels;
    avctx->block_align              = st->block_align;
    avctx->channel_layout           = st->channel_layout;
    avctx->audio_service_type       = st->audio_service_type;
    avctx->bits_per_coded_sample    = st->bits_per_coded_sample;
}

#if !HAVE_AVCODEC_IS_DECODER_API
static int av_codec_is_decoder(AVCodec *codec)
{
    return !!codec->decode;
}
#endif

void mp_add_lavc_decoders(struct mp_decoder_list *list, enum AVMediaType type)
{
    AVCodec *cur = NULL;
    for (;;) {
        cur = av_codec_next(cur);
        if (!cur)
            break;
        if (av_codec_is_decoder(cur) && cur->type == type) {
            struct mp_decoder_entry entry = {
                .family = "lavc",
                .codec = mp_codec_from_av_codec_id(cur->id),
                .decoder = cur->name,
                .desc = cur->long_name,
            };
            assert(entry.family);
            MP_TARRAY_APPEND(list, list->entries, list->num_entries, entry);
        }
    }
}

#if HAVE_AVCODEC_CODEC_DESC_API

int mp_codec_to_av_codec_id(const char *codec)
{

    const AVCodecDescriptor *desc = avcodec_descriptor_get_by_name(codec);
    return desc ? desc->id : CODEC_ID_NONE;
}

const char *mp_codec_from_av_codec_id(int codec_id)
{
    const AVCodecDescriptor *desc = avcodec_descriptor_get(codec_id);
    return desc ? desc->name : NULL;
}

#else

struct mp_av_codec {
    const char *name;
    int codec_id;
};

// Some decoders have a different name from the canonical codec name, for
// example the codec "dts" CODEC_ID_DTS has the decoder named "dca", and
// avcodec_find_decoder_by_name("dts") would return 0. We always want the
// canonical name.
// On newer lavc versions, avcodec_descriptor_get_by_name("dts") will return
// CODEC_ID_DTS, which is what we want, but for older versions we need this
// lookup table.
struct mp_av_codec mp_av_codec_id_list[] = {
    {"ra_144",          CODEC_ID_RA_144},
    {"ra_288",          CODEC_ID_RA_288},
    {"smackaudio",      CODEC_ID_SMACKAUDIO},
    {"dts",             CODEC_ID_DTS},
    {"musepack7",       CODEC_ID_MUSEPACK7},
    {"musepack8",       CODEC_ID_MUSEPACK8},
    {"amr_nb",          CODEC_ID_AMR_NB},
    {"amr_wb",          CODEC_ID_AMR_WB},
    {"adpcm_g722",      CODEC_ID_ADPCM_G722},
    {"adpcm_g726",      CODEC_ID_ADPCM_G726},
    {"westwood_snd1",   CODEC_ID_WESTWOOD_SND1},
    {"mp4als",          CODEC_ID_MP4ALS},
    {"vixl",            CODEC_ID_VIXL},
    {"flv1",            CODEC_ID_FLV1},
    {"msmpeg4v3",       CODEC_ID_MSMPEG4V3},
    {"jpeg2000",        CODEC_ID_JPEG2000},
    {"ulti",            CODEC_ID_ULTI},
    {"smackvideo",      CODEC_ID_SMACKVIDEO},
    {"tscc",            CODEC_ID_TSCC},
    {"cscd",            CODEC_ID_CSCD},
    {"tgv",             CODEC_ID_TGV},
    {"roq",             CODEC_ID_ROQ},
    {"idcin",           CODEC_ID_IDCIN},
    {"ws_vqa",          CODEC_ID_WS_VQA},
    {0},
};

int mp_codec_to_av_codec_id(const char *codec)
{
    for (int n = 0; mp_av_codec_id_list[n].name; n++) {
        if (strcmp(mp_av_codec_id_list[n].name, codec) == 0)
            return mp_av_codec_id_list[n].codec_id;
    }
    AVCodec *avcodec = avcodec_find_decoder_by_name(codec);
    return avcodec ? avcodec->id : CODEC_ID_NONE;
}

const char *mp_codec_from_av_codec_id(int codec_id)
{
    for (int n = 0; mp_av_codec_id_list[n].name; n++) {
        if (mp_av_codec_id_list[n].codec_id == codec_id)
            return mp_av_codec_id_list[n].name;
    }
    AVCodec *avcodec = avcodec_find_decoder(codec_id);
    return avcodec ? avcodec->name : NULL;
}

#endif
