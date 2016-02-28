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
#include "mpv_talloc.h"

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

double get_relative_time(struct MPContext *mpctx)
{
    int64_t new_time = mp_time_us();
    int64_t delta = new_time - mpctx->last_time;
    mpctx->last_time = new_time;
    return delta * 0.000001;
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

    if (mpctx->ao && mpctx->ao_chain)
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

    if (mpctx->ao && mpctx->ao_chain)
        ao_resume(mpctx->ao);
    if (mpctx->video_out)
        vo_set_paused(mpctx->video_out, false);

    (void)get_relative_time(mpctx);     // ignore time that passed during pause

end:
    mp_notify(mpctx, mpctx->opts->pause ? MPV_EVENT_PAUSE : MPV_EVENT_UNPAUSE, 0);
}

void add_step_frame(struct MPContext *mpctx, int dir)
{
    if (!mpctx->vo_chain)
        return;
    if (dir > 0) {
        mpctx->step_frames += 1;
        unpause_player(mpctx);
    } else if (dir < 0) {
        if (!mpctx->hrseek_backstep || !mpctx->hrseek_active) {
            queue_seek(mpctx, MPSEEK_BACKSTEP, 0, MPSEEK_VERY_EXACT, true);
            pause_player(mpctx);
        }
    }
}

// Clear some playback-related fields on file loading or after seeks.
void reset_playback_state(struct MPContext *mpctx)
{
    if (mpctx->lavfi)
        lavfi_seek_reset(mpctx->lavfi);

    for (int n = 0; n < mpctx->num_tracks; n++) {
        if (mpctx->tracks[n]->d_video)
            video_reset(mpctx->tracks[n]->d_video);
        if (mpctx->tracks[n]->d_audio)
            audio_reset_decoding(mpctx->tracks[n]->d_audio);
    }

    reset_video_state(mpctx);
    reset_audio_state(mpctx);
    reset_subtitle_state(mpctx);

    mpctx->hrseek_active = false;
    mpctx->hrseek_framedrop = false;
    mpctx->hrseek_lastframe = false;
    mpctx->hrseek_backstep = false;
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
static int mp_seek(MPContext *mpctx, struct seek_params seek)
{
    struct MPOpts *opts = mpctx->opts;

    if (!mpctx->demuxer || seek.type == MPSEEK_NONE || seek.amount == MP_NOPTS_VALUE)
        return -1;

    if (!mpctx->demuxer->seekable) {
        MP_ERR(mpctx, "Can't seek in this file.\n");
        return -1;
    }

    bool hr_seek_very_exact = seek.exact == MPSEEK_VERY_EXACT;
    double current_time = get_current_time(mpctx);
    if (current_time == MP_NOPTS_VALUE)
        current_time = 0;
    double seek_pts = MP_NOPTS_VALUE;
    int demux_flags = 0;

    switch (seek.type) {
    case MPSEEK_ABSOLUTE:
        seek_pts = seek.amount;
        break;
    case MPSEEK_BACKSTEP:
        seek_pts = current_time;
        hr_seek_very_exact = true;
        break;
    case MPSEEK_RELATIVE:
        demux_flags = seek.amount > 0 ? SEEK_FORWARD : SEEK_BACKWARD;
        seek_pts = current_time + seek.amount;
        break;
    case MPSEEK_FACTOR: ;
        double len = get_time_length(mpctx);
        if (len >= 0)
            seek_pts = seek.amount * len;
        demux_flags = seek_pts > current_time ? SEEK_FORWARD : SEEK_BACKWARD;
        break;
    default: abort();
    }

    double demux_pts = seek_pts;

    bool hr_seek = opts->correct_pts && seek.exact != MPSEEK_KEYFRAME &&
                 ((opts->hr_seek == 0 && seek.type == MPSEEK_ABSOLUTE) ||
                  opts->hr_seek > 0 || seek.exact >= MPSEEK_EXACT) &&
                 seek_pts != MP_NOPTS_VALUE;

    if (seek.type == MPSEEK_FACTOR || seek.amount < 0 ||
        (seek.type == MPSEEK_ABSOLUTE && seek.amount < mpctx->last_chapter_pts))
        mpctx->last_chapter_seek = -2;

    // Under certain circumstances, prefer SEEK_FACTOR.
    if (seek.type == MPSEEK_FACTOR && !hr_seek &&
        (mpctx->demuxer->ts_resets_possible || seek_pts == MP_NOPTS_VALUE))
    {
        demux_pts = seek.amount;
        demux_flags |= SEEK_FACTOR;
    }

    if (hr_seek) {
        double hr_seek_offset = opts->hr_seek_demuxer_offset;
        // Always try to compensate for possibly bad demuxers in "special"
        // situations where we need more robustness from the hr-seek code, even
        // if the user doesn't use --hr-seek-demuxer-offset.
        // The value is arbitrary, but should be "good enough" in most situations.
        if (hr_seek_very_exact)
            hr_seek_offset = MPMAX(hr_seek_offset, 0.5); // arbitrary
        demux_pts -= hr_seek_offset;
        demux_flags = (demux_flags | SEEK_HR | SEEK_BACKWARD) & ~SEEK_FORWARD;
    }

    demux_seek(mpctx->demuxer, demux_pts, demux_flags);

    // Seek external, extra files too:
    for (int t = 0; t < mpctx->num_tracks; t++) {
        struct track *track = mpctx->tracks[t];
        if (track->selected && track->is_external && track->demuxer) {
            double main_new_pos = demux_pts;
            if (demux_flags & SEEK_FACTOR)
                main_new_pos = seek_pts;
            demux_seek(track->demuxer, main_new_pos, 0);
        }
    }

    clear_audio_output_buffers(mpctx);
    reset_playback_state(mpctx);

    /* Use the target time as "current position" for further relative
     * seeks etc until a new video frame has been decoded */
    mpctx->last_seek_pts = seek_pts;

    if (hr_seek) {
        mpctx->hrseek_active = true;
        mpctx->hrseek_framedrop = !hr_seek_very_exact && opts->hr_seek_framedrop;
        mpctx->hrseek_backstep = seek.type == MPSEEK_BACKSTEP;
        mpctx->hrseek_pts = seek_pts;

        MP_VERBOSE(mpctx, "hr-seek, skipping to %f%s%s\n", mpctx->hrseek_pts,
                   mpctx->hrseek_framedrop ? "" : " (no framedrop)",
                   mpctx->hrseek_backstep ? " (backstep)" : "");
    }

    if (mpctx->stop_play == AT_END_OF_FILE)
        mpctx->stop_play = KEEP_PLAYING;

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

    if (mpctx->stop_play == AT_END_OF_FILE)
        mpctx->stop_play = KEEP_PLAYING;

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
    case MPSEEK_BACKSTEP:
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
        mp_seek(mpctx, mpctx->seek);
        mpctx->seek = (struct seek_params){0};
    }
}

