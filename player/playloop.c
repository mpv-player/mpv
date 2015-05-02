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
#include "common/encode.h"
#include "options/m_property.h"
#include "common/playlist.h"
#include "input/input.h"

#include "misc/dispatch.h"
#include "osdep/terminal.h"
#include "osdep/timer.h"

#include "audio/mixer.h"
#include "audio/decode/dec_audio.h"
#include "audio/filter/af.h"
#include "audio/out/ao.h"
#include "demux/demux.h"
#include "stream/stream.h"
#include "sub/osd.h"
#include "video/filter/vf.h"
#include "video/decode/dec_video.h"
#include "video/out/vo.h"

#include "core.h"
#include "client.h"
#include "command.h"

// Wait until mp_input_wakeup(mpctx->input) is called, since the last time
// mp_wait_events() was called. (But see mp_process_input().)
void mp_wait_events(struct MPContext *mpctx, double sleeptime)
{
    mp_input_wait(mpctx->input, sleeptime);
}

// Process any queued input, whether it's user input, or requests from client
// API threads. This also resets the "wakeup" flag used with mp_wait_events().
void mp_process_input(struct MPContext *mpctx)
{
    mp_dispatch_queue_process(mpctx->dispatch, 0);
    for (;;) {
        mp_cmd_t *cmd = mp_input_read_cmd(mpctx->input);
        if (!cmd)
            break;
        run_command(mpctx, cmd, NULL);
        mp_cmd_free(cmd);
        mp_dispatch_queue_process(mpctx->dispatch, 0);
    }
}

void pause_player(struct MPContext *mpctx)
{
    mpctx->opts->pause = 1;

    if (mpctx->video_out)
        vo_control(mpctx->video_out, VOCTRL_RESTORE_SCREENSAVER, NULL);

    if (mpctx->paused)
        goto end;
    mpctx->paused = true;
    mpctx->step_frames = 0;
    mpctx->time_frame -= get_relative_time(mpctx);
    mpctx->osd_function = 0;
    mpctx->osd_force_update = true;
    mpctx->paused_for_cache = false;

    if (mpctx->ao && mpctx->d_audio)
        ao_pause(mpctx->ao);
    if (mpctx->video_out)
        vo_set_paused(mpctx->video_out, true);

end:
    mp_notify(mpctx, mpctx->opts->pause ? MPV_EVENT_PAUSE : MPV_EVENT_UNPAUSE, 0);
}

void unpause_player(struct MPContext *mpctx)
{
    mpctx->opts->pause = 0;

    if (mpctx->video_out && mpctx->opts->stop_screensaver)
        vo_control(mpctx->video_out, VOCTRL_KILL_SCREENSAVER, NULL);

    if (!mpctx->paused)
        goto end;
    // Don't actually unpause while cache is loading.
    if (mpctx->paused_for_cache)
        goto end;
    mpctx->paused = false;
    mpctx->osd_function = 0;
    mpctx->osd_force_update = true;

    if (mpctx->ao && mpctx->d_audio)
        ao_resume(mpctx->ao);
    if (mpctx->video_out)
        vo_set_paused(mpctx->video_out, false);

    (void)get_relative_time(mpctx);     // ignore time that passed during pause

end:
    mp_notify(mpctx, mpctx->opts->pause ? MPV_EVENT_PAUSE : MPV_EVENT_UNPAUSE, 0);
}

void add_step_frame(struct MPContext *mpctx, int dir)
{
    if (!mpctx->d_video)
        return;
    if (dir > 0) {
        mpctx->step_frames += 1;
        unpause_player(mpctx);
    } else if (dir < 0) {
        if (!mpctx->backstep_active && !mpctx->hrseek_active) {
            mpctx->backstep_active = true;
            mpctx->backstep_start_seek_ts = mpctx->vo_pts_history_seek_ts;
            pause_player(mpctx);
        }
    }
}

// Clear some playback-related fields on file loading or after seeks.
void reset_playback_state(struct MPContext *mpctx)
{
    reset_video_state(mpctx);
    reset_audio_state(mpctx);
    reset_subtitle_state(mpctx);

    mpctx->hrseek_active = false;
    mpctx->hrseek_framedrop = false;
    mpctx->hrseek_lastframe = false;
    mpctx->playback_pts = MP_NOPTS_VALUE;
    mpctx->last_seek_pts = MP_NOPTS_VALUE;
    mpctx->cache_wait_time = 0;
    mpctx->step_frames = 0;
    mpctx->restart_complete = false;

#if HAVE_ENCODING
    encode_lavc_discontinuity(mpctx->encode_lavc_ctx);
#endif
}

