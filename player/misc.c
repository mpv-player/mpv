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
#include <pthread.h>
#include <assert.h>

#include "config.h"
#include "talloc.h"

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

double get_relative_time(struct MPContext *mpctx)
{
    int64_t new_time = mp_time_us();
    int64_t delta = new_time - mpctx->last_time;
    mpctx->last_time = new_time;
    return delta * 0.000001;
}

double rel_time_to_abs(struct MPContext *mpctx, struct m_rel_time t)
{
    double length = get_time_length(mpctx);
    double start = get_start_time(mpctx);
    switch (t.type) {
    case REL_TIME_ABSOLUTE:
        return t.pos;
    case REL_TIME_RELATIVE:
        if (t.pos >= 0) {
            return start + t.pos;
        } else {
            if (length >= 0)
                return MPMAX(start + length + t.pos, 0.0);
        }
        break;
    case REL_TIME_PERCENT:
        if (length >= 0)
            return start + length * (t.pos / 100.0);
        break;
    case REL_TIME_CHAPTER:
        if (chapter_start_time(mpctx, t.pos) != MP_NOPTS_VALUE)
            return chapter_start_time(mpctx, t.pos);
        break;
    }
    return MP_NOPTS_VALUE;
}

double get_play_end_pts(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    double end = MP_NOPTS_VALUE;
    if (opts->play_end.type) {
        end = rel_time_to_abs(mpctx, opts->play_end);
    } else if (opts->play_length.type) {
        double startpts = get_start_time(mpctx);
        double start = rel_time_to_abs(mpctx, opts->play_start);
        if (start == MP_NOPTS_VALUE)
            start = startpts;
        double length = rel_time_to_abs(mpctx, opts->play_length);
        if (start != MP_NOPTS_VALUE && length != MP_NOPTS_VALUE)
            end = start + length;
    }
    if (opts->chapterrange[1] > 0) {
        double cend = chapter_start_time(mpctx, opts->chapterrange[1]);
        if (cend != MP_NOPTS_VALUE && (end == MP_NOPTS_VALUE || cend < end))
            end = cend;
    }
    return end;
}

// Time used to seek external tracks to.
double get_main_demux_pts(struct MPContext *mpctx)
{
    double main_new_pos = MP_NOPTS_VALUE;
    if (mpctx->demuxer) {
        for (int n = 0; n < mpctx->demuxer->num_streams; n++) {
            struct sh_stream *stream = mpctx->demuxer->streams[n];
            if (main_new_pos == MP_NOPTS_VALUE && stream->type != STREAM_SUB)
                main_new_pos = demux_get_next_pts(stream);
        }
    }
    return main_new_pos;
}

double get_start_time(struct MPContext *mpctx)
{
    return mpctx->demuxer ? mpctx->demuxer->start_time : 0;
}

// Get the offset from the given track to the video.
double get_track_video_offset(struct MPContext *mpctx, struct track *track)
{
    if (track && track->under_timeline)
        return mpctx->video_offset;
    if (track && track->is_external)
        return get_start_time(mpctx);
    return 0;
}

float mp_get_cache_percent(struct MPContext *mpctx)
{
    if (mpctx->demuxer) {
        int64_t size = -1;
        int64_t fill = -1;
        demux_stream_control(mpctx->demuxer, STREAM_CTRL_GET_CACHE_SIZE, &size);
        demux_stream_control(mpctx->demuxer, STREAM_CTRL_GET_CACHE_FILL, &fill);
        if (size > 0 && fill >= 0)
            return fill / (size / 100.0);
    }
    return -1;
}

bool mp_get_cache_idle(struct MPContext *mpctx)
{
    int idle = 0;
    if (mpctx->demuxer)
        demux_stream_control(mpctx->demuxer, STREAM_CTRL_GET_CACHE_IDLE, &idle);
    return idle;
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
    if (!track)
        return;
    mp_deselect_track(mpctx, track);
    if (track) {
        if (track->type == STREAM_AUDIO)
            MP_INFO(mpctx, "Audio: no audio\n");
        if (track->type == STREAM_VIDEO)
            MP_INFO(mpctx, "Video: no video\n");
        if (mpctx->opts->stop_playback_on_init_failure ||
            (!mpctx->current_track[0][STREAM_AUDIO] &&
             !mpctx->current_track[0][STREAM_VIDEO]))
        {
            mpctx->stop_play = PT_ERROR;
            if (mpctx->error_playing >= 0)
                mpctx->error_playing = MPV_ERROR_NOTHING_TO_PLAY;
        }
        mpctx->sleeptime = 0;
    }
}

void stream_dump(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    char *filename = opts->stream_dump;
    stream_t *stream = mpctx->stream;
    assert(stream && filename);

    int64_t size = 0;
    stream_control(stream, STREAM_CTRL_GET_SIZE, &size);

    stream_set_capture_file(stream, filename);

    while (mpctx->stop_play == KEEP_PLAYING && !stream->eof) {
        if (!opts->quiet && ((stream->pos / (1024 * 1024)) % 2) == 1) {
            uint64_t pos = stream->pos;
            MP_MSG(mpctx, MSGL_STATUS, "Dumping %lld/%lld...",
                   (long long int)pos, (long long int)size);
        }
        stream_fill_buffer(stream);
        mp_process_input(mpctx);
    }
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

// Create a talloc'ed copy of mpctx->global. It contains a copy of the global
// option struct. It still just references some things though, like mp_log.
// The main purpose is letting threads access the option struct without the
// need for additional synchronization.
struct mpv_global *create_sub_global(struct MPContext *mpctx)
{
    struct mpv_global *new = talloc_ptrtype(NULL, new);
    struct m_config *new_config = m_config_dup(new, mpctx->mconfig);
    *new = (struct mpv_global){
        .log = mpctx->global->log,
        .opts = new_config->optstruct,
    };
    return new;
}

struct wrapper_args {
    struct MPContext *mpctx;
    void (*thread_fn)(void *);
    void *thread_arg;
    pthread_mutex_t mutex;
    bool done;
};

static void *thread_wrapper(void *pctx)
{
    struct wrapper_args *args = pctx;
    mpthread_set_name("opener");
    args->thread_fn(args->thread_arg);
    pthread_mutex_lock(&args->mutex);
    args->done = true;
    pthread_mutex_unlock(&args->mutex);
    mp_input_wakeup(args->mpctx->input); // this interrupts mp_idle()
    return NULL;
}

// Run the thread_fn in a new thread. Wait until the thread returns, but while
// waiting, process input and input commands.
int mpctx_run_reentrant(struct MPContext *mpctx, void (*thread_fn)(void *arg),
                        void *thread_arg)
{
    struct wrapper_args args = {mpctx, thread_fn, thread_arg};
    pthread_mutex_init(&args.mutex, NULL);
    bool success = false;
    pthread_t thread;
    if (pthread_create(&thread, NULL, thread_wrapper, &args))
        goto done;
    while (!success) {
        mp_idle(mpctx);

        if (mpctx->stop_play)
            mp_cancel_trigger(mpctx->playback_abort);

        pthread_mutex_lock(&args.mutex);
        success |= args.done;
        pthread_mutex_unlock(&args.mutex);
    }
    pthread_join(thread, NULL);
done:
    pthread_mutex_destroy(&args.mutex);
    return success ? 0 : -1;
}
