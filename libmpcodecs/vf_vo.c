#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"

#include "mp_image.h"
#include "vf.h"

#include "libvo/video_out.h"

#ifdef CONFIG_ASS
#include "libass/ass.h"
#include "libass/ass_mp.h"
extern ass_track_t* ass_track;
#endif

//===========================================================================//

extern int sub_visibility;
extern float sub_delay;

struct vf_priv_s {
    double pts;
    const vo_functions_t *vo;
#ifdef CONFIG_ASS
    ass_renderer_t* ass_priv;
    int prev_visibility;
#endif
};
#define video_out (vf->priv->vo)

static int query_format(struct vf_instance_s* vf, unsigned int fmt); /* forward declaration */

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){

    if ((width <= 0) || (height <= 0) || (d_width <= 0) || (d_height <= 0))
    {
	mp_msg(MSGT_CPLAYER, MSGL_ERR, "VO: invalid dimensions!\n");
	return 0;
    }

  if(video_out->info)
  { const vo_info_t *info = video_out->info;
    mp_msg(MSGT_CPLAYER,MSGL_INFO,"VO: [%s] %dx%d => %dx%d %s %s%s%s%s\n",info->short_name,
         width, height,
         d_width, d_height,
	 vo_format_name(outfmt),
         (flags&VOFLAG_FULLSCREEN)?" [fs]":"",
         (flags&VOFLAG_MODESWITCHING)?" [vm]":"",
         (flags&VOFLAG_SWSCALE)?" [zoom]":"",
         (flags&VOFLAG_FLIPPING)?" [flip]":"");
    mp_msg(MSGT_CPLAYER,MSGL_V,"VO: Description: %s\n",info->name);
    mp_msg(MSGT_CPLAYER,MSGL_V,"VO: Author: %s\n", info->author);
    if(info->comment && strlen(info->comment) > 0)
        mp_msg(MSGT_CPLAYER,MSGL_V,"VO: Comment: %s\n", info->comment);
  }

    // save vo's stride capability for the wanted colorspace:
    vf->default_caps=query_format(vf,outfmt);

    if(config_video_out(video_out,width,height,d_width,d_height,flags,"MPlayer",outfmt))
	return 0;

#ifdef CONFIG_ASS
    if (vf->priv->ass_priv)
	ass_configure(vf->priv->ass_priv, width, height, !!(vf->default_caps & VFCAP_EOSD_UNSCALED));
#endif

    ++vo_config_count;
    return 1;
}

static int control(struct vf_instance_s* vf, int request, void* data)
{
    switch(request){
    case VFCTRL_GET_DEINTERLACE:
    {
        if(!video_out) return CONTROL_FALSE; // vo not configured?
        return(video_out->control(VOCTRL_GET_DEINTERLACE, data)
                == VO_TRUE) ? CONTROL_TRUE : CONTROL_FALSE;
    }
    case VFCTRL_SET_DEINTERLACE:
    {
        if(!video_out) return CONTROL_FALSE; // vo not configured?
        return(video_out->control(VOCTRL_SET_DEINTERLACE, data)
                == VO_TRUE) ? CONTROL_TRUE : CONTROL_FALSE;
    }
    case VFCTRL_DRAW_OSD:
	if(!vo_config_count) return CONTROL_FALSE; // vo not configured?
	video_out->draw_osd();
	return CONTROL_TRUE;
    case VFCTRL_FLIP_PAGE:
    {
	if(!vo_config_count) return CONTROL_FALSE; // vo not configured?
	video_out->flip_page();
	return CONTROL_TRUE;
    }
    case VFCTRL_SET_EQUALIZER:
    {
	vf_equalizer_t *eq=data;
	if(!vo_config_count) return CONTROL_FALSE; // vo not configured?
	return (video_out->control(VOCTRL_SET_EQUALIZER, eq->item, eq->value) == VO_TRUE) ? CONTROL_TRUE : CONTROL_FALSE;
    }
    case VFCTRL_GET_EQUALIZER:
    {
	vf_equalizer_t *eq=data;
	if(!vo_config_count) return CONTROL_FALSE; // vo not configured?
	return (video_out->control(VOCTRL_GET_EQUALIZER, eq->item, &eq->value) == VO_TRUE) ? CONTROL_TRUE : CONTROL_FALSE;
    }
#ifdef CONFIG_ASS
    case VFCTRL_INIT_EOSD:
    {
        vf->priv->ass_priv = ass_renderer_init((ass_library_t*)data);
        if (!vf->priv->ass_priv) return CONTROL_FALSE;
        ass_configure_fonts(vf->priv->ass_priv);
        vf->priv->prev_visibility = 0;
        return CONTROL_TRUE;
    }
    case VFCTRL_DRAW_EOSD:
    {
        mp_eosd_images_t images = {NULL, 2};
        double pts = vf->priv->pts;
        if (!vo_config_count || !vf->priv->ass_priv) return CONTROL_FALSE;
        if (sub_visibility && vf->priv->ass_priv && ass_track && (pts != MP_NOPTS_VALUE)) {
            mp_eosd_res_t res;
            memset(&res, 0, sizeof(res));
            if (video_out->control(VOCTRL_GET_EOSD_RES, &res) == VO_TRUE) {
                ass_set_frame_size(vf->priv->ass_priv, res.w, res.h);
                ass_set_margins(vf->priv->ass_priv, res.mt, res.mb, res.ml, res.mr);
                ass_set_aspect_ratio(vf->priv->ass_priv, (double)res.w / res.h);
            }

            images.imgs = ass_mp_render_frame(vf->priv->ass_priv, ass_track, (pts+sub_delay) * 1000 + .5, &images.changed);
            if (!vf->priv->prev_visibility)
                images.changed = 2;
            vf->priv->prev_visibility = 1;
        } else
            vf->priv->prev_visibility = 0;
        vf->priv->prev_visibility = sub_visibility;
        return (video_out->control(VOCTRL_DRAW_EOSD, &images) == VO_TRUE) ? CONTROL_TRUE : CONTROL_FALSE;
    }
#endif
    case VFCTRL_GET_PTS:
    {
	*(double *)data = vf->priv->pts;
	return CONTROL_TRUE;
    }
    }
    // return video_out->control(request,data);
    return CONTROL_UNKNOWN;
}

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    int flags=video_out->control(VOCTRL_QUERY_FORMAT,&fmt);
    // draw_slice() accepts stride, draw_frame() doesn't:
    if(flags)
	if(fmt==IMGFMT_YV12 || fmt==IMGFMT_I420 || fmt==IMGFMT_IYUV)
	    flags|=VFCAP_ACCEPT_STRIDE;
    return flags;
}

