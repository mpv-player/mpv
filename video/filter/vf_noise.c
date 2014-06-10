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
#include <math.h>

#include <libavutil/mem.h>

#include "config.h"
#include "common/msg.h"
#include "options/m_option.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"
#include "video/memcpy_pic.h"

#include "vf_lavfi.h"

#define MAX_NOISE 4096
#define MAX_SHIFT 1024
#define MAX_RES (MAX_NOISE-MAX_SHIFT)

//===========================================================================//

static inline void lineNoise(uint8_t *dst, uint8_t *src, int8_t *noise, int len, int shift);
static inline void lineNoiseAvg(uint8_t *dst, uint8_t *src, int len, int8_t **shift);

typedef struct FilterParam{
        int strength;
        int uniform;
        int temporal;
        int quality;
        int averaged;
        int pattern;
        int shiftptr;
        int8_t *noise;
        int8_t *prev_shift[MAX_RES][3];
        int nonTempRandShift[MAX_RES];
}FilterParam;

struct vf_priv_s {
        FilterParam lumaParam;
        FilterParam chromaParam;
        int strength;
        int averaged;
        int pattern;
        int temporal;
        int uniform;
        int hq;
        struct vf_lw_opts *lw_opts;
};

static const int patt[4] = {
    -1,0,1,0
};

#define RAND_N(range) ((int) ((double)range*rand()/(RAND_MAX+1.0)))
static int8_t *initNoise(FilterParam *fp){
        int strength= fp->strength;
        int uniform= fp->uniform;
        int averaged= fp->averaged;
        int pattern= fp->pattern;
        int8_t *noise= av_malloc(MAX_NOISE*sizeof(int8_t));
        int i, j;

        srand(123457);

        for(i=0,j=0; i<MAX_NOISE; i++,j++)
        {
                if(uniform) {
                        if (averaged) {
                                if (pattern) {
                                        noise[i]= (RAND_N(strength) - strength/2)/6
                                                +patt[j%4]*strength*0.25/3;
                                } else {
                                        noise[i]= (RAND_N(strength) - strength/2)/3;
                                }
                        } else {
                                if (pattern) {
                                    noise[i]= (RAND_N(strength) - strength/2)/2
                                            + patt[j%4]*strength*0.25;
                                } else {
                                        noise[i]= RAND_N(strength) - strength/2;
                                }
                        }
                } else {
                        double x1, x2, w, y1;
                        do {
                                x1 = 2.0 * rand()/(float)RAND_MAX - 1.0;
                                x2 = 2.0 * rand()/(float)RAND_MAX - 1.0;
                                w = x1 * x1 + x2 * x2;
                        } while ( w >= 1.0 );

                        w = sqrt( (-2.0 * log( w ) ) / w );
                        y1= x1 * w;
                        y1*= strength / sqrt(3.0);
                        if (pattern) {
                            y1 /= 2;
                            y1 += patt[j%4]*strength*0.35;
                        }
                        if     (y1<-128) y1=-128;
                        else if(y1> 127) y1= 127;
                        if (averaged) y1 /= 3.0;
                        noise[i]= (int)y1;
                }
                if (RAND_N(6) == 0) j--;
        }


        for (i = 0; i < MAX_RES; i++)
            for (j = 0; j < 3; j++)
                fp->prev_shift[i][j] = noise + (rand()&(MAX_SHIFT-1));

        for(i=0; i<MAX_RES; i++){
                fp->nonTempRandShift[i]= rand()&(MAX_SHIFT-1);
        }

        fp->noise= noise;
        fp->shiftptr= 0;
        return noise;
}

/***************************************************************************/

static inline void lineNoise(uint8_t *dst, uint8_t *src, int8_t *noise, int len, int shift){
        int i;
        noise+= shift;
        for(i=0; i<len; i++)
        {
                int v= src[i]+ noise[i];
                if(v>255)       dst[i]=255; //FIXME optimize
                else if(v<0)    dst[i]=0;
                else            dst[i]=v;
        }
}

/***************************************************************************/

static inline void lineNoiseAvg(uint8_t *dst, uint8_t *src, int len, int8_t **shift){
        int i;
        int8_t *src2= (int8_t*)src;

        for(i=0; i<len; i++)
        {
            const int n= shift[0][i] + shift[1][i] + shift[2][i];
            dst[i]= src2[i]+((n*src2[i])>>7);
        }
}

