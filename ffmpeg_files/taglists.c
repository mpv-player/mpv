/*
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


/* This file contains:
 * - the tables ff_codec_bmp_tags and ff_codec_wav_tags from FFmpeg's
 *   libavformat/riff.c, renamed to have an extra mp_ prefix, and with
 *   libavcodec version check added around CODEC_ID_ macros only available
 *   in latest libavcodec versions
 * - an implementation of av_codec_get_tag and av_codec_get_id from
 *   libavformat/utils.c, renamed to have an extra mp_ prefix
 */


const struct mp_AVCodecTag mp_ff_codec_bmp_tags[] = {
    { CODEC_ID_H264,         MKTAG('H', '2', '6', '4') },
    { CODEC_ID_H264,         MKTAG('h', '2', '6', '4') },
    { CODEC_ID_H264,         MKTAG('X', '2', '6', '4') },
    { CODEC_ID_H264,         MKTAG('x', '2', '6', '4') },
    { CODEC_ID_H264,         MKTAG('a', 'v', 'c', '1') },
    { CODEC_ID_H264,         MKTAG('V', 'S', 'S', 'H') },
    { CODEC_ID_H263,         MKTAG('H', '2', '6', '3') },
    { CODEC_ID_H263,         MKTAG('X', '2', '6', '3') },
    { CODEC_ID_H263,         MKTAG('T', '2', '6', '3') },
    { CODEC_ID_H263,         MKTAG('L', '2', '6', '3') },
    { CODEC_ID_H263,         MKTAG('V', 'X', '1', 'K') },
    { CODEC_ID_H263,         MKTAG('Z', 'y', 'G', 'o') },
    { CODEC_ID_H263P,        MKTAG('H', '2', '6', '3') },
    { CODEC_ID_H263I,        MKTAG('I', '2', '6', '3') }, /* intel h263 */
    { CODEC_ID_H261,         MKTAG('H', '2', '6', '1') },
    { CODEC_ID_H263P,        MKTAG('U', '2', '6', '3') },
    { CODEC_ID_H263P,        MKTAG('v', 'i', 'v', '1') },
    { CODEC_ID_MPEG4,        MKTAG('F', 'M', 'P', '4') },
    { CODEC_ID_MPEG4,        MKTAG('D', 'I', 'V', 'X') },
    { CODEC_ID_MPEG4,        MKTAG('D', 'X', '5', '0') },
    { CODEC_ID_MPEG4,        MKTAG('X', 'V', 'I', 'D') },
    { CODEC_ID_MPEG4,        MKTAG('M', 'P', '4', 'S') },
    { CODEC_ID_MPEG4,        MKTAG('M', '4', 'S', '2') },
    { CODEC_ID_MPEG4,        MKTAG( 4 ,  0 ,  0 ,  0 ) }, /* some broken avi use this */
    { CODEC_ID_MPEG4,        MKTAG('D', 'I', 'V', '1') },
    { CODEC_ID_MPEG4,        MKTAG('B', 'L', 'Z', '0') },
    { CODEC_ID_MPEG4,        MKTAG('m', 'p', '4', 'v') },
    { CODEC_ID_MPEG4,        MKTAG('U', 'M', 'P', '4') },
    { CODEC_ID_MPEG4,        MKTAG('W', 'V', '1', 'F') },
    { CODEC_ID_MPEG4,        MKTAG('S', 'E', 'D', 'G') },
    { CODEC_ID_MPEG4,        MKTAG('R', 'M', 'P', '4') },
    { CODEC_ID_MPEG4,        MKTAG('3', 'I', 'V', '2') },
    { CODEC_ID_MPEG4,        MKTAG('W', 'A', 'W', 'V') }, /* WaWv MPEG-4 Video Codec */
    { CODEC_ID_MPEG4,        MKTAG('F', 'F', 'D', 'S') },
    { CODEC_ID_MPEG4,        MKTAG('F', 'V', 'F', 'W') },
    { CODEC_ID_MPEG4,        MKTAG('D', 'C', 'O', 'D') },
    { CODEC_ID_MPEG4,        MKTAG('M', 'V', 'X', 'M') },
    { CODEC_ID_MPEG4,        MKTAG('P', 'M', '4', 'V') },
    { CODEC_ID_MPEG4,        MKTAG('S', 'M', 'P', '4') },
    { CODEC_ID_MPEG4,        MKTAG('D', 'X', 'G', 'M') },
    { CODEC_ID_MPEG4,        MKTAG('V', 'I', 'D', 'M') },
    { CODEC_ID_MPEG4,        MKTAG('M', '4', 'T', '3') },
    { CODEC_ID_MPEG4,        MKTAG('G', 'E', 'O', 'X') },
    { CODEC_ID_MPEG4,        MKTAG('H', 'D', 'X', '4') }, /* flipped video */
    { CODEC_ID_MPEG4,        MKTAG('D', 'M', 'K', '2') },
    { CODEC_ID_MPEG4,        MKTAG('D', 'I', 'G', 'I') },
    { CODEC_ID_MPEG4,        MKTAG('I', 'N', 'M', 'C') },
    { CODEC_ID_MPEG4,        MKTAG('E', 'P', 'H', 'V') }, /* Ephv MPEG-4 */
    { CODEC_ID_MPEG4,        MKTAG('E', 'M', '4', 'A') },
    { CODEC_ID_MPEG4,        MKTAG('M', '4', 'C', 'C') }, /* Divio MPEG-4 */
    { CODEC_ID_MPEG4,        MKTAG('S', 'N', '4', '0') },
    { CODEC_ID_MPEG4,        MKTAG('V', 'S', 'P', 'X') },
    { CODEC_ID_MPEG4,        MKTAG('U', 'L', 'D', 'X') },
    { CODEC_ID_MPEG4,        MKTAG('G', 'E', 'O', 'V') },
    { CODEC_ID_MPEG4,        MKTAG('S', 'I', 'P', 'P') }, /* Samsung SHR-6040 */
    { CODEC_ID_MSMPEG4V3,    MKTAG('M', 'P', '4', '3') },
    { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '3') },
    { CODEC_ID_MSMPEG4V3,    MKTAG('M', 'P', 'G', '3') },
    { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '5') },
    { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '6') },
    { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '4') },
    { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'V', 'X', '3') },
    { CODEC_ID_MSMPEG4V3,    MKTAG('A', 'P', '4', '1') },
    { CODEC_ID_MSMPEG4V3,    MKTAG('C', 'O', 'L', '1') },
    { CODEC_ID_MSMPEG4V3,    MKTAG('C', 'O', 'L', '0') },
    { CODEC_ID_MSMPEG4V2,    MKTAG('M', 'P', '4', '2') },
    { CODEC_ID_MSMPEG4V2,    MKTAG('D', 'I', 'V', '2') },
    { CODEC_ID_MSMPEG4V1,    MKTAG('M', 'P', 'G', '4') },
    { CODEC_ID_MSMPEG4V1,    MKTAG('M', 'P', '4', '1') },
    { CODEC_ID_WMV1,         MKTAG('W', 'M', 'V', '1') },
    { CODEC_ID_WMV2,         MKTAG('W', 'M', 'V', '2') },
    { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 's', 'd') },
    { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', 'd') },
    { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', '1') },
    { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 's', 'l') },
    { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', '2', '5') },
    { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', '5', '0') },
    { CODEC_ID_DVVIDEO,      MKTAG('c', 'd', 'v', 'c') }, /* Canopus DV */
    { CODEC_ID_DVVIDEO,      MKTAG('C', 'D', 'V', 'H') }, /* Canopus DV */
    { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'c', ' ') },
    { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'c', 's') },
    { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', '1') },
    { CODEC_ID_MPEG1VIDEO,   MKTAG('m', 'p', 'g', '1') },
    { CODEC_ID_MPEG1VIDEO,   MKTAG('m', 'p', 'g', '2') },
    { CODEC_ID_MPEG2VIDEO,   MKTAG('m', 'p', 'g', '2') },
    { CODEC_ID_MPEG2VIDEO,   MKTAG('M', 'P', 'E', 'G') },
    { CODEC_ID_MPEG1VIDEO,   MKTAG('P', 'I', 'M', '1') },
    { CODEC_ID_MPEG2VIDEO,   MKTAG('P', 'I', 'M', '2') },
    { CODEC_ID_MPEG1VIDEO,   MKTAG('V', 'C', 'R', '2') },
    { CODEC_ID_MPEG1VIDEO,   MKTAG( 1 ,  0 ,  0 ,  16) },
    { CODEC_ID_MPEG2VIDEO,   MKTAG( 2 ,  0 ,  0 ,  16) },
    { CODEC_ID_MPEG4,        MKTAG( 4 ,  0 ,  0 ,  16) },
    { CODEC_ID_MPEG2VIDEO,   MKTAG('D', 'V', 'R', ' ') },
    { CODEC_ID_MPEG2VIDEO,   MKTAG('M', 'M', 'E', 'S') },
    { CODEC_ID_MPEG2VIDEO,   MKTAG('L', 'M', 'P', '2') }, /* Lead MPEG2 in avi */
    { CODEC_ID_MPEG2VIDEO,   MKTAG('s', 'l', 'i', 'f') },
    { CODEC_ID_MPEG2VIDEO,   MKTAG('E', 'M', '2', 'V') },
    { CODEC_ID_MPEG2VIDEO,   MKTAG('M', '7', '0', '1') }, /* Matrox MPEG2 intra-only */
    { CODEC_ID_MJPEG,        MKTAG('M', 'J', 'P', 'G') },
    { CODEC_ID_MJPEG,        MKTAG('L', 'J', 'P', 'G') },
    { CODEC_ID_MJPEG,        MKTAG('d', 'm', 'b', '1') },
    { CODEC_ID_MJPEG,        MKTAG('m', 'j', 'p', 'a') },
    { CODEC_ID_LJPEG,        MKTAG('L', 'J', 'P', 'G') },
    { CODEC_ID_MJPEG,        MKTAG('J', 'P', 'G', 'L') }, /* Pegasus lossless JPEG */
    { CODEC_ID_JPEGLS,       MKTAG('M', 'J', 'L', 'S') }, /* JPEG-LS custom FOURCC for avi - encoder */
    { CODEC_ID_JPEGLS,       MKTAG('M', 'J', 'P', 'G') },
    { CODEC_ID_MJPEG,        MKTAG('M', 'J', 'L', 'S') }, /* JPEG-LS custom FOURCC for avi - decoder */
    { CODEC_ID_MJPEG,        MKTAG('j', 'p', 'e', 'g') },
    { CODEC_ID_MJPEG,        MKTAG('I', 'J', 'P', 'G') },
    { CODEC_ID_MJPEG,        MKTAG('A', 'V', 'R', 'n') },
    { CODEC_ID_MJPEG,        MKTAG('A', 'C', 'D', 'V') },
    { CODEC_ID_MJPEG,        MKTAG('Q', 'I', 'V', 'G') },
    { CODEC_ID_MJPEG,        MKTAG('S', 'L', 'M', 'J') }, /* SL M-JPEG */
    { CODEC_ID_MJPEG,        MKTAG('C', 'J', 'P', 'G') }, /* Creative Webcam JPEG */
    { CODEC_ID_MJPEG,        MKTAG('I', 'J', 'L', 'V') }, /* Intel JPEG Library Video Codec */
    { CODEC_ID_MJPEG,        MKTAG('M', 'V', 'J', 'P') }, /* Midvid JPEG Video Codec */
    { CODEC_ID_MJPEG,        MKTAG('A', 'V', 'I', '1') },
    { CODEC_ID_MJPEG,        MKTAG('A', 'V', 'I', '2') },
    { CODEC_ID_MJPEG,        MKTAG('M', 'T', 'S', 'J') },
    { CODEC_ID_MJPEG,        MKTAG('Z', 'J', 'P', 'G') }, /* Paradigm Matrix M-JPEG Codec */
    { CODEC_ID_HUFFYUV,      MKTAG('H', 'F', 'Y', 'U') },
    { CODEC_ID_FFVHUFF,      MKTAG('F', 'F', 'V', 'H') },
    { CODEC_ID_CYUV,         MKTAG('C', 'Y', 'U', 'V') },
    { CODEC_ID_RAWVIDEO,     MKTAG( 0 ,  0 ,  0 ,  0 ) },
    { CODEC_ID_RAWVIDEO,     MKTAG( 3 ,  0 ,  0 ,  0 ) },
    { CODEC_ID_RAWVIDEO,     MKTAG('I', '4', '2', '0') },
    { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'U', 'Y', '2') },
    { CODEC_ID_RAWVIDEO,     MKTAG('Y', '4', '2', '2') },
    { CODEC_ID_RAWVIDEO,     MKTAG('V', '4', '2', '2') },
    { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'U', 'N', 'V') },
    { CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'N', 'V') },
    { CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'N', 'Y') },
    { CODEC_ID_RAWVIDEO,     MKTAG('u', 'y', 'v', '1') },
    { CODEC_ID_RAWVIDEO,     MKTAG('2', 'V', 'u', '1') },
    { CODEC_ID_RAWVIDEO,     MKTAG('2', 'v', 'u', 'y') },
    { CODEC_ID_RAWVIDEO,     MKTAG('y', 'u', 'v', 's') },
    { CODEC_ID_RAWVIDEO,     MKTAG('P', '4', '2', '2') },
    { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'V', '1', '2') },
    { CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'V', 'Y') },
    { CODEC_ID_RAWVIDEO,     MKTAG('V', 'Y', 'U', 'Y') },
    { CODEC_ID_RAWVIDEO,     MKTAG('I', 'Y', 'U', 'V') },
    { CODEC_ID_RAWVIDEO,     MKTAG('Y', '8', '0', '0') },
    { CODEC_ID_RAWVIDEO,     MKTAG('H', 'D', 'Y', 'C') },
    { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'V', 'U', '9') },
    { CODEC_ID_RAWVIDEO,     MKTAG('V', 'D', 'T', 'Z') }, /* SoftLab-NSK VideoTizer */
    { CODEC_ID_RAWVIDEO,     MKTAG('Y', '4', '1', '1') },
    { CODEC_ID_RAWVIDEO,     MKTAG('N', 'V', '1', '2') },
    { CODEC_ID_RAWVIDEO,     MKTAG('N', 'V', '2', '1') },
    { CODEC_ID_RAWVIDEO,     MKTAG('Y', '4', '1', 'B') },
    { CODEC_ID_RAWVIDEO,     MKTAG('Y', '4', '2', 'B') },
    { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'U', 'V', '9') },
    { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'V', 'U', '9') },
    { CODEC_ID_FRWU,         MKTAG('F', 'R', 'W', 'U') },
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52, 89, 0)
    { CODEC_ID_R10K,         MKTAG('R', '1', '0', 'k') },
