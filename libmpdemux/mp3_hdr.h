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

#ifndef MPLAYER_MP3_HDR_H
#define MPLAYER_MP3_HDR_H

#include <stddef.h>

int mp_get_mp3_header(unsigned char* hbuf,int* chans, int* freq, int* spf, int* mpa_layer, int* br);

#define mp_decode_mp3_header(hbuf)  mp_get_mp3_header(hbuf,NULL,NULL,NULL,NULL,NULL)

static inline int mp_check_mp3_header(unsigned int head){
    if( (head & 0x0000e0ff) != 0x0000e0ff ||
        (head & 0x00fc0000) == 0x00fc0000) return 0;
    if(mp_decode_mp3_header((unsigned char*)(&head))<=0) return 0;
    return 1;
}

#endif /* MPLAYER_MP3_HDR_H */