// return -1 if seek failed (non-seekable stream?), 0 otherwise
static int mp_seek(MPContext *mpctx, struct seek_params seek,
                   bool timeline_fallthrough)
{
    struct MPOpts *opts = mpctx->opts;
    uint64_t prev_seek_ts = mpctx->vo_pts_history_seek_ts;
    int prev_step = mpctx->step_frames;

    if (!mpctx->demuxer)
        return -1;

    if (!mpctx->demuxer->seekable) {
        MP_ERR(mpctx, "Can't seek in this file.\n");
        return -1;
    }

    if (mpctx->stop_play == AT_END_OF_FILE)
        mpctx->stop_play = KEEP_PLAYING;

    double hr_seek_offset = opts->hr_seek_demuxer_offset;
    bool hr_seek_very_exact = seek.exact == MPSEEK_VERY_EXACT;
    // Always try to compensate for possibly bad demuxers in "special"
    // situations where we need more robustness from the hr-seek code, even
    // if the user doesn't use --hr-seek-demuxer-offset.
    // The value is arbitrary, but should be "good enough" in most situations.
    if (hr_seek_very_exact)
        hr_seek_offset = MPMAX(hr_seek_offset, 0.5); // arbitrary

    bool hr_seek = opts->correct_pts && seek.exact != MPSEEK_KEYFRAME;
    hr_seek &= (opts->hr_seek == 0 && seek.type == MPSEEK_ABSOLUTE) ||
               opts->hr_seek > 0 || seek.exact >= MPSEEK_EXACT;
    if (seek.type == MPSEEK_FACTOR || seek.amount < 0 ||
        (seek.type == MPSEEK_ABSOLUTE && seek.amount < mpctx->last_chapter_pts))
        mpctx->last_chapter_seek = -2;
    if (seek.type == MPSEEK_FACTOR && !mpctx->demuxer->ts_resets_possible) {
        double len = get_time_length(mpctx);
        if (len >= 0) {
            seek.amount = seek.amount * len + get_start_time(mpctx);
            seek.type = MPSEEK_ABSOLUTE;
        }
    }
    int direction = 0;
    if (seek.type == MPSEEK_RELATIVE && (!mpctx->demuxer->rel_seeks || hr_seek)) {
        seek.type = MPSEEK_ABSOLUTE;
        direction = seek.amount > 0 ? 1 : -1;
        seek.amount += get_current_time(mpctx);
    }
    hr_seek &= seek.type == MPSEEK_ABSOLUTE; // otherwise, no target PTS known

    double demuxer_amount = seek.amount;
    if (mpctx->timeline) {
        bool need_reset = false;
        demuxer_amount = timeline_set_from_time(mpctx, seek.amount,
                                                &need_reset);
        if (need_reset) {
            reinit_video_chain(mpctx);
            reinit_audio_chain(mpctx);
            reinit_subs(mpctx, 0);
            reinit_subs(mpctx, 1);
        }
    }

    int demuxer_style = 0;
    switch (seek.type) {
    case MPSEEK_FACTOR:
        demuxer_style |= SEEK_ABSOLUTE | SEEK_FACTOR;
        break;
    case MPSEEK_ABSOLUTE:
        demuxer_style |= SEEK_ABSOLUTE;
        break;
    }
    if (hr_seek || direction < 0) {
        demuxer_style |= SEEK_BACKWARD;
    } else if (direction > 0) {
        demuxer_style |= SEEK_FORWARD;
    }
    if (hr_seek)
        demuxer_style |= SEEK_HR;

    if (hr_seek)
        demuxer_amount -= hr_seek_offset;
    demux_seek(mpctx->demuxer, demuxer_amount, demuxer_style);

    // Seek external, extra files too:
    for (int t = 0; t < mpctx->num_tracks; t++) {
        struct track *track = mpctx->tracks[t];
        if (track->selected && track->is_external && track->demuxer) {
            double main_new_pos = seek.amount;
            if (seek.type != MPSEEK_ABSOLUTE)
                main_new_pos = get_main_demux_pts(mpctx);
            main_new_pos -= get_track_video_offset(mpctx, track);
            demux_seek(track->demuxer, main_new_pos, SEEK_ABSOLUTE | SEEK_BACKWARD);
        }
    }

    if (!timeline_fallthrough)
        clear_audio_output_buffers(mpctx);

    reset_playback_state(mpctx);

    if (timeline_fallthrough) {
        // Important if video reinit happens.
        mpctx->vo_pts_history_seek_ts = prev_seek_ts;
        mpctx->step_frames = prev_step;
    } else {
        mpctx->vo_pts_history_seek_ts++;
        mpctx->backstep_active = false;
    }

    /* Use the target time as "current position" for further relative
     * seeks etc until a new video frame has been decoded */
    if (seek.type == MPSEEK_ABSOLUTE)
        mpctx->last_seek_pts = seek.amount;

    // The hr_seek==false case is for skipping frames with PTS before the
    // current timeline chapter start. It's not really known where the demuxer
    // level seek will end up, so the hrseek mechanism is abused to skip all
    // frames before chapter start by setting hrseek_pts to the chapter start.
    // It does nothing when the seek is inside of the current chapter, and
    // seeking past the chapter is handled elsewhere.
    if (hr_seek || mpctx->timeline) {
        mpctx->hrseek_active = true;
        mpctx->hrseek_framedrop = !hr_seek_very_exact;
        mpctx->hrseek_pts = hr_seek ? seek.amount
                                 : mpctx->timeline[mpctx->timeline_part].start;
    }

    mpctx->start_timestamp = mp_time_sec();
    mpctx->sleeptime = 0;

    mp_notify(mpctx, MPV_EVENT_SEEK, NULL);
    mp_notify(mpctx, MPV_EVENT_TICK, NULL);

    return 0;
}

