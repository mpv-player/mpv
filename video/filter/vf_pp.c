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
#include <errno.h>
#include <assert.h>

#include "config.h"
#include "core/mp_msg.h"
#include "core/cpudetect.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"
#include "libpostproc/postprocess.h"

struct vf_priv_s {
    int pp;
    pp_mode *ppMode[PP_QUALITY_MAX+1];
    void *context;
    unsigned int outfmt;
};

//===========================================================================//

static int config(struct vf_instance *vf,
        int width, int height, int d_width, int d_height,
	unsigned int voflags, unsigned int outfmt){
    int flags=
          (gCpuCaps.hasMMX   ? PP_CPU_CAPS_MMX   : 0)
	| (gCpuCaps.hasMMX2  ? PP_CPU_CAPS_MMX2  : 0);

    switch(outfmt){
    case IMGFMT_444P: flags|= PP_FORMAT_444; break;
    case IMGFMT_422P: flags|= PP_FORMAT_422; break;
    case IMGFMT_411P: flags|= PP_FORMAT_411; break;
    default:          flags|= PP_FORMAT_420; break;
    }

    if(vf->priv->context) pp_free_context(vf->priv->context);
    vf->priv->context= pp_get_context(width, height, flags);

    return vf_next_config(vf,width,height,d_width,d_height,voflags,outfmt);
}

static void uninit(struct vf_instance *vf){
    int i;
    for(i=0; i<=PP_QUALITY_MAX; i++){
        if(vf->priv->ppMode[i])
	    pp_free_mode(vf->priv->ppMode[i]);
    }
    if(vf->priv->context) pp_free_context(vf->priv->context);
}

static int query_format(struct vf_instance *vf, unsigned int fmt){
    switch(fmt){
    case IMGFMT_444P:
    case IMGFMT_422P:
    case IMGFMT_420P:
    case IMGFMT_411P: ;
	int caps = vf_next_query_format(vf,fmt);
        if (caps)
            caps |= VFCAP_POSTPROC;
        return caps;
    }
    return 0;
}

static int control(struct vf_instance *vf, int request, void* data){
    switch(request){
    case VFCTRL_QUERY_MAX_PP_LEVEL:
	return PP_QUALITY_MAX;
    case VFCTRL_SET_PP_LEVEL:
	vf->priv->pp= *((unsigned int*)data);
	return CONTROL_TRUE;
    }
    return vf_next_control(vf,request,data);
}

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
    // pass-through if pp disabled
    if (!vf->priv->pp)
        return mpi;

    bool non_local = vf->priv->pp & 0xFFFF;

    struct mp_image *dmpi = mpi;
    if (!mp_image_is_writeable(mpi) || non_local) {
        dmpi = vf_alloc_out_image(vf);
        mp_image_copy_attributes(dmpi, mpi);
    }

    // apparently this is required
    assert(mpi->stride[0] >= ((mpi->w+7)&(~7)));

	// do the postprocessing! (or copy if no DR)
	pp_postprocess((const uint8_t **)mpi->planes, mpi->stride,
		    dmpi->planes,dmpi->stride,
		    (mpi->w+7)&(~7),mpi->h,
		    mpi->qscale, mpi->qstride,
		    vf->priv->ppMode[ vf->priv->pp ], vf->priv->context,
#ifdef PP_PICT_TYPE_QP2
		    mpi->pict_type | (mpi->qscale_type ? PP_PICT_TYPE_QP2 : 0));
#else
		    mpi->pict_type);
#endif

    if (dmpi != mpi)
        talloc_free(mpi);
    return dmpi;
}

//===========================================================================//

static const unsigned int fmt_list[]={
    IMGFMT_420P,
    IMGFMT_444P,
    IMGFMT_422P,
    IMGFMT_411P,
    0
};

static int vf_open(vf_instance_t *vf, char *args){
    int i;

    vf->query_format=query_format;
    vf->control=control;
    vf->config=config;
    vf->filter=filter;
    vf->uninit=uninit;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    vf->priv->context=NULL;

    // check csp:
    vf->priv->outfmt=vf_match_csp(&vf->next,fmt_list,IMGFMT_420P);
    if(!vf->priv->outfmt) return 0; // no csp match :(

    char *name = args ? args : "de";

	for(i=0; i<=PP_QUALITY_MAX; i++){
            vf->priv->ppMode[i]= pp_get_mode_by_name_and_quality(name, i);
            if(vf->priv->ppMode[i]==NULL) return -1;
        }

    vf->priv->pp=PP_QUALITY_MAX;
    return 1;
}

const vf_info_t vf_info_pp = {
    "postprocessing",
    "pp",
    "A'rpi",
    "",
    vf_open,
    NULL
};

//===========================================================================//
