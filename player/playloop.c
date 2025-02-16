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

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#include "client.h"
#include "command.h"
#include "core.h"
#include "mpv_talloc.h"
#include "screenshot.h"

#include "audio/out/ao.h"
#include "common/common.h"
#include "common/encode.h"
#include "common/msg.h"
#include "common/playlist.h"
#include "common/stats.h"
#include "demux/demux.h"
#include "filters/f_decoder_wrapper.h"
#include "filters/filter_internal.h"
#include "input/input.h"
#include "misc/dispatch.h"
#include "options/m_config_frontend.h"
#include "options/m_property.h"
#include "options/options.h"
#include "osdep/terminal.h"
#include "osdep/timer.h"
#include "stream/stream.h"
#include "sub/dec_sub.h"
#include "sub/osd.h"
#include "video/out/vo.h"

// Wait until mp_wakeup_core() is called, since the last time
// mp_wait_events() was called.
void mp_wait_events(struct MPContext *mpctx)
{
    mp_client_send_property_changes(mpctx);

    stats_event(mpctx->stats, "iterations");

    bool sleeping = mpctx->sleeptime > 0;
    if (sleeping)
        MP_STATS(mpctx, "start sleep");

    mp_dispatch_queue_process(mpctx->dispatch, mpctx->sleeptime);

    mpctx->sleeptime = INFINITY;

    if (sleeping)
        MP_STATS(mpctx, "end sleep");
}

// Set the timeout used when the playloop goes to sleep. This means the
// playloop will re-run as soon as the timeout elapses (or earlier).
// mp_set_timeout(c, 0) is essentially equivalent to mp_wakeup_core(c).
void mp_set_timeout(struct MPContext *mpctx, double sleeptime)
{
    if (mpctx->sleeptime > sleeptime) {
        mpctx->sleeptime = sleeptime;
        int64_t abstime = mp_time_ns_add(mp_time_ns(), sleeptime);
        mp_dispatch_adjust_timeout(mpctx->dispatch, abstime);
    }
}

// Cause the playloop to run. This can be called from any thread. If called
// from within the playloop itself, it will be run immediately again, instead
// of going to sleep in the next mp_wait_events().
void mp_wakeup_core(struct MPContext *mpctx)
{
    mp_dispatch_interrupt(mpctx->dispatch);
}

// Opaque callback variant of mp_wakeup_core().
void mp_wakeup_core_cb(void *ctx)
{
    struct MPContext *mpctx = ctx;
    mp_wakeup_core(mpctx);
}

void mp_core_lock(struct MPContext *mpctx)
{
    mp_dispatch_lock(mpctx->dispatch);
}

void mp_core_unlock(struct MPContext *mpctx)
{
    mp_dispatch_unlock(mpctx->dispatch);
}

// Process any queued user input.
static void mp_process_input(struct MPContext *mpctx)
{
    int processed = 0;
    for (;;) {
        mp_cmd_t *cmd = mp_input_read_cmd(mpctx->input);
        if (!cmd)
            break;
        run_command(mpctx, cmd, NULL, NULL, NULL);
        processed = 1;
    }
    mp_set_timeout(mpctx, mp_input_get_delay(mpctx->input));
    if (processed)
        mp_notify(mpctx, MP_EVENT_INPUT_PROCESSED, NULL);
}

// Process any queued option callbacks.
static void handle_option_callbacks(struct MPContext *mpctx)
{
    for (int i = 0; i < mpctx->num_option_callbacks; i++)
        mp_option_run_callback(mpctx, i);
    mpctx->num_option_callbacks = 0;
}

double get_relative_time(struct MPContext *mpctx)
{
    int64_t new_time = mp_time_ns();
    int64_t delta = new_time - mpctx->last_time;
    mpctx->last_time = new_time;
    return delta * 1e-9;
}

void update_core_idle_state(struct MPContext *mpctx)
{
    bool eof = mpctx->video_status == STATUS_EOF &&
               mpctx->audio_status == STATUS_EOF;
    bool active = !mpctx->paused && mpctx->restart_complete &&
                  !mpctx->stop_play && mpctx->in_playloop && !eof;

    if (mpctx->playback_active != active) {
        mpctx->playback_active = active;

        update_screensaver_state(mpctx);

        mp_notify(mpctx, MP_EVENT_CORE_IDLE, NULL);
    }
}

bool get_internal_paused(struct MPContext *mpctx)
{
    return mpctx->opts->pause || mpctx->paused_for_cache;
}

