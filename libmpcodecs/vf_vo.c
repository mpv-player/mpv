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
#include <stdbool.h>

#include "config.h"
#include "mp_msg.h"

#include "mp_image.h"
#include "vf.h"

#include "libvo/video_out.h"

#ifdef CONFIG_ASS
#include "ass_mp.h"
extern ASS_Track *ass_track;
#endif

//===========================================================================//

extern int sub_visibility;
extern float sub_delay;

struct vf_priv_s {
    struct vo *vo;
#ifdef CONFIG_ASS
    ASS_Renderer *ass_priv;
    int prev_visibility;
    double scale_ratio;
#endif
};
#define video_out (vf->priv->vo)

static int query_format(struct vf_instance *vf, unsigned int fmt); /* forward declaration */
static void draw_slice(struct vf_instance *vf, unsigned char** src, int* stride, int w,int h, int x, int y);

static int config(struct vf_instance *vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){

    if ((width <= 0) || (height <= 0) || (d_width <= 0) || (d_height <= 0))
    {
	mp_msg(MSGT_CPLAYER, MSGL_ERR, "VO: invalid dimensions!\n");
	return 0;
    }

    const vo_info_t *info = video_out->driver->info;
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

    // save vo's stride capability for the wanted colorspace:
    vf->default_caps=query_format(vf,outfmt);
    vf->draw_slice = (vf->default_caps & VOCAP_NOSLICES) ? NULL : draw_slice;

    if (vo_config(video_out, width, height, d_width, d_height, flags, "MPlayer", outfmt))
	return 0;

#ifdef CONFIG_ASS
    vf->priv->scale_ratio = (double) d_width / d_height * height / width;

    if (vf->priv->ass_priv)
	ass_configure(vf->priv->ass_priv, width, height, !!(vf->default_caps & VFCAP_EOSD_UNSCALED));
#endif

    return 1;
}

static int control(struct vf_instance *vf, int request, void* data)
{
    switch(request){
    case VFCTRL_GET_DEINTERLACE:
    {
        if(!video_out) return CONTROL_FALSE; // vo not configured?
        return vo_control(video_out, VOCTRL_GET_DEINTERLACE, data) == VO_TRUE;
    }
    case VFCTRL_SET_DEINTERLACE:
    {
        if(!video_out) return CONTROL_FALSE; // vo not configured?
        return vo_control(video_out, VOCTRL_SET_DEINTERLACE, data) == VO_TRUE;
    }
    case VFCTRL_GET_YUV_COLORSPACE:
        return vo_control(video_out, VOCTRL_GET_YUV_COLORSPACE, data) == true;
    case VFCTRL_SET_YUV_COLORSPACE:
        return vo_control(video_out, VOCTRL_SET_YUV_COLORSPACE, data) == true;
    case VFCTRL_DRAW_OSD:
	if(!video_out->config_ok) return CONTROL_FALSE; // vo not configured?
	vo_draw_osd(video_out, data);
	return CONTROL_TRUE;
    case VFCTRL_REDRAW_OSD:
        return vo_control(video_out, VOCTRL_REDRAW_OSD, data) == true;
    case VFCTRL_SET_EQUALIZER:
    {
	vf_equalizer_t *eq=data;
	if(!video_out->config_ok) return CONTROL_FALSE; // vo not configured?
	struct voctrl_set_equalizer_args param = {eq->item, eq->value};
	return vo_control(video_out, VOCTRL_SET_EQUALIZER, &param) == VO_TRUE;
    }
    case VFCTRL_GET_EQUALIZER:
    {
	vf_equalizer_t *eq=data;
	if(!video_out->config_ok) return CONTROL_FALSE; // vo not configured?
	struct voctrl_get_equalizer_args param = {eq->item, &eq->value};
	return vo_control(video_out, VOCTRL_GET_EQUALIZER, &param) == VO_TRUE;
    }
#ifdef CONFIG_ASS
    case VFCTRL_INIT_EOSD:
    {
        vf->priv->ass_priv = ass_renderer_init((ASS_Library*)data);
        if (!vf->priv->ass_priv) return CONTROL_FALSE;
        ass_configure_fonts(vf->priv->ass_priv);
        vf->priv->prev_visibility = 0;
        return CONTROL_TRUE;
    }
    case VFCTRL_DRAW_EOSD:
    {
        mp_eosd_images_t images = {NULL, 2};
        double pts = video_out->next_pts;
        if (!video_out->config_ok || !vf->priv->ass_priv) return CONTROL_FALSE;
        if (sub_visibility && vf->priv->ass_priv && ass_track && (pts != MP_NOPTS_VALUE)) {
            mp_eosd_res_t res;
            memset(&res, 0, sizeof(res));
            if (vo_control(video_out, VOCTRL_GET_EOSD_RES, &res) == VO_TRUE) {
                ass_set_frame_size(vf->priv->ass_priv, res.w, res.h);
                ass_set_margins(vf->priv->ass_priv, res.mt, res.mb, res.ml, res.mr);
                ass_set_aspect_ratio(vf->priv->ass_priv, vf->priv->scale_ratio, 1);
            }

            images.imgs = ass_mp_render_frame(vf->priv->ass_priv, ass_track, (pts+sub_delay) * 1000 + .5, &images.changed);
            if (!vf->priv->prev_visibility)
                images.changed = 2;
            vf->priv->prev_visibility = 1;
        } else
            vf->priv->prev_visibility = 0;
        vf->priv->prev_visibility = sub_visibility;
        return vo_control(video_out, VOCTRL_DRAW_EOSD, &images) == VO_TRUE;
    }
#endif
    }
    return CONTROL_UNKNOWN;
}

