/*
 * based on:	XreaL's x_r_img_tga.* (http://www.sourceforge.net/projects/xreal/)
 *		libtarga.*
 *		xli's tga.*
 *
 * Copyright (c) 2002 Tilman Sauerbeck <tsauerbeck@users.sourceforge.net>
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
#include <string.h>

#include "config.h"
#include "mp_msg.h"

#include "mpbswap.h"
#include "libvo/fastmemcpy.h"

#include "vd_internal.h"

static const vd_info_t info =
{
    "TGA Images decoder",
    "mtga",
    "Tilman Sauerbeck, A'rpi",
    "Tilman Sauerbeck",
    "only 24bpp and 32bpp RGB targa files support so far"
};

LIBVD_EXTERN(mtga)

typedef enum
{
    TGA_NO_DATA,
    TGA_UNCOMP_PALETTED,
    TGA_UNCOMP_TRUECOLOR,
    TGA_UNCOMP_GRAYSCALE,
    TGA_RLE_PALETTED = 9,
    TGA_RLE_TRUECOLOR,
    TGA_RLE_GRAYSCALE
} TGAImageType;

typedef struct
{
    unsigned char   id_len;
    unsigned short  img_type;

    unsigned short  width;
    unsigned short  height;

    unsigned char   bpp;
    unsigned char   origin; /* 0 = lower left, 1 = upper left */
    unsigned short  start_row;
    short           increment;
} TGAInfo;

static unsigned int out_fmt = 0;

static int last_w = -1;
static int last_h = -1;
static int last_c = -1;


/* to set/get/query special features/parameters */
static int control(sh_video_t *sh, int cmd, void *arg, ...)
{
    switch (cmd)
    {
	case VDCTRL_QUERY_FORMAT:
	    if (*((int *) arg) == out_fmt) return CONTROL_TRUE;
	    return CONTROL_FALSE;
    }
    return CONTROL_UNKNOWN;
}

/* init driver */
static int init(sh_video_t *sh)
{
    sh->context = calloc(1, sizeof(TGAInfo));
    last_w = -1;

    return 1;
}


/* uninit driver */
static void uninit(sh_video_t *sh)
{
    TGAInfo	*info = sh->context;
    free(info);
    return;
}


/* decode a runlength-encoded tga */
static void decode_rle_tga(TGAInfo *info, unsigned char *data, mp_image_t *mpi)
{
    int	    row, col, replen, i, num_bytes = info->bpp / 8;
    unsigned char   repetitions, packet_header, *final;

    /* see line 207 to see why this loop is set up like this */
    for (row = info->start_row; (!info->origin && row) || (info->origin && row < info->height); row += info->increment)
    {
	final = mpi->planes[0] + mpi->stride[0] * row;

	for (col = 0; col < info->width; col += repetitions)
	{
	    packet_header = *data++;
	    repetitions = (1 + (packet_header & 0x7f));
	    replen = repetitions * num_bytes;

	    if (packet_header & 0x80) /* runlength encoded packet */
	    {
		memcpy(final, data, num_bytes);

		// Note: this will be slow when DR to vram!
		i=num_bytes;
		while(2*i<=replen){
		    memcpy(final+i,final,i);
		    i*=2;
		}
		memcpy(final+i,final,replen-i);
		data += num_bytes;
	    }
	    else /* raw packet */
	    {
		fast_memcpy(final, data, replen);
		data += replen;
	    }

	    final += replen;
	}
    }

    return;
}


static void decode_uncompressed_tga(TGAInfo *info, unsigned char *data, mp_image_t *mpi)
{
    unsigned char   *final;
    int	    row, num_bytes = info->bpp / 8;

    /* see line 207 to see why this loop is set up like this */
    for (row = info->start_row; (!info->origin && row) || (info->origin && row < info->height); row += info->increment)
    {
	final = mpi->planes[0] + mpi->stride[0] * row;
	fast_memcpy(final, data, info->width * num_bytes);
	data += info->width * num_bytes;
    }

    return;
}


static short read_tga_header(unsigned char *buf, TGAInfo *info)
{
    info->id_len = buf[0];

    info->img_type = buf[2];

    /* targa data is always stored in little endian byte order */
    info->width = le2me_16(*(unsigned short *) &buf[12]);
    info->height = le2me_16(*(unsigned short *) &buf[14]);

    info->bpp = buf[16];

    info->origin = (buf[17] & 0x20) >> 5;

    /* FIXME check for valid targa data */

    return 0;
}


/* decode a frame */
static mp_image_t *decode(sh_video_t *sh, void *raw, int len, int flags)
{
    TGAInfo	    *info = sh->context;
    unsigned char   *data = raw;
    mp_image_t	    *mpi;


    if (len <= 0)
	return NULL; /* skip frame */

    read_tga_header(data, info); /* read information about the file */

    if (info->bpp == 24)
	out_fmt = IMGFMT_BGR24;
    else if (info->bpp == 32)
	out_fmt = IMGFMT_BGR32;
    else
    {
	mp_msg(MSGT_DECVIDEO, MSGL_INFO, "Unsupported TGA type! depth=%d\n",info->bpp);
	return NULL;
    }

    if (info->img_type != TGA_UNCOMP_TRUECOLOR && info->img_type != TGA_RLE_TRUECOLOR) /* not a true color image */
    {
	mp_msg(MSGT_DECVIDEO, MSGL_INFO, "Unsupported TGA type: %i!\n", info->img_type);
	return NULL;
    }

    /* if img.origin is 0, we decode from bottom to top. if it's 1, we decode from top to bottom */
    info->start_row = (info->origin) ? 0 : info->height - 1;
    info->increment = (info->origin) ? 1 : -1;

    /* set data to the beginning of the image data */
    data += 18 + info->id_len;

    /* (re)init libvo if image parameters changed (width/height/colorspace) */
    if (last_w != info->width || last_h != info->height || last_c != out_fmt)
    {
	last_w = info->width;
	last_h = info->height;
	last_c = out_fmt;

	if (!out_fmt || !mpcodecs_config_vo(sh, info->width, info->height, out_fmt))
	    return NULL;
    }

    if (!(mpi = mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE, info->width, info->height)))
	return NULL;

    /* finally decode the image */
    if (info->img_type == TGA_UNCOMP_TRUECOLOR)
	decode_uncompressed_tga(info, data, mpi);
    else if (info->img_type == TGA_RLE_TRUECOLOR)
	decode_rle_tga(info, data, mpi);
//    else
//	mpi = NULL;

    return mpi;
}
