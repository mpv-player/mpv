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
#include <math.h>

#include "vf_dlopen.h"

#include "filterutils.h"

/*
 * interlacing detector
 *
 * usage: -vf dlopen=./ildetect.so:<method>:<threshold>
 *
 * outputs an interlacing detection report at the end
 *
 * methods:
 *   0 = transcode 32detect (default)
 *   1 = decomb IsCombed
 *   2 = IsCombedTIVTC
 *   3 = simple average
 *
 * threshold:
 *   normalized at 1
 */

typedef struct {
    int method;
    double combing_threshold;
    double motion_threshold;
    double motion_amount;
    double yes_threshold;
    double no_threshold;
    double total_yes_threshold;
    double total_no_threshold;
    double tc_threshold;
    double decision_threshold;
    double tc_decision_threshold;
    double lastcombed;
    int numtotalframes;
    int numdecidedframes;
    int totalcombedframes;
    int numjumpingadjacentframes;
    int numdecidedadjacentframes;
    unsigned char *buffer_data;
    size_t buffer_size;
} ildetect_data_t;

static int il_config(struct vf_dlopen_context *ctx)
{
    ctx->out_height -= 4;
    ctx->out_d_height = ctx->out_height;
    return 1;
}

static int il_decision(struct vf_dlopen_context *ctx,
        int p0, int p1, int p2, int p3, int p4)
{
    ildetect_data_t *il = ctx->priv;

    // model for threshold: p0, p2, p4 = 0; p1, p3 = t
    switch (il->method) {
        case 0: { // diff-diff (transcode 32detect)
                    int d12 = p1 - p2; // t
                    int d13 = p1 - p3; // 0
                    if (abs(d12) > 15 * il->combing_threshold &&
                            abs(d13) < 10 * il->combing_threshold)
                        return 1;
                    // true for t > 15
                    break;
                }
        case 1: { // multiply (decomb IsCombed)
                    int d12 = p1 - p2; // t
                    int d32 = p3 - p2; // t
                    if (d12 * d32 > pow(il->combing_threshold, 2) * (25*25))
                        return 1;
                    // true for t > 21
                    break;
                }
        case 2: { // blur-blur (IsCombedTIVTC)
                    int b024 = p0 + 6 * p2 + p4; // 0
                    int b13 = 4 * p1 + 4 * p3; // 8t
                    if (abs(b024 - b13) > il->combing_threshold * 8 * 20)
                        return 1;
                    // true for t > 20
                    break;
                }
        case 3: { // average-average
                    int d123 = p1 + p3 - 2 * p2; // 2t
                    int d024 = p0 + p4 - 2 * p2; // 0
                    if ((abs(d123) - abs(d024)) > il->combing_threshold * 30)
                        return 1;
                    // true for t > 15
                    break;
                }
    }
    return 0;
}

static int il_put_image(struct vf_dlopen_context *ctx)
{
    ildetect_data_t *il = ctx->priv;
    unsigned int x, y;
    int first_frame = 0;

    size_t sz = ctx->inpic.planestride[0] * ctx->inpic.planeheight[0];
    if (sz != il->buffer_size) {
        il->buffer_data = realloc(il->buffer_data, sz);
        il->buffer_size = sz;
        first_frame = 1;
    }

    assert(ctx->inpic.planes == 1);
    assert(ctx->outpic[0].planes == 1);

    assert(ctx->inpic.planewidth[0] == ctx->outpic[0].planewidth[0]);
    assert(ctx->inpic.planeheight[0] == ctx->outpic[0].planeheight[0] + 4);

    if (first_frame) {
        printf("First frame\n");
        il->lastcombed = -1;
    } else {
        // detect interlacing
        // for each row of 5 pixels, compare:
        // p2 vs (p1 + p3) / 2
        // p2 vs (p0 + p4) / 2
        unsigned int totalcombedframes = 0; // add 255 per combed pixel
        unsigned int totalpixels = 0;
        for (y = 0; y < ctx->inpic.planeheight[0] - 4; ++y) {
            unsigned char *in_line =
                &ctx->inpic.plane[0][ctx->inpic.planestride[0] * y];
            unsigned char *buf_line =
                &il->buffer_data[ctx->inpic.planestride[0] * y];
            unsigned char *out_line =
                &ctx->outpic->plane[0][ctx->outpic->planestride[0] * y];
            for (x = 0; x < ctx->inpic.planewidth[0]; ++x) {
                int b2 = buf_line[x + ctx->inpic.planestride[0] * 2];
                int p0 = in_line[x];
                int p1 = in_line[x + ctx->inpic.planestride[0]];
                int p2 = in_line[x + ctx->inpic.planestride[0] * 2];
                int p3 = in_line[x + ctx->inpic.planestride[0] * 3];
                int p4 = in_line[x + ctx->inpic.planestride[0] * 4];
                int is_moving = abs(b2 - p2) > il->motion_threshold;

                if (!is_moving) {
                    out_line[x] = 128;
                    continue;
                }

                ++totalpixels;

                int combed = il_decision(ctx, p0, p1, p2, p3, p4);
                totalcombedframes += combed;
                out_line[x] = 255 * combed;
            }
        }

        double avgpixels = totalpixels / (double)
            ((ctx->inpic.planeheight[0] - 4) * ctx->inpic.planewidth[0]);

        if (avgpixels > il->motion_amount) {
            double avgcombed = totalcombedframes / (double) totalpixels;

            if (il->lastcombed >= 0) {
                if (il->lastcombed < il->no_threshold ||
                        il->lastcombed > il->yes_threshold)
                    if (avgcombed < il->no_threshold ||
                            avgcombed > il->yes_threshold)
                        ++il->numdecidedadjacentframes;
                if (il->lastcombed > il->yes_threshold &&
                        avgcombed < il->no_threshold)
                    ++il->numjumpingadjacentframes;
                if (il->lastcombed < il->no_threshold &&
                        avgcombed > il->yes_threshold)
                    ++il->numjumpingadjacentframes;
            }

            il->lastcombed = avgcombed;

            if (avgcombed > il->yes_threshold) {
                ++il->numdecidedframes;
                ++il->totalcombedframes;
            } else if (avgcombed < il->no_threshold) {
                ++il->numdecidedframes;
            }
        } else
            il->lastcombed = -1;
    }

    ++il->numtotalframes;

    copy_plane(
        il->buffer_data, ctx->inpic.planestride[0],
        ctx->inpic.plane[0], ctx->inpic.planestride[0],
        ctx->inpic.planewidth[0],
        ctx->inpic.planeheight[0]);

    ctx->outpic[0].pts = ctx->inpic.pts;
    return 1;
}

