/*
 * MPEG-ES video parser
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "parse_es.h"

//static unsigned char videobuffer[MAX_VIDEO_PACKET_SIZE];
unsigned char* videobuffer=NULL;
int videobuf_len=0;
int next_nal = -1;
///! legacy variable, 4 if stream is synced, 0 if not
int videobuf_code_len=0;

#define MAX_SYNCLEN (10 * 1024 * 1024)
// sync video stream, and returns next packet code
int sync_video_packet(demux_stream_t *ds){
  if (!videobuf_code_len) {
  int skipped=0;
    if (!demux_pattern_3(ds, NULL, MAX_SYNCLEN, &skipped, 0x100)) {
      if (skipped == MAX_SYNCLEN)
      mp_msg(MSGT_DEMUXER, MSGL_ERR, "parse_es: could not sync video stream!\n");
      goto eof_out;
    }
    next_nal = demux_getc(ds);
    if (next_nal < 0)
      goto eof_out;
    videobuf_code_len = 4;
  if(skipped) mp_dbg(MSGT_PARSEES,MSGL_DBG2,"videobuf: %d bytes skipped  (next: 0x1%02X)\n",skipped,next_nal);
  }
  return 0x100|next_nal;

eof_out:
  next_nal = -1;
  videobuf_code_len = 0;
  return 0;
}

// return: packet length
int read_video_packet(demux_stream_t *ds){
int packet_start;
  int res, read;

  if (VIDEOBUFFER_SIZE - videobuf_len < 5)
    return 0;
  // SYNC STREAM
//  if(!sync_video_packet(ds)) return 0; // cannot sync (EOF)

  // COPY STARTCODE:
  packet_start=videobuf_len;
  videobuffer[videobuf_len+0]=0;
  videobuffer[videobuf_len+1]=0;
  videobuffer[videobuf_len+2]=1;
  videobuffer[videobuf_len+3]=next_nal;
  videobuf_len+=4;

  // READ PACKET:
  res = demux_pattern_3(ds, &videobuffer[videobuf_len],
          VIDEOBUFFER_SIZE - videobuf_len, &read, 0x100);
  videobuf_len += read;
  if (!res)
    goto eof_out;

  videobuf_len-=3;

  mp_dbg(MSGT_PARSEES,MSGL_DBG2,"videobuf: packet 0x1%02X  len=%d  (total=%d)\n",videobuffer[packet_start+3],videobuf_len-packet_start,videobuf_len);

  // Save next packet code:
  next_nal = demux_getc(ds);
  if (next_nal < 0)
    goto eof_out;
  videobuf_code_len=4;

  return videobuf_len-packet_start;

eof_out:
  next_nal = -1;
  videobuf_code_len = 0;
  return videobuf_len - packet_start;
}

// return: next packet code
int skip_video_packet(demux_stream_t *ds){

  // SYNC STREAM
//  if(!sync_video_packet(ds)) return 0; // cannot sync (EOF)

  videobuf_code_len=0; // force resync

  // SYNC AGAIN:
  return sync_video_packet(ds);
}

/* stripped down version of a52_syncinfo() from liba52
 * copyright belongs to Michel Lespinasse <walken@zoy.org>
 * and Aaron Holtzman <aholtzma@ess.engr.uvic.ca> */
int mp_a52_framesize(uint8_t * buf, int *srate)
{
    int rate[] = {  32,  40,  48,  56,  64,  80,  96, 112,
                   128, 160, 192, 224, 256, 320, 384, 448,
                   512, 576, 640
    };
    uint8_t halfrate[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3 };
    int frmsizecod, bitrate, half;

    if ((buf[0] != 0x0b) || (buf[1] != 0x77))    /* syncword */
        return 0;

    if (buf[5] >= 0x60)                          /* bsid >= 12 */
        return 0;

    half = halfrate[buf[5] >> 3];

    frmsizecod = buf[4] & 63;
    if (frmsizecod >= 38)
        return 0;

    bitrate = rate[frmsizecod >> 1];

    switch (buf[4] & 0xc0) {
    case 0:    /* 48 KHz */
        *srate = 48000 >> half;
        return 4 * bitrate;
    case 0x40: /* 44.1 KHz */
        *srate = 44100 >> half;
        return 2 * (320 * bitrate / 147 + (frmsizecod & 1));
    case 0x80: /* 32 KHz */
        *srate = 32000 >> half;
        return 6 * bitrate;
    }

    return 0;
}
