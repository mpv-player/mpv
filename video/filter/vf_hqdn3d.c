/*
 * Copyright (C) 2003 Daniel Moreno <comac@comac.darktech.org>
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

#include "mpvcore/mp_msg.h"
#include "options/m_option.h"
#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"

#include "vf_lavfi.h"

#define PARAM1_DEFAULT 4.0
#define PARAM2_DEFAULT 3.0
#define PARAM3_DEFAULT 6.0

//===========================================================================//

struct vf_priv_s {
        int Coefs[4][512*16];
        unsigned int *Line;
	unsigned short *Frame[3];
        double strength[4];
        struct vf_lw_opts *lw_opts;
};


/***************************************************************************/

static void uninit(struct vf_instance *vf)
{
	free(vf->priv->Line);
	free(vf->priv->Frame[0]);
	free(vf->priv->Frame[1]);
	free(vf->priv->Frame[2]);

	vf->priv->Line     = NULL;
	vf->priv->Frame[0] = NULL;
	vf->priv->Frame[1] = NULL;
	vf->priv->Frame[2] = NULL;
}

static int config(struct vf_instance *vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){

	uninit(vf);
        vf->priv->Line = malloc(width*sizeof(unsigned int));

	return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static inline unsigned int LowPassMul(unsigned int PrevMul, unsigned int CurrMul, int* Coef){
//    int dMul= (PrevMul&0xFFFFFF)-(CurrMul&0xFFFFFF);
    int dMul= PrevMul-CurrMul;
    unsigned int d=((dMul+0x10007FF)>>12);
    return CurrMul + Coef[d];
}

static void deNoiseTemporal(
                    unsigned char *Frame,        // mpi->planes[x]
                    unsigned char *FrameDest,    // dmpi->planes[x]
                    unsigned short *FrameAnt,
                    int W, int H, int sStride, int dStride,
                    int *Temporal)
{
    long X, Y;
    unsigned int PixelDst;

    for (Y = 0; Y < H; Y++){
        for (X = 0; X < W; X++){
            PixelDst = LowPassMul(FrameAnt[X]<<8, Frame[X]<<16, Temporal);
            FrameAnt[X] = ((PixelDst+0x1000007F)>>8);
            FrameDest[X]= ((PixelDst+0x10007FFF)>>16);
        }
        Frame += sStride;
        FrameDest += dStride;
        FrameAnt += W;
    }
}

static void deNoiseSpacial(
                    unsigned char *Frame,        // mpi->planes[x]
                    unsigned char *FrameDest,    // dmpi->planes[x]
                    unsigned int *LineAnt,       // vf->priv->Line (width bytes)
                    int W, int H, int sStride, int dStride,
                    int *Horizontal, int *Vertical)
{
    long X, Y;
    long sLineOffs = 0, dLineOffs = 0;
    unsigned int PixelAnt;
    unsigned int PixelDst;

    /* First pixel has no left nor top neighbor. */
    PixelDst = LineAnt[0] = PixelAnt = Frame[0]<<16;
    FrameDest[0]= ((PixelDst+0x10007FFF)>>16);

    /* First line has no top neighbor, only left. */
    for (X = 1; X < W; X++){
        PixelDst = LineAnt[X] = LowPassMul(PixelAnt, Frame[X]<<16, Horizontal);
        FrameDest[X]= ((PixelDst+0x10007FFF)>>16);
    }

    for (Y = 1; Y < H; Y++){
	sLineOffs += sStride, dLineOffs += dStride;
        /* First pixel on each line doesn't have previous pixel */
        PixelAnt = Frame[sLineOffs]<<16;
        PixelDst = LineAnt[0] = LowPassMul(LineAnt[0], PixelAnt, Vertical);
        FrameDest[dLineOffs]= ((PixelDst+0x10007FFF)>>16);

        for (X = 1; X < W; X++){
            /* The rest are normal */
            PixelAnt = LowPassMul(PixelAnt, Frame[sLineOffs+X]<<16, Horizontal);
            PixelDst = LineAnt[X] = LowPassMul(LineAnt[X], PixelAnt, Vertical);
            FrameDest[dLineOffs+X]= ((PixelDst+0x10007FFF)>>16);
        }
    }
}

static void deNoise(unsigned char *Frame,        // mpi->planes[x]
                    unsigned char *FrameDest,    // dmpi->planes[x]
                    unsigned int *LineAnt,      // vf->priv->Line (width bytes)
		    unsigned short **FrameAntPtr,
                    int W, int H, int sStride, int dStride,
                    int *Horizontal, int *Vertical, int *Temporal)
{
    long X, Y;
    long sLineOffs = 0, dLineOffs = 0;
    unsigned int PixelAnt;
    unsigned int PixelDst;
    unsigned short* FrameAnt=(*FrameAntPtr);

    if(!FrameAnt){
	(*FrameAntPtr)=FrameAnt=malloc(W*H*sizeof(unsigned short));
	for (Y = 0; Y < H; Y++){
	    unsigned short* dst=&FrameAnt[Y*W];
	    unsigned char* src=Frame+Y*sStride;
	    for (X = 0; X < W; X++) dst[X]=src[X]<<8;
	}
    }

    if(!Horizontal[0] && !Vertical[0]){
        deNoiseTemporal(Frame, FrameDest, FrameAnt,
                        W, H, sStride, dStride, Temporal);
        return;
    }
    if(!Temporal[0]){
        deNoiseSpacial(Frame, FrameDest, LineAnt,
                       W, H, sStride, dStride, Horizontal, Vertical);
        return;
    }

    /* First pixel has no left nor top neighbor. Only previous frame */
    LineAnt[0] = PixelAnt = Frame[0]<<16;
    PixelDst = LowPassMul(FrameAnt[0]<<8, PixelAnt, Temporal);
    FrameAnt[0] = ((PixelDst+0x1000007F)>>8);
    FrameDest[0]= ((PixelDst+0x10007FFF)>>16);

    /* First line has no top neighbor. Only left one for each pixel and
     * last frame */
    for (X = 1; X < W; X++){
        LineAnt[X] = PixelAnt = LowPassMul(PixelAnt, Frame[X]<<16, Horizontal);
        PixelDst = LowPassMul(FrameAnt[X]<<8, PixelAnt, Temporal);
        FrameAnt[X] = ((PixelDst+0x1000007F)>>8);
        FrameDest[X]= ((PixelDst+0x10007FFF)>>16);
    }

    for (Y = 1; Y < H; Y++){
	unsigned short* LinePrev=&FrameAnt[Y*W];
	sLineOffs += sStride, dLineOffs += dStride;
        /* First pixel on each line doesn't have previous pixel */
        PixelAnt = Frame[sLineOffs]<<16;
        LineAnt[0] = LowPassMul(LineAnt[0], PixelAnt, Vertical);
	PixelDst = LowPassMul(LinePrev[0]<<8, LineAnt[0], Temporal);
        LinePrev[0] = ((PixelDst+0x1000007F)>>8);
        FrameDest[dLineOffs]= ((PixelDst+0x10007FFF)>>16);

        for (X = 1; X < W; X++){
            /* The rest are normal */
            PixelAnt = LowPassMul(PixelAnt, Frame[sLineOffs+X]<<16, Horizontal);
            LineAnt[X] = LowPassMul(LineAnt[X], PixelAnt, Vertical);
	    PixelDst = LowPassMul(LinePrev[X]<<8, LineAnt[X], Temporal);
            LinePrev[X] = ((PixelDst+0x1000007F)>>8);
            FrameDest[dLineOffs+X]= ((PixelDst+0x10007FFF)>>16);
        }
    }
}


static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
	int cw= mpi->w >> mpi->chroma_x_shift;
	int ch= mpi->h >> mpi->chroma_y_shift;
        int W = mpi->w, H = mpi->h;

        struct mp_image *dmpi = vf_alloc_out_image(vf);
        mp_image_copy_attributes(dmpi, mpi);

        deNoise(mpi->planes[0], dmpi->planes[0],
		vf->priv->Line, &vf->priv->Frame[0], W, H,
                mpi->stride[0], dmpi->stride[0],
                vf->priv->Coefs[0],
                vf->priv->Coefs[0],
                vf->priv->Coefs[1]);
        deNoise(mpi->planes[1], dmpi->planes[1],
		vf->priv->Line, &vf->priv->Frame[1], cw, ch,
                mpi->stride[1], dmpi->stride[1],
                vf->priv->Coefs[2],
                vf->priv->Coefs[2],
                vf->priv->Coefs[3]);
        deNoise(mpi->planes[2], dmpi->planes[2],
		vf->priv->Line, &vf->priv->Frame[2], cw, ch,
                mpi->stride[2], dmpi->stride[2],
                vf->priv->Coefs[2],
                vf->priv->Coefs[2],
                vf->priv->Coefs[3]);

        talloc_free(mpi);
        return dmpi;
}

