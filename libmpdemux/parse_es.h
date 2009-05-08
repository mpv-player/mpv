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

#ifndef MPLAYER_PARSE_ES_H
#define MPLAYER_PARSE_ES_H

#include "demuxer.h"

#define MAX_VIDEO_PACKET_SIZE (224*1024+4)
#define VIDEOBUFFER_SIZE 0x100000

extern unsigned char* videobuffer;
extern int videobuf_len;
extern unsigned char videobuf_code[4];
extern int videobuf_code_len;

// sync video stream, and returns next packet code
int sync_video_packet(demux_stream_t *ds);

// return: packet length
int read_video_packet(demux_stream_t *ds);

// return: next packet code
int skip_video_packet(demux_stream_t *ds);

#endif /* MPLAYER_PARSE_ES_H */
