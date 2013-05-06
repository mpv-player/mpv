/*
 * Copyright (C) 2006 Michael Niedermayer <michaelni@gmx.at>
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

#include "config.h"
#include "core/cpudetect.h"
#include "core/options.h"
#include "core/m_struct.h"

#include "core/mp_msg.h"
#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"
#include "video/memcpy_pic.h"
#include "libavutil/common.h"

//===========================================================================//

struct vf_priv_s {
    int mode;
    int parity;
    int buffered_i;
    int buffered_tff;
    double buffered_pts;
    double buffered_pts_delta;
    mp_image_t *buffered_mpi;
    int stride[3];
    uint8_t *ref[4][3];
    int do_deinterlace;
};

static const struct vf_priv_s vf_priv_default = {
    .do_deinterlace = 1,
};

static void (*filter_line)(struct vf_priv_s *p, uint8_t *dst, uint8_t *prev, uint8_t *cur, uint8_t *next, int w, int refs, int parity);

static void store_ref(struct vf_priv_s *p, uint8_t *src[3], int src_stride[3], int width, int height){
    int i;

    memcpy (p->ref[3], p->ref[0], sizeof(uint8_t *)*3);
    memmove(p->ref[0], p->ref[1], sizeof(uint8_t *)*3*3);

    for(i=0; i<3; i++){
        int is_chroma= !!i;
        int pn_width  = width >>is_chroma;
        int pn_height = height>>is_chroma;


        memcpy_pic(p->ref[2][i], src[i], pn_width, pn_height, p->stride[i], src_stride[i]);

        memcpy(p->ref[2][i] +  pn_height   * p->stride[i],
                          src[i] + (pn_height-1)*src_stride[i], pn_width);
        memcpy(p->ref[2][i] + (pn_height+1)* p->stride[i],
                          src[i] + (pn_height-1)*src_stride[i], pn_width);

        memcpy(p->ref[2][i] -   p->stride[i], src[i], pn_width);
        memcpy(p->ref[2][i] - 2*p->stride[i], src[i], pn_width);
    }
}

#if HAVE_MMX

#define LOAD4(mem,dst) \
            "movd      "mem", "#dst" \n\t"\
            "punpcklbw %%mm7, "#dst" \n\t"

#define PABS(tmp,dst) \
            "pxor     "#tmp", "#tmp" \n\t"\
            "psubw    "#dst", "#tmp" \n\t"\
            "pmaxsw   "#tmp", "#dst" \n\t"

#define CHECK(pj,mj) \
            "movq "#pj"(%[cur],%[mrefs]), %%mm2 \n\t" /* cur[x-refs-1+j] */\
            "movq "#mj"(%[cur],%[prefs]), %%mm3 \n\t" /* cur[x+refs-1-j] */\
            "movq      %%mm2, %%mm4 \n\t"\
            "movq      %%mm2, %%mm5 \n\t"\
            "pxor      %%mm3, %%mm4 \n\t"\
            "pavgb     %%mm3, %%mm5 \n\t"\
            "pand     %[pb1], %%mm4 \n\t"\
            "psubusb   %%mm4, %%mm5 \n\t"\
            "psrlq     $8,    %%mm5 \n\t"\
            "punpcklbw %%mm7, %%mm5 \n\t" /* (cur[x-refs+j] + cur[x+refs-j])>>1 */\
            "movq      %%mm2, %%mm4 \n\t"\
            "psubusb   %%mm3, %%mm2 \n\t"\
            "psubusb   %%mm4, %%mm3 \n\t"\
            "pmaxub    %%mm3, %%mm2 \n\t"\
            "movq      %%mm2, %%mm3 \n\t"\
            "movq      %%mm2, %%mm4 \n\t" /* ABS(cur[x-refs-1+j] - cur[x+refs-1-j]) */\
            "psrlq      $8,   %%mm3 \n\t" /* ABS(cur[x-refs  +j] - cur[x+refs  -j]) */\
            "psrlq     $16,   %%mm4 \n\t" /* ABS(cur[x-refs+1+j] - cur[x+refs+1-j]) */\
            "punpcklbw %%mm7, %%mm2 \n\t"\
            "punpcklbw %%mm7, %%mm3 \n\t"\
            "punpcklbw %%mm7, %%mm4 \n\t"\
            "paddw     %%mm3, %%mm2 \n\t"\
            "paddw     %%mm4, %%mm2 \n\t" /* score */

