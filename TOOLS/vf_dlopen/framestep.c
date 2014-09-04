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
#include "filterutils.h"

#define MIN(a,b) ((a)<(b)?(a):(b))

/*
 * frame stepping filter
 *
 * usage: --vf=dlopen=/path/to/framestep.so:5
 *
 * outputs every 5th frame
 *
 * usage: --vf=dlopen=/path/to/framestep.so:5:3
 *
 * outputs every 5th frame, starting with frame index 3 (default: 0)
 */

typedef struct {
    int step, pos;
} framestep_data_t;

static int framestep_put_image(struct vf_dlopen_context *ctx)
{
    framestep_data_t *framestep = ctx->priv;

    // stepping
    if (framestep->pos < 0)
        return 0;
    --framestep->pos;
    if (framestep->pos >= 0)
        return 0;
    framestep->pos += framestep->step;

    // copying
    assert(ctx->inpic.planes == ctx->outpic[0].planes);
    int np = ctx->inpic.planes;
    int p;
    for (p = 0; p < np; ++p) {
        assert(ctx->inpic.planewidth[p] == ctx->outpic->planewidth[p]);
        assert(ctx->inpic.planeheight[p] == ctx->outpic->planeheight[p]);
        copy_plane(
            ctx->outpic->plane[p],
            ctx->outpic->planestride[p],
            ctx->inpic.plane[p],
            ctx->inpic.planestride[p],
            MIN(ctx->inpic.planestride[p], ctx->outpic->planestride[p]),
            ctx->inpic.planeheight[p]
            );
    }
    ctx->outpic->pts = ctx->inpic.pts;

    return 1;
}

void framestep_uninit(struct vf_dlopen_context *ctx)
{
    free(ctx->priv);
}

int vf_dlopen_getcontext(struct vf_dlopen_context *ctx, int argc, const char **argv)
{
    VF_DLOPEN_CHECK_VERSION(ctx);

    if (argc != 1 && argc != 2)
        return -1;

    framestep_data_t *framestep = calloc(1,sizeof(framestep_data_t));

    framestep->step = atoi(argv[0]);
    framestep->pos = (argc >= 2) ? atoi(argv[1]) : 0;

    ctx->priv = framestep;
    ctx->put_image = framestep_put_image;
    ctx->uninit = framestep_uninit;

    return 1;
}
