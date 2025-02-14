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

#include "mpv_talloc.h"

#include "common/msg.h"
#include "options/options.h"
#include "common/common.h"

#include "stream/stream.h"
#include "sub/dec_sub.h"
#include "demux/demux.h"
#include "video/mp_image.h"

#include "core.h"

// 0: primary sub, 1: secondary sub, -1: not selected
static int get_order(struct MPContext *mpctx, struct track *track)
{
    for (int n = 0; n < num_ptracks[STREAM_SUB]; n++) {
        if (mpctx->current_track[n][STREAM_SUB] == track)
            return n;
    }
    return -1;
}

static void reset_subtitles(struct MPContext *mpctx, struct track *track)
{
    if (track->d_sub) {
        sub_reset(track->d_sub);
        sub_set_play_dir(track->d_sub, mpctx->play_dir);
    }
}

// Only matters for subs on an image.
void redraw_subs(struct MPContext *mpctx)
{
    for (int n = 0; n < num_ptracks[STREAM_SUB]; n++) {
        if (mpctx->current_track[n][STREAM_SUB] &&
            mpctx->current_track[n][STREAM_SUB]->d_sub)
        {
            mpctx->current_track[n][STREAM_SUB]->redraw_subs = true;
        }
    }
}

void reset_subtitle_state(struct MPContext *mpctx)
{
    for (int n = 0; n < mpctx->num_tracks; n++)
        reset_subtitles(mpctx, mpctx->tracks[n]);
    term_osd_clear_subs(mpctx);
}

void uninit_sub(struct MPContext *mpctx, struct track *track)
{
    if (track && track->d_sub) {
        int order = get_order(mpctx, track);
        reset_subtitles(mpctx, track);
        term_osd_set_subs(mpctx, NULL, order);
        sub_select(track->d_sub, false);
        osd_set_sub(mpctx->osd, order, NULL);
        sub_destroy(track->d_sub);
        track->d_sub = NULL;
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

    if (!dec_sub)
        return true;

    if (mpctx->vo_chain) {
        struct mp_image_params params = mpctx->vo_chain->filter->input_params;
        if (params.imgfmt)
            sub_control(dec_sub, SD_CTRL_SET_VIDEO_PARAMS, &params);
    }

    // Checking if packets have special animations is relatively expensive.
    // This is only needed if we are rendering ASS subtitles with no video
    // being played.
    bool still_image = mpctx->video_out && ((mpctx->video_status == STATUS_EOF &&
                       mpctx->opts->subs_rend->sub_past_video_end) ||
                       !mpctx->current_track[0][STREAM_VIDEO] ||
                       mpctx->current_track[0][STREAM_VIDEO]->image);
    sub_control(dec_sub, SD_CTRL_SET_ANIMATED_CHECK, &still_image);

    if (track->demuxer->fully_read && sub_can_preload(dec_sub)) {
        // Assume fully_read implies no interleaved audio/video streams.
        // (Reading packets will change the demuxer position.)
        demux_seek(track->demuxer, 0, 0);
        sub_preload(dec_sub);
    }

    bool packets_read = false;
    bool sub_updated = false;
    sub_read_packets(dec_sub, video_pts, mpctx->paused, &packets_read, &sub_updated);

    double osd_pts = osd_get_force_video_pts(mpctx->osd);

    // Check if we need to update subtitles for these special cases. Always
    // update on discontinuities like seeking or a new file.
    if (sub_updated || track->redraw_subs || osd_pts == MP_NOPTS_VALUE) {
        // Always force a redecode of all packets if we seek on a still image.
        if (track->redraw_subs && still_image)
            sub_redecode_cached_packets(dec_sub);

        // Handle displaying subtitles on terminal.
        if (track->selected && track->type == STREAM_SUB && !mpctx->video_out) {
            char *text = sub_get_text(dec_sub, video_pts, SD_TEXT_TYPE_PLAIN);
            term_osd_set_subs(mpctx, text, get_order(mpctx, track));
            talloc_free(text);
        }

        // Handle displaying subtitles on VO with no video being played. This is
        // quite different, because normally subtitles are redrawn on new video
        // frames, using the video frames' timestamps.
        if (still_image) {
            if (osd_pts != video_pts) {
                osd_set_force_video_pts(mpctx->osd, video_pts);
                osd_query_and_reset_want_redraw(mpctx->osd);
                vo_redraw(mpctx->video_out);
            }
        }
    }

    track->redraw_subs = false;
    return packets_read;
}

// Return true if the subtitles for the given PTS are ready; false if the player
// should wait for new demuxer data, and then should retry.
bool update_subtitles(struct MPContext *mpctx, double video_pts)
{
    bool ok = true;
    for (int n = 0; n < num_ptracks[STREAM_SUB]; n++)
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

    track->d_sub = sub_create(mpctx->global, track,
                              get_all_attachments(mpctx),
                              get_order(mpctx, track));
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

    assert(!track->d_sub);

    if (!init_subdec(mpctx, track)) {
        error_on_track(mpctx, track);
        return;
    }

    sub_select(track->d_sub, true);
    int order = get_order(mpctx, track);
    osd_set_sub(mpctx->osd, order, track->d_sub);

    // When paused we have to wait for packets to be available.
    // Retry on a timeout until we get a packet. If still not successful,
    // then queue it for later in the playloop (but this will have a delay).
    if (mpctx->playback_initialized) {
        track->demuxer_ready = false;
        int64_t end = mp_time_ns() + MP_TIME_MS_TO_NS(50);
        while (!track->demuxer_ready && mp_time_ns() < end)
            track->demuxer_ready = update_subtitles(mpctx, mpctx->playback_pts) ||
                                  !mpctx->paused;
        if (!track->demuxer_ready)
            mp_wakeup_core(mpctx);

    }
}

void reinit_sub_all(struct MPContext *mpctx)
{
    for (int n = 0; n < num_ptracks[STREAM_SUB]; n++)
        reinit_sub(mpctx, mpctx->current_track[n][STREAM_SUB]);
}