// The value passed here is the new value for mpctx->opts->pause
void set_pause_state(struct MPContext *mpctx, bool user_pause)
{
    struct MPOpts *opts = mpctx->opts;

    opts->pause = user_pause;

    bool internal_paused = get_internal_paused(mpctx);
    if (internal_paused != mpctx->paused) {
        mpctx->paused = internal_paused;

        if (mpctx->ao) {
            bool eof = mpctx->audio_status == STATUS_EOF;
            ao_set_paused(mpctx->ao, internal_paused, eof);
        }

        if (mpctx->video_out)
            vo_set_paused(mpctx->video_out, internal_paused);

        mpctx->osd_function = 0;
        mpctx->osd_force_update = true;

        mp_wakeup_core(mpctx);

        if (internal_paused) {
            mpctx->step_frames = 0;
            mpctx->time_frame -= get_relative_time(mpctx);
        } else {
            (void)get_relative_time(mpctx); // ignore time that passed during pause
        }
    }

    update_core_idle_state(mpctx);

    m_config_notify_change_opt_ptr(mpctx->mconfig, &opts->pause);
}

void update_internal_pause_state(struct MPContext *mpctx)
{
    set_pause_state(mpctx, mpctx->opts->pause);
}

void update_screensaver_state(struct MPContext *mpctx)
{
    if (!mpctx->video_out)
        return;

    bool saver_state = (!mpctx->playback_active || !mpctx->opts->stop_screensaver) &&
                       mpctx->opts->stop_screensaver != 2;
    vo_control_async(mpctx->video_out, saver_state ? VOCTRL_RESTORE_SCREENSAVER
                                                   : VOCTRL_KILL_SCREENSAVER, NULL);
}

void add_step_frame(struct MPContext *mpctx, int dir, bool use_seek)
{
    if (!mpctx->vo_chain)
        return;
    if (dir > 0 && !use_seek) {
        mpctx->step_frames += dir;
        set_pause_state(mpctx, false);
    } else {
        if (!mpctx->hrseek_active) {
            queue_seek(mpctx, MPSEEK_FRAMESTEP, dir, MPSEEK_VERY_EXACT, 0);
            set_pause_state(mpctx, true);
        }
    }
}

// Clear some playback-related fields on file loading or after seeks.
void reset_playback_state(struct MPContext *mpctx)
{
    mp_filter_reset(mpctx->filter_root);

    reset_video_state(mpctx);
    reset_audio_state(mpctx);
    reset_subtitle_state(mpctx);

    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *t = mpctx->tracks[n];
        // (Often, but not always, this is redundant and also done elsewhere.)
        if (t->dec)
            mp_decoder_wrapper_set_play_dir(t->dec, mpctx->play_dir);
        if (t->d_sub)
            sub_set_play_dir(t->d_sub, mpctx->play_dir);
    }

    // May need unpause first
    if (mpctx->paused_for_cache)
        update_internal_pause_state(mpctx);

    mpctx->hrseek_active = false;
    mpctx->hrseek_lastframe = false;
    mpctx->hrseek_backstep = false;
    mpctx->current_seek = (struct seek_params){0};
    mpctx->playback_pts = MP_NOPTS_VALUE;
    mpctx->step_frames = 0;
    mpctx->ab_loop_clip = true;
    mpctx->restart_complete = false;
    mpctx->paused_for_cache = false;
    mpctx->cache_buffer = 100;
    mpctx->cache_update_pts = MP_NOPTS_VALUE;

    encode_lavc_discontinuity(mpctx->encode_lavc_ctx);

    update_internal_pause_state(mpctx);
    update_core_idle_state(mpctx);
}

static double calculate_framestep_pts(MPContext *mpctx, double current_time,
                                      int step_frames)
{
    // Crude guess at the pts. Use current_time if step_frames is -1.
    int previous_frame = mpctx->num_past_frames - 1;
    int offset = step_frames == -1 ? 0 : step_frames;
    double pts = mpctx->past_frames[previous_frame].approx_duration * offset;
    return current_time + pts;
}

