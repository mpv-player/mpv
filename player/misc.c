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
#include <errno.h>
#include <assert.h>

#include "config.h"
#include "mpv_talloc.h"

#include "osdep/io.h"
#include "osdep/timer.h"
#include "osdep/threads.h"

#include "common/msg.h"
#include "options/options.h"
#include "options/m_property.h"
#include "options/m_config.h"
#include "common/common.h"
#include "common/global.h"
#include "common/encode.h"
#include "common/playlist.h"
#include "input/input.h"

#include "audio/out/ao.h"
#include "demux/demux.h"
#include "stream/stream.h"
#include "video/out/vo.h"

#include "core.h"
#include "command.h"

double rel_time_to_abs(struct MPContext *mpctx, struct m_rel_time t)
{
    double length = get_time_length(mpctx);
    // Relative times are an offset to the start of the file.
    double start = 0;
    if (mpctx->demuxer && !mpctx->opts->rebase_start_time)
        start = mpctx->demuxer->start_time;

    switch (t.type) {
    case REL_TIME_ABSOLUTE:
        return t.pos;
    case REL_TIME_RELATIVE:
        if (t.pos >= 0) {
            return start + t.pos;
        } else {
            if (length >= 0)
                return start + MPMAX(length + t.pos, 0.0);
        }
        break;
    case REL_TIME_PERCENT:
        if (length >= 0)
            return start + length * (t.pos / 100.0);
        break;
    case REL_TIME_CHAPTER:
        return chapter_start_time(mpctx, t.pos); // already absolute time
    }

    return MP_NOPTS_VALUE;
}

static double get_play_end_pts_setting(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    double end = rel_time_to_abs(mpctx, opts->play_end);
    double length = rel_time_to_abs(mpctx, opts->play_length);
    if (length != MP_NOPTS_VALUE) {
        double start = get_play_start_pts(mpctx);
        if (end == MP_NOPTS_VALUE || start + length < end)
            end = start + length;
    }
    return end;
}

// Return absolute timestamp against which currently playing media should be
// clipped. Returns MP_NOPTS_VALUE if no clipping should happen.
double get_play_end_pts(struct MPContext *mpctx)
{
    double end = get_play_end_pts_setting(mpctx);
    double ab[2];
    if (mpctx->ab_loop_clip && get_ab_loop_times(mpctx, ab)) {
        if (end == MP_NOPTS_VALUE || end > ab[1])
            end = ab[1];
    }
    return end;
}

// Get the absolute PTS at which playback should start.
// Never returns MP_NOPTS_VALUE.
double get_play_start_pts(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    double res = rel_time_to_abs(mpctx, opts->play_start);
    if (res == MP_NOPTS_VALUE) {
        res = 0;
        if (!opts->rebase_start_time && mpctx->demuxer)
            res = mpctx->demuxer->start_time;
        // Backward playback -> start from end by default.
        if (mpctx->play_dir < 0 && mpctx->demuxer)
            res = MPMAX(mpctx->demuxer->duration, 0);
    }
    return res;
}

// Get timestamps to use for AB-loop. Returns false iff any of the timestamps
// are invalid and/or AB-loops are currently disabled, and set t[] to either
// the user options or NOPTS on best effort basis.
bool get_ab_loop_times(struct MPContext *mpctx, double t[2])
{
    struct MPOpts *opts = mpctx->opts;
    int dir = mpctx->play_dir;

    t[0] = opts->ab_loop[0];
    t[1] = opts->ab_loop[1];

    if (t[0] == MP_NOPTS_VALUE || t[1] == MP_NOPTS_VALUE || t[0] == t[1])
        return false;

    if (t[0] * dir > t[1] * dir)
        MPSWAP(double, t[0], t[1]);

    return true;
}

double get_track_seek_offset(struct MPContext *mpctx, struct track *track)
{
    struct MPOpts *opts = mpctx->opts;
    if (track->selected) {
        if (track->type == STREAM_AUDIO)
            return -opts->audio_delay;
        if (track->type == STREAM_SUB)
            return -opts->subs_rend->sub_delay;
    }
    return 0;
}

void issue_refresh_seek(struct MPContext *mpctx, enum seek_precision min_prec)
{
    // let queued seeks execute at a slightly later point
    if (mpctx->seek.type) {
        mp_wakeup_core(mpctx);
        return;
    }
    // repeat currently ongoing seeks
    if (mpctx->current_seek.type) {
        mpctx->seek = mpctx->current_seek;
        mp_wakeup_core(mpctx);
        return;
    }
    queue_seek(mpctx, MPSEEK_ABSOLUTE, get_current_time(mpctx), min_prec, 0);
}