void il_uninit(struct vf_dlopen_context *ctx)
{
    ildetect_data_t *il = ctx->priv;

    double avgdecided = il->numtotalframes
        ? il->numdecidedframes / (double) il->numtotalframes : -1;
    double avgadjacent = il->numdecidedframes
        ? il->numdecidedadjacentframes / (double) il->numdecidedframes : -1;
    double avgscore = il->numdecidedframes
        ? il->totalcombedframes / (double) il->numdecidedframes : -1;
    double avgjumps = il->numdecidedadjacentframes
        ? il->numjumpingadjacentframes / (double) il->numdecidedadjacentframes : -1;

    printf("ildetect: Avg decided: %f\n", avgdecided);
    printf("ildetect: Avg adjacent decided: %f\n", avgadjacent);
    printf("ildetect: Avg interlaced decided: %f\n", avgscore);
    printf("ildetect: Avg interlaced/progressive adjacent decided: %f\n", avgjumps);

    if (avgdecided < il->decision_threshold)
        avgadjacent = avgscore = avgjumps = -1;

    if (avgadjacent < il->tc_decision_threshold)
        avgadjacent = avgjumps = -1;

    if (avgscore < 0)
        printf("ildetect: Content is probably: undecided\n");
    else if (avgscore < il->total_no_threshold)
        printf("ildetect: Content is probably: PROGRESSIVE\n");
    else if (avgscore > il->total_yes_threshold && avgjumps < 0)
        printf("ildetect: Content is probably: INTERLACED (possibly telecined)\n");
    else if (avgjumps > il->tc_threshold)
        printf("ildetect: Content is probably: TELECINED\n");
    else if (avgscore > il->total_yes_threshold)
        printf("ildetect: Content is probably: INTERLACED\n");
    else
        printf("ildetect: Content is probably: unknown\n");

    free(ctx->priv);
}

int vf_dlopen_getcontext(struct vf_dlopen_context *ctx, int argc, const char **argv)
{
    VF_DLOPEN_CHECK_VERSION(ctx);
    (void) argc;
    (void) argv;

    ildetect_data_t *il = calloc(1,sizeof(ildetect_data_t));

#define A(i,d) ((argc>(i) && *argv[i]) ? atof(argv[i]) : (d))
    il->method = A(0, 0);
    il->combing_threshold = A(1, 1);
    il->motion_threshold = A(2, 6);
    il->motion_amount = A(3, 0.1);
    il->yes_threshold = A(4, 0.1);
    il->no_threshold = A(5, 0.05);
    il->total_yes_threshold = A(6, 0.1);
    il->total_no_threshold = A(7, 0.05);
    il->tc_threshold = A(8, 0.1);
    il->decision_threshold = A(9, 0.2);
    il->tc_decision_threshold = A(10, 0.2);

    static struct vf_dlopen_formatpair map[] = {
        { "gray", "gray" },
        { NULL, NULL }
    };
    ctx->format_mapping = map;
    ctx->config = il_config;
    ctx->put_image = il_put_image;
    ctx->uninit = il_uninit;
    ctx->priv = il;
    return 1;
}
