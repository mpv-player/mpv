/*
 * based on libmpeg2/header.c by Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mpeg_hdr.h"

#include "mp_msg.h"

static float frameratecode2framerate[16] = {
  0,
  // Official mpeg1/2 framerates: (1-8)
  24000.0/1001, 24,25,
  30000.0/1001, 30,50,
  60000.0/1001, 60,
  // Xing's 15fps: (9)
  15,
  // libmpeg3's "Unofficial economy rates": (10-13)
  5,10,12,15,
  // some invalid ones: (14-15)
  0,0
};


int mp_header_process_sequence_header (mp_mpeg_header_t * picture, const unsigned char * buffer)
{
    int width, height;

    if ((buffer[6] & 0x20) != 0x20){
	fprintf(stderr, "missing marker bit!\n");
	return 1;	/* missing marker_bit */
    }

    height = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];

    picture->display_picture_width = (height >> 12);
    picture->display_picture_height = (height & 0xfff);

    width = ((height >> 12) + 15) & ~15;
    height = ((height & 0xfff) + 15) & ~15;

    picture->aspect_ratio_information = buffer[3] >> 4;
    picture->frame_rate_code = buffer[3] & 15;
    picture->fps=frameratecode2framerate[picture->frame_rate_code];
    picture->bitrate = (buffer[4]<<10)|(buffer[5]<<2)|(buffer[6]>>6);
    picture->mpeg1 = 1;
    picture->picture_structure = 3; //FRAME_PICTURE;
    picture->display_time=100;
    picture->frame_rate_extension_n = 1;
    picture->frame_rate_extension_d = 1;
    return 0;
}

static int header_process_sequence_extension (mp_mpeg_header_t * picture,
					      unsigned char * buffer)
{
    /* check chroma format, size extensions, marker bit */

    if ( ((buffer[1] & 0x06) == 0x00) ||
         ((buffer[1] & 0x01) != 0x00) || (buffer[2] & 0xe0) ||
         ((buffer[3] & 0x01) != 0x01) )
	return 1;

    picture->progressive_sequence = (buffer[1] >> 3) & 1;
    picture->frame_rate_extension_n = ((buffer[5] >> 5) & 3) + 1;
    picture->frame_rate_extension_d = (buffer[5] & 0x1f) + 1;

    picture->mpeg1 = 0;
    return 0;
}

static int header_process_picture_coding_extension (mp_mpeg_header_t * picture, unsigned char * buffer)
{
    picture->picture_structure = buffer[2] & 3;
    picture->top_field_first = buffer[3] >> 7;
    picture->repeat_first_field = (buffer[3] >> 1) & 1;
    picture->progressive_frame = buffer[4] >> 7;

    // repeat_first implementation by A'rpi/ESP-team, based on libmpeg3:
    picture->display_time=100;
    if(picture->repeat_first_field){
        if(picture->progressive_sequence){
            if(picture->top_field_first)
                picture->display_time+=200;
            else
                picture->display_time+=100;
        } else
        if(picture->progressive_frame){
                picture->display_time+=50;
        }
    }
    //temopral hack. We calc time on every field, so if we have 2 fields
    // interlaced we'll end with double time for 1 frame
    if( picture->picture_structure!=3 ) picture->display_time/=2;
    return 0;
}

int mp_header_process_extension (mp_mpeg_header_t * picture, unsigned char * buffer)
{
    switch (buffer[0] & 0xf0) {
    case 0x10:	/* sequence extension */
	return header_process_sequence_extension (picture, buffer);
    case 0x80:	/* picture coding extension */
	return header_process_picture_coding_extension (picture, buffer);
    }
    return 0;
}

float mpeg12_aspect_info(mp_mpeg_header_t *picture)
{
    float aspect = 0.0;

    switch(picture->aspect_ratio_information) {
      case 2:  // PAL/NTSC SVCD/DVD 4:3
      case 8:  // PAL VCD 4:3
      case 12: // NTSC VCD 4:3
        aspect=4.0/3.0;
        break;
      case 3:  // PAL/NTSC Widescreen SVCD/DVD 16:9
      case 6:  // (PAL?)/NTSC Widescreen SVCD 16:9
        aspect=16.0/9.0;
        break;
      case 4:  // according to ISO-138182-2 Table 6.3
        aspect=2.21;
        break;
      case 1:  // VGA 1:1 - do not prescale
      case 9: // Movie Type ??? / 640x480
        aspect=0.0;
        break;
      default:
        mp_msg(MSGT_DECVIDEO,MSGL_ERR,"Detected unknown aspect_ratio_information in mpeg sequence header.\n"
               "Please report the aspect value (%i) along with the movie type (VGA,PAL,NTSC,"
               "SECAM) and the movie resolution (720x576,352x240,480x480,...) to the MPlayer"
               " developers, so that we can add support for it!\nAssuming 1:1 aspect for now.\n",
               picture->aspect_ratio_information);
    }

    return aspect;
}

