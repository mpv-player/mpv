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

#ifndef MP_AVCOMMON_H
#define MP_AVCOMMON_H

#include <inttypes.h>

#include <libavutil/avutil.h>
#include <libavutil/rational.h>
#include <libavcodec/avcodec.h>

struct mp_decoder_list;
struct demux_packet;
struct AVDictionary;
struct mp_log;

int mp_lavc_set_extradata(AVCodecContext *avctx, void *ptr, int size);
void mp_copy_lav_codec_headers(AVCodecContext *avctx, AVCodecContext *st);
void mp_set_av_packet(AVPacket *dst, struct demux_packet *mpkt, AVRational *tb);
int64_t mp_pts_to_av(double mp_pts, AVRational *tb);
double mp_pts_from_av(int64_t av_pts, AVRational *tb);
void mp_set_avcodec_threads(struct mp_log *l, AVCodecContext *avctx, int threads);
void mp_add_lavc_decoders(struct mp_decoder_list *list, enum AVMediaType type);
int mp_codec_to_av_codec_id(const char *codec);
const char *mp_codec_from_av_codec_id(int codec_id);
void mp_set_avdict(struct AVDictionary **dict, char **kv);
void mp_avdict_print_unset(struct mp_log *log, int msgl, struct AVDictionary *d);
int mp_set_avopts(struct mp_log *log, void *avobj, char **kv);

#endif
