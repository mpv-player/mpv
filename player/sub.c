/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
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
#include "common/global.h"

#include "stream/stream.h"
#include "sub/dec_sub.h"
#include "demux/demux.h"
#include "video/mp_image.h"
#include "video/decode/dec_video.h"
#include "video/filter/vf.h"

#include "core.h"

static void reset_subtitles(struct MPContext *mpctx, int order)
{
    if (mpctx->d_sub[order])
        sub_reset(mpctx->d_sub[order]);
    term_osd_set_subs(mpctx, NULL);
}

void reset_subtitle_state(struct MPContext *mpctx)
{
    reset_subtitles(mpctx, 0);
    reset_subtitles(mpctx, 1);
}

void uninit_sub(struct MPContext *mpctx, int order)
{
    if (mpctx->d_sub[order]) {
        reset_subtitles(mpctx, order);
        sub_select(mpctx->d_sub[order], false);
        mpctx->d_sub[order] = NULL; // not destroyed
        osd_set_sub(mpctx->osd, OSDTYPE_SUB + order, NULL);
        reselect_demux_streams(mpctx);
    }
}

void uninit_sub_all(struct MPContext *mpctx)
{
    uninit_sub(mpctx, 0);
    uninit_sub(mpctx, 1);
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
    return track->demuxer == mpctx->demuxer;
}

static void update_subtitle(struct MPContext *mpctx, int order)
{
    struct MPOpts *opts = mpctx->opts;
    struct track *track = mpctx->current_track[order][STREAM_SUB];
    struct dec_sub *dec_sub = mpctx->d_sub[order];

    if (!track || !dec_sub)
        return;

    if (mpctx->d_video) {
        struct mp_image_params params = mpctx->d_video->vfilter->override_params;
        if (params.imgfmt)
            sub_control(dec_sub, SD_CTRL_SET_VIDEO_PARAMS, &params);
    }

    double refpts_s = mpctx->playback_pts;
    double curpts_s = refpts_s - opts->sub_delay;

    if (!track->preloaded && track->stream) {
        struct sh_stream *sh_stream = track->stream;
        bool interleaved = is_interleaved(mpctx, track);

        while (1) {
            if (interleaved && !demux_has_packet(sh_stream))
                break;
            double subpts_s = demux_get_next_pts(sh_stream);
            if (!demux_has_packet(sh_stream))
                break;
            if (subpts_s > curpts_s) {
                MP_DBG(mpctx, "Sub early: c_pts=%5.3f s_pts=%5.3f\n",
                       curpts_s, subpts_s);
                // Often subs can be handled in advance
                if (!sub_accepts_packet_in_advance(dec_sub))
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
    if (order == 0 && !mpctx->video_out)
        term_osd_set_subs(mpctx, sub_get_text(dec_sub, curpts_s));
}

void update_subtitles(struct MPContext *mpctx)
{
    update_subtitle(mpctx, 0);
    update_subtitle(mpctx, 1);
}

static void reinit_subdec(struct MPContext *mpctx, struct track *track)
{
    struct MPOpts *opts = mpctx->opts;

    struct dec_sub *dec_sub = track->dec_sub;

    if (sub_is_initialized(dec_sub))
        return;

    sub_init(dec_sub, track->demuxer, track->stream);

    struct sh_video *sh_video =
        mpctx->d_video ? mpctx->d_video->header->video : NULL;
    double fps = sh_video ? sh_video->fps : 25;
    sub_control(dec_sub, SD_CTRL_SET_VIDEO_DEF_FPS, &fps);

    // Don't do this if the file has video/audio streams. Don't do it even
    // if it has only sub streams, because reading packets will change the
    // demuxer position.
    if (!track->preloaded && track->is_external && !opts->sub_clear_on_seek) {
        demux_seek(track->demuxer, 0, SEEK_ABSOLUTE);
        track->preloaded = sub_read_all_packets(dec_sub, track->stream);
        if (track->preloaded)
            demux_stop_thread(track->demuxer);
    }
}

void reinit_subs(struct MPContext *mpctx, int order)
{
    struct track *track = mpctx->current_track[order][STREAM_SUB];

    assert(!mpctx->d_sub[order]);

    struct sh_stream *sh = track ? track->stream : NULL;
    if (!sh)
        return;

    if (!track->dec_sub)
        track->dec_sub = sub_create(mpctx->global);
    mpctx->d_sub[order] = track->dec_sub;

    sub_select(track->dec_sub, true);
    reinit_subdec(mpctx, track);
    osd_set_sub(mpctx->osd, OSDTYPE_SUB + order, track->dec_sub);
    sub_control(track->dec_sub, SD_CTRL_SET_TOP, &(bool){!!order});
}
