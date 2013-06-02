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
#include <assert.h>
#include <string.h>

#include <ass/ass.h>

#include "talloc.h"

#include "core/options.h"
#include "core/mp_common.h"
#include "core/mp_msg.h"
#include "sub.h"
#include "dec_sub.h"
#include "ass_mp.h"
#include "sd.h"

struct sd_ass_priv {
    struct ass_track *ass_track;
    bool vsfilter_aspect;
    bool incomplete_event;
    struct sub_bitmap *parts;
    bool flush_on_seek;
    char last_text[500];
};

static bool is_native_ass(const char *t)
{
    return strcmp(t, "ass") == 0 || strcmp(t, "ssa") == 0;
}

static bool supports_format(const char *format)
{
    // ass-text is produced by converters and the subreader.c ssa parser; this
    // format has ASS tags, but doesn't start with any prelude, nor does it
    // have extradata.
    return format && (is_native_ass(format) || strcmp(format, "ass-text") == 0);
}

static void free_last_event(ASS_Track *track)
{
    assert(track->n_events > 0);
    ass_free_event(track, track->n_events - 1);
    track->n_events--;
}

static int init(struct sd *sd)
{
    if (!sd->ass_library || !sd->ass_renderer)
        return -1;

    bool ass = is_native_ass(sd->codec);
    struct sd_ass_priv *ctx = talloc_zero(NULL, struct sd_ass_priv);
    sd->priv = ctx;
    if (sd->ass_track) {
        ctx->ass_track = sd->ass_track;
    } else if (ass) {
        ctx->ass_track = ass_new_track(sd->ass_library);
    } else
        ctx->ass_track = mp_ass_default_track(sd->ass_library, sd->opts);

    if (sd->extradata) {
        ass_process_codec_private(ctx->ass_track, sd->extradata,
                                  sd->extradata_len);
    }

    ctx->vsfilter_aspect = ass;
    return 0;
}

static void decode(struct sd *sd, struct demux_packet *packet)
{
    void *data = packet->buffer;
    int data_len = packet->len;
    double pts = packet->pts;
    double duration = packet->duration;
    unsigned char *text = data;
    struct sd_ass_priv *ctx = sd->priv;
    ASS_Track *track = ctx->ass_track;
    if (is_native_ass(sd->codec)) {
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
    for (int i = 0; i < track->n_events; i++)
        if (track->events[i].Start == ipts
            && (duration <= 0 || track->events[i].Duration == iduration)
            && strcmp(track->events[i].Text, text) == 0)
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
    event->Text = strdup(text);
}

static void get_bitmaps(struct sd *sd, struct mp_osd_res dim, double pts,
                        struct sub_bitmaps *res)
{
    struct sd_ass_priv *ctx = sd->priv;
    struct MPOpts *opts = sd->opts;

    if (pts == MP_NOPTS_VALUE || !sd->ass_renderer)
        return;

    double scale = dim.display_par;
    bool use_vs_aspect = opts->ass_style_override
                         ? opts->ass_vsfilter_aspect_compat : 1;
    if (ctx->vsfilter_aspect && use_vs_aspect)
        scale = scale * dim.video_par;
    ASS_Renderer *renderer = sd->ass_renderer;
    mp_ass_configure(renderer, opts, &dim);
    ass_set_aspect_ratio(renderer, scale, 1);
    mp_ass_render_frame(renderer, ctx->ass_track, pts * 1000 + .5,
                        &ctx->parts, res);
    talloc_steal(ctx, ctx->parts);
}

struct buf {
    char *start;
    int size;
    int len;
};

static void append(struct buf *b, char c)
{
    if (b->len < b->size) {
        b->start[b->len] = c;
        b->len++;
    }
}

static void ass_to_plaintext(struct buf *b, const char *in)
{
    bool in_tag = false;
    bool in_drawing = false;
    while (*in) {
        if (in_tag) {
            if (in[0] == '}') {
                in += 1;
                in_tag = false;
            } else if (in[0] == '\\' && in[1] == 'p') {
                in += 2;
                // skip text between \pN and \p0 tags
                if (in[0] == '0') {
                    in_drawing = false;
                } else if (in[0] >= '1' && in[0] <= '9') {
                    in_drawing = true;
                }
            } else {
                in += 1;
            }
        } else {
            if (in[0] == '\\' && (in[1] == 'N' || in[1] == 'n')) {
                in += 2;
                append(b, '\n');
            } else if (in[0] == '\\' && in[1] == 'h') {
                in += 2;
                append(b, ' ');
            } else if (in[0] == '{') {
                in += 1;
                in_tag = true;
            } else {
                if (!in_drawing)
                    append(b, in[0]);
                in += 1;
            }
        }
    }
}

// Empty string counts as whitespace. Reads s[len-1] even if there are \0s.
static bool is_whitespace_only(char *s, int len)
{
    for (int n = 0; n < len; n++) {
        if (s[n] != ' ' && s[n] != '\t')
            return false;
    }
    return true;
}

static char *get_text(struct sd *sd, double pts)
{
    struct sd_ass_priv *ctx = sd->priv;
    ASS_Track *track = ctx->ass_track;

    if (pts == MP_NOPTS_VALUE)
        return NULL;

    struct buf b = {ctx->last_text, sizeof(ctx->last_text) - 1};

    for (int i = 0; i < track->n_events; ++i) {
        ASS_Event *event = track->events + i;
        double start = event->Start / 1000.0;
        double end = (event->Start + event->Duration) / 1000.0;
        if (pts >= start && pts < end) {
            if (event->Text) {
                int start = b.len;
                ass_to_plaintext(&b, event->Text);
                if (!is_whitespace_only(&b.start[b.len], b.len - start))
                    append(&b, '\n');
            }
        }
    }

    b.start[b.len] = '\0';

    if (b.len > 0 && b.start[b.len - 1] == '\n')
        b.start[b.len - 1] = '\0';

    return ctx->last_text;
}

static void fix_events(struct sd *sd)
{
    struct sd_ass_priv *ctx = sd->priv;
    ctx->flush_on_seek = false;
}

static void reset(struct sd *sd)
{
    struct sd_ass_priv *ctx = sd->priv;
    if (ctx->incomplete_event)
        free_last_event(ctx->ass_track);
    ctx->incomplete_event = false;
    if (ctx->flush_on_seek)
        ass_flush_events(ctx->ass_track);
    ctx->flush_on_seek = false;
}

static void uninit(struct sd *sd)
{
    struct sd_ass_priv *ctx = sd->priv;

    if (sd->ass_track != ctx->ass_track)
        ass_free_track(ctx->ass_track);
    talloc_free(ctx);
}

const struct sd_functions sd_ass = {
    .accept_packets_in_advance = true,
    .supports_format = supports_format,
    .init = init,
    .decode = decode,
    .get_bitmaps = get_bitmaps,
    .get_text = get_text,
    .fix_events = fix_events,
    .reset = reset,
    .uninit = uninit,
};

struct ass_track *sub_get_ass_track(struct dec_sub *sub)
{
    struct sd *sd = sub_get_last_sd(sub);
    if (sd && sd->driver == &sd_ass && sd->priv) {
        struct sd_ass_priv *ctx = sd->priv;
        return ctx->ass_track;
    }
    return NULL;
}