#define CHECK1 \
            "movq      %%mm0, %%mm3 \n\t"\
            "pcmpgtw   %%mm2, %%mm3 \n\t" /* if(score < spatial_score) */\
            "pminsw    %%mm2, %%mm0 \n\t" /* spatial_score= score; */\
            "movq      %%mm3, %%mm6 \n\t"\
            "pand      %%mm3, %%mm5 \n\t"\
            "pandn     %%mm1, %%mm3 \n\t"\
            "por       %%mm5, %%mm3 \n\t"\
            "movq      %%mm3, %%mm1 \n\t" /* spatial_pred= (cur[x-refs+j] + cur[x+refs-j])>>1; */

#define CHECK2 /* pretend not to have checked dir=2 if dir=1 was bad.\
                  hurts both quality and speed, but matches the C version. */\
            "paddw    %[pw1], %%mm6 \n\t"\
            "psllw     $14,   %%mm6 \n\t"\
            "paddsw    %%mm6, %%mm2 \n\t"\
            "movq      %%mm0, %%mm3 \n\t"\
            "pcmpgtw   %%mm2, %%mm3 \n\t"\
            "pminsw    %%mm2, %%mm0 \n\t"\
            "pand      %%mm3, %%mm5 \n\t"\
            "pandn     %%mm1, %%mm3 \n\t"\
            "por       %%mm5, %%mm3 \n\t"\
            "movq      %%mm3, %%mm1 \n\t"

