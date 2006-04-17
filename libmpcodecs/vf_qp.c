/*
    Copyright (C) 2004 Michael Niedermayer <michaelni@gmx.at>

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
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

#include "config.h"

#include "mp_msg.h"
#include "cpudetect.h"

#if 1
double ff_eval(char *s, double *const_value, const char **const_name,
               double (**func1)(void *, double), const char **func1_name,
               double (**func2)(void *, double, double), char **func2_name,
               void *opaque);
#endif

// Needed to bring in lrintf.
#define HAVE_AV_CONFIG_H

#include "libavcodec/avcodec.h"
#include "libavcodec/dsputil.h"
#include "libavutil/common.h"

/* FIXME: common.h defines printf away when HAVE_AV_CONFIG
 * is defined, but mp_image.h needs printf.
 */
#undef printf

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"
#include "libvo/fastmemcpy.h"


struct vf_priv_s {
	char eq[200];
	int8_t *qp;
	int8_t lut[257];
	int qp_stride;
};

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
	int h= (height+15)>>4;
        int i;

	vf->priv->qp_stride= (width+15)>>4;
        vf->priv->qp= av_malloc(vf->priv->qp_stride*h*sizeof(int8_t));
        
        for(i=-129; i<128; i++){
            double const_values[]={
                M_PI,
                M_E,
                i != -129,
                i,
                0
            };
            static const char *const_names[]={
                "PI",
                "E",
                "known",
                "qp",
                NULL
            };

            vf->priv->lut[i+129]= lrintf(ff_eval(vf->priv->eq, const_values, const_names, NULL, NULL, NULL, NULL, NULL));
        }

	return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static void get_image(struct vf_instance_s* vf, mp_image_t *mpi){
    if(mpi->flags&MP_IMGFLAG_PRESERVE) return; // don't change
    // ok, we can do pp in-place (or pp disabled):
    vf->dmpi=vf_get_image(vf->next,mpi->imgfmt,
        mpi->type, mpi->flags, mpi->w, mpi->h);
    mpi->planes[0]=vf->dmpi->planes[0];
    mpi->stride[0]=vf->dmpi->stride[0];
    mpi->width=vf->dmpi->width;
    if(mpi->flags&MP_IMGFLAG_PLANAR){
        mpi->planes[1]=vf->dmpi->planes[1];
        mpi->planes[2]=vf->dmpi->planes[2];
	mpi->stride[1]=vf->dmpi->stride[1];
	mpi->stride[2]=vf->dmpi->stride[2];
    }
    mpi->flags|=MP_IMGFLAG_DIRECT;
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts){
	mp_image_t *dmpi;
        int x,y;

	if(!(mpi->flags&MP_IMGFLAG_DIRECT)){
		// no DR, so get a new image! hope we'll get DR buffer:
		vf->dmpi=vf_get_image(vf->next,mpi->imgfmt,
		MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_PREFER_ALIGNED_STRIDE,
		mpi->w,mpi->h);
	}

	dmpi= vf->dmpi;
        
	if(!(mpi->flags&MP_IMGFLAG_DIRECT)){
		memcpy_pic(dmpi->planes[0], mpi->planes[0], mpi->w, mpi->h, dmpi->stride[0], mpi->stride[0]);
    		if(mpi->flags&MP_IMGFLAG_PLANAR){
		    memcpy_pic(dmpi->planes[1], mpi->planes[1], mpi->w>>mpi->chroma_x_shift, mpi->h>>mpi->chroma_y_shift, dmpi->stride[1], mpi->stride[1]);
		    memcpy_pic(dmpi->planes[2], mpi->planes[2], mpi->w>>mpi->chroma_x_shift, mpi->h>>mpi->chroma_y_shift, dmpi->stride[2], mpi->stride[2]);
		}
	}
        vf_clone_mpi_attributes(dmpi, mpi);
        
        dmpi->qscale = vf->priv->qp;
        dmpi->qstride= vf->priv->qp_stride;
        if(mpi->qscale){
            for(y=0; y<((dmpi->h+15)>>4); y++){
                for(x=0; x<vf->priv->qp_stride; x++){
                    dmpi->qscale[x + dmpi->qstride*y]= 
                        vf->priv->lut[ 129 + ((int8_t)mpi->qscale[x + mpi->qstride*y]) ];
                }
            }
        }else{
            int qp= vf->priv->lut[0];
            for(y=0; y<((dmpi->h+15)>>4); y++){
                for(x=0; x<vf->priv->qp_stride; x++){
                    dmpi->qscale[x + dmpi->qstride*y]= qp;
                }
            }
        }

	return vf_next_put_image(vf,dmpi, pts);
}

static void uninit(struct vf_instance_s* vf){
	if(!vf->priv) return;

	if(vf->priv->qp) av_free(vf->priv->qp);
	vf->priv->qp= NULL;
	
	av_free(vf->priv);
	vf->priv=NULL;
}

//===========================================================================//
static int open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->put_image=put_image;
    vf->get_image=get_image;
    vf->uninit=uninit;
    vf->priv=av_malloc(sizeof(struct vf_priv_s));
    memset(vf->priv, 0, sizeof(struct vf_priv_s));
    
//    avcodec_init();

    if (args) strncpy(vf->priv->eq, args, 199);
	
    return 1;
}

vf_info_t vf_info_qp = {
    "QP changer",
    "qp",
    "Michael Niedermayer",
    "",
    open,
    NULL
};
