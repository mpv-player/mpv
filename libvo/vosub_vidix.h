/*
 * vosub_vidix interface to any mplayer vo driver
 *
 * copyright (C) 2002 Nick Kurshev <nickols_k@mail.ru>
 *
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

#ifndef MPLAYER_VOSUB_VIDIX_H
#define MPLAYER_VOSUB_VIDIX_H

#include <stdint.h>
#include "video_out.h"

		    /* drvname can be NULL */
int	 vidix_preinit(const char *drvname,vo_functions_t *server);
int      vidix_init(unsigned src_width,unsigned src_height,
		    unsigned dest_x,unsigned dest_y,unsigned dst_width,
		    unsigned dst_height,unsigned format,unsigned dest_bpp,
		    unsigned vid_w,unsigned vid_h);
int	 vidix_start(void);
int	 vidix_stop(void);
void     vidix_term( void );
uint32_t vidix_control(uint32_t request, void *data, ...);
uint32_t vidix_query_fourcc(uint32_t fourcc);

#include "vidix/vidix.h"
/* graphic keys */
int vidix_grkey_support(void);
int vidix_grkey_get(vidix_grkey_t *gr_key);
int vidix_grkey_set(const vidix_grkey_t *gr_key);

#endif /* MPLAYER_VOSUB_VIDIX_H */