// This combines consecutive seek requests.
void queue_seek(struct MPContext *mpctx, enum seek_type type, double amount,
                enum seek_precision exact, bool immediate)
{
    struct seek_params *seek = &mpctx->seek;
    switch (type) {
    case MPSEEK_RELATIVE:
        seek->immediate |= immediate;
        if (seek->type == MPSEEK_FACTOR)
            return;  // Well... not common enough to bother doing better
        seek->amount += amount;
        seek->exact = MPMAX(seek->exact, exact);
        if (seek->type == MPSEEK_NONE)
            seek->exact = exact;
        if (seek->type == MPSEEK_ABSOLUTE)
            return;
        seek->type = MPSEEK_RELATIVE;
        return;
    case MPSEEK_ABSOLUTE:
    case MPSEEK_FACTOR:
        *seek = (struct seek_params) {
            .type = type,
            .amount = amount,
            .exact = exact,
            .immediate = immediate,
        };
        return;
    case MPSEEK_NONE:
        *seek = (struct seek_params){ 0 };
        return;
    }
    abort();
}

void execute_queued_seek(struct MPContext *mpctx)
{
    if (mpctx->seek.type) {
        // Let explicitly imprecise seeks cancel precise seeks:
        if (mpctx->hrseek_active && mpctx->seek.exact == MPSEEK_KEYFRAME)
            mpctx->start_timestamp = -1e9;
        /* If the user seeks continuously (keeps arrow key down)
         * try to finish showing a frame from one location before doing
         * another seek (which could lead to unchanging display). */
        if (!mpctx->seek.immediate && mpctx->video_status < STATUS_READY &&
            mp_time_sec() - mpctx->start_timestamp < 0.3)
            return;
        mp_seek(mpctx, mpctx->seek, false);
        mpctx->seek = (struct seek_params){0};
    }
}

// -1 if unknown
double get_time_length(struct MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return -1;

    if (mpctx->timeline)
        return mpctx->timeline[mpctx->num_timeline_parts].start;

    double len = demuxer_get_time_length(demuxer);
    if (len >= 0)
        return len;

    return -1; // unknown
}

double get_current_time(struct MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return 0;
    if (mpctx->playback_pts != MP_NOPTS_VALUE)
        return mpctx->playback_pts;
    if (mpctx->last_seek_pts != MP_NOPTS_VALUE)
        return mpctx->last_seek_pts;
    return 0;
}

double get_playback_time(struct MPContext *mpctx)
{
    double cur = get_current_time(mpctx);
    double start = get_start_time(mpctx);
    // During seeking, the time corresponds to the last seek time - apply some
    // cosmetics to it.
    if (mpctx->playback_pts == MP_NOPTS_VALUE) {
        double length = get_time_length(mpctx);
        if (length >= 0)
            cur = MPCLAMP(cur, start, start + length);
    }
    return cur >= start ? cur - start : cur;
}

// Return playback position in 0.0-1.0 ratio, or -1 if unknown.
double get_current_pos_ratio(struct MPContext *mpctx, bool use_range)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return -1;
    double ans = -1;
    double start = get_start_time(mpctx);
    double len = get_time_length(mpctx);
    if (use_range) {
        double startpos = rel_time_to_abs(mpctx, mpctx->opts->play_start);
        double endpos = get_play_end_pts(mpctx);
        if (endpos == MP_NOPTS_VALUE || endpos > start + MPMAX(0, len))
            endpos = start + MPMAX(0, len);
        if (startpos == MP_NOPTS_VALUE || startpos < start)
            startpos = start;
        if (endpos < startpos)
            endpos = startpos;
        start = startpos;
        len = endpos - startpos;
    }
    double pos = get_current_time(mpctx);
    if (len > 0)
        ans = MPCLAMP((pos - start) / len, 0, 1);
    if (ans < 0 || demuxer->ts_resets_possible) {
        int64_t size;
        if (demux_stream_control(demuxer, STREAM_CTRL_GET_SIZE, &size) > 0) {
            if (size > 0 && demuxer->filepos >= 0)
                ans = MPCLAMP(demuxer->filepos / (double)size, 0, 1);
        }
    }
    if (use_range) {
        if (mpctx->opts->play_frames > 0)
            ans = MPMAX(ans, 1.0 -
                    mpctx->max_frames / (double) mpctx->opts->play_frames);
    }
    return ans;
}

