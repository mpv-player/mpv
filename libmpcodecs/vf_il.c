/*
    Copyright (C) 2002 Michael Niedermayer <michaelni@gmx.at>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include "../config.h"
#include "../mp_msg.h"

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"
#include "../libvo/fastmemcpy.h"


//===========================================================================//


struct vf_priv_s {
	int interleave;
	int chroma;
	int swap;
};


/***************************************************************************/


static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){

	return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static int interleave(uint8_t *dst, uint8_t *src, int w, int h, int dstStride, int srcStride, int interleave, int swap){
	const int a= swap;
	const int b= 1-a;
	const int m= h>>1;
	int y;

	switch(interleave){
	case -1:
		for(y=0; y < m; y++){
			memcpy(dst + dstStride* y     , src + srcStride*(y*2 + a), w);
			memcpy(dst + dstStride*(y + m), src + srcStride*(y*2 + b), w);
		}
		break;
	case 0:
		for(y=0; y < m; y++){
			memcpy(dst + dstStride* y*2   , src + srcStride*(y*2 + a), w);
			memcpy(dst + dstStride*(y*2+1), src + srcStride*(y*2 + b), w);
		}
		break;
	case 1:
		for(y=0; y < m; y++){
			memcpy(dst + dstStride*(y*2+a), src + srcStride* y     , w);
			memcpy(dst + dstStride*(y*2+b), src + srcStride*(y + m), w);
		}
		break;
	}
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi){
	int w;

	mp_image_t *dmpi=vf_get_image(vf->next,mpi->imgfmt,
		MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
		mpi->w,mpi->h);

	if(mpi->flags&MP_IMGFLAG_PLANAR)
		w= mpi->w;
	else
		w= mpi->w * mpi->bpp/8;

	interleave(dmpi->planes[0], mpi->planes[0], 
		w, mpi->h, dmpi->stride[0], mpi->stride[0], vf->priv->interleave, vf->priv->swap);

	if(mpi->flags&MP_IMGFLAG_PLANAR){
		int cw= mpi->w >> mpi->chroma_x_shift;
		int ch= mpi->h >> mpi->chroma_y_shift;

	
		if(vf->priv->chroma){
			interleave(dmpi->planes[1], mpi->planes[1], cw,ch, 
				dmpi->stride[1], mpi->stride[1], vf->priv->interleave, vf->priv->swap);
			interleave(dmpi->planes[2], mpi->planes[2], cw,ch, 
				dmpi->stride[2], mpi->stride[2], vf->priv->interleave, vf->priv->swap);
		}else{
			int y;
			for(y=0; y < ch; y++)
			    memcpy(dmpi->planes[1] + dmpi->stride[1]*y, mpi->planes[1] + mpi->stride[1]*y, cw);
			for(y=0; y < ch; y++)
			    memcpy(dmpi->planes[2] + dmpi->stride[2]*y, mpi->planes[2] + mpi->stride[2]*y, cw);
		}
	}
    
	return vf_next_put_image(vf,dmpi);
}

//===========================================================================//

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
	/* we support all formats :) */
	return vf_next_query_format(vf, fmt); 
}

static int open(vf_instance_t *vf, char* args){
	char *pos, *max;

	vf->config=config;
	vf->put_image=put_image;
//	vf->get_image=get_image;
	vf->query_format=query_format;
	vf->priv=malloc(sizeof(struct vf_priv_s));
	memset(vf->priv, 0, sizeof(struct vf_priv_s));

	if(args==NULL) return 0;

	max= args + strlen(args);

	pos= strchr(args, 's');
	if(pos && pos<max) vf->priv->swap=1;
	pos= strchr(args, 'c');
	if(pos && pos<max) vf->priv->chroma=1;
	pos= strchr(args, 'i');
	if(pos && pos<max) vf->priv->interleave=1;
	pos= strchr(args, 'd');
	if(pos && pos<max) vf->priv->interleave=-1;

	return 1;
}

vf_info_t vf_info_il = {
    "(de)interleave",
    "il",
    "Michael Niedermayer",
    "",
    open
};

//===========================================================================//
