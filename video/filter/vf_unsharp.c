/*
 * Copyright (C) 2002 Remi Guyomarch <rguyom@pobox.com>
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
#include <inttypes.h>
#include <math.h>

#include "config.h"
#include "mpvcore/mp_msg.h"
#include "mpvcore/cpudetect.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"
#include "video/memcpy_pic.h"
#include "libavutil/common.h"

//===========================================================================//

#define MIN_MATRIX_SIZE 3
#define MAX_MATRIX_SIZE 63

typedef struct FilterParam {
    int msizeX, msizeY;
    double amount;
    uint32_t *SC[MAX_MATRIX_SIZE-1];
} FilterParam;

struct vf_priv_s {
    FilterParam lumaParam;
    FilterParam chromaParam;
    unsigned int outfmt;
};


//===========================================================================//

/* This code is based on :

An Efficient algorithm for Gaussian blur using finite-state machines
Frederick M. Waltz and John W. V. Miller

SPIE Conf. on Machine Vision Systems for Inspection and Metrology VII
Originally published Boston, Nov 98

*/

static void unsharp( uint8_t *dst, uint8_t *src, int dstStride, int srcStride, int width, int height, FilterParam *fp ) {

    uint32_t **SC = fp->SC;
    uint32_t SR[MAX_MATRIX_SIZE-1], Tmp1, Tmp2;
    uint8_t* src2 = src; // avoid gcc warning

    int32_t res;
    int x, y, z;
    int amount = fp->amount * 65536.0;
    int stepsX = fp->msizeX/2;
    int stepsY = fp->msizeY/2;
    int scalebits = (stepsX+stepsY)*2;
    int32_t halfscale = 1 << ((stepsX+stepsY)*2-1);

    if( !fp->amount ) {
	if( src == dst )
	    return;
	if( dstStride == srcStride )
	    memcpy( dst, src, srcStride*height );
	else
	    for( y=0; y<height; y++, dst+=dstStride, src+=srcStride )
		memcpy( dst, src, width );
	return;
    }

    for( y=0; y<2*stepsY; y++ )
	memset( SC[y], 0, sizeof(SC[y][0]) * (width+2*stepsX) );

    for( y=-stepsY; y<height+stepsY; y++ ) {
	if( y < height ) src2 = src;
	memset( SR, 0, sizeof(SR[0]) * (2*stepsX-1) );
	for( x=-stepsX; x<width+stepsX; x++ ) {
	    Tmp1 = x<=0 ? src2[0] : x>=width ? src2[width-1] : src2[x];
	    for( z=0; z<stepsX*2; z+=2 ) {
		Tmp2 = SR[z+0] + Tmp1; SR[z+0] = Tmp1;
		Tmp1 = SR[z+1] + Tmp2; SR[z+1] = Tmp2;
	    }
	    for( z=0; z<stepsY*2; z+=2 ) {
		Tmp2 = SC[z+0][x+stepsX] + Tmp1; SC[z+0][x+stepsX] = Tmp1;
		Tmp1 = SC[z+1][x+stepsX] + Tmp2; SC[z+1][x+stepsX] = Tmp2;
	    }
	    if( x>=stepsX && y>=stepsY ) {
		uint8_t* srx = src - stepsY*srcStride + x - stepsX;
		uint8_t* dsx = dst - stepsY*dstStride + x - stepsX;

		res = (int32_t)*srx + ( ( ( (int32_t)*srx - (int32_t)((Tmp1+halfscale) >> scalebits) ) * amount ) >> 16 );
		*dsx = res>255 ? 255 : res<0 ? 0 : (uint8_t)res;
	    }
	}
	if( y >= 0 ) {
	    dst += dstStride;
	    src += srcStride;
	}
    }
}

//===========================================================================//

static int config( struct vf_instance *vf,
		   int width, int height, int d_width, int d_height,
		   unsigned int flags, unsigned int outfmt ) {

    int z, stepsX, stepsY;
    FilterParam *fp;
    char *effect;

    // allocate buffers

    fp = &vf->priv->lumaParam;
    effect = fp->amount == 0 ? "don't touch" : fp->amount < 0 ? "blur" : "sharpen";
    mp_msg( MSGT_VFILTER, MSGL_INFO, "unsharp: %dx%d:%0.2f (%s luma) \n", fp->msizeX, fp->msizeY, fp->amount, effect );
    memset( fp->SC, 0, sizeof( fp->SC ) );
    stepsX = fp->msizeX/2;
    stepsY = fp->msizeY/2;
    for( z=0; z<2*stepsY; z++ )
	fp->SC[z] = av_malloc(sizeof(*(fp->SC[z])) * (width+2*stepsX));

    fp = &vf->priv->chromaParam;
    effect = fp->amount == 0 ? "don't touch" : fp->amount < 0 ? "blur" : "sharpen";
    mp_msg( MSGT_VFILTER, MSGL_INFO, "unsharp: %dx%d:%0.2f (%s chroma)\n", fp->msizeX, fp->msizeY, fp->amount, effect );
    memset( fp->SC, 0, sizeof( fp->SC ) );
    stepsX = fp->msizeX/2;
    stepsY = fp->msizeY/2;
    for( z=0; z<2*stepsY; z++ )
	fp->SC[z] = av_malloc(sizeof(*(fp->SC[z])) * (width+2*stepsX));

    return vf_next_config( vf, width, height, d_width, d_height, flags, outfmt );
}

