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
#include "mp_msg.h"
#include "cpudetect.h"

#include "img_format.h"
#include "mp_image.h"
#include "vd.h"
#include "vf.h"
#include "fmt-conversion.h"
#include "mpbswap.h"

#include "libswscale/swscale.h"
#include "vf_scale.h"

#include "m_option.h"
#include "m_struct.h"

static struct vf_priv_s {
    int w,h;
    int v_chr_drop;
    double param[2];
    unsigned int fmt;
    struct SwsContext *ctx;
    struct SwsContext *ctx2; //for interlaced slices only
    unsigned char* palette;
    int interlaced;
    int noup;
    int accurate_rnd;
} const vf_priv_dflt = {
  -1,-1,
  0,
  {SWS_PARAM_DEFAULT, SWS_PARAM_DEFAULT},
  0,
  NULL,
  NULL,
  NULL
};

//===========================================================================//

void sws_getFlagsAndFilterFromCmdLine(int *flags, SwsFilter **srcFilterParam, SwsFilter **dstFilterParam);

static const unsigned int outfmt_list[]={
// YUV:
    IMGFMT_444P,
    IMGFMT_444P16_LE,
    IMGFMT_444P16_BE,
    IMGFMT_422P,
    IMGFMT_422P16_LE,
    IMGFMT_422P16_BE,
    IMGFMT_YV12,
    IMGFMT_I420,
    IMGFMT_420P16_LE,
    IMGFMT_420P16_BE,
    IMGFMT_420A,
    IMGFMT_IYUV,
    IMGFMT_YVU9,
    IMGFMT_IF09,
    IMGFMT_411P,
    IMGFMT_NV12,
    IMGFMT_NV21,
    IMGFMT_YUY2,
    IMGFMT_UYVY,
    IMGFMT_440P,
// RGB and grayscale (Y8 and Y800):
    IMGFMT_BGR32,
    IMGFMT_RGB32,
    IMGFMT_BGR24,
    IMGFMT_RGB24,
    IMGFMT_RGB48LE,
    IMGFMT_RGB48BE,
    IMGFMT_BGR16,
    IMGFMT_RGB16,
    IMGFMT_BGR15,
    IMGFMT_RGB15,
    IMGFMT_BGR12,
    IMGFMT_RGB12,
    IMGFMT_Y800,
    IMGFMT_Y8,
    IMGFMT_BGR8,
    IMGFMT_RGB8,
    IMGFMT_BGR4,
    IMGFMT_RGB4,
    IMGFMT_BG4B,
    IMGFMT_RG4B,
    IMGFMT_BGR1,
    IMGFMT_RGB1,
    0
};

/**
 * A list of preferred conversions, in order of preference.
 * This should be used for conversions that e.g. involve no scaling
 * or to stop vf_scale from choosing a conversion that has no
 * fast assembler implementation.
 */
static int preferred_conversions[][2] = {
    {IMGFMT_YUY2, IMGFMT_UYVY},
    {IMGFMT_YUY2, IMGFMT_422P},
    {IMGFMT_UYVY, IMGFMT_YUY2},
    {IMGFMT_UYVY, IMGFMT_422P},
    {IMGFMT_422P, IMGFMT_YUY2},
    {IMGFMT_422P, IMGFMT_UYVY},
    {0, 0}
};

static unsigned int find_best_out(vf_instance_t *vf, int in_format){
    unsigned int best=0;
    int i = -1;
    int j = -1;
    int format = 0;

    // find the best outfmt:
    while (1) {
        int ret;
        if (j < 0) {
            format = in_format;
            j = 0;
        } else if (i < 0) {
            while (preferred_conversions[j][0] &&
                   preferred_conversions[j][0] != in_format)
                j++;
            format = preferred_conversions[j++][1];
            // switch to standard list
            if (!format)
                i = 0;
        }
        if (i >= 0)
            format = outfmt_list[i++];
        if (!format)
            break;
        ret = vf_next_query_format(vf, format);

	mp_msg(MSGT_VFILTER,MSGL_DBG2,"scale: query(%s) -> %d\n",vo_format_name(format),ret&3);
	if(ret&VFCAP_CSP_SUPPORTED_BY_HW){
            best=format; // no conversion -> bingo!
            break;
        }
	if(ret&VFCAP_CSP_SUPPORTED && !best)
            best=format; // best with conversion
    }
    return best;
}

