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

#include "mpcommon.h"
#include "mp_msg.h"
#include "libmpdemux/stheader.h"
#include "sub.h"
#include "ass_mp.h"
#include "sd.h"
#include "subassconvert.h"

struct sd_ass_priv {
    struct ass_track *ass_track;
    bool incomplete_event;
};

static void free_last_event(ASS_Track *track)
{
    assert(track->n_events > 0);
    ass_free_event(track, track->n_events - 1);
    track->n_events--;
}

static int init(struct sh_sub *sh, struct osd_state *osd)
{
    struct sd_ass_priv *ctx;

    if (sh->initialized) {
        ctx = sh->context;
    } else {
        ctx = talloc_zero(NULL, struct sd_ass_priv);
        sh->context = ctx;
        if (sh->type == 'a') {
            ctx->ass_track = ass_new_track(osd->ass_library);
            if (sh->extradata)
                ass_process_codec_private(ctx->ass_track, sh->extradata,
                                          sh->extradata_len);
        } else
            ctx->ass_track = mp_ass_default_track(osd->ass_library, sh->opts);
    }

    assert(osd->ass_track == NULL);
    osd->ass_track = ctx->ass_track;
    osd->vsfilter_aspect = sh->type == 'a';
    return 0;
}

static void decode(struct sh_sub *sh, struct osd_state *osd, void *data,
                   int data_len, double pts, double duration)
{
    unsigned char *text = data;
    struct sd_ass_priv *ctx = sh->context;
    ASS_Track *track = ctx->ass_track;

    if (sh->type == 'a') { // ssa/ass subs
        ass_process_chunk(track, data, data_len,
                          (long long)(pts*1000 + 0.5),
                          (long long)(duration*1000 + 0.5));
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

static void reset(struct sh_sub *sh, struct osd_state *osd)
{
    struct sd_ass_priv *ctx = sh->context;
    if (ctx->incomplete_event)
        free_last_event(ctx->ass_track);
    ctx->incomplete_event = false;
}

static void switch_off(struct sh_sub *sh, struct osd_state *osd)
{
    reset(sh, osd);
    osd->ass_track = NULL;
}

static void uninit(struct sh_sub *sh)
{
    struct sd_ass_priv *ctx = sh->context;

    ass_free_track(ctx->ass_track);
    talloc_free(ctx);
}

const struct sd_functions sd_ass = {
    .init = init,
    .decode = decode,
    .reset = reset,
    .switch_off = switch_off,
    .uninit = uninit,
};
