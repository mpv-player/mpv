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
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#include "mpv_talloc.h"

#include "osdep/io.h"
#include "osdep/timer.h"
#include "osdep/threads.h"

#include "common/msg.h"
#include "options/options.h"
#include "options/m_property.h"
#include "options/m_config.h"
#include "options/path.h"
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

const int num_ptracks[STREAM_TYPE_COUNT] = {
    [STREAM_VIDEO] = 1,
    [STREAM_AUDIO] = 1,
    [STREAM_SUB] = 2,
};

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
    if (res == MP_NOPTS_VALUE)
        res = get_start_time(mpctx, mpctx->play_dir);
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

    if (!mpctx->remaining_ab_loops)
        return false;

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
        {
            for (int n = 0; n < num_ptracks[STREAM_SUB]; n++) {
                if (mpctx->current_track[n][STREAM_SUB] == track)
                    return -opts->subs_shared->sub_delay[n];
            }
        }
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

void update_content_type(struct MPContext *mpctx, struct track *track)
{
    enum mp_content_type content_type;
    if (!track || !track->vo_c) {
        content_type = MP_CONTENT_NONE;
    } else if (track->image) {
        content_type = MP_CONTENT_IMAGE;
    } else {
        content_type = MP_CONTENT_VIDEO;
    }
    if (mpctx->video_out)
        vo_control(mpctx->video_out, VOCTRL_CONTENT_TYPE, &content_type);
}

void update_vo_playback_state(struct MPContext *mpctx)
{
    if (mpctx->video_out && mpctx->video_out->config_ok) {
        struct voctrl_playback_state oldstate = mpctx->vo_playback_state;
        double pos = get_current_pos_ratio(mpctx, false);
        struct voctrl_playback_state newstate = {
            .taskbar_progress = mpctx->opts->vo->taskbar_progress && pos >= 0,
            .playing = mpctx->playing,
            .paused = mpctx->paused,
            .position = pos > 0 ? lrint(pos * UINT8_MAX) : 0,
        };

        if (oldstate.taskbar_progress != newstate.taskbar_progress ||
            oldstate.playing != newstate.playing ||
            oldstate.paused != newstate.paused ||
            oldstate.position != newstate.position)
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
        (!mpctx->current_track[0][STREAM_AUDIO] &&
         !mpctx->current_track[0][STREAM_VIDEO]))
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
    bool ok = false;

    char *filename = mp_get_user_path(NULL, mpctx->global, opts->stream_dump);
    stream_t *stream = stream_create(source_filename,
                                     STREAM_ORIGIN_DIRECT | STREAM_READ,
                                     mpctx->playback_abort, mpctx->global);
    if (!stream || stream->is_directory)
        goto done;

    int64_t size = stream_get_size(stream);

    FILE *dest = fopen(filename, "wb");
    if (!dest) {
        MP_ERR(mpctx, "Error opening dump file: %s\n", mp_strerror(errno));
        goto done;
    }

    ok = true;

    while (mpctx->stop_play == KEEP_PLAYING && ok) {
        if (!opts->quiet && ((stream->pos / (1024 * 1024)) % 2) == 1) {
            uint64_t pos = stream->pos;
            MP_MSG(mpctx, MSGL_STATUS, "Dumping %lld/%lld...",
                   (long long int)pos, (long long int)size);
        }
        uint8_t buf[4096];
        int len = stream_read(stream, buf, sizeof(buf));
        if (!len) {
            ok &= stream->eof;
            break;
        }
        ok &= fwrite(buf, len, 1, dest) == 1;
        mp_wakeup_core(mpctx); // don't actually sleep
        mp_idle(mpctx); // but process input
    }

    ok &= fclose(dest) == 0;
done:
    free_stream(stream);
    talloc_free(filename);
    return ok ? 0 : -1;
}

