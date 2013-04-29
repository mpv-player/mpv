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
#include <ass/ass.h>
#include <assert.h>
#include <string.h>

#include "talloc.h"

#include "core/options.h"
#include "core/mp_common.h"
#include "core/mp_msg.h"
#include "demux/stheader.h"
#include "sub.h"
#include "dec_sub.h"
#include "ass_mp.h"
#include "sd.h"
#include "subassconvert.h"

struct sd_ass_priv {
    struct ass_track *ass_track;
    bool vsfilter_aspect;
    bool incomplete_event;
    struct sub_bitmap *parts;
    bool flush_on_seek;
};

static bool probe(struct sh_sub *sh)
{
    return is_text_sub(sh->gsh->codec);
}

static void free_last_event(ASS_Track *track)
{
    assert(track->n_events > 0);
    ass_free_event(track, track->n_events - 1);
    track->n_events--;
}

static int init(struct sh_sub *sh, struct osd_state *osd)
{
    struct sd_ass_priv *ctx;
    bool ass = is_ass_sub(sh->gsh->codec);

    if (sh->initialized) {
        ctx = sh->context;
    } else {
        ctx = talloc_zero(NULL, struct sd_ass_priv);
        sh->context = ctx;
        if (ass) {
            ctx->ass_track = ass_new_track(osd->ass_library);
            if (sh->extradata)
                ass_process_codec_private(ctx->ass_track, sh->extradata,
                                          sh->extradata_len);
        } else
            ctx->ass_track = mp_ass_default_track(osd->ass_library, sh->opts);
    }

    ctx->vsfilter_aspect = ass;
    return 0;
}

static void decode(struct sh_sub *sh, struct osd_state *osd, void *data,
                   int data_len, double pts, double duration)
{
    unsigned char *text = data;
    struct sd_ass_priv *ctx = sh->context;
    ASS_Track *track = ctx->ass_track;

    if (is_ass_sub(sh->gsh->codec)) {
        if (bstr_startswith0((bstr){data, data_len}, "Dialogue: ")) {
            // broken ffmpeg ASS packet format
            ctx->flush_on_seek = true;
            ass_process_data(track, data, data_len);
        } else {
            ass_process_chunk(track, data, data_len,
                              (long long)(pts*1000 + 0.5),
                              (long long)(duration*1000 + 0.5));
        }
        return;
    }
    // plaintext subs
    if (pts == MP_NOPTS_VALUE) {
        mp_msg(MSGT_SUBREADER, MSGL_WARN, "Subtitle without pts, ignored\n");
        return;
    }
    long long ipts = pts * 1000 + 0.5;
    long long iduration = duration * 1000 + 0.5;
    if (ctx->incomplete_event) {
        ctx->incomplete_event = false;
        ASS_Event *event = track->events + track->n_events - 1;
        if (ipts <= event->Start)
            free_last_event(track);
        else
            event->Duration = ipts - event->Start;
    }
    // Note: we rely on there being guaranteed 0 bytes after data packets
    int len = strlen(text);
    if (len < 5) {
        // Some tracks use a whitespace (but not empty) packet to mark end
        // of previous subtitle.
        for (int i = 0; i < len; i++)
            if (!strchr(" \f\n\r\t\v", text[i]))
                goto not_all_whitespace;
        return;
    }
 not_all_whitespace:;
    char buf[500];
    subassconvert_subrip(text, buf, sizeof(buf));
    for (int i = 0; i < track->n_events; i++)
        if (track->events[i].Start == ipts
            && (duration <= 0 || track->events[i].Duration == iduration)
            && strcmp(track->events[i].Text, buf) == 0)
            return;   // We've already added this subtitle
    if (duration <= 0) {
        iduration = 10000;
        ctx->incomplete_event = true;
    }
    int eid = ass_alloc_event(track);
    ASS_Event *event = track->events + eid;
    event->Start = ipts;
    event->Duration = iduration;
    event->Style = track->default_style;
    event->Text = strdup(buf);
}

static void get_bitmaps(struct sh_sub *sh, struct osd_state *osd,
                        struct mp_osd_res dim, double pts,
                        struct sub_bitmaps *res)
{
    struct sd_ass_priv *ctx = sh->context;
    struct MPOpts *opts = osd->opts;

    if (pts == MP_NOPTS_VALUE)
        return;

    double scale = dim.display_par;
    bool use_vs_aspect = opts->ass_style_override
                         ? opts->ass_vsfilter_aspect_compat : 1;
    if (ctx->vsfilter_aspect && use_vs_aspect)
        scale = scale * dim.video_par;
    ASS_Renderer *renderer = osd->ass_renderer;
    mp_ass_configure(renderer, opts, &dim);
    ass_set_aspect_ratio(renderer, scale, 1);
    mp_ass_render_frame(renderer, ctx->ass_track, pts * 1000 + .5,
                        &ctx->parts, res);
    talloc_steal(ctx, ctx->parts);
}

static void reset(struct sh_sub *sh, struct osd_state *osd)
{
    struct sd_ass_priv *ctx = sh->context;
    if (ctx->incomplete_event)
        free_last_event(ctx->ass_track);
    ctx->incomplete_event = false;
    if (ctx->flush_on_seek)
        ass_flush_events(ctx->ass_track);
    ctx->flush_on_seek = false;
}

static void uninit(struct sh_sub *sh)
{
    struct sd_ass_priv *ctx = sh->context;

    ass_free_track(ctx->ass_track);
    talloc_free(ctx);
}

const struct sd_functions sd_ass = {
    .probe = probe,
    .init = init,
    .decode = decode,
    .get_bitmaps = get_bitmaps,
    .reset = reset,
    .switch_off = reset,
    .uninit = uninit,
};

static int sd_ass_track_destructor(void *ptr)
{
    uninit(ptr);
    return 1;
}

struct sh_sub *sd_ass_create_from_track(struct ass_track *track,
                                        bool vsfilter_aspect,
                                        struct MPOpts *opts)
{
    struct sh_sub *sh = talloc(NULL, struct sh_sub);
    talloc_set_destructor(sh, sd_ass_track_destructor);
    *sh = (struct sh_sub) {
        .opts = opts,
        .gsh = talloc_struct(sh, struct sh_stream, {
            .codec = "ass",
        }),
        .sd_driver = &sd_ass,
        .context = talloc_struct(sh, struct sd_ass_priv, {
            .ass_track = track,
            .vsfilter_aspect = vsfilter_aspect,
        }),
        .initialized = true,
    };
    return sh;
}

struct ass_track *sub_get_ass_track(struct osd_state *osd)
{
    struct sh_sub *sh = osd ? osd->sh_sub : NULL;
    if (sh && sh->sd_driver == &sd_ass && sh->context) {
        struct sd_ass_priv *ctx = sh->context;
        return ctx->ass_track;
    }
    return NULL;
}
