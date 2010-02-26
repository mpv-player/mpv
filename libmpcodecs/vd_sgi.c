/*
 * Copyright (c) 2003 Todd Kirby <slapcat@pacbell.net>
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

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"
#include "libavutil/common.h"
#include "mpbswap.h"
#include "vd_internal.h"

#define SGI_HEADER_LEN 512
#define SGI_MAGIC 474

#define SGI_GRAYSCALE_IMAGE 1
#define SGI_RGB_IMAGE 3
#define SGI_RGBA_IMAGE 4

#define OUT_PIXEL_STRIDE 3 /* RGB */


static const vd_info_t info =
{
  "SGI Image decoder",
  "sgi",
  "Todd Kirby",
  "Todd Kirby",
  ""
};

LIBVD_EXTERN(sgi)

typedef struct {
  short magic;
  char rle;
  char bytes_per_channel;
  unsigned short dimension;
  unsigned short xsize;
  unsigned short ysize;
  unsigned short zsize;
} SGIInfo;

static unsigned int outfmt = IMGFMT_BGR24;

static unsigned short last_x = -1;
static unsigned short last_y = -1;


/* to set/get/query special features/parameters */
static int
control(sh_video_t* sh, int cmd, void *arg, ...)
{
  switch (cmd)
  {
    case VDCTRL_QUERY_FORMAT:
      if (*((unsigned int *) arg) == outfmt) {
        return CONTROL_TRUE;
      }
      return CONTROL_FALSE;
  }
  return CONTROL_UNKNOWN;
}


/* init driver */
static int
init(sh_video_t *sh)
{
  sh->context = calloc(1, sizeof(SGIInfo));
  last_x = -1;

  return 1;
}


/* uninit driver */
static void
uninit(sh_video_t *sh)
{
  SGIInfo	*info = sh->context;
  free(info);
}


/* expand an rle row into a channel */
static void
expandrow(unsigned char *optr, unsigned char *iptr, int chan_offset)
{
  unsigned char pixel, count;
  optr += chan_offset;

  while (1) {
    pixel = *iptr++;

    if (!(count = (pixel & 0x7f))) {
      return;
    }
    if(pixel & 0x80) {
      while (count--) {
        *optr = *iptr;
        optr += OUT_PIXEL_STRIDE;
        iptr++;
      }
    } else {
      pixel = *iptr++;

      while (count--) {
        *optr = pixel;
        optr += OUT_PIXEL_STRIDE;
      }
    }
  }
}


/* expand an rle row into all 3 channels.
   a separate function for grayscale so we don't slow down the
   more common case rgb function with a bunch of ifs. */
static void
expandrow_gs(unsigned char *optr, unsigned char *iptr)
{
  unsigned char pixel, count;

  while (1) {
    pixel = *iptr++;

    if (!(count = (pixel & 0x7f))) {
      return;
    }
    if(pixel & 0x80) {
      while (count--) {
        optr[0] = *iptr;
        optr[1] = *iptr;
        optr[2] = *iptr;
        optr += OUT_PIXEL_STRIDE;
        iptr++;
      }
    } else {
      pixel = *iptr++;

      while (count--) {
        optr[0] = pixel;
        optr[1] = pixel;
        optr[2] = pixel;
        optr += OUT_PIXEL_STRIDE;
      }
    }
  }
}


/* decode a run length encoded sgi image */
static void
decode_rle_sgi(SGIInfo *info, unsigned char *data, mp_image_t *mpi)
{
  unsigned char *rle_data, *dest_row;
  uint32_t *starttab;
  int y, z, xsize, ysize, zsize, chan_offset;
  long start_offset;

  xsize = info->xsize;
  ysize = info->ysize;
  zsize = info->zsize;

  /* rle offset table is right after the header */
  starttab = (uint32_t*)(data + SGI_HEADER_LEN);

   for (z = 0; z < zsize; z++) {

     /* set chan_offset so RGB ends up BGR */
     chan_offset = (zsize - 1) - z;

     /* The origin for SGI images is the lower-left corner
        so read scan lines from bottom to top */
     for (y = ysize - 1; y >= 0; y--) {
       dest_row = mpi->planes[0] + mpi->stride[0] * (ysize - 1 - y);

      /* set start of next run (offsets are from start of header) */
      start_offset = be2me_32(*(uint32_t*) &starttab[y + z * ysize]);

      rle_data = &data[start_offset];

      if(info->zsize == SGI_GRAYSCALE_IMAGE) {
        expandrow_gs(dest_row, rle_data);
      } else {
        expandrow(dest_row, rle_data, chan_offset);
      }
    }
  }
}