static int config(struct vf_instance *vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    unsigned int best=find_best_out(vf, outfmt);
    int vo_flags;
    int int_sws_flags=0;
    int round_w=0, round_h=0;
    int i;
    SwsFilter *srcFilter, *dstFilter;
    enum PixelFormat dfmt, sfmt;

    if(!best){
	mp_msg(MSGT_VFILTER,MSGL_WARN,"SwScale: no supported outfmt found :(\n");
	return 0;
    }
    sfmt = imgfmt2pixfmt(outfmt);
    if (outfmt == IMGFMT_RGB8 || outfmt == IMGFMT_BGR8) sfmt = PIX_FMT_PAL8;
    dfmt = imgfmt2pixfmt(best);

    vo_flags=vf->next->query_format(vf->next,best);

    // scaling to dwidth*d_height, if all these TRUE:
    // - option -zoom
    // - no other sw/hw up/down scaling avail.
    // - we're after postproc
    // - user didn't set w:h
    if(!(vo_flags&VFCAP_POSTPROC) && (flags&4) &&
	    vf->priv->w<0 && vf->priv->h<0){	// -zoom
	int x=(vo_flags&VFCAP_SWSCALE) ? 0 : 1;
	if(d_width<width || d_height<height){
	    // downscale!
	    if(vo_flags&VFCAP_HWSCALE_DOWN) x=0;
	} else {
	    // upscale:
	    if(vo_flags&VFCAP_HWSCALE_UP) x=0;
	}
	if(x){
	    // user wants sw scaling! (-zoom)
	    vf->priv->w=d_width;
	    vf->priv->h=d_height;
	}
    }

    if(vf->priv->noup){
        if((vf->priv->w > width) + (vf->priv->h > height) >= vf->priv->noup){
            vf->priv->w= width;
            vf->priv->h= height;
        }
    }

    if (vf->priv->w <= -8) {
      vf->priv->w += 8;
      round_w = 1;
    }
    if (vf->priv->h <= -8) {
      vf->priv->h += 8;
      round_h = 1;
    }

    if (vf->priv->w < -3 || vf->priv->h < -3 ||
         (vf->priv->w < -1 && vf->priv->h < -1)) {
      // TODO: establish a direct connection to the user's brain
      // and find out what the heck he thinks MPlayer should do
      // with this nonsense.
      mp_msg(MSGT_VFILTER, MSGL_ERR, "SwScale: EUSERBROKEN Check your parameters, they make no sense!\n");
      return 0;
    }

    if (vf->priv->w == -1)
      vf->priv->w = width;
    if (vf->priv->w == 0)
      vf->priv->w = d_width;

    if (vf->priv->h == -1)
      vf->priv->h = height;
    if (vf->priv->h == 0)
      vf->priv->h = d_height;

    if (vf->priv->w == -3)
      vf->priv->w = vf->priv->h * width / height;
    if (vf->priv->w == -2)
      vf->priv->w = vf->priv->h * d_width / d_height;

    if (vf->priv->h == -3)
      vf->priv->h = vf->priv->w * height / width;
    if (vf->priv->h == -2)
      vf->priv->h = vf->priv->w * d_height / d_width;

    if (round_w)
      vf->priv->w = ((vf->priv->w + 8) / 16) * 16;
    if (round_h)
      vf->priv->h = ((vf->priv->h + 8) / 16) * 16;

    // calculate the missing parameters:
    switch(best) {
    case IMGFMT_YV12:		/* YV12 needs w & h rounded to 2 */
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_NV12:
    case IMGFMT_NV21:
      vf->priv->h = (vf->priv->h + 1) & ~1;
    case IMGFMT_YUY2:		/* YUY2 needs w rounded to 2 */
    case IMGFMT_UYVY:
      vf->priv->w = (vf->priv->w + 1) & ~1;
    }

    mp_msg(MSGT_VFILTER,MSGL_DBG2,"SwScale: scaling %dx%d %s to %dx%d %s  \n",
	width,height,vo_format_name(outfmt),
	vf->priv->w,vf->priv->h,vo_format_name(best));

    // free old ctx:
    if(vf->priv->ctx) sws_freeContext(vf->priv->ctx);
    if(vf->priv->ctx2)sws_freeContext(vf->priv->ctx2);

    // new swscaler:
    sws_getFlagsAndFilterFromCmdLine(&int_sws_flags, &srcFilter, &dstFilter);
    int_sws_flags|= vf->priv->v_chr_drop << SWS_SRC_V_CHR_DROP_SHIFT;
    int_sws_flags|= vf->priv->accurate_rnd * SWS_ACCURATE_RND;
    vf->priv->ctx=sws_getContext(width, height >> vf->priv->interlaced,
	    sfmt,
		  vf->priv->w, vf->priv->h >> vf->priv->interlaced,
	    dfmt,
	    int_sws_flags | get_sws_cpuflags(), srcFilter, dstFilter, vf->priv->param);
    if(vf->priv->interlaced){
        vf->priv->ctx2=sws_getContext(width, height >> 1,
	    sfmt,
		  vf->priv->w, vf->priv->h >> 1,
	    dfmt,
	    int_sws_flags | get_sws_cpuflags(), srcFilter, dstFilter, vf->priv->param);
    }
    if(!vf->priv->ctx){
	// error...
	mp_msg(MSGT_VFILTER,MSGL_WARN,"Couldn't init SwScaler for this setup\n");
	return 0;
    }
    vf->priv->fmt=best;

    if(vf->priv->palette){
	free(vf->priv->palette);
	vf->priv->palette=NULL;
    }
    switch(best){
    case IMGFMT_RGB8: {
      /* set 332 palette for 8 bpp */
	vf->priv->palette=malloc(4*256);
	for(i=0; i<256; i++){
	    vf->priv->palette[4*i+0]=4*(i>>6)*21;
	    vf->priv->palette[4*i+1]=4*((i>>3)&7)*9;
	    vf->priv->palette[4*i+2]=4*((i&7)&7)*9;
            vf->priv->palette[4*i+3]=0;
	}
	break; }
    case IMGFMT_BGR8: {
      /* set 332 palette for 8 bpp */
	vf->priv->palette=malloc(4*256);
	for(i=0; i<256; i++){
	    vf->priv->palette[4*i+0]=4*(i&3)*21;
	    vf->priv->palette[4*i+1]=4*((i>>2)&7)*9;
	    vf->priv->palette[4*i+2]=4*((i>>5)&7)*9;
            vf->priv->palette[4*i+3]=0;
	}
	break; }
    case IMGFMT_BGR4:
    case IMGFMT_BG4B: {
	vf->priv->palette=malloc(4*16);
	for(i=0; i<16; i++){
	    vf->priv->palette[4*i+0]=4*(i&1)*63;
	    vf->priv->palette[4*i+1]=4*((i>>1)&3)*21;
	    vf->priv->palette[4*i+2]=4*((i>>3)&1)*63;
            vf->priv->palette[4*i+3]=0;
	}
	break; }
    case IMGFMT_RGB4:
    case IMGFMT_RG4B: {
	vf->priv->palette=malloc(4*16);
	for(i=0; i<16; i++){
	    vf->priv->palette[4*i+0]=4*(i>>3)*63;
	    vf->priv->palette[4*i+1]=4*((i>>1)&3)*21;
	    vf->priv->palette[4*i+2]=4*((i&1)&1)*63;
            vf->priv->palette[4*i+3]=0;
	}
	break; }
    }

    if(!opt_screen_size_x && !opt_screen_size_y && !(screen_size_xy >= 0.001)){
	// Compute new d_width and d_height, preserving aspect
	// while ensuring that both are >= output size in pixels.
	if (vf->priv->h * d_width > vf->priv->w * d_height) {
		d_width = vf->priv->h * d_width / d_height;
		d_height = vf->priv->h;
	} else {
		d_height = vf->priv->w * d_height / d_width;
		d_width = vf->priv->w;
	}
	//d_width=d_width*vf->priv->w/width;
	//d_height=d_height*vf->priv->h/height;
    }
    return vf_next_config(vf,vf->priv->w,vf->priv->h,d_width,d_height,flags,best);
}

