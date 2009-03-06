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

// prototypes:
//static ao_info_t info;
static int control(int cmd, void *arg);
static int init(int rate,int channels,int format,int flags);
static void uninit(int immed);
static void reset(void);
static int get_space(void);
static int play(void* data,int len,int flags);
static float get_delay(void);
static void audio_pause(void);
static void audio_resume(void);

#define LIBAO_EXTERN(x) const ao_functions_t audio_out_##x =\
{\
	&info,\
	control,\
	init,\
        uninit,\
	reset,\
	get_space,\
	play,\
	get_delay,\
	audio_pause,\
	audio_resume\
};

#endif /* MPLAYER_AUDIO_OUT_INTERNAL_H */
