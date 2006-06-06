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
#include <inttypes.h>
#include <math.h>

#include "config.h"

#include "mp_msg.h"

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"
#include "libvo/fastmemcpy.h"

#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#define ABS(a) ((a) > 0 ? (a) : (-(a)))

#define MIN3(a,b,c) MIN(MIN(a,b),c)
#define MAX3(a,b,c) MAX(MAX(a,b),c)

//===========================================================================//

struct vf_priv_s {
    int mode;
    int parity;
    int stride[3];
    uint8_t *ref[3][3];
};

static void store_ref(struct vf_priv_s *p, uint8_t *src[3], int src_stride[3], int width, int height){
    int i;

    memcpy (p->ref[2], p->ref[0], sizeof(uint8_t *)*3);
    memmove(p->ref[0], p->ref[1], sizeof(uint8_t *)*3*2);

    for(i=0; i<3; i++){
        int is_chroma= !!i;

        memcpy_pic(p->ref[1][i], src[i], width>>is_chroma, height>>is_chroma, p->stride[i], src_stride[i]);
    }
}

static void filter(struct vf_priv_s *p, uint8_t *dst[3], uint8_t *src[3], int dst_stride[3], int src_stride[3], int width, int height, int parity, int tff){
    int x, y, i, j, k;

    for(i=0; i<3; i++){
        int is_chroma= !!i;
        int w= width >>is_chroma;
        int h= height>>is_chroma;
        int refs= p->stride[i];
        int srcs= src_stride[i];

//        assert(refs == src_stride[i]); //FIXME

        for(y=0; y<h; y++){
            if((y ^ parity) & 1){
                for(x=0; x<w; x++){
                    if(x>0 && y>0 && x+1<w && y+1<h){
                        uint8_t *prev= &p->ref[0][i][x + y*refs];
                        uint8_t *cur = &p->ref[1][i][x + y*refs];
                        uint8_t *next= &src[i][x + y*srcs];
                        uint8_t *prev2= (tff ^ (y&1)) ? cur  : prev;
                        uint8_t *next2= (tff ^ (y&1)) ? next : cur ;
                        int next2s=     (tff ^ (y&1)) ? srcs : refs;

                        int temporal_diff0= ABS(prev2[0] - next2[0]);
                        int temporal_diff1=( ABS(prev[-refs] - cur[-refs]) + ABS(prev[+refs] - cur[+refs]) )>>1;
                        int temporal_diff2=( ABS(next[-srcs] - cur[-refs]) + ABS(next[+srcs] - cur[+refs]) )>>1;
                        int diff= MAX(temporal_diff0>>1, MAX(temporal_diff1, temporal_diff2));
                        int temporal_pred= (prev2[0] + next2[0])>>1;
                        int spatial_pred= 0;
                        int spatial_score= 1<<30;
                        int v= temporal_pred;
#if 0
#define RANGE 8
                        for(j=-RANGE; j<=RANGE; j++){
                            int score= 0;
                            for(k=-ABS(j)-1; k<=ABS(j)+1; k++)
                                score+= ABS(cur[-refs+k+j] - cur[+refs+k-j]);
#else
#define RANGE 1
                        for(j=-RANGE; j<=RANGE; j++){
                            int score= ABS(cur[-refs-1+j] - cur[+refs-1-j])
                                     + ABS(cur[-refs  +j] - cur[+refs  -j])
                                     + ABS(cur[-refs+1+j] - cur[+refs+1-j])
                                     + ABS(j);
#endif

                            if(score < spatial_score){
                                spatial_score= score;
                                spatial_pred= (cur[-refs  +j] + cur[+refs  -j])>>1;
                            }
                        }
                        if(y>1 && y+2<h){
                            int b= (prev2[-2*refs] + next2[-2*next2s])>>1;
                            int c= cur[-refs];
                            int d= temporal_pred;
                            int e= cur[+refs];
                            int f= (prev2[+2*refs] + next2[+2*next2s])>>1;
#if 0
                            int a= y>2 ? cur[-3*refs] : 0;
                            int g= y+3<h ? cur[+3*refs] : 0;
                            int max= MAX3(d-e, d-c, MIN3(MAX(b-c,f-e),MAX(b-c,b-a),MAX(f-g,f-e)) );
                            int min= MIN3(d-e, d-c, MAX3(MIN(b-c,f-e),MIN(b-c,b-a),MIN(f-g,f-e)) );
#else
                            int max= MAX3(d-e, d-c, MIN(b-c, f-e));
                            int min= MIN3(d-e, d-c, MAX(b-c, f-e));
#endif

                            diff= MAX3(diff, min, -max);
                        }

                        if(v < spatial_pred) v= MIN(v + diff, spatial_pred);
                        else                 v= MAX(v - diff, spatial_pred);

                        dst[i][x + y*dst_stride[i]]= v;
                    }else
                        dst[i][x + y*dst_stride[i]]= p->ref[1][i][x + y*refs];
                }
            }else{
                for(x=0; x<w; x++){
                    dst[i][x + y*dst_stride[i]]= p->ref[1][i][x + y*refs];
                }
            }
        }
    }
}

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
        int i;

        for(i=0; i<3; i++){
            int is_chroma= !!i;
            int w= ((width  + 31) & (~31))>>is_chroma;
            int h= ((height + 31) & (~31))>>is_chroma;

            vf->priv->stride[i]= w;
            vf->priv->ref[0][i]= malloc(w*h*sizeof(uint8_t));
            vf->priv->ref[1][i]= malloc(w*h*sizeof(uint8_t));
        }

	return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts){
    mp_image_t *dmpi;
    int ret=0;
    int tff, i;

    if(vf->priv->parity < 0) {
        if (mpi->fields & MP_IMGFIELD_ORDERED)
            tff = !!(mpi->fields & MP_IMGFIELD_TOP_FIRST);
        else
            tff = 1;
    }
    else tff = (vf->priv->parity&1)^1;

    for(i=0; i<=vf->priv->mode; i++){
        dmpi=vf_get_image(vf->next,mpi->imgfmt,
            MP_IMGTYPE_TEMP,
            MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_PREFER_ALIGNED_STRIDE,
            mpi->width,mpi->height);
        vf_clone_mpi_attributes(dmpi, mpi);
        filter(vf->priv, dmpi->planes, mpi->planes, dmpi->stride, mpi->stride, mpi->w, mpi->h, i ^ tff ^ 1, tff);
        ret |= vf_next_put_image(vf, dmpi, pts /*FIXME*/);
        if(i<vf->priv->mode)
            vf_next_control(vf, VFCTRL_FLIP_PAGE, NULL);
    }
    store_ref(vf->priv, mpi->planes, mpi->stride, mpi->w, mpi->h);

    return ret;
}

