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
#include <assert.h>

#include "config.h"
#include "talloc.h"

#include "osdep/io.h"
#include "osdep/timer.h"

#include "common/msg.h"
#include "options/options.h"
#include "options/m_property.h"
#include "common/common.h"
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
    switch (t.type) {
    case REL_TIME_ABSOLUTE:
        return t.pos;
    case REL_TIME_NEGATIVE:
        if (length != 0)
            return MPMAX(length - t.pos, 0.0);
        break;
    case REL_TIME_PERCENT:
        if (length != 0)
            return length * (t.pos / 100.0);
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
    if (opts->play_end.type) {
        return rel_time_to_abs(mpctx, opts->play_end);
    } else if (opts->play_length.type) {
        double startpts = get_start_time(mpctx);
        double start = rel_time_to_abs(mpctx, opts->play_start);
        if (start == MP_NOPTS_VALUE)
            start = startpts;
        double length = rel_time_to_abs(mpctx, opts->play_length);
        if (start != MP_NOPTS_VALUE && length != MP_NOPTS_VALUE)
            return start + length;
    }
    return MP_NOPTS_VALUE;
}

// Time used to seek external tracks to.
double get_main_demux_pts(struct MPContext *mpctx)
{
    double main_new_pos = MP_NOPTS_VALUE;
    if (mpctx->demuxer) {
        for (int n = 0; n < mpctx->demuxer->num_streams; n++) {
            if (main_new_pos == MP_NOPTS_VALUE)
                main_new_pos = demux_get_next_pts(mpctx->demuxer->streams[n]);
        }
    }
    return main_new_pos;
}

double get_start_time(struct MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return 0;
    // We reload the demuxer on menu transitions; don't make it use the first
    // timestamp it finds as start PTS.
    if (mpctx->nav_state)
        return 0;
    return demuxer_get_start_time(demuxer);
}

int mp_get_cache_percent(struct MPContext *mpctx)
{
    if (mpctx->stream) {
        int64_t size = -1;
        int64_t fill = -1;
        stream_control(mpctx->stream, STREAM_CTRL_GET_CACHE_SIZE, &size);
        stream_control(mpctx->stream, STREAM_CTRL_GET_CACHE_FILL, &fill);
        if (size > 0 && fill >= 0)
            return fill / (size / 100);
    }
    return -1;
}

bool mp_get_cache_idle(struct MPContext *mpctx)
{
    int idle = 0;
    if (mpctx->stream)
        stream_control(mpctx->stream, STREAM_CTRL_GET_CACHE_IDLE, &idle);
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

        if (mpctx->video_out) {
            mpctx->video_out->window_title = talloc_strdup(mpctx->video_out, title);
            vo_control(mpctx->video_out, VOCTRL_UPDATE_WINDOW_TITLE, title);
        }

        if (mpctx->ao) {
            ao_control(mpctx->ao, AOCONTROL_UPDATE_STREAM_TITLE, title);
        }
    } else {
        talloc_free(title);
    }
}

void stream_dump(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    char *filename = opts->stream_dump;
    stream_t *stream = mpctx->stream;
    assert(stream && filename);

    stream_set_capture_file(stream, filename);

    while (mpctx->stop_play == KEEP_PLAYING && !stream->eof) {
        if (!opts->quiet && ((stream->pos / (1024 * 1024)) % 2) == 1) {
            uint64_t pos = stream->pos;
            uint64_t end = stream->end_pos;
            MP_MSG(mpctx, MSGL_STATUS, "Dumping %lld/%lld...",
                   (long long int)pos, (long long int)end);
        }
        stream_fill_buffer(stream);
        for (;;) {
            mp_cmd_t *cmd = mp_input_get_cmd(mpctx->input, 0, false);
            if (!cmd)
                break;
            run_command(mpctx, cmd);
            talloc_free(cmd);
        }
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
