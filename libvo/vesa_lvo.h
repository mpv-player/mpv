/*
 *  vesa_lvo.c
 *
 *	Copyright (C) Nick Kurshev <nickols_k@mail.ru> - Oct 2001
 *
 *  You can redistribute this file under terms and conditions
 *  of GNU General Public licence v2.
 *
 * This file contains vo_vesa interface to Linux Video Overlay.
 */

#ifndef __VESA_LVO_INCLUDED
#define __VESA_LVO_INCLUDED

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

#endif
