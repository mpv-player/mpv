#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "../config.h"
#include "../mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

struct vf_priv_s {
	int w, h;
	float aspect;
};

static int config(struct vf_instance_s* vf,
	int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt)
{
	if (vf->priv->w && vf->priv->h) {
		d_width = vf->priv->w;
		d_height = vf->priv->h;
	} else {
		if (vf->priv->aspect * height > width) {
			d_width = height * vf->priv->aspect;
			d_height = height;
		} else {
			d_height = width / vf->priv->aspect;
			d_width = width;
		}
	}
	return vf_next_config(vf, width, height, d_width, d_height, flags, outfmt);
}

static int open(vf_instance_t *vf, char* args)
{
	vf->config = config;
	vf->draw_slice = vf_next_draw_slice;
	//vf->default_caps = 0;
	vf->priv = calloc(sizeof(struct vf_priv_s), 1);
	vf->priv->aspect = 4.0/3.0;
	if (args) {
		if (strchr(args, '/')) {
			int w, h;
			sscanf(args, "%d/%d", &w, &h);
			vf->priv->aspect = (float)w/h;
		} else if (strchr(args, '.')) {
			sscanf(args, "%f", &vf->priv->aspect);
		} else {
			sscanf(args, "%d:%d", &vf->priv->w, &vf->priv->h);
		}
	}
	return 1;
}

vf_info_t vf_info_dsize = {
    "reset displaysize/aspect",
    "dsize",
    "Rich Felker",
    "",
    open,
    NULL
};

