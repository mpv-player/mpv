/* Valid values for ANTIALIASING_ALGORITHM:
   0: none (fastest, most ugly)
   1: aproximate
   2: full (slowest, best looking)
 */
#define ANTIALIASING_ALGORITHM 1

/* SPUdec.c
   Skeleton of function spudec_process_controll() is from xine sources.
   Further works:
   LGB,... (yeah, try to improve it and insert your name here! ;-)

   Kim Minh Kaplan
   implement fragments reassembly, RLE decoding.
   read brightness from the IFO.

   For information on SPU format see <URL:http://sam.zoy.org/doc/dvd/subtitles/>
   and <URL:http://members.aol.com/mpucoder/DVD/spu.html>

 */
#include "config.h"
#include "mp_msg.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#if ANTIALIASING_ALGORITHM == 2
#include <math.h>
#endif
#include "spudec.h"

#define MIN(a, b)	((a)<(b)?(a):(b))

typedef struct {
  unsigned int global_palette[16];
  unsigned int orig_frame_width, orig_frame_height;
  unsigned char* packet;
  size_t packet_reserve;	/* size of the memory pointed to by packet */
  unsigned int packet_offset;	/* end of the currently assembled fragment */
  unsigned int packet_size;	/* size of the packet once all fragments are assembled */
  unsigned int control_start;	/* index of start of control data */
  unsigned int palette[4];
  unsigned int alpha[4];
  unsigned int now_pts;
  unsigned int start_pts, end_pts;
  unsigned int start_col, end_col;
  unsigned int start_row, end_row;
  unsigned int width, height, stride;
  unsigned int current_nibble[2]; /* next data nibble (4 bits) to be
                                     processed (for RLE decoding) for
                                     even and odd lines */
  int deinterlace_oddness;	/* 0 or 1, index into current_nibble */
  size_t image_size;		/* Size of the image buffer */
  unsigned char *image;		/* Grayscale value */
  unsigned char *aimage;	/* Alpha value */
  unsigned int scaled_frame_width, scaled_frame_height;
  unsigned int scaled_start_col, scaled_start_row;
  unsigned int scaled_width, scaled_height, scaled_stride;
  size_t scaled_image_size;
  unsigned char *scaled_image;
  unsigned char *scaled_aimage;
} spudec_handle_t;

static inline unsigned int get_be16(const unsigned char *p)
{
  return (p[0] << 8) + p[1];
}

static inline unsigned int get_be24(const unsigned char *p)
{
  return (get_be16(p) << 8) + p[2];
}

static void next_line(spudec_handle_t *this)
{
  if (this->current_nibble[this->deinterlace_oddness] % 2)
    this->current_nibble[this->deinterlace_oddness]++;
  this->deinterlace_oddness = (this->deinterlace_oddness + 1) % 2;
}

static inline unsigned char get_nibble(spudec_handle_t *this)
{
  unsigned char nib;
  unsigned int *nibblep = this->current_nibble + this->deinterlace_oddness;
  if (*nibblep / 2 >= this->control_start) {
    mp_msg(MSGT_SPUDEC,MSGL_WARN, "SPUdec: ERROR: get_nibble past end of packet\n");
    return 0;
  }
  nib = this->packet[*nibblep / 2];
  if (*nibblep % 2)
    nib &= 0xf;
  else
    nib >>= 4;
  ++*nibblep;
  return nib;
}

static inline int mkalpha(int i)
{
  /* In mplayer's alpha planes, 0 is transparent, then 1 is nearly
     opaque upto 255 which is transparent */
  switch (i) {
  case 0xf:
    return 1;
  case 0:
    return 0;
  default:
    return (0xf - i) << 4;
  }
}

