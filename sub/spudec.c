/*
 * Skeleton of function spudec_process_controll() is from xine sources.
 * Further works:
 * LGB,... (yeah, try to improve it and insert your name here! ;-)
 *
 * Kim Minh Kaplan
 * implement fragments reassembly, RLE decoding.
 * read brightness from the IFO.
 *
 * For information on SPU format see <URL:http://sam.zoy.org/doc/dvd/subtitles/>
 * and <URL:http://members.aol.com/mpucoder/DVD/spu.html>
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

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <libavutil/common.h>
#include <libavutil/intreadwrite.h>

#include "config.h"
#include "core/mp_msg.h"

#include "spudec.h"
#include "sub.h"
#include "core/mp_common.h"
#include "video/csputils.h"

typedef struct spu_packet_t packet_t;
struct spu_packet_t {
  int is_decoded;
  unsigned char *packet;
  int data_len;
  unsigned int palette[4];
  unsigned int alpha[4];
  unsigned int control_start;	/* index of start of control data */
  unsigned int current_nibble[2]; /* next data nibble (4 bits) to be
                                     processed (for RLE decoding) for
                                     even and odd lines */
  int deinterlace_oddness;	/* 0 or 1, index into current_nibble */
  unsigned int start_col;
  unsigned int start_row;
  unsigned int width, height, stride;
  unsigned int start_pts, end_pts;
  packet_t *next;
};

typedef struct {
  packet_t *queue_head;
  packet_t *queue_tail;
  unsigned int global_palette[16];
  unsigned int orig_frame_width, orig_frame_height;
  unsigned char* packet;
  size_t packet_reserve;	/* size of the memory pointed to by packet */
  unsigned int packet_offset;	/* end of the currently assembled fragment */
  unsigned int packet_size;	/* size of the packet once all fragments are assembled */
  int packet_pts;		/* PTS for this packet */
  unsigned int palette[4];
  unsigned int alpha[4];
  unsigned int cuspal[4];
  unsigned int custom;
  unsigned int now_pts;
  unsigned int start_pts, end_pts;
  unsigned int start_col;
  unsigned int start_row;
  unsigned int width, height, stride;
  size_t image_size;		/* Size of the image buffer */
  unsigned char *image;		/* Grayscale value */
  unsigned int pal_start_col, pal_start_row;
  unsigned int pal_width, pal_height;
  unsigned char *pal_image;	/* palette entry value */
  int auto_palette; /* 1 if we lack a palette and must use an heuristic. */
  int font_start_level;  /* Darkest value used for the computed font */
  int spu_changed;
  unsigned int forced_subs_only;     /* flag: 0=display all subtitle, !0 display only forced subtitles */
  unsigned int is_forced_sub;         /* true if current subtitle is a forced subtitle */

  struct sub_bitmap sub_part, borrowed_sub_part;
  struct osd_bmp_indexed borrowed_bmp;
} spudec_handle_t;

static void spudec_queue_packet(spudec_handle_t *this, packet_t *packet)
{
  if (this->queue_head == NULL)
    this->queue_head = packet;
  else
    this->queue_tail->next = packet;
  this->queue_tail = packet;
}

static packet_t *spudec_dequeue_packet(spudec_handle_t *this)
{
  packet_t *retval = this->queue_head;

  this->queue_head = retval->next;
  if (this->queue_head == NULL)
    this->queue_tail = NULL;

  return retval;
}

static void spudec_free_packet(packet_t *packet)
{
  free(packet->packet);
  free(packet);
}

static inline unsigned int get_be16(const unsigned char *p)
{
  return (p[0] << 8) + p[1];
}

static inline unsigned int get_be24(const unsigned char *p)
{
  return (get_be16(p) << 8) + p[2];
}

static void next_line(packet_t *packet)
{
  if (packet->current_nibble[packet->deinterlace_oddness] % 2)
    packet->current_nibble[packet->deinterlace_oddness]++;
  packet->deinterlace_oddness = (packet->deinterlace_oddness + 1) % 2;
}