#endif
    { CODEC_ID_R210,         MKTAG('r', '2', '1', '0') },
    { CODEC_ID_V210,         MKTAG('v', '2', '1', '0') },
    { CODEC_ID_INDEO3,       MKTAG('I', 'V', '3', '1') },
    { CODEC_ID_INDEO3,       MKTAG('I', 'V', '3', '2') },
    { CODEC_ID_INDEO4,       MKTAG('I', 'V', '4', '1') },
    { CODEC_ID_INDEO5,       MKTAG('I', 'V', '5', '0') },
    { CODEC_ID_VP3,          MKTAG('V', 'P', '3', '1') },
    { CODEC_ID_VP3,          MKTAG('V', 'P', '3', '0') },
    { CODEC_ID_VP5,          MKTAG('V', 'P', '5', '0') },
    { CODEC_ID_VP6,          MKTAG('V', 'P', '6', '0') },
    { CODEC_ID_VP6,          MKTAG('V', 'P', '6', '1') },
    { CODEC_ID_VP6,          MKTAG('V', 'P', '6', '2') },
    { CODEC_ID_VP6F,         MKTAG('V', 'P', '6', 'F') },
    { CODEC_ID_VP6F,         MKTAG('F', 'L', 'V', '4') },
    { CODEC_ID_VP8,          MKTAG('V', 'P', '8', '0') },
    { CODEC_ID_ASV1,         MKTAG('A', 'S', 'V', '1') },
    { CODEC_ID_ASV2,         MKTAG('A', 'S', 'V', '2') },
    { CODEC_ID_VCR1,         MKTAG('V', 'C', 'R', '1') },
    { CODEC_ID_FFV1,         MKTAG('F', 'F', 'V', '1') },
    { CODEC_ID_XAN_WC4,      MKTAG('X', 'x', 'a', 'n') },
    { CODEC_ID_MIMIC,        MKTAG('L', 'M', '2', '0') },
    { CODEC_ID_MSRLE,        MKTAG('m', 'r', 'l', 'e') },
    { CODEC_ID_MSRLE,        MKTAG( 1 ,  0 ,  0 ,  0 ) },
    { CODEC_ID_MSRLE,        MKTAG( 2 ,  0 ,  0 ,  0 ) },
    { CODEC_ID_MSVIDEO1,     MKTAG('M', 'S', 'V', 'C') },
    { CODEC_ID_MSVIDEO1,     MKTAG('m', 's', 'v', 'c') },
    { CODEC_ID_MSVIDEO1,     MKTAG('C', 'R', 'A', 'M') },
    { CODEC_ID_MSVIDEO1,     MKTAG('c', 'r', 'a', 'm') },
    { CODEC_ID_MSVIDEO1,     MKTAG('W', 'H', 'A', 'M') },
    { CODEC_ID_MSVIDEO1,     MKTAG('w', 'h', 'a', 'm') },
    { CODEC_ID_CINEPAK,      MKTAG('c', 'v', 'i', 'd') },
    { CODEC_ID_TRUEMOTION1,  MKTAG('D', 'U', 'C', 'K') },
    { CODEC_ID_TRUEMOTION1,  MKTAG('P', 'V', 'E', 'Z') },
    { CODEC_ID_MSZH,         MKTAG('M', 'S', 'Z', 'H') },
    { CODEC_ID_ZLIB,         MKTAG('Z', 'L', 'I', 'B') },
    { CODEC_ID_SNOW,         MKTAG('S', 'N', 'O', 'W') },
    { CODEC_ID_4XM,          MKTAG('4', 'X', 'M', 'V') },
    { CODEC_ID_FLV1,         MKTAG('F', 'L', 'V', '1') },
    { CODEC_ID_FLASHSV,      MKTAG('F', 'S', 'V', '1') },
    { CODEC_ID_SVQ1,         MKTAG('s', 'v', 'q', '1') },
    { CODEC_ID_TSCC,         MKTAG('t', 's', 'c', 'c') },
    { CODEC_ID_ULTI,         MKTAG('U', 'L', 'T', 'I') },
    { CODEC_ID_VIXL,         MKTAG('V', 'I', 'X', 'L') },
    { CODEC_ID_QPEG,         MKTAG('Q', 'P', 'E', 'G') },
    { CODEC_ID_QPEG,         MKTAG('Q', '1', '.', '0') },
    { CODEC_ID_QPEG,         MKTAG('Q', '1', '.', '1') },
    { CODEC_ID_WMV3,         MKTAG('W', 'M', 'V', '3') },
    { CODEC_ID_WMV3,         MKTAG('W', 'M', 'V', 'P') },
    { CODEC_ID_VC1,          MKTAG('W', 'V', 'C', '1') },
    { CODEC_ID_VC1,          MKTAG('W', 'M', 'V', 'A') },
    { CODEC_ID_LOCO,         MKTAG('L', 'O', 'C', 'O') },
    { CODEC_ID_WNV1,         MKTAG('W', 'N', 'V', '1') },
    { CODEC_ID_AASC,         MKTAG('A', 'A', 'S', 'C') },
    { CODEC_ID_INDEO2,       MKTAG('R', 'T', '2', '1') },
    { CODEC_ID_FRAPS,        MKTAG('F', 'P', 'S', '1') },
    { CODEC_ID_THEORA,       MKTAG('t', 'h', 'e', 'o') },
    { CODEC_ID_TRUEMOTION2,  MKTAG('T', 'M', '2', '0') },
    { CODEC_ID_CSCD,         MKTAG('C', 'S', 'C', 'D') },
    { CODEC_ID_ZMBV,         MKTAG('Z', 'M', 'B', 'V') },
    { CODEC_ID_KMVC,         MKTAG('K', 'M', 'V', 'C') },
    { CODEC_ID_CAVS,         MKTAG('C', 'A', 'V', 'S') },
    { CODEC_ID_JPEG2000,     MKTAG('M', 'J', '2', 'C') },
    { CODEC_ID_VMNC,         MKTAG('V', 'M', 'n', 'c') },
    { CODEC_ID_TARGA,        MKTAG('t', 'g', 'a', ' ') },
    { CODEC_ID_PNG,          MKTAG('M', 'P', 'N', 'G') },
    { CODEC_ID_PNG,          MKTAG('P', 'N', 'G', '1') },
    { CODEC_ID_CLJR,         MKTAG('c', 'l', 'j', 'r') },
    { CODEC_ID_DIRAC,        MKTAG('d', 'r', 'a', 'c') },
    { CODEC_ID_RPZA,         MKTAG('a', 'z', 'p', 'r') },
    { CODEC_ID_RPZA,         MKTAG('R', 'P', 'Z', 'A') },
    { CODEC_ID_RPZA,         MKTAG('r', 'p', 'z', 'a') },
    { CODEC_ID_SP5X,         MKTAG('S', 'P', '5', '4') },
    { CODEC_ID_AURA,         MKTAG('A', 'U', 'R', 'A') },
    { CODEC_ID_AURA2,        MKTAG('A', 'U', 'R', '2') },
    { CODEC_ID_DPX,          MKTAG('d', 'p', 'x', ' ') },
    { CODEC_ID_KGV1,         MKTAG('K', 'G', 'V', '1') },
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52, 108, 0)
    { CODEC_ID_LAGARITH,     MKTAG('L', 'A', 'G', 'S') },
