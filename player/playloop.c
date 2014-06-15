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
#include "screenshot.h"
#include "command.h"

#define WAKEUP_PERIOD 0.5

static const char av_desync_help_text[] =
"\n\n"
"           *************************************************\n"
"           **** Audio/Video desynchronisation detected! ****\n"
"           *************************************************\n\n"
"This means either the audio or the video is played too slowly.\n"
"Possible reasons, problems, workarounds:\n"
"- Your system is simply too slow for this file.\n"
"     Transcode it to a lower bitrate file with tools like HandBrake.\n"
"- Broken/buggy _audio_ driver.\n"
"     Experiment with different values for --autosync, 30 is a good start.\n"
"     If you have PulseAudio, try --ao=alsa .\n"
"- Slow video output.\n"
"     Try a different --vo driver (--vo=help for a list) or try --framedrop!\n"
"- Playing a video file with --vo=opengl with higher FPS than the monitor.\n"
"     This is due to vsync limiting the framerate.\n"
"- Playing from a slow network source.\n"
"     Download the file instead.\n"
"- Try to find out whether audio/video/subs are causing this by experimenting\n"
"  with --no-video, --no-audio, or --no-sub.\n"
"- If you swiched audio or video tracks, try seeking to force synchronization.\n"
"If none of this helps you, file a bug report.\n\n";


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
    mpctx->paused_for_cache = false;

    if (mpctx->video_out && mpctx->d_video && mpctx->video_out->config_ok)
        vo_control(mpctx->video_out, VOCTRL_PAUSE, NULL);

    if (mpctx->ao && mpctx->d_audio)
        ao_pause(mpctx->ao);    // pause audio, keep data if possible

    // Only print status if there's actually a file being played.
    if (mpctx->num_sources)
        print_status(mpctx);

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

    if (mpctx->ao && mpctx->d_audio)
        ao_resume(mpctx->ao);
    if (mpctx->video_out && mpctx->d_video && mpctx->video_out->config_ok)
        vo_control(mpctx->video_out, VOCTRL_RESUME, NULL);      // resume video
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

static void seek_reset(struct MPContext *mpctx, bool reset_ao, bool reset_ac)
{
    if (mpctx->d_video) {
        video_reset_decoding(mpctx->d_video);
        vo_seek_reset(mpctx->video_out);
    }

    if (mpctx->d_audio && reset_ac) {
        audio_reset_decoding(mpctx->d_audio);
        if (reset_ao)
            clear_audio_output_buffers(mpctx);
    }

    reset_subtitles(mpctx, 0);
    reset_subtitles(mpctx, 1);

    mpctx->video_pts = MP_NOPTS_VALUE;
    mpctx->video_next_pts = MP_NOPTS_VALUE;
    mpctx->playing_last_frame = false;
    mpctx->last_frame_duration = 0;
    mpctx->delay = 0;
    mpctx->time_frame = 0;
    mpctx->restart_playback = true;
    mpctx->hrseek_active = false;
    mpctx->hrseek_framedrop = false;
    mpctx->total_avsync_change = 0;
    mpctx->drop_frame_cnt = 0;
    mpctx->dropped_frames = 0;
    mpctx->playback_pts = MP_NOPTS_VALUE;
    mpctx->eof_reached = false;

#if HAVE_ENCODING
    encode_lavc_discontinuity(mpctx->encode_lavc_ctx);
#endif

    mp_notify(mpctx, MPV_EVENT_SEEK, NULL);
}