static void start_slice(struct vf_instance *vf, mp_image_t *mpi){
//    printf("start_slice called! flag=%d\n",mpi->flags&MP_IMGFLAG_DRAW_CALLBACK);
    if(!(mpi->flags&MP_IMGFLAG_DRAW_CALLBACK)) return; // shouldn't happen
    // they want slices!!! allocate the buffer.
    mpi->priv=vf->dmpi=vf_get_image(vf->next,vf->priv->fmt,
//	mpi->type, mpi->flags & (~MP_IMGFLAG_DRAW_CALLBACK),
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_PREFER_ALIGNED_STRIDE,
	vf->priv->w, vf->priv->h);
}

static void scale(struct SwsContext *sws1, struct SwsContext *sws2, uint8_t *src[MP_MAX_PLANES], int src_stride[MP_MAX_PLANES],
                  int y, int h,  uint8_t *dst[MP_MAX_PLANES], int dst_stride[MP_MAX_PLANES], int interlaced){
    uint8_t *src2[MP_MAX_PLANES]={src[0], src[1], src[2], src[3]};
#if HAVE_BIGENDIAN
    uint32_t pal2[256];
    if (src[1] && !src[2]){
        int i;
        for(i=0; i<256; i++)
            pal2[i]= bswap_32(((uint32_t*)src[1])[i]);
        src2[1]= pal2;
    }
#endif

    if(interlaced){
        int i;
        uint8_t *dst2[MP_MAX_PLANES]={dst[0], dst[1], dst[2], dst[3]};
        int src_stride2[MP_MAX_PLANES]={2*src_stride[0], 2*src_stride[1], 2*src_stride[2], 2*src_stride[3]};
        int dst_stride2[MP_MAX_PLANES]={2*dst_stride[0], 2*dst_stride[1], 2*dst_stride[2], 2*dst_stride[3]};

        sws_scale(sws1, src2, src_stride2, y>>1, h>>1, dst2, dst_stride2);
        for(i=0; i<MP_MAX_PLANES; i++){
            src2[i] += src_stride[i];
            dst2[i] += dst_stride[i];
        }
        sws_scale(sws2, src2, src_stride2, y>>1, h>>1, dst2, dst_stride2);
    }else{
        sws_scale(sws1, src2, src_stride, y, h, dst, dst_stride);
    }
}

