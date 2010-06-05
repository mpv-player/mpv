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
#include "help_mp.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "m_option.h"
#include "m_struct.h"

static struct vf_priv_s {
    unsigned int fmt;
} const vf_priv_dflt = {
  IMGFMT_YV12
};

//===========================================================================//

static int query_format(struct vf_instance *vf, unsigned int fmt){
    if(fmt!=vf->priv->fmt)
	return vf_next_query_format(vf,fmt);
    return 0;
}

static int vf_open(vf_instance_t *vf, char *args){
    vf->query_format=query_format;
    vf->draw_slice=vf_next_draw_slice;
    vf->default_caps=0;
    return 1;
}

#define ST_OFF(f) M_ST_OFF(struct vf_priv_s,f)
static const m_option_t vf_opts_fields[] = {
  {"fmt", ST_OFF(fmt), CONF_TYPE_IMGFMT, 0,0 ,0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};

static const m_struct_t vf_opts = {
  "noformat",
  sizeof(struct vf_priv_s),
  &vf_priv_dflt,
  vf_opts_fields
};

const vf_info_t vf_info_noformat = {
    "disallow one output format",
    "noformat",
    "Joey",
    "",
    vf_open,
    &vf_opts
};

//===========================================================================//
