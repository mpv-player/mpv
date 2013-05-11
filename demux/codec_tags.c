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
#include "core/av_common.h"

/* The following tables map FourCCs to codec names (as defined by libavcodec).
 * However, this includes only names that are not defined by libavformat's
 * RIFF tag tables, or which are mapped differently (for unknown reasons).
 * These mappings were extracted from the codecs.conf file when it was removed,
 * and these mappings have been maintained for years, meaning they are well
 * tested and possibly make some files work. Or they could just be bugs.
 *
 * Note that demux_lavf does not use these tables, and always uses the native
 * libavformat mappings.
 *
 * Note about internal mplayer FourCCs:
 *
 * These are made-up FourCCs which don't actually show up in files, but which
 * were used by the original MPlayer to identify codecs. Since we don't quite
 * know/care if there are still some uses of them left, they're still here.
 */

struct mp_codec_tag {
    uint32_t tag;
    const char *codec;
};

static const struct mp_codec_tag mp_audio_codec_tags[] = {
    {MKTAG('Q', 'D', 'M', '2'), "qdm2"},
    {MKTAG('Q', 'c', 'l', 'p'), "qcelp"},
    {MKTAG('s', 'q', 'c', 'p'), "qcelp"},
    {MKTAG('Q', 'c', 'l', 'q'), "qcelp"},
    {MKTAG('1', '4', '_', '4'), "ra_144"},
    {MKTAG('l', 'p', 'c', 'J'), "ra_144"},
    {MKTAG('2', '8', '_', '8'), "ra_288"},
    {MKTAG('c', 'o', 'o', 'k'), "cook"},
    {MKTAG('a', 't', 'r', 'c'), "atrac3"},
    {MKTAG('s', 'i', 'p', 'r'), "sipr"},
    {MKTAG('S', 'M', 'K', 'A'), "smackaudio"},
    {MKTAG('A', 'x', 'a', 'n'), "xan_dpcm"},
    {0x594a                   , "xan_dpcm"},
    {MKTAG('T', 'H', 'P', 'A'), "adpcm_thp"},
    {MKTAG('R', 'A', 'D', 'V'), "dvaudio"},
    {MKTAG('v', 'd', 'v', 'a'), "dvaudio"},
    {MKTAG('d', 'v', 'c', 'a'), "dvaudio"},
    {MKTAG('d', 'v', 'a', 'u'), "dvaudio"},
    {MKTAG('m', 'p', '4', 'a'), "aac"},
    {MKTAG('M', 'P', '4', 'A'), "aac"},
    {MKTAG('r', 'a', 'a', 'c'), "aac"},
    {MKTAG('r', 'a', 'c', 'p'), "aac"},
    {0xa106                   , "aac"}, // libav 0.8: -, ffmpeg 1.1: aac
    {0xaac0                   , "aac"},
    {MKTAG('f', 'L', 'a', 'C'), "flac"},
    {MKTAG('m', 's', 241, 172), "flac"},
    {MKTAG('a', 'l', 'a', 'c'), "alac"},
    {MKTAG('A', 'P', 'E', ' '), "ape"},
    {MKTAG('d', 'a', 'u', 'd'), "pcm_s24daud"},
    {MKTAG('W', 'M', 'A', '3'), "wmapro"},
    {MKTAG('M', 'A', 'C', '3'), "mace3"},
    {MKTAG('M', 'A', 'C', '6'), "mace6"},
    {MKTAG('S', 'O', 'N', 'C'), "sonic"},
    {0x2048                   , "sonic"}, // libav 0.8: -, ffmpeg 1.1: sonic
    {MKTAG('T', 'S', 0  , 'U'), "mp3"},
    {MKTAG('G', 'S', 'M', ' '), "gsm"},
    {0x1500                   , "gsm"}, // lavf: gsm_ms
    {MKTAG('a', 'g', 's', 'm'), "gsm"},
    {0x32                     , "gsm_ms"},
    {MKTAG(1  , 0  , 1  , 0  ), "pcm_dvd"},
    {MKTAG('B', 'S', 'S', 'D'), "s302m"},
    {MKTAG('A', 'C', '-', '3'), "ac3"},
    {MKTAG('d', 'n', 'e', 't'), "ac3"},
    {MKTAG('s', 'a', 'c', '3'), "ac3"},
    {MKTAG('E', 'A', 'C', '3'), "eac3"},
    {0x86                     , "dts"},
    {MKTAG('M', 'P', 'C', ' '), "musepack7"},
    {MKTAG('M', 'P', 'C', '8'), "musepack8"},
    {MKTAG('M', 'P', 'C', 'K'), "musepack8"},
    {MKTAG('s', 'a', 'm', 'r'), "amr_nb"},
    {MKTAG('s', 'a', 'w', 'b'), "amr_wb"},
    {MKTAG('v', 'r', 'b', 's'), "vorbis"},
    // Special cased in ad_lavc:
    {0                        , "pcm"},
    {0x1                      , "pcm"}, // lavf: pcm_s16le
    {0x3                      , "pcm"}, // lavf: pcm_f32le
    {0xfffe                   , "pcm"},
    {MKTAG('t', 'w', 'o', 's'), "pcm"},
    // ------- internal mplayer FourCCs ------
    {MKTAG('O', 'p', 'u', 's'), "opus"}, // demux_mkv.c
    {MKTAG('a', 'L', 'a', 'C'), "alac"}, // demux_mkv.c
    {MKTAG('S', 'a', 'd', 'x'), "adpcm_adx"},
    {MKTAG('A', 'M', 'V', 'A'), "adpcm_ima_amv"},
    {MKTAG('R', 'o', 'Q', 'A'), "roq_dpcm"},
    {MKTAG('B', 'A', 'U', '1'), "binkaudio_dct"},
    {MKTAG('B', 'A', 'U', '2'), "binkaudio_rdft"},
    {MKTAG('D', 'C', 'I', 'A'), "dsicinaudio"},
    {MKTAG('4', 'X', 'M', 'A'), "adpcm_4xm"},
    {MKTAG('A', 'I', 'W', 'S'), "adpcm_ima_ws"},
    {MKTAG('S', 'N', 'D', '1'), "westwood_snd1"},
    {MKTAG('I', 'N', 'P', 'A'), "interplay_dpcm"},
    {MKTAG('A', 'D', 'E', 'A'), "adpcm_ea"},
    {MKTAG('A', 'D', 'X', 'A'), "adpcm_ea_maxis_xa"},
    {MKTAG('P', 'S', 'X', 'A'), "adpcm_xa"},
    {MKTAG('M', 'P', '4', 'L'), "aac_latm"},
    {MKTAG('T', 'T', 'A', '1'), "tta"},
    {MKTAG('W', 'V', 'P', 'K'), "wavpack"},
    {MKTAG('s', 'h', 'r', 'n'), "shorten"},
    {MKTAG('A', 'L', 'S', ' '), "mp4als"},
    {MKTAG('M', 'L', 'P', ' '), "mlp"},
    {MKTAG('T', 'R', 'H', 'D'), "truehd"},
    {MKTAG('N', 'E', 'L', 'L'), "nellymoser"},
    {MKTAG('m', '4', 'a', 29 ), "mp3on4"},
    {MKTAG('a', 'd', 'u', 'U'), "mp3adu"},
    {MKTAG('B', 'P', 'C', 'M'), "pcm_bluray"},
    {MKTAG('P', 'L', 'X', 'F'), "pcm_lxf"},
    {MKTAG('T', 'W', 'I', '2'), "twinvq"},
    {0x20776172,                "pcm"}, // demux_mpg.c dvdpcm
    {0},
};