static void draw_slice(struct vf_instance *vf,
        unsigned char** src, int* stride, int w,int h, int x, int y){
    mp_image_t *dmpi=vf->dmpi;
    if(!dmpi){
	mp_msg(MSGT_VFILTER,MSGL_FATAL,"vf_scale: draw_slice() called with dmpi=NULL (no get_image?)\n");
	return;
    }
//    printf("vf_scale::draw_slice() y=%d h=%d\n",y,h);
    scale(vf->priv->ctx, vf->priv->ctx2, src, stride, y, h, dmpi->planes, dmpi->stride, vf->priv->interlaced);
}

static int put_image(struct vf_instance *vf, mp_image_t *mpi, double pts){
    mp_image_t *dmpi=mpi->priv;

//    printf("vf_scale::put_image(): processing whole frame! dmpi=%p flag=%d\n",
//	dmpi, (mpi->flags&MP_IMGFLAG_DRAW_CALLBACK));

  if(!(mpi->flags&MP_IMGFLAG_DRAW_CALLBACK && dmpi)){

    // hope we'll get DR buffer:
    dmpi=vf_get_image(vf->next,vf->priv->fmt,
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_PREFER_ALIGNED_STRIDE,
	vf->priv->w, vf->priv->h);

      scale(vf->priv->ctx, vf->priv->ctx, mpi->planes,mpi->stride,0,mpi->h,dmpi->planes,dmpi->stride, vf->priv->interlaced);
  }

    if(vf->priv->w==mpi->w && vf->priv->h==mpi->h){
	// just conversion, no scaling -> keep postprocessing data
	// this way we can apply pp filter to non-yv12 source using scaler
        vf_clone_mpi_attributes(dmpi, mpi);
    }

    if(vf->priv->palette) dmpi->planes[1]=vf->priv->palette; // export palette!

    return vf_next_put_image(vf,dmpi, pts);
}