static inline unsigned char get_nibble(packet_t *packet)
{
  unsigned char nib;
  unsigned int *nibblep = packet->current_nibble + packet->deinterlace_oddness;
  if (*nibblep / 2 >= packet->control_start) {
    mp_msg(MSGT_SPUDEC,MSGL_WARN, "SPUdec: ERROR: get_nibble past end of packet\n");
    return 0;
  }
  nib = packet->packet[*nibblep / 2];
  if (*nibblep % 2)
    nib &= 0xf;
  else
    nib >>= 4;
  ++*nibblep;
  return nib;
}

static int spudec_alloc_image(spudec_handle_t *this, int stride, int height)
{
  if (this->width > stride) // just a safeguard
    this->width = stride;
  this->stride = stride;
  this->height = height;
  if (this->image_size < this->stride * this->height) {
    if (this->image != NULL) {
      free(this->image);
      this->image = NULL;
      free(this->pal_image);
      this->pal_image = NULL;
      this->image_size = 0;
      this->pal_width = this->pal_height  = 0;
    }
    this->image = malloc(2 * this->stride * this->height);
    if (this->image) {
      this->image_size = this->stride * this->height;
      // use stride here as well to simplify reallocation checks
      this->pal_image = malloc(this->stride * this->height);
    }
  }
  return this->image != NULL;
}

