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
#include <sys/types.h>

#include "config.h"
#include "core/mp_msg.h"
#include "core/options.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"
#include "video/fmt-conversion.h"
#include "compat/mpbswap.h"

#include "video/sws_utils.h"

#include "video/csputils.h"
#include "video/out/vo.h"

#include "core/m_option.h"
#include "core/m_struct.h"

static struct vf_priv_s {
    int w,h;
    int cfg_w, cfg_h;
    int v_chr_drop;
    double param[2];
    unsigned int fmt;
    struct SwsContext *ctx;
    struct SwsContext *ctx2; //for interlaced slices only
    unsigned char* palette;
    int interlaced;
    int noup;
    int accurate_rnd;
    struct mp_csp_details colorspace;
} const vf_priv_dflt = {
  0, 0,
  -1,-1,
  0,
  {SWS_PARAM_DEFAULT, SWS_PARAM_DEFAULT},
  0,
  NULL,
  NULL,
  NULL
};

static int mp_sws_set_colorspace(struct SwsContext *sws,
                                 struct mp_csp_details *csp);

//===========================================================================//

static const unsigned int outfmt_list[]={
// YUV:
    IMGFMT_444P,
    IMGFMT_444P16_LE,
    IMGFMT_444P16_BE,
    IMGFMT_444P14_LE,
    IMGFMT_444P14_BE,
    IMGFMT_444P12_LE,
    IMGFMT_444P12_BE,
    IMGFMT_444P10_LE,
    IMGFMT_444P10_BE,
    IMGFMT_444P9_LE,
    IMGFMT_444P9_BE,
    IMGFMT_422P,
    IMGFMT_422P16_LE,
    IMGFMT_422P16_BE,
    IMGFMT_422P14_LE,
    IMGFMT_422P14_BE,
    IMGFMT_422P12_LE,
    IMGFMT_422P12_BE,
    IMGFMT_422P10_LE,
    IMGFMT_422P10_BE,
    IMGFMT_422P9_LE,
    IMGFMT_422P9_BE,
    IMGFMT_YV12,
    IMGFMT_I420,
    IMGFMT_420P16_LE,
    IMGFMT_420P16_BE,
    IMGFMT_420P14_LE,
    IMGFMT_420P14_BE,
    IMGFMT_420P12_LE,
    IMGFMT_420P12_BE,
    IMGFMT_420P10_LE,
    IMGFMT_420P10_BE,
    IMGFMT_420P9_LE,
    IMGFMT_420P9_BE,
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
    IMGFMT_ABGR,
    IMGFMT_ARGB,
    IMGFMT_BGRA,
    IMGFMT_RGBA,
    IMGFMT_BGR24,
    IMGFMT_RGB24,
    IMGFMT_GBRP,
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
    {IMGFMT_420P10, IMGFMT_YV12},
    {IMGFMT_GBRP, IMGFMT_BGR24},
    {IMGFMT_GBRP, IMGFMT_RGB24},
    {IMGFMT_GBRP, IMGFMT_BGR32},
    {IMGFMT_GBRP, IMGFMT_RGB32},
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
    struct MPOpts *opts = vf->opts;
    unsigned int best=find_best_out(vf, outfmt);
    int int_sws_flags=0;
    int round_w=0, round_h=0;
    int i;
    SwsFilter *srcFilter, *dstFilter;
    enum PixelFormat dfmt, sfmt;

    vf->priv->colorspace = (struct mp_csp_details) {0};

    if(!best){
	mp_msg(MSGT_VFILTER,MSGL_WARN,"SwScale: no supported outfmt found :(\n");
	return 0;
    }
    sfmt = imgfmt2pixfmt(outfmt);
    if (outfmt == IMGFMT_RGB8 || outfmt == IMGFMT_BGR8) sfmt = PIX_FMT_PAL8;
    dfmt = imgfmt2pixfmt(best);

    vf->next->query_format(vf->next,best);

    vf->priv->w = vf->priv->cfg_w;
    vf->priv->h = vf->priv->cfg_h;

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

    // check for upscaling, now that all parameters had been applied
    if(vf->priv->noup){
        if((vf->priv->w > width) + (vf->priv->h > height) >= vf->priv->noup){
            vf->priv->w= width;
            vf->priv->h= height;
        }
    }

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
	    int_sws_flags, srcFilter, dstFilter, vf->priv->param);
    if(vf->priv->interlaced){
        vf->priv->ctx2=sws_getContext(width, height >> 1,
	    sfmt,
		  vf->priv->w, vf->priv->h >> 1,
	    dfmt,
	    int_sws_flags, srcFilter, dstFilter, vf->priv->param);
    }
    if(!vf->priv->ctx){
	// error...
	mp_msg(MSGT_VFILTER,MSGL_WARN,"Couldn't init SwScaler for this setup\n");
	return 0;
    }
    vf->priv->fmt=best;

    free(vf->priv->palette);
    vf->priv->palette=NULL;
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

    if (!opts->screen_size_x && !opts->screen_size_y
        && !(opts->screen_size_xy >= 0.001)) {
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
    const uint8_t *src2[MP_MAX_PLANES]={src[0], src[1], src[2], src[3]};
#if BYTE_ORDER == BIG_ENDIAN
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
    case VFCTRL_SET_YUV_COLORSPACE: {
        struct mp_csp_details colorspace = *(struct mp_csp_details *)data;
        if (mp_sws_set_colorspace(vf->priv->ctx, &colorspace) >= 0) {
            if (vf->priv->ctx2)
                mp_sws_set_colorspace(vf->priv->ctx2, &colorspace);
            vf->priv->colorspace = colorspace;
            return 1;
        }
        break;
    }
    case VFCTRL_GET_YUV_COLORSPACE: {
        /* This scale filter should never react to colorspace commands if it
         * doesn't do YUV->RGB conversion. But because finding out whether this
         * is really YUV->RGB (and not YUV->YUV or anything else) is hard,
         * react only if the colorspace has been set explicitly before. The
         * trick is that mp_sws_set_colorspace does not succeed for YUV->YUV
         * and RGB->YUV conversions, which makes this code correct in "most"
         * cases. (This would be trivial to do correctly if libswscale exposed
         * functionality like isYUV()).
         */
        if (vf->priv->colorspace.format) {
            *(struct mp_csp_details *)data = vf->priv->colorspace;
            return CONTROL_TRUE;
        }
        break;
    }
    default:
	break;
    }

    return vf_next_control(vf,request,data);
}

