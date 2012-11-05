/*
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
#include "core/mp_msg.h"

#include "video/mp_image.h"
#include "vf.h"

#include "video/out/vo.h"

//===========================================================================//

static int config(struct vf_instance *vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    flags&=~VOFLAG_FLIPPING; // remove the FLIP flag
    return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
    mpi->planes[0]=mpi->planes[0]+
		    mpi->stride[0]*(mpi->height-1);
    mpi->stride[0]=-mpi->stride[0];
    if(mpi->flags&MP_IMGFLAG_PLANAR){
        mpi->planes[1]=mpi->planes[1]+
	    mpi->stride[1]*((mpi->height>>mpi->chroma_y_shift)-1);
	mpi->stride[1]=-mpi->stride[1];
	mpi->planes[2]=mpi->planes[2]+
	    mpi->stride[2]*((mpi->height>>mpi->chroma_y_shift)-1);
	mpi->stride[2]=-mpi->stride[2];
    }
    return mpi;
}

//===========================================================================//

static int vf_open(vf_instance_t *vf, char *args){
    vf->config=config;
    vf->filter=filter;
    return 1;
}

const vf_info_t vf_info_flip = {
    "flip image upside-down",
    "flip",
    "A'rpi",
    "",
    vf_open,
    NULL
};

//===========================================================================//
