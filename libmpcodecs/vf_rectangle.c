#include <stdio.h>
#include <stdlib.h>
#include "mp_image.h"
#include "vf.h"

struct vf_priv_s {
    int x, y, w, h;
};

static void
put_image(struct vf_instance_s* vf, mp_image_t* mpi){
    mp_image_t* dmpi;
    int x, y, w, h;
    unsigned int bpp, count;
    unsigned char *p1, *p2;
    dmpi = vf_get_image(vf->next, mpi->imgfmt, MP_IMGTYPE_TEMP, 0, mpi->w, mpi->h);
    bpp = dmpi->bpp / 8;
    memcpy(dmpi->planes[0], mpi->planes[0], dmpi->stride[0] * bpp * mpi->height);
    memcpy(dmpi->planes[1], mpi->planes[1], dmpi->stride[1] * mpi->chroma_height);
    memcpy(dmpi->planes[2], mpi->planes[2], dmpi->stride[2] * mpi->chroma_height);

    /* Draw the rectangle */
    x = vf->priv->x;
    if (x < 0)
	x = 0;
    y = vf->priv->y;
    if (y < 0)
	y = 0;
    w = vf->priv->w;
    if (w < 0)
	w = dmpi->w - x;
    h = vf->priv->h;
    if (h < 0)
	h = dmpi->h - y;
    count = w * bpp;
    p1 = dmpi->planes[0] + y * dmpi->stride[0] + x * bpp;
    if (h == 1)
	while (count--) {
	    *p1 = 0xff - *p1;
	    ++p1;
	}
    else {
	p2 = p1 + (h - 1) * dmpi->stride[0];
	while (count--) {
	    *p1 = 0xff - *p1;
	    ++p1;
	    *p2 = 0xff - *p2;
	    ++p2;
	}
    }
    count = h;
    p1 = dmpi->planes[0] + y * dmpi->stride[0] + x * bpp;
    if (w == 1)
	while (count--) {
	    int i = bpp;
	    while (i--)
		p1[i] ^= 0xff;
	    p1 += dmpi->stride[0];
	}
    else {
	p2 = p1 + (w - 1) * bpp;
	while (count--) {
	    int i = bpp;
	    while (i--) {
		p1[i] = 0xff - p1[i];
		p2[i] = 0xff - p2[i];
	    }
	    p1 += dmpi->stride[0];
	    p2 += dmpi->stride[0];
	}
    }
    vf_next_put_image(vf, dmpi);
}

static int
open(vf_instance_t* vf, char* args) {
    vf->put_image = put_image;
    vf->priv = malloc(sizeof(struct vf_priv_s));
    vf->priv->x = -1;
    vf->priv->y = -1;
    vf->priv->w = -1;
    vf->priv->h = -1;
    if (args)
	sscanf(args, "%d:%d:%d:%d", 
	       &vf->priv->w, &vf->priv->h, &vf->priv->x, &vf->priv->y);
    printf("Crop: %d x %d, %d ; %d\n",
	   vf->priv->w, vf->priv->h, vf->priv->x, vf->priv->y);
    return 1;
}

vf_info_t vf_info_rectangle = {
    "draw rectangle",
    "rectangle",
    "Kim Minh Kaplan",
    "",
    open
};
