/* 
 * Copyright (C) 2002 the xine project
 * 
 * This file is part of xine, a unix video player.
 * 
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../config.h"
#include "bswap.h"

/* variable length (bit) code */
typedef struct vlc_code_s {
  int16_t	 value	:10,
		 length	:6;
} vlc_code_t;

#define VIDEOBUFSIZE	1280 * 1024

//char temp_buf[VIDEOBUFSIZE];

#define MEDIAN(a,b,c)	((a < b != b >= c) ? b : ((a < c != c > b) ? c : a))

#include "svq1.h"
#include "svq1_cb.h"

#ifdef USE_LIBAVCODEC
typedef void (*op_pixels_func)(unsigned char *block, const unsigned char *pixels, int line_size, int h);
extern op_pixels_func put_pixels_tab[4];
extern op_pixels_func put_no_rnd_pixels_tab[4];
//#define HAVE_AV_CONFIG_H
#endif

#ifdef HAVE_AV_CONFIG_H
// use libavcodec's get_bits():
//#define ALT_BITSTREAM_READER
//#define ALIGNED_BITSTREAM
#include "../../libavcodec/common.h"
#define bit_buffer_t GetBitContext
#define get_bit_cache(buf) (show_bits(buf,24)<<8)
//#define get_bit_cache(buf) show_bits(buf,32)
#else
// use built-in version:

/* memory bit stream */
typedef struct bit_buffer_s {
  uint8_t	*buffer;
  uint32_t	 bitpos;
} bit_buffer_t;

static inline void skip_bits(bit_buffer_t *bitbuf, int n){
  bitbuf->bitpos+=n;
}

static void init_get_bits(bit_buffer_t *bitbuf, 
                   unsigned char *buffer, int buffer_size){
  bitbuf->buffer=buffer;
  bitbuf->bitpos=0;
}

static inline uint32_t get_bits (bit_buffer_t *bitbuf, int count) {
  uint32_t result;

  /* load 32 bits of data (byte-aligned) */
  result   = be2me_32 (*((uint32_t *) &bitbuf->buffer[bitbuf->bitpos >> 3]));

  /* compensate for sub-byte offset */
  result <<= (bitbuf->bitpos & 0x7);

  /* flush num bits */
  bitbuf->bitpos += count;

  /* return num bits */
  return result >> (32 - count);
}

/*
 * Return next 32 bits (left aligned).
 */
static inline uint32_t get_bit_cache(bit_buffer_t *bitbuf) {
  uint32_t result;

  /* load 32 bits of data (byte-aligned) */
  result   = be2me_32 (*((uint32_t *) &bitbuf->buffer[bitbuf->bitpos >> 3]));

  /* compensate for sub-byte offset */
  result <<= (bitbuf->bitpos & 0x7);

  return result;
}

#endif

