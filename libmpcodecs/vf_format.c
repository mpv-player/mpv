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
  IMGFMT_YUY2
};

//===========================================================================//

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    if(fmt==vf->priv->fmt)
	return vf_next_query_format(vf,fmt);
    return 0;
}

static int open(vf_instance_t *vf, char* args){
    vf->query_format=query_format;
    vf->default_caps=0;
    return 1;
}

#define ST_OFF(f) M_ST_OFF(struct vf_priv_s,f)
static m_option_t vf_opts_fields[] = {
  {"fmt", ST_OFF(fmt), CONF_TYPE_IMGFMT, 0,0 ,0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};

static m_struct_t vf_opts = {
  "format",
  sizeof(struct vf_priv_s),
  &vf_priv_dflt,
  vf_opts_fields
};

const vf_info_t vf_info_format = {
    "force output format",
    "format",
    "A'rpi",
    "FIXME! get_image()/put_image()",
    open,
    &vf_opts
};

//===========================================================================//
