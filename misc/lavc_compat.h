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

#pragma once

#include <libavcodec/avcodec.h>

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(61, 13, 100)
enum AVCodecConfig {
    AV_CODEC_CONFIG_PIX_FORMAT,     ///< AVPixelFormat, terminated by AV_PIX_FMT_NONE
    AV_CODEC_CONFIG_FRAME_RATE,     ///< AVRational, terminated by {0, 0}
    AV_CODEC_CONFIG_SAMPLE_RATE,    ///< int, terminated by 0
    AV_CODEC_CONFIG_SAMPLE_FORMAT,  ///< AVSampleFormat, terminated by AV_SAMPLE_FMT_NONE
    AV_CODEC_CONFIG_CHANNEL_LAYOUT, ///< AVChannelLayout, terminated by {0}
    AV_CODEC_CONFIG_COLOR_RANGE,    ///< AVColorRange, terminated by AVCOL_RANGE_UNSPECIFIED
    AV_CODEC_CONFIG_COLOR_SPACE,    ///< AVColorSpace, terminated by AVCOL_SPC_UNSPECIFIED
};
#endif

static inline int mp_avcodec_get_supported_config(const AVCodecContext *avctx,
                                                  const AVCodec *codec,
                                                  enum AVCodecConfig config,
                                                  const void **out_configs)
{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 13, 100)
    return avcodec_get_supported_config(avctx, codec, config, 0, out_configs, NULL);
#else
    const AVCodec *avcodec = codec ? codec : avctx->codec;

    switch (config) {
    case AV_CODEC_CONFIG_PIX_FORMAT:
        *out_configs = avcodec->pix_fmts;
        break;
    case AV_CODEC_CONFIG_FRAME_RATE:
        *out_configs = avcodec->supported_framerates;
        break;
    case AV_CODEC_CONFIG_SAMPLE_RATE:
        *out_configs = avcodec->supported_samplerates;
        break;
    case AV_CODEC_CONFIG_SAMPLE_FORMAT:
        *out_configs = avcodec->sample_fmts;
        break;
    case AV_CODEC_CONFIG_CHANNEL_LAYOUT:
        *out_configs = avcodec->ch_layouts;
        break;
    default:
        abort();  // Not implemented
    }

    return *out_configs ? 0 : -1;
#endif
}