/***************************************************************************/

static void donoise(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, int width, int height, FilterParam *fp){
        int8_t *noise= fp->noise;
        int y;
        int shift=0;

        if(!noise)
        {
                if(src==dst) return;

                if(dstStride==srcStride) memcpy(dst, src, srcStride*height);
                else
                {
                        for(y=0; y<height; y++)
                        {
                                memcpy(dst, src, width);
                                dst+= dstStride;
                                src+= srcStride;
                        }
                }
                return;
        }

        for(y=0; y<height; y++)
        {
                if(fp->temporal)        shift=  rand()&(MAX_SHIFT  -1);
                else                    shift= fp->nonTempRandShift[y];

                if(fp->quality==0) shift&= ~7;
                if (fp->averaged) {
                    lineNoiseAvg(dst, src, width, fp->prev_shift[y]);
                    fp->prev_shift[y][fp->shiftptr] = noise + shift;
                } else {
                    lineNoise(dst, src, noise, width, shift);
                }
                dst+= dstStride;
                src+= srcStride;
        }
        fp->shiftptr++;
        if (fp->shiftptr == 3) fp->shiftptr = 0;
}

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
        struct mp_image *dmpi = mpi;
        if (!mp_image_is_writeable(mpi)) {
            dmpi = vf_alloc_out_image(vf);
            mp_image_copy_attributes(dmpi, mpi);
        }

        donoise(dmpi->planes[0], mpi->planes[0], dmpi->stride[0], mpi->stride[0], mpi->w, mpi->h, &vf->priv->lumaParam);
        donoise(dmpi->planes[1], mpi->planes[1], dmpi->stride[1], mpi->stride[1], mpi->w/2, mpi->h/2, &vf->priv->chromaParam);
        donoise(dmpi->planes[2], mpi->planes[2], dmpi->stride[2], mpi->stride[2], mpi->w/2, mpi->h/2, &vf->priv->chromaParam);

        if (dmpi != mpi)
            talloc_free(mpi);
        return dmpi;
}

static void uninit(struct vf_instance *vf){
        if(!vf->priv) return;

        av_free(vf->priv->chromaParam.noise);
        vf->priv->chromaParam.noise= NULL;

        av_free(vf->priv->lumaParam.noise);
        vf->priv->lumaParam.noise= NULL;
}

//===========================================================================//

static int query_format(struct vf_instance *vf, unsigned int fmt){
        switch(fmt)
        {
        case IMGFMT_420P:
                return vf_next_query_format(vf,IMGFMT_420P);
        }
        return 0;
}

static void parse(FilterParam *fp, struct vf_priv_s *p){
        fp->strength= p->strength;
        fp->uniform=p->uniform;
        fp->temporal=p->temporal;
        fp->quality=p->hq;
        fp->pattern=p->pattern;
        fp->averaged=p->averaged;

        if(fp->strength) initNoise(fp);
}

static int vf_open(vf_instance_t *vf){
    vf->filter=filter;
    vf->query_format=query_format;
    vf->uninit=uninit;

#define CH(f) ((f) ? '+' : '-')
    struct vf_priv_s *p = vf->priv;
    if (vf_lw_set_graph(vf, p->lw_opts, "noise", "-1:%d:%ca%cp%ct%cu",
                        p->strength, CH(p->averaged), CH(p->pattern),
                        CH(p->temporal), CH(p->uniform)) >= 0)
    {
        return 1;
    }

    parse(&vf->priv->lumaParam, vf->priv);
    parse(&vf->priv->chromaParam, vf->priv);

    return 1;
}

#define OPT_BASE_STRUCT struct vf_priv_s
const vf_info_t vf_info_noise = {
    .description = "noise generator",
    .name = "noise",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .options = (const struct m_option[]){
        OPT_INTRANGE("strength", strength, 0, 0, 100, OPTDEF_INT(2)),
        OPT_FLAG("averaged", averaged, 0),
        OPT_FLAG("pattern", pattern, 0),
        OPT_FLAG("temporal", temporal, 0),
        OPT_FLAG("uniform", uniform, 0),
        OPT_FLAG("hq", hq, 0),
        OPT_SUBSTRUCT("", lw_opts, vf_lw_conf, 0),
        {0}
    },
};

//===========================================================================//