static void mp_seek(MPContext *mpctx, struct seek_params seek)
{
    struct MPOpts *opts = mpctx->opts;

    if (!mpctx->demuxer || !seek.type || seek.amount == MP_NOPTS_VALUE)
        return;

    if (seek.type == MPSEEK_CHAPTER) {
        mpctx->last_chapter_flag = false;
        seek.type = MPSEEK_ABSOLUTE;
    } else {
        mpctx->last_chapter_seek = -2;
    }

    bool hr_seek_very_exact = seek.exact == MPSEEK_VERY_EXACT;
    double current_time = get_playback_time(mpctx);
    if (current_time == MP_NOPTS_VALUE && seek.type == MPSEEK_RELATIVE)
        return;
    if (current_time == MP_NOPTS_VALUE)
        current_time = 0;
    double seek_pts = MP_NOPTS_VALUE;
    int demux_flags = 0;

    switch (seek.type) {
    case MPSEEK_ABSOLUTE:
        seek_pts = seek.amount;
        break;
    case MPSEEK_FRAMESTEP:
        seek_pts = calculate_framestep_pts(mpctx, current_time,
                                           (int)seek.amount);
        hr_seek_very_exact = true;
        break;
    case MPSEEK_RELATIVE:
        demux_flags = seek.amount > 0 ? SEEK_FORWARD : 0;
        seek_pts = current_time + seek.amount;
        break;
    case MPSEEK_FACTOR: ;
        double len = get_time_length(mpctx);
        if (len >= 0)
            seek_pts = seek.amount * len;
        break;
    default: MP_ASSERT_UNREACHABLE();
    }

    double demux_pts = seek_pts;

    bool hr_seek = seek.exact != MPSEEK_KEYFRAME && seek_pts != MP_NOPTS_VALUE &&
        (seek.exact >= MPSEEK_EXACT || opts->hr_seek == 1 ||
         (opts->hr_seek >= 0 && seek.type == MPSEEK_ABSOLUTE) ||
         (opts->hr_seek == 2 && (!mpctx->vo_chain || mpctx->vo_chain->is_sparse)));

    // Under certain circumstances, prefer SEEK_FACTOR.
    if (seek.type == MPSEEK_FACTOR && !hr_seek &&
        (mpctx->demuxer->ts_resets_possible || seek_pts == MP_NOPTS_VALUE))
    {
        demux_pts = seek.amount;
        demux_flags |= SEEK_FACTOR;
    }

    int play_dir = opts->play_dir;
    if (play_dir < 0)
        demux_flags |= SEEK_SATAN;

    if (hr_seek) {
        double hr_seek_offset = opts->hr_seek_demuxer_offset;
        // Always try to compensate for possibly bad demuxers in "special"
        // situations where we need more robustness from the hr-seek code, even
        // if the user doesn't use --hr-seek-demuxer-offset.
        // The value is arbitrary, but should be "good enough" in most situations.
        if (hr_seek_very_exact)
            hr_seek_offset = MPMAX(hr_seek_offset, 0.5); // arbitrary
        for (int n = 0; n < mpctx->num_tracks; n++) {
            double offset = 0;
            if (!mpctx->tracks[n]->is_external)
                offset += get_track_seek_offset(mpctx, mpctx->tracks[n]);
            hr_seek_offset = MPMAX(hr_seek_offset, -offset);
        }
        demux_pts -= hr_seek_offset * play_dir;
        demux_flags = (demux_flags | SEEK_HR) & ~SEEK_FORWARD;
        // For HR seeks in backward playback mode, the correct seek rounding
        // direction is forward instead of backward.
        if (play_dir < 0)
            demux_flags |= SEEK_FORWARD;
    }

    if (!mpctx->demuxer->seekable)
        demux_flags |= SEEK_CACHED;

    demux_flags |= SEEK_BLOCK;

    if (!demux_seek(mpctx->demuxer, demux_pts, demux_flags)) {
        if (!mpctx->demuxer->seekable) {
            MP_ERR(mpctx, "Cannot seek in this stream.\n");
            MP_ERR(mpctx, "You can force it with '--force-seekable=yes'.\n");
        }
        return;
    }

    mpctx->play_dir = play_dir;

    // Seek external, extra files too:
    for (int t = 0; t < mpctx->num_tracks; t++) {
        struct track *track = mpctx->tracks[t];
        if (track->selected && track->is_external && track->demuxer) {
            double main_new_pos = demux_pts;
            if (!hr_seek || track->is_external)
                main_new_pos += get_track_seek_offset(mpctx, track);
            if (demux_flags & SEEK_FACTOR)
                main_new_pos = seek_pts;
            demux_seek(track->demuxer, main_new_pos,
                       demux_flags & (SEEK_SATAN | SEEK_BLOCK));
        }
    }

    if (!(seek.flags & MPSEEK_FLAG_NOFLUSH))
        clear_audio_output_buffers(mpctx);

    reset_playback_state(mpctx);

    demux_block_reading(mpctx->demuxer, false);
    for (int t = 0; t < mpctx->num_tracks; t++) {
        struct track *track = mpctx->tracks[t];
        if (track->selected && track->demuxer)
            demux_block_reading(track->demuxer, false);
    }

    /* Use the target time as "current position" for further relative
     * seeks etc until a new video frame has been decoded */
    mpctx->last_seek_pts = seek_pts;

    if (hr_seek) {
        mpctx->hrseek_active = true;
        mpctx->hrseek_backstep = seek.type == MPSEEK_FRAMESTEP && seek.amount == -1;
        mpctx->hrseek_pts = seek_pts * mpctx->play_dir;

        // allow decoder to drop frames before hrseek_pts
        bool hrseek_framedrop = !hr_seek_very_exact && opts->hr_seek_framedrop;

        MP_VERBOSE(mpctx, "hr-seek, skipping to %f%s%s\n", mpctx->hrseek_pts,
                   hrseek_framedrop ? "" : " (no framedrop)",
                   mpctx->hrseek_backstep ? " (backstep)" : "");

        for (int n = 0; n < mpctx->num_tracks; n++) {
            struct track *track = mpctx->tracks[n];
            struct mp_decoder_wrapper *dec = track->dec;
            if (dec && hrseek_framedrop)
                mp_decoder_wrapper_set_start_pts(dec, mpctx->hrseek_pts);
        }
    }

    if (mpctx->stop_play == AT_END_OF_FILE)
        mpctx->stop_play = KEEP_PLAYING;

    mpctx->start_timestamp = mp_time_sec();
    mp_wakeup_core(mpctx);

    mp_notify(mpctx, MPV_EVENT_SEEK, NULL);
    mp_notify(mpctx, MPV_EVENT_TICK, NULL);

    update_ab_loop_clip(mpctx);

    mpctx->current_seek = seek;
    redraw_subs(mpctx);
}