static void spudec_process_data(spudec_handle_t *this)
{
  unsigned int cmap[4], alpha[4];
  unsigned int i, x, y;

  this->scaled_frame_width = 0;
  this->scaled_frame_height = 0;
  for (i = 0; i < 4; ++i) {
    alpha[i] = mkalpha(this->alpha[i]);
    if (alpha[i] == 0)
      cmap[i] = 0;
    else {
      cmap[i] = ((this->global_palette[this->palette[i]] >> 16) & 0xff);
      if (cmap[i] + alpha[i] > 255)
	cmap[i] = 256 - alpha[i];
    }
  }

  if (this->image_size < this->stride * this->height) {
    if (this->image != NULL) {
      free(this->image);
      this->image_size = 0;
    }
    this->image = malloc(2 * this->stride * this->height);
    if (this->image) {
      this->image_size = this->stride * this->height;
      this->aimage = this->image + this->image_size;
    }
  }
  if (this->image == NULL)
    return;

  /* Kludge: draw_alpha needs width multiple of 8. */
  if (this->width < this->stride)
    for (y = 0; y < this->height; ++y)
      memset(this->aimage + y * this->stride + this->width, 0, this->stride - this->width);

  i = this->current_nibble[1];
  x = 0;
  y = 0;
  while (this->current_nibble[0] < i
	 && this->current_nibble[1] / 2 < this->control_start
	 && y < this->height) {
    unsigned int len, color;
    unsigned int rle = 0;
    rle = get_nibble(this);
    if (rle < 0x04) {
      rle = (rle << 4) | get_nibble(this);
      if (rle < 0x10) {
	rle = (rle << 4) | get_nibble(this);
	if (rle < 0x040) {
	  rle = (rle << 4) | get_nibble(this);
	  if (rle < 0x0004)
	    rle |= ((this->width - x) << 2);
	}
      }
    }
    color = 3 - (rle & 0x3);
    len = rle >> 2;
    if (len > this->width - x || len == 0)
      len = this->width - x;
    /* FIXME have to use palette and alpha map*/
    memset(this->image + y * this->stride + x, cmap[color], len);
    memset(this->aimage + y * this->stride + x, alpha[color], len);
    x += len;
    if (x >= this->width) {
      next_line(this);
      x = 0;
      ++y;
    }
  }
}

static void spudec_process_control(spudec_handle_t *this, unsigned int pts100)
{
  int a,b; /* Temporary vars */
  unsigned int date, type;
  unsigned int off;
  unsigned int start_off = 0;
  unsigned int next_off;

  this->control_start = get_be16(this->packet + 2);
  next_off = this->control_start;
  while (start_off != next_off) {
    start_off = next_off;
    date = get_be16(this->packet + start_off);
    next_off = get_be16(this->packet + start_off + 2);
    mp_msg(MSGT_SPUDEC,MSGL_DBG2, "date=%d\n", date);
    off = start_off + 4;
    for (type = this->packet[off++]; type != 0xff; type = this->packet[off++]) {
      mp_msg(MSGT_SPUDEC,MSGL_DBG2, "cmd=%d  ",type);
      switch(type) {
      case 0x00:
	/* Menu ID, 1 byte */
	mp_msg(MSGT_SPUDEC,MSGL_DBG2,"Menu ID\n");
	break;
      case 0x01:
	/* Start display */
	mp_msg(MSGT_SPUDEC,MSGL_DBG2,"Start display!\n");
	this->start_pts = pts100 + date;
	this->end_pts = UINT_MAX;
	break;
      case 0x02:
	/* Stop display */
	mp_msg(MSGT_SPUDEC,MSGL_DBG2,"Stop display!\n");
	this->end_pts = pts100 + date;
	break;
      case 0x03:
	/* Palette */
	this->palette[0] = this->packet[off] >> 4;
	this->palette[1] = this->packet[off] & 0xf;
	this->palette[2] = this->packet[off + 1] >> 4;
	this->palette[3] = this->packet[off + 1] & 0xf;
	mp_msg(MSGT_SPUDEC,MSGL_DBG2,"Palette %d, %d, %d, %d\n",
	       this->palette[0], this->palette[1], this->palette[2], this->palette[3]);
	off+=2;
	break;
      case 0x04:
	/* Alpha */
	this->alpha[0] = this->packet[off] >> 4;
	this->alpha[1] = this->packet[off] & 0xf;
	this->alpha[2] = this->packet[off + 1] >> 4;
	this->alpha[3] = this->packet[off + 1] & 0xf;
	mp_msg(MSGT_SPUDEC,MSGL_DBG2,"Alpha %d, %d, %d, %d\n",
	       this->alpha[0], this->alpha[1], this->alpha[2], this->alpha[3]);
	off+=2;
	break;
      case 0x05:
	/* Co-ords */
	a = get_be24(this->packet + off);
	b = get_be24(this->packet + off + 3);
	this->start_col = a >> 12;
	this->end_col = a & 0xfff;
	this->width = (this->end_col < this->start_col) ? 0 : this->end_col - this->start_col + 1;
	this->stride = (this->width + 7) & ~7; /* Kludge: draw_alpha needs width multiple of 8 */
	this->start_row = b >> 12;
	this->end_row = b & 0xfff;
	this->height = (this->end_row < this->start_row) ? 0 : this->end_row - this->start_row /* + 1 */;
	mp_msg(MSGT_SPUDEC,MSGL_DBG2,"Coords  col: %d - %d  row: %d - %d  (%dx%d)\n",
	       this->start_col, this->end_col, this->start_row, this->end_row,
	       this->width, this->height);
	off+=6;
	break;
      case 0x06:
	/* Graphic lines */
	this->current_nibble[0] = 2 * get_be16(this->packet + off);
	this->current_nibble[1] = 2 * get_be16(this->packet + off + 2);
	mp_msg(MSGT_SPUDEC,MSGL_DBG2,"Graphic offset 1: %d  offset 2: %d\n",
	       this->current_nibble[0] / 2, this->current_nibble[1] / 2);
	off+=4;
	break;
      case 0xff:
	/* All done, bye-bye */
	mp_msg(MSGT_SPUDEC,MSGL_DBG2,"Done!\n");
	return;
//	break;
      default:
	mp_msg(MSGT_SPUDEC,MSGL_WARN,"spudec: Error determining control type 0x%02x.  Skipping %d bytes.\n",
	       type, next_off - off);
	goto next_control;
      }
    }
  next_control:
    ;
  }
}