static int control(struct vf_instance *vf, int request, void* data){
    int *table;
    int *inv_table;
    int r;
    int brightness, contrast, saturation, srcRange, dstRange;
    vf_equalizer_t *eq;

  if(vf->priv->ctx)
    switch(request){
    case VFCTRL_GET_EQUALIZER:
	r= sws_getColorspaceDetails(vf->priv->ctx, &inv_table, &srcRange, &table, &dstRange, &brightness, &contrast, &saturation);
	if(r<0) break;

	eq = data;
	if (!strcmp(eq->item,"brightness")) {
		eq->value =  ((brightness*100) + (1<<15))>>16;
	}
	else if (!strcmp(eq->item,"contrast")) {
		eq->value = (((contrast  *100) + (1<<15))>>16) - 100;
	}
	else if (!strcmp(eq->item,"saturation")) {
		eq->value = (((saturation*100) + (1<<15))>>16) - 100;
	}
	else
		break;
	return CONTROL_TRUE;
    case VFCTRL_SET_EQUALIZER:
	r= sws_getColorspaceDetails(vf->priv->ctx, &inv_table, &srcRange, &table, &dstRange, &brightness, &contrast, &saturation);
	if(r<0) break;
//printf("set %f %f %f\n", brightness/(float)(1<<16), contrast/(float)(1<<16), saturation/(float)(1<<16));
	eq = data;

	if (!strcmp(eq->item,"brightness")) {
		brightness = (( eq->value     <<16) + 50)/100;
	}
	else if (!strcmp(eq->item,"contrast")) {
		contrast   = (((eq->value+100)<<16) + 50)/100;
	}
	else if (!strcmp(eq->item,"saturation")) {
		saturation = (((eq->value+100)<<16) + 50)/100;
	}
	else
		break;

	r= sws_setColorspaceDetails(vf->priv->ctx, inv_table, srcRange, table, dstRange, brightness, contrast, saturation);
	if(r<0) break;
	if(vf->priv->ctx2){
            r= sws_setColorspaceDetails(vf->priv->ctx2, inv_table, srcRange, table, dstRange, brightness, contrast, saturation);
            if(r<0) break;
        }

	return CONTROL_TRUE;
    default:
	break;
    }

    return vf_next_control(vf,request,data);
}

//===========================================================================//

//  supported Input formats: YV12, I420, IYUV, YUY2, UYVY, BGR32, BGR24, BGR16, BGR15, RGB32, RGB24, Y8, Y800

static int query_format(struct vf_instance *vf, unsigned int fmt){
    switch(fmt){
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_UYVY:
    case IMGFMT_YUY2:
    case IMGFMT_BGR32:
    case IMGFMT_BGR24:
    case IMGFMT_BGR16:
    case IMGFMT_BGR15:
    case IMGFMT_RGB32:
    case IMGFMT_RGB24:
    case IMGFMT_Y800:
    case IMGFMT_Y8:
    case IMGFMT_YVU9:
    case IMGFMT_IF09:
    case IMGFMT_444P:
    case IMGFMT_422P:
    case IMGFMT_411P:
    case IMGFMT_440P:
    case IMGFMT_420A:
    case IMGFMT_444P16_LE:
    case IMGFMT_444P16_BE:
    case IMGFMT_422P16_LE:
    case IMGFMT_422P16_BE:
    case IMGFMT_420P16_LE:
    case IMGFMT_420P16_BE:
    case IMGFMT_BGR8:
    case IMGFMT_RGB8:
    case IMGFMT_BG4B:
    case IMGFMT_RG4B:
    case IMGFMT_RGB48LE:
    case IMGFMT_RGB48BE:
    {
	unsigned int best=find_best_out(vf, fmt);
	int flags;
	if(!best) return 0;	 // no matching out-fmt
	flags=vf_next_query_format(vf,best);
	if(!(flags&(VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW))) return 0; // huh?
	if(fmt!=best) flags&=~VFCAP_CSP_SUPPORTED_BY_HW;
	// do not allow scaling, if we are before the PP fliter!
	if(!(flags&VFCAP_POSTPROC)) flags|=VFCAP_SWSCALE;
	return flags;
      }
    }
    return 0;	// nomatching in-fmt
}

