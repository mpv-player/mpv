/*
 * helper routines for building MPEG 1/2 PS/PES packets
 *
 * Copyright (C) 2006 Benjamin Zores
 *
 * based on code borrowed from vo_mpegpes/vo_dxr2:
 *    (C) 2000 Ralph Metzler <ralph@convergence.de>
 *             Marcus Metzler <marcus@convergence.de>
 *             Gerard Lantau
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

#ifndef MPLAYER_MPEG_PACKETIZER_H
#define MPLAYER_MPEG_PACKETIZER_H

#include <stdint.h>

/* Send MPEG <type> PES packet */
int send_mpeg_pes_packet (unsigned char *data, int len, int id, uint64_t pts, 
                          int type, int my_write (const unsigned char *data, int len));

/* Send MPEG <type> PS packet */
int send_mpeg_ps_packet (unsigned char *data, int len, int id, uint64_t pts, 
                         int type,int my_write (const unsigned char *data, int len));

/* Send MPEG 2 LPCM packet */
int send_mpeg_lpcm_packet (unsigned char *data, int len,
                           int id, uint64_t pts, int freq_id,
                           int my_write (const unsigned char *data, int len));

#endif /* MPLAYER_MPEG_PACKETIZER_H */