static int decode_svq1_block (bit_buffer_t *bitbuf, uint8_t *pixels, int pitch, int intra) {
  uint32_t    bit_cache;
  vlc_code_t *vlc;
  uint8_t    *list[63];
  uint32_t   *dst;
  uint32_t   *codebook;
  int	      entries[6];
  int	      i, j, m, n;
  int	      mean, stages;
  int	      x, y, width, height, level;
  uint32_t    n1, n2, n3, n4;

  /* initialize list for breadth first processing of vectors */
  list[0] = pixels;

  /* recursively process vector */
  for (i=0, m=1, n=1, level=5; i < n; i++) {
    for (; level > 0; i++) {

      /* process next depth */
      if (i == m) {
	m = n;

	if (--level == 0)
	  break;
      }

      /* divide block if next bit set */
      if (get_bits (bitbuf, 1) == 0)
	break;

      /* add child nodes */
      list[n++] = list[i];
      list[n++] = list[i] + (((level & 1) ? pitch : 1) << ((level / 2) + 1));
    }

    /* destination address and vector size */
    dst = (uint32_t *) list[i];
    width = 1 << ((4 + level) /2);
    height = 1 << ((3 + level) /2);

    /* get number of stages (-1 skips vector, 0 for mean only) */
    bit_cache = get_bit_cache (bitbuf);

    if (intra)
      vlc = &intra_vector_tables[level][bit_cache >> (32 - 7)];
    else
      vlc = &inter_vector_tables[level][bit_cache >> (32 - 6)];

    /* flush bits */
    stages	    = vlc->value;
    skip_bits(bitbuf,vlc->length);

    if (stages == -1) {
      if (intra) {
	for (y=0; y < height; y++) {
	  memset (&dst[y*(pitch / 4)], 0, width);
	}
      }
      continue;		/* skip vector */
    }

    if ((stages > 0) && (level >= 4)) {
      return -1;	/* invalid vector */
    }

    /* get mean value for vector */
    bit_cache = get_bit_cache (bitbuf);

    if (intra) {
      if (bit_cache >= 0x25000000)
	vlc = &intra_mean_table_0[(bit_cache >> (32 - 8)) - 37];
      else if (bit_cache >= 0x03400000)
	vlc = &intra_mean_table_1[(bit_cache >> (32 - 10)) - 13];
      else if (bit_cache >= 0x00040000) 
	vlc = &intra_mean_table_2[(bit_cache >> (32 - 14)) - 1];
      else
	vlc = &intra_mean_table_3[bit_cache >> (32 - 20)];
    } else {
      if (bit_cache >= 0x0B000000)
	vlc = &inter_mean_table_0[(bit_cache >> (32 - 8)) - 11];
      else if (bit_cache >= 0x01200000)
	vlc = &inter_mean_table_1[(bit_cache >> (32 - 12)) - 18];
      else if (bit_cache >= 0x002E0000) 
	vlc = &inter_mean_table_2[(bit_cache >> (32 - 15)) - 23];
      else if (bit_cache >= 0x00094000)
	vlc = &inter_mean_table_3[(bit_cache >> (32 - 18)) - 37];
      else if (bit_cache >= 0x00049000)
	vlc = &inter_mean_table_4[(bit_cache >> (32 - 20)) - 73];
      else
	vlc = &inter_mean_table_5[bit_cache >> (32 - 22)];
    }

    /* flush bits */
    mean	    = vlc->value;
    skip_bits(bitbuf,vlc->length);

    if (intra && stages == 0) {
      for (y=0; y < height; y++) {
	memset (&dst[y*(pitch / 4)], mean, width);
      }
    } else {
      codebook = (uint32_t *) (intra ? intra_codebooks[level] : inter_codebooks[level]);
      bit_cache = get_bits (bitbuf, 4*stages);

      /* calculate codebook entries for this vector */
      for (j=0; j < stages; j++) {
	entries[j] = (((bit_cache >> (4*(stages - j - 1))) & 0xF) + 16*j) << (level + 1);
      }

      mean -= (stages * 128);
      n4    = ((mean + (mean >> 31)) << 16) | (mean & 0xFFFF);

      for (y=0; y < height; y++) {
	for (x=0; x < (width / 4); x++, codebook++) {
	  if (intra) {
	    n1 = n4;
	    n2 = n4;
	  } else {
	    n3 = dst[x];

	    /* add mean value to vector */
	    n1 = ((n3 & 0xFF00FF00) >> 8) + n4;
	    n2 =  (n3 & 0x00FF00FF)	  + n4;
	  }

	  /* add codebook entries to vector */
	  for (j=0; j < stages; j++) {
	    n3  = codebook[entries[j]] ^ 0x80808080;
	    n1 += ((n3 & 0xFF00FF00) >> 8);
	    n2 +=  (n3 & 0x00FF00FF);
	  }

	  /* clip to [0..255] */
	  if (n1 & 0xFF00FF00) {
	    n3  = ((( n1 >> 15) & 0x00010001) | 0x01000100) - 0x00010001;
	    n1 += 0x7F007F00;
	    n1 |= (((~n1 >> 15) & 0x00010001) | 0x01000100) - 0x00010001;
	    n1 &= (n3 & 0x00FF00FF);
	  }

	  if (n2 & 0xFF00FF00) {
	    n3  = ((( n2 >> 15) & 0x00010001) | 0x01000100) - 0x00010001;
	    n2 += 0x7F007F00;
	    n2 |= (((~n2 >> 15) & 0x00010001) | 0x01000100) - 0x00010001;
	    n2 &= (n3 & 0x00FF00FF);
	  }

	  /* store result */
	  dst[x] = (n1 << 8) | n2;
        }

        dst += (pitch / 4);
      }
    }
  }

  return 0;
}

