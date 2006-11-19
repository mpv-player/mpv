/*
 * CodecID definitions for Matroska files
 *
 * see http://cvs.corecodec.org/cgi-bin/cvsweb.cgi/~checkout~/matroska/doc/website/specs/codex.html?rev=HEAD&content-type=text/html
 */

#ifndef __MATROSKA_H
#define __MATROSKA_H

#include "demuxer.h"

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
#define MKV_A_MP2        "A_MPEG/L2"
#define MKV_A_MP3        "A_MPEG/L3"
#define MKV_A_PCM        "A_PCM/INT/LIT"
#define MKV_A_PCM_BE     "A_PCM/INT/BIG"
#define MKV_A_VORBIS     "A_VORBIS"
#define MKV_A_ACM        "A_MS/ACM"
#define MKV_A_REAL28     "A_REAL/28_8"
#define MKV_A_REALATRC   "A_REAL/ATRC"
#define MKV_A_REALCOOK   "A_REAL/COOK"
#define MKV_A_REALDNET   "A_REAL/DNET"
#define MKV_A_REALSIPR   "A_REAL/SIPR"
#define MKV_A_QDMC       "A_QUICKTIME/QDMC"
#define MKV_A_QDMC2      "A_QUICKTIME/QDM2"
#define MKV_A_FLAC       "A_FLAC"

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

#define MKV_S_TEXTASCII  "S_TEXT/ASCII"
#define MKV_S_TEXTUTF8   "S_TEXT/UTF8"
#define MKV_S_TEXTSSA    "S_TEXT/SSA"
#define MKV_S_TEXTASS    "S_TEXT/ASS"
#define MKV_S_VOBSUB     "S_VOBSUB"
#define MKV_S_SSA        "S_SSA" // Deprecated
#define MKV_S_ASS        "S_ASS" // Deprecated

int demux_mkv_change_subs(demuxer_t *demuxer, int new_num);
void demux_mkv_get_audio_lang(demuxer_t *demuxer, int track_num, char *lang,
                            int maxlen);
void demux_mkv_get_sub_lang(demuxer_t *demuxer, int track_num, char *lang,
                            int maxlen);

#endif /* __MATROSKA_H */
