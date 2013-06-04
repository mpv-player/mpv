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

#ifndef MPLAYER_SPUDEC_H
#define MPLAYER_SPUDEC_H

#include <stdint.h>

struct sub_bitmaps;
struct mp_osd_res;

void spudec_heartbeat(void *this, unsigned int pts100);
void spudec_assemble(void *this, unsigned char *packet, unsigned int len, int pts100);
void spudec_get_indexed(void *this, struct mp_osd_res *dim, struct sub_bitmaps *res);
void *spudec_new_scaled(unsigned int frame_width, unsigned int frame_height, uint8_t *extradata, int extradata_len);
void spudec_free(void *this);
void spudec_reset(void *this);	// called after seek
int spudec_visible(void *this); // check if spu is visible
int spudec_changed(void *this);
void spudec_set_changed(void *this);
void spudec_set_forced_subs_only(void * const this, const unsigned int flag);

#endif /* MPLAYER_SPUDEC_H */