// -1 if unknown
double get_time_length(struct MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return -1;

    double len = demuxer_get_time_length(demuxer);
    if (len >= 0)
        return len;

    return -1; // unknown
}

double get_current_time(struct MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (demuxer) {
        if (mpctx->playback_pts != MP_NOPTS_VALUE)
            return mpctx->playback_pts;
        if (mpctx->last_seek_pts != MP_NOPTS_VALUE)
            return mpctx->last_seek_pts;
    }
    return MP_NOPTS_VALUE;
}

double get_playback_time(struct MPContext *mpctx)
{
    double cur = get_current_time(mpctx);
    if (cur == MP_NOPTS_VALUE)
        return cur;
    // During seeking, the time corresponds to the last seek time - apply some
    // cosmetics to it.
    if (mpctx->playback_pts == MP_NOPTS_VALUE) {
        double length = get_time_length(mpctx);
        if (length >= 0)
            cur = MPCLAMP(cur, 0, length);
    }
    return cur;
}

// Return playback position in 0.0-1.0 ratio, or -1 if unknown.
double get_current_pos_ratio(struct MPContext *mpctx, bool use_range)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return -1;
    double ans = -1;
    double start = 0;
    double len = get_time_length(mpctx);
    if (use_range) {
        double startpos = rel_time_to_abs(mpctx, mpctx->opts->play_start);
        double endpos = get_play_end_pts(mpctx);
        if (endpos == MP_NOPTS_VALUE || endpos > MPMAX(0, len))
            endpos = MPMAX(0, len);
        if (startpos == MP_NOPTS_VALUE || startpos < 0)
            startpos = 0;
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
    return dname;
}