//MPEG4 HEADERS
unsigned char mp_getbits(unsigned char *buffer, unsigned int from, unsigned char len)
{
    unsigned int n;
    unsigned char m, u, l, y;

    n = from / 8;
    m = from % 8;
    u = 8 - m;
    l = (len > u ? len - u : 0);

    y = (buffer[n] << m);
    if(8 > len)
    	y  >>= (8-len);
    if(l)
    	y |= (buffer[n+1] >> (8-l));

    //fprintf(stderr, "GETBITS(%d -> %d): bytes=0x%x 0x%x, n=%d, m=%d, l=%d, u=%d, Y=%d\n",
    //	from, (int) len, (int) buffer[n],(int) buffer[n+1], n, (int) m, (int) l, (int) u, (int) y);
    return  y;
}

static inline unsigned int mp_getbits16(unsigned char *buffer, unsigned int from, unsigned char len)
{
    if(len > 8)
        return (mp_getbits(buffer, from, len - 8) << 8) | mp_getbits(buffer, from + len - 8, 8);
    else
        return mp_getbits(buffer, from, len);
}

#define getbits mp_getbits
#define getbits16 mp_getbits16

static int read_timeinc(mp_mpeg_header_t * picture, unsigned char * buffer, int n)
{
    if(picture->timeinc_bits > 8) {
      picture->timeinc_unit = getbits(buffer, n, picture->timeinc_bits - 8) << 8;
      n += picture->timeinc_bits - 8;
      picture->timeinc_unit |= getbits(buffer, n, 8);
      n += 8;
    } else {
      picture->timeinc_unit = getbits(buffer, n, picture->timeinc_bits);
      n += picture->timeinc_bits;
    }
    //fprintf(stderr, "TIMEINC2: %d, bits: %d\n", picture->timeinc_unit, picture->timeinc_bits);
    return n;
}

int mp4_header_process_vol(mp_mpeg_header_t * picture, unsigned char * buffer)
{
    unsigned int n, aspect=0, aspectw=0, aspecth=0,  x=1, v;

    //begins with 0x0000012x
    picture->fps = 0;
    picture->timeinc_bits = picture->timeinc_resolution = picture->timeinc_unit = 0;
    n = 9;
    if(getbits(buffer, n, 1))
      n += 7;
    n++;
    aspect=getbits(buffer, n, 4);
    n += 4;
    if(aspect == 0x0f) {
      aspectw = getbits(buffer, n, 8);
      n += 8;
      aspecth = getbits(buffer, n, 8);
      n += 8;
    }

    if(getbits(buffer, n, 1)) {
      n += 4;
      if(getbits(buffer, n, 1))
        n += 79;
      n++;
    } else n++;

    n+=3;

    picture->timeinc_resolution = getbits(buffer, n, 8) << 8;
    n += 8;
    picture->timeinc_resolution |= getbits(buffer, n, 8);
    n += 8;

    picture->timeinc_bits = 0;
    v = picture->timeinc_resolution - 1;
    while(v && (x<16)) {
      v>>=1;
      picture->timeinc_bits++;
    }
    picture->timeinc_bits = (picture->timeinc_bits > 1 ? picture->timeinc_bits : 1);

    n++; //marker bit

    if(getbits(buffer, n++, 1)) {      //fixed_vop_timeinc
      n += read_timeinc(picture, buffer, n);

      if(picture->timeinc_unit)
        picture->fps = (float) picture->timeinc_resolution / (float) picture->timeinc_unit;
    }

    n++; //marker bit
    picture->display_picture_width = getbits16(buffer, n, 13);
    n += 13;
    n++; //marker bit
    picture->display_picture_height = getbits16(buffer, n, 13);
    n += 13;

    //fprintf(stderr, "ASPECT: %d, PARW=%d, PARH=%d, TIMEINCRESOLUTION: %d, FIXED_TIMEINC: %d (number of bits: %d), FPS: %u\n",
    //	aspect, aspectw, aspecth, picture->timeinc_resolution, picture->timeinc_unit, picture->timeinc_bits, picture->fps);

    return 0;
}

