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

#ifndef MPLAYER_AD_INTERNAL_H
#define MPLAYER_AD_INTERNAL_H

#include "codec-cfg.h"
#include "libaf/af_format.h"

#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

#include "ad.h"

extern int audio_output_channels;
static int init(sh_audio_t *sh);
static int preinit(sh_audio_t *sh);
static void uninit(sh_audio_t *sh);
static int control(sh_audio_t *sh,int cmd,void* arg, ...);
static int decode_audio(sh_audio_t *sh,unsigned char *buffer,int minlen,int maxlen);

#define LIBAD_EXTERN(x) const ad_functions_t mpcodecs_ad_##x = {\
	&info,\
	preinit,\
	init,\
        uninit,\
	control,\
	decode_audio\
};

#endif /* MPLAYER_AD_INTERNAL_H */