static void filter_line_mmx2(struct vf_priv_s *p, uint8_t *dst, uint8_t *prev, uint8_t *cur, uint8_t *next, int w, int refs, int parity){
    static const uint64_t pw_1 = 0x0001000100010001ULL;
    static const uint64_t pb_1 = 0x0101010101010101ULL;
    const int mode = p->mode;
    uint64_t tmp0, tmp1, tmp2, tmp3;
    int x;

#define FILTER\
    for(x=0; x<w; x+=4){\
        __asm__ volatile(\
            "pxor      %%mm7, %%mm7 \n\t"\
            LOAD4("(%[cur],%[mrefs])", %%mm0) /* c = cur[x-refs] */\
            LOAD4("(%[cur],%[prefs])", %%mm1) /* e = cur[x+refs] */\
            LOAD4("(%["prev2"])", %%mm2) /* prev2[x] */\
            LOAD4("(%["next2"])", %%mm3) /* next2[x] */\
            "movq      %%mm3, %%mm4 \n\t"\
            "paddw     %%mm2, %%mm3 \n\t"\
            "psraw     $1,    %%mm3 \n\t" /* d = (prev2[x] + next2[x])>>1 */\
            "movq      %%mm0, %[tmp0] \n\t" /* c */\
            "movq      %%mm3, %[tmp1] \n\t" /* d */\
            "movq      %%mm1, %[tmp2] \n\t" /* e */\
            "psubw     %%mm4, %%mm2 \n\t"\
            PABS(      %%mm4, %%mm2) /* temporal_diff0 */\
            LOAD4("(%[prev],%[mrefs])", %%mm3) /* prev[x-refs] */\
            LOAD4("(%[prev],%[prefs])", %%mm4) /* prev[x+refs] */\
            "psubw     %%mm0, %%mm3 \n\t"\
            "psubw     %%mm1, %%mm4 \n\t"\
            PABS(      %%mm5, %%mm3)\
            PABS(      %%mm5, %%mm4)\
            "paddw     %%mm4, %%mm3 \n\t" /* temporal_diff1 */\
            "psrlw     $1,    %%mm2 \n\t"\
            "psrlw     $1,    %%mm3 \n\t"\
            "pmaxsw    %%mm3, %%mm2 \n\t"\
            LOAD4("(%[next],%[mrefs])", %%mm3) /* next[x-refs] */\
            LOAD4("(%[next],%[prefs])", %%mm4) /* next[x+refs] */\
            "psubw     %%mm0, %%mm3 \n\t"\
            "psubw     %%mm1, %%mm4 \n\t"\
            PABS(      %%mm5, %%mm3)\
            PABS(      %%mm5, %%mm4)\
            "paddw     %%mm4, %%mm3 \n\t" /* temporal_diff2 */\
            "psrlw     $1,    %%mm3 \n\t"\
            "pmaxsw    %%mm3, %%mm2 \n\t"\
            "movq      %%mm2, %[tmp3] \n\t" /* diff */\
\
            "paddw     %%mm0, %%mm1 \n\t"\
            "paddw     %%mm0, %%mm0 \n\t"\
            "psubw     %%mm1, %%mm0 \n\t"\
            "psrlw     $1,    %%mm1 \n\t" /* spatial_pred */\
            PABS(      %%mm2, %%mm0)      /* ABS(c-e) */\
\
            "movq -1(%[cur],%[mrefs]), %%mm2 \n\t" /* cur[x-refs-1] */\
            "movq -1(%[cur],%[prefs]), %%mm3 \n\t" /* cur[x+refs-1] */\
            "movq      %%mm2, %%mm4 \n\t"\
            "psubusb   %%mm3, %%mm2 \n\t"\
            "psubusb   %%mm4, %%mm3 \n\t"\
            "pmaxub    %%mm3, %%mm2 \n\t"\
            "pshufw $9,%%mm2, %%mm3 \n\t"\
            "punpcklbw %%mm7, %%mm2 \n\t" /* ABS(cur[x-refs-1] - cur[x+refs-1]) */\
            "punpcklbw %%mm7, %%mm3 \n\t" /* ABS(cur[x-refs+1] - cur[x+refs+1]) */\
            "paddw     %%mm2, %%mm0 \n\t"\
            "paddw     %%mm3, %%mm0 \n\t"\
            "psubw    %[pw1], %%mm0 \n\t" /* spatial_score */\
\
            CHECK(-2,0)\
            CHECK1\
            CHECK(-3,1)\
            CHECK2\
            CHECK(0,-2)\
            CHECK1\
            CHECK(1,-3)\
            CHECK2\
\
            /* if(p->mode<2) ... */\
            "movq    %[tmp3], %%mm6 \n\t" /* diff */\
            "cmpl      $2, %[mode] \n\t"\
            "jge       1f \n\t"\
            LOAD4("(%["prev2"],%[mrefs],2)", %%mm2) /* prev2[x-2*refs] */\
            LOAD4("(%["next2"],%[mrefs],2)", %%mm4) /* next2[x-2*refs] */\
            LOAD4("(%["prev2"],%[prefs],2)", %%mm3) /* prev2[x+2*refs] */\
            LOAD4("(%["next2"],%[prefs],2)", %%mm5) /* next2[x+2*refs] */\
            "paddw     %%mm4, %%mm2 \n\t"\
            "paddw     %%mm5, %%mm3 \n\t"\
            "psrlw     $1,    %%mm2 \n\t" /* b */\
            "psrlw     $1,    %%mm3 \n\t" /* f */\
            "movq    %[tmp0], %%mm4 \n\t" /* c */\
            "movq    %[tmp1], %%mm5 \n\t" /* d */\
            "movq    %[tmp2], %%mm7 \n\t" /* e */\
            "psubw     %%mm4, %%mm2 \n\t" /* b-c */\
            "psubw     %%mm7, %%mm3 \n\t" /* f-e */\
            "movq      %%mm5, %%mm0 \n\t"\
            "psubw     %%mm4, %%mm5 \n\t" /* d-c */\
            "psubw     %%mm7, %%mm0 \n\t" /* d-e */\
            "movq      %%mm2, %%mm4 \n\t"\
            "pminsw    %%mm3, %%mm2 \n\t"\
            "pmaxsw    %%mm4, %%mm3 \n\t"\
            "pmaxsw    %%mm5, %%mm2 \n\t"\
            "pminsw    %%mm5, %%mm3 \n\t"\
            "pmaxsw    %%mm0, %%mm2 \n\t" /* max */\
            "pminsw    %%mm0, %%mm3 \n\t" /* min */\
            "pxor      %%mm4, %%mm4 \n\t"\
            "pmaxsw    %%mm3, %%mm6 \n\t"\
            "psubw     %%mm2, %%mm4 \n\t" /* -max */\
            "pmaxsw    %%mm4, %%mm6 \n\t" /* diff= MAX3(diff, min, -max); */\
            "1: \n\t"\
\
            "movq    %[tmp1], %%mm2 \n\t" /* d */\
            "movq      %%mm2, %%mm3 \n\t"\
            "psubw     %%mm6, %%mm2 \n\t" /* d-diff */\
            "paddw     %%mm6, %%mm3 \n\t" /* d+diff */\
            "pmaxsw    %%mm2, %%mm1 \n\t"\
            "pminsw    %%mm3, %%mm1 \n\t" /* d = clip(spatial_pred, d-diff, d+diff); */\
            "packuswb  %%mm1, %%mm1 \n\t"\
\
            :[tmp0]"=m"(tmp0),\
             [tmp1]"=m"(tmp1),\
             [tmp2]"=m"(tmp2),\
             [tmp3]"=m"(tmp3)\
            :[prev] "r"(prev),\
             [cur]  "r"(cur),\
             [next] "r"(next),\
             [prefs]"r"((x86_reg)refs),\
             [mrefs]"r"((x86_reg)-refs),\
             [pw1]  "m"(pw_1),\
             [pb1]  "m"(pb_1),\
             [mode] "g"(mode)\
        );\
        __asm__ volatile("movd %%mm1, %0" :"=m"(*dst));\
        dst += 4;\
        prev+= 4;\
        cur += 4;\
        next+= 4;\
    }

    if(parity){
#define prev2 "prev"
#define next2 "cur"
        FILTER
#undef prev2
#undef next2
    }else{
#define prev2 "cur"
#define next2 "next"
        FILTER
#undef prev2
#undef next2
    }
}
#undef LOAD4
#undef PABS
#undef CHECK
#undef CHECK1
#undef CHECK2
#undef FILTER