// This combines consecutive seek requests.
void queue_seek(struct MPContext *mpctx, enum seek_type type, double amount,
                enum seek_precision exact, int flags)
{
    struct seek_params *seek = &mpctx->seek;

    mp_wakeup_core(mpctx);

    switch (type) {
    case MPSEEK_RELATIVE:
        seek->flags |= flags;
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
    case MPSEEK_FRAMESTEP:
    case MPSEEK_CHAPTER:
        *seek = (struct seek_params) {
            .type = type,
            .amount = amount,
            .exact = exact,
            .flags = flags,
        };
        return;
    case MPSEEK_NONE:
        *seek = (struct seek_params){ 0 };
        return;
    }
    MP_ASSERT_UNREACHABLE();
}

void execute_queued_seek(struct MPContext *mpctx)
{
    if (mpctx->seek.type) {
        bool queued_hr_seek = mpctx->seek.exact != MPSEEK_KEYFRAME;
        // Let explicitly imprecise seeks cancel precise seeks:
        if (mpctx->hrseek_active && !queued_hr_seek)
            mpctx->start_timestamp = -1e9;
        // If the user seeks continuously (keeps arrow key down) try to finish
        // showing a frame from one location before doing another seek (instead
        // of never updating the screen).
        if ((mpctx->seek.flags & MPSEEK_FLAG_DELAY) &&
            mp_time_sec() - mpctx->start_timestamp < 0.3)
        {
            // Wait until a video frame is available and has been shown.
            if (mpctx->video_status < STATUS_PLAYING)
                return;
            // On A/V hr-seeks, always wait for the full result, to avoid corner
            // cases when seeking past EOF (we want it to determine that EOF
            // actually happened, instead of overwriting it with the new seek).
            if (mpctx->hrseek_active && queued_hr_seek && mpctx->vo_chain &&
                mpctx->ao_chain && !mpctx->restart_complete)
                return;
        }
        mp_seek(mpctx, mpctx->seek);
        mpctx->seek = (struct seek_params){0};
    }
}

// NOPTS (i.e. <0) if unknown
double get_time_length(struct MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->demuxer;
    return demuxer && demuxer->duration >= 0 ? demuxer->duration : MP_NOPTS_VALUE;
}

// Return approximate PTS of first frame played. This can be completely wrong
// for a number of reasons in a number of situations.
double get_start_time(struct MPContext *mpctx, int dir)
{
    double res = 0;
    if (mpctx->demuxer) {
        if (!mpctx->opts->rebase_start_time)
            res += mpctx->demuxer->start_time;
        if (dir < 0)
            res += MPMAX(mpctx->demuxer->duration, 0);
    }
    return res;
}

double get_current_time(struct MPContext *mpctx)
{
    if (!mpctx->demuxer)
        return MP_NOPTS_VALUE;
    if (mpctx->playback_pts != MP_NOPTS_VALUE)
        return mpctx->playback_pts * mpctx->play_dir;
    return mpctx->last_seek_pts;
}

double get_playback_time(struct MPContext *mpctx)
{
    double cur = get_current_time(mpctx);
    // During seeking, the time corresponds to the last seek time - apply some
    // cosmetics to it.
    if (cur != MP_NOPTS_VALUE && mpctx->playback_pts == MP_NOPTS_VALUE) {
        double length = get_time_length(mpctx);
        if (length >= 0)
            cur = MPCLAMP(cur, 0, length);
    }
    // Force to 0 if this is not MP_NOPTS_VALUE.
    if (cur != MP_NOPTS_VALUE && cur < 0)
        cur = 0.0;
    return cur;
}

// Return playback position in 0.0-1.0 ratio, or -1 if unknown.
double get_current_pos_ratio(struct MPContext *mpctx, bool use_range)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return -1;
    double ret = -1;
    double start = 0;
    double len = get_time_length(mpctx);
    if (use_range) {
        double startpos = get_play_start_pts(mpctx);
        double endpos = get_play_end_pts(mpctx);
        if (endpos > MPMAX(0, len))
            endpos = MPMAX(0, len);
        if (endpos < startpos)
            endpos = startpos;
        start = startpos;
        len = endpos - startpos;
    }
    double pos = get_current_time(mpctx);
    if (len > 0)
        ret = MPCLAMP((pos - start) / len, 0, 1);
    if (ret < 0) {
        int64_t size = demuxer->filesize;
        if (size > 0 && demuxer->filepos >= 0)
            ret = MPCLAMP(demuxer->filepos / (double)size, 0, 1);
    }
    if (use_range) {
        if (mpctx->opts->play_frames > 0)
            ret = MPMAX(ret, 1.0 -
                    mpctx->max_frames / (double) mpctx->opts->play_frames);
    }
    return ret;
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
    return mpctx->last_chapter_flag ?
        mpctx->last_chapter_seek : MPMAX(mpctx->last_chapter_seek, i - 1);
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