static void uninit(struct vf_instance_s* vf){
    int i;
    if(!vf->priv) return;

    for(i=0; i<3*2; i++){
        if(vf->priv->ref[i&1][i/2]) free(vf->priv->ref[i&1][i/2]);
        vf->priv->ref[i&1][i/2]= NULL;
    }
    free(vf->priv);
    vf->priv=NULL;
}

//===========================================================================//
static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    switch(fmt){
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	case IMGFMT_Y800:
	case IMGFMT_Y8:
	    return vf_next_query_format(vf,fmt);
    }
    return 0;
}

static int open(vf_instance_t *vf, char* args){

    vf->config=config;
    vf->put_image=put_image;
    vf->query_format=query_format;
    vf->uninit=uninit;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    memset(vf->priv, 0, sizeof(struct vf_priv_s));

    vf->priv->mode=0;
    vf->priv->parity= -1;

    if (args) sscanf(args, "%d:%d", &vf->priv->mode, &vf->priv->parity);

    if(vf->priv->mode < 0 || vf->priv->mode > 1)
        vf->priv->mode=0;

    return 1;
}

vf_info_t vf_info_yadif = {
    "Yet Another DeInterlacing Filter",
    "yadif",
    "Michael Niedermayer",
    "",
    open,
    NULL
};