// return -1 if seek failed (non-seekable stream?), 0 otherwise
static int mp_seek(MPContext *mpctx, struct seek_params seek,
                   bool timeline_fallthrough)
{
    struct MPOpts *opts = mpctx->opts;
    uint64_t prev_seek_ts = mpctx->vo_pts_history_seek_ts;

    if (!mpctx->demuxer)
        return -1;

    if (!mpctx->demuxer->seekable) {
        MP_ERR(mpctx, "Can't seek in this file.\n");
        return -1;
    }

    if (mpctx->stop_play == AT_END_OF_FILE)
        mpctx->stop_play = KEEP_PLAYING;

    double hr_seek_offset = opts->hr_seek_demuxer_offset;
    bool hr_seek_very_exact = seek.exact > 1;
    // Always try to compensate for possibly bad demuxers in "special"
    // situations where we need more robustness from the hr-seek code, even
    // if the user doesn't use --hr-seek-demuxer-offset.
    // The value is arbitrary, but should be "good enough" in most situations.
    if (hr_seek_very_exact)
        hr_seek_offset = MPMAX(hr_seek_offset, 0.5); // arbitrary

    bool hr_seek = mpctx->demuxer->accurate_seek && opts->correct_pts;
    hr_seek &= seek.exact >= 0 && seek.type != MPSEEK_FACTOR;
    hr_seek &= (opts->hr_seek == 0 && seek.type == MPSEEK_ABSOLUTE) ||
               opts->hr_seek > 0 || seek.exact > 0;
    if (seek.type == MPSEEK_FACTOR || seek.amount < 0 ||
        (seek.type == MPSEEK_ABSOLUTE && seek.amount < mpctx->last_chapter_pts))
        mpctx->last_chapter_seek = -2;
    if (seek.type == MPSEEK_FACTOR) {
        double len = get_time_length(mpctx);
        if (len > 0 && !mpctx->demuxer->ts_resets_possible) {
            seek.amount = seek.amount * len + get_start_time(mpctx);
            seek.type = MPSEEK_ABSOLUTE;
        }
    }
    if ((mpctx->demuxer->accurate_seek || mpctx->timeline)
        && seek.type == MPSEEK_RELATIVE) {
        seek.type = MPSEEK_ABSOLUTE;
        seek.direction = seek.amount > 0 ? 1 : -1;
        seek.amount += get_current_time(mpctx);
    }

    /* At least the liba52 decoder wants to read from the input stream
     * during initialization, so reinit must be done after the demux_seek()
     * call that clears possible stream EOF. */
    bool need_reset = false;
    double demuxer_amount = seek.amount;
    if (mpctx->timeline) {
        demuxer_amount = timeline_set_from_time(mpctx, seek.amount,
                                                &need_reset);
        if (demuxer_amount == -1) {
            assert(!need_reset);
            mpctx->stop_play = AT_END_OF_FILE;
            if (mpctx->d_audio && !timeline_fallthrough) {
                // Seek outside of the file -> clear audio from current position
                clear_audio_decode_buffers(mpctx);
                clear_audio_output_buffers(mpctx);
            }
            return -1;
        }
    }
    if (need_reset) {
        reinit_video_chain(mpctx);
        reinit_subs(mpctx, 0);
        reinit_subs(mpctx, 1);
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
    if (hr_seek || seek.direction < 0)
        demuxer_style |= SEEK_BACKWARD;
    else if (seek.direction > 0)
        demuxer_style |= SEEK_FORWARD;
    if (hr_seek || opts->mkv_subtitle_preroll)
        demuxer_style |= SEEK_SUBPREROLL;

    if (hr_seek)
        demuxer_amount -= hr_seek_offset;
    int seekresult = demux_seek(mpctx->demuxer, demuxer_amount, demuxer_style);
    if (seekresult == 0) {
        if (need_reset) {
            reinit_audio_chain(mpctx);
            seek_reset(mpctx, !timeline_fallthrough, false);
        }
        return -1;
    }

    // Seek external, extra files too:
    for (int t = 0; t < mpctx->num_tracks; t++) {
        struct track *track = mpctx->tracks[t];
        if (track->selected && track->is_external && track->demuxer) {
            double main_new_pos;
            if (seek.type == MPSEEK_ABSOLUTE) {
                main_new_pos = seek.amount - mpctx->video_offset;
            } else {
                main_new_pos = get_main_demux_pts(mpctx);
            }
            demux_seek(track->demuxer, main_new_pos, SEEK_ABSOLUTE);
        }
    }

    if (need_reset)
        reinit_audio_chain(mpctx);
    /* If we just reinitialized audio it doesn't need to be reset,
     * and resetting could lose audio some decoders produce during init. */
    seek_reset(mpctx, !timeline_fallthrough, !need_reset);

    if (timeline_fallthrough) {
        // Important if video reinit happens.
        mpctx->vo_pts_history_seek_ts = prev_seek_ts;
    } else {
        mpctx->vo_pts_history_seek_ts++;
        mpctx->backstep_active = false;
    }

    /* Use the target time as "current position" for further relative
     * seeks etc until a new video frame has been decoded */
    if (seek.type == MPSEEK_ABSOLUTE) {
        mpctx->video_pts = seek.amount;
        mpctx->last_seek_pts = seek.amount;
    } else
        mpctx->last_seek_pts = MP_NOPTS_VALUE;

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

    return 0;
}

// This combines consecutive seek requests.
void queue_seek(struct MPContext *mpctx, enum seek_type type, double amount,
                int exact, bool immediate)
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
        if (seek->amount == 0) {
            *seek = (struct seek_params){ 0 };
            return;
        }
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
        /* If the user seeks continuously (keeps arrow key down)
         * try to finish showing a frame from one location before doing
         * another seek (which could lead to unchanging display). */
        if (!mpctx->seek.immediate && mpctx->restart_playback &&
            mp_time_sec() - mpctx->start_timestamp < 0.3)
            return;
        mp_seek(mpctx, mpctx->seek, false);
        mpctx->seek = (struct seek_params){0};
    }
}