// returns NULL if chapter name unavailable
char *chapter_name(struct MPContext *mpctx, int chapter)
{
    if (chapter < 0 || chapter >= mpctx->num_chapters)
        return NULL;
    return mp_tags_get_str(mpctx->chapters[chapter].metadata, "title");
}

// returns the start of the chapter in seconds (NOPTS if unavailable)
double chapter_start_time(struct MPContext *mpctx, int chapter)
{
    if (chapter == -1)
        return 0;
    if (chapter >= 0 && chapter < mpctx->num_chapters)
        return mpctx->chapters[chapter].pts;
    return MP_NOPTS_VALUE;
}

int get_chapter_count(struct MPContext *mpctx)
{
    return mpctx->num_chapters;
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
    if (mpctx->vo_chain && mp_time_sec() - mpctx->start_timestamp < 0.1) {
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
        if (mpctx->max_frames >= 0 && !mpctx->stop_play)
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
        queue_seek(mpctx, MPSEEK_ABSOLUTE, 0, MPSEEK_DEFAULT, true);
        if (opts->loop_file > 0)
            opts->loop_file--;
    }
}

void seek_to_last_frame(struct MPContext *mpctx)
{
    if (!mpctx->vo_chain)
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
                   });
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
        if (mpctx->vo_chain) {
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

// Execute a forceful refresh of the VO window. This clears the window from
// the previous video. It also creates/destroys the VO on demand.
// It tries to make the change only in situations where the window is
// definitely needed or not needed, or if the force parameter is set (the
// latter also decides whether to clear an existing window, because there's
// no way to know if this has already been done or not).
int handle_force_window(struct MPContext *mpctx, bool force)
{
    // True if we're either in idle mode, or loading of the file has finished.
    // It's also set via force in some stages during file loading.
    bool act = !mpctx->playing || mpctx->playback_initialized || force;

    // On the other hand, if a video track is selected, but no video is ever
    // decoded on it, then create the window.
    bool stalled_video = mpctx->playback_initialized && mpctx->restart_complete &&
                         mpctx->video_status == STATUS_EOF && mpctx->vo_chain &&
                         !mpctx->video_out->config_ok;

    // Don't interfere with real video playback
    if (mpctx->vo_chain && !stalled_video)
        return 0;

    if (!mpctx->opts->force_vo) {
        if (act && !mpctx->vo_chain)
            uninit_video_out(mpctx);
        return 0;
    }

    if (mpctx->opts->force_vo != 2 && !act)
        return 0;

    if (!mpctx->video_out) {
        struct vo_extra ex = {
            .input_ctx = mpctx->input,
            .osd = mpctx->osd,
            .encode_lavc_ctx = mpctx->encode_lavc_ctx,
        };
        mpctx->video_out = init_best_video_out(mpctx->global, &ex);
        if (!mpctx->video_out)
            goto err;
        mpctx->mouse_cursor_visible = true;
    }

    if (!mpctx->video_out->config_ok || force) {
        struct vo *vo = mpctx->video_out;
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
            .p_w = 1, .p_h = 1,
        };
        if (vo_reconfig(vo, &p) < 0)
            goto err;
        vo_control(vo, VOCTRL_RESTORE_SCREENSAVER, NULL);
        vo_set_paused(vo, true);
        vo_redraw(vo);
        mp_notify(mpctx, MPV_EVENT_VIDEO_RECONFIG, NULL);
    }

    return 0;

err:
    mpctx->opts->force_vo = 0;
    uninit_video_out(mpctx);
    MP_FATAL(mpctx, "Error opening/initializing the VO window.\n");
    return -1;
}

// Potentially needed by some Lua scripts, which assume TICK always comes.
static void handle_dummy_ticks(struct MPContext *mpctx)
{
    if (mpctx->video_status == STATUS_EOF || mpctx->paused) {
        if (mp_time_sec() - mpctx->last_idle_tick > 0.050) {
            mpctx->last_idle_tick = mp_time_sec();
            mp_notify(mpctx, MPV_EVENT_TICK, NULL);
        }
    }
}

