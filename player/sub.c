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

static bool update_subtitle(struct MPContext *mpctx, double video_pts, int order)
{
    struct MPOpts *opts = mpctx->opts;
    struct track *track = mpctx->current_track[order][STREAM_SUB];
    struct dec_sub *dec_sub = mpctx->d_sub[order];

    if (!track || !dec_sub || video_pts == MP_NOPTS_VALUE)
        return true;

    if (mpctx->d_video) {
        struct mp_image_params params = mpctx->d_video->vfilter->override_params;
        if (params.imgfmt)
            sub_control(dec_sub, SD_CTRL_SET_VIDEO_PARAMS, &params);
    }

    video_pts -= opts->sub_delay;

    if (!track->preloaded) {
        if (!sub_read_packets(dec_sub, video_pts))
            return false;
    }

    // Handle displaying subtitles on terminal; never done for secondary subs
    if (order == 0 && !mpctx->video_out)
        term_osd_set_subs(mpctx, sub_get_text(dec_sub, video_pts));

    return true;
}

// Return true if the subtitles for the given PTS are ready; false if the player
// should wait for new demuxer data, and then should retry.
bool update_subtitles(struct MPContext *mpctx, double video_pts)
{
    return update_subtitle(mpctx, video_pts, 0) &
           update_subtitle(mpctx, video_pts, 1);
}

static bool init_subdec(struct MPContext *mpctx, struct track *track)
{
    struct MPOpts *opts = mpctx->opts;

    assert(!track->dec_sub);

    if (!track->demuxer || !track->stream)
        return false;

    track->dec_sub = sub_create(mpctx->global, track->demuxer, track->stream);
    if (!track->dec_sub)
        return false;

    struct sh_video *sh_video =
        mpctx->d_video ? mpctx->d_video->header->video : NULL;
    double fps = sh_video ? sh_video->fps : 25;
    sub_control(track->dec_sub, SD_CTRL_SET_VIDEO_DEF_FPS, &fps);

    // Don't do this if the file has video/audio streams. Don't do it even
    // if it has only sub streams, because reading packets will change the
    // demuxer position.
    if (track->is_external && !opts->sub_clear_on_seek) {
        demux_seek(track->demuxer, 0, SEEK_ABSOLUTE);
        track->preloaded = sub_read_all_packets(track->dec_sub);
        if (track->preloaded)
            demux_stop_thread(track->demuxer);
    }

    return true;
}

void reinit_subs(struct MPContext *mpctx, int order)
{
    assert(!mpctx->d_sub[order]);

    struct track *track = mpctx->current_track[order][STREAM_SUB];
    if (!track)
        return;

    if (!track->dec_sub && !init_subdec(mpctx, track)) {
        error_on_track(mpctx, track);
        return;
    }

    sub_select(track->dec_sub, true);
    osd_set_sub(mpctx->osd, OSDTYPE_SUB + order, track->dec_sub);
    sub_control(track->dec_sub, SD_CTRL_SET_TOP, &(bool){!!order});

    mpctx->d_sub[order] = track->dec_sub;
}
