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
    char *decoder_desc;

    bool try_spdif;

    // For free use by the ad_driver
    void *priv;

    // Strictly internal (dec_audio.c).

    double pts; // endpts of previous frame
    struct demux_packet *packet;
    struct mp_audio *current_frame;
    int current_state;
};

struct mp_decoder_list *audio_decoder_list(void);
int audio_init_best_codec(struct dec_audio *d_audio);
void audio_uninit(struct dec_audio *d_audio);

void audio_work(struct dec_audio *d_audio);
int audio_get_frame(struct dec_audio *d_audio, struct mp_audio **out_frame);

void audio_reset_decoding(struct dec_audio *d_audio);

#endif /* MPLAYER_DEC_AUDIO_H */
