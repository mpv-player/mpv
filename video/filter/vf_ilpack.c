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
#include <libavutil/attributes.h>

#include "config.h"
#include "common/msg.h"
#include "options/m_option.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"

typedef void (pack_func_t)(unsigned char *dst, unsigned char *y,
    unsigned char *u, unsigned char *v, int w, int us, int vs);

struct vf_priv_s {
    int mode;
    pack_func_t *pack[2];
};

static void pack_nn(unsigned char *dst, unsigned char *y,
    unsigned char *u, unsigned char *v, int w,
    int av_unused us, int av_unused vs)
{
    int j;
    for (j = w/2; j; j--) {
        *dst++ = *y++;
        *dst++ = *u++;
        *dst++ = *y++;
        *dst++ = *v++;
    }
}

static void pack_li_0(unsigned char *dst, unsigned char *y,
    unsigned char *u, unsigned char *v, int w, int us, int vs)
{
    int j;
    for (j = w/2; j; j--) {
        *dst++ = *y++;
        *dst++ = (u[us+us] + 7*u[0])>>3;
        *dst++ = *y++;
        *dst++ = (v[vs+vs] + 7*v[0])>>3;
        u++; v++;
    }
}

static void pack_li_1(unsigned char *dst, unsigned char *y,
    unsigned char *u, unsigned char *v, int w, int us, int vs)
{
    int j;
    for (j = w/2; j; j--) {
        *dst++ = *y++;
        *dst++ = (3*u[us+us] + 5*u[0])>>3;
        *dst++ = *y++;
        *dst++ = (3*v[vs+vs] + 5*v[0])>>3;
        u++; v++;
    }
}

static void ilpack(unsigned char *dst, unsigned char *src[3],
    int dststride, int srcstride[3], int w, int h, pack_func_t *pack[2])
{
    int i;
    unsigned char *y, *u, *v;
    int ys = srcstride[0], us = srcstride[1], vs = srcstride[2];
    int a, b;

    y = src[0];
    u = src[1];
    v = src[2];

    pack_nn(dst, y, u, v, w, 0, 0);
    y += ys; dst += dststride;
    pack_nn(dst, y, u+us, v+vs, w, 0, 0);
    y += ys; dst += dststride;
    for (i=2; i<h-2; i++) {
        a = (i&2) ? 1 : -1;
        b = (i&1) ^ ((i&2)>>1);
        pack[b](dst, y, u, v, w, us*a, vs*a);
        y += ys;
        if ((i&3) == 1) {
            u -= us;
            v -= vs;
        } else {
            u += us;
            v += vs;
        }
        dst += dststride;
    }
    pack_nn(dst, y, u, v, w, 0, 0);
    y += ys; dst += dststride; u += us; v += vs;
    pack_nn(dst, y, u, v, w, 0, 0);
}


static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
    mp_image_t *dmpi = vf_alloc_out_image(vf);
    mp_image_copy_attributes(dmpi, mpi);


    ilpack(dmpi->planes[0], mpi->planes, dmpi->stride[0], mpi->stride, mpi->w, mpi->h, vf->priv->pack);

    talloc_free(mpi);
    return dmpi;
}

static int config(struct vf_instance *vf,
          int width, int height, int d_width, int d_height,
          unsigned int flags, unsigned int outfmt)
{
    /* FIXME - also support UYVY output? */
    return vf_next_config(vf, width, height, d_width, d_height, flags, IMGFMT_YUYV);
}


static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    /* FIXME - really any YUV 4:2:0 input format should work */
    switch (fmt) {
    case IMGFMT_420P:
        return vf_next_query_format(vf,IMGFMT_YUYV);
    }
    return 0;
}

static int vf_open(vf_instance_t *vf)
{
    vf->config=config;
    vf->query_format=query_format;
    vf->filter=filter;

    switch(vf->priv->mode) {
    case 0:
        vf->priv->pack[0] = vf->priv->pack[1] = pack_nn;
        break;
    case 1:
        vf->priv->pack[0] = pack_li_0;
        vf->priv->pack[1] = pack_li_1;
        break;
    }

    return 1;
}

#define OPT_BASE_STRUCT struct vf_priv_s
const vf_info_t vf_info_ilpack = {
    .description = "4:2:0 planar -> 4:2:2 packed reinterlacer",
    .name = "ilpack",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .priv_defaults = &(const struct vf_priv_s){
        .mode = 1,
    },
    .options = (const struct m_option[]){
        OPT_INTRANGE("mode", mode, 0, 0, 1),
        {0}
    },
};