#endif /* HAVE_MMX */

static void filter_line_c(struct vf_priv_s *p, uint8_t *dst, uint8_t *prev, uint8_t *cur, uint8_t *next, int w, int refs, int parity){
    int x;
    uint8_t *prev2= parity ? prev : cur ;
    uint8_t *next2= parity ? cur  : next;
    for(x=0; x<w; x++){
        int c= cur[-refs];
        int d= (prev2[0] + next2[0])>>1;
        int e= cur[+refs];
        int temporal_diff0= FFABS(prev2[0] - next2[0]);
        int temporal_diff1=( FFABS(prev[-refs] - c) + FFABS(prev[+refs] - e) )>>1;
        int temporal_diff2=( FFABS(next[-refs] - c) + FFABS(next[+refs] - e) )>>1;
        int diff= FFMAX3(temporal_diff0>>1, temporal_diff1, temporal_diff2);
        int spatial_pred= (c+e)>>1;
        int spatial_score= FFABS(cur[-refs-1] - cur[+refs-1]) + FFABS(c-e)
                         + FFABS(cur[-refs+1] - cur[+refs+1]) - 1;

#define CHECK(j)\
    {   int score= FFABS(cur[-refs-1+j] - cur[+refs-1-j])\
                 + FFABS(cur[-refs  +j] - cur[+refs  -j])\
                 + FFABS(cur[-refs+1+j] - cur[+refs+1-j]);\
        if(score < spatial_score){\
            spatial_score= score;\
            spatial_pred= (cur[-refs  +j] + cur[+refs  -j])>>1;\

        CHECK(-1) CHECK(-2) }} }}
        CHECK( 1) CHECK( 2) }} }}

        if(p->mode<2){
            int b= (prev2[-2*refs] + next2[-2*refs])>>1;
            int f= (prev2[+2*refs] + next2[+2*refs])>>1;
#if 0
            int a= cur[-3*refs];
            int g= cur[+3*refs];
            int max= FFMAX3(d-e, d-c, FFMIN3(FFMAX(b-c,f-e),FFMAX(b-c,b-a),FFMAX(f-g,f-e)) );
            int min= FFMIN3(d-e, d-c, FFMAX3(FFMIN(b-c,f-e),FFMIN(b-c,b-a),FFMIN(f-g,f-e)) );
#else
            int max= FFMAX3(d-e, d-c, FFMIN(b-c, f-e));
            int min= FFMIN3(d-e, d-c, FFMAX(b-c, f-e));
#endif

            diff= FFMAX3(diff, min, -max);
        }

        if(spatial_pred > d + diff)
           spatial_pred = d + diff;
        else if(spatial_pred < d - diff)
           spatial_pred = d - diff;

        dst[0] = spatial_pred;

        dst++;
        cur++;
        prev++;
        next++;
        prev2++;
        next2++;
    }
}