void mp4_header_process_vop(mp_mpeg_header_t * picture, unsigned char * buffer)
{
  int n;
  n = 0;
  picture->picture_type = getbits(buffer, n, 2);
  n += 2;
  while(getbits(buffer, n, 1))
    n++;
  n++;
  getbits(buffer, n, 1);
  n++;
  n += read_timeinc(picture, buffer, n);
}

#define min(a, b) ((a) <= (b) ? (a) : (b))

static unsigned int read_golomb(unsigned char *buffer, unsigned int *init)
{
  unsigned int x, v = 0, v2 = 0, m, len = 0, n = *init;

  while(getbits(buffer, n++, 1) == 0)
    len++;

  x = len + n;
  while(n < x)
  {
    m = min(x - n, 8);
    v |= getbits(buffer, n, m);
    n += m;
    if(x - n > 8)
      v <<= 8;
  }

  v2 = 1;
  for(n = 0; n < len; n++)
    v2 <<= 1;
  v2 = (v2 - 1) + v;

  //fprintf(stderr, "READ_GOLOMB(%u), V=2^%u + %u-1 = %u\n", *init, len, v, v2);
  *init = x;
  return v2;
}

inline static int read_golomb_s(unsigned char *buffer, unsigned int *init)
{
  unsigned int v = read_golomb(buffer, init);
  return (v & 1) ? ((v + 1) >> 1) : -(v >> 1);
}

static int h264_parse_vui(mp_mpeg_header_t * picture, unsigned char * buf, unsigned int n)
{
  unsigned int overscan, vsp_color, chroma, timing, fixed_fps;

  if(getbits(buf, n++, 1))
  {
    picture->aspect_ratio_information = getbits(buf, n, 8);
    n += 8;
    if(picture->aspect_ratio_information == 255)
    {
      picture->display_picture_width = (getbits(buf, n, 8) << 8) | getbits(buf, n + 8, 8);
      n += 16;

      picture->display_picture_height = (getbits(buf, n, 8) << 8) | getbits(buf, n + 8, 8);
      n += 16;
    }
  }

  if((overscan=getbits(buf, n++, 1)))
    n++;
  if((vsp_color=getbits(buf, n++, 1)))
  {
    n += 4;
    if(getbits(buf, n++, 1))
      n += 24;
  }
  if((chroma=getbits(buf, n++, 1)))
  {
    read_golomb(buf, &n);
    read_golomb(buf, &n);
  }
  if((timing=getbits(buf, n++, 1)))
  {
    picture->timeinc_unit = (getbits(buf, n, 8) << 24) | (getbits(buf, n+8, 8) << 16) | (getbits(buf, n+16, 8) << 8) | getbits(buf, n+24, 8);
    n += 32;

    picture->timeinc_resolution = (getbits(buf, n, 8) << 24) | (getbits(buf, n+8, 8) << 16) | (getbits(buf, n+16, 8) << 8) | getbits(buf, n+24, 8);
    n += 32;

    fixed_fps = getbits(buf, n, 1);

    if(picture->timeinc_unit > 0 && picture->timeinc_resolution > 0)
      picture->fps = (float) picture->timeinc_resolution / (float) picture->timeinc_unit;
    if(fixed_fps)
      picture->fps /= 2;
  }

  //fprintf(stderr, "H264_PARSE_VUI, OVESCAN=%u, VSP_COLOR=%u, CHROMA=%u, TIMING=%u, DISPW=%u, DISPH=%u, TIMERES=%u, TIMEINC=%u, FIXED_FPS=%u\n", overscan, vsp_color, chroma, timing, picture->display_picture_width, picture->display_picture_height,
  //	picture->timeinc_resolution, picture->timeinc_unit, picture->timeinc_unit, fixed_fps);

  return n;
}

static int mp_unescape03(unsigned char *buf, int len);