static void get_image(struct vf_instance_s* vf,
        mp_image_t *mpi){
    if(vo_directrendering && vo_config_count)
	video_out->control(VOCTRL_GET_IMAGE,mpi);
}

static int put_image(struct vf_instance_s* vf,
        mp_image_t *mpi, double pts){
  if(!vo_config_count) return 0; // vo not configured?
  // record pts (potentially modified by filters) for main loop
  vf->priv->pts = pts;
  // first check, maybe the vo/vf plugin implements draw_image using mpi:
  if(video_out->control(VOCTRL_DRAW_IMAGE,mpi)==VO_TRUE) return 1; // done.
  // nope, fallback to old draw_frame/draw_slice:
  if(!(mpi->flags&(MP_IMGFLAG_DIRECT|MP_IMGFLAG_DRAW_CALLBACK))){
    // blit frame:
//    if(mpi->flags&MP_IMGFLAG_PLANAR)
    if(vf->default_caps&VFCAP_ACCEPT_STRIDE)
        video_out->draw_slice(mpi->planes,mpi->stride,mpi->w,mpi->h,mpi->x,mpi->y);
    else
        video_out->draw_frame(mpi->planes);
  }
  return 1;
}

static void start_slice(struct vf_instance_s* vf,
		       mp_image_t *mpi) {
    if(!vo_config_count) return; // vo not configured?
    video_out->control(VOCTRL_START_SLICE,mpi);
}

static void draw_slice(struct vf_instance_s* vf,
        unsigned char** src, int* stride, int w,int h, int x, int y){
    if(!vo_config_count) return; // vo not configured?
    video_out->draw_slice(src,stride,w,h,x,y);
}

static void uninit(struct vf_instance_s* vf)
{
    if (vf->priv) {
#ifdef CONFIG_ASS
        if (vf->priv->ass_priv)
            ass_renderer_done(vf->priv->ass_priv);
#endif
        free(vf->priv);
    }
}
//===========================================================================//

static int open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->control=control;
    vf->query_format=query_format;
    vf->get_image=get_image;
    vf->put_image=put_image;
    vf->draw_slice=draw_slice;
    vf->start_slice=start_slice;
    vf->uninit=uninit;
    vf->priv=calloc(1, sizeof(struct vf_priv_s));
    vf->priv->vo = (const vo_functions_t *)args;
    if(!video_out) return 0; // no vo ?
//    if(video_out->preinit(args)) return 0; // preinit failed
    return 1;
}

const vf_info_t vf_info_vo = {
    "libvo wrapper",
    "vo",
    "A'rpi",
    "for internal use",
    open,
    NULL
};

//===========================================================================//
