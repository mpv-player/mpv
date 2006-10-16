/*
    Copyright (C) 2006 Michael Niedermayer <michaelni@gmx.at>

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
	char eq[3][200];
        int framenum;
        mp_image_t *mpi;
};

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
        int i;

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

//FIXME spatial interpolate
//FIXME keep the last few frames
static double lum(struct vf_instance_s* vf, double x, double y){
    mp_image_t *mpi= vf->priv->mpi;
    x= clip(x, 0, vf->priv->mpi->w-1);
    y= clip(y, 0, vf->priv->mpi->h-1);
    return mpi->planes[0][(int)x + (int)y * mpi->stride[0]];
}

static double cb(struct vf_instance_s* vf, double x, double y){
    mp_image_t *mpi= vf->priv->mpi;
    x= clip(x, 0, (vf->priv->mpi->w >> mpi->chroma_x_shift)-1);
    y= clip(y, 0, (vf->priv->mpi->h >> mpi->chroma_y_shift)-1);
    return mpi->planes[1][(int)x + (int)y * mpi->stride[1]];
}

static double cr(struct vf_instance_s* vf, double x, double y){
    mp_image_t *mpi= vf->priv->mpi;
    x= clip(x, 0, (vf->priv->mpi->w >> mpi->chroma_x_shift)-1);
    y= clip(y, 0, (vf->priv->mpi->h >> mpi->chroma_y_shift)-1);
    return mpi->planes[2][(int)x + (int)y * mpi->stride[2]];
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts){
	mp_image_t *dmpi;
        int x,y, plane;
        static const char *const_names[]={
            "PI",
            "E",
            "X",
            "Y",
            "W",
            "H",
            "N",
            "SW",
            "SH",
            NULL
        };
        static const char *func2_names[]={
            "lum",
            "cb",
            "cr",
            "p",
            NULL
        };

	if(!(mpi->flags&MP_IMGFLAG_DIRECT)){
		// no DR, so get a new image! hope we'll get DR buffer:
		vf->dmpi=vf_get_image(vf->next,mpi->imgfmt,
		MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_PREFER_ALIGNED_STRIDE,
		mpi->w,mpi->h);
	}

	dmpi= vf->dmpi;
        vf->priv->mpi= mpi;

        vf_clone_mpi_attributes(dmpi, mpi);

        for(plane=0; plane<3; plane++){
            int w= mpi->w >> (plane ? mpi->chroma_x_shift : 0);
            int h= mpi->h >> (plane ? mpi->chroma_y_shift : 0);
            uint8_t *dst  = dmpi->planes[plane];
            int dst_stride= dmpi->stride[plane];
            double (*func2[])(void *, double, double)={
                lum,
                cb,
                cr,
                plane==0 ? lum : (plane==1 ? cb : cr),
                NULL
            };
            double const_values[]={
                M_PI,
                M_E,
                0,
                0,
                w,
                h,
                vf->priv->framenum,
                w/(double)mpi->w,
                h/(double)mpi->h,
                0
            };
            for(y=0; y<h; y++){
                const_values[3]=y;
                for(x=0; x<w; x++){
                    const_values[2]=x;
                    dst[x+y* dst_stride]= ff_eval(vf->priv->eq[plane], const_values, const_names, NULL, NULL, func2, func2_names, vf);
                }
            }
        }

        vf->priv->framenum++;

	return vf_next_put_image(vf,dmpi, pts);
}

static void uninit(struct vf_instance_s* vf){
	if(!vf->priv) return;

	av_free(vf->priv);
	vf->priv=NULL;
}

//===========================================================================//
static int open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->put_image=put_image;
//    vf->get_image=get_image;
    vf->uninit=uninit;
    vf->priv=av_malloc(sizeof(struct vf_priv_s));
    memset(vf->priv, 0, sizeof(struct vf_priv_s));

    if (args) sscanf(args, "%199s:%199s:%199s", vf->priv->eq[0], vf->priv->eq[1], vf->priv->eq[2]);

    if(!vf->priv->eq[1][0]) strncpy(vf->priv->eq[1], vf->priv->eq[0], 199);
    if(!vf->priv->eq[2][0]) strncpy(vf->priv->eq[2], vf->priv->eq[1], 199);

    return 1;
}

vf_info_t vf_info_geq = {
    "generic equation filter",
    "geq",
    "Michael Niedermayer",
    "",
    open,
    NULL
};
