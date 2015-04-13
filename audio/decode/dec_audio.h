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
    struct af_stream *afilter;
    char *decoder_desc;
    int init_retries;
    struct mp_audio_pool *pool;
    struct mp_audio decode_format;
    struct mp_audio *waiting;   // used on format-change
    // set by decoder
    int bitrate;                // input bitrate, can change with VBR sources
    // last known pts value in output from decoder
    double pts;
    // number of samples output by decoder after last known pts
    int pts_offset;
    // For free use by the ad_driver
    void *priv;
};

enum {
    AD_OK = 0,
    AD_ERR = -1,
    AD_EOF = -2,
    AD_NEW_FMT = -3,
    AD_WAIT = -4,
};

struct mp_decoder_list *audio_decoder_list(void);
int audio_init_best_codec(struct dec_audio *d_audio, char *audio_decoders);
int audio_decode(struct dec_audio *d_audio, struct mp_audio_buffer *outbuf,
                 int minsamples);
int initial_audio_decode(struct dec_audio *d_audio);
void audio_reset_decoding(struct dec_audio *d_audio);
void audio_uninit(struct dec_audio *d_audio);

#endif /* MPLAYER_DEC_AUDIO_H */
