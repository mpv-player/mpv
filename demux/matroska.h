/*
 * CodecID definitions for Matroska files
 *
 * see http://www.matroska.org/technical/specs/codecid/index.html
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

#ifndef MPLAYER_MATROSKA_H
#define MPLAYER_MATROSKA_H

struct timeline;
void build_ordered_chapter_timeline(struct timeline *tl);

#define MKV_A_AAC_2MAIN  "A_AAC/MPEG2/MAIN"
#define MKV_A_AAC_2LC    "A_AAC/MPEG2/LC"
#define MKV_A_AAC_2SBR   "A_AAC/MPEG2/LC/SBR"
#define MKV_A_AAC_2SSR   "A_AAC/MPEG2/SSR"
#define MKV_A_AAC_4MAIN  "A_AAC/MPEG4/MAIN"
#define MKV_A_AAC_4LC    "A_AAC/MPEG4/LC"
#define MKV_A_AAC_4SBR   "A_AAC/MPEG4/LC/SBR"
#define MKV_A_AAC_4SSR   "A_AAC/MPEG4/SSR"
#define MKV_A_AAC_4LTP   "A_AAC/MPEG4/LTP"
#define MKV_A_AAC        "A_AAC"
#define MKV_A_AC3        "A_AC3"
#define MKV_A_DTS        "A_DTS"
#define MKV_A_EAC3       "A_EAC3"
#define MKV_A_MP2        "A_MPEG/L2"
#define MKV_A_MP3        "A_MPEG/L3"
#define MKV_A_PCM        "A_PCM/INT/LIT"
#define MKV_A_PCM_BE     "A_PCM/INT/BIG"
#define MKV_A_PCM_FLT    "A_PCM/FLOAT/IEEE"
#define MKV_A_VORBIS     "A_VORBIS"
#define MKV_A_OPUS       "A_OPUS"
#define MKV_A_OPUS_EXP   "A_OPUS/EXPERIMENTAL"
#define MKV_A_ACM        "A_MS/ACM"
#define MKV_A_REAL28     "A_REAL/28_8"
#define MKV_A_REALATRC   "A_REAL/ATRC"
#define MKV_A_REALCOOK   "A_REAL/COOK"
#define MKV_A_REALDNET   "A_REAL/DNET"
#define MKV_A_REALSIPR   "A_REAL/SIPR"
#define MKV_A_QDMC       "A_QUICKTIME/QDMC"
#define MKV_A_QDMC2      "A_QUICKTIME/QDM2"
#define MKV_A_FLAC       "A_FLAC"
#define MKV_A_ALAC       "A_ALAC"
#define MKV_A_TTA1       "A_TTA1"
#define MKV_A_WAVPACK    "A_WAVPACK4"
#define MKV_A_TRUEHD     "A_TRUEHD"

#define MKV_V_MSCOMP     "V_MS/VFW/FOURCC"
#define MKV_V_REALV10    "V_REAL/RV10"
#define MKV_V_REALV20    "V_REAL/RV20"
#define MKV_V_REALV30    "V_REAL/RV30"
#define MKV_V_REALV40    "V_REAL/RV40"
#define MKV_V_SORENSONV1 "V_SORENSON/V1"
#define MKV_V_SORENSONV2 "V_SORENSON/V2"
#define MKV_V_SORENSONV3 "V_SORENSON/V3"
#define MKV_V_CINEPAK    "V_CINEPAK"
#define MKV_V_QUICKTIME  "V_QUICKTIME"
#define MKV_V_MPEG1      "V_MPEG1"
#define MKV_V_MPEG2      "V_MPEG2"
#define MKV_V_MPEG4_SP   "V_MPEG4/ISO/SP"
#define MKV_V_MPEG4_ASP  "V_MPEG4/ISO/ASP"
#define MKV_V_MPEG4_AP   "V_MPEG4/ISO/AP"
#define MKV_V_MPEG4_AVC  "V_MPEG4/ISO/AVC"
#define MKV_V_THEORA     "V_THEORA"
#define MKV_V_VP8        "V_VP8"
#define MKV_V_VP9        "V_VP9"
#define MKV_V_MJPEG      "V_MJPEG"
#define MKV_V_UNCOMPRESSED "V_UNCOMPRESSED"
#define MKV_V_DIRAC      "V_DIRAC"
#define MKV_V_PRORES     "V_PRORES"
#define MKV_V_HEVC       "V_MPEGH/ISO/HEVC"

#define MKV_S_TEXTASCII  "S_TEXT/ASCII"
#define MKV_S_TEXTUTF8   "S_TEXT/UTF8"
#define MKV_S_TEXTSSA    "S_TEXT/SSA"
#define MKV_S_TEXTASS    "S_TEXT/ASS"
#define MKV_S_VOBSUB     "S_VOBSUB"
#define MKV_S_PGS        "S_HDMV/PGS"
#define MKV_S_SSA        "S_SSA" // Deprecated
#define MKV_S_ASS        "S_ASS" // Deprecated
#define MKV_S_WEBVTT_S   "D_WEBVTT/SUBTITLES"
#define MKV_S_WEBVTT_C   "D_WEBVTT/CAPTIONS"
#define MKV_S_DVB        "S_DVBSUB"

#endif /* MPLAYER_MATROSKA_H */