static void spudec_decode(spudec_handle_t *this, unsigned int pts100)
{
  spudec_process_control(this, pts100);
  spudec_process_data(this);
}


void spudec_assemble(void *this, unsigned char *packet, unsigned int len, unsigned int pts100)
{
  spudec_handle_t *spu = (spudec_handle_t*)this;
//  spudec_heartbeat(this, pts100);
  if (len < 2) {
      mp_msg(MSGT_SPUDEC,MSGL_WARN,"SPUasm: packet too short\n");
      return;
  }
  if (spu->packet_offset == 0) {
    unsigned int len2 = get_be16(packet);
    // Start new fragment
    if (spu->packet_reserve < len2) {
      if (spu->packet != NULL)
	free(spu->packet);
      spu->packet = malloc(len2);
      spu->packet_reserve = spu->packet != NULL ? len2 : 0;
    }
    if (spu->packet != NULL) {
      spu->deinterlace_oddness = 0;
      spu->packet_size = len2;
      if (len > len2) {
	mp_msg(MSGT_SPUDEC,MSGL_WARN,"SPUasm: invalid frag len / len2: %d / %d \n", len, len2);
	return;
      }
      memcpy(spu->packet, packet, len);
      spu->packet_offset = len;
    }
  } else {
    // Continue current fragment
    if (spu->packet_size < spu->packet_offset + len){
      mp_msg(MSGT_SPUDEC,MSGL_WARN,"SPUasm: invalid fragment\n");
      spu->packet_size = spu->packet_offset = 0;
    } else {
      memcpy(spu->packet + spu->packet_offset, packet, len);
      spu->packet_offset += len;
    }
  }
#if 1
  // check if we have a complete packet (unfortunatelly packet_size is bad
  // for some disks)
//  if (spu->packet_offset == spu->packet_size)
  { unsigned int x=0,y;
    while(x+4<=spu->packet_offset){
      y=get_be16(spu->packet+x+2); // next control pointer
      mp_msg(MSGT_SPUDEC,MSGL_DBG2,"SPUtest: x=%d y=%d off=%d size=%d\n",x,y,spu->packet_offset,spu->packet_size);
      if(x>=4 && x==y){		// if it points to self - we're done!
        // we got it!
	mp_msg(MSGT_SPUDEC,MSGL_DBG2,"SPUgot: off=%d  size=%d \n",spu->packet_offset,spu->packet_size);
	spudec_decode(spu, pts100);
	spu->packet_offset = 0;
	break;
      }
      if(y<=x || y>=spu->packet_size){ // invalid?
	mp_msg(MSGT_SPUDEC,MSGL_WARN,"SPUtest: broken packet!!!!! y=%d < x=%d\n",y,x);
        spu->packet_size = spu->packet_offset = 0;
        break;
      }
      x=y;
    }
  }
#else
  if (spu->packet_offset == spu->packet_size) {
    spudec_decode(spu, pts100);
    spu->packet_offset = 0;
  }
#endif
}

