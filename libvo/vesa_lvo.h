/*
 * vo_vesa interface to Linux Video Overlay
 *
 * copyright (C) 2001 Nick Kurshev <nickols_k@mail.ru>
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

#ifndef MPLAYER_VESA_LVO_H
#define MPLAYER_VESA_LVO_H

#include <stdint.h>

int	 vlvo_preinit(const char *drvname);
int      vlvo_init(unsigned src_width,unsigned src_height,
		   unsigned x_org,unsigned y_org,unsigned dst_width,
		   unsigned dst_height,unsigned format,unsigned dest_bpp);
void     vlvo_term( void );
uint32_t vlvo_query_info(uint32_t format);

uint32_t vlvo_draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y);
uint32_t vlvo_draw_frame(uint8_t *src[]);
void     vlvo_flip_page(void);
void     vlvo_draw_osd(void);

#endif /* MPLAYER_VESA_LVO_H */
