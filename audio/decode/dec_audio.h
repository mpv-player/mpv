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
#include "audio/audio.h"
#include "demux/demux.h"
#include "demux/stheader.h"

struct mp_audio_buffer;
struct mp_decoder_list;

struct dec_audio {
    struct mp_log *log;
    struct MPOpts *opts;
    struct mpv_global *global;
    const struct ad_functions *ad_driver;
    struct sh_stream *header;
    struct mp_audio_buffer *decode_buffer;
    struct af_stream *afilter;
    char *decoder_desc;
    struct mp_tags *metadata;
    // set by decoder
    struct mp_audio decoded;    // format of decoded audio (no data, temporarily
                                // different from decode_buffer during format
                                // changes)
    int i_bps;                  // input bitrate, can change with VBR sources
    // last known pts value in output from decoder
    double pts;
    // number of samples output by decoder after last known pts
    int pts_offset;
    // For free use by the ad_driver
    void *priv;
};

struct mp_decoder_list *audio_decoder_list(void);
int audio_init_best_codec(struct dec_audio *d_audio, char *audio_decoders);
int audio_decode(struct dec_audio *d_audio, struct mp_audio_buffer *outbuf,
                 int minsamples);
void audio_reset_decoding(struct dec_audio *d_audio);
void audio_uninit(struct dec_audio *d_audio);

int audio_init_filters(struct dec_audio *d_audio, int in_samplerate,
                       int *out_samplerate, struct mp_chmap *out_channels,
                       int *out_format);

#endif /* MPLAYER_DEC_AUDIO_H */