//===========================================================================//

static int query_format(struct vf_instance *vf, unsigned int fmt){
        switch(fmt)
	{
        case IMGFMT_444P:
        case IMGFMT_422P:
        case IMGFMT_440P:
        case IMGFMT_420P:
        case IMGFMT_411P:
        case IMGFMT_410P:
		return vf_next_query_format(vf, fmt);
	}
	return 0;
}


#define ABS(A) ( (A) > 0 ? (A) : -(A) )

static void PrecalcCoefs(int *Ct, double Dist25)
{
    int i;
    double Gamma, Simil, C;

    Gamma = log(0.25) / log(1.0 - Dist25/255.0 - 0.00001);

    for (i = -255*16; i <= 255*16; i++)
    {
        Simil = 1.0 - ABS(i) / (16*255.0);
        C = pow(Simil, Gamma) * 65536.0 * (double)i / 16.0;
        Ct[16*256+i] = (C<0) ? (C-0.5) : (C+0.5);
    }

    Ct[0] = (Dist25 != 0);
}

#define LUMA_SPATIAL   0
#define LUMA_TMP       1
#define CHROMA_SPATIAL 2
#define CHROMA_TMP     3

static int vf_open(vf_instance_t *vf){
        struct vf_priv_s *s = vf->priv;

	vf->config=config;
	vf->filter=filter;
        vf->query_format=query_format;
        vf->uninit=uninit;

        if (vf_lw_set_graph(vf, s->lw_opts, "hqdn3d", "%f:%f:%f:%f",
                            s->strength[0], s->strength[1],
                            s->strength[2], s->strength[3]) >= 0)
        {
            return 1;
        }

        if (!s->strength[LUMA_SPATIAL])
            s->strength[LUMA_SPATIAL] = PARAM1_DEFAULT;
        if (!s->strength[CHROMA_SPATIAL])
            s->strength[CHROMA_SPATIAL] = PARAM2_DEFAULT * s->strength[LUMA_SPATIAL] / PARAM1_DEFAULT;
        if (!s->strength[LUMA_TMP])
            s->strength[LUMA_TMP]   = PARAM3_DEFAULT * s->strength[LUMA_SPATIAL] / PARAM1_DEFAULT;
        if (!s->strength[CHROMA_TMP])
            s->strength[CHROMA_TMP] = s->strength[LUMA_TMP] * s->strength[CHROMA_SPATIAL] / s->strength[LUMA_SPATIAL];

        for (int n = 0; n < 4; n++)
            PrecalcCoefs(vf->priv->Coefs[n], s->strength[n]);

	return 1;
}

#define OPT_BASE_STRUCT struct vf_priv_s
const vf_info_t vf_info_hqdn3d = {
    .description = "High Quality 3D Denoiser",
    .name = "hqdn3d",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .options = (const struct m_option[]){
        OPT_DOUBLE("luma_spatial", strength[0], 0),
        OPT_DOUBLE("chroma_spatial", strength[1], 0),
        OPT_DOUBLE("luma_tmp", strength[2], 0),
        OPT_DOUBLE("chroma_tmp", strength[3], 0),
        OPT_SUBSTRUCT("", lw_opts, vf_lw_conf, 0),
        {0}
    },
};

//===========================================================================//
