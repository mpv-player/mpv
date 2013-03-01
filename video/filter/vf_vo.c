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
#include "core/mp_msg.h"
#include "core/options.h"

#include "video/mp_image.h"
#include "vf.h"

#include "video/out/vo.h"

#include "sub/sub.h"

struct vf_priv_s {
    struct vo *vo;
};
#define video_out (vf->priv->vo)

static int config(struct vf_instance *vf,
                  int width, int height, int d_width, int d_height,
                  unsigned int flags, unsigned int outfmt)
{

    if ((width <= 0) || (height <= 0) || (d_width <= 0) || (d_height <= 0)) {
        mp_msg(MSGT_CPLAYER, MSGL_ERR, "VO: invalid dimensions!\n");
        return 0;
    }

    const vo_info_t *info = video_out->driver->info;
    mp_msg(MSGT_CPLAYER, MSGL_INFO, "VO: [%s] %dx%d => %dx%d %s %s%s\n",
           info->short_name,
           width, height,
           d_width, d_height,
           vo_format_name(outfmt),
           (flags & VOFLAG_FULLSCREEN) ? " [fs]" : "",
           (flags & VOFLAG_FLIPPING) ? " [flip]" : "");
    mp_msg(MSGT_CPLAYER, MSGL_V, "VO: Description: %s\n", info->name);
    mp_msg(MSGT_CPLAYER, MSGL_V, "VO: Author: %s\n", info->author);
    if (info->comment && strlen(info->comment) > 0)
        mp_msg(MSGT_CPLAYER, MSGL_V, "VO: Comment: %s\n", info->comment);

    if (vo_config(video_out, width, height, d_width, d_height, flags, outfmt))
        return 0;

    return 1;
}

static int control(struct vf_instance *vf, int request, void *data)
{
    switch (request) {
    case VFCTRL_GET_DEINTERLACE:
        return vo_control(video_out, VOCTRL_GET_DEINTERLACE, data) == VO_TRUE;
    case VFCTRL_SET_DEINTERLACE:
        return vo_control(video_out, VOCTRL_SET_DEINTERLACE, data) == VO_TRUE;
    case VFCTRL_GET_YUV_COLORSPACE:
        return vo_control(video_out, VOCTRL_GET_YUV_COLORSPACE, data) == true;
    case VFCTRL_SET_YUV_COLORSPACE:
        return vo_control(video_out, VOCTRL_SET_YUV_COLORSPACE, data) == true;
    case VFCTRL_SET_EQUALIZER: {
        vf_equalizer_t *eq = data;
        if (!video_out->config_ok)
            return CONTROL_FALSE;                       // vo not configured?
        struct voctrl_set_equalizer_args param = {
            eq->item, eq->value
        };
        return vo_control(video_out, VOCTRL_SET_EQUALIZER, &param) == VO_TRUE;
    }
    case VFCTRL_GET_EQUALIZER: {
        vf_equalizer_t *eq = data;
        if (!video_out->config_ok)
            return CONTROL_FALSE;                       // vo not configured?
        struct voctrl_get_equalizer_args param = {
            eq->item, &eq->value
        };
        return vo_control(video_out, VOCTRL_GET_EQUALIZER, &param) == VO_TRUE;
    }
    case VFCTRL_HWDEC_DECODER_RENDER:
        return vo_control(video_out, VOCTRL_HWDEC_DECODER_RENDER, data);
    case VFCTRL_HWDEC_ALLOC_SURFACE:
        return vo_control(video_out, VOCTRL_HWDEC_ALLOC_SURFACE, data);
    }
    return CONTROL_UNKNOWN;
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    return video_out->driver->query_format(video_out, fmt);
}

static void uninit(struct vf_instance *vf)
{
    if (vf->priv) {
        /* Allow VO (which may live on to work with another instance of vf_vo)
         * to get rid of numbered-mpi references that will now be invalid. */
        vo_seek_reset(video_out);
        free(vf->priv);
    }
}

static int vf_open(vf_instance_t *vf, char *args)
{
    vf->config = config;
    vf->control = control;
    vf->query_format = query_format;
    vf->uninit = uninit;
    vf->priv = calloc(1, sizeof(struct vf_priv_s));
    vf->priv->vo = (struct vo *)args;
    if (!video_out)
        return 0;
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