// 0-100, -1 if unknown
int get_percent_pos(struct MPContext *mpctx)
{
    double pos = get_current_pos_ratio(mpctx, false);
    return pos < 0 ? -1 : pos * 100;
}

// -2 is no chapters, -1 is before first chapter
int get_current_chapter(struct MPContext *mpctx)
{
    if (!mpctx->num_chapters)
        return -2;
    double current_pts = get_current_time(mpctx);
    int i;
    for (i = 0; i < mpctx->num_chapters; i++)
        if (current_pts < mpctx->chapters[i].pts)
            break;
    return MPMAX(mpctx->last_chapter_seek, i - 1);
}

char *chapter_display_name(struct MPContext *mpctx, int chapter)
{
    char *name = chapter_name(mpctx, chapter);
    char *dname = NULL;
    if (name) {
        dname = talloc_asprintf(NULL, "(%d) %s", chapter + 1, name);
    } else if (chapter < -1) {
        dname = talloc_strdup(NULL, "(unavailable)");
    } else {
        int chapter_count = get_chapter_count(mpctx);
        if (chapter_count <= 0)
            dname = talloc_asprintf(NULL, "(%d)", chapter + 1);
        else
            dname = talloc_asprintf(NULL, "(%d) of %d", chapter + 1,
                                    chapter_count);
    }
    talloc_free(name);
    return dname;
}

// returns NULL if chapter name unavailable
char *chapter_name(struct MPContext *mpctx, int chapter)
{
    if (chapter < 0 || chapter >= mpctx->num_chapters)
        return NULL;
    return talloc_strdup(NULL, mpctx->chapters[chapter].name);
}

// returns the start of the chapter in seconds (-1 if unavailable)
double chapter_start_time(struct MPContext *mpctx, int chapter)
{
    if (chapter == -1)
        return get_start_time(mpctx);
    if (chapter >= 0 && chapter < mpctx->num_chapters)
        return mpctx->chapters[chapter].pts;
    return MP_NOPTS_VALUE;
}

int get_chapter_count(struct MPContext *mpctx)
{
    return mpctx->num_chapters;
}

// Seek to a given chapter. Queues the seek.
bool mp_seek_chapter(struct MPContext *mpctx, int chapter)
{
    int num = get_chapter_count(mpctx);
    if (num == 0)
        return false;
    if (chapter < -1 || chapter >= num)
        return false;

    mpctx->last_chapter_seek = -2;

    double pts = chapter_start_time(mpctx, chapter);
    if (pts == MP_NOPTS_VALUE)
        return false;

    queue_seek(mpctx, MPSEEK_ABSOLUTE, pts, MPSEEK_DEFAULT, true);
    mpctx->last_chapter_seek = chapter;
    mpctx->last_chapter_pts = pts;
    return true;
}

static void handle_osd_redraw(struct MPContext *mpctx)
{
    if (!mpctx->video_out || !mpctx->video_out->config_ok)
        return;
    // If we're playing normally, let OSD be redrawn naturally as part of
    // video display.
    if (!mpctx->paused) {
        if (mpctx->sleeptime < 0.1 && mpctx->video_status == STATUS_PLAYING)
            return;
    }
    // Don't redraw immediately during a seek (makes it significantly slower).
    if (mpctx->d_video && mp_time_sec() - mpctx->start_timestamp < 0.1) {
        mpctx->sleeptime = MPMIN(mpctx->sleeptime, 0.1);
        return;
    }
    bool want_redraw = osd_query_and_reset_want_redraw(mpctx->osd) ||
                       vo_want_redraw(mpctx->video_out);
    if (!want_redraw)
        return;
    vo_redraw(mpctx->video_out);
    mpctx->sleeptime = 0;
}

