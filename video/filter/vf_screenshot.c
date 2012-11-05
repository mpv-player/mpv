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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "core/mp_msg.h"
#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"
#include "video/sws_utils.h"
#include "video/fmt-conversion.h"
#include "video/memcpy_pic.h"

#include <libswscale/swscale.h>

struct vf_priv_s {
    int display_w, display_h;
    void (*image_callback)(void *, mp_image_t *);
    void *image_callback_ctx;
    int shot;
};

//===========================================================================//

static int config(struct vf_instance *vf,
                  int width, int height, int d_width, int d_height,
                  unsigned int flags, unsigned int outfmt)
{
    vf->priv->display_w = d_width;
    vf->priv->display_h = d_height;
    return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
    if(vf->priv->shot) {
        vf->priv->shot=0;
        mp_image_t image = *mpi;
        image.flags &= ~MP_IMGFLAG_ALLOCATED;
        mp_image_copy_attributes(&image, mpi);
        mp_image_set_display_size(&image, vf->priv->display_w,
                                  vf->priv->display_h);
        vf->priv->image_callback(vf->priv->image_callback_ctx, &image);
    }

    return mpi;
}

static int control (vf_instance_t *vf, int request, void *data)
{
    if(request==VFCTRL_SCREENSHOT) {
        struct vf_ctrl_screenshot *cmd = (struct vf_ctrl_screenshot *)data;
        vf->priv->image_callback = cmd->image_callback;
        vf->priv->image_callback_ctx = cmd->image_callback_ctx;
        vf->priv->shot=1;
        return CONTROL_TRUE;
    }
    return vf_next_control (vf, request, data);
}


//===========================================================================//

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    enum PixelFormat av_format = imgfmt2pixfmt(fmt);

    if (av_format != PIX_FMT_NONE && sws_isSupportedInput(av_format))
        return vf_next_query_format(vf, fmt);
    return 0;
}

static void uninit(vf_instance_t *vf)
{
    free(vf->priv);
}

static int vf_open(vf_instance_t *vf, char *args)
{
    vf->config=config;
    vf->control=control;
    vf->filter=filter;
    vf->query_format=query_format;
    vf->uninit=uninit;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    vf->priv->shot=0;
    return 1;
}

const vf_info_t vf_info_screenshot = {
    "screenshot to file",
    "screenshot",
    "A'rpi, Jindrich Makovicka",
    "",
    vf_open,
    NULL
};

// screenshot.c will look for a filter named "screenshot_force", and not use
// the VO based screenshot code if it's in the filter chain.
const vf_info_t vf_info_screenshot_force = {
    "screenshot to file (override VO based screenshot code)",
    "screenshot_force",
    "A'rpi, Jindrich Makovicka",
    "",
    vf_open,
    NULL
};

//===========================================================================//
