/*
 * set of helper routines for building MPEG 1/2 PS/PES packets
 *
 * Copyright (C) 2006 Benjamin Zores
 *
 * Based on code borrowed from vo_mpegpes/vo_dxr2:
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

#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#include "mp_msg.h"
#include "mpeg_packetizer.h"

#define PES_MAX_SIZE 2048

static const unsigned char ps2_header[] = {
  0x00, 0x00, 0x01, 0xba, 0x44, 0x00, 0x04, 0x00,
  0x04, 0x01, 0x01, 0x86, 0xa3, 0xf8
};


static const unsigned char ps1_header[] = {
  0x00, 0x00, 0x01, 0xba, 0x21, 0x00,
  0xb9, 0x37, 0x83, 0x80, 0xc3, 0x51,
};

/* Send MPEG <type> PES packet */
static int
send_mpeg_pes_packet_ll(unsigned char *data, int len, int id, uint64_t pts,
                      int type, unsigned char *header, int header_len,
                      int align4, int my_write (const unsigned char *data, int len))
{
  int ptslen = (pts ? 5 : 0);
  int n = 0;
  int idx, plen;
  int hdr;
  unsigned char pes_header[PES_MAX_SIZE];

  mp_msg (MSGT_HEADER, MSGL_DBG2,
          "MPEG%d PES packet: 0x%x => %"PRIu64"   \n", type, id, pts);
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
    if (6 + hdr + ptslen + payload_size + header_len > PES_MAX_SIZE)
      payload_size = PES_MAX_SIZE - 6 - hdr - ptslen - header_len;
    if(align4)
      payload_size &= ~3;

    /* construct PES header: packetize */
    plen = payload_size + hdr + ptslen + header_len;
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

    if(header_len)
    {
        memcpy(&pes_header[idx], header, header_len);
        idx += header_len;
    }

    my_write (pes_header, idx);
    n = my_write (data, payload_size);

    len -= n;
    data += n;
    ptslen = 0; /* store PTS only once, at first packet! */
    if(align4 && len < 4)
      break;
  }

  return n;
}

int
send_mpeg_pes_packet (unsigned char *data, int len, int id, uint64_t pts,
                      int type, int my_write (const unsigned char *data, int len))
{
    return send_mpeg_pes_packet_ll(data, len, id, pts, type, NULL, 0, 0, my_write);
}


/* Send MPEG <type> PS packet */
int
send_mpeg_ps_packet(unsigned char *data, int len, int id, uint64_t pts, int type,
                      int my_write (const unsigned char *data, int len))
{
  if(type == 2)
    my_write (ps2_header, sizeof (ps2_header));
  else
    my_write (ps1_header, sizeof (ps1_header));
  return send_mpeg_pes_packet (data, len, id, pts, type, my_write);
}

/* Send MPEG 2 LPCM packet */
int
send_mpeg_lpcm_packet(unsigned char* data, int len,
                       int id, uint64_t pts, int freq_id,
                       int my_write (const unsigned char *data, int len))
{
    unsigned char header[7] = {0xA0, 0x07, 0x00, 0x04, 0x0C, 1 | (freq_id << 4), 0x80};
    return send_mpeg_pes_packet_ll(data, len, 0xBD, pts, 2, header, sizeof(header), 1, my_write);
}