#endif
    { CODEC_ID_NONE,         0 }
};

const struct mp_AVCodecTag mp_ff_codec_wav_tags[] = {
    { CODEC_ID_PCM_S16LE,       0x0001 },
    { CODEC_ID_PCM_U8,          0x0001 }, /* must come after s16le in this list */
    { CODEC_ID_PCM_S24LE,       0x0001 },
    { CODEC_ID_PCM_S32LE,       0x0001 },
    { CODEC_ID_ADPCM_MS,        0x0002 },
    { CODEC_ID_PCM_F32LE,       0x0003 },
    { CODEC_ID_PCM_F64LE,       0x0003 }, /* must come after f32le in this list */
    { CODEC_ID_PCM_ALAW,        0x0006 },
    { CODEC_ID_PCM_MULAW,       0x0007 },
    { CODEC_ID_WMAVOICE,        0x000A },
    { CODEC_ID_ADPCM_IMA_WAV,   0x0011 },
    { CODEC_ID_PCM_ZORK,        0x0011 }, /* must come after adpcm_ima_wav in this list */
    { CODEC_ID_ADPCM_YAMAHA,    0x0020 },
    { CODEC_ID_TRUESPEECH,      0x0022 },
    { CODEC_ID_GSM_MS,          0x0031 },
    { CODEC_ID_ADPCM_G726,      0x0045 },
    { CODEC_ID_MP2,             0x0050 },
    { CODEC_ID_MP3,             0x0055 },
    { CODEC_ID_AMR_NB,          0x0057 },
    { CODEC_ID_AMR_WB,          0x0058 },
    { CODEC_ID_ADPCM_IMA_DK4,   0x0061 },  /* rogue format number */
    { CODEC_ID_ADPCM_IMA_DK3,   0x0062 },  /* rogue format number */
    { CODEC_ID_ADPCM_IMA_WAV,   0x0069 },
    { CODEC_ID_VOXWARE,         0x0075 },
    { CODEC_ID_AAC,             0x00ff },
    { CODEC_ID_SIPR,            0x0130 },
    { CODEC_ID_WMAV1,           0x0160 },
    { CODEC_ID_WMAV2,           0x0161 },
    { CODEC_ID_WMAPRO,          0x0162 },
    { CODEC_ID_WMALOSSLESS,     0x0163 },
    { CODEC_ID_ADPCM_CT,        0x0200 },
    { CODEC_ID_ATRAC3,          0x0270 },
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52, 88, 0)
    { CODEC_ID_ADPCM_G722,      0x028F },