static void handle_pause_on_low_cache(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    if (!mpctx->demuxer)
        return;

    int idle = -1;
    demux_stream_control(mpctx->demuxer, STREAM_CTRL_GET_CACHE_IDLE, &idle);

    struct demux_ctrl_reader_state s = {.idle = true, .ts_duration = -1};
    demux_control(mpctx->demuxer, DEMUXER_CTRL_GET_READER_STATE, &s);

    if (mpctx->restart_complete && idle != -1) {
        if (mpctx->paused && mpctx->paused_for_cache) {
            if (!opts->cache_pausing || s.ts_duration >= mpctx->cache_wait_time
                || s.idle)
            {
                double elapsed_time = mp_time_sec() - mpctx->cache_stop_time;
                if (elapsed_time > mpctx->cache_wait_time) {
                    mpctx->cache_wait_time *= 1.5 + 0.1;
                } else {
                    mpctx->cache_wait_time /= 1.5 - 0.1;
                }
                mpctx->paused_for_cache = false;
                if (!opts->pause)
                    unpause_player(mpctx);
                mp_notify(mpctx, MP_EVENT_CACHE_UPDATE, NULL);
            }
            mpctx->sleeptime = MPMIN(mpctx->sleeptime, 0.2);
        } else {
            if (opts->cache_pausing && s.underrun) {
                bool prev_paused_user = opts->pause;
                pause_player(mpctx);
                mpctx->paused_for_cache = true;
                opts->pause = prev_paused_user;
                mpctx->cache_stop_time = mp_time_sec();
                mp_notify(mpctx, MP_EVENT_CACHE_UPDATE, NULL);
            }
        }
        mpctx->cache_wait_time = MPCLAMP(mpctx->cache_wait_time, 1, 10);
    }

    // Also update cache properties.
    bool busy = idle == 0;
    if (!s.idle) {
        busy |= idle != -1;
        busy |= mp_client_event_is_registered(mpctx, MP_EVENT_CACHE_UPDATE);
    }
    if (busy || mpctx->next_cache_update > 0) {
        double now = mp_time_sec();
        if (mpctx->next_cache_update <= now) {
            mpctx->next_cache_update = busy ? now + 0.25 : 0;
            mp_notify(mpctx, MP_EVENT_CACHE_UPDATE, NULL);
        }
        if (mpctx->next_cache_update > 0) {
            mpctx->sleeptime =
                MPMIN(mpctx->sleeptime, mpctx->next_cache_update - now);
        }
    }
}

double get_cache_buffering_percentage(struct MPContext *mpctx)
{
    if (mpctx->demuxer && mpctx->paused_for_cache && mpctx->cache_wait_time > 0) {
        struct demux_ctrl_reader_state s = {.idle = true, .ts_duration = -1};
        demux_control(mpctx->demuxer, DEMUXER_CTRL_GET_READER_STATE, &s);
        if (s.ts_duration < 0)
            s.ts_duration = 0;

        return MPCLAMP(s.ts_duration / mpctx->cache_wait_time, 0.0, 1.0);
    }
    if (mpctx->demuxer && !mpctx->paused_for_cache)
        return 1.0;
    return -1;
}

static void handle_heartbeat_cmd(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    if (opts->heartbeat_cmd && !mpctx->paused && mpctx->video_out) {
        double now = mp_time_sec();
        if (mpctx->next_heartbeat <= now) {
            mpctx->next_heartbeat = now + opts->heartbeat_interval;
            system(opts->heartbeat_cmd);
        }
        mpctx->sleeptime = MPMIN(mpctx->sleeptime, mpctx->next_heartbeat - now);
    }
}

static void handle_cursor_autohide(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct vo *vo = mpctx->video_out;

    if (!vo)
        return;

    bool mouse_cursor_visible = mpctx->mouse_cursor_visible;
    double now = mp_time_sec();

    unsigned mouse_event_ts = mp_input_get_mouse_event_counter(mpctx->input);
    if (mpctx->mouse_event_ts != mouse_event_ts) {
        mpctx->mouse_event_ts = mouse_event_ts;
        mpctx->mouse_timer = now + opts->cursor_autohide_delay / 1000.0;
        mouse_cursor_visible = true;
    }

    if (mpctx->mouse_timer > now) {
        mpctx->sleeptime = MPMIN(mpctx->sleeptime, mpctx->mouse_timer - now);
    } else {
        mouse_cursor_visible = false;
    }

    if (opts->cursor_autohide_delay == -1)
        mouse_cursor_visible = true;

    if (opts->cursor_autohide_delay == -2)
        mouse_cursor_visible = false;

    if (opts->cursor_autohide_fs && !opts->vo.fullscreen)
        mouse_cursor_visible = true;

    if (mouse_cursor_visible != mpctx->mouse_cursor_visible)
        vo_control(vo, VOCTRL_SET_CURSOR_VISIBILITY, &mouse_cursor_visible);
    mpctx->mouse_cursor_visible = mouse_cursor_visible;
}

static void handle_vo_events(struct MPContext *mpctx)
{
    struct vo *vo = mpctx->video_out;
    int events = vo ? vo_query_and_reset_events(vo, VO_EVENTS_USER) : 0;
    if (events & VO_EVENT_RESIZE)
        mp_notify(mpctx, MP_EVENT_WIN_RESIZE, NULL);
    if (events & VO_EVENT_WIN_STATE)
        mp_notify(mpctx, MP_EVENT_WIN_STATE, NULL);
}