static const int mp_csp_to_swscale[MP_CSP_COUNT] = {
    [MP_CSP_BT_601] = SWS_CS_ITU601,
    [MP_CSP_BT_709] = SWS_CS_ITU709,
    [MP_CSP_SMPTE_240M] = SWS_CS_SMPTE240M,
};

// Adjust the colorspace used for YUV->RGB conversion. On other conversions,
// do nothing or return an error.
// The csp argument is set to the supported values.
// Return 0 on success and -1 on error.
static int mp_sws_set_colorspace(struct SwsContext *sws,
                                 struct mp_csp_details *csp)
{
    int *table, *inv_table;
    int brightness, contrast, saturation, srcRange, dstRange;

    csp->levels_out = MP_CSP_LEVELS_PC;

    // NOTE: returns an error if the destination format is YUV
    if (sws_getColorspaceDetails(sws, &inv_table, &srcRange, &table, &dstRange,
                                 &brightness, &contrast, &saturation) == -1)
        goto error_out;

    int sws_csp = mp_csp_to_swscale[csp->format];
    if (sws_csp == 0) {
        // colorspace not supported, go with a reasonable default
        csp->format = SWS_CS_ITU601;
        sws_csp = MP_CSP_BT_601;
    }

    /* The swscale API for these is hardly documented.
     * Apparently table/range only apply to YUV. Thus dstRange has no effect
     * for YUV->RGB conversions, and conversions to limited-range RGB are
     * not supported.
     */
    srcRange = csp->levels_in == MP_CSP_LEVELS_PC;
    const int *new_inv_table = sws_getCoefficients(sws_csp);

    if (sws_setColorspaceDetails(sws, new_inv_table, srcRange, table, dstRange,
        brightness, contrast, saturation) == -1)
        goto error_out;

    return 0;

error_out:
    *csp = (struct mp_csp_details){0};
    return -1;
}

//===========================================================================//

//  supported Input formats: YV12, I420, IYUV, YUY2, UYVY, BGR32, BGR24, BGR16, BGR15, RGB32, RGB24, Y8, Y800

static int query_format(struct vf_instance *vf, unsigned int fmt){
    if (!IMGFMT_IS_HWACCEL(fmt) && imgfmt2pixfmt(fmt) != PIX_FMT_NONE) {
	unsigned int best=find_best_out(vf, fmt);
	int flags;
	if(!best) return 0;	 // no matching out-fmt
	flags=vf_next_query_format(vf,best);
	if(!(flags&(VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW))) return 0; // huh?
	if(fmt!=best) flags&=~VFCAP_CSP_SUPPORTED_BY_HW;
	return flags;
    }
    return 0;	// nomatching in-fmt
}

static void uninit(struct vf_instance *vf){
    if(vf->priv->ctx) sws_freeContext(vf->priv->ctx);
    if(vf->priv->ctx2) sws_freeContext(vf->priv->ctx2);
    free(vf->priv->palette);
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
    vf->priv->cfg_w,
    vf->priv->cfg_h);

    return 1;
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
  {"w", ST_OFF(cfg_w), CONF_TYPE_INT, M_OPT_MIN,-11,0, NULL},
  {"h", ST_OFF(cfg_h), CONF_TYPE_INT, M_OPT_MIN,-11,0, NULL},
  {"interlaced", ST_OFF(interlaced), CONF_TYPE_INT, M_OPT_RANGE, 0, 1, NULL},
  {"chr-drop", ST_OFF(v_chr_drop), CONF_TYPE_INT, M_OPT_RANGE, 0, 3, NULL},
  {"param" , ST_OFF(param[0]), CONF_TYPE_DOUBLE, M_OPT_RANGE, 0.0, 100.0, NULL},
  {"param2", ST_OFF(param[1]), CONF_TYPE_DOUBLE, M_OPT_RANGE, 0.0, 100.0, NULL},
  // Note that here the 2 field is NULL (ie 0)
  // As we want this option to act on the option struct itself
  {"presize", 0, CONF_TYPE_OBJ_PRESETS, 0, 0, 0, (void *)&size_preset},
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