int h264_parse_sps(mp_mpeg_header_t * picture, unsigned char * buf, int len)
{
  unsigned int n = 0, v, i, k, mbh;
  int frame_mbs_only;

  len = mp_unescape03(buf, len);

  picture->fps = picture->timeinc_unit = picture->timeinc_resolution = 0;
  n = 24;
  read_golomb(buf, &n);
  if(buf[0] >= 100){
    if(read_golomb(buf, &n) == 3)
      n++;
    read_golomb(buf, &n);
    read_golomb(buf, &n);
    n++;
    if(getbits(buf, n++, 1)){
      for(i = 0; i < 8; i++)
      {  // scaling list is skipped for now
        if(getbits(buf, n++, 1))
        {
          v = 8;
          for(k = (i < 6 ? 16 : 64); k && v; k--)
            v = (v + read_golomb_s(buf, &n)) & 255;
        }
      }
    }
  }
  read_golomb(buf, &n);
  v = read_golomb(buf, &n);
  if(v == 0)
    read_golomb(buf, &n);
  else if(v == 1)
  {
    getbits(buf, n++, 1);
    read_golomb(buf, &n);
    read_golomb(buf, &n);
    v = read_golomb(buf, &n);
    for(i = 0; i < v; i++)
      read_golomb(buf, &n);
  }
  read_golomb(buf, &n);
  getbits(buf, n++, 1);
  picture->display_picture_width = 16 *(read_golomb(buf, &n)+1);
  mbh = read_golomb(buf, &n)+1;
  frame_mbs_only = getbits(buf, n++, 1);
  picture->display_picture_height = 16 * (2 - frame_mbs_only) * mbh;
  if(!frame_mbs_only)
    getbits(buf, n++, 1);
  getbits(buf, n++, 1);
  if(getbits(buf, n++, 1))
  {
    read_golomb(buf, &n);
    read_golomb(buf, &n);
    read_golomb(buf, &n);
    read_golomb(buf, &n);
  }
  if(getbits(buf, n++, 1))
    n = h264_parse_vui(picture, buf, n);

  return n;
}

static int mp_unescape03(unsigned char *buf, int len)
{
  unsigned char *dest;
  int i, j, skip;

  dest = malloc(len);
  if(! dest)
    return 0;

  j = i = skip = 0;
  while(i <= len-3)
  {
    if(buf[i] == 0 && buf[i+1] == 0 && buf[i+2] == 3)
    {
      dest[j] = dest[j+1] = 0;
      j += 2;
      i += 3;
      skip++;
    }
    else
    {
      dest[j] = buf[i];
      j++;
      i++;
    }
  }
  dest[j] = buf[len-2];
  dest[j+1] = buf[len-1];
  len -= skip;
  memcpy(buf, dest, len);
  free(dest);

  return len;
}

int mp_vc1_decode_sequence_header(mp_mpeg_header_t * picture, unsigned char * buf, int len)
{
  int n, x;

  len = mp_unescape03(buf, len);

  picture->display_picture_width = picture->display_picture_height = 0;
  picture->fps = 0;
  n = 0;
  x = getbits(buf, n, 2);
  n += 2;
  if(x != 3) //not advanced profile
    return 0;

  getbits16(buf, n, 14);
  n += 14;
  picture->display_picture_width = getbits16(buf, n, 12) * 2 + 2;
  n += 12;
  picture->display_picture_height = getbits16(buf, n, 12) * 2 + 2;
  n += 12;
  getbits(buf, n, 6);
  n += 6;
  x = getbits(buf, n, 1);
  n += 1;
  if(x) //display info
  {
    getbits16(buf, n, 14);
    n += 14;
    getbits16(buf, n, 14);
    n += 14;
    if(getbits(buf, n++, 1)) //aspect ratio
    {
      x = getbits(buf, n, 4);
      n += 4;
      if(x == 15)
      {
        getbits16(buf, n, 16);
        n += 16;
      }
    }

    if(getbits(buf, n++, 1)) //framerates
    {
      int frexp=0, frnum=0, frden=0;

      if(getbits(buf, n++, 1))
      {
        frexp = getbits16(buf, n, 16);
        n += 16;
        picture->fps = (double) (frexp+1) / 32.0;
      }
      else
      {
        float frates[] = {0, 24000, 25000, 30000, 50000, 60000, 48000, 72000, 0};
        float frdivs[] = {0, 1000, 1001, 0};

        frnum = getbits(buf, n, 8);
        n += 8;
        frden = getbits(buf, n, 4);
        n += 4;
        if((frden == 1 || frden == 2) && (frnum < 8))
            picture->fps = frates[frnum] / frdivs[frden];
      }
    }
  }

  //free(dest);
  return 1;
}