static int query_format(struct vf_instance *vf, unsigned int fmt){
    int flags = vo_control(video_out, VOCTRL_QUERY_FORMAT, &fmt);
    // draw_slice() accepts stride, draw_frame() doesn't:
    if(flags)
	if(fmt==IMGFMT_YV12 || fmt==IMGFMT_I420 || fmt==IMGFMT_IYUV)
	    flags|=VFCAP_ACCEPT_STRIDE;
    return flags;
}

static void get_image(struct vf_instance *vf,
        mp_image_t *mpi){
    if (!video_out->config_ok)
        return;
    // GET_IMAGE is required for hardware-accelerated formats
    if(vo_directrendering ||
       IMGFMT_IS_XVMC(mpi->imgfmt) || IMGFMT_IS_VDPAU(mpi->imgfmt))
	vo_control(video_out, VOCTRL_GET_IMAGE, mpi);
}

static int put_image(struct vf_instance *vf,
        mp_image_t *mpi, double pts){
  if(!video_out->config_ok) return 0; // vo not configured?
  // first check, maybe the vo/vf plugin implements draw_image using mpi:
  if (vo_draw_image(video_out, mpi, pts) >= 0)
      return 1;
  // nope, fallback to old draw_frame/draw_slice:
  if(!(mpi->flags&(MP_IMGFLAG_DIRECT|MP_IMGFLAG_DRAW_CALLBACK))){
    // blit frame:
//    if(mpi->flags&MP_IMGFLAG_PLANAR)
    if(vf->default_caps&VFCAP_ACCEPT_STRIDE)
        vo_draw_slice(video_out, mpi->planes,mpi->stride,mpi->w,mpi->h,mpi->x,mpi->y);
    else
        vo_draw_frame(video_out, mpi->planes);
  }
  return 1;
}

static void start_slice(struct vf_instance *vf,
		       mp_image_t *mpi) {
    if(!video_out->config_ok) return; // vo not configured?
    vo_control(video_out, VOCTRL_START_SLICE,mpi);
}

static void draw_slice(struct vf_instance *vf,
        unsigned char** src, int* stride, int w,int h, int x, int y){
    if(!video_out->config_ok) return; // vo not configured?
    vo_draw_slice(video_out, src,stride,w,h,x,y);
}

static void uninit(struct vf_instance *vf)
{
    if (vf->priv) {
        /* Allow VO (which may live on to work with another instance of vf_vo)
         * to get rid of numbered-mpi references that will now be invalid. */
        vo_seek_reset(video_out);
#ifdef CONFIG_ASS
        if (vf->priv->ass_priv)
            ass_renderer_done(vf->priv->ass_priv);
#endif
        free(vf->priv);
    }
}
//===========================================================================//

static int vf_open(vf_instance_t *vf, char *args){
    vf->config=config;
    vf->control=control;
    vf->query_format=query_format;
    vf->get_image=get_image;
    vf->put_image=put_image;
    vf->draw_slice=draw_slice;
    vf->start_slice=start_slice;
    vf->uninit=uninit;
    vf->priv=calloc(1, sizeof(struct vf_priv_s));
    vf->priv->vo = (struct vo *)args;
    if(!video_out) return 0; // no vo ?
    return 1;
}

const vf_info_t vf_info_vo = {
    "libvo wrapper",
    "vo",
    "A'rpi",
    "for internal use",
    vf_open,
    NULL
};

//===========================================================================//
