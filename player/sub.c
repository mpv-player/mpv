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

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <math.h>
#include <assert.h>

#include "config.h"
#include "talloc.h"

#include "common/msg.h"
#include "options/options.h"
#include "common/common.h"

#include "stream/stream.h"
#include "sub/dec_sub.h"
#include "demux/demux.h"
#include "video/mp_image.h"
#include "video/decode/dec_video.h"
#include "video/filter/vf.h"

#include "core.h"

void uninit_subs(struct demuxer *demuxer)
{
    for (int i = 0; i < demuxer->num_streams; i++) {
        struct sh_stream *sh = demuxer->streams[i];
        if (sh->sub) {
            sub_destroy(sh->sub->dec_sub);
            sh->sub->dec_sub = NULL;
        }
    }
}

// When reading subtitles from a demuxer, and we read video or audio from the
// demuxer, we should not explicitly read subtitle packets. (With external
// subs, we have to.)
static bool is_interleaved(struct MPContext *mpctx, struct track *track)
{
    if (track->is_external || !track->demuxer)
        return false;

    struct demuxer *demuxer = track->demuxer;
    for (int t = 0; t < mpctx->num_tracks; t++) {
        struct track *other = mpctx->tracks[t];
        if (other != track && other->selected && other->demuxer == demuxer &&
            (other->type == STREAM_VIDEO || other->type == STREAM_AUDIO))
            return true;
    }
    return false;
}

void reset_subtitles(struct MPContext *mpctx, int order)
{
    int obj = order ? OSDTYPE_SUB2 : OSDTYPE_SUB;
    if (mpctx->d_sub[order])
        sub_reset(mpctx->d_sub[order]);
    set_osd_subtitle(mpctx, NULL);
    osd_set_text(mpctx->osd, obj, NULL);
}

void reset_subtitle_state(struct MPContext *mpctx)
{
    reset_subtitles(mpctx, 0);
    reset_subtitles(mpctx, 1);
}

static void update_subtitle(struct MPContext *mpctx, int order)
{
    struct MPOpts *opts = mpctx->opts;
    if (order == 0) {
        if (!(mpctx->initialized_flags & INITIALIZED_SUB))
            return;
    } else {
        if (!(mpctx->initialized_flags & INITIALIZED_SUB2))
            return;
    }

    struct track *track = mpctx->current_track[order][STREAM_SUB];
    struct dec_sub *dec_sub = mpctx->d_sub[order];
    assert(track && dec_sub);
    int obj = order ? OSDTYPE_SUB2 : OSDTYPE_SUB;

    if (mpctx->d_video) {
        struct mp_image_params params = mpctx->d_video->vfilter->override_params;
        if (params.imgfmt)
            sub_control(dec_sub, SD_CTRL_SET_VIDEO_PARAMS, &params);
    }

    struct osd_sub_state state;
    osd_get_sub(mpctx->osd, obj, &state);

    state.video_offset =
        track->under_timeline ? mpctx->video_offset : get_start_time(mpctx);

    osd_set_sub(mpctx->osd, obj, &state);

    double refpts_s = mpctx->playback_pts - state.video_offset;
    double curpts_s = refpts_s - opts->sub_delay;

    if (!track->preloaded && track->stream) {
        struct sh_stream *sh_stream = track->stream;
        bool interleaved = is_interleaved(mpctx, track);

        assert(sh_stream->sub->dec_sub == dec_sub);

        while (1) {
            if (interleaved && !demux_has_packet(sh_stream))
                break;
            double subpts_s = demux_get_next_pts(sh_stream);
            if (!demux_has_packet(sh_stream))
                break;
            if (subpts_s > curpts_s) {
                MP_DBG(mpctx, "Sub early: c_pts=%5.3f s_pts=%5.3f\n",
                       curpts_s, subpts_s);
                // Libass handled subs can be fed to it in advance
                if (!sub_accept_packets_in_advance(dec_sub))
                    break;
                // Try to avoid demuxing whole file at once
                if (subpts_s > curpts_s + 1 && !interleaved)
                    break;
            }
            struct demux_packet *pkt = demux_read_packet(sh_stream);
            MP_DBG(mpctx, "Sub: c_pts=%5.3f s_pts=%5.3f duration=%5.3f len=%d\n",
                   curpts_s, pkt->pts, pkt->duration, pkt->len);
            sub_decode(dec_sub, pkt);
            talloc_free(pkt);
        }
    }

    // Handle displaying subtitles on terminal; never done for secondary subs
    if (order == 0) {
        if (!state.render_bitmap_subs || !mpctx->video_out) {
            sub_lock(dec_sub);
            set_osd_subtitle(mpctx, sub_get_text(dec_sub, curpts_s));
            sub_unlock(dec_sub);
        }
    } else if (order == 1) {
        sub_lock(dec_sub);
        osd_set_text(mpctx->osd, obj, sub_get_text(dec_sub, curpts_s));
        sub_unlock(dec_sub);
    }
}

