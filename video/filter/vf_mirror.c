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

static int config(struct vf_instance *vf, int width, int height,
                  int d_width, int d_height,
                  unsigned int flags, unsigned int fmt)
{
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
    int a_w = MP_ALIGN_DOWN(width, desc.align_x);
    vf_rescale_dsize(&d_width, &d_height, width, height, a_w, height);
    return vf_next_config(vf, a_w, height, d_width, d_height, flags, fmt);
}

static inline void mirror_4_m(uint8_t *dst, uint8_t *src, int p,
                              int c0, int c1, int c2, int c3)
{
    for (int x = 0; x < p; x++) {
        dst[x * 4 + 0] = src[(p - x - 1) * 4 + c0];
        dst[x * 4 + 1] = src[(p - x - 1) * 4 + c1];
        dst[x * 4 + 2] = src[(p - x - 1) * 4 + c2];
        dst[x * 4 + 3] = src[(p - x - 1) * 4 + c3];
    }
}

static inline void mirror(uint8_t *dst, uint8_t *src, int bp, int w)
{
    for (int x = 0; x < w; x++)
        memcpy(dst + x * bp, src + (w - x - 1) * bp, bp);
}

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
    mp_image_t *dmpi = vf_alloc_out_image(vf);
    mp_image_copy_attributes(dmpi, mpi);

    for (int p = 0; p < mpi->num_planes; p++) {
        for (int y = 0; y < mpi->plane_h[p]; y++) {
            void *p_src = mpi->planes[p] + mpi->stride[p] * y;
            void *p_dst = dmpi->planes[p] + dmpi->stride[p] * y;
            int w = dmpi->plane_w[p];
            if (mpi->imgfmt == IMGFMT_YUYV) {
                mirror_4_m(p_dst, p_src, w / 2, 2, 1, 0, 3);
            } else if (mpi->imgfmt == IMGFMT_UYVY) {
                mirror_4_m(p_dst, p_src, w / 2, 0, 3, 2, 1);
            } else {
                // make the compiler unroll the memcpy in mirror()
                switch (mpi->fmt.bytes[p]) {
                case 1: mirror(p_dst, p_src, 1, w); break;
                case 2: mirror(p_dst, p_src, 2, w); break;
                case 3: mirror(p_dst, p_src, 3, w); break;
                case 4: mirror(p_dst, p_src, 4, w); break;
                default:
                    mirror(p_dst, p_src, mpi->fmt.bytes[p], w);
                }
            }
        }
    }

    talloc_free(mpi);
    return dmpi;
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
    if (!(desc.flags & MP_IMGFLAG_BYTE_ALIGNED))
        return 0;
    return vf_next_query_format(vf, fmt);
}

static int vf_open(vf_instance_t *vf, char *args){
    vf->config=config;
    vf->filter=filter;
    vf->query_format=query_format;
    return 1;
}

const vf_info_t vf_info_mirror = {
    .description = "horizontal mirror",
    .name = "mirror",
    .open = vf_open,
};

//===========================================================================//
