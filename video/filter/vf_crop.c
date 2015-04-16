/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "common/msg.h"
#include "options/options.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"

#include "options/m_option.h"

static const struct vf_priv_s {
    int crop_w,crop_h;
    int crop_x,crop_y;
} vf_priv_dflt = {
  -1,-1,
  -1,-1
};

//===========================================================================//

static int config(struct vf_instance *vf,
        int width, int height, int d_width, int d_height,
        unsigned int flags, unsigned int outfmt)
{
    // calculate the missing parameters:
    if(vf->priv->crop_w<=0 || vf->priv->crop_w>width) vf->priv->crop_w=width;
    if(vf->priv->crop_h<=0 || vf->priv->crop_h>height) vf->priv->crop_h=height;
    if(vf->priv->crop_x<0) vf->priv->crop_x=(width-vf->priv->crop_w)/2;
    if(vf->priv->crop_y<0) vf->priv->crop_y=(height-vf->priv->crop_h)/2;
    // rounding:

    struct mp_imgfmt_desc fmt = mp_imgfmt_get_desc(outfmt);

    vf->priv->crop_x = MP_ALIGN_DOWN(vf->priv->crop_x, fmt.align_x);
    vf->priv->crop_y = MP_ALIGN_DOWN(vf->priv->crop_y, fmt.align_y);

    // check:
    if(vf->priv->crop_w+vf->priv->crop_x>width ||
       vf->priv->crop_h+vf->priv->crop_y>height){
        MP_WARN(vf, "Bad position/width/height - cropped area outside of the original!\n");
        return 0;
    }
    vf_rescale_dsize(&d_width, &d_height, width, height,
                     vf->priv->crop_w, vf->priv->crop_h);
    return vf_next_config(vf,vf->priv->crop_w,vf->priv->crop_h,d_width,d_height,flags,outfmt);
}

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
    mp_image_crop(mpi, vf->priv->crop_x, vf->priv->crop_y,
                  vf->priv->crop_x + vf->priv->crop_w,
                  vf->priv->crop_y + vf->priv->crop_h);
    return mpi;
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    if (!IMGFMT_IS_HWACCEL(fmt))
        return vf_next_query_format(vf, fmt);
    return 0;
}

static int vf_open(vf_instance_t *vf){
    vf->config=config;
    vf->filter=filter;
    vf->query_format=query_format;
    return 1;
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
    OPT_INT("w", crop_w, M_OPT_MIN, .min = 0),
    OPT_INT("h", crop_h, M_OPT_MIN, .min = 0),
    OPT_INT("x", crop_x, M_OPT_MIN, .min = -1),
    OPT_INT("y", crop_y, M_OPT_MIN, .min = -1),
    {0}
};

const vf_info_t vf_info_crop = {
    .description = "cropping",
    .name = "crop",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .priv_defaults = &vf_priv_dflt,
    .options = vf_opts_fields,
};

//===========================================================================//
