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

#ifndef MPLAYER_VD_INTERNAL_H
#define MPLAYER_VD_INTERNAL_H

#include "codec-cfg.h"
#include "img_format.h"

#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

#include "vd.h"

extern int divx_quality;

// prototypes:
//static vd_info_t info;
static int control(sh_video_t *sh,int cmd,void* arg,...);
static int init(sh_video_t *sh);
static void uninit(sh_video_t *sh);
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags);

#define LIBVD_EXTERN(x) const vd_functions_t mpcodecs_vd_##x = {\
	&info,\
	init,\
        uninit,\
	control,\
	decode\
};

#endif /* MPLAYER_VD_INTERNAL_H */