#endif
    { CODEC_ID_IMC,             0x0401 },
    { CODEC_ID_GSM_MS,          0x1500 },
    { CODEC_ID_TRUESPEECH,      0x1501 },
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52, 94, 0)
    { CODEC_ID_AAC_LATM,        0x1602 },
#endif
    { CODEC_ID_AC3,             0x2000 },
    { CODEC_ID_DTS,             0x2001 },
    { CODEC_ID_SONIC,           0x2048 },
    { CODEC_ID_SONIC_LS,        0x2048 },
    { CODEC_ID_PCM_MULAW,       0x6c75 },
    { CODEC_ID_AAC,             0x706d },
    { CODEC_ID_AAC,             0x4143 },
    { CODEC_ID_FLAC,            0xF1AC },
    { CODEC_ID_ADPCM_SWF,       ('S'<<8)+'F' },
    { CODEC_ID_VORBIS,          ('V'<<8)+'o' }, //HACK/FIXME, does vorbis in WAV/AVI have an (in)official id?

    /* FIXME: All of the IDs below are not 16 bit and thus illegal. */
    // for NuppelVideo (nuv.c)
    { CODEC_ID_PCM_S16LE, MKTAG('R', 'A', 'W', 'A') },
    { CODEC_ID_MP3,       MKTAG('L', 'A', 'M', 'E') },
    { CODEC_ID_MP3,       MKTAG('M', 'P', '3', ' ') },
    { CODEC_ID_NONE,      0 },
};