/* decode an sgi image */
static void
decode_uncompressed_sgi(SGIInfo *info, unsigned char *data, mp_image_t *mpi)
{
  unsigned char *src_row, *dest_row;
  int x, y, z, xsize, ysize, zsize, chan_offset;

  xsize = info->xsize;
  ysize = info->ysize;
  zsize = info->zsize;

  /* skip header */
  data += SGI_HEADER_LEN;

  for (z = 0; z < zsize; z++) {

    /* set row ptr to start of current plane */
    src_row = data + (xsize * ysize * z);

    /* set chan_offset for RGB -> BGR */
    chan_offset = (zsize - 1) - z;

    /* the origin for SGI images is the lower-left corner
       so read scan lines from bottom to top. */
    for (y = ysize - 1; y >= 0; y--) {
      dest_row = mpi->planes[0] + mpi->stride[0] * y;
      for (x = 0; x < xsize; x++) {

        /* we only do 24 bit output so promote 8 bit pixels to 24 */
        if (zsize == SGI_GRAYSCALE_IMAGE) {
          /* write greyscale value into all channels */
          dest_row[0] = src_row[x];
          dest_row[1] = src_row[x];
          dest_row[2] = src_row[x];
        } else {
          dest_row[chan_offset] = src_row[x];
        }

        dest_row += OUT_PIXEL_STRIDE;
      }

      /* move to next row of the current source plane */
      src_row += xsize;
    }
  }
}


/* read sgi header fields */
static void
read_sgi_header(unsigned char *buf, SGIInfo *info)
{
  /* sgi data is always stored in big endian byte order */
  info->magic = be2me_16(*(unsigned short *) &buf[0]);
  info->rle = buf[2];
  info->bytes_per_channel = buf[3];
  info->dimension = be2me_16(*(unsigned short *) &buf[4]);
  info->xsize = be2me_16(*(unsigned short *) &buf[6]);
  info->ysize = be2me_16(*(unsigned short *) &buf[8]);
  info->zsize = be2me_16(*(unsigned short *) &buf[10]);
}


/* decode a frame */
static
mp_image_t *decode(sh_video_t *sh, void *raw, int len, int flags)
{
  SGIInfo *info = sh->context;
  unsigned char *data = raw;
  mp_image_t *mpi;

  if (len <= 0) {
    return NULL; /* skip frame */
  }

  read_sgi_header(data, info);

  /* make sure this is an SGI image file */
  if (info->magic != SGI_MAGIC) {
    mp_msg(MSGT_DECVIDEO, MSGL_INFO, "Bad magic number in image.\n");
    return NULL;
  }

  /* check image depth */
  if (info->bytes_per_channel != 1) {
    mp_msg(MSGT_DECVIDEO, MSGL_INFO,
        "Unsupported bytes per channel value %i.\n", info->bytes_per_channel);
    return NULL;
  }

  /* check image dimension */
  if (info->dimension != 2 && info->dimension != 3) {
    mp_msg(MSGT_DECVIDEO, MSGL_INFO, "Unsupported image dimension %i.\n",
        info->dimension);
    return NULL;
  }

  /* change rgba images to rgb so alpha channel will be ignored */
  if (info->zsize == SGI_RGBA_IMAGE) {
    info->zsize = SGI_RGB_IMAGE;
  }

  /* check image depth */
  if (info->zsize != SGI_RGB_IMAGE && info->zsize != SGI_GRAYSCALE_IMAGE) {
    mp_msg(MSGT_DECVIDEO, MSGL_INFO, "Unsupported image depth.\n");
    return NULL;
  }

  /* (re)init libvo if image size is changed */
  if (last_x != info->xsize || last_y != info->ysize)
  {
    last_x = info->xsize;
    last_y = info->ysize;

    if (!mpcodecs_config_vo(sh, info->xsize, info->ysize, outfmt)) {
      mp_msg(MSGT_DECVIDEO, MSGL_INFO, "Config vo failed:\n");
      return NULL;
    }
  }

  if (!(mpi = mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
          info->xsize, info->ysize))) {
    return NULL;
  }

  if (info->rle) {
    decode_rle_sgi(info, data, mpi);
  } else {
    decode_uncompressed_sgi(info, data, mpi);
  }

  return mpi;
}
