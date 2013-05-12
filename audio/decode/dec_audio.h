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

#ifndef MPLAYER_DEC_AUDIO_H
#define MPLAYER_DEC_AUDIO_H

#include "audio/chmap.h"
#include "demux/stheader.h"

struct bstr;
struct mp_decoder_list;

struct mp_decoder_list *mp_audio_decoder_list(void);
int init_best_audio_codec(sh_audio_t *sh_audio, char *audio_decoders);
int decode_audio(sh_audio_t *sh_audio, struct bstr *outbuf, int minlen);
void decode_audio_prepend_bytes(struct bstr *outbuf, int count, int byte);
void resync_audio_stream(sh_audio_t *sh_audio);
void skip_audio_frame(sh_audio_t *sh_audio);
void uninit_audio(sh_audio_t *sh_audio);

int init_audio_filters(sh_audio_t *sh_audio, int in_samplerate,
                       int *out_samplerate, struct mp_chmap *out_channels,
                       int *out_format);

#endif /* MPLAYER_DEC_AUDIO_H */
