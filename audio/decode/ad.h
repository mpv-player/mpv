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

#ifndef MPLAYER_AD_H
#define MPLAYER_AD_H

#include "core/codecs.h"
#include "demux/stheader.h"

typedef struct mp_codec_info ad_info_t;

struct mp_decoder_list;

/* interface of video decoder drivers */
typedef struct ad_functions
{
    const char *name;
    void (*add_decoders)(struct mp_decoder_list *list);
    int (*preinit)(sh_audio_t *sh);
    int (*init)(sh_audio_t *sh, const char *decoder);
    void (*uninit)(sh_audio_t *sh);
    int (*control)(sh_audio_t *sh,int cmd,void* arg, ...);
    int (*decode_audio)(sh_audio_t *sh, unsigned char *buffer, int minlen,
                        int maxlen);
} ad_functions_t;

// NULL terminated array of all drivers
extern const ad_functions_t * const mpcodecs_ad_drivers[];

// fallback if ADCTRL_RESYNC not implemented: sh_audio->a_in_buffer_len=0;
#define ADCTRL_RESYNC_STREAM 1   // resync, called after seeking

// fallback if ADCTRL_SKIP not implemented: ds_fill_buffer(sh_audio->ds);
#define ADCTRL_SKIP_FRAME 2      // skip block/frame, called while seeking

#endif /* MPLAYER_AD_H */