static int decode_motion_vector (bit_buffer_t *bitbuf, svq1_pmv_t *mv, svq1_pmv_t **pmv) {
  uint32_t    bit_cache;
  vlc_code_t *vlc;
  int	      diff, sign;
  int	      i;

  for (i=0; i < 2; i++) {

    /* get motion code */
    bit_cache = get_bit_cache (bitbuf);

    if (!(bit_cache & 0xFFE00000))
      return -1;	/* invalid vlc code */

    if (bit_cache & 0x80000000) {
      diff = 0;

      /* flush bit */
      skip_bits(bitbuf,1);

    } else {
      if (bit_cache >= 0x06000000) {
        vlc = &motion_table_0[(bit_cache >> (32 - 7)) - 3];
      } else {
        vlc = &motion_table_1[(bit_cache >> (32 - 12)) - 2];
      }

      /* decode motion vector differential */
      sign = (int) (bit_cache << (vlc->length - 1)) >> 31;
      diff = (vlc->value ^ sign) - sign;

      /* flush bits */
      skip_bits(bitbuf,vlc->length);
    }

    /* add median of motion vector predictors and clip result */
    if (i == 1)
      mv->y = ((diff + MEDIAN(pmv[0]->y, pmv[1]->y, pmv[2]->y)) << 26) >> 26;
    else
      mv->x = ((diff + MEDIAN(pmv[0]->x, pmv[1]->x, pmv[2]->x)) << 26) >> 26;
  }

  return 0;
}

static void skip_block (uint8_t *current, uint8_t *previous, int pitch, int x, int y) {
  uint8_t *src;
  uint8_t *dst;
  int	   i;

  src = &previous[x + y*pitch];
  dst = current;

  for (i=0; i < 16; i++) {
    memcpy (dst, src, 16);
    src += pitch;
    dst += pitch;
  }
}

static int motion_inter_block (bit_buffer_t *bitbuf,
			       uint8_t *current, uint8_t *previous, int pitch,
			       svq1_pmv_t *motion, int x, int y) {
  uint8_t    *src;
  uint8_t    *dst;
  svq1_pmv_t  mv;
  svq1_pmv_t *pmv[3];
  int	      sx, sy;
  int	      result;

  /* predict and decode motion vector */
  pmv[0] = &motion[0];
  pmv[1] = &motion[(x / 8) + 2];
  pmv[2] = &motion[(x / 8) + 4];

  if (y == 0) {
    pmv[1] = pmv[0];
    pmv[2] = pmv[0];
  }

  result = decode_motion_vector (bitbuf, &mv, pmv);

  if (result != 0)
    return result;

  motion[0].x		= mv.x;
  motion[0].y		= mv.y;
  motion[(x / 8) + 2].x	= mv.x;
  motion[(x / 8) + 2].y	= mv.y;
  motion[(x / 8) + 3].x	= mv.x;
  motion[(x / 8) + 3].y	= mv.y;

  src = &previous[(x + (mv.x >> 1)) + (y + (mv.y >> 1))*pitch];
  dst = current;

#ifdef USE_LIBAVCODEC
  put_pixels_tab[((mv.y & 1) << 1) | (mv.x & 1)](dst,src,pitch,16);
  put_pixels_tab[((mv.y & 1) << 1) | (mv.x & 1)](dst+8,src+8,pitch,16);
#else
  /* form prediction */
  if (mv.y & 0x1) {
    if (mv.x & 0x1) {
      for (sy=0; sy < 16; sy++) {
	for (sx=0; sx < 16; sx++) {
	  dst[sx] = (src[sx] + src[sx + 1] + src[sx + pitch] + src[sx + pitch + 1] + 2) >> 2;
	}
	src += pitch;
	dst += pitch;
      }
    } else {
      for (sy=0; sy < 16; sy++) {
	for (sx=0; sx < 16; sx++) {
	  dst[sx] = (src[sx] + src[sx + pitch] + 1) >> 1;
	}
	src += pitch;
	dst += pitch;
      }
    }
  } else {
    if (mv.x & 0x1) {
      for (sy=0; sy < 16; sy++) {
	for (sx=0; sx < 16; sx++) {
	  dst[sx] = (src[sx] + src[sx + 1] + 1) >> 1;
	}
	src += pitch;
	dst += pitch;
      }
    } else {
      for (sy=0; sy < 16; sy++) {
	memcpy (dst, src, 16);
	src += pitch;
	dst += pitch;
      }
    }
  }
#endif

  return 0;
}

