/* author:	Tilman Sauerbeck <tsauerbeck@users.sourceforge.net>
 * based on:	XreaL's x_r_img_tga.* (http://www.sourceforge.net/projects/xreal/)
 *		libtarga.*
 *		xli's tga.*
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "bswap.h"
#include "postproc/rgb2rgb.h"
#include "libvo/fastmemcpy.h"

#include "vd_internal.h"

static vd_info_t info =
{
    "TGA Images decoder",
    "mtga",
    "Tilman Sauerbeck",
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
} TGAInfo;

typedef struct
{
    /* red, green, blue, alpha */
    unsigned char   r;
    unsigned char   g;
    unsigned char   b;
    unsigned char   a;
} ColorChannels;


static unsigned int out_fmt = 0;

static int last_w = -1;
static int last_h = -1;
static int last_c = -1;

/* to set/get/query special features/parameters */
static int control(sh_video_t *sh, int cmd, void *arg, ...)
{
    return CONTROL_UNKNOWN;
}

/* init driver */
static int init(sh_video_t *sh)
{
    last_w = -1;
    return 1;
}


/* uninit driver */
static void uninit(sh_video_t *sh)
{
}


/* decode a runlength-encoded tga */
static void decode_rle_tga(TGAInfo info, unsigned char *data, mp_image_t **mpi)
{
    ColorChannels   chans = {0, 0, 0, 0};
    unsigned char   repetitions, packet_header, *final;
    unsigned short  row, col, i;
    short	    modifier;


    /* if img.origin is 0, we decode from bottom to top. if it's 1, we decode from top to bottom */
    row = (info.origin) ? 0 : info.height - 1;
    modifier = (info.origin) ? 1 : -1;
   
    for (;;)
    {
	final = (*mpi)->planes[0] + (*mpi)->stride[0] * row;

	for (col = 0; col < info.width;)
	{
	    packet_header = *data++;
	    repetitions = 1 + (packet_header & 0x7f);
	    
	    if (packet_header & 0x80)
	    {
		chans.b = *data++;
	        chans.g = *data++;
	        chans.r = *data++;
	        chans.a = (info.bpp == 32) ? *data++ : 255;

		for (i = 0; i < repetitions; i++)
		{
		    *final++ = chans.r;
		    *final++ = chans.g;
		    *final++ = chans.b;
		    *final++ = chans.a;
		    
		    col++;
		}
	    }
	    else /* raw packet */
	    {
		for (i = 0; i < repetitions; i++)
		{
		    chans.b = *data++;
		    chans.g = *data++;
		    chans.r = *data++;
	
		    *final++ = chans.r;
		    *final++ = chans.g;
		    *final++ = chans.b;
		    *final++ = chans.a = (info.bpp == 32) ? *data++ : 255;
		    
		    col++;
		}
	    }
	}

	row = row + modifier;

	if ((!info.origin && !row) || (info.origin && row >= info.height))
	    break;
    }

    
    return;
}


static void decode_uncompressed_tga(TGAInfo info, unsigned char *data, mp_image_t **mpi)
{
    ColorChannels   chans;
    unsigned short  row, col;
    unsigned char   *final;
    short	    modifier;


    /* if img.origin is 0, we decode from bottom to top. if it's 1, we decode from top to bottom */
    row = (info.origin) ? 0 : info.height - 1;
    modifier = (info.origin) ? 1 : -1;
    
    for (;;)
    {
	final = (*mpi)->planes[0] + (*mpi)->stride[0] * row;

	for (col = 0; col < info.width; col++)
	{
	    chans.b = *data++;
	    chans.g = *data++;
	    chans.r = *data++;

	    *final++ = chans.r;
	    *final++ = chans.g;
	    *final++ = chans.b;
	    *final++ = info.bpp == 32 ? *data++ : 255;

	}

	row = row + modifier;

	if ((!info.origin && !row) || (info.origin && row >= info.height))
	    break;
    }


    return;
}


static short read_tga_header(unsigned char *buf, TGAInfo *info)
{
    (*info).id_len = buf[0];
    
    (*info).img_type = buf[2];

    /* targa data is always stored in little endian byte order */
    (*info).width = le2me_16(*(unsigned short *) &buf[12]);
    (*info).height = le2me_16(*(unsigned short *) &buf[14]);

    (*info).bpp = buf[16];
    
    (*info).origin = (buf[17] & 0x20) >> 5;
 
    /* FIXME check for valid targa data */
    
    return 0;
}


/* decode a frame */
static mp_image_t *decode(sh_video_t *sh, void *raw, int len, int flags)
{
    TGAInfo	    info;
    unsigned char   *data = raw;
    mp_image_t	    *mpi;
    
    
    if (len <= 0)
	return NULL; /* skip frame */

    read_tga_header(data, &info); /* read information about the file */
    
    if (info.bpp == 24)
	out_fmt = IMGFMT_RGB24;
    else if (info.bpp == 32)
	out_fmt = IMGFMT_RGB32;
    else
    {
	mp_msg(MSGT_DECVIDEO, MSGL_INFO, "Unsupported TGA type!\n");
	return NULL;
    }
    
    if (info.img_type != TGA_UNCOMP_TRUECOLOR && info.img_type != TGA_RLE_TRUECOLOR) /* not a true color image */
    {
	mp_msg(MSGT_DECVIDEO, MSGL_INFO, "Unsupported TGA type: %i!\n", info.img_type);
	return NULL;
    }

    /* set data to the beginning of the image data */
    data = data + 18 + info.id_len;
  
    /* (re)init libvo if image parameters changed (width/height/colorspace) */
    if (last_w != info.width || last_h != info.height || last_c != out_fmt)
    {
	last_w = info.width;
	last_h = info.height;
	last_c = out_fmt;
	
	if (!out_fmt || !mpcodecs_config_vo(sh, info.width, info.height, out_fmt))
	    return NULL;
    }

    if (!(mpi = mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE, info.width, info.height)))
	return NULL;
  
    /* finally decode the image */
    if (info.img_type == TGA_UNCOMP_TRUECOLOR)
        decode_uncompressed_tga(info, data, &mpi);
    else if (info.img_type == TGA_RLE_TRUECOLOR)
	decode_rle_tga(info, data, &mpi);
    else
	mpi = NULL;


    return mpi;
}