double get_time_length(struct MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return 0;

    if (mpctx->timeline)
        return mpctx->timeline[mpctx->num_timeline_parts].start;

    double len = demuxer_get_time_length(demuxer);
    if (len >= 0)
        return len;

    // Unknown
    return 0;
}

/* If there are timestamps from stream level then use those (for example
 * DVDs can have consistent times there while the MPEG-level timestamps
 * reset). */
double get_current_time(struct MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return 0;
    if (demuxer->stream_pts != MP_NOPTS_VALUE)
        return demuxer->stream_pts;
    if (mpctx->playback_pts != MP_NOPTS_VALUE)
        return mpctx->playback_pts;
    if (mpctx->last_seek_pts != MP_NOPTS_VALUE)
        return mpctx->last_seek_pts;
    return 0;
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
        if (endpos == MP_NOPTS_VALUE || endpos > start + len)
            endpos = start + len;
        if (startpos == MP_NOPTS_VALUE || startpos < start)
            startpos = start;
        if (endpos < startpos)
            endpos = startpos;
        start = startpos;
        len = endpos - startpos;
    }
    double pos = get_current_time(mpctx);
    if (len > 0 && !demuxer->ts_resets_possible) {
        ans = MPCLAMP((pos - start) / len, 0, 1);
    } else {
        struct stream *s = demuxer->stream;
        int64_t size;
        if (stream_control(s, STREAM_CTRL_GET_SIZE, &size) > 0 && size > 0) {
            int64_t fpos =
                demuxer->filepos >= 0 ? demuxer->filepos : stream_tell(s);
            ans = MPCLAMP(fpos / (double)size, 0, 1);
        }
    }
    if (use_range) {
        if (mpctx->opts->play_frames > 0)
            ans = MPMAX(ans, 1.0 -
                    mpctx->max_frames / (double) mpctx->opts->play_frames);
    }
    return ans;
}

int get_percent_pos(struct MPContext *mpctx)
{
    int pos = get_current_pos_ratio(mpctx, false) * 100;
    return MPCLAMP(pos, 0, 100);
}

// -2 is no chapters, -1 is before first chapter
int get_current_chapter(struct MPContext *mpctx)
{
    if (!mpctx->num_chapters)
        return -2;
    double current_pts = get_current_time(mpctx);
    int i;
    for (i = 1; i < mpctx->num_chapters; i++)
        if (current_pts < mpctx->chapters[i].start)
            break;
    return MPMAX(mpctx->last_chapter_seek, i - 1);
}

char *chapter_display_name(struct MPContext *mpctx, int chapter)
{
    char *name = chapter_name(mpctx, chapter);
    char *dname = name;
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
    if (dname != name)
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
        return mpctx->chapters[chapter].start;
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

    queue_seek(mpctx, MPSEEK_ABSOLUTE, pts, 0, true);
    mpctx->last_chapter_seek = chapter;
    mpctx->last_chapter_pts = pts;
    return true;
}

static void update_avsync(struct MPContext *mpctx)
{
    if (!mpctx->d_audio || !mpctx->d_video)
        return;

    double a_pos = playing_audio_pts(mpctx);

    mpctx->last_av_difference = a_pos - mpctx->video_pts + mpctx->audio_delay;
    if (mpctx->time_frame > 0)
        mpctx->last_av_difference +=
                mpctx->time_frame * mpctx->opts->playback_speed;
    if (a_pos == MP_NOPTS_VALUE || mpctx->video_pts == MP_NOPTS_VALUE)
        mpctx->last_av_difference = MP_NOPTS_VALUE;
    if (mpctx->last_av_difference > 0.5 && mpctx->drop_frame_cnt > 50
        && !mpctx->drop_message_shown) {
        MP_WARN(mpctx, "%s", av_desync_help_text);
        mpctx->drop_message_shown = true;
    }
}

/* Modify video timing to match the audio timeline. There are two main
 * reasons this is needed. First, video and audio can start from different
 * positions at beginning of file or after a seek (MPlayer starts both
 * immediately even if they have different pts). Second, the file can have
 * audio timestamps that are inconsistent with the duration of the audio
 * packets, for example two consecutive timestamp values differing by
 * one second but only a packet with enough samples for half a second
 * of playback between them.
 */