// If the current playback position (or seek target) falls before the B
// position, actually make playback loop when reaching the B point. The
// intention is that you can seek out of the ab-loop range.
void update_ab_loop_clip(struct MPContext *mpctx)
{
    double pts = get_current_time(mpctx);
    double ab[2];
    mpctx->ab_loop_clip = pts != MP_NOPTS_VALUE &&
                          get_ab_loop_times(mpctx, ab) &&
                          pts * mpctx->play_dir <= ab[1] * mpctx->play_dir;
}

static void handle_osd_redraw(struct MPContext *mpctx)
{
    if (!mpctx->video_out || !mpctx->video_out->config_ok || (mpctx->playing && mpctx->stop_play))
        return;
    // If we're playing normally, let OSD be redrawn naturally as part of
    // video display.
    if (!mpctx->paused) {
        if (mpctx->sleeptime < 0.1 && mpctx->video_status == STATUS_PLAYING)
            return;
    }
    // Don't redraw immediately during a seek (makes it significantly slower).
    bool use_video = mpctx->vo_chain && !mpctx->vo_chain->is_sparse;
    if (use_video && mp_time_sec() - mpctx->start_timestamp < 0.1) {
        mp_set_timeout(mpctx, 0.1);
        return;
    }
    bool want_redraw = osd_query_and_reset_want_redraw(mpctx->osd) ||
                       vo_want_redraw(mpctx->video_out);
    if (!want_redraw)
        return;
    vo_redraw(mpctx->video_out);
}

static void clear_underruns(struct MPContext *mpctx)
{
    if (mpctx->ao_chain && mpctx->ao_chain->underrun) {
        mpctx->ao_chain->underrun = false;
        mp_wakeup_core(mpctx);
    }

    if (mpctx->vo_chain && mpctx->vo_chain->underrun) {
        mpctx->vo_chain->underrun = false;
        mp_wakeup_core(mpctx);
    }
}

static void handle_update_cache(struct MPContext *mpctx)
{
    bool force_update = false;
    struct MPOpts *opts = mpctx->opts;

    if (!mpctx->demuxer || mpctx->encode_lavc_ctx) {
        clear_underruns(mpctx);
        return;
    }

    double now = mp_time_sec();

    struct demux_reader_state s;
    demux_get_reader_state(mpctx->demuxer, &s);

    mpctx->demux_underrun |= s.underrun;

    int cache_buffer = 100;
    bool use_pause_on_low_cache = opts->cache_pause && mpctx->play_dir > 0;

    if (!mpctx->restart_complete) {
        // Audio or video is restarting, and initial buffering is enabled. Make
        // sure we actually restart them in paused mode, so no audio gets
        // dropped and video technically doesn't start yet.
        use_pause_on_low_cache &= opts->cache_pause_initial &&
                                    (mpctx->video_status == STATUS_READY ||
                                     mpctx->audio_status == STATUS_READY);
    }

    bool is_low = use_pause_on_low_cache && !s.idle &&
                  s.ts_info.duration < opts->cache_pause_wait;

    // Enter buffering state only if there actually was an underrun (or if
    // initial caching before playback restart is used).
    bool need_wait = is_low;
    if (is_low && !mpctx->paused_for_cache && mpctx->restart_complete) {
        // Wait only if an output underrun was registered. (Or if there is no
        // underrun detection.)
        bool output_underrun = false;

        if (mpctx->ao_chain)
            output_underrun |= mpctx->ao_chain->underrun;
        if (mpctx->vo_chain)
            output_underrun |= mpctx->vo_chain->underrun;

        // Output underruns could be sporadic (unrelated to demuxer buffer state
        // and for example caused by slow decoding), so use a past demuxer
        // underrun as indication that the underrun was possibly due to a
        // demuxer underrun.
        need_wait = mpctx->demux_underrun && output_underrun;
    }

    // Let the underrun flag "stick" around until the cache has fully recovered.
    // See logic where demux_underrun is used.
    if (!is_low)
        mpctx->demux_underrun = false;

    if (mpctx->paused_for_cache != need_wait) {
        mpctx->paused_for_cache = need_wait;
        update_internal_pause_state(mpctx);
        force_update = true;
        if (mpctx->paused_for_cache)
            mpctx->cache_stop_time = now;
    }

    if (!mpctx->paused_for_cache)
        clear_underruns(mpctx);

    if (mpctx->paused_for_cache) {
        cache_buffer =
            100 * MPCLAMP(s.ts_info.duration / opts->cache_pause_wait, 0, 0.99);
        mp_set_timeout(mpctx, 0.2);
    }

    // Also update cache properties.
    bool busy = !s.idle;
    if (fabs(mpctx->cache_update_pts - mpctx->playback_pts) >= 1.0)
        busy = true;
    if (busy || mpctx->next_cache_update > 0) {
        if (mpctx->next_cache_update <= now) {
            mpctx->next_cache_update = busy ? now + 0.25 : 0;
            force_update = true;
        }
        if (mpctx->next_cache_update > 0)
            mp_set_timeout(mpctx, mpctx->next_cache_update - now);
    }

    if (mpctx->cache_buffer != cache_buffer) {
        if ((mpctx->cache_buffer == 100) != (cache_buffer == 100)) {
            if (cache_buffer < 100) {
                MP_VERBOSE(mpctx, "Enter buffering (buffer went from %d%% -> %d%%) [%fs].\n",
                           mpctx->cache_buffer, cache_buffer, s.ts_info.duration);
            } else {
                double t = now - mpctx->cache_stop_time;
                MP_VERBOSE(mpctx, "End buffering (waited %f secs) [%fs].\n",
                           t, s.ts_info.duration);
            }
        } else {
            MP_VERBOSE(mpctx, "Still buffering (buffer went from %d%% -> %d%%) [%fs].\n",
                       mpctx->cache_buffer, cache_buffer, s.ts_info.duration);
        }
        mpctx->cache_buffer = cache_buffer;
        force_update = true;
    }

    if (s.eof && !busy)
        prefetch_next(mpctx);

    if (force_update) {
        mpctx->cache_update_pts = mpctx->playback_pts;
        mp_notify(mpctx, MP_EVENT_CACHE_UPDATE, NULL);
    }
}

