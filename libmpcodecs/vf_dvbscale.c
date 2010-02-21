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

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

struct vf_priv_s {
    int aspect;
};

//===========================================================================//

static int config(struct vf_instance *vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){

    int scaled_y=vf->priv->aspect*d_height/d_width;

    d_width=width; // do X-scaling by hardware
    d_height=scaled_y;

    return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static int vf_open(vf_instance_t *vf, char *args){
    vf->config=config;
    vf->default_caps=0;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    vf->priv->aspect=768;
    if(args) vf->priv->aspect=atoi(args);
    return 1;
}

const vf_info_t vf_info_dvbscale = {
    "calc Y scaling for DVB card",
    "dvbscale",
    "A'rpi",
    "",
    vf_open,
    NULL
};

//===========================================================================//