void spudec_reset(void *this)	// called after seek
{
  spudec_handle_t *spu = (spudec_handle_t*)this;
  spu->now_pts = 0;
  spu->packet_size = spu->packet_offset = 0;
}

void spudec_heartbeat(void *this, unsigned int pts100)
{
  ((spudec_handle_t *)this)->now_pts = pts100;
}

void spudec_draw(void *this, void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride))
{
    spudec_handle_t *spu = (spudec_handle_t *)this;
    if (spu->start_pts <= spu->now_pts && spu->now_pts < spu->end_pts && spu->image)
	draw_alpha(spu->start_col, spu->start_row, spu->width, spu->height,
		   spu->image, spu->aimage, spu->stride);
}

/* transform mplayer's alpha value into an opacity value that is linear */
static inline int canon_alpha(int alpha)
{
  return alpha ? 256 - alpha : 0;
}

void spudec_draw_scaled(void *me, unsigned int dxs, unsigned int dys, void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride))
{
  spudec_handle_t *spu = (spudec_handle_t *)me;
  if (spu->start_pts <= spu->now_pts && spu->now_pts < spu->end_pts) {
    if (spu->orig_frame_width == 0 || spu->orig_frame_height == 0
	|| (spu->orig_frame_width == dxs && spu->orig_frame_height == dys)) {
      if (spu->image)
	draw_alpha(spu->start_col, spu->start_row, spu->width, spu->height,
		   spu->image, spu->aimage, spu->stride);
    }
    else {
      if (spu->scaled_frame_width != dxs || spu->scaled_frame_height != dys) {	/* Resizing is needed */
	/* scaled_x = scalex * x / 0x100
	   scaled_y = scaley * y / 0x100
	   order of operations is important because of rounding. */
	unsigned int scalex = 0x100 * dxs / spu->orig_frame_width;
	unsigned int scaley = 0x100 * dys / spu->orig_frame_height;
	spu->scaled_start_col = spu->start_col * scalex / 0x100;
	spu->scaled_start_row = spu->start_row * scaley / 0x100;
	spu->scaled_width = spu->width * scalex / 0x100;
	spu->scaled_height = spu->height * scaley / 0x100;
	/* Kludge: draw_alpha needs width multiple of 8 */
	spu->scaled_stride = (spu->scaled_width + 7) & ~7;
	if (spu->scaled_image_size < spu->scaled_stride * spu->scaled_height) {
	  if (spu->scaled_image) {
	    free(spu->scaled_image);
	    spu->scaled_image_size = 0;
	  }
	  spu->scaled_image = malloc(2 * spu->scaled_stride * spu->scaled_height);
	  if (spu->scaled_image) {
	    spu->scaled_image_size = spu->scaled_stride * spu->scaled_height;
	    spu->scaled_aimage = spu->scaled_image + spu->scaled_image_size;
	  }
	}
	if (spu->scaled_image) {
	  unsigned int x, y;
	  /* Kludge: draw_alpha needs width multiple of 8. */
	  if (spu->scaled_width < spu->scaled_stride)
	    for (y = 0; y < spu->scaled_height; ++y) {
	      memset(spu->scaled_aimage + y * spu->scaled_stride + spu->scaled_width, 0,
		     spu->scaled_stride - spu->scaled_width);
	      /* FIXME: Why is this one needed? */
	      memset(spu->scaled_image + y * spu->scaled_stride + spu->scaled_width, 0,
		     spu->scaled_stride - spu->scaled_width);
	    }
#if ANTIALIASING_ALGORITHM == 0
	  /* no antialiasing */
	  for (y = 0; y < spu->scaled_height; ++y) {
	    int unscaled_y = y * 0x100 / scaley;
	    int strides = spu->stride * unscaled_y;
	    int scaled_strides = spu->scaled_stride * y;
	    for (x = 0; x < spu->scaled_width; ++x) {
	      int unscaled_x = x * 0x100 / scalex;
	      spu->scaled_image[scaled_strides + x] = spu->image[strides + unscaled_x];
	      spu->scaled_aimage[scaled_strides + x] = spu->aimage[strides + unscaled_x];
	    }
	  }
#elif ANTIALIASING_ALGORITHM == 1
	  {
	    /* Intermediate antialiasing. */
	    for (y = 0; y < spu->scaled_height; ++y) {
	      const unsigned int unscaled_top = y * spu->orig_frame_height / dys;
	      unsigned int unscaled_bottom = (y + 1) * spu->orig_frame_height / dys;
	      if (unscaled_bottom >= spu->height)
		unscaled_bottom = spu->height - 1;
	      for (x = 0; x < spu->scaled_width; ++x) {
		const unsigned int unscaled_left = x * spu->orig_frame_width / dxs;
		unsigned int unscaled_right = (x + 1) * spu->orig_frame_width / dxs;
		unsigned int color = 0;
		unsigned int alpha = 0;
		unsigned int walkx, walky;
		unsigned int base, tmp;
		if (unscaled_right >= spu->width)
		  unscaled_right = spu->width - 1;
		for (walky = unscaled_top; walky <= unscaled_bottom; ++walky)
		  for (walkx = unscaled_left; walkx <= unscaled_right; ++walkx) {
		    base = walky * spu->stride + walkx;
		    tmp = canon_alpha(spu->aimage[base]);
		    alpha += tmp;
		    color += tmp * spu->image[base];
		  }
		base = y * spu->scaled_stride + x;
		spu->scaled_image[base] = alpha ? color / alpha : 0;
		spu->scaled_aimage[base] =
		  alpha * (1 + unscaled_bottom - unscaled_top) * (1 + unscaled_right - unscaled_left);
		/* spu->scaled_aimage[base] =
		  alpha * dxs * dys / spu->orig_frame_width / spu->orig_frame_height; */
		if (spu->scaled_aimage[base]) {
		  spu->scaled_aimage[base] = 256 - spu->scaled_aimage[base];
		  if (spu->scaled_aimage[base] + spu->scaled_image[base] > 255)
		    spu->scaled_image[base] = 256 - spu->scaled_aimage[base];
		}
	      }
	    }
	  }
#else
	  {
	    /* Best antialiasing.  Very slow. */
	    /* Any pixel (x, y) represents pixels from the original
	       rectangular region comprised between the columns
	       unscaled_y and unscaled_y + 0x100 / scaley and the rows
	       unscaled_x and unscaled_x + 0x100 / scalex

	       The original rectangular region that the scaled pixel
	       represents is cut in 9 rectangular areas like this:
	       
	       +---+-----------------+---+
	       | 1 |        2        | 3 |
	       +---+-----------------+---+
	       |   |                 |   |
	       | 4 |        5        | 6 |
	       |   |                 |   |
	       +---+-----------------+---+
	       | 7 |        8        | 9 |
	       +---+-----------------+---+

	       The width of the left column is at most one pixel and
	       it is never null and its right column is at a pixel
	       boundary.  The height of the top row is at most one
	       pixel it is never null and its bottom row is at a
	       pixel boundary. The width and height of region 5 are
	       integral values.  The width of the right column is
	       what remains and is less than one pixel.  The height
	       of the bottom row is what remains and is less than
	       one pixel.

	       The row above 1, 2, 3 is unscaled_y.  The row between
	       1, 2, 3 and 4, 5, 6 is top_low_row.  The row between 4,
	       5, 6 and 7, 8, 9 is (unsigned int)unscaled_y_bottom.
	       The row beneath 7, 8, 9 is unscaled_y_bottom.

	       The column left of 1, 4, 7 is unscaled_x.  The column
	       between 1, 4, 7 and 2, 5, 8 is left_right_column.  The
	       column between 2, 5, 8 and 3, 6, 9 is (unsigned
	       int)unscaled_x_right.  The column right of 3, 6, 9 is
	       unscaled_x_right. */
	    const double inv_scalex = (double) 0x100 / scalex;
	    const double inv_scaley = (double) 0x100 / scaley;
	    for (y = 0; y < spu->scaled_height; ++y) {
	      const double unscaled_y = y * inv_scaley;
	      const double unscaled_y_bottom = unscaled_y + inv_scaley;
	      const unsigned int top_low_row = MIN(unscaled_y_bottom, unscaled_y + 1.0);
	      const double top = top_low_row - unscaled_y;
	      const unsigned int height = unscaled_y_bottom > top_low_row
		? (unsigned int) unscaled_y_bottom - top_low_row
		: 0;
	      const double bottom = unscaled_y_bottom > top_low_row
		? unscaled_y_bottom - floor(unscaled_y_bottom)
		: 0.0;
	      for (x = 0; x < spu->scaled_width; ++x) {
		const double unscaled_x = x * inv_scalex;
		const double unscaled_x_right = unscaled_x + inv_scalex;
		const unsigned int left_right_column = MIN(unscaled_x_right, unscaled_x + 1.0);
		const double left = left_right_column - unscaled_x;
		const unsigned int width = unscaled_x_right > left_right_column
		  ? (unsigned int) unscaled_x_right - left_right_column
		  : 0;
		const double right = unscaled_x_right > left_right_column
		  ? unscaled_x_right - floor(unscaled_x_right)
		  : 0.0;
		double color = 0.0;
		double alpha = 0.0;
		double tmp;
		unsigned int base;
		/* Now use these informations to compute a good alpha,
                   and lightness.  The sum is on each of the 9
                   region's surface and alpha and lightness.

		  transformed alpha = sum(surface * alpha) / sum(surface)
		  transformed color = sum(surface * alpha * color) / sum(surface * alpha)
		*/
		/* 1: top left part */
		base = spu->stride * (unsigned int) unscaled_y;
		tmp = left * top * canon_alpha(spu->aimage[base + (unsigned int) unscaled_x]);
		alpha += tmp;
		color += tmp * spu->image[base + (unsigned int) unscaled_x];
		/* 2: top center part */
		if (width > 0) {
		  unsigned int walkx;
		  for (walkx = left_right_column; walkx < (unsigned int) unscaled_x_right; ++walkx) {
		    base = spu->stride * (unsigned int) unscaled_y + walkx;
		    tmp = /* 1.0 * */ top * canon_alpha(spu->aimage[base]);
		    alpha += tmp;
		    color += tmp * spu->image[base];
		  }
		}
		/* 3: top right part */
		if (right > 0.0) {
		  base = spu->stride * (unsigned int) unscaled_y + (unsigned int) unscaled_x_right;
		  tmp = right * top * canon_alpha(spu->aimage[base]);
		  alpha += tmp;
		  color += tmp * spu->image[base];
		}
		/* 4: center left part */
		if (height > 0) {
		  unsigned int walky;
		  for (walky = top_low_row; walky < (unsigned int) unscaled_y_bottom; ++walky) {
		    base = spu->stride * walky + (unsigned int) unscaled_x;
		    tmp = left /* * 1.0 */ * canon_alpha(spu->aimage[base]);
		    alpha += tmp;
		    color += tmp * spu->image[base];
		  }
		}
		/* 5: center part */
		if (width > 0 && height > 0) {
		  unsigned int walky;
		  for (walky = top_low_row; walky < (unsigned int) unscaled_y_bottom; ++walky) {
		    unsigned int walkx;
		    base = spu->stride * walky;
		    for (walkx = left_right_column; walkx < (unsigned int) unscaled_x_right; ++walkx) {
		      tmp = /* 1.0 * 1.0 * */ canon_alpha(spu->aimage[base + walkx]);
		      alpha += tmp;
		      color += tmp * spu->image[base + walkx];
		    }
		  }		    
		}
		/* 6: center right part */
		if (right > 0.0 && height > 0) {
		  unsigned int walky;
		  for (walky = top_low_row; walky < (unsigned int) unscaled_y_bottom; ++walky) {
		    base = spu->stride * walky + (unsigned int) unscaled_x_right;
		    tmp = right /* * 1.0 */ * canon_alpha(spu->aimage[base]);
		    alpha += tmp;
		    color += tmp * spu->image[base];
		  }
		}
		/* 7: bottom left part */
		if (bottom > 0.0) {
		  base = spu->stride * (unsigned int) unscaled_y_bottom + (unsigned int) unscaled_x;
		  tmp = left * bottom * canon_alpha(spu->aimage[base]);
		  alpha += tmp;
		  color += tmp * spu->image[base];
		}
		/* 8: bottom center part */
		if (width > 0 && bottom > 0.0) {
		  unsigned int walkx;
		  base = spu->stride * (unsigned int) unscaled_y_bottom;
		  for (walkx = left_right_column; walkx < (unsigned int) unscaled_x_right; ++walkx) {
		    tmp = /* 1.0 * */ bottom * canon_alpha(spu->aimage[base + walkx]);
		    alpha += tmp;
		    color += tmp * spu->image[base + walkx];
		  }
		}
		/* 9: bottom right part */
		if (right > 0.0 && bottom > 0.0) {
		  base = spu->stride * (unsigned int) unscaled_y_bottom + (unsigned int) unscaled_x_right;
		  tmp = right * bottom * canon_alpha(spu->aimage[base]);
		  alpha += tmp;
		  color += tmp * spu->image[base];
		}
		/* Finally mix these transparency and brightness information suitably */
		base = spu->scaled_stride * y + x;
		spu->scaled_image[base] = alpha > 0 ? color / alpha : 0;
		spu->scaled_aimage[base] = alpha * scalex * scaley / 0x10000;
		if (spu->scaled_aimage[base]) {
		  spu->scaled_aimage[base] = 256 - spu->scaled_aimage[base];
		  if (spu->scaled_aimage[base] + spu->scaled_image[base] > 255)
		    spu->scaled_image[base] = 256 - spu->scaled_aimage[base];
		}
	      }
	    }
	  }
#endif
	  spu->scaled_frame_width = dxs;
	  spu->scaled_frame_height = dys;
	}
      }
      if (spu->scaled_image)
	draw_alpha(spu->scaled_start_col, spu->scaled_start_row, spu->scaled_width, spu->scaled_height,
		   spu->scaled_image, spu->scaled_aimage, spu->scaled_stride);
    }
  }
}

void *spudec_new_scaled(unsigned int *palette, unsigned int frame_width, unsigned int frame_height)
{
  spudec_handle_t *this = calloc(1, sizeof(spudec_handle_t));
  if (this) {
    if (palette)
      memcpy(this->global_palette, palette, sizeof(this->global_palette));
    this->packet = NULL;
    this->image = NULL;
    this->scaled_image = NULL;
    this->orig_frame_width = frame_width;
    this->orig_frame_height = frame_height;
  }
  else
    perror("FATAL: spudec_init: calloc");
  return this;
}

void *spudec_new(unsigned int *palette)
{
    return spudec_new_scaled(palette, 0, 0);
}

void spudec_free(void *this)
{
  spudec_handle_t *spu = (spudec_handle_t*)this;
  if (spu) {
    if (spu->packet)
      free(spu->packet);
    if (spu->scaled_image)
	free(spu->scaled_image);
    if (spu->image)
      free(spu->image);
    free(spu);
  }
}
