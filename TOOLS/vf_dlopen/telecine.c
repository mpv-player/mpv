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
 * telecine filter
 *
 * usage: --vf=dlopen=/path/to/telecine.so:t:23
 *
 * Parameter: first parameter is "t" for top field first, "b" for bottom field first
 * then digits (0-9) for how many fields a frame is to be displayed
 *
 * Typical patterns (see http://en.wikipedia.org/wiki/Telecine):
 *
 * NTSC output (30i):
 * 27.5p: 32222
 * 24p: 23 (classic)
 * 24p: 2332 (preferred)
 * 20p: 33
 * 18p: 334
 * 16p: 3444
 *
 * PAL output (25i):
 * 27.5p: 12222
 * 24p: 222222222223 ("Euro pulldown")
 * 16.67p: 33
 * 16p: 33333334
 */

typedef struct {
    int firstfield;
    const char *pattern;
    unsigned int pattern_pos;
    unsigned char *buffer_plane[4];
    size_t buffer_size[4];
    int pts_num;
    int pts_denom;
    int occupied;
    double lastpts_in;
    double lastpts_out;
    int first_frame_of_group;
} tc_data_t;

static int tc_config(struct vf_dlopen_context *ctx)
{
    // we may return more than one pic!
    tc_data_t *tc = ctx->priv;
    const char *p;
    int max = 0;
    tc->pts_num = 0;
    tc->pts_denom = 0;
    for (p = tc->pattern; *p; ++p) {
        if (*p - '0' > max)
            max = *p - '0';
        tc->pts_num += 2;
        tc->pts_denom += *p - '0';
    }
    ctx->out_cnt = (max + 1) / 2;
    printf(
        "Telecine pattern %s yields up to %d frames per frame, pts advance factor: %d/%d\n",
        tc->pattern, ctx->out_cnt, tc->pts_num, tc->pts_denom);
    return 1;
}

static int tc_put_image(struct vf_dlopen_context *ctx)
{
    tc_data_t *tc = ctx->priv;

    unsigned p;
    unsigned np = ctx->outpic[0].planes;
    assert(ctx->inpic.planes == ctx->outpic[0].planes);

    int need_reinit = 0;

    // fix buffers
    for (p = 0; p < np; ++p) {
        size_t sz = ctx->inpic.planestride[p] * ctx->inpic.planeheight[p];
        if (sz != tc->buffer_size[p]) {
            if (p == 0 && tc->buffer_plane[p])
                printf("WARNING: reinitializing telecine buffers.\n");
            tc->buffer_plane[p] = realloc(tc->buffer_plane[p], sz);
            tc->buffer_size[p] = sz;
            need_reinit = 1;
        }
    }

    // too big pts change? reinit
    if (ctx->inpic.pts < tc->lastpts_in || ctx->inpic.pts > tc->lastpts_in + 0.5)
        need_reinit = 1;

    if (need_reinit) {
        // initialize telecine
        tc->pattern_pos = 0;
        tc->occupied = 0;
        tc->lastpts_in = ctx->inpic.pts;
        tc->lastpts_out = ctx->inpic.pts;
    }

    int len = tc->pattern[tc->pattern_pos] - '0';
    unsigned nout;
    double delta = ctx->inpic.pts - tc->lastpts_in;
    tc->lastpts_in = ctx->inpic.pts;

    for (nout = 0; nout < ctx->out_cnt; ++nout) {
        for (p = 0; p < np; ++p) {
            assert(ctx->inpic.planewidth[p] == ctx->outpic[nout].planewidth[p]);
            assert(ctx->inpic.planeheight[p] == ctx->outpic[nout].planeheight[p]);
        }
    }
    nout = 0;

    if (tc->pattern_pos == 0 && !tc->occupied) {
        // at the start of the pattern, reset pts
        // printf("pts reset: %f -> %f (delta: %f)\n", tc->lastpts_out, ctx->inpic.pts, ctx->inpic.pts - tc->lastpts_out);
        tc->lastpts_out = ctx->inpic.pts;
        tc->first_frame_of_group = 1;
    }
    ++tc->pattern_pos;
    if (!tc->pattern[tc->pattern_pos])
        tc->pattern_pos = 0;

    if (len == 0) {
        // do not output any field from this frame
        return 0;
    }

    if (tc->occupied) {
        for (p = 0; p < np; ++p) {
            // fill in the EARLIER field from the buffered pic
            copy_plane(
                &ctx->outpic[nout].plane[p][ctx->outpic[nout].planestride[p] * tc->firstfield],
                ctx->outpic[nout].planestride[p] * 2,
                &tc->buffer_plane[p][ctx->inpic.planestride[p] * tc->firstfield],
                ctx->inpic.planestride[p] * 2,
                MIN(ctx->inpic.planestride[p], ctx->outpic[nout].planestride[p]),
                (ctx->inpic.planeheight[p] - tc->firstfield + 1) / 2
                );
            // fill in the LATER field from the new pic
            copy_plane(
                &ctx->outpic[nout].plane[p][ctx->outpic[nout].planestride[p] * !tc->firstfield],
                ctx->outpic[nout].planestride[p] * 2,
                &ctx->inpic.plane[p][ctx->inpic.planestride[p] * !tc->firstfield],
                ctx->inpic.planestride[p] * 2,
                MIN(ctx->inpic.planestride[p], ctx->outpic[nout].planestride[p]),
                (ctx->inpic.planeheight[p] - !tc->firstfield + 1) / 2
                );
        }
        if (tc->first_frame_of_group)
            tc->first_frame_of_group = 0;
        else
            tc->lastpts_out += (delta * tc->pts_num) / tc->pts_denom;
        ctx->outpic[nout].pts = tc->lastpts_out;
        // printf("pts written: %f\n", ctx->outpic[nout].pts);
        ++nout;
        --len;
        tc->occupied = 0;
    }

    while (len >= 2) {
        // output THIS image as-is
        for (p = 0; p < np; ++p)
            copy_plane(
                ctx->outpic[nout].plane[p], ctx->outpic[nout].planestride[p],
                ctx->inpic.plane[p], ctx->inpic.planestride[p],
                MIN(ctx->inpic.planestride[p], ctx->outpic[nout].planestride[p]),
                ctx->inpic.planeheight[p]
                );
        if (tc->first_frame_of_group)
            tc->first_frame_of_group = 0;
        else
            tc->lastpts_out += (delta * tc->pts_num) / tc->pts_denom;
        ctx->outpic[nout].pts = tc->lastpts_out;
        // printf("pts written: %f\n", ctx->outpic[nout].pts);
        ++nout;
        len -= 2;
    }

    if (len >= 1) {
        // copy THIS image to the buffer, we need it later
        for (p = 0; p < np; ++p)
            copy_plane(
                &tc->buffer_plane[p][0], ctx->inpic.planestride[p],
                &ctx->inpic.plane[p][0], ctx->inpic.planestride[p],
                ctx->inpic.planestride[p],
                ctx->inpic.planeheight[p]
                );
        tc->occupied = 1;
    }

    return nout;
}

void tc_uninit(struct vf_dlopen_context *ctx)
{
    tc_data_t *tc = ctx->priv;
    free(tc->buffer_plane[3]);
    free(tc->buffer_plane[2]);
    free(tc->buffer_plane[1]);
    free(tc->buffer_plane[0]);
    free(tc);
}

int vf_dlopen_getcontext(struct vf_dlopen_context *ctx, int argc, const char **argv)
{
    VF_DLOPEN_CHECK_VERSION(ctx);

    const char *a0 = (argc < 1) ? "t" : argv[0];
    const char *a1 = (argc < 2) ? "23" : argv[1];

    if (!a0[0] || a0[1] || !a1[0] || argc > 2)
        return -1;

    tc_data_t *tc = calloc(1,sizeof(tc_data_t));

    if (a0[0] == 't')
        tc->firstfield = 0;
    else if (a0[0] == 'b')
        tc->firstfield = 1;
    else {
        printf("telecine: invalid first field\n");
        free(tc);
        return -1;
    }

    tc->pattern = a1;

    const char *p;
    for (p = tc->pattern; *p; ++p)
        if (*p < '0' || *p > '9') {
            printf("telecine: invalid pattern\n");
            free(tc);
            return -1;
        }

    ctx->priv = tc;
    ctx->format_mapping = NULL; // anything goes
    ctx->config = tc_config;
    ctx->put_image = tc_put_image;
    ctx->uninit = tc_uninit;

    return 1;
}