static void setup_palette(spudec_handle_t *spu, uint32_t palette[256])
{
    memset(palette, 0, sizeof(*palette) * 256);
    struct mp_csp_params csp = MP_CSP_PARAMS_DEFAULTS;
    csp.int_bits_in = 8;
    csp.int_bits_out = 8;
    float cmatrix[3][4];
    mp_get_yuv2rgb_coeffs(&csp, cmatrix);
    for (int i = 0; i < 4; ++i) {
        int alpha = spu->alpha[i];
        // extend 4 -> 8 bit
        alpha |= alpha << 4;
        if (spu->custom && (spu->cuspal[i] >> 31) != 0)
            alpha = 0;
        int color = spu->custom ? spu->cuspal[i] :
                    spu->global_palette[spu->palette[i]];
        int c[3] = {(color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff};
        mp_map_int_color(cmatrix, 8, c);
        // R and G swapped, possibly due to vobsub_palette_to_yuv()
        palette[i] = (alpha << 24u) | (c[2] << 16) | (c[1] << 8) | c[0];
    }
}

static void crop_image(struct sub_bitmap *part)
{
    if (part->w < 1 || part->h < 1)
        return;
    struct osd_bmp_indexed *bmp = part->bitmap;
    bool invisible[256];
    for (int n = 0; n < 256; n++)
        invisible[n] = !(bmp->palette[n] >> 24);
    int y0 = 0, y1 = part->h, x0 = part->w, x1 = 0;
    bool y_all_invisible = true;
    for (int y = 0; y < part->h; y++) {
        uint8_t *pixels = bmp->bitmap + part->stride * y;
        int cur = 0;
        while (cur < part->w && invisible[pixels[cur]])
            cur++;
        int start_visible = cur;
        int last_visible = -1;
        while (cur < part->w) {
            if (!invisible[pixels[cur]])
                last_visible = cur;
            cur++;
        }
        x0 = FFMIN(x0, start_visible);
        x1 = FFMAX(x1, last_visible);
        bool all_invisible = last_visible == -1;
        if (all_invisible) {
            if (y_all_invisible)
                y0 = y;
        } else {
            y_all_invisible = false;
            y1 = y + 1;
        }
    }
    bmp->bitmap += x0 + y0 * part->stride;
    part->w = FFMAX(x1 - x0, 0);
    part->h = FFMAX(y1 - y0, 0);
    part->x += x0;
    part->y += y0;
}

static void spudec_process_data(spudec_handle_t *this, packet_t *packet)
{
  unsigned int i, x, y;
  uint8_t *dst;

  if (!spudec_alloc_image(this, packet->stride, packet->height))
    return;

  this->pal_start_col = packet->start_col;
  this->pal_start_row = packet->start_row;
  this->pal_height = packet->height;
  this->pal_width  = packet->width;
  this->stride = packet->stride;
  memcpy(this->palette, packet->palette, sizeof(this->palette));
  memcpy(this->alpha,   packet->alpha,   sizeof(this->alpha));

  i = packet->current_nibble[1];
  x = 0;
  y = 0;
  dst = this->pal_image;
  while (packet->current_nibble[0] < i
	 && packet->current_nibble[1] / 2 < packet->control_start
	 && y < this->pal_height) {
    unsigned int len, color;
    unsigned int rle = 0;
    rle = get_nibble(packet);
    if (rle < 0x04) {
      if (rle == 0) {
	rle = (rle << 4) | get_nibble(packet);
	if (rle < 0x04)
	  rle = (rle << 4) | get_nibble(packet);
      }
      rle = (rle << 4) | get_nibble(packet);
    }
    color = 3 - (rle & 0x3);
    len = rle >> 2;
    x += len;
    if (len == 0 || x >= this->pal_width) {
      len += this->pal_width - x;
      next_line(packet);
      x = 0;
      ++y;
    }
    memset(dst, color, len);
    dst += len;
  }

  struct sub_bitmap *sub_part = &this->sub_part;
  struct osd_bmp_indexed *bmp = &this->borrowed_bmp;
  bmp->bitmap = this->pal_image;
  setup_palette(this, bmp->palette);
  sub_part->bitmap = bmp;
  sub_part->stride = this->pal_width;
  sub_part->w = this->pal_width;
  sub_part->h = this->pal_height;
  sub_part->x = this->pal_start_col;
  sub_part->y = this->pal_start_row;
  crop_image(sub_part);
}


/*
  This function tries to create a usable palette.
  It determines how many non-transparent colors are used, and assigns different
gray scale values to each color.
  I tested it with four streams and even got something readable. Half of the
times I got black characters with white around and half the reverse.
*/
static void compute_palette(spudec_handle_t *this, packet_t *packet)
{
  int used[16],i,cused,start,step,color;

  memset(used, 0, sizeof(used));
  for (i=0; i<4; i++)
    if (packet->alpha[i]) /* !Transparent? */
       used[packet->palette[i]] = 1;
  for (cused=0, i=0; i<16; i++)
    if (used[i]) cused++;
  if (!cused) return;
  if (cused == 1) {
    start = 0x80;
    step = 0;
  } else {
    start = 72;
    step = (0xF0-start)/(cused-1);
  }
  memset(used, 0, sizeof(used));
  for (i=0; i<4; i++) {
    color = packet->palette[i];
    if (packet->alpha[i] && !used[color]) { /* not assigned? */
       used[color] = 1;
       this->global_palette[color] = start<<16;
       start += step;
    }
  }
}

static void spudec_process_control(spudec_handle_t *this, int pts100)
{
  int a,b,c,d; /* Temporary vars */
  unsigned int date, type;
  unsigned int off;
  unsigned int start_off = 0;
  unsigned int next_off;
  unsigned int start_pts = 0;
  unsigned int end_pts = 0;
  unsigned int current_nibble[2] = {0, 0};
  unsigned int control_start;
  unsigned int display = 0;
  unsigned int start_col = 0;
  unsigned int end_col = 0;
  unsigned int start_row = 0;
  unsigned int end_row = 0;
  unsigned int width = 0;
  unsigned int height = 0;
  unsigned int stride = 0;

  control_start = get_be16(this->packet + 2);
  next_off = control_start;
  while (start_off != next_off) {
    start_off = next_off;
    date = get_be16(this->packet + start_off) * 1024;
    next_off = get_be16(this->packet + start_off + 2);
    mp_msg(MSGT_SPUDEC,MSGL_DBG2, "date=%d\n", date);
    off = start_off + 4;
    for (type = this->packet[off++]; type != 0xff; type = this->packet[off++]) {
      mp_msg(MSGT_SPUDEC,MSGL_DBG2, "cmd=%d  ",type);
      switch(type) {
      case 0x00:
	/* Menu ID, 1 byte */
	mp_msg(MSGT_SPUDEC,MSGL_DBG2,"Menu ID\n");
        /* shouldn't a Menu ID type force display start? */
	start_pts = pts100 < 0 && -pts100 >= date ? 0 : pts100 + date;
	end_pts = UINT_MAX;
	display = 1;
	this->is_forced_sub=~0; // current subtitle is forced
	break;
      case 0x01:
	/* Start display */
	mp_msg(MSGT_SPUDEC,MSGL_DBG2,"Start display!\n");
	start_pts = pts100 < 0 && -pts100 >= date ? 0 : pts100 + date;
	end_pts = UINT_MAX;
	display = 1;
	this->is_forced_sub=0;
	break;
      case 0x02:
	/* Stop display */
	mp_msg(MSGT_SPUDEC,MSGL_DBG2,"Stop display!\n");
	end_pts = pts100 < 0 && -pts100 >= date ? 0 : pts100 + date;
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
	a = this->packet[off] >> 4;
	b = this->packet[off] & 0xf;
	c = this->packet[off + 1] >> 4;
	d = this->packet[off + 1] & 0xf;
	// Note: some DVDs change these values to create a fade-in/fade-out effect
	// We can not handle this, so just keep the highest value during the display time.
	if (display) {
		a = FFMAX(a, this->alpha[0]);
		b = FFMAX(b, this->alpha[1]);
		c = FFMAX(c, this->alpha[2]);
		d = FFMAX(d, this->alpha[3]);
	}
	this->alpha[0] = a;
	this->alpha[1] = b;
	this->alpha[2] = c;
	this->alpha[3] = d;
	mp_msg(MSGT_SPUDEC,MSGL_DBG2,"Alpha %d, %d, %d, %d\n",
	       this->alpha[0], this->alpha[1], this->alpha[2], this->alpha[3]);
	off+=2;
	break;
      case 0x05:
	/* Co-ords */
	a = get_be24(this->packet + off);
	b = get_be24(this->packet + off + 3);
	start_col = a >> 12;
	end_col = a & 0xfff;
	width = (end_col < start_col) ? 0 : end_col - start_col + 1;
	stride = (width + 7) & ~7; /* Kludge: draw_alpha needs width multiple of 8 */
	start_row = b >> 12;
	end_row = b & 0xfff;
	height = (end_row < start_row) ? 0 : end_row - start_row /* + 1 */;
	mp_msg(MSGT_SPUDEC,MSGL_DBG2,"Coords  col: %d - %d  row: %d - %d  (%dx%d)\n",
	       start_col, end_col, start_row, end_row,
	       width, height);
	off+=6;
	break;
      case 0x06:
	/* Graphic lines */
	current_nibble[0] = 2 * get_be16(this->packet + off);
	current_nibble[1] = 2 * get_be16(this->packet + off + 2);
	mp_msg(MSGT_SPUDEC,MSGL_DBG2,"Graphic offset 1: %d  offset 2: %d\n",
	       current_nibble[0] / 2, current_nibble[1] / 2);
	off+=4;
	break;
      default:
	mp_msg(MSGT_SPUDEC,MSGL_WARN,"spudec: Error determining control type 0x%02x.  Skipping %d bytes.\n",
	       type, next_off - off);
	goto next_control;
      }
    }
  next_control:
    if (!display)
      continue;
    if (end_pts == UINT_MAX && start_off != next_off) {
      end_pts = get_be16(this->packet + next_off) * 1024;
      end_pts = 1 - pts100 >= end_pts ? 0 : pts100 + end_pts - 1;
    }
    if (end_pts > 0) {
      packet_t *packet = calloc(1, sizeof(packet_t));
      int i;
      packet->start_pts = start_pts;
      packet->end_pts = end_pts;
      packet->current_nibble[0] = current_nibble[0];
      packet->current_nibble[1] = current_nibble[1];
      packet->start_row = start_row;
      packet->start_col = start_col;
      packet->width = width;
      packet->height = height;
      packet->stride = stride;
      packet->control_start = control_start;
      for (i=0; i<4; i++) {
	packet->alpha[i] = this->alpha[i];
	packet->palette[i] = this->palette[i];
      }
      packet->packet = malloc(this->packet_size);
      memcpy(packet->packet, this->packet, this->packet_size);
      spudec_queue_packet(this, packet);
    }
  }
}

static void spudec_decode(spudec_handle_t *this, int pts100)
{
  spudec_process_control(this, pts100);
}

int spudec_changed(void * this)
{
    spudec_handle_t * spu = this;
    return spu->spu_changed || spu->now_pts > spu->end_pts;
}

void spudec_assemble(void *this, unsigned char *packet, unsigned int len, int pts100)
{
  spudec_handle_t *spu = this;
//  spudec_heartbeat(this, pts100);
  if (len < 2) {
      mp_msg(MSGT_SPUDEC,MSGL_WARN,"SPUasm: packet too short\n");
      return;
  }
  spu->packet_pts = pts100;
  if (spu->packet_offset == 0) {
    unsigned int len2 = get_be16(packet);
    // Start new fragment
    if (spu->packet_reserve < len2) {
      free(spu->packet);
      spu->packet = malloc(len2);
      spu->packet_reserve = spu->packet != NULL ? len2 : 0;
    }
    if (spu->packet != NULL) {
      spu->packet_size = len2;
      if (len > len2) {
	mp_msg(MSGT_SPUDEC,MSGL_WARN,"SPUasm: invalid frag len / len2: %d / %d \n", len, len2);
	return;
      }
      memcpy(spu->packet, packet, len);
      spu->packet_offset = len;
      spu->packet_pts = pts100;
    }
  } else {
    // Continue current fragment
    if (spu->packet_size < spu->packet_offset + len){
      mp_msg(MSGT_SPUDEC,MSGL_WARN,"SPUasm: invalid fragment\n");
      spu->packet_size = spu->packet_offset = 0;
      return;
    } else {
      memcpy(spu->packet + spu->packet_offset, packet, len);
      spu->packet_offset += len;
    }
  }
#if 1
  // check if we have a complete packet (unfortunatelly packet_size is bad
  // for some disks)
  // [cb] packet_size is padded to be even -> may be one byte too long
  if ((spu->packet_offset == spu->packet_size) ||
      ((spu->packet_offset + 1) == spu->packet_size)){
    unsigned int x=0,y;
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
    // [cb] packet is done; start new packet
    spu->packet_offset = 0;
  }
#else
  if (spu->packet_offset == spu->packet_size) {
    spudec_decode(spu, pts100);
    spu->packet_offset = 0;
  }
#endif
}

void spudec_set_changed(void *this)
{
  spudec_handle_t *spu = this;

  spu->spu_changed = 1;
}

void spudec_reset(void *this)	// called after seek
{
  spudec_handle_t *spu = this;
  while (spu->queue_head)
    spudec_free_packet(spudec_dequeue_packet(spu));
  spu->now_pts = 0;
  spu->end_pts = 0;
  spu->packet_size = spu->packet_offset = 0;
  spudec_set_changed(spu);
}

void spudec_heartbeat(void *this, unsigned int pts100)
{
  spudec_handle_t *spu = this;
  spu->now_pts = pts100;

  // TODO: detect and handle broken timestamps (e.g. due to wrapping)
  while (spu->queue_head != NULL && pts100 >= spu->queue_head->start_pts) {
    packet_t *packet = spudec_dequeue_packet(spu);
    spu->start_pts = packet->start_pts;
    spu->end_pts = packet->end_pts;
    if (packet->is_decoded) {
      free(spu->image);
      spu->image_size = packet->data_len;
      spu->image      = packet->packet;
      packet->packet  = NULL;
      spu->width      = packet->width;
      spu->height     = packet->height;
      spu->stride     = packet->stride;
      spu->start_col  = packet->start_col;
      spu->start_row  = packet->start_row;
    } else {
      if (spu->auto_palette)
        compute_palette(spu, packet);
      spudec_process_data(spu, packet);
    }
    spudec_free_packet(packet);
    spudec_set_changed(spu);
  }
}

int spudec_visible(void *this){
    spudec_handle_t *spu = this;
    int ret=(spu->start_pts <= spu->now_pts &&
	     spu->now_pts < spu->end_pts &&
	     spu->height > 0);
//    printf("spu visible: %d  \n",ret);
    return ret;
}

void spudec_set_forced_subs_only(void * const this, const unsigned int flag)
{
    spudec_handle_t *spu = this;
    if (!!flag != !!spu->forced_subs_only) {
        spu->forced_subs_only = !!flag;
        spudec_set_changed(spu);
    }
}

void spudec_get_indexed(void *this, struct mp_osd_res *dim,
                        struct sub_bitmaps *res)
{
    spudec_handle_t *spu = this;
    *res = (struct sub_bitmaps) { .format = SUBBITMAP_INDEXED };
    struct sub_bitmap *part = &spu->borrowed_sub_part;
    res->parts = part;
    *part = spu->sub_part;
    // Empty subs do happen when cropping
    bool empty = part->w < 1 || part->h < 1;
    if (spudec_visible(spu) && !empty) {
        double xscale = (double) (dim->w - dim->ml - dim->mr) / spu->orig_frame_width;
        double yscale = (double) (dim->h - dim->mt - dim->mb) / spu->orig_frame_height;
        part->x = part->x * xscale + dim->ml;
        part->y = part->y * yscale + dim->mt;
        part->dw = part->w * xscale;
        part->dh = part->h * yscale;
        res->num_parts = 1;
        res->scaled = true;
    }
    if (spu->spu_changed) {
        res->bitmap_id = res->bitmap_pos_id = 1;
        spu->spu_changed = 0;
    }
}

static unsigned int vobsub_palette_to_yuv(unsigned int pal)
{
    int r, g, b, y, u, v;
    // Palette in idx file is not rgb value, it was calculated by wrong formula.
    // Here's reversed formula of the one used to generate palette in idx file.
    r = pal >> 16 & 0xff;
    g = pal >> 8 & 0xff;
    b = pal & 0xff;
    y = av_clip_uint8( 0.1494  * r + 0.6061 * g + 0.2445 * b);
    u = av_clip_uint8( 0.6066  * r - 0.4322 * g - 0.1744 * b + 128);
    v = av_clip_uint8(-0.08435 * r - 0.3422 * g + 0.4266 * b + 128);
    y = y * 219 / 255 + 16;
    return y << 16 | u << 8 | v;
}

static unsigned int vobsub_rgb_to_yuv(unsigned int rgb)
{
    int r, g, b, y, u, v;
    r = rgb >> 16 & 0xff;
    g = rgb >> 8 & 0xff;
    b = rgb & 0xff;
    y = ( 0.299   * r + 0.587   * g + 0.114   * b) * 219 / 255 + 16.5;
    u = (-0.16874 * r - 0.33126 * g + 0.5     * b) * 224 / 255 + 128.5;
    v = ( 0.5     * r - 0.41869 * g - 0.08131 * b) * 224 / 255 + 128.5;
    return y << 16 | u << 8 | v;
}

static void spudec_parse_extradata(spudec_handle_t *this,
                                   uint8_t *extradata, int extradata_len)
{
  uint8_t *buffer, *ptr;
  unsigned int *pal = this->global_palette, *cuspal = this->cuspal;
  unsigned int tridx;
  int i;

  if (extradata_len == 16*4) {
    for (i=0; i<16; i++)
      pal[i] = AV_RB32(extradata + i*4);
    this->auto_palette = 0;
    return;
  }

  if (!(ptr = buffer = malloc(extradata_len+1)))
    return;
  memcpy(buffer, extradata, extradata_len);
  buffer[extradata_len] = 0;

  do {
    if (*ptr == '#')
        continue;
    if (!strncmp(ptr, "size: ", 6))
        sscanf(ptr + 6, "%dx%d", &this->orig_frame_width, &this->orig_frame_height);
    if (!strncmp(ptr, "palette: ", 9) &&
        sscanf(ptr + 9, "%x, %x, %x, %x, %x, %x, %x, %x, "
                        "%x, %x, %x, %x, %x, %x, %x, %x",
               &pal[ 0], &pal[ 1], &pal[ 2], &pal[ 3],
               &pal[ 4], &pal[ 5], &pal[ 6], &pal[ 7],
               &pal[ 8], &pal[ 9], &pal[10], &pal[11],
               &pal[12], &pal[13], &pal[14], &pal[15]) == 16) {
      for (i=0; i<16; i++)
        pal[i] = vobsub_palette_to_yuv(pal[i]);
      this->auto_palette = 0;
    }
    if (!strncasecmp(ptr, "forced subs: on", 15))
      this->forced_subs_only = 1;
    if (!strncmp(ptr, "custom colors: ON, tridx: ", 26) &&
        sscanf(ptr + 26, "%x, colors: %x, %x, %x, %x",
               &tridx, cuspal+0, cuspal+1, cuspal+2, cuspal+3) == 5) {
      for (i=0; i<4; i++) {
        cuspal[i] = vobsub_rgb_to_yuv(cuspal[i]);
        if (tridx & (1 << (12-4*i)))
          cuspal[i] |= 1 << 31;
      }
      this->custom = 1;
    }
  } while ((ptr=strchr(ptr,'\n')) && *++ptr);

  free(buffer);
}

void *spudec_new_scaled(unsigned int frame_width, unsigned int frame_height, uint8_t *extradata, int extradata_len)
{
  spudec_handle_t *this = calloc(1, sizeof(spudec_handle_t));
  if (this){
    this->orig_frame_height = frame_height;
    this->orig_frame_width  = frame_width;
    // set up palette:
    this->auto_palette = 1;
    if (extradata)
      spudec_parse_extradata(this, extradata, extradata_len);
    /* XXX Although the video frame is some size, the SPU frame is
       always maximum size i.e. 720 wide and 576 or 480 high */
    // For HD files in MKV the VobSub resolution can be higher though,
    // see largeres_vobsub.mkv
    if (this->orig_frame_width <= 720 && this->orig_frame_height <= 576) {
      this->orig_frame_width = 720;
      if (this->orig_frame_height == 480 || this->orig_frame_height == 240)
        this->orig_frame_height = 480;
      else
        this->orig_frame_height = 576;
    }
  }
  else
    mp_msg(MSGT_SPUDEC,MSGL_FATAL, "FATAL: spudec_init: calloc");
  return this;
}

void spudec_free(void *this)
{
  spudec_handle_t *spu = this;
  if (spu) {
    while (spu->queue_head)
      spudec_free_packet(spudec_dequeue_packet(spu));
    free(spu->packet);
    spu->packet = NULL;
    free(spu->image);
    spu->image = NULL;
    free(spu->pal_image);
    spu->pal_image = NULL;
    spu->image_size = 0;
    spu->pal_width = spu->pal_height  = 0;
    free(spu);
  }
}
