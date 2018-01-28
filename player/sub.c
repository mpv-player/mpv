/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <math.h>
#include <assert.h>

#include "config.h"
#include "mpv_talloc.h"

#include "common/msg.h"
#include "options/options.h"
#include "common/common.h"
#include "common/global.h"

#include "stream/stream.h"
#include "sub/dec_sub.h"
#include "demux/demux.h"
#include "video/mp_image.h"

#include "core.h"

// 0: primary sub, 1: secondary sub, -1: not selected
static int get_order(struct MPContext *mpctx, struct track *track)
{
    for (int n = 0; n < NUM_PTRACKS; n++) {
        if (mpctx->current_track[n][STREAM_SUB] == track)
            return n;
    }
    return -1;
}

static void reset_subtitles(struct MPContext *mpctx, struct track *track)
{
    if (track->d_sub)
        sub_reset(track->d_sub);
    term_osd_set_subs(mpctx, NULL);
}

void reset_subtitle_state(struct MPContext *mpctx)
{
    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct dec_sub *d_sub = mpctx->tracks[n]->d_sub;
        if (d_sub)
            sub_reset(d_sub);
    }
    term_osd_set_subs(mpctx, NULL);
}

void uninit_sub(struct MPContext *mpctx, struct track *track)
{
    if (track && track->d_sub) {
        reset_subtitles(mpctx, track);
        sub_select(track->d_sub, false);
        int order = get_order(mpctx, track);
        osd_set_sub(mpctx->osd, order, NULL);
    }
}

void uninit_sub_all(struct MPContext *mpctx)
{
    for (int n = 0; n < mpctx->num_tracks; n++)
        uninit_sub(mpctx, mpctx->tracks[n]);
}

static bool update_subtitle(struct MPContext *mpctx, double video_pts,
                            struct track *track)
{
    struct dec_sub *dec_sub = track ? track->d_sub : NULL;

    if (!dec_sub || video_pts == MP_NOPTS_VALUE)
        return true;

    if (mpctx->vo_chain) {
        struct mp_image_params params = mpctx->vo_chain->filter->input_params;
        if (params.imgfmt)
            sub_control(dec_sub, SD_CTRL_SET_VIDEO_PARAMS, &params);
    }

    if (track->demuxer->fully_read && sub_can_preload(dec_sub)) {
        // Assume fully_read implies no interleaved audio/video streams.
        // (Reading packets will change the demuxer position.)
        demux_seek(track->demuxer, 0, 0);
        sub_preload(dec_sub);
    }

    if (!sub_read_packets(dec_sub, video_pts))
        return false;

    // Handle displaying subtitles on terminal; never done for secondary subs
    if (mpctx->current_track[0][STREAM_SUB] == track && !mpctx->video_out)
        term_osd_set_subs(mpctx, sub_get_text(dec_sub, video_pts));

    // Handle displaying subtitles on VO with no video being played. This is
    // quite differently, because normally subtitles are redrawn on new video
    // frames, using the video frames' timestamps.
    if (mpctx->video_out && mpctx->video_status == STATUS_EOF) {
        if (osd_get_force_video_pts(mpctx->osd) != video_pts) {
            osd_set_force_video_pts(mpctx->osd, video_pts);
            osd_query_and_reset_want_redraw(mpctx->osd);
            vo_redraw(mpctx->video_out);
            // Force an arbitrary minimum FPS
            mp_set_timeout(mpctx, 0.1);
        }
    }

    return true;
}

// Return true if the subtitles for the given PTS are ready; false if the player
// should wait for new demuxer data, and then should retry.
bool update_subtitles(struct MPContext *mpctx, double video_pts)
{
    bool ok = true;
    for (int n = 0; n < NUM_PTRACKS; n++)
        ok &= update_subtitle(mpctx, video_pts, mpctx->current_track[n][STREAM_SUB]);
    return ok;
}

static struct attachment_list *get_all_attachments(struct MPContext *mpctx)
{
    struct attachment_list *list = talloc_zero(NULL, struct attachment_list);
    struct demuxer *prev_demuxer = NULL;
    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *t = mpctx->tracks[n];
        if (!t->demuxer || prev_demuxer == t->demuxer)
            continue;
        prev_demuxer = t->demuxer;
        for (int i = 0; i < t->demuxer->num_attachments; i++) {
            struct demux_attachment *att = &t->demuxer->attachments[i];
            struct demux_attachment copy = {
                .name = talloc_strdup(list, att->name),
                .type = talloc_strdup(list, att->type),
                .data = talloc_memdup(list, att->data, att->data_size),
                .data_size = att->data_size,
            };
            MP_TARRAY_APPEND(list, list->entries, list->num_entries, copy);
        }
    }
    return list;
}

static bool init_subdec(struct MPContext *mpctx, struct track *track)
{
    assert(!track->d_sub);

    if (!track->demuxer || !track->stream)
        return false;

    track->d_sub = sub_create(mpctx->global, track->stream,
                              get_all_attachments(mpctx));
    if (!track->d_sub)
        return false;

    struct track *vtrack = mpctx->current_track[0][STREAM_VIDEO];
    struct mp_codec_params *v_c =
        vtrack && vtrack->stream ? vtrack->stream->codec : NULL;
    double fps = v_c ? v_c->fps : 25;
    sub_control(track->d_sub, SD_CTRL_SET_VIDEO_DEF_FPS, &fps);

    return true;
}

void reinit_sub(struct MPContext *mpctx, struct track *track)
{
    if (!track || !track->stream || track->stream->type != STREAM_SUB)
        return;

    if (!track->d_sub && !init_subdec(mpctx, track)) {
        error_on_track(mpctx, track);
        return;
    }

    sub_select(track->d_sub, true);
    int order = get_order(mpctx, track);
    osd_set_sub(mpctx->osd, order, track->d_sub);
    sub_control(track->d_sub, SD_CTRL_SET_TOP, &(bool){!!order});

    if (mpctx->playback_initialized)
        update_subtitles(mpctx, mpctx->playback_pts);
}

void reinit_sub_all(struct MPContext *mpctx)
{
    for (int n = 0; n < NUM_PTRACKS; n++)
        reinit_sub(mpctx, mpctx->current_track[n][STREAM_SUB]);
}