static void uninit(struct vf_instance *vf){
    if(vf->priv->ctx) sws_freeContext(vf->priv->ctx);
    if(vf->priv->ctx2) sws_freeContext(vf->priv->ctx2);
    if(vf->priv->palette) free(vf->priv->palette);
    free(vf->priv);
}

static int vf_open(vf_instance_t *vf, char *args){
    vf->config=config;
    vf->start_slice=start_slice;
    vf->draw_slice=draw_slice;
    vf->put_image=put_image;
    vf->query_format=query_format;
    vf->control= control;
    vf->uninit=uninit;
    mp_msg(MSGT_VFILTER,MSGL_V,"SwScale params: %d x %d (-1=no scaling)\n",
    vf->priv->w,
    vf->priv->h);

    return 1;
}

//global sws_flags from the command line
int sws_flags=2;

//global srcFilter
static SwsFilter *src_filter= NULL;

float sws_lum_gblur= 0.0;
float sws_chr_gblur= 0.0;
int sws_chr_vshift= 0;
int sws_chr_hshift= 0;
float sws_chr_sharpen= 0.0;
float sws_lum_sharpen= 0.0;

int get_sws_cpuflags(void){
    return
          (gCpuCaps.hasMMX   ? SWS_CPU_CAPS_MMX   : 0)
	| (gCpuCaps.hasMMX2  ? SWS_CPU_CAPS_MMX2  : 0)
	| (gCpuCaps.has3DNow ? SWS_CPU_CAPS_3DNOW : 0)
        | (gCpuCaps.hasAltiVec ? SWS_CPU_CAPS_ALTIVEC : 0);
}

void sws_getFlagsAndFilterFromCmdLine(int *flags, SwsFilter **srcFilterParam, SwsFilter **dstFilterParam)
{
	static int firstTime=1;
	*flags=0;

#if ARCH_X86
	if(gCpuCaps.hasMMX)
		__asm__ volatile("emms\n\t"::: "memory"); //FIXME this should not be required but it IS (even for non-MMX versions)
#endif
	if(firstTime)
	{
		firstTime=0;
		*flags= SWS_PRINT_INFO;
	}
	else if( mp_msg_test(MSGT_VFILTER,MSGL_DBG2) ) *flags= SWS_PRINT_INFO;

	if(src_filter) sws_freeFilter(src_filter);

	src_filter= sws_getDefaultFilter(
		sws_lum_gblur, sws_chr_gblur,
		sws_lum_sharpen, sws_chr_sharpen,
		sws_chr_hshift, sws_chr_vshift, verbose>1);

	switch(sws_flags)
	{
		case 0: *flags|= SWS_FAST_BILINEAR; break;
		case 1: *flags|= SWS_BILINEAR; break;
		case 2: *flags|= SWS_BICUBIC; break;
		case 3: *flags|= SWS_X; break;
		case 4: *flags|= SWS_POINT; break;
		case 5: *flags|= SWS_AREA; break;
		case 6: *flags|= SWS_BICUBLIN; break;
		case 7: *flags|= SWS_GAUSS; break;
		case 8: *flags|= SWS_SINC; break;
		case 9: *flags|= SWS_LANCZOS; break;
		case 10:*flags|= SWS_SPLINE; break;
		default:*flags|= SWS_BILINEAR; break;
	}

	*srcFilterParam= src_filter;
	*dstFilterParam= NULL;
}