int get_cache_buffering_percentage(struct MPContext *mpctx)
{
    return mpctx->demuxer ? mpctx->cache_buffer : -1;
}

static void handle_update_subtitles(struct MPContext *mpctx)
{
    if (mpctx->video_status == STATUS_EOF) {
        update_subtitles(mpctx, mpctx->playback_pts);
        return;
    }

    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *track = mpctx->tracks[n];
        if (track->type == STREAM_SUB && !track->demuxer_ready) {
            update_subtitles(mpctx, mpctx->playback_pts);
            break;
        }
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
        mp_set_timeout(mpctx, mpctx->mouse_timer - now);
    } else {
        mouse_cursor_visible = false;
    }

    if (opts->cursor_autohide_delay == -1)
        mouse_cursor_visible = true;

    if (opts->cursor_autohide_delay == -2)
        mouse_cursor_visible = false;

    if (opts->cursor_autohide_fs && !opts->vo->fullscreen)
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
    if (events & VO_EVENT_DPI)
        mp_notify(mpctx, MP_EVENT_WIN_STATE2, NULL);
    if (events & VO_EVENT_FOCUS)
        mp_notify(mpctx, MP_EVENT_FOCUS, NULL);
    if (events & VO_EVENT_AMBIENT_LIGHTING_CHANGED)
        mp_notify(mpctx, MP_EVENT_AMBIENT_LIGHTING_CHANGED, NULL);
}

static void handle_sstep(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    if (mpctx->stop_play || !mpctx->restart_complete)
        return;

    if (opts->step_sec > 0 && !mpctx->paused) {
        set_osd_function(mpctx, OSD_FFW);
        queue_seek(mpctx, MPSEEK_RELATIVE, opts->step_sec, MPSEEK_DEFAULT, 0);
    }

    if (mpctx->video_status >= STATUS_EOF) {
        if (mpctx->max_frames >= 0 && !mpctx->stop_play)
            mpctx->stop_play = AT_END_OF_FILE; // force EOF even if audio left
        if (mpctx->step_frames > 0 && !mpctx->paused)
            set_pause_state(mpctx, true);
    }
}

