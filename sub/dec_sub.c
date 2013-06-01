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

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "config.h"
#include "demux/stheader.h"
#include "sub/sd.h"
#include "sub/sub.h"
#include "sub/dec_sub.h"
#include "core/options.h"

extern const struct sd_functions sd_ass;
extern const struct sd_functions sd_lavc;
extern const struct sd_functions sd_spu;

static const struct sd_functions *sd_list[] = {
#ifdef CONFIG_ASS
    &sd_ass,
#endif
    &sd_lavc,
    &sd_spu,
    NULL
};

struct dec_sub {
    struct MPOpts *opts;
    struct sd init_sd;

    struct sd *sd;
};

struct dec_sub *sub_create(struct MPOpts *opts)
{
    struct dec_sub *sub = talloc_zero(NULL, struct dec_sub);
    sub->opts = opts;
    return sub;
}

void sub_destroy(struct dec_sub *sub)
{
    if (!sub)
        return;
    if (sub->sd && sub->sd->driver->uninit)
        sub->sd->driver->uninit(sub->sd);
    talloc_free(sub->sd);
    talloc_free(sub);
}

bool sub_is_initialized(struct dec_sub *sub)
{
    return !!sub->sd;
}

struct sd *sub_get_sd(struct dec_sub *sub)
{
    return sub->sd;
}

void sub_set_video_res(struct dec_sub *sub, int w, int h)
{
    sub->init_sd.sub_video_w = w;
    sub->init_sd.sub_video_h = h;
}

void sub_set_extradata(struct dec_sub *sub, void *data, int data_len)
{
    sub->init_sd.extradata = data_len ? talloc_memdup(sub, data, data_len) : NULL;
    sub->init_sd.extradata_len = data_len;
}

void sub_set_ass_renderer(struct dec_sub *sub, struct ass_library *ass_library,
                          struct ass_renderer *ass_renderer)
{
    sub->init_sd.ass_library = ass_library;
    sub->init_sd.ass_renderer = ass_renderer;
}

static int sub_init_decoder(struct dec_sub *sub, struct sd *sd)
{
    sd->driver = NULL;
    for (int n = 0; sd_list[n]; n++) {
        if (sd_list[n]->supports_format(sd->codec)) {
            sd->driver = sd_list[n];
            break;
        }
    }

    if (!sd->driver)
        return -1;

    if (sd->driver->init(sd) < 0)
        return -1;

    return 0;
}

void sub_init_from_sh(struct dec_sub *sub, struct sh_sub *sh)
{
    assert(!sub->sd);
    if (sh->extradata && !sub->init_sd.extradata)
        sub_set_extradata(sub, sh->extradata, sh->extradata_len);
    struct sd *sd = talloc(NULL, struct sd);
    *sd = sub->init_sd;
    sd->opts = sub->opts;
    sd->codec = sh->gsh->codec;
    sd->ass_track = sh->track;
    if (sub_init_decoder(sub, sd) < 0) {
        talloc_free(sd);
        sd = NULL;
    }
    sub->sd = sd;
}

bool sub_accept_packets_in_advance(struct dec_sub *sub)
{
    return sub->sd && sub->sd->driver->accept_packets_in_advance;
}

void sub_decode(struct dec_sub *sub, struct demux_packet *packet)
{
    if (sub->sd)
        sub->sd->driver->decode(sub->sd, packet);
}

void sub_get_bitmaps(struct dec_sub *sub, struct mp_osd_res dim, double pts,
                     struct sub_bitmaps *res)
{
    struct MPOpts *opts = sub->opts;

    *res = (struct sub_bitmaps) {0};
    if (sub->sd && opts->sub_visibility) {
        if (sub->sd->driver->get_bitmaps)
            sub->sd->driver->get_bitmaps(sub->sd, dim, pts, res);
    }
}

bool sub_has_get_text(struct dec_sub *sub)
{
    return sub->sd && sub->sd->driver->get_text;
}

char *sub_get_text(struct dec_sub *sub, double pts)
{
    struct MPOpts *opts = sub->opts;
    char *text = NULL;
    if (sub->sd && opts->sub_visibility) {
        if (sub->sd->driver->get_text)
            text = sub->sd->driver->get_text(sub->sd, pts);
    }
    return text;
}

void sub_reset(struct dec_sub *sub)
{
    if (sub->sd && sub->sd->driver->reset)
        sub->sd->driver->reset(sub->sd);
}