static const struct mp_codec_tag mp_video_codec_tags[] = {
    {MKTAG('V', 'B', 'V', '1'), "vb"},
    {MKTAG('M', 'L', '2', '0'), "mimic"},
    {MKTAG('R', '1', '0', 'g'), "r10k"},
    {MKTAG('m', '1', 'v', '1'), "mpeg1video"},
    {MKTAG('m', 'p', 'g', '2'), "mpeg2video"}, // lavf: mpeg1video
    {MKTAG('M', 'P', 'G', '2'), "mpeg2video"}, // lavf: mpeg1video
    {MKTAG('m', 'x', '5', 'p'), "mpeg2video"},
    {MKTAG('h', 'd', 'v', '1'), "mpeg2video"},
    {MKTAG('h', 'd', 'v', '2'), "mpeg2video"},
    {MKTAG('h', 'd', 'v', '3'), "mpeg2video"},
    {MKTAG('h', 'd', 'v', '4'), "mpeg2video"},
    {MKTAG('h', 'd', 'v', '5'), "mpeg2video"},
    {MKTAG('h', 'd', 'v', '6'), "mpeg2video"},
    {MKTAG('h', 'd', 'v', '7'), "mpeg2video"},
    {MKTAG('h', 'd', 'v', '8'), "mpeg2video"},
    {MKTAG('h', 'd', 'v', '9'), "mpeg2video"},
    {MKTAG('x', 'd', 'v', '1'), "mpeg2video"},
    {MKTAG('x', 'd', 'v', '2'), "mpeg2video"},
    {MKTAG('x', 'd', 'v', '3'), "mpeg2video"},
    {MKTAG('x', 'd', 'v', '4'), "mpeg2video"},
    {MKTAG('x', 'd', 'v', '5'), "mpeg2video"},
    {MKTAG('x', 'd', 'v', '6'), "mpeg2video"},
    {MKTAG('x', 'd', 'v', '7'), "mpeg2video"},
    {MKTAG('x', 'd', 'v', '8'), "mpeg2video"},
    {MKTAG('x', 'd', 'v', '9'), "mpeg2video"},
    {MKTAG('x', 'd', 'v', 'a'), "mpeg2video"},
    {MKTAG('x', 'd', 'v', 'b'), "mpeg2video"},
    {MKTAG('x', 'd', 'v', 'c'), "mpeg2video"},
    {MKTAG('x', 'd', 'v', 'd'), "mpeg2video"},
    {MKTAG('x', 'd', 'v', 'e'), "mpeg2video"},
    {MKTAG('x', 'd', 'v', 'f'), "mpeg2video"},
    {MKTAG('x', 'd', '5', 'a'), "mpeg2video"},
    {MKTAG('x', 'd', '5', 'b'), "mpeg2video"},
    {MKTAG('x', 'd', '5', 'c'), "mpeg2video"},
    {MKTAG('x', 'd', '5', 'd'), "mpeg2video"},
    {MKTAG('x', 'd', '5', 'e'), "mpeg2video"},
    {MKTAG('x', 'd', '5', 'f'), "mpeg2video"},
    {MKTAG('x', 'd', '5', '9'), "mpeg2video"},
    {MKTAG('x', 'd', '5', '4'), "mpeg2video"},
    {MKTAG('x', 'd', '5', '5'), "mpeg2video"},
    {MKTAG('m', 'x', '5', 'n'), "mpeg2video"},
    {MKTAG('m', 'x', '4', 'n'), "mpeg2video"},
    {MKTAG('m', 'x', '4', 'p'), "mpeg2video"},
    {MKTAG('m', 'x', '3', 'n'), "mpeg2video"},
    {MKTAG('m', 'x', '3', 'p'), "mpeg2video"},
    {MKTAG('A', 'V', 'm', 'p'), "mpeg2video"},
    {MKTAG('V', 'C', 'R', '2'), "mpeg2video"}, // lavf: mpeg1video
    {MKTAG('m', 'p', '2', 'v'), "mpeg2video"},
    {MKTAG('m', '2', 'v', '1'), "mpeg2video"},
    {MKTAG('R', 'J', 'P', 'G'), "nuv"},
    {MKTAG('b', 'm', 'p', ' '), "bmp"},
    {MKTAG('b', 'm', 'p', 0  ), "bmp"},
    {MKTAG('g', 'i', 'f', ' '), "gif"},
    {MKTAG('t', 'i', 'f', 'f'), "tiff"},
    {MKTAG('p', 'c', 'x', ' '), "pcx"},
    {MKTAG('m', 't', 'g', 'a'), "targa"},
    {MKTAG('M', 'T', 'G', 'A'), "targa"},
    {MKTAG('r', 'l', 'e', ' '), "qtrle"},
    {MKTAG('s', 'm', 'c', ' '), "smc"},
    {MKTAG('8', 'B', 'P', 'S'), "8bps"},
    {MKTAG('W', 'R', 'L', 'E'), "msrle"},
    {MKTAG('F', 'F', 'V', 'H'), "huffyuv"}, // lavf: ffvhuff
    {MKTAG('P', 'I', 'X', 'L'), "vixl"},
    {MKTAG('X', 'I', 'X', 'L'), "vixl"},
    {MKTAG('q', 'd', 'r', 'w'), "qdraw"},
    {MKTAG('D', 'I', 'V', 'F'), "msmpeg4v3"},
    {MKTAG('d', 'i', 'v', 'f'), "msmpeg4v3"},
    {MKTAG('3', 'I', 'V', 'D'), "msmpeg4v3"},
    {MKTAG('3', 'i', 'v', 'd'), "msmpeg4v3"},
    {MKTAG('w', 'm', 'v', 'p'), "wmv3"}, // lavf: wmv3image
    {MKTAG('W', 'M', 'V', 'P'), "wmv3"}, // lavf: wmv3image
    {MKTAG('v', 'c', '-', '1'), "vc1"},
    {MKTAG('V', 'C', '-', '1'), "vc1"},
    {MKTAG('v', 'v', 'v', 'c'), "h264"},
    {MKTAG('a', 'i', '5', '5'), "h264"},
    {MKTAG('a', 'i', '1', '5'), "h264"},
    {MKTAG('a', 'i', '1', 'q'), "h264"},
    {MKTAG('a', 'i', '5', 'q'), "h264"},
    {MKTAG('a', 'i', '1', '2'), "h264"},
    {MKTAG(5  , 0  , 0  , 16 ), "h264"},
    {MKTAG('S', 'V', 'Q', '3'), "svq3"}, // libav 0.8: -, ffmpeg 1.1: svq3
    {MKTAG('d', 'r', 'a', 'c'), "dirac"}, // libav 0.8: -, ffmpeg 1.1: dirac
    {MKTAG('A', 'V', 'R', 'n'), "mjpeg"}, // libav 0.8: mjpeg, ffmpeg 1.1: avrn
    {MKTAG('A', 'V', 'D', 'J'), "mjpeg"},
    {MKTAG('A', 'D', 'J', 'V'), "mjpeg"},
    {MKTAG('J', 'F', 'I', 'F'), "mjpeg"},
    {MKTAG('M', 'J', 'L', 'S'), "mjpeg"}, // lavf: jpegls
    {MKTAG('m', 'j', 'p', 'b'), "mjpegb"},
    {MKTAG('U', '2', '6', '3'), "h263"}, // libav 0.8: -, ffmpeg 1.1: h263
    {MKTAG('v', 'i', 'v', '1'), "h263"},
    {MKTAG('s', '2', '6', '3'), "h263"}, // libav 0.8: -, ffmpeg 1.1: flv1
    {MKTAG('S', '2', '6', '3'), "h263"}, // same
    {MKTAG('D', '2', '6', '3'), "h263"},
    {MKTAG('I', 'L', 'V', 'R'), "h263"},
    {MKTAG('d', 'v', 'p', ' '), "dvvideo"},
    {MKTAG('d', 'v', 'p', 'p'), "dvvideo"},
    {MKTAG('A', 'V', 'd', 'v'), "dvvideo"},
    {MKTAG('A', 'V', 'd', '1'), "dvvideo"},
    {MKTAG('d', 'v', 'h', 'q'), "dvvideo"},
    {MKTAG('d', 'v', 'h', 'p'), "dvvideo"},
    {MKTAG('d', 'v', 'h', '5'), "dvvideo"},
    {MKTAG('d', 'v', 'h', '6'), "dvvideo"},
    {MKTAG('d', 'v', 'h', '3'), "dvvideo"},
    {MKTAG('d', 'v', 's', '1'), "dvvideo"},
    {MKTAG('R', 'V', '2', '0'), "rv20"},
    {MKTAG('r', 'v', '2', '0'), "rv20"},
    {MKTAG('R', 'V', 'T', 'R'), "rv20"},
    {MKTAG('R', 'V', '3', '0'), "rv30"},
    {MKTAG('r', 'v', '3', '0'), "rv30"},
    {MKTAG('R', 'V', '4', '0'), "rv40"},
    {MKTAG('r', 'v', '4', '0'), "rv40"},
    {MKTAG('R', 'V', '1', '0'), "rv10"},
    {MKTAG('r', 'v', '1', '0'), "rv10"},
    {MKTAG('R', 'V', '1', '3'), "rv10"},
    {MKTAG('r', 'v', '1', '3'), "rv10"},
    {MKTAG('T', 'h', 'r', 'a'), "theora"},
    {0xfffc                   , "theora"},
    {MKTAG('V', 'P', '6', 'A'), "vp6a"},
    {MKTAG('S', 'P', '5', '3'), "sp5x"},
    {MKTAG('S', 'P', '5', '5'), "sp5x"},
    {MKTAG('S', 'P', '5', '6'), "sp5x"},
    {MKTAG('S', 'P', '5', '7'), "sp5x"},
    {MKTAG('S', 'P', '5', '8'), "sp5x"},
    {MKTAG('S', 'M', 'K', '2'), "smackvideo"},
    {MKTAG('S', 'M', 'K', '4'), "smackvideo"},
    {MKTAG('a', 'v', 's', '2'), "cavs"},
    {MKTAG('A', 'V', 'd', 'n'), "dnxhd"},
    {MKTAG('a', 'p', 'c', 'h'), "prores"},
    {MKTAG('a', 'p', 'c', 'n'), "prores"},
    {MKTAG('a', 'p', 'c', 's'), "prores"},
    {MKTAG('a', 'p', 'c', 'o'), "prores"},
    {MKTAG('a', 'p', '4', 'h'), "prores"},
    {MKTAG('f', 'V', 'G', 'T'), "tgv"},
    // These are probably not correctly handled. The original codecs.conf
    // entries mapped these to separate pixel formats via vd_raw and the
    // "out" directive (look at MPlayer's codecs.conf).
    // Should they be aliased to supported FourCCs of the same formats?
    {MKTAG('A', 'V', '1', 'x'), "rawvideo"},
    {MKTAG('A', 'V', 'u', 'p'), "rawvideo"},
    {MKTAG('4', '4', '4', 'p'), "rawvideo"},
    {MKTAG('4', '4', '4', 'P'), "rawvideo"},
    {MKTAG('4', '2', '2', 'p'), "rawvideo"},
    {MKTAG('4', '2', '2', 'P'), "rawvideo"},
    // Unknown:
    {MKTAG('r', 'a', 'w', ' '), "rawvideo"},
    {MKTAG('D', 'V', 'O', 'O'), "rawvideo"},
    // ------- internal mplayer FourCCs ------
    {MKTAG('A', 'N', 'M', ' '), "anm"},
    {MKTAG('B', 'I', 'K', 'f'), "binkvideo"},
    {MKTAG('B', 'I', 'K', 'g'), "binkvideo"},
    {MKTAG('B', 'I', 'K', 'h'), "binkvideo"},
    {MKTAG('B', 'I', 'K', 'i'), "binkvideo"},
    {MKTAG('C', 'D', 'G', 'R'), "cdgraphics"},
    {MKTAG('M', 'V', 'I', '1'), "motionpixels"},
    {MKTAG('M', 'D', 'E', 'C'), "mdec"},
    {MKTAG('N', 'U', 'V', '1'), "nuv"},
    {MKTAG('F', 'L', 'I', 'C'), "flic"},
    {MKTAG('R', 'o', 'Q', 'V'), "roq"},
    {MKTAG('A', 'M', 'V', 'V'), "amv"},
    {MKTAG('F', 'F', 'J', 'V'), "jv"},
    {MKTAG('T', 'S', 'E', 'Q'), "tiertexseqvideo"},
    {MKTAG('V', 'M', 'D', 'V'), "vmdvideo"},
    {MKTAG('D', 'X', 'A', '1'), "dxa"},
    {MKTAG('D', 'C', 'I', 'V'), "dsicinvideo"},
    {MKTAG('T', 'H', 'P', 'V'), "thp"},
    {MKTAG('B', 'F', 'I', 'V'), "bfi"},
    {MKTAG('B', 'E', 'T', 'H'), "bethsoftvid"},
    {MKTAG('R', 'L', '2', 'V'), "rl2"},
    {MKTAG('T', 'X', 'D', 'V'), "txd"},
    {MKTAG('W', 'C', '3', 'V'), "xan_wc3"},
    {MKTAG('I', 'D', 'C', 'I'), "idcin"},
    {MKTAG('I', 'N', 'P', 'V'), "interplayvideo"},
    {MKTAG('V', 'Q', 'A', 'V'), "ws_vqa"},
    {MKTAG('C', '9', '3', 'V'), "c93"},
    {MKTAG('V', 'P', '9', '0'), "vp9"},
    {0},
};

