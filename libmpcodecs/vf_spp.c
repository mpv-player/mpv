/*
    Copyright (C) 2003 Michael Niedermayer <michaelni@gmx.at>

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

/*
 * This implementation is based on an algorithm described in
 * "Aria Nosratinia Embedded Post-Processing for 
 * Enhancement of Compressed Images (1999)"
 * (http://citeseer.nj.nec.com/nosratinia99embedded.html)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

#include "../config.h"
#include "../mp_msg.h"
#include "../cpudetect.h"
#include "../libavcodec/avcodec.h"
#include "../libavcodec/dsputil.h"

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"
#include "../libvo/fastmemcpy.h"

#define XMIN(a,b) ((a) < (b) ? (a) : (b))

//===========================================================================//
const uint8_t  __attribute__((aligned(8))) dither[8][8]={
{  0,  48,  12,  60,   3,  51,  15,  63, },
{ 32,  16,  44,  28,  35,  19,  47,  31, },
{  8,  56,   4,  52,  11,  59,   7,  55, },
{ 40,  24,  36,  20,  43,  27,  39,  23, },
{  2,  50,  14,  62,   1,  49,  13,  61, },
{ 34,  18,  46,  30,  33,  17,  45,  29, },
{ 10,  58,   6,  54,   9,  57,   5,  53, },
{ 42,  26,  38,  22,  41,  25,  37,  21, },
};

const uint8_t offset[127][2]= {
{0,0},
{0,0}, {4,4},
{0,0}, {2,2}, {6,4}, {4,6},
{0,0}, {5,1}, {2,2}, {7,3}, {4,4}, {1,5}, {6,6}, {3,7}, 

{0,0}, {4,0}, {1,1}, {5,1}, {3,2}, {7,2}, {2,3}, {6,3}, 
{0,4}, {4,4}, {1,5}, {5,5}, {3,6}, {7,6}, {2,7}, {6,7}, 

{0,0}, {0,2}, {0,4}, {0,6}, {1,1}, {1,3}, {1,5}, {1,7}, 
{2,0}, {2,2}, {2,4}, {2,6}, {3,1}, {3,3}, {3,5}, {3,7}, 
{4,0}, {4,2}, {4,4}, {4,6}, {5,1}, {5,3}, {5,5}, {5,7}, 
{6,0}, {6,2}, {6,4}, {6,6}, {7,1}, {7,3}, {7,5}, {7,7}, 

{0,0}, {4,4}, {0,4}, {4,0}, {2,2}, {6,6}, {2,6}, {6,2}, 
{0,2}, {4,6}, {0,6}, {4,2}, {2,0}, {6,4}, {2,4}, {6,0}, 
{1,1}, {5,5}, {1,5}, {5,1}, {3,3}, {7,7}, {3,7}, {7,3}, 
{1,3}, {5,7}, {1,7}, {5,3}, {3,1}, {7,5}, {3,5}, {7,1}, 
{0,1}, {4,5}, {0,5}, {4,1}, {2,3}, {6,7}, {2,7}, {6,3}, 
{0,3}, {4,7}, {0,7}, {4,3}, {2,1}, {6,5}, {2,5}, {6,1}, 
{1,0}, {5,4}, {1,4}, {5,0}, {3,2}, {7,6}, {3,6}, {7,2}, 
{1,2}, {5,6}, {1,6}, {5,2}, {3,0}, {7,4}, {3,4}, {7,0},
};

struct vf_priv_s {
	int log2_count;
	int qp;
	int mpeg2;
	unsigned int outfmt;
	int temp_stride;
	uint8_t *src;
	int16_t *temp;
	AVCodecContext *avctx;
	DSPContext dsp;
};

#define SHIFT 22

static inline void requantize(DCTELEM dst[64], DCTELEM src[64], int qp, uint8_t *permutation){
	int i; 
	const int qmul= qp<<1;
	int bias= 0; //FIXME
	unsigned int threshold1, threshold2;
	
	threshold1= qmul*((1<<3) - bias) - 1;
	threshold2= (threshold1<<1);
        
	memset(dst, 0, 64*sizeof(DCTELEM));
	dst[0]= (src[0] + 4)>>3;

        for(i=1; i<64; i++){
		int level= src[i];
		if(((unsigned)(level+threshold1))>threshold2){
			const int j= permutation[i];
			dst[j]= (level + 4)>>3;
		}
	}
}

static inline void add_block(int16_t *dst, int stride, DCTELEM block[64]){
	int y;
	
	for(y=0; y<8; y++){
		*(uint32_t*)&dst[0 + y*stride]+= *(uint32_t*)&block[0 + y*8];
		*(uint32_t*)&dst[2 + y*stride]+= *(uint32_t*)&block[2 + y*8];
		*(uint32_t*)&dst[4 + y*stride]+= *(uint32_t*)&block[4 + y*8];
		*(uint32_t*)&dst[6 + y*stride]+= *(uint32_t*)&block[6 + y*8];
	}
}

static void filter(struct vf_priv_s *p, uint8_t *dst, uint8_t *src, int dst_stride, int src_stride, int width, int height, uint8_t *qp_store, int qp_stride, int is_luma){
	int x, y, i;
	const int count= 1<<p->log2_count;
	const int log2_scale= 6-p->log2_count;
	const int stride= p->temp_stride; //FIXME
	uint64_t block_align[32];
	DCTELEM *block = (DCTELEM *)block_align;
	DCTELEM *block2= (DCTELEM *)(block_align+16);
        
	for(y=0; y<height; y++){
		memcpy(p->src + 8 + 8*stride + y*stride, src + y*src_stride, width);
		memset(p->temp + 8*stride + y*stride, 0, stride*sizeof(int16_t));
		for(x=0; x<8; x++){ 
			int index= 8 + 8*stride + y*stride;
			p->src[index         - x - 1]= p->src[index +         x    ];
			p->src[index + width + x    ]= p->src[index + width - x - 1];
		}
	}
	for(y=0; y<8; y++){
		memcpy(p->src + (      7-y)*stride, p->src + (      y+8)*stride, stride);
		memcpy(p->src + (height+8+y)*stride, p->src + (height-y+7)*stride, stride);
	}
	//FIXME (try edge emu)

	for(y=0; y<height+8; y+=8){
		for(x=0; x<width+8; x+=8){
			const int qps= 3 + is_luma;
			int qp;
                        
			if(p->qp)
				qp= p->qp;
			else{
				qp= qp_store[ (XMIN(x, width-1)>>qps) + (XMIN(y, height-1)>>qps) * qp_stride];
				if(p->mpeg2) qp>>=1;
			}
			for(i=0; i<count; i++){
				const int x1= x + offset[i+count-1][0];
				const int y1= y + offset[i+count-1][1];
				const int index= x1 + y1*stride;
				p->dsp.get_pixels(block, p->src + index, stride);
				p->dsp.fdct(block);
				requantize(block2, block, qp, p->dsp.idct_permutation);
				p->dsp.idct(block2);
				add_block(p->temp + index, stride, block2);
			}
		}
	}
        
#define STORE(pos) \
	temp= ((p->temp[index + pos]<<log2_scale) + d[pos])>>6;\
	if(temp & 0x100) temp= ~(temp>>31);\
	dst[x + y*dst_stride + pos]= temp;

	for(y=0; y<height; y++){
		uint8_t *d= dither[y&7];
		for(x=0; x<width; x+=8){
			const int index= 8 + 8*stride + x + y*stride;
			int temp;
			STORE(0);
			STORE(1);
			STORE(2);
			STORE(3);
			STORE(4);
			STORE(5);
			STORE(6);
			STORE(7);
		}
	}
#if 0
	for(y=0; y<height; y++){
		for(x=0; x<width; x++){
			if((((x>>6) ^ (y>>6)) & 1) == 0)
				dst[x + y*dst_stride]= p->src[8 + 8*stride  + x + y*stride];
			if((x&63) == 0 || (y&63)==0)
				dst[x + y*dst_stride] += 128;
                }
	}
#endif
	//FIXME reorder for better caching
}

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){

	vf->priv->temp_stride= (width+16+15)&(~15);
        vf->priv->temp= malloc(vf->priv->temp_stride*(height+16)*sizeof(int16_t));
        vf->priv->src = malloc(vf->priv->temp_stride*(height+16)*sizeof(uint8_t));
        
	return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static void get_image(struct vf_instance_s* vf, mp_image_t *mpi){
    if(mpi->flags&MP_IMGFLAG_PRESERVE) return; // don't change
    if(mpi->imgfmt!=vf->priv->outfmt) return; // colorspace differ
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

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi){
	mp_image_t *dmpi;

	if(!(mpi->flags&MP_IMGFLAG_DIRECT)){
		// no DR, so get a new image! hope we'll get DR buffer:
		vf->dmpi=vf_get_image(vf->next,vf->priv->outfmt,
		MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
		mpi->w,mpi->h);
	}

	dmpi= vf->dmpi;
        
        vf->priv->mpeg2= mpi->qscale_type;

	filter(vf->priv, dmpi->planes[0], mpi->planes[0], dmpi->stride[0], mpi->stride[0], mpi->w, mpi->h, mpi->qscale, mpi->qstride, 1);
	filter(vf->priv, dmpi->planes[1], mpi->planes[1], dmpi->stride[1], mpi->stride[1], mpi->w>>1, mpi->h>>1, mpi->qscale, mpi->qstride, 0);
	filter(vf->priv, dmpi->planes[2], mpi->planes[2], dmpi->stride[2], mpi->stride[2], mpi->w>>1, mpi->h>>1, mpi->qscale, mpi->qstride, 0);

        vf_clone_mpi_attributes(dmpi, mpi);

#ifdef HAVE_MMX
	if(gCpuCaps.hasMMX) asm volatile ("emms\n\t");
#endif
#ifdef HAVE_MMX2
	if(gCpuCaps.hasMMX2) asm volatile ("sfence\n\t");
#endif

	return vf_next_put_image(vf,dmpi);
}

static void uninit(struct vf_instance_s* vf){
	if(!vf->priv) return;

	if(vf->priv->temp) free(vf->priv->temp);
	vf->priv->temp= NULL;
	if(vf->priv->src) free(vf->priv->src);
	vf->priv->src= NULL;
        if(vf->priv->avctx) free(vf->priv->avctx);
        vf->priv->avctx= NULL;
	
	free(vf->priv);
	vf->priv=NULL;
}

//===========================================================================//

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
	switch(fmt)
	{
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
		return vf_next_query_format(vf,vf->priv->outfmt);
	}
	return 0;
}

static unsigned int fmt_list[]={
    IMGFMT_YV12,
    IMGFMT_I420,
    IMGFMT_IYUV,
    0
};

static int open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->put_image=put_image;
    vf->get_image=get_image;
    vf->query_format=query_format;
    vf->uninit=uninit;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    memset(vf->priv, 0, sizeof(struct vf_priv_s));
    
    avcodec_init();

    vf->priv->avctx= avcodec_alloc_context();
    dsputil_init(&vf->priv->dsp, vf->priv->avctx);

    vf->priv->log2_count= 6;
    
    if (args) sscanf(args, "%d:%d", &vf->priv->log2_count, &vf->priv->qp);
	
    // check csp:
    vf->priv->outfmt=vf_match_csp(&vf->next,fmt_list,IMGFMT_YV12);
    if(!vf->priv->outfmt)
    {
	uninit(vf);
        return 0; // no csp match :(
    }
    
    return 1;
}

vf_info_t vf_info_spp = {
    "simple postprocess",
    "spp",
    "Michael Niedermayer",
    "",
    open,
    NULL
};

//===========================================================================//