static void filter(struct vf_priv_s *p, uint8_t *dst[3], int dst_stride[3], int width, int height, int parity, int tff){
    int y, i;

    for(i=0; i<3; i++){
        int is_chroma= !!i;
        int w= width >>is_chroma;
        int h= height>>is_chroma;
        int refs= p->stride[i];

        for(y=0; y<h; y++){
            if((y ^ parity) & 1){
                uint8_t *prev= &p->ref[0][i][y*refs];
                uint8_t *cur = &p->ref[1][i][y*refs];
                uint8_t *next= &p->ref[2][i][y*refs];
                uint8_t *dst2= &dst[i][y*dst_stride[i]];
                filter_line(p, dst2, prev, cur, next, w, refs, parity ^ tff);
            }else{
                memcpy(&dst[i][y*dst_stride[i]], &p->ref[1][i][y*refs], w);
            }
        }
    }
#if HAVE_MMX
    if(gCpuCaps.hasMMX2) __asm__ volatile("emms \n\t" : : : "memory");
#endif
}

static int config(struct vf_instance *vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
        int i, j;

        for(i=0; i<3; i++){
            int is_chroma= !!i;
            int w= ((width   + 31) & (~31))>>is_chroma;
            int h=(((height  +  1) & ( ~1))>>is_chroma) + 6;

            vf->priv->stride[i]= w;
            for(j=0; j<3; j++)
                vf->priv->ref[j][i]= (char *)malloc(w*h)+3*w;
        }

	return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static int continue_buffered_image(struct vf_instance *vf, struct mp_image *mpi);

static int filter_image(struct vf_instance *vf, struct mp_image *mpi)
{
    int tff;
    if(vf->priv->parity < 0) {
        if (mpi->fields & MP_IMGFIELD_ORDERED)
            tff = !!(mpi->fields & MP_IMGFIELD_TOP_FIRST);
        else
            tff = 1;
    }
    else tff = (vf->priv->parity&1)^1;

    store_ref(vf->priv, mpi->planes, mpi->stride, mpi->w, mpi->h);

    {
        double delta;
        if (vf->priv->buffered_pts == MP_NOPTS_VALUE)
            delta = 1001.0/60000.0; // delta = field time distance
        else
            delta = (mpi->pts - vf->priv->buffered_pts) / 2;
        if (delta <= 0.0 || delta >= 0.5)
            delta = 0.0;
        vf->priv->buffered_pts_delta = delta;
    }

    vf->priv->buffered_tff = tff;
    vf->priv->buffered_i = 0;
    vf->priv->buffered_pts = mpi->pts;

    if (vf->priv->do_deinterlace == 0) {
        vf_add_output_frame(vf, mpi);
        mpi = NULL;
    } else if (vf->priv->do_deinterlace == 1) {
        vf->priv->do_deinterlace = 2;
    } else {
        while (continue_buffered_image(vf, mpi)) {
        }
    }

    talloc_free(mpi);

    return 0;
}

static int continue_buffered_image(struct vf_instance *vf, struct mp_image *mpi)
{
    int tff = vf->priv->buffered_tff;
    double pts = vf->priv->buffered_pts;
    int i;
    int ret=0;

    pts += (vf->priv->buffered_i - 0.5 * (vf->priv->mode&1)) * vf->priv->buffered_pts_delta;

    for(i = vf->priv->buffered_i; i<=(vf->priv->mode&1); i++){
        struct mp_image *dmpi = vf_alloc_out_image(vf);
        mp_image_copy_attributes(dmpi, mpi);
        filter(vf->priv, dmpi->planes, dmpi->stride, mpi->w, mpi->h, i ^ tff ^ 1, tff);
        if (i < (vf->priv->mode & 1))
            ret = 1; // more images to come
        dmpi->pts = pts;
        vf_add_output_frame(vf, dmpi);
        break;
    }
    vf->priv->buffered_i = 1;
    return ret;
}

static void uninit(struct vf_instance *vf){
    int i;
    if(!vf->priv) return;

    for(i=0; i<3*3; i++){
        uint8_t **p= &vf->priv->ref[i%3][i/3];
        if(*p) free(*p - 3*vf->priv->stride[i/3]);
        *p= NULL;
    }
}

//===========================================================================//
static int query_format(struct vf_instance *vf, unsigned int fmt){
    switch(fmt){
	case IMGFMT_420P:
	    return vf_next_query_format(vf,fmt);
    }
    return 0;
}

static int control(struct vf_instance *vf, int request, void* data){
    switch (request){
      case VFCTRL_GET_DEINTERLACE:
        *(int*)data = vf->priv->do_deinterlace;
        return CONTROL_OK;
      case VFCTRL_SET_DEINTERLACE:
        vf->priv->do_deinterlace = 2*!!*(int*)data;
        return CONTROL_OK;
    }
    return vf_next_control (vf, request, data);
}

static int vf_open(vf_instance_t *vf, char *args){

    vf->config=config;
    vf->filter_ext=filter_image;
    vf->query_format=query_format;
    vf->uninit=uninit;
    vf->control=control;

    vf->priv->parity= -1;

    filter_line = filter_line_c;
#if HAVE_MMX
    if(gCpuCaps.hasMMX2) filter_line = filter_line_mmx2;
#endif

    return 1;
}

#undef ST_OFF
#define ST_OFF(f) M_ST_OFF(struct vf_priv_s,f)
static const m_option_t vf_opts_fields[] = {
  {"mode", ST_OFF(mode), CONF_TYPE_INT, M_OPT_RANGE, 0, 3},
  {"enabled", ST_OFF(do_deinterlace), CONF_TYPE_FLAG, 0, 0, 1},
  {0}
};

static const m_struct_t vf_opts = {
  "yadif",
  sizeof(struct vf_priv_s),
  &vf_priv_default,
  vf_opts_fields
};

const vf_info_t vf_info_yadif = {
    "Yet Another DeInterlacing Filter",
    "yadif",
    "Michael Niedermayer",
    "",
    vf_open,
    &vf_opts
};
