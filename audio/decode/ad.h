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

#ifndef MPLAYER_AD_H
#define MPLAYER_AD_H

#include "common/codecs.h"
#include "demux/stheader.h"
#include "demux/demux.h"

#include "audio/format.h"
#include "audio/audio.h"
#include "dec_audio.h"

struct mp_decoder_list;

/* interface of video decoder drivers */
struct ad_functions {
    const char *name;
    void (*add_decoders)(struct mp_decoder_list *list);
    int (*init)(struct dec_audio *da, const char *decoder);
    void (*uninit)(struct dec_audio *da);
    int (*control)(struct dec_audio *da, int cmd, void *arg);
    int (*decode_packet)(struct dec_audio *da, struct mp_audio **out);
};

enum ad_ctrl {
    ADCTRL_RESET = 1,   // flush and reset state, e.g. after seeking
};

#endif /* MPLAYER_AD_H */