static int motion_inter_4v_block (bit_buffer_t *bitbuf,
				  uint8_t *current, uint8_t *previous, int pitch,
				  svq1_pmv_t *motion,int x, int y) {
  uint8_t    *src;
  uint8_t    *dst;
  svq1_pmv_t  mv;
  svq1_pmv_t *pmv[4];
  int	      sx, sy;
  int	      i, result;

  /* predict and decode motion vector (0) */
  pmv[0] = &motion[0];
  pmv[1] = &motion[(x / 8) + 2];
  pmv[2] = &motion[(x / 8) + 4];

  if (y == 0) {
    pmv[1] = pmv[0];
    pmv[2] = pmv[0];
  }

  result = decode_motion_vector (bitbuf, &mv, pmv);

  if (result != 0)
    return result;

  /* predict and decode motion vector (1) */
  pmv[0] = &mv;
  pmv[1] = &motion[(x / 8) + 3];

  if (y == 0) {
    pmv[1] = pmv[0];
    pmv[2] = pmv[0];
  }

  result = decode_motion_vector (bitbuf, &motion[0], pmv);

  if (result != 0)
    return result;

  /* predict and decode motion vector (2) */
  pmv[1] = &motion[0];
  pmv[2] = &motion[(x / 8) + 1];

  result = decode_motion_vector (bitbuf, &motion[(x / 8) + 2], pmv);

  if (result != 0)
    return result;

  /* predict and decode motion vector (3) */
  pmv[2] = &motion[(x / 8) + 2];
  pmv[3] = &motion[(x / 8) + 3];

  result = decode_motion_vector (bitbuf, pmv[3], pmv);

  if (result != 0)
    return result;

  /* form predictions */
  for (i=0; i < 4; i++) {
    src = &previous[(x + (pmv[i]->x >> 1)) + (y + (pmv[i]->y >> 1))*pitch];
    dst = current;

#ifdef USE_LIBAVCODEC
    put_pixels_tab[((pmv[i]->y & 1) << 1) | (pmv[i]->x & 1)](dst,src,pitch,8);
#else
    if (pmv[i]->y & 0x1) {
      if (pmv[i]->x & 0x1) {
	for (sy=0; sy < 8; sy++) {
	  for (sx=0; sx < 8; sx++) { 
	    dst[sx] = (src[sx] + src[sx + 1] + src[sx + pitch] + src[sx + pitch + 1] + 2) >> 2;
	  }
	  src += pitch;
	  dst += pitch;
	}
      } else {
	for (sy=0; sy < 8; sy++) {
	  for (sx=0; sx < 8; sx++) {
	    dst[sx] = (src[sx] + src[sx + pitch] + 1) >> 1;
	  }
	  src += pitch;
	  dst += pitch;
	}
      }
    } else {
      if (pmv[i]->x & 0x1) {
	for (sy=0; sy < 8; sy++) {
	  for (sx=0; sx < 8; sx++) {
	    dst[sx] = (src[sx] + src[sx + 1] + 1) >> 1;
	  }
	  src += pitch;
	  dst += pitch;
	}
      } else {
	for (sy=0; sy < 8; sy++) {
	  memcpy (dst, src, 8);
	  src += pitch;
	  dst += pitch;
	}
      }
    }
#endif

    /* select next block */
    if (i & 1) {
      current  += 8*(pitch - 1);
      previous += 8*(pitch - 1);
    } else {
      current  += 8;
      previous += 8;
    }
  }

  return 0;
}