static const int mp_fourcc_video_aliases[][2] = {
    // msmpeg4
    {MKTAG('M', 'P', 'G', '3'), MKTAG('d', 'i', 'v', '3')},
    {MKTAG('m', 'p', 'g', '3'), MKTAG('d', 'i', 'v', '3')},
    {MKTAG('M', 'P', '4', '3'), MKTAG('d', 'i', 'v', '3')},
    {MKTAG('m', 'p', '4', '3'), MKTAG('d', 'i', 'v', '3')},
    {MKTAG('D', 'I', 'V', '5'), MKTAG('d', 'i', 'v', '3')},
    {MKTAG('d', 'i', 'v', '5'), MKTAG('d', 'i', 'v', '3')},
    {MKTAG('D', 'I', 'V', '6'), MKTAG('d', 'i', 'v', '4')},
    {MKTAG('d', 'i', 'v', '6'), MKTAG('d', 'i', 'v', '4')},
    {MKTAG('A', 'P', '4', '1'), MKTAG('d', 'i', 'v', '3')},

    // msmpeg4v2
    {MKTAG('D', 'I', 'V', '2'), MKTAG('m', 'p', '4', '2')},
    {MKTAG('d', 'i', 'v', '2'), MKTAG('m', 'p', '4', '2')},
    {MKTAG('D', 'I', 'V', '1'), MKTAG('d', 'i', 'v', 'x')},
    {MKTAG('d', 'i', 'v', '1'), MKTAG('d', 'i', 'v', 'x')},

    // mpeg4
    {MKTAG('d', 'x', '5', '0'), MKTAG('D', 'X', '5', '0')},
    {MKTAG('B', 'L', 'Z', '0'), MKTAG('D', 'X', '5', '0')},

    {MKTAG('v', 'i', 'v', '1'), MKTAG('h', '2', '6', '3')},
    {MKTAG('T', 'h', 'r', 'a'), MKTAG('t', 'h', 'e', 'o')},

    {0},
};

