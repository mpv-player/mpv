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
#include "mp_msg.h"

#include "mp_image.h"
#include "vf.h"

#include "libvo/video_out.h"

//===========================================================================//

static int config(struct vf_instance *vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    flags&=~VOFLAG_FLIPPING; // remove the FLIP flag
    return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static void get_image(struct vf_instance *vf, mp_image_t *mpi){
    if(mpi->flags&MP_IMGFLAG_ACCEPT_STRIDE){
	// try full DR !
	vf->dmpi=vf_get_image(vf->next,mpi->imgfmt,
	    mpi->type, mpi->flags, mpi->width, mpi->height);
	// set up mpi as a upside-down image of dmpi:
	mpi->planes[0]=vf->dmpi->planes[0]+
		    vf->dmpi->stride[0]*(vf->dmpi->height-1);
	mpi->stride[0]=-vf->dmpi->stride[0];
	if(mpi->flags&MP_IMGFLAG_PLANAR){
	    mpi->planes[1]=vf->dmpi->planes[1]+
		    vf->dmpi->stride[1]*((vf->dmpi->height>>mpi->chroma_y_shift)-1);
	    mpi->stride[1]=-vf->dmpi->stride[1];
	    mpi->planes[2]=vf->dmpi->planes[2]+
		    vf->dmpi->stride[2]*((vf->dmpi->height>>mpi->chroma_y_shift)-1);
	    mpi->stride[2]=-vf->dmpi->stride[2];
	}
	mpi->flags|=MP_IMGFLAG_DIRECT;
	mpi->priv=(void*)vf->dmpi;
    }
}

static int put_image(struct vf_instance *vf, mp_image_t *mpi, double pts){
    if(mpi->flags&MP_IMGFLAG_DIRECT){
	// we've used DR, so we're ready...
	if(!(mpi->flags&MP_IMGFLAG_PLANAR))
	    ((mp_image_t*)mpi->priv)->planes[1] = mpi->planes[1]; // passthrough rgb8 palette
	return vf_next_put_image(vf,(mp_image_t*)mpi->priv, pts);
    }

    vf->dmpi=vf_get_image(vf->next,mpi->imgfmt,
	MP_IMGTYPE_EXPORT, MP_IMGFLAG_ACCEPT_STRIDE,
	mpi->width, mpi->height);

    // set up mpi as a upside-down image of dmpi:
    vf->dmpi->planes[0]=mpi->planes[0]+
		    mpi->stride[0]*(mpi->height-1);
    vf->dmpi->stride[0]=-mpi->stride[0];
    if(vf->dmpi->flags&MP_IMGFLAG_PLANAR){
        vf->dmpi->planes[1]=mpi->planes[1]+
	    mpi->stride[1]*((mpi->height>>mpi->chroma_y_shift)-1);
	vf->dmpi->stride[1]=-mpi->stride[1];
	vf->dmpi->planes[2]=mpi->planes[2]+
	    mpi->stride[2]*((mpi->height>>mpi->chroma_y_shift)-1);
	vf->dmpi->stride[2]=-mpi->stride[2];
    } else
	vf->dmpi->planes[1]=mpi->planes[1]; // passthru bgr8 palette!!!

    return vf_next_put_image(vf,vf->dmpi, pts);
}

//===========================================================================//

static int vf_open(vf_instance_t *vf, char *args){
    vf->config=config;
    vf->get_image=get_image;
    vf->put_image=put_image;
    vf->default_reqs=VFCAP_ACCEPT_STRIDE;
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
