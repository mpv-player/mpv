#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"

#include "mp_image.h"
#include "vf.h"

#include "libvo/video_out.h"

#ifdef USE_ASS
#include "libass/ass.h"
#include "libass/ass_mp.h"
extern ass_track_t* ass_track;
#endif

//===========================================================================//

extern int sub_visibility;
extern float sub_delay;

typedef struct vf_vo_data_s {
    double pts;
    vo_functions_t *vo;
} vf_vo_data_t;

struct vf_priv_s {
    vf_vo_data_t* vf_vo_data;
#ifdef USE_ASS
    ass_instance_t* ass_priv;
    ass_settings_t ass_settings;
#endif
};
#define video_out (vf->priv->vf_vo_data->vo)

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
    vf->default_caps=query_format(vf,outfmt) & VFCAP_ACCEPT_STRIDE;

    if(video_out->config(width,height,d_width,d_height,flags,"MPlayer",outfmt))
	return 0;

#ifdef USE_ASS
    if (vf->priv->ass_priv) {
        vf->priv->ass_settings.font_size_coeff = ass_font_scale;
        vf->priv->ass_settings.line_spacing = ass_line_spacing;
        vf->priv->ass_settings.use_margins = ass_use_margins;
    }
#endif

    ++vo_config_count;
    return 1;
}

static int control(struct vf_instance_s* vf, int request, void* data)
{
    switch(request){
#ifdef USE_OSD
    case VFCTRL_DRAW_OSD:
	if(!vo_config_count) return CONTROL_FALSE; // vo not configured?
	video_out->draw_osd();
	return CONTROL_TRUE;
#endif
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
	return((video_out->control(VOCTRL_SET_EQUALIZER, eq->item, eq->value) == VO_TRUE) ? CONTROL_TRUE : CONTROL_FALSE);
    }
    case VFCTRL_GET_EQUALIZER:
    {
	vf_equalizer_t *eq=data;
	if(!vo_config_count) return CONTROL_FALSE; // vo not configured?
	return((video_out->control(VOCTRL_GET_EQUALIZER, eq->item, &eq->value) == VO_TRUE) ? CONTROL_TRUE : CONTROL_FALSE);
    }
#ifdef USE_ASS
    case VFCTRL_INIT_EOSD:
    {
        vf->priv->ass_priv = ass_init();
        return vf->priv->ass_priv ? CONTROL_TRUE : CONTROL_FALSE;
    }
    case VFCTRL_DRAW_EOSD:
    {
        ass_image_t* images = 0;
        double pts = vf->priv->vf_vo_data->pts;
        if (!vo_config_count || !vf->priv->ass_priv) return CONTROL_FALSE;
        if (sub_visibility && vf->priv->ass_priv && ass_track && (pts != MP_NOPTS_VALUE)) {
            mp_eosd_res_t res;
            ass_settings_t* const settings = &vf->priv->ass_settings;
            memset(&res, 0, sizeof(res));
            if (video_out->control(VOCTRL_GET_EOSD_RES, &res) == VO_TRUE) {
                settings->frame_width = res.w;
                settings->frame_height = res.h;
                settings->top_margin = res.mt;
                settings->bottom_margin = res.mb;
                settings->left_margin = res.ml;
                settings->right_margin = res.mr;
                settings->aspect = ((double)res.w) / res.h;
            }
            ass_configure(vf->priv->ass_priv, settings);

            images = ass_render_frame(vf->priv->ass_priv, ass_track, (pts+sub_delay) * 1000 + .5);
        }
        return (video_out->control(VOCTRL_DRAW_EOSD, images) == VO_TRUE) ? CONTROL_TRUE : CONTROL_FALSE;
    }
#endif
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
  vf->priv->vf_vo_data->pts = pts;
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
#ifdef USE_ASS
        if (vf->priv->ass_priv)
            ass_done(vf->priv->ass_priv);
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
    vf->priv->vf_vo_data=(vf_vo_data_t*)args;
    if(!video_out) return 0; // no vo ?
//    if(video_out->preinit(args)) return 0; // preinit failed
    return 1;
}

vf_info_t vf_info_vo = {
    "libvo wrapper",
    "vo",
    "A'rpi",
    "for internal use",
    open,
    NULL
};

//===========================================================================//