//===========================================================================//

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
    struct mp_image *dmpi = mpi;
    if (!mp_image_is_writeable(mpi)) {
        dmpi = vf_alloc_out_image(vf);
        mp_image_copy_attributes(dmpi, mpi);
    }

    unsharp( dmpi->planes[0], mpi->planes[0], dmpi->stride[0], mpi->stride[0], mpi->w,   mpi->h,   &vf->priv->lumaParam );
    unsharp( dmpi->planes[1], mpi->planes[1], dmpi->stride[1], mpi->stride[1], mpi->w/2, mpi->h/2, &vf->priv->chromaParam );
    unsharp( dmpi->planes[2], mpi->planes[2], dmpi->stride[2], mpi->stride[2], mpi->w/2, mpi->h/2, &vf->priv->chromaParam );

#if HAVE_MMX
    if(gCpuCaps.hasMMX)
	__asm__ volatile ("emms\n\t");
#endif
#if HAVE_MMX2
    if(gCpuCaps.hasMMX2)
	__asm__ volatile ("sfence\n\t");
#endif

    if (dmpi != mpi)
        talloc_free(mpi);
    return dmpi;
}

static void uninit( struct vf_instance *vf ) {
    unsigned int z;
    FilterParam *fp;

    if( !vf->priv ) return;

    fp = &vf->priv->lumaParam;
    for( z=0; z<sizeof(fp->SC)/sizeof(fp->SC[0]); z++ ) {
	av_free( fp->SC[z] );
	fp->SC[z] = NULL;
    }
    fp = &vf->priv->chromaParam;
    for( z=0; z<sizeof(fp->SC)/sizeof(fp->SC[0]); z++ ) {
	av_free( fp->SC[z] );
	fp->SC[z] = NULL;
    }

    free( vf->priv );
    vf->priv = NULL;
}

//===========================================================================//

static int query_format( struct vf_instance *vf, unsigned int fmt ) {
    switch(fmt) {
    case IMGFMT_420P:
	return vf_next_query_format( vf, vf->priv->outfmt );
    }
    return 0;
}

//===========================================================================//

static void parse( FilterParam *fp, char* args ) {

    // l7x5:0.8:c3x3:-0.2

    char *z;
    char *pos = args;
    char *max = args + strlen(args);

    // parse matrix sizes
    fp->msizeX = ( pos && pos+1<max ) ? atoi( pos+1 ) : 0;
    z = strchr( pos+1, 'x' );
    fp->msizeY = ( z && z+1<max ) ? atoi( pos=z+1 ) : fp->msizeX;

    // min/max & odd
    fp->msizeX = 1 | av_clip(fp->msizeX, MIN_MATRIX_SIZE, MAX_MATRIX_SIZE);
    fp->msizeY = 1 | av_clip(fp->msizeY, MIN_MATRIX_SIZE, MAX_MATRIX_SIZE);

    // parse amount
    pos = strchr( pos+1, ':' );
    fp->amount = ( pos && pos+1<max ) ? atof( pos+1 ) : 0;
}

//===========================================================================//

static const unsigned int fmt_list[] = {
    IMGFMT_420P,
    0
};

static int vf_open( vf_instance_t *vf, char *args ) {
    vf->config       = config;
    vf->filter       = filter;
    vf->query_format = query_format;
    vf->uninit       = uninit;
    vf->priv         = malloc( sizeof(struct vf_priv_s) );
    memset( vf->priv, 0, sizeof(struct vf_priv_s) );

    if( args ) {
	char *args2 = strchr( args, 'l' );
	if( args2 )
	    parse( &vf->priv->lumaParam, args2 );
	else {
	    vf->priv->lumaParam.amount =
	    vf->priv->lumaParam.msizeX =
	    vf->priv->lumaParam.msizeY = 0;
	}

	args2 = strchr( args, 'c' );
	if( args2 )
	    parse( &vf->priv->chromaParam, args2 );
	else {
	    vf->priv->chromaParam.amount =
	    vf->priv->chromaParam.msizeX =
	    vf->priv->chromaParam.msizeY = 0;
	}

	if( !vf->priv->lumaParam.msizeX && !vf->priv->chromaParam.msizeX )
	    return 0; // nothing to do
    }

    // check csp:
    vf->priv->outfmt = vf_match_csp( &vf->next, fmt_list, IMGFMT_420P );
    if( !vf->priv->outfmt ) {
	uninit( vf );
        return 0; // no csp match :(
    }

    return 1;
}

const vf_info_t vf_info_unsharp = {
    "unsharp mask & gaussian blur",
    "unsharp",
    "Remi Guyomarch",
    "",
    vf_open,
    NULL
};

//===========================================================================//
