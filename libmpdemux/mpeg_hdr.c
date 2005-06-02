
// based on libmpeg2/header.c by Aaron Holtzman <aholtzma@ess.engr.uvic.ca>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mpeg_hdr.h"

static int frameratecode2framerate[16] = {
  0,
  // Official mpeg1/2 framerates: (1-8)
  24000*10000/1001, 24*10000,25*10000,
  30000*10000/1001, 30*10000,50*10000,
  60000*10000/1001, 60*10000,
  // Xing's 15fps: (9)
  15*10000,
  // libmpeg3's "Unofficial economy rates": (10-13)
  5*10000,10*10000,12*10000,15*10000,
  // some invalid ones: (14-15)
  0,0
};


int mp_header_process_sequence_header (mp_mpeg_header_t * picture, unsigned char * buffer)
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

#define getbits mp_getbits

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
    
    if(getbits(buffer, n, 1)) {	//fixed_vop_timeinc
      n++;
      n = read_timeinc(picture, buffer, n);
      
      if(picture->timeinc_unit)
        picture->fps = (picture->timeinc_resolution * 10000) / picture->timeinc_unit;
    }
    
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
  n = read_timeinc(picture, buffer, n);
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
      picture->fps = ((uint64_t)picture->timeinc_resolution * 10000) / picture->timeinc_unit;
  }
  
  //fprintf(stderr, "H264_PARSE_VUI, OVESCAN=%u, VSP_COLOR=%u, CHROMA=%u, TIMING=%u, DISPW=%u, DISPH=%u, TIMERES=%u, TIMEINC=%u, FIXED_FPS=%u\n", overscan, vsp_color, chroma, timing, picture->display_picture_width, picture->display_picture_height,
  //	picture->timeinc_resolution, picture->timeinc_unit, picture->timeinc_unit, fixed_fps);
  
  return n;
}

int h264_parse_sps(mp_mpeg_header_t * picture, unsigned char * buf, int len)
{
  unsigned int n = 0, v, i, j, mbh;
  unsigned char *dest;
  int frame_mbs_only;

  dest = (unsigned char*) malloc(len);
  if(! dest)
    return 0;
  j = i = 0;
  while(i <= len-3)
  {
    if(buf[i] == 0 && buf[i+1] == 0 && buf[i+2] == 3)
    {
      dest[j] = dest[j+1] = 0;
      j += 2;
      i += 3;
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
  j += 2;
  len = j+1;
  buf = dest;
  
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
      //FIXME scaling matrix
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

  free(dest);
  return n;
}