static void adjust_sync(struct MPContext *mpctx, double frame_time)
{
    struct MPOpts *opts = mpctx->opts;

    if (!mpctx->d_audio || mpctx->syncing_audio)
        return;

    double a_pts = written_audio_pts(mpctx) - mpctx->delay;
    double v_pts = mpctx->video_next_pts;
    double av_delay = a_pts - v_pts;
    // Try to sync vo_flip() so it will *finish* at given time
    av_delay += mpctx->last_vo_flip_duration;
    av_delay += mpctx->audio_delay;   // This much pts difference is desired

    double change = av_delay * 0.1;
    double max_change = opts->default_max_pts_correction >= 0 ?
                        opts->default_max_pts_correction : frame_time * 0.1;
    if (change < -max_change)
        change = -max_change;
    else if (change > max_change)
        change = max_change;
    mpctx->delay += change;
    mpctx->total_avsync_change += change;
}

static bool handle_osd_redraw(struct MPContext *mpctx)
{
    if (!mpctx->video_out || !mpctx->video_out->config_ok)
        return false;
    bool want_redraw = vo_get_want_redraw(mpctx->video_out) |
                       osd_query_and_reset_want_redraw(mpctx->osd);
    if (!want_redraw)
        return false;
    vo_redraw(mpctx->video_out);
    return true;
}

static void handle_metadata_update(struct MPContext *mpctx)
{
    if (mp_time_sec() > mpctx->last_metadata_update + 2) {
        if (demux_info_update(mpctx->demuxer))
            mp_notify(mpctx, MPV_EVENT_METADATA_UPDATE, NULL);
        mpctx->last_metadata_update = mp_time_sec();
    }
}

static void handle_pause_on_low_cache(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    if (!mpctx->stream)
        return;
    int64_t fill = -1;
    stream_control(mpctx->stream, STREAM_CTRL_GET_CACHE_FILL, &fill);
    int cache_kb = fill > 0 ? (fill + 1023) / 1024 : -1;
    bool idle = mp_get_cache_idle(mpctx);
    if (mpctx->paused && mpctx->paused_for_cache) {
        if (cache_kb < 0 || cache_kb >= opts->stream_cache_unpause || idle) {
            mpctx->paused_for_cache = false;
            if (!opts->pause)
                unpause_player(mpctx);
        }
    } else {
        if (cache_kb >= 0 && cache_kb <= opts->stream_cache_pause && !idle &&
            opts->stream_cache_pause < opts->stream_cache_unpause)
        {
            bool prev_paused_user = opts->pause;
            pause_player(mpctx);
            mpctx->paused_for_cache = true;
            opts->pause = prev_paused_user;
        }
    }
}