static const char *lookup_tag(const struct mp_codec_tag *mp_table,
                              const struct AVCodecTag *av_table,
                              uint32_t tag)
{
    for (int n = 0; mp_table[n].codec; n++) {
        if (mp_table[n].tag == tag)
            return mp_table[n].codec;
    }
    const struct AVCodecTag *av_tags[] = {av_table, NULL};
    int id = av_codec_get_id(av_tags, tag);
    return id == AV_CODEC_ID_NONE ? NULL : mp_codec_from_av_codec_id(id);
}

void mp_set_video_codec_from_tag(struct sh_video *sh)
{
    sh->gsh->codec = lookup_tag(mp_video_codec_tags,
                                avformat_get_riff_video_tags(),
                                sh->format);
}

void mp_set_audio_codec_from_tag(struct sh_audio *sh)
{
    sh->gsh->codec = lookup_tag(mp_audio_codec_tags,
                                avformat_get_riff_audio_tags(),
                                sh->format);
}

uint32_t mp_video_fourcc_alias(uint32_t fourcc)
{
    for (int n = 0; mp_fourcc_video_aliases[n][0]; n++) {
        if (mp_fourcc_video_aliases[n][0] == fourcc)
            return mp_fourcc_video_aliases[n][1];
    }
    return fourcc;
}
