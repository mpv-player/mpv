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
#include <inttypes.h>

#include "config.h"
#include "core/mp_msg.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"

struct vf_priv_s {
    int direction;
};

static void rotate(unsigned char* dst,unsigned char* src,int dststride,int srcstride,int w,int h,int bpp,int dir){
    int y;
    if(dir&1){
	src+=srcstride*(w-1);
	srcstride*=-1;
    }
    if(dir&2){
	dst+=dststride*(h-1);
	dststride*=-1;
    }

    for(y=0;y<h;y++){
	int x;
	switch(bpp){
	case 1:
	    for(x=0;x<w;x++) dst[x]=src[y+x*srcstride];
	    break;
	case 2:
	    for(x=0;x<w;x++) *((short*)(dst+x*2))=*((short*)(src+y*2+x*srcstride));
	    break;
	case 4:
	    for(x=0;x<w;x++) *((int*)(dst+x*4))=*((int*)(src+y*4+x*srcstride));
        default:
            for(x=0;x<w;x++){
                for (int b=0;b<bpp;b++)
                    dst[x*bpp+b]=src[b+y*bpp+x*srcstride];
            }
            break;
	}
	dst+=dststride;
    }
}

static int config(struct vf_instance *vf, int width, int height,
                  int d_width, int d_height,
                  unsigned int flags, unsigned int outfmt)
{
    if (vf->priv->direction & 4) {
        if (width < height)
            vf->priv->direction &= 3;
    }
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(outfmt);
    int a_w = MP_ALIGN_DOWN(width, desc.align_x);
    int a_h = MP_ALIGN_DOWN(height, desc.align_y);
    vf_rescale_dsize(&d_width, &d_height, width, height, a_w, a_h);
    return vf_next_config(vf, a_h, a_w, d_height, d_width, flags, outfmt);
}

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
    if (vf->priv->direction & 4)
        return mpi;

    struct mp_image *dmpi = vf_alloc_out_image(vf);
    mp_image_copy_attributes(dmpi, mpi);

    for (int p = 0; p < mpi->num_planes; p++) {
        rotate(dmpi->planes[p],mpi->planes[p], dmpi->stride[p],mpi->stride[p],
               dmpi->plane_w[p], dmpi->plane_h[p], mpi->fmt.bytes[p],
               vf->priv->direction);
    }

    talloc_free(mpi);
    return dmpi;
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
    if (!(desc.flags & MP_IMGFLAG_BYTE_ALIGNED))
        return 0;
    if (desc.chroma_xs != desc.chroma_ys)
        return 0;
    if (desc.num_planes == 1 && (desc.chroma_xs || desc.chroma_ys))
        return 0;
    return vf_next_query_format(vf, fmt);
}

static int vf_open(vf_instance_t *vf, char *args){
    vf->config=config;
    vf->filter=filter;
    vf->query_format=query_format;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    vf->priv->direction=args?atoi(args):0;
    return 1;
}

const vf_info_t vf_info_rotate = {
    "rotate",
    "rotate",
    "A'rpi",
    "",
    vf_open,
    NULL
};

//===========================================================================//