void update_vo_playback_state(struct MPContext *mpctx)
{
    if (mpctx->video_out && mpctx->video_out->config_ok) {
        struct voctrl_playback_state oldstate = mpctx->vo_playback_state;
        struct voctrl_playback_state newstate = {
            .taskbar_progress = mpctx->opts->vo->taskbar_progress,
            .playing = mpctx->playing,
            .paused = mpctx->paused,
            .percent_pos = get_percent_pos(mpctx),
        };

        if (oldstate.taskbar_progress != newstate.taskbar_progress ||
            oldstate.playing != newstate.playing ||
            oldstate.paused != newstate.paused ||
            oldstate.percent_pos != newstate.percent_pos)
        {
            // Don't update progress bar if it was and still is hidden
            if ((oldstate.playing && oldstate.taskbar_progress) ||
                (newstate.playing && newstate.taskbar_progress))
            {
                vo_control_async(mpctx->video_out,
                                 VOCTRL_UPDATE_PLAYBACK_STATE, &newstate);
            }
            mpctx->vo_playback_state = newstate;
        }
    } else {
        mpctx->vo_playback_state = (struct voctrl_playback_state){ 0 };
    }
}

void update_window_title(struct MPContext *mpctx, bool force)
{
    if (!mpctx->video_out && !mpctx->ao) {
        talloc_free(mpctx->last_window_title);
        mpctx->last_window_title = NULL;
        return;
    }
    char *title = mp_property_expand_string(mpctx, mpctx->opts->wintitle);
    if (!mpctx->last_window_title || force ||
        strcmp(title, mpctx->last_window_title) != 0)
    {
        talloc_free(mpctx->last_window_title);
        mpctx->last_window_title = talloc_steal(mpctx, title);

        if (mpctx->video_out)
            vo_control(mpctx->video_out, VOCTRL_UPDATE_WINDOW_TITLE, title);

        if (mpctx->ao) {
            ao_control(mpctx->ao, AOCONTROL_UPDATE_STREAM_TITLE, title);
        }
    } else {
        talloc_free(title);
    }
}

void error_on_track(struct MPContext *mpctx, struct track *track)
{
    if (!track || !track->selected)
        return;
    mp_deselect_track(mpctx, track);
    if (track->type == STREAM_AUDIO)
        MP_INFO(mpctx, "Audio: no audio\n");
    if (track->type == STREAM_VIDEO)
        MP_INFO(mpctx, "Video: no video\n");
    if (mpctx->opts->stop_playback_on_init_failure ||
        !(mpctx->vo_chain || mpctx->ao_chain))
    {
        if (!mpctx->stop_play)
            mpctx->stop_play = PT_ERROR;
        if (mpctx->error_playing >= 0)
            mpctx->error_playing = MPV_ERROR_NOTHING_TO_PLAY;
    }
    mp_wakeup_core(mpctx);
}

int stream_dump(struct MPContext *mpctx, const char *source_filename)
{
    struct MPOpts *opts = mpctx->opts;
    stream_t *stream = stream_open(source_filename, mpctx->global);
    if (!stream)
        return -1;

    int64_t size = stream_get_size(stream);

    FILE *dest = fopen(opts->stream_dump, "wb");
    if (!dest) {
        MP_ERR(mpctx, "Error opening dump file: %s\n", mp_strerror(errno));
        return -1;
    }

    bool ok = true;

    while (mpctx->stop_play == KEEP_PLAYING && ok) {
        if (!opts->quiet && ((stream->pos / (1024 * 1024)) % 2) == 1) {
            uint64_t pos = stream->pos;
            MP_MSG(mpctx, MSGL_STATUS, "Dumping %lld/%lld...",
                   (long long int)pos, (long long int)size);
        }
        bstr data = stream_peek(stream, 4096);
        if (data.len == 0) {
            ok &= stream->eof;
            break;
        }
        ok &= fwrite(data.start, data.len, 1, dest) == 1;
        stream_skip(stream, data.len);
        mp_wakeup_core(mpctx); // don't actually sleep
        mp_idle(mpctx); // but process input
    }

    ok &= fclose(dest) == 0;
    free_stream(stream);
    return ok ? 0 : -1;
}

void merge_playlist_files(struct playlist *pl)
{
    if (!pl->first)
        return;
    char *edl = talloc_strdup(NULL, "edl://");
    for (struct playlist_entry *e = pl->first; e; e = e->next) {
        if (e != pl->first)
            edl = talloc_strdup_append_buffer(edl, ";");
        // Escape if needed
        if (e->filename[strcspn(e->filename, "=%,;\n")] ||
            bstr_strip(bstr0(e->filename)).len != strlen(e->filename))
        {
            // %length%
            edl = talloc_asprintf_append_buffer(edl, "%%%zd%%", strlen(e->filename));
        }
        edl = talloc_strdup_append_buffer(edl, e->filename);
    }
    playlist_clear(pl);
    playlist_add_file(pl, edl);
    talloc_free(edl);
}