void merge_playlist_files(struct playlist *pl)
{
    if (!pl->num_entries)
        return;
    char *edl = talloc_strdup(NULL, "edl://");
    for (int n = 0; n < pl->num_entries; n++) {
        struct playlist_entry *e = pl->entries[n];
        if (n)
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
    playlist_append_file(pl, edl);
    talloc_free(edl);
}

const char *mp_status_str(enum playback_status st)
{
    switch (st) {
    case STATUS_SYNCING:    return "syncing";
    case STATUS_READY:      return "ready";
    case STATUS_PLAYING:    return "playing";
    case STATUS_DRAINING:   return "draining";
    case STATUS_EOF:        return "eof";
    default:                return "bug";
    }
}

bool str_in_list(bstr str, char **list)
{
    if (!list)
        return false;
    while (*list) {
        if (!bstrcasecmp0(str, *list++))
            return true;
    }
    return false;
}

#define ADD_FLAG(ctx, dst, flag, first) do {                           \
    bstr_xappend_asprintf(ctx, &dst, " %s%s", first ? "[" : "", flag); \
    first = false;                                                     \
} while(0)

char *mp_format_track_metadata(void *ctx, struct track *t, bool add_lang)
{
    struct sh_stream *s = t->stream;
    bstr dst = {0};

    if (t->title)
        bstr_xappend_asprintf(ctx, &dst, "'%s' ", t->title);

    const char *codec = s ? s->codec->codec : NULL;

    bstr_xappend0(ctx, &dst, "(");

    if (add_lang && t->lang)
        bstr_xappend_asprintf(ctx, &dst, "%s ", t->lang);

    bstr_xappend0(ctx, &dst, codec ? codec : "<unknown>");

    if (s && s->codec->codec_profile)
        bstr_xappend_asprintf(ctx, &dst, " [%s]", s->codec->codec_profile);
    if (s && s->codec->disp_w)
        bstr_xappend_asprintf(ctx, &dst, " %dx%d", s->codec->disp_w, s->codec->disp_h);
    if (s && s->codec->fps && !t->image) {
        char *fps = mp_format_double(ctx, s->codec->fps, 4, false, false, true);
        bstr_xappend_asprintf(ctx, &dst, " %s fps", fps);
    }
    if (s && s->codec->channels.num)
        bstr_xappend_asprintf(ctx, &dst, " %dch", s->codec->channels.num);
    if (s && s->codec->samplerate)
        bstr_xappend_asprintf(ctx, &dst, " %d Hz", s->codec->samplerate);
    if (s && s->codec->bitrate > 0 && s->codec->bitrate < INT_MAX - 500) {
        bstr_xappend_asprintf(ctx, &dst, " %d kbps", (s->codec->bitrate + 500) / 1000);
    } else if (s && s->hls_bitrate > 0 && s->hls_bitrate < INT_MAX - 500) {
        bstr_xappend_asprintf(ctx, &dst, " %d kbps", (s->hls_bitrate + 500) / 1000);
    }
    bstr_xappend0(ctx, &dst, ")");

    bool first = true;
    if (t->default_track)
        ADD_FLAG(ctx, dst, "default", first);
    if (t->forced_track)
        ADD_FLAG(ctx, dst, "forced", first);
    if (t->dependent_track)
        ADD_FLAG(ctx, dst, "dependent", first);
    if (t->visual_impaired_track)
        ADD_FLAG(ctx, dst, "visual-impaired", first);
    if (t->hearing_impaired_track)
        ADD_FLAG(ctx, dst, "hearing-impaired", first);
    if (t->is_external)
        ADD_FLAG(ctx, dst, "external", first);
    if (!first)
        bstr_xappend0(ctx, &dst, "]");

    return bstrto0(ctx, dst);
}

const char *mp_find_non_filename_media_title(MPContext *mpctx)
{
    const char *name = mpctx->opts->media_title;
    if (name && name[0])
        return name;
    if (mpctx->demuxer) {
        name = mp_tags_get_str(mpctx->demuxer->metadata, "service_name");
        if (name && name[0])
            return name;
        name = mp_tags_get_str(mpctx->demuxer->metadata, "title");
        if (name && name[0])
            return name;
        name = mp_tags_get_str(mpctx->demuxer->metadata, "icy-title");
        if (name && name[0])
            return name;
    }
    if (mpctx->playing && mpctx->playing->title)
        return mpctx->playing->title;
    return NULL;
}
