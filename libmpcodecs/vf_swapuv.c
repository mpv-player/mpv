/*
 * Copyright (C) 2002 Michael Niedermayer <michaelni@gmx.at>
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
#include <assert.h>

#include "config.h"
#include "mp_msg.h"

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"


//===========================================================================//

static void get_image(struct vf_instance_s* vf, mp_image_t *mpi){
    mp_image_t *dmpi= vf_get_image(vf->next, mpi->imgfmt, 
	mpi->type, mpi->flags, mpi->w, mpi->h);

    mpi->planes[0]=dmpi->planes[0];
    mpi->planes[1]=dmpi->planes[2];
    mpi->planes[2]=dmpi->planes[1];
    mpi->stride[0]=dmpi->stride[0];
    mpi->stride[1]=dmpi->stride[2];
    mpi->stride[2]=dmpi->stride[1];
    mpi->width=dmpi->width;

    mpi->flags|=MP_IMGFLAG_DIRECT;
    mpi->priv=(void*)dmpi;
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts){
    mp_image_t *dmpi;
    
    if(mpi->flags&MP_IMGFLAG_DIRECT){
	dmpi=(mp_image_t*)mpi->priv;
    } else {
	dmpi=vf_get_image(vf->next, mpi->imgfmt, MP_IMGTYPE_EXPORT, 0, mpi->w, mpi->h);
	assert(mpi->flags&MP_IMGFLAG_PLANAR);
	dmpi->planes[0]=mpi->planes[0];
	dmpi->planes[1]=mpi->planes[2];
	dmpi->planes[2]=mpi->planes[1];
	dmpi->stride[0]=mpi->stride[0];
	dmpi->stride[1]=mpi->stride[2];
	dmpi->stride[2]=mpi->stride[1];
	dmpi->width=mpi->width;
    }
    
    vf_clone_mpi_attributes(dmpi, mpi);

    return vf_next_put_image(vf,dmpi, pts);
}

//===========================================================================//

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
	switch(fmt)
	{
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	case IMGFMT_YVU9:
	case IMGFMT_444P:
	case IMGFMT_422P:
	case IMGFMT_411P:
		return vf_next_query_format(vf, fmt);
	}
	return 0;
}

static int open(vf_instance_t *vf, char* args){
    vf->put_image=put_image;
    vf->get_image=get_image;
    vf->query_format=query_format;
    return 1;
}

const vf_info_t vf_info_swapuv = {
    "UV swapper",
    "swapuv",
    "Michael Niedermayer",
    "",
    open,
    NULL
};

//===========================================================================//