void add_frame_pts(struct MPContext *mpctx, double pts)
{
    if (pts == MP_NOPTS_VALUE || mpctx->hrseek_framedrop) {
        mpctx->vo_pts_history_seek_ts++; // mark discontinuity
        return;
    }
    if (mpctx->vo_pts_history_pts[0] == pts) // may be called multiple times
        return;
    for (int n = MAX_NUM_VO_PTS - 1; n >= 1; n--) {
        mpctx->vo_pts_history_seek[n] = mpctx->vo_pts_history_seek[n - 1];
        mpctx->vo_pts_history_pts[n] = mpctx->vo_pts_history_pts[n - 1];
    }
    mpctx->vo_pts_history_seek[0] = mpctx->vo_pts_history_seek_ts;
    mpctx->vo_pts_history_pts[0] = pts;
}

static double find_previous_pts(struct MPContext *mpctx, double pts)
{
    for (int n = 0; n < MAX_NUM_VO_PTS - 1; n++) {
        if (pts == mpctx->vo_pts_history_pts[n] &&
            mpctx->vo_pts_history_seek[n] != 0 &&
            mpctx->vo_pts_history_seek[n] == mpctx->vo_pts_history_seek[n + 1])
        {
            return mpctx->vo_pts_history_pts[n + 1];
        }
    }
    return MP_NOPTS_VALUE;
}

static double get_last_frame_pts(struct MPContext *mpctx)
{
    if (mpctx->vo_pts_history_seek[0] == mpctx->vo_pts_history_seek_ts)
        return mpctx->vo_pts_history_pts[0];
    return MP_NOPTS_VALUE;
}

static void handle_backstep(struct MPContext *mpctx)
{
    if (!mpctx->backstep_active)
        return;

    double current_pts = mpctx->last_vo_pts;
    mpctx->backstep_active = false;
    if (mpctx->d_video && current_pts != MP_NOPTS_VALUE) {
        double seek_pts = find_previous_pts(mpctx, current_pts);
        if (seek_pts != MP_NOPTS_VALUE) {
            queue_seek(mpctx, MPSEEK_ABSOLUTE, seek_pts, MPSEEK_VERY_EXACT, true);
        } else {
            double last = get_last_frame_pts(mpctx);
            if (last != MP_NOPTS_VALUE && last >= current_pts &&
                mpctx->backstep_start_seek_ts != mpctx->vo_pts_history_seek_ts)
            {
                MP_ERR(mpctx, "Backstep failed.\n");
                queue_seek(mpctx, MPSEEK_ABSOLUTE, current_pts,
                           MPSEEK_VERY_EXACT, true);
            } else if (!mpctx->hrseek_active) {
                MP_VERBOSE(mpctx, "Start backstep indexing.\n");
                // Force it to index the video up until current_pts.
                // The whole point is getting frames _before_ that PTS,
                // so apply an arbitrary offset. (In theory the offset
                // has to be large enough to reach the previous frame.)
                mp_seek(mpctx, (struct seek_params){
                               .type = MPSEEK_ABSOLUTE,
                               .amount = current_pts - 1.0,
                               }, false);
                // Don't leave hr-seek mode. If all goes right, hr-seek
                // mode is cancelled as soon as the frame before
                // current_pts is found during hr-seeking.
                // Note that current_pts should be part of the index,
                // otherwise we can't find the previous frame, so set the
                // seek target an arbitrary amount of time after it.
                if (mpctx->hrseek_active) {
                    mpctx->hrseek_pts = current_pts + 10.0;
                    mpctx->hrseek_framedrop = false;
                    mpctx->backstep_active = true;
                }
            } else {
                mpctx->backstep_active = true;
            }
        }
    }
}

static void handle_sstep(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    if (mpctx->stop_play || !mpctx->restart_complete)
        return;

    if (opts->step_sec > 0 && !mpctx->paused) {
        set_osd_function(mpctx, OSD_FFW);
        queue_seek(mpctx, MPSEEK_RELATIVE, opts->step_sec, MPSEEK_DEFAULT, true);
    }

    if (mpctx->video_status >= STATUS_EOF) {
        if (mpctx->max_frames >= 0)
            mpctx->stop_play = AT_END_OF_FILE; // force EOF even if audio left
        if (mpctx->step_frames > 0 && !mpctx->paused)
            pause_player(mpctx);
    }
}

static void handle_loop_file(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    if (opts->loop_file && mpctx->stop_play == AT_END_OF_FILE) {
        mpctx->stop_play = KEEP_PLAYING;
        set_osd_function(mpctx, OSD_FFW);
        queue_seek(mpctx, MPSEEK_ABSOLUTE, get_start_time(mpctx),
                   MPSEEK_DEFAULT, true);
        if (opts->loop_file > 0)
            opts->loop_file--;
    }
}