static int decode_delta_block (bit_buffer_t *bitbuf,
			uint8_t *current, uint8_t *previous, int pitch,
			svq1_pmv_t *motion, int x, int y) {
  uint32_t bit_cache;
  uint32_t block_type;
  int	   result = 0;

  /* get block type */
  bit_cache = get_bit_cache (bitbuf);

  bit_cache	>>= (32 - 3);
  block_type	  = block_type_table[bit_cache].value;
  skip_bits(bitbuf,block_type_table[bit_cache].length);

  /* reset motion vectors */
  if (block_type == SVQ1_BLOCK_SKIP || block_type == SVQ1_BLOCK_INTRA) {
    motion[0].x		  = 0;
    motion[0].y		  = 0;
    motion[(x / 8) + 2].x = 0;
    motion[(x / 8) + 2].y = 0;
    motion[(x / 8) + 3].x = 0;
    motion[(x / 8) + 3].y = 0;
  }

  switch (block_type) {
  case SVQ1_BLOCK_SKIP:
    skip_block (current, previous, pitch, x, y);
    break;

  case SVQ1_BLOCK_INTER:
    result = motion_inter_block (bitbuf, current, previous, pitch, motion, x, y);

    if (result != 0)
      break;

    result = decode_svq1_block (bitbuf, current, pitch, 0);
    break;

  case SVQ1_BLOCK_INTER_4V:
    result = motion_inter_4v_block (bitbuf, current, previous, pitch, motion, x, y);

    if (result != 0)
      break;

    result = decode_svq1_block (bitbuf, current, pitch, 0);
    break;

  case SVQ1_BLOCK_INTRA:
    result = decode_svq1_block (bitbuf, current, pitch, 1);
    break;
  }

  return result;
}

/* standard video sizes */
static struct { int width; int height; } frame_size_table[8] = {
  { 160, 120 }, { 128,  96 }, { 176, 144 }, { 352, 288 },
  { 704, 576 }, { 240, 180 }, { 320, 240 }, {  -1,  -1 }
};

static int decode_frame_header (bit_buffer_t *bitbuf, svq1_t *svq1) {
  int frame_size_code;

  /* unknown field */
  get_bits (bitbuf, 8);

  /* frame type */
  svq1->frame_type = get_bits (bitbuf, 2);

  if (svq1->frame_type == 3)
    return -1;

  if (svq1->frame_type == SVQ1_FRAME_INTRA) {

    /* unknown fields */
    if (svq1->frame_code == 0x50 || svq1->frame_code == 0x60) {
      get_bits (bitbuf, 16);
    }

    if ((svq1->frame_code ^ 0x10) >= 0x50) {
      skip_bits(bitbuf,8*get_bits (bitbuf, 8));
    }

    get_bits (bitbuf, 2);
    get_bits (bitbuf, 2);
    get_bits (bitbuf, 1);

    /* load frame size */
    frame_size_code = get_bits (bitbuf, 3);

    if (frame_size_code == 7) {
      /* load width, height (12 bits each) */
      svq1->frame_width = get_bits (bitbuf, 12);
      svq1->frame_height = get_bits (bitbuf, 12);

      if (!svq1->frame_width || !svq1->frame_height)
        return -1;
    } else {
      /* get width, height from table */
      svq1->frame_width = frame_size_table[frame_size_code].width;
      svq1->frame_height = frame_size_table[frame_size_code].height;
    }
  }

  /* unknown fields */
  if (get_bits (bitbuf, 1) == 1) {
    get_bits (bitbuf, 1);
    get_bits (bitbuf, 1);

    if (get_bits (bitbuf, 2) != 0)
      return -1;
  }

  if (get_bits (bitbuf, 1) == 1) {
    get_bits (bitbuf, 1);
    get_bits (bitbuf, 4);
    get_bits (bitbuf, 1);
    get_bits (bitbuf, 2);

    while (get_bits (bitbuf, 1) == 1) {
      get_bits (bitbuf, 8);
    }
  }
  
  return 0;
}