static void handle_heartbeat_cmd(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    if (opts->heartbeat_cmd && !mpctx->paused) {
        double now = mp_time_sec();
        if (now - mpctx->last_heartbeat > opts->heartbeat_interval) {
            mpctx->last_heartbeat = now;
            system(opts->heartbeat_cmd);
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

    unsigned mouse_event_ts = mp_input_get_mouse_event_counter(mpctx->input);
    if (mpctx->mouse_event_ts != mouse_event_ts) {
        mpctx->mouse_event_ts = mouse_event_ts;
        mpctx->mouse_timer =
            mp_time_sec() + opts->cursor_autohide_delay / 1000.0;
        mouse_cursor_visible = true;
    }

    if (mp_time_sec() >= mpctx->mouse_timer)
        mouse_cursor_visible = false;

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

static void handle_input_and_seek_coalesce(struct MPContext *mpctx)
{
    mp_cmd_t *cmd;
    while ((cmd = mp_input_get_cmd(mpctx->input, 0, 1)) != NULL) {
        mp_dispatch_queue_process(mpctx->dispatch, 0);
        cmd = mp_input_get_cmd(mpctx->input, 0, 0);
        run_command(mpctx, cmd);
        mp_cmd_free(cmd);
        if (mpctx->stop_play)
            break;
    }
    mp_dispatch_queue_process(mpctx->dispatch, 0);
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
    bool demuxer_ok = mpctx->demuxer && mpctx->demuxer->accurate_seek;
    if (demuxer_ok && mpctx->d_video && current_pts != MP_NOPTS_VALUE) {
        double seek_pts = find_previous_pts(mpctx, current_pts);
        if (seek_pts != MP_NOPTS_VALUE) {
            queue_seek(mpctx, MPSEEK_ABSOLUTE, seek_pts, 2, true);
        } else {
            double last = get_last_frame_pts(mpctx);
            if (last != MP_NOPTS_VALUE && last >= current_pts &&
                mpctx->backstep_start_seek_ts != mpctx->vo_pts_history_seek_ts)
            {
                MP_ERR(mpctx, "Backstep failed.\n");
                queue_seek(mpctx, MPSEEK_ABSOLUTE, current_pts, 2, true);
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
    if (opts->step_sec > 0 && !mpctx->stop_play && !mpctx->paused &&
        !mpctx->restart_playback)
    {
        set_osd_function(mpctx, OSD_FFW);
        queue_seek(mpctx, MPSEEK_RELATIVE, opts->step_sec, 0, true);
    }
}

static void handle_loop_file(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    if (opts->loop_file && mpctx->stop_play == AT_END_OF_FILE) {
        set_osd_function(mpctx, OSD_FFW);
        queue_seek(mpctx, MPSEEK_ABSOLUTE, get_start_time(mpctx), 0, true);
    }
}

static void handle_keep_open(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    if (opts->keep_open && mpctx->stop_play == AT_END_OF_FILE) {
        mpctx->stop_play = KEEP_PLAYING;
        mpctx->playback_pts = mpctx->last_vo_pts;
        mpctx->eof_reached = true;
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
void handle_force_window(struct MPContext *mpctx, bool reconfig)
{
    // Don't interfere with real video playback
    if (mpctx->d_video)
        return;

    struct vo *vo = mpctx->video_out;
    if (!vo)
        return;

    if (!vo->config_ok || reconfig) {
        MP_INFO(mpctx, "Creating non-video VO window.\n");
        // Pick whatever works
        int config_format = 0;
        for (int fmt = IMGFMT_START; fmt < IMGFMT_END; fmt++) {
            if (vo->driver->query_format(vo, fmt)) {
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
        vo_reconfig(vo, &p, 0);
        vo_redraw(vo);
        mp_notify(mpctx, MPV_EVENT_VIDEO_RECONFIG, NULL);
    }
}

static double timing_sleep(struct MPContext *mpctx, double time_frame)
{
    // assume kernel HZ=100 for softsleep, works with larger HZ but with
    // unnecessarily high CPU usage
    struct MPOpts *opts = mpctx->opts;
    double margin = opts->softsleep ? 0.011 : 0;
    while (time_frame > margin) {
        mp_sleep_us(1000000 * (time_frame - margin));
        time_frame -= get_relative_time(mpctx);
    }
    if (opts->softsleep) {
        if (time_frame < 0)
            MP_WARN(mpctx, "Warning! Softsleep underflow!\n");
        while (time_frame > 0)
            time_frame -= get_relative_time(mpctx);  // burn the CPU
    }
    return time_frame;
}

static double get_wakeup_period(struct MPContext *mpctx)
{
    /* Even if we can immediately wake up in response to most input events,
     * there are some timers which are not registered to the event loop
     * and need to be checked periodically (like automatic mouse cursor hiding).
     * OSD content updates behave similarly. Also some uncommon input devices
     * may not have proper FD event support.
     */
    double sleeptime = WAKEUP_PERIOD;

#if !HAVE_POSIX_SELECT
    // No proper file descriptor event handling; keep waking up to poll input
    sleeptime = MPMIN(sleeptime, 0.02);
#endif

    if (mpctx->video_out)
        if (mpctx->video_out->wakeup_period > 0)
            sleeptime = MPMIN(sleeptime, mpctx->video_out->wakeup_period);

    return sleeptime;
}

void run_playloop(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    bool full_audio_buffers = false;
    bool audio_left = false, video_left = false;
    double endpts = get_play_end_pts(mpctx);
    bool end_is_chapter = false;
    double sleeptime = get_wakeup_period(mpctx);
    bool was_restart = mpctx->restart_playback;
    bool new_frame_shown = false;

#if HAVE_ENCODING
    if (encode_lavc_didfail(mpctx->encode_lavc_ctx)) {
        mpctx->stop_play = PT_QUIT;
        return;
    }
#endif

    // Add tracks that were added by the demuxer later (e.g. MPEG)
    if (!mpctx->timeline && mpctx->demuxer)
        add_demuxer_tracks(mpctx, mpctx->demuxer);

    if (mpctx->timeline) {
        double end = mpctx->timeline[mpctx->timeline_part + 1].start;
        if (endpts == MP_NOPTS_VALUE || end < endpts) {
            endpts = end;
            end_is_chapter = true;
        }
    }

    if (opts->chapterrange[1] > 0) {
        double end = chapter_start_time(mpctx, opts->chapterrange[1]);
        if (end != MP_NOPTS_VALUE && (endpts == MP_NOPTS_VALUE || end < endpts))
            endpts = end;
    }

    if (mpctx->d_audio && !mpctx->restart_playback && !ao_untimed(mpctx->ao)) {
        int status = fill_audio_out_buffers(mpctx, endpts);
        full_audio_buffers = status >= 0;
        // Not at audio stream EOF yet
        audio_left = status > -2;
    }

    if (mpctx->video_out) {
        vo_check_events(mpctx->video_out);
        handle_cursor_autohide(mpctx);
        handle_heartbeat_cmd(mpctx);
    }

    while (mpctx->d_video) {   // never loops, for "break;" only
        struct vo *vo = mpctx->video_out;
        update_fps(mpctx);

        // Whether there's still at least 1 video frame that can be shown.
        // If false, it means we can reconfig the VO if needed (normally, this
        // would disrupt playback, so only do it on !still_playing).
        bool still_playing = vo_has_next_frame(vo, true);
        // For the last frame case (frame is being displayed).
        still_playing |= mpctx->playing_last_frame;
        still_playing |= mpctx->last_frame_duration > 0;

        double frame_time = 0;
        int r = update_video(mpctx, endpts, !still_playing, &frame_time);

        MP_TRACE(mpctx, "update_video: %d\n", r);
        if (r < 0) {
            MP_FATAL(mpctx, "Could not initialize video chain.\n");
            int uninit = INITIALIZED_VCODEC;
            if (!opts->force_vo)
                uninit |= INITIALIZED_VO;
            uninit_player(mpctx, uninit);
            if (!mpctx->current_track[STREAM_AUDIO])
                mpctx->stop_play = PT_NEXT_ENTRY;
            mpctx->error_playing = true;
            handle_force_window(mpctx, true);
            break;
        }

        if (r == 0) {
            if (!mpctx->playing_last_frame && mpctx->last_frame_duration > 0) {
                mpctx->time_frame += mpctx->last_frame_duration;
                mpctx->last_frame_duration = 0;
                mpctx->playing_last_frame = true;
                MP_VERBOSE(mpctx, "showing last frame\n");
            }
            if (mpctx->playing_last_frame) {
                r = 1; // don't stop playback yet
                MP_TRACE(mpctx, "still showing last frame\n");
            }
        }

        video_left = r > 0;

        if (r == 2)
            MP_TRACE(mpctx, "frametime=%5.3f\n", frame_time);

        if (r == 2 && !mpctx->restart_playback) {
            mpctx->time_frame += frame_time / opts->playback_speed;
            adjust_sync(mpctx, frame_time);
        }

        if (!video_left) {
            mpctx->delay = 0;
            mpctx->last_av_difference = 0;
        }

        if (!video_left || (mpctx->paused && !mpctx->restart_playback)) {
            if (mpctx->paused)
                video_left |= vo->hasframe;
            break;
        }

        if (r != 2 && !mpctx->playing_last_frame) {
            sleeptime = 0;
            break;
        }

        mpctx->time_frame -= get_relative_time(mpctx);
        double audio_pts = playing_audio_pts(mpctx);
        if (!mpctx->sync_audio_to_video) {
            mpctx->time_frame = 0;
        } else if (full_audio_buffers && !mpctx->restart_playback) {
            double buffered_audio = ao_get_delay(mpctx->ao);
            MP_TRACE(mpctx, "audio delay=%f\n", buffered_audio);

            if (opts->autosync) {
                /* Smooth reported playback position from AO by averaging
                 * it with the value expected based on previus value and
                 * time elapsed since then. May help smooth video timing
                 * with audio output that have inaccurate position reporting.
                 * This is badly implemented; the behavior of the smoothing
                 * now undesirably depends on how often this code runs
                 * (mainly depends on video frame rate). */
                float predicted = (mpctx->delay / opts->playback_speed +
                                   mpctx->time_frame);
                float difference = buffered_audio - predicted;
                buffered_audio = predicted + difference / opts->autosync;
            }

            mpctx->time_frame = (buffered_audio -
                                 mpctx->delay / opts->playback_speed);
        } else {
            /* If we're more than 200 ms behind the right playback
             * position, don't try to speed up display of following
             * frames to catch up; continue with default speed from
             * the current frame instead.
             * If untimed is set always output frames immediately
             * without sleeping.
             */
            if (mpctx->time_frame < -0.2 || opts->untimed || vo->untimed)
                mpctx->time_frame = 0;
        }

        double vsleep = mpctx->time_frame - vo->flip_queue_offset;
        if (vsleep > 0.050) {
            sleeptime = MPMIN(sleeptime, vsleep - 0.040);
            break;
        }
        sleeptime = 0;
        mpctx->playing_last_frame = false;

        // last frame case (don't set video_left - consider format changes)
        if (r != 2)
            break;

        //=================== FLIP PAGE (VIDEO BLT): ======================


        mpctx->video_pts = mpctx->video_next_pts;
        mpctx->last_vo_pts = mpctx->video_pts;
        mpctx->playback_pts = mpctx->video_pts;

        update_subtitles(mpctx);
        update_osd_msg(mpctx);

        MP_STATS(mpctx, "vo draw frame");

        vo_new_frame_imminent(vo);

        MP_STATS(mpctx, "vo sleep");

        mpctx->time_frame -= get_relative_time(mpctx);
        mpctx->time_frame -= vo->flip_queue_offset;
        if (mpctx->time_frame > 0.001)
            mpctx->time_frame = timing_sleep(mpctx, mpctx->time_frame);
        mpctx->time_frame += vo->flip_queue_offset;

        int64_t t2 = mp_time_us();
        /* Playing with playback speed it's possible to get pathological
         * cases with mpctx->time_frame negative enough to cause an
         * overflow in pts_us calculation, thus the MPMAX. */
        double time_frame = MPMAX(mpctx->time_frame, -1);
        int64_t pts_us = mpctx->last_time + time_frame * 1e6;
        int duration = -1;
        double pts2 = vo_get_next_pts(vo, 0); // this is the next frame PTS
        if (mpctx->video_pts != MP_NOPTS_VALUE && pts2 == MP_NOPTS_VALUE) {
            // Make up a frame duration. Using the frame rate is not a good
            // choice, since the frame rate could be unset/broken/random.
            float fps = mpctx->d_video->fps;
            double frame_duration = fps > 0 ? 1.0 / fps : 0;
            pts2 = mpctx->video_pts + MPCLAMP(frame_duration, 0.0, 5.0);
        }
        if (pts2 != MP_NOPTS_VALUE) {
            // expected A/V sync correction is ignored
            double diff = (pts2 - mpctx->video_pts);
            diff /= opts->playback_speed;
            if (mpctx->time_frame < 0)
                diff += mpctx->time_frame;
            if (diff < 0)
                diff = 0;
            if (diff > 10)
                diff = 10;
            duration = diff * 1e6;
            mpctx->last_frame_duration = diff;
        }
        if (mpctx->restart_playback)
            duration = -1;

        MP_STATS(mpctx, "start flip");
        vo_flip_page(vo, pts_us | 1, duration);
        MP_STATS(mpctx, "end flip");

        if (audio_pts != MP_NOPTS_VALUE)
            MP_STATS(mpctx, "value %f ptsdiff", mpctx->video_pts - audio_pts);

        mpctx->last_vo_flip_duration = (mp_time_us() - t2) * 0.000001;
        if (vo->driver->flip_page_timed) {
            // No need to adjust sync based on flip speed
            mpctx->last_vo_flip_duration = 0;
            // For print_status - VO call finishing early is OK for sync
            mpctx->time_frame -= get_relative_time(mpctx);
        }
        mpctx->shown_vframes++;
        if (mpctx->restart_playback) {
            if (mpctx->sync_audio_to_video) {
                mpctx->syncing_audio = true;
                if (mpctx->d_audio)
                    fill_audio_out_buffers(mpctx, endpts);
                mpctx->restart_playback = false;
                mp_notify(mpctx, MPV_EVENT_PLAYBACK_RESTART, NULL);
            }
            mpctx->time_frame = 0;
            get_relative_time(mpctx);
        }
        update_avsync(mpctx);
        screenshot_flip(mpctx);
        new_frame_shown = true;

        mp_notify(mpctx, MPV_EVENT_TICK, NULL);

        break;
    } // video

    if (!video_left || mpctx->paused) {
        if (mp_time_sec() - mpctx->last_idle_tick > 0.5) {
            mpctx->last_idle_tick = mp_time_sec();
            mp_notify(mpctx, MPV_EVENT_TICK, NULL);
        }
    }

    video_left &= mpctx->sync_audio_to_video; // force no-video semantics

    if (mpctx->d_audio && (mpctx->restart_playback ? !video_left :
                           ao_untimed(mpctx->ao) && (mpctx->delay <= 0 ||
                                                     !video_left)))
    {
        int status = fill_audio_out_buffers(mpctx, endpts);
        full_audio_buffers = status >= 0 && !ao_untimed(mpctx->ao);
        // Not at audio stream EOF yet
        audio_left = status > -2;
    }
    if (!video_left)
        mpctx->restart_playback = false;

    update_osd_msg(mpctx);

    if (!video_left && (!mpctx->paused || was_restart)) {
        double a_pos = 0;
        if (mpctx->d_audio)
            a_pos = playing_audio_pts(mpctx);
        mpctx->playback_pts = a_pos;
        if (was_restart)
            mp_notify(mpctx, MPV_EVENT_PLAYBACK_RESTART, NULL);
    }

    update_subtitles(mpctx);

    /* It's possible for the user to simultaneously switch both audio
     * and video streams to "disabled" at runtime. Handle this by waiting
     * rather than immediately stopping playback due to EOF.
     *
     * When all audio has been written to output driver, stay in the
     * main loop handling commands until it has been mostly consumed,
     * except in the gapless case, where the next file will be started
     * while audio from the current one still remains to be played.
     *
     * We want this check to trigger if we seeked to this position,
     * but not if we paused at it with audio possibly still buffered in
     * the AO. There's currently no working way to check buffered audio
     * inside AO while paused. Thus the "was_restart" check below, which
     * should trigger after seek only, when we know there's no audio
     * buffered.
     */
    if ((mpctx->d_audio || mpctx->d_video) && !audio_left && !video_left
        && (opts->gapless_audio || !mpctx->d_audio || ao_eof_reached(mpctx->ao))
        && (!mpctx->paused || was_restart)) {
        if (end_is_chapter) {
            mp_seek(mpctx, (struct seek_params){
                           .type = MPSEEK_ABSOLUTE,
                           .amount = mpctx->timeline[mpctx->timeline_part+1].start
                           }, true);
        } else
            mpctx->stop_play = AT_END_OF_FILE;
    }

    mp_handle_nav(mpctx);

    if (!mpctx->stop_play && !mpctx->restart_playback) {

        // If no more video is available, one frame means one playloop iteration.
        // Otherwise, one frame means one video frame.
        if (!video_left)
            new_frame_shown = true;

        if (opts->playing_msg && !mpctx->playing_msg_shown && new_frame_shown) {
            mpctx->playing_msg_shown = true;
            char *msg =
                mp_property_expand_escaped_string(mpctx, opts->playing_msg);
            MP_INFO(mpctx, "%s\n", msg);
            talloc_free(msg);
        }

        if (mpctx->max_frames >= 0) {
            if (new_frame_shown)
                mpctx->max_frames--;
            if (mpctx->max_frames <= 0)
                mpctx->stop_play = PT_NEXT_ENTRY;
        }

        if (mpctx->step_frames > 0 && !mpctx->paused) {
            if (new_frame_shown)
                mpctx->step_frames--;
            if (mpctx->step_frames == 0)
                pause_player(mpctx);
        }

    }

    if (!mpctx->stop_play) {
        if (mpctx->restart_playback)
            sleeptime = 0;
        if (sleeptime > 0) {
            if (handle_osd_redraw(mpctx))
                sleeptime = 0;
        }
        if (sleeptime > 0) {
            MP_STATS(mpctx, "start sleep");
            mp_input_get_cmd(mpctx->input, sleeptime * 1000, true);
            MP_STATS(mpctx, "end sleep");
        }
    }

    handle_metadata_update(mpctx);

    handle_pause_on_low_cache(mpctx);

    handle_input_and_seek_coalesce(mpctx);

    handle_backstep(mpctx);

    handle_sstep(mpctx);

    handle_loop_file(mpctx);

    handle_keep_open(mpctx);

    handle_chapter_change(mpctx);

    handle_force_window(mpctx, false);

    execute_queued_seek(mpctx);

    if (mpctx->opts->use_terminal)
        getch2_poll();
}

// Waiting for the slave master to send us a new file to play.
void idle_loop(struct MPContext *mpctx)
{
    // ================= idle loop (STOP state) =========================
    bool need_reinit = true;
    while (mpctx->opts->player_idle_mode && !mpctx->playlist->current
           && mpctx->stop_play != PT_QUIT)
    {
        mpctx->eof_reached = true;
        if (need_reinit) {
            mp_notify(mpctx, MPV_EVENT_IDLE, NULL);
            handle_force_window(mpctx, true);
        }
        need_reinit = false;
        int uninit = INITIALIZED_AO;
        if (!mpctx->opts->force_vo)
            uninit |= INITIALIZED_VO;
        uninit_player(mpctx, uninit);
        handle_force_window(mpctx, false);
        if (mpctx->video_out)
            vo_check_events(mpctx->video_out);
        update_osd_msg(mpctx);
        handle_osd_redraw(mpctx);
        mp_cmd_t *cmd = mp_input_get_cmd(mpctx->input,
                                         get_wakeup_period(mpctx) * 1000,
                                         false);
        if (cmd)
            run_command(mpctx, cmd);
        mp_cmd_free(cmd);
        mp_dispatch_queue_process(mpctx->dispatch, 0);
        if (mpctx->opts->use_terminal)
            getch2_poll();
    }
}