void seek_to_last_frame(struct MPContext *mpctx)
{
    if (!mpctx->d_video)
        return;
    if (mpctx->hrseek_lastframe) // exit if we already tried this
        return;
    MP_VERBOSE(mpctx, "seeking to last frame...\n");
    // Approximately seek close to the end of the file.
    // Usually, it will seek some seconds before end.
    double end = get_play_end_pts(mpctx);
    if (end == MP_NOPTS_VALUE)
        end = get_time_length(mpctx);
    mp_seek(mpctx, (struct seek_params){
                   .type = MPSEEK_ABSOLUTE,
                   .amount = end,
                   .exact = MPSEEK_VERY_EXACT,
                   }, false);
    // Make it exact: stop seek only if last frame was reached.
    if (mpctx->hrseek_active) {
        mpctx->hrseek_pts = 1e99; // "infinite"
        mpctx->hrseek_lastframe = true;
    }
}

static void handle_keep_open(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    if (opts->keep_open && mpctx->stop_play == AT_END_OF_FILE &&
        (opts->keep_open == 2 || !playlist_get_next(mpctx->playlist, 1)) &&
        opts->loop_times == 1)
    {
        mpctx->stop_play = KEEP_PLAYING;
        if (mpctx->d_video) {
            if (!vo_has_frame(mpctx->video_out)) // EOF not reached normally
                seek_to_last_frame(mpctx);
            mpctx->playback_pts = mpctx->last_vo_pts;
        }
        if (!mpctx->opts->pause)
            pause_player(mpctx);
    }
}

static void handle_chapter_change(struct MPContext *mpctx)
{
    int chapter = get_current_chapter(mpctx);
    if (chapter != mpctx->last_chapter) {
        mpctx->last_chapter = chapter;
        mp_notify(mpctx, MPV_EVENT_CHAPTER_CHANGE, NULL);
    }
}

// Execute a forceful refresh of the VO window, if it hasn't had a valid frame
// for a while. The problem is that a VO with no valid frame (vo->hasframe==0)
// doesn't redraw video and doesn't OSD interaction. So screw it, hard.
// It also closes the VO if force_window or video display is not active.
void handle_force_window(struct MPContext *mpctx, bool reconfig)
{
    // Don't interfere with real video playback
    if (mpctx->d_video)
        return;

    if (!mpctx->opts->force_vo && mpctx->video_out)
        uninit_video_out(mpctx);

    if (mpctx->video_out && (!mpctx->video_out->config_ok || reconfig)) {
        struct vo *vo = mpctx->video_out;
        MP_INFO(mpctx, "Creating non-video VO window.\n");
        // Pick whatever works
        int config_format = 0;
        uint8_t fmts[IMGFMT_END - IMGFMT_START] = {0};
        vo_query_formats(vo, fmts);
        for (int fmt = IMGFMT_START; fmt < IMGFMT_END; fmt++) {
            if (fmts[fmt - IMGFMT_START]) {
                config_format = fmt;
                break;
            }
        }
        int w = 960;
        int h = 480;
        struct mp_image_params p = {
            .imgfmt = config_format,
            .w = w,   .h = h,
            .d_w = w, .d_h = h,
        };
        if (vo_reconfig(vo, &p, 0) < 0) {
            mpctx->opts->force_vo = 0;
            uninit_video_out(mpctx);
            return;
        }
        vo_control(vo, VOCTRL_RESTORE_SCREENSAVER, NULL);
        vo_set_paused(vo, true);
        vo_redraw(vo);
        mp_notify(mpctx, MPV_EVENT_VIDEO_RECONFIG, NULL);
    }
}

// Potentially needed by some Lua scripts, which assume TICK always comes.
static void handle_dummy_ticks(struct MPContext *mpctx)
{
    if (mpctx->video_status == STATUS_EOF || mpctx->paused) {
        if (mp_time_sec() - mpctx->last_idle_tick > 0.5) {
            mpctx->last_idle_tick = mp_time_sec();
            mp_notify(mpctx, MPV_EVENT_TICK, NULL);
        }
    }
}

// We always make sure audio and video buffers are filled before actually
// starting playback. This code handles starting them at the same time.
static void handle_playback_restart(struct MPContext *mpctx, double endpts)
{
    struct MPOpts *opts = mpctx->opts;

    if (mpctx->audio_status < STATUS_READY ||
        mpctx->video_status < STATUS_READY)
        return;

    if (mpctx->video_status == STATUS_READY) {
        mpctx->video_status = STATUS_PLAYING;
        get_relative_time(mpctx);
        mpctx->sleeptime = 0;
    }

    if (mpctx->audio_status == STATUS_READY)
        fill_audio_out_buffers(mpctx, endpts); // actually play prepared buffer

    if (!mpctx->restart_complete) {
        mpctx->hrseek_active = false;
        mpctx->restart_complete = true;
        mp_notify(mpctx, MPV_EVENT_PLAYBACK_RESTART, NULL);
        if (!mpctx->playing_msg_shown) {
            if (opts->playing_msg && opts->playing_msg[0]) {
                char *msg =
                    mp_property_expand_escaped_string(mpctx, opts->playing_msg);
                MP_INFO(mpctx, "%s\n", msg);
                talloc_free(msg);
            }
            if (opts->osd_playing_msg && opts->osd_playing_msg[0]) {
                char *msg =
                    mp_property_expand_escaped_string(mpctx, opts->osd_playing_msg);
                set_osd_msg(mpctx, 1, opts->osd_duration, "%s", msg);
                talloc_free(msg);
            }
        }
        mpctx->playing_msg_shown = true;
    }
}