static void handle_loop_file(struct MPContext *mpctx)
{
    if (mpctx->stop_play != AT_END_OF_FILE)
        return;

    double target = MP_NOPTS_VALUE;
    enum seek_precision prec = MPSEEK_DEFAULT;

    double ab[2];
    if (get_ab_loop_times(mpctx, ab) && mpctx->ab_loop_clip) {
        if (mpctx->remaining_ab_loops > 0) {
            mpctx->remaining_ab_loops--;
            mp_notify_property(mpctx, "remaining-ab-loops");
        }
        target = ab[0];
        prec = MPSEEK_EXACT;
    } else if (mpctx->remaining_file_loops) {
        if (mpctx->remaining_file_loops > 0) {
            mpctx->remaining_file_loops--;
            mp_notify_property(mpctx, "remaining-file-loops");
        }
        target = get_start_time(mpctx, mpctx->play_dir);
    }

    if (target != MP_NOPTS_VALUE) {
        if (!mpctx->shown_aframes && !mpctx->shown_vframes) {
            MP_WARN(mpctx, "No media data to loop.\n");
            return;
        }

        mpctx->stop_play = KEEP_PLAYING;
        set_osd_function(mpctx, OSD_FFW);
        mark_seek(mpctx);

        // Assumes execute_queued_seek() happens before next audio/video is
        // attempted to be decoded or filtered.
        queue_seek(mpctx, MPSEEK_ABSOLUTE, target, prec, MPSEEK_FLAG_NOFLUSH);
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
    double end = MP_NOPTS_VALUE;
    if (mpctx->play_dir > 0) {
        end = get_play_end_pts(mpctx);
        if (end == MP_NOPTS_VALUE)
            end = get_time_length(mpctx);
    } else {
        end = get_start_time(mpctx, 1);
    }
    mp_seek(mpctx, (struct seek_params){
                   .type = MPSEEK_ABSOLUTE,
                   .amount = end,
                   .exact = MPSEEK_VERY_EXACT,
                   });
    // Make it exact: stop seek only if last frame was reached.
    if (mpctx->hrseek_active) {
        mpctx->hrseek_pts = INFINITY * mpctx->play_dir;
        mpctx->hrseek_lastframe = true;
    }
}

static void handle_keep_open(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    if (opts->keep_open && mpctx->stop_play == AT_END_OF_FILE &&
        (opts->keep_open == 2 ||
        (!playlist_get_next(mpctx->playlist, 1) && opts->loop_times == 1)))
    {
        mpctx->stop_play = KEEP_PLAYING;
        if (mpctx->vo_chain) {
            if (!vo_has_frame(mpctx->video_out)) { // EOF not reached normally
                seek_to_last_frame(mpctx);
                mpctx->audio_status = STATUS_EOF;
                mpctx->video_status = STATUS_EOF;
            }
        }
        if (opts->keep_open_pause) {
            if (mpctx->ao && ao_is_playing(mpctx->ao))
                return;
            set_pause_state(mpctx, true);
        }
    }
}

static void handle_chapter_change(struct MPContext *mpctx)
{
    int chapter = get_current_chapter(mpctx);
    if (chapter != mpctx->last_chapter) {
        mpctx->last_chapter = chapter;
        mp_notify(mpctx, MP_EVENT_CHAPTER_CHANGE, NULL);
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
    bool act = mpctx->stop_play || mpctx->playback_initialized || force;

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
            .wakeup_cb = mp_wakeup_core_cb,
            .wakeup_ctx = mpctx,
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

        // Use a 16:9 aspect ratio so that fullscreen on a 16:9 screen will not
        // have vertical margins, which can lead to a different size or position
        // of subtitles than with 16:9 videos.
        int w = 960;
        int h = 540;
        struct mp_image_params p = {
            .imgfmt = config_format,
            .w = w,   .h = h,
            .p_w = 1, .p_h = 1,
            .force_window = true,
        };
        if (vo_reconfig(vo, &p) < 0)
            goto err;
        struct track *track = mpctx->current_track[0][STREAM_VIDEO];
        update_content_type(mpctx, track);
        update_screensaver_state(mpctx);
        vo_set_paused(vo, true);
        vo_redraw(vo);
        mp_notify(mpctx, MPV_EVENT_VIDEO_RECONFIG, NULL);
    }

    return 0;

err:
    mpctx->opts->force_vo = 0;
    m_config_notify_change_opt_ptr(mpctx->mconfig, &mpctx->opts->force_vo);
    uninit_video_out(mpctx);
    MP_FATAL(mpctx, "Error opening/initializing the VO window.\n");
    return -1;
}

// Potentially needed by some Lua scripts, which assume TICK always comes.
static void handle_dummy_ticks(struct MPContext *mpctx)
{
    if ((mpctx->video_status != STATUS_PLAYING &&
         mpctx->video_status != STATUS_DRAINING) ||
         mpctx->paused)
    {
        if (mp_time_sec() - mpctx->last_idle_tick > 0.050) {
            mpctx->last_idle_tick = mp_time_sec();
            mp_notify(mpctx, MPV_EVENT_TICK, NULL);
        }
    }
}

// Update current playback time.
static void handle_playback_time(struct MPContext *mpctx)
{
    if (mpctx->vo_chain &&
        !mpctx->vo_chain->is_sparse &&
        mpctx->video_status >= STATUS_PLAYING &&
        mpctx->video_status < STATUS_EOF)
    {
        mpctx->playback_pts = mpctx->video_pts;
    } else if (mpctx->audio_status >= STATUS_PLAYING &&
               mpctx->audio_status < STATUS_EOF)
    {
        mpctx->playback_pts = playing_audio_pts(mpctx);
    } else if (mpctx->video_status == STATUS_EOF &&
               mpctx->audio_status == STATUS_EOF)
    {
        double apts = playing_audio_pts(mpctx);
        double vpts = mpctx->video_pts;
        double mpts = MP_PTS_MAX(apts, vpts);
        if (mpts != MP_NOPTS_VALUE)
            mpctx->playback_pts = mpts;
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

    handle_update_cache(mpctx);

    if (mpctx->video_status == STATUS_READY) {
        mpctx->video_status = STATUS_PLAYING;
        get_relative_time(mpctx);
        mp_wakeup_core(mpctx);
        MP_DBG(mpctx, "starting video playback\n");
    }

    if (mpctx->audio_status == STATUS_READY) {
        // If a new seek is queued while the current one finishes, don't
        // actually play the audio, but resume seeking immediately.
        if (mpctx->seek.type && mpctx->video_status == STATUS_PLAYING) {
            handle_playback_time(mpctx);
            mpctx->seek.flags &= ~MPSEEK_FLAG_DELAY; // immediately
            execute_queued_seek(mpctx);
            return;
        }

        audio_start_ao(mpctx);
    }

    if (!mpctx->restart_complete) {
        mpctx->hrseek_active = false;
        mpctx->restart_complete = true;
        mpctx->current_seek = (struct seek_params){0};
        handle_playback_time(mpctx);
        mp_notify(mpctx, MPV_EVENT_PLAYBACK_RESTART, NULL);
        update_core_idle_state(mpctx);
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
                set_osd_msg(mpctx, 1, opts->osd_playing_msg_duration ?
                            opts->osd_playing_msg_duration : opts->osd_duration,
                            "%s", msg);
                talloc_free(msg);
            }
        }
        mpctx->playing_msg_shown = true;
        mp_wakeup_core(mpctx);
        update_ab_loop_clip(mpctx);
        MP_VERBOSE(mpctx, "playback restart complete @ %f, audio=%s, video=%s%s\n",
                   mpctx->playback_pts, mp_status_str(mpctx->audio_status),
                   mp_status_str(mpctx->video_status),
                   get_internal_paused(mpctx) ? " (paused)" : "");

        // To avoid strange effects when using relative seeks, especially if
        // there are no proper audio & video timestamps (seeks after EOF).
        double length = get_time_length(mpctx);
        if (mpctx->last_seek_pts != MP_NOPTS_VALUE && length >= 0)
            mpctx->last_seek_pts = MPCLAMP(mpctx->last_seek_pts, 0, length);

        // Continuous seeks past EOF => treat as EOF instead of repeating seek.
        if (mpctx->seek.type == MPSEEK_RELATIVE && mpctx->seek.amount > 0 &&
            mpctx->video_status == STATUS_EOF &&
            mpctx->audio_status == STATUS_EOF)
            mpctx->seek = (struct seek_params){0};
    }
}

