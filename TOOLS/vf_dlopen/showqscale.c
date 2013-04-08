/*
 * Copyright (c) 2012 Rudolf Polzer <divVerent@xonotic.org>
 *
 * This file is part of mpv's vf_dlopen examples.
 *
 * mpv's vf_dlopen examples are free software; you can redistribute them and/or
 * modify them under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * mpv's vf_dlopen examples are distributed in the hope that they will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv's vf_dlopen examples; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf_dlopen.h"

/*
 * qscale visualizer
 *
 * usage: -vf dlopen=./showqscale.so
 *
 * uses reddish colors for high QPs, and greenish colors for low QPs
 */

#define PLANE_Y 0
#define PLANE_U 1
#define PLANE_V 2

static int qs_put_image(struct vf_dlopen_context *ctx)
{
    unsigned int x, y, p;

    assert(ctx->inpic.planes == ctx->outpic[0].planes);

    for (p = 0; p < ctx->outpic[0].planes; ++p) {
        assert(ctx->inpic.planewidth[p] == ctx->outpic[0].planewidth[p]);
        assert(ctx->inpic.planeheight[p] == ctx->outpic[0].planeheight[p]);
        if ((p == PLANE_U || p == PLANE_V) && ctx->inpic_qscale)
            continue;
#if 0
        // copy as is
        for (y = 0; y < ctx->outpic[0].planeheight[p]; ++y)
            memcpy(
                &ctx->outpic[0].plane[p][ctx->outpic[0].planestride[p] * y],
                &inpic[ctx->outpic[0].planeofs[p] + ctx->inpic.planestride[p] * y],
                ctx->outpic[0].planewidth[p]
                );
#else
        // reduce contrast
        for (y = 0; y < ctx->outpic[0].planeheight[p]; ++y)
            for (x = 0; x < ctx->outpic[0].planewidth[p]; ++x)
                ctx->outpic[0].plane[p][ctx->outpic[0].planestride[p] * y + x] =
                    0x20 + ((ctx->inpic.plane[p][ctx->inpic.planestride[p] * y + x] * 3) >> 2);
#endif
    }

    if (ctx->inpic_qscale) {
        int qmin = 255;
        int qmax = -255;

        // clear U plane
        p = PLANE_U;
        for (y = 0; y < ctx->outpic[0].planeheight[p]; ++y)
            memset(
                &ctx->outpic[0].plane[p][ctx->outpic[0].planestride[p] * y],
                0x80,
                ctx->outpic[0].planewidth[p]
                );

        // replace V by the qp (0 = green, 12 = red)
        p = PLANE_V;
        for (y = 0; y < ctx->outpic[0].planeheight[p]; ++y)
            for (x = 0; x < ctx->outpic[0].planewidth[p]; ++x) {
                int q = ctx->inpic_qscale[
                    (x >> (ctx->inpic_qscaleshift - ctx->inpic.planexshift[p])) +
                    (y >> (ctx->inpic_qscaleshift - ctx->inpic.planeyshift[p])) * ctx->inpic_qscalestride];
                if (q < qmin)
                    qmin = q;
                if (q > qmax)
                    qmax = q;
                int v = 128 + 21 * (q - 6); // range: 0 = green, 12 = red
                if (v < 0)
                    v = 0;
                if (v > 255)
                    v = 255;
                ctx->outpic[0].plane[p][ctx->outpic[0].planestride[p] * y + x] = v;
            }

        // printf("qscale range: %d .. %d\n", qmin, qmax);
    }

    ctx->outpic[0].pts = ctx->inpic.pts;
    return 1;
}

int vf_dlopen_getcontext(struct vf_dlopen_context *ctx, int argc, const char **argv)
{
    VF_DLOPEN_CHECK_VERSION(ctx);
    (void) argc;
    (void) argv;
    static struct vf_dlopen_formatpair map[] = {
        { "yv12", "yv12" },
        { NULL, NULL }
    };
    ctx->format_mapping = map;
    ctx->put_image = qs_put_image;
    return 1;
}