// Determines whether the end of the current segment is reached, and switch to
// the next one if required. Also handles regular playback end.
static void handle_segment_switch(struct MPContext *mpctx, bool end_is_new_segment)
{
    /* Don't quit while paused and we're displaying the last video frame. On the
     * other hand, if we don't have a video frame, then the user probably seeked
     * outside of the video, and we do want to quit. */
    bool prevent_eof =
        mpctx->paused && mpctx->video_out && vo_has_frame(mpctx->video_out);
    /* It's possible for the user to simultaneously switch both audio
     * and video streams to "disabled" at runtime. Handle this by waiting
     * rather than immediately stopping playback due to EOF.
     */
    if ((mpctx->d_audio || mpctx->d_video) && !prevent_eof &&
        mpctx->audio_status == STATUS_EOF &&
        mpctx->video_status == STATUS_EOF)
    {
        int new_part = mpctx->timeline_part + 1;
        if (end_is_new_segment && new_part < mpctx->num_timeline_parts) {
            mp_seek(mpctx, (struct seek_params){
                           .type = MPSEEK_ABSOLUTE,
                           .amount = mpctx->timeline[new_part].start
                           }, true);
        } else {
            mpctx->stop_play = AT_END_OF_FILE;
        }
    }
}

void run_playloop(struct MPContext *mpctx)
{
    double endpts = get_play_end_pts(mpctx);
    bool end_is_new_segment = false;

#if HAVE_ENCODING
    if (encode_lavc_didfail(mpctx->encode_lavc_ctx)) {
        mpctx->stop_play = PT_QUIT;
        return;
    }
#endif

    update_demuxer_properties(mpctx);

    if (mpctx->timeline) {
        double end = mpctx->timeline[mpctx->timeline_part + 1].start;
        if (endpts == MP_NOPTS_VALUE || end < endpts) {
            end_is_new_segment = true;
            endpts = end;
        }
    }

    handle_cursor_autohide(mpctx);
    handle_vo_events(mpctx);
    handle_heartbeat_cmd(mpctx);
    handle_command_updates(mpctx);

    fill_audio_out_buffers(mpctx, endpts);
    write_video(mpctx, endpts);

    handle_playback_restart(mpctx, endpts);

    // Use the audio timestamp if no video, or video is enabled, but has ended.
    if (mpctx->video_status == STATUS_EOF &&
        mpctx->audio_status >= STATUS_PLAYING &&
        mpctx->audio_status < STATUS_EOF)
    {
        mpctx->playback_pts = playing_audio_pts(mpctx);
    }

    handle_dummy_ticks(mpctx);

    update_osd_msg(mpctx);
    update_subtitles(mpctx);

    handle_segment_switch(mpctx, end_is_new_segment);

    mp_handle_nav(mpctx);

    handle_loop_file(mpctx);

    handle_keep_open(mpctx);

    handle_sstep(mpctx);

    if (mpctx->stop_play)
        return;

    handle_osd_redraw(mpctx);

    mp_wait_events(mpctx, mpctx->sleeptime);
    mpctx->sleeptime = 100.0; // infinite for all practical purposes

    handle_pause_on_low_cache(mpctx);

    mp_process_input(mpctx);

    handle_backstep(mpctx);

    handle_chapter_change(mpctx);

    handle_force_window(mpctx, false);

    execute_queued_seek(mpctx);
}

void mp_idle(struct MPContext *mpctx)
{
    handle_dummy_ticks(mpctx);
    mp_wait_events(mpctx, mpctx->sleeptime);
    mpctx->sleeptime = 100.0;
    mp_process_input(mpctx);
    handle_command_updates(mpctx);
    handle_cursor_autohide(mpctx);
    handle_vo_events(mpctx);
    update_osd_msg(mpctx);
    handle_osd_redraw(mpctx);
}

// Waiting for the slave master to send us a new file to play.
void idle_loop(struct MPContext *mpctx)
{
    // ================= idle loop (STOP state) =========================
    bool need_reinit = true;
    while (mpctx->opts->player_idle_mode && !mpctx->playlist->current
           && mpctx->stop_play != PT_QUIT)
    {
        if (need_reinit) {
            mp_notify(mpctx, MPV_EVENT_IDLE, NULL);
            uninit_audio_out(mpctx);
            handle_force_window(mpctx, true);
            mpctx->sleeptime = 0;
            need_reinit = false;
        }
        mp_idle(mpctx);
    }
}