// We always make sure audio and video buffers are filled before actually
// starting playback. This code handles starting them at the same time.
static void handle_playback_restart(struct MPContext *mpctx)
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
        fill_audio_out_buffers(mpctx); // actually play prepared buffer

    if (!mpctx->restart_complete) {
        mpctx->hrseek_active = false;
        mpctx->restart_complete = true;
        mp_notify(mpctx, MPV_EVENT_PLAYBACK_RESTART, NULL);
        if (!mpctx->playing_msg_shown) {
            if (opts->playing_msg && opts->playing_msg[0]) {
                char *msg =
                    mp_property_expand_escaped_string(mpctx, opts->playing_msg);
                struct mp_log *log = mp_log_new(NULL, mpctx->log, "!term-msg");
                mp_info(log, "%s\n", msg);
                talloc_free(log);
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
        MP_VERBOSE(mpctx, "playback restart complete\n");
    }
}

static void handle_eof(struct MPContext *mpctx)
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
    if ((mpctx->ao_chain || mpctx->vo_chain) && !prevent_eof &&
        mpctx->audio_status == STATUS_EOF &&
        mpctx->video_status == STATUS_EOF &&
        !mpctx->stop_play)
    {
        mpctx->stop_play = AT_END_OF_FILE;
    }
}

static void handle_complex_filter_decoders(struct MPContext *mpctx)
{
    if (!mpctx->lavfi)
        return;

    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *track = mpctx->tracks[n];
        if (!track->selected)
            continue;
        if (!track->sink || !lavfi_needs_input(track->sink))
            continue;
        if (track->d_audio) {
            audio_work(track->d_audio);
            struct mp_audio *fr;
            int res = audio_get_frame(track->d_audio, &fr);
            if (res == DATA_OK) {
                lavfi_send_frame_a(track->sink, fr);
            } else {
                lavfi_send_status(track->sink, res);
            }
        }
        if (track->d_video) {
            video_work(track->d_video);
            struct mp_image *fr;
            int res = video_get_frame(track->d_video, &fr);
            if (res == DATA_OK) {
                lavfi_send_frame_v(track->sink, fr);
            } else {
                lavfi_send_status(track->sink, res);
            }
        }
    }
}

void run_playloop(struct MPContext *mpctx)
{
#if HAVE_ENCODING
    if (encode_lavc_didfail(mpctx->encode_lavc_ctx)) {
        mpctx->stop_play = PT_QUIT;
        return;
    }
#endif

    update_demuxer_properties(mpctx);

    handle_complex_filter_decoders(mpctx);

    handle_cursor_autohide(mpctx);
    handle_vo_events(mpctx);
    handle_heartbeat_cmd(mpctx);
    handle_command_updates(mpctx);

    fill_audio_out_buffers(mpctx);
    write_video(mpctx);

    if (mpctx->lavfi) {
        if (lavfi_process(mpctx->lavfi))
            mpctx->sleeptime = 0;
        if (lavfi_has_failed(mpctx->lavfi))
            mpctx->stop_play = AT_END_OF_FILE;
    }

    handle_playback_restart(mpctx);

    // Use the audio timestamp if no video, or video is enabled, but has ended.
    if (mpctx->video_status == STATUS_EOF &&
        mpctx->audio_status >= STATUS_PLAYING &&
        mpctx->audio_status < STATUS_EOF)
    {
        mpctx->playback_pts = playing_audio_pts(mpctx);
    }

    handle_dummy_ticks(mpctx);

    update_osd_msg(mpctx);
    if (!mpctx->video_out)
        update_subtitles(mpctx, mpctx->playback_pts);

    handle_eof(mpctx);

    handle_loop_file(mpctx);

    handle_ab_loop(mpctx);

    handle_keep_open(mpctx);

    handle_sstep(mpctx);

    if (mpctx->stop_play)
        return;

    handle_osd_redraw(mpctx);

    mp_wait_events(mpctx, mpctx->sleeptime);
    mpctx->sleeptime = 1e9; // infinite for all practical purposes

    handle_pause_on_low_cache(mpctx);

    mp_process_input(mpctx);

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
            uninit_audio_out(mpctx);
            handle_force_window(mpctx, true);
            mpctx->sleeptime = 0;
            mp_notify(mpctx, MPV_EVENT_IDLE, NULL);
            need_reinit = false;
        }
        mp_idle(mpctx);
    }
}
