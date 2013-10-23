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
#include "mpvcore/mp_msg.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"

#include "mpvcore/m_option.h"

static struct vf_priv_s {
    int fmt;
} const vf_priv_dflt = {
  IMGFMT_420P
};

//===========================================================================//

static int query_format(struct vf_instance *vf, unsigned int fmt){
    if(fmt!=vf->priv->fmt)
	return vf_next_query_format(vf,fmt);
    return 0;
}

static int vf_open(vf_instance_t *vf, char *args){
    vf->query_format=query_format;
    return 1;
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
    OPT_IMAGEFORMAT("fmt", fmt, 0),
    {0}
};

const vf_info_t vf_info_noformat = {
    .description = "disallow one output format",
    .name = "noformat",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .priv_defaults = &vf_priv_dflt,
    .options = vf_opts_fields,
};

//===========================================================================//