int svq1_decode_frame (svq1_t *svq1, uint8_t *buffer,int buffer_len) {
  bit_buffer_t	bitbuf;
  uint8_t      *current, *previous;
  int		result, i, x, y, width, height;
  int		luma_size, chroma_size;

//  memcpy(temp_buf,buffer,buffer_len); buffer=temp_buf;
  
  /* initialize bit buffer */
  init_get_bits(&bitbuf,buffer,buffer_len);

  /* decode frame header */
  svq1->frame_code = get_bits (&bitbuf, 22);

  if ((svq1->frame_code & ~0x70) || !(svq1->frame_code & 0x60))
    return -1;

  /* swap some header bytes (why?) */
  if (svq1->frame_code != 0x20) {
    uint32_t *src = (uint32_t *) (buffer + 4);

    for (i=0; i < 4; i++) {
      src[i] = ((src[i] << 16) | (src[i] >> 16)) ^ src[7 - i];
    }
  }

  result = decode_frame_header (&bitbuf, svq1);

  if (result != 0)
    return result;

  /* check frame size (changed?) */
  if (((svq1->frame_width + 3) & ~0x3) != svq1->width ||
      ((svq1->frame_height + 3) & ~0x3) != svq1->height) {

    /* free current buffers */
    free (svq1->current);
    free (svq1->previous);
    free (svq1->motion);

    svq1->width		= (svq1->frame_width + 3) & ~0x3;
    svq1->height	= (svq1->frame_height + 3) & ~0x3;
    svq1->luma_width	= (svq1->width + 15) & ~0xF;
    svq1->luma_height	= (svq1->height + 15) & ~0xF;
    svq1->chroma_width	= ((svq1->width / 4) + 15) & ~0xF;
    svq1->chroma_height	= ((svq1->height / 4) + 15) & ~0xF;

    /* allocate new pixel and motion buffers for updated frame size */
    luma_size		= svq1->luma_width * svq1->luma_height;
    chroma_size		= svq1->chroma_width * svq1->chroma_height;

    svq1->motion	= (svq1_pmv_t *) malloc (((svq1->luma_width / 8) + 3) * sizeof(svq1_pmv_t));
    svq1->current	= (uint8_t *) malloc (luma_size + 2*chroma_size);
    svq1->previous	= (uint8_t *) malloc (luma_size + 2*chroma_size);
    svq1->offsets[0]	= 0;
    svq1->offsets[1]	= luma_size;
    svq1->offsets[2]	= luma_size + chroma_size;

    for (i=0; i < 3; i++) {
      svq1->base[i] = svq1->current + svq1->offsets[i];
    }

    svq1->reference_frame = 0;
  }

  /* delta frame requires reference frame */
  if (svq1->frame_type != SVQ1_FRAME_INTRA && !svq1->reference_frame)
    return -1;

  /* decode y, u and v components */
  for (i=0; i < 3; i++) {
    if (i == 0) {
      width  = svq1->luma_width;
      height = svq1->luma_height;
    } else {
      width  = svq1->chroma_width;
      height = svq1->chroma_height;
    }

    current  = svq1->current + svq1->offsets[i];
    previous = svq1->previous + svq1->offsets[i];

    if (svq1->frame_type == SVQ1_FRAME_INTRA) {
      /* keyframe */
      for (y=0; y < height; y+=16) {
	for (x=0; x < width; x+=16) {
	  result = decode_svq1_block (&bitbuf, &current[x], width, 1);

	  if (result != 0)
	    return result;
	}

	current += 16*width;
      }
    } else {
      /* delta frame */
      memset (svq1->motion, 0, ((width / 8) + 3) * sizeof(svq1_pmv_t));

      for (y=0; y < height; y+=16) {
	for (x=0; x < width; x+=16) {
	  result = decode_delta_block (&bitbuf, &current[x], previous,
				       width, svq1->motion, x, y);

	  if (result != 0)
	    return result;
	}

	svq1->motion[0].x = 0;
	svq1->motion[0].y = 0;

	current += 16*width;
      }
    }
  }

  /* update pixel buffers for frame copy */
  for (i=0; i < 3; i++) {
    svq1->base[i] = svq1->current + svq1->offsets[i];
  }

  /* update backward reference frame */
  if (svq1->frame_type != SVQ1_FRAME_DROPPABLE) {
    uint8_t *tmp = svq1->previous;
    svq1->previous = svq1->current;
    svq1->current = tmp;
    svq1->reference_frame = 1;    
  }

  return 0;
}

void svq1_free (svq1_t *svq1){
  if (svq1) {
    free (svq1->current);
    free (svq1->previous);
    free (svq1->motion);
    free (svq1);
  }
}

