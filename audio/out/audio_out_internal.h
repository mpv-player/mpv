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

#ifndef MPLAYER_AUDIO_OUT_INTERNAL_H
#define MPLAYER_AUDIO_OUT_INTERNAL_H

#include "core/options.h"
#include "ao.h"

// prototypes:
//static ao_info_t info;
static int control(int cmd, void *arg);
static int init(int rate,const struct mp_chmap *channels,int format,int flags);
static void uninit(int immed);
static void reset(void);
static int get_space(void);
static int play(void* data,int len,int flags);
static float get_delay(void);
static void audio_pause(void);
static void audio_resume(void);

extern struct ao *global_ao;
#define ao_data (*global_ao)
#define mixer_channel (global_ao->opts->mixer_channel)
#define mixer_device (global_ao->opts->mixer_device)

#define LIBAO_EXTERN(x) const struct ao_driver audio_out_##x = { \
    .info = &info,                                               \
    .control   = old_ao_control,                                 \
    .init      = old_ao_init,                                    \
    .uninit    = old_ao_uninit,                                  \
    .reset     = old_ao_reset,                                   \
    .get_space = old_ao_get_space,                               \
    .play      = old_ao_play,                                    \
    .get_delay = old_ao_get_delay,                               \
    .pause     = old_ao_pause,                                   \
    .resume    = old_ao_resume,                                  \
    .old_functions = &(const struct ao_old_functions) {          \
        .control    = control,                                   \
	.init       = init,                                      \
        .uninit     = uninit,                                    \
	.reset      = reset,                                     \
	.get_space  = get_space,                                 \
	.play       = play,                                      \
	.get_delay  = get_delay,                                 \
	.pause      = audio_pause,                               \
	.resume     = audio_resume,                              \
    },                                                           \
};

#endif /* MPLAYER_AUDIO_OUT_INTERNAL_H */