static void handle_eof(struct MPContext *mpctx)
{
    if (mpctx->seek.type)
        return; // for proper keep-open operation

    /* Don't quit while paused and we're displaying the last video frame. On the
     * other hand, if we don't have a video frame, then the user probably seeked
     * outside of the video, and we do want to quit. */
    bool prevent_eof =
        mpctx->paused && mpctx->video_out && vo_has_frame(mpctx->video_out) &&
        !mpctx->vo_chain->is_coverart;
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

static void handle_clipboard_updates(struct MPContext *mpctx)
{
    if (mp_clipboard_data_changed(mpctx->clipboard))
        mp_notify_property(mpctx, "clipboard");
}

void run_playloop(struct MPContext *mpctx)
{
    if (encode_lavc_didfail(mpctx->encode_lavc_ctx)) {
        mpctx->stop_play = PT_ERROR;
        return;
    }

    update_demuxer_properties(mpctx);

    handle_cursor_autohide(mpctx);
    handle_vo_events(mpctx);
    handle_command_updates(mpctx);

    if (mpctx->lavfi && mp_filter_has_failed(mpctx->lavfi))
        mpctx->stop_play = AT_END_OF_FILE;

    fill_audio_out_buffers(mpctx);
    write_video(mpctx);

    handle_playback_restart(mpctx);

    handle_playback_time(mpctx);

    handle_dummy_ticks(mpctx);

    handle_clipboard_updates(mpctx);

    update_osd_msg(mpctx);

    handle_update_subtitles(mpctx);

    handle_each_frame_screenshot(mpctx);

    handle_eof(mpctx);

    handle_loop_file(mpctx);

    handle_keep_open(mpctx);

    handle_sstep(mpctx);

    update_core_idle_state(mpctx);

    execute_queued_seek(mpctx);

    if (mpctx->stop_play)
        return;

    handle_osd_redraw(mpctx);

    if (mp_filter_graph_run(mpctx->filter_root))
        mp_wakeup_core(mpctx);

    mp_wait_events(mpctx);

    handle_update_cache(mpctx);

    mp_process_input(mpctx);

    handle_option_callbacks(mpctx);

    handle_chapter_change(mpctx);

    handle_force_window(mpctx, false);
}

void mp_idle(struct MPContext *mpctx)
{
    handle_dummy_ticks(mpctx);
    handle_clipboard_updates(mpctx);
    mp_wait_events(mpctx);
    mp_process_input(mpctx);
    handle_option_callbacks(mpctx);
    handle_command_updates(mpctx);
    handle_update_cache(mpctx);
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
    while (mpctx->opts->player_idle_mode && mpctx->stop_play == PT_STOP) {
        if (need_reinit) {
            uninit_audio_out(mpctx);
            handle_force_window(mpctx, true);
            mp_wakeup_core(mpctx);
            mp_notify(mpctx, MPV_EVENT_IDLE, NULL);
            need_reinit = false;
        }
        mp_idle(mpctx);
    }
}