void update_subtitles(struct MPContext *mpctx)
{
    update_subtitle(mpctx, 0);
    update_subtitle(mpctx, 1);
}

static void reinit_subdec(struct MPContext *mpctx, struct track *track,
                          struct dec_sub *dec_sub)
{
    if (sub_is_initialized(dec_sub))
        return;

    struct sh_video *sh_video =
        mpctx->d_video ? mpctx->d_video->header->video : NULL;
    int w = sh_video ? sh_video->disp_w : 0;
    int h = sh_video ? sh_video->disp_h : 0;
    float fps = sh_video ? sh_video->fps : 25;

    sub_set_video_res(dec_sub, w, h);
    sub_set_video_fps(dec_sub, fps);
    sub_set_ass_renderer(dec_sub, mpctx->ass_library, mpctx->ass_renderer);
    sub_init_from_sh(dec_sub, track->stream);

    // Don't do this if the file has video/audio streams. Don't do it even
    // if it has only sub streams, because reading packets will change the
    // demuxer position.
    if (!track->preloaded && track->is_external) {
        demux_seek(track->demuxer, 0, SEEK_ABSOLUTE);
        track->preloaded = sub_read_all_packets(dec_sub, track->stream);
    }
}

void reinit_subs(struct MPContext *mpctx, int order)
{
    struct MPOpts *opts = mpctx->opts;
    struct track *track = mpctx->current_track[order][STREAM_SUB];
    int obj = order ? OSDTYPE_SUB2 : OSDTYPE_SUB;
    int init_flag = order ? INITIALIZED_SUB2 : INITIALIZED_SUB;

    assert(!(mpctx->initialized_flags & init_flag));

    struct sh_stream *sh = track ? track->stream : NULL;
    if (!sh)
        return;

    if (!sh->sub->dec_sub) {
        assert(!mpctx->d_sub[order]);
        sh->sub->dec_sub = sub_create(mpctx->global);
    }

    assert(!mpctx->d_sub[order] || sh->sub->dec_sub == mpctx->d_sub[order]);

    // The decoder is kept in the stream header in order to make ordered
    // chapters work well.
    mpctx->d_sub[order] = sh->sub->dec_sub;

    mpctx->initialized_flags |= init_flag;

    struct dec_sub *dec_sub = mpctx->d_sub[order];
    assert(dec_sub);

    reinit_subdec(mpctx, track, dec_sub);

    struct osd_sub_state state = {
        .dec_sub = dec_sub,
        // Decides whether to use OSD path or normal subtitle rendering path.
        .render_bitmap_subs = opts->ass_enabled || !sub_has_get_text(dec_sub),
    };

    // Secondary subs are rendered with the "text" renderer to transform them
    // to toptitles.
    if (order == 1 && sub_has_get_text(dec_sub))
        state.render_bitmap_subs = false;

    reset_subtitles(mpctx, order);

    osd_set_sub(mpctx->osd, obj, &state);
}
