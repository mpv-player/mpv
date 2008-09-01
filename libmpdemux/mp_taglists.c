/*
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

#include "libavformat/avformat.h"
#include "libavformat/riff.h"

static const AVCodecTag mp_wav_tags[] = {
    { CODEC_ID_RA_144,            MKTAG('1', '4', '_', '4')},
    { CODEC_ID_RA_288,            MKTAG('2', '8', '_', '8')},
    { CODEC_ID_ADPCM_4XM,         MKTAG('4', 'X', 'M', 'A')},
    { CODEC_ID_ADPCM_EA,          MKTAG('A', 'D', 'E', 'A')},
    { CODEC_ID_ADPCM_EA_MAXIS_XA, MKTAG('A', 'D', 'X', 'A')},
    { CODEC_ID_ADPCM_IMA_WS,      MKTAG('A', 'I', 'W', 'S')},
    { CODEC_ID_ADPCM_THP,         MKTAG('T', 'H', 'P', 'A')},
    { CODEC_ID_ADPCM_XA,          MKTAG('P', 'S', 'X', 'A')},
    { CODEC_ID_AMR_NB,            MKTAG('n', 'b',   0,   0)},
    { CODEC_ID_COOK,              MKTAG('c', 'o', 'o', 'k')},
    { CODEC_ID_DSICINAUDIO,       MKTAG('D', 'C', 'I', 'A')},
    { CODEC_ID_EAC3,              MKTAG('E', 'A', 'C', '3')},
    { CODEC_ID_INTERPLAY_DPCM,    MKTAG('I', 'N', 'P', 'A')},
    { CODEC_ID_MLP,               MKTAG('M', 'L', 'P', ' ')},
    { CODEC_ID_MUSEPACK7,         MKTAG('M', 'P', 'C', ' ')},
    { CODEC_ID_MUSEPACK8,         MKTAG('M', 'P', 'C', '8')},
    { CODEC_ID_NELLYMOSER,        MKTAG('N', 'E', 'L', 'L')},
    { CODEC_ID_QCELP,             MKTAG('Q', 'c', 'l', 'p')},
    { CODEC_ID_QDM2,              MKTAG('Q', 'D', 'M', '2')},
    { CODEC_ID_ROQ_DPCM,          MKTAG('R', 'o', 'Q', 'A')},
    { CODEC_ID_SHORTEN,           MKTAG('s', 'h', 'r', 'n')},
    { CODEC_ID_SPEEX,             MKTAG('s', 'p', 'x', ' ')},
    { CODEC_ID_TTA,               MKTAG('T', 'T', 'A', '1')},
    { CODEC_ID_WAVPACK,           MKTAG('W', 'V', 'P', 'K')},
    { CODEC_ID_WESTWOOD_SND1,     MKTAG('S', 'N', 'D', '1')},
    { CODEC_ID_XAN_DPCM,          MKTAG('A', 'x', 'a', 'n')},
    { 0, 0 },
};

const struct AVCodecTag *mp_wav_taglists[] = {codec_wav_tags, mp_wav_tags, 0};

static const AVCodecTag mp_wav_override_tags[] = {
    { CODEC_ID_PCM_S8,            MKTAG('t', 'w', 'o', 's')},
    { CODEC_ID_PCM_U8,            1},
    { CODEC_ID_PCM_S16BE,         MKTAG('t', 'w', 'o', 's')},
    { CODEC_ID_PCM_S16LE,         1},
    { CODEC_ID_PCM_S24BE,         MKTAG('i', 'n', '2', '4')},
    { 0, 0 },
};

const struct AVCodecTag *mp_wav_override_taglists[] = {mp_wav_override_tags, 0};

static const AVCodecTag mp_bmp_tags[] = {
    { CODEC_ID_AMV,               MKTAG('A', 'M', 'V', 'V')},
    { CODEC_ID_BETHSOFTVID,       MKTAG('B', 'E', 'T', 'H')},
    { CODEC_ID_BFI,               MKTAG('B', 'F', 'I', 'V')},
    { CODEC_ID_C93,               MKTAG('C', '9', '3', 'V')},
    { CODEC_ID_DSICINVIDEO,       MKTAG('D', 'C', 'I', 'V')},
    { CODEC_ID_DXA,               MKTAG('D', 'X', 'A', '1')},
    { CODEC_ID_FLIC,              MKTAG('F', 'L', 'I', 'C')},
    { CODEC_ID_IDCIN,             MKTAG('I', 'D', 'C', 'I')},
    { CODEC_ID_INTERPLAY_VIDEO,   MKTAG('I', 'N', 'P', 'V')},
    { CODEC_ID_MDEC,              MKTAG('M', 'D', 'E', 'C')},
    { CODEC_ID_MOTIONPIXELS,      MKTAG('M', 'V', 'I', '1')},
    { CODEC_ID_RL2,               MKTAG('R', 'L', '2', 'V')},
    { CODEC_ID_ROQ,               MKTAG('R', 'o', 'Q', 'V')},
    { CODEC_ID_RV10,              MKTAG('R', 'V', '1', '0')},
    { CODEC_ID_RV20,              MKTAG('R', 'V', '2', '0')},
    { CODEC_ID_RV30,              MKTAG('R', 'V', '3', '0')},
    { CODEC_ID_RV40,              MKTAG('R', 'V', '4', '0')},
    { CODEC_ID_THP,               MKTAG('T', 'H', 'P', 'V')},
    { CODEC_ID_TIERTEXSEQVIDEO,   MKTAG('T', 'S', 'E', 'Q')},
    { CODEC_ID_TXD,               MKTAG('T', 'X', 'D', 'V')},
    { CODEC_ID_VP6A,              MKTAG('V', 'P', '6', 'A')},
    { CODEC_ID_VMDVIDEO,          MKTAG('V', 'M', 'D', 'V')},
    { CODEC_ID_WS_VQA,            MKTAG('V', 'Q', 'A', 'V')},
    { CODEC_ID_XAN_WC3,           MKTAG('W', 'C', '3', 'V')},
    { CODEC_ID_NUV,               MKTAG('N', 'U', 'V', '1')},
    { 0, 0 },
};

const struct AVCodecTag *mp_bmp_taglists[] = {codec_bmp_tags, mp_bmp_tags, 0};