// will use sws_flags & src_filter (from cmd line)
struct SwsContext *sws_getContextFromCmdLine(int srcW, int srcH, int srcFormat, int dstW, int dstH, int dstFormat)
{
	int flags;
	SwsFilter *dstFilterParam, *srcFilterParam;
	enum PixelFormat dfmt, sfmt;

	dfmt = imgfmt2pixfmt(dstFormat);
	sfmt = imgfmt2pixfmt(srcFormat);
	if (srcFormat == IMGFMT_RGB8 || srcFormat == IMGFMT_BGR8) sfmt = PIX_FMT_PAL8;
	sws_getFlagsAndFilterFromCmdLine(&flags, &srcFilterParam, &dstFilterParam);

	return sws_getContext(srcW, srcH, sfmt, dstW, dstH, dfmt, flags | get_sws_cpuflags(), srcFilterParam, dstFilterParam, NULL);
}

/// An example of presets usage
static const struct size_preset {
  char* name;
  int w, h;
} vf_size_presets_defs[] = {
  // TODO add more 'standard' resolutions
  { "qntsc", 352, 240 },
  { "qpal", 352, 288 },
  { "ntsc", 720, 480 },
  { "pal", 720, 576 },
  { "sntsc", 640, 480 },
  { "spal", 768, 576 },
  { NULL, 0, 0}
};

#define ST_OFF(f) M_ST_OFF(struct size_preset,f)
static const m_option_t vf_size_preset_fields[] = {
  {"w", ST_OFF(w), CONF_TYPE_INT, M_OPT_MIN,1 ,0, NULL},
  {"h", ST_OFF(h), CONF_TYPE_INT, M_OPT_MIN,1 ,0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};

static const m_struct_t vf_size_preset = {
  "scale_size_preset",
  sizeof(struct size_preset),
  NULL,
  vf_size_preset_fields
};

static const m_struct_t vf_opts;
static const m_obj_presets_t size_preset = {
  &vf_size_preset, // Input struct desc
  &vf_opts, // Output struct desc
  vf_size_presets_defs, // The list of presets
  ST_OFF(name) // At wich offset is the name field in the preset struct
};

/// Now the options
#undef ST_OFF
#define ST_OFF(f) M_ST_OFF(struct vf_priv_s,f)
static const m_option_t vf_opts_fields[] = {
  {"w", ST_OFF(w), CONF_TYPE_INT, M_OPT_MIN,-11,0, NULL},
  {"h", ST_OFF(h), CONF_TYPE_INT, M_OPT_MIN,-11,0, NULL},
  {"interlaced", ST_OFF(interlaced), CONF_TYPE_INT, M_OPT_RANGE, 0, 1, NULL},
  {"chr-drop", ST_OFF(v_chr_drop), CONF_TYPE_INT, M_OPT_RANGE, 0, 3, NULL},
  {"param" , ST_OFF(param[0]), CONF_TYPE_DOUBLE, M_OPT_RANGE, 0.0, 100.0, NULL},
  {"param2", ST_OFF(param[1]), CONF_TYPE_DOUBLE, M_OPT_RANGE, 0.0, 100.0, NULL},
  // Note that here the 2 field is NULL (ie 0)
  // As we want this option to act on the option struct itself
  {"presize", 0, CONF_TYPE_OBJ_PRESETS, 0, 0, 0, &size_preset},
  {"noup", ST_OFF(noup), CONF_TYPE_INT, M_OPT_RANGE, 0, 2, NULL},
  {"arnd", ST_OFF(accurate_rnd), CONF_TYPE_FLAG, 0, 0, 1, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};

static const m_struct_t vf_opts = {
  "scale",
  sizeof(struct vf_priv_s),
  &vf_priv_dflt,
  vf_opts_fields
};

const vf_info_t vf_info_scale = {
    "software scaling",
    "scale",
    "A'rpi",
    "",
    vf_open,
    &vf_opts
};

//===========================================================================//
