/*
 *  Copyright (C) 2006 Benjamin Zores
 *   Set of helper routines for building MPEG 1/2 PS/PES packets.
 *
 *   Based on various code bororwed from vo_mpegpes/vo_dxr2 :
 *      (C) 2000 Ralph Metzler <ralph@convergence.de>
 *               Marcus Metzler <marcus@convergence.de>
 *               Gerard Lantau
 *
 *   This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#include "mp_msg.h"
#include "mpeg_packetizer.h"

#define PES_MAX_SIZE 2048

static unsigned char pes_header[PES_MAX_SIZE];

static unsigned char ps2_header[] = {
  0x00, 0x00, 0x01, 0xba, 0x44, 0x00, 0x04, 0x00,
  0x04, 0x01, 0x01, 0x86, 0xa3, 0xf8
};


static unsigned char ps1_header[] = {
  0x00, 0x00, 0x01, 0xba, 0x21, 0x00,
  0xb9, 0x37, 0x83, 0x80, 0xc3, 0x51,
};

/* Send MPEG <type> PES packet */
int
send_mpeg_pes_packet (unsigned char *data, int len, int id, uint64_t pts,
                      int type, int my_write (unsigned char *data, int len))
{
  int ptslen = 5;
  int n = 0;
  int idx, plen;
  int hdr;

  mp_msg (MSGT_HEADER, MSGL_DBG2,
          "MPEG2 PES packet: 0x%x => %lu   \n", id, pts);
  memset (pes_header, '\0', PES_MAX_SIZE);
  
  /* startcode */
  pes_header[0] = 0;
  pes_header[1] = 0;
  pes_header[2] = 0x01;
  pes_header[3] = id; /* stream id */

  while (len > 0)
  {
    int payload_size = len;  /* data + PTS */
    if(type == 2)
        hdr = 3;
    else
        hdr = (ptslen ? 0 : 1);
    if (6 + hdr + ptslen + payload_size > PES_MAX_SIZE)
      payload_size = PES_MAX_SIZE - 6 - hdr - ptslen;

    /* construct PES header: packetize */
    plen = payload_size + hdr + ptslen;
    pes_header[4] = plen >> 8;
    pes_header[5] = plen & 255;
    idx = 6;
    
    if (ptslen)
    {
      int x;
      
      if(type == 2)
      {
        pes_header[idx++] = 0x81;
        pes_header[idx++] = 0x80;
        pes_header[idx++] = ptslen;
      }
      
      /* presentation time stamp */
      x = (0x02 << 4) | (((pts >> 30) & 0x07) << 1) | 1;
      pes_header[idx++] = x;
      
      x = ((((pts >> 15) & 0x7fff) << 1) | 1);
      pes_header[idx++] = x >>8;
      pes_header[idx++] = x & 255;
      
      x = (((pts & 0x7fff) << 1) | 1);
      pes_header[idx++] = x >> 8;
      pes_header[idx++] = x & 255;
    }
    else
    {
      if(type == 2)
      {
        pes_header[idx++] = 0x81;
        pes_header[idx++] = 0x00;
        pes_header[idx++] = 0x00;
      }
      else
        pes_header[idx++] = 0x0f;
    }
    
    my_write (pes_header, idx);
    n = my_write (data, payload_size);

    len -= n;
    data += n;
    ptslen = 0; /* store PTS only once, at first packet! */
  }

  return n;
}

/* Send MPEG <type> PS packet */
int
send_mpeg_ps_packet (unsigned char *data, int len, int id, uint64_t pts, int type,
                      int my_write (unsigned char *data, int len))
{
  if(type == 2)
    my_write (ps2_header, sizeof (ps2_header));
  else
    my_write (ps1_header, sizeof (ps1_header));
  return send_mpeg_pes_packet (data, len, id, pts, type, my_write);
}

/* Send MPEG 2 LPCM packet */
int
send_mpeg_lpcm_packet (unsigned char* data, int len,
                       int id, uint64_t pts, int freq_id,
                       int my_write (unsigned char *data, int len))
{
  int ptslen = pts ? 5 : 0;
  int n = 0;

  mp_msg (MSGT_HEADER, MSGL_DBG2,
          "MPEG LPCM packet: 0x%x => %lu   \n", id, pts);
  memset (pes_header, '\0', PES_MAX_SIZE);
  
  /* startcode */
  pes_header[0] = 0;
  pes_header[1] = 0;
  pes_header[2] = 1;
  pes_header[3] = 0xBD; /* stream id */
    
  while (len >= 4)
  {
    int payload_size = PES_MAX_SIZE - 6 - 20; /* max possible data len */
    if( payload_size > len)
      payload_size = len;

    payload_size &= ~3; /* align! */

    /* packetsize */
    pes_header[4] = (payload_size + 3 + ptslen + 7) >> 8;
    pes_header[5] = (payload_size + 3 + ptslen + 7) & 255;

    /* stuffing */
    /* TTCCxxxx  CC=css TT=type: 1=STD 0=mpeg1 2=vob */
    pes_header[6] = 0x81;
	      
    /* FFxxxxxx   FF=pts flags=2 vs 0 */
    pes_header[7] = ptslen ? 0x80 : 0;

    /* header length */
    pes_header[8] = ptslen;
	      
    if (ptslen)
    {
      int x;

      /* presentation time stamp */
      x = (0x02 << 4) | (((pts >> 30) & 0x07) << 1) | 1;
      pes_header[9] = x;
      
      x = ((((pts >> 15) & 0x7fff) << 1) | 1);
      pes_header[10] = x >> 8;
      pes_header[11] = x &255;
      
      x = (((pts & 0x7fff) << 1) | 1);
      pes_header[12] = x >> 8;
      pes_header[13] = x & 255;
    }
	      
    /* ============ LPCM header: (7 bytes) ================= */
    /* Info by mocm@convergence.de */
    
    /* ID */
    pes_header[ptslen + 9] = id;

    /* number of frames */
    pes_header[ptslen + 10] = 0x07;

    /* first acces unit pointer, i.e. start of audio frame */
    pes_header[ptslen + 11] = 0x00;
    pes_header[ptslen + 12] = 0x04;

    /* audio emphasis on-off                                  1 bit */
    /* audio mute on-off                                      1 bit */
    /* reserved                                               1 bit */
    /* audio frame number                                     5 bit */
    pes_header[ptslen + 13] = 0x0C;

    /* quantization word length                               2 bit */
    /* audio sampling frequency (48khz = 0, 96khz = 1)        2 bit */
    /* reserved                                               1 bit */
    /* number of audio channels - 1 (e.g. stereo = 1)         3 bit */
    pes_header[ptslen + 14] = 1 | (freq_id << 4);

    /* dynamic range control (0x80 if off) */
    pes_header [ptslen + 15] = 0x80;

    memcpy (&pes_header[6 + 3 + ptslen + 7], data, payload_size);
    n += my_write (pes_header, 6 + 3 + ptslen + 7 + payload_size);

    len -= payload_size;
    data += payload_size;
    ptslen = 0; /* store PTS only once, at first packet! */
  }

  return n;
}