static unsigned int ff_codec_get_tag(const struct mp_AVCodecTag *tags, int id)
{
    while (tags->id != CODEC_ID_NONE) {
        if (tags->id == id)
            return tags->tag;
        tags++;
    }
    return 0;
}

static enum CodecID ff_codec_get_id(const struct mp_AVCodecTag *tags, unsigned int tag)
{
    int i;
    for(i=0; tags[i].id != CODEC_ID_NONE;i++) {
        if(tag == tags[i].tag)
            return tags[i].id;
    }
    for(i=0; tags[i].id != CODEC_ID_NONE; i++) {
        if(   toupper((tag >> 0)&0xFF) == toupper((tags[i].tag >> 0)&0xFF)
           && toupper((tag >> 8)&0xFF) == toupper((tags[i].tag >> 8)&0xFF)
           && toupper((tag >>16)&0xFF) == toupper((tags[i].tag >>16)&0xFF)
           && toupper((tag >>24)&0xFF) == toupper((tags[i].tag >>24)&0xFF))
            return tags[i].id;
    }
    return CODEC_ID_NONE;
}

unsigned int mp_av_codec_get_tag(const struct mp_AVCodecTag * const *tags, enum CodecID id)
{
    int i;
    for(i=0; tags && tags[i]; i++){
        int tag= ff_codec_get_tag(tags[i], id);
        if(tag) return tag;
    }
    return 0;
}

enum CodecID mp_av_codec_get_id(const struct mp_AVCodecTag * const *tags, unsigned int tag)
{
    int i;
    for(i=0; tags && tags[i]; i++){
        enum CodecID id= ff_codec_get_id(tags[i], tag);
        if(id!=CODEC_ID_NONE) return id;
    }
    return CODEC_ID_NONE;
}
