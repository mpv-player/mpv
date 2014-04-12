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

#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>

#include <libavutil/avstring.h>
#include <libavutil/common.h>

#include "config.h"
#include "talloc.h"
#include "client.h"
#include "common/msg.h"
#include "common/msg_control.h"
#include "command.h"
#include "osdep/timer.h"
#include "common/common.h"
#include "input/input.h"
#include "stream/stream.h"
#include "demux/demux.h"
#include "demux/stheader.h"
#include "stream/resolve/resolve.h"
#include "common/playlist.h"
#include "common/playlist_parser.h"
#include "sub/osd.h"
#include "sub/dec_sub.h"
#include "options/m_option.h"
#include "options/m_property.h"
#include "options/m_config.h"
#include "video/filter/vf.h"
#include "video/decode/vd.h"
#include "video/out/vo.h"
#include "video/csputils.h"
#include "audio/mixer.h"
#include "audio/audio_buffer.h"
#include "audio/out/ao.h"
#include "audio/filter/af.h"
#include "video/decode/dec_video.h"
#include "audio/decode/dec_audio.h"
#include "options/path.h"
#include "stream/tv.h"
#include "stream/pvr.h"
#if HAVE_DVBIN
#include "stream/dvbin.h"
#endif
#include "screenshot.h"
#if HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifndef __MINGW32__
#include <sys/wait.h>
#endif

#include "osdep/io.h"

#include "core.h"
#include "lua.h"

struct command_ctx {
    double last_seek_time;
    double last_seek_pts;

    struct cycle_counter *cycle_counters;
    int num_cycle_counters;

#define OVERLAY_MAX_ID 64
    void *overlay_map[OVERLAY_MAX_ID];
    struct sub_bitmaps external2;
};

static int edit_filters(struct MPContext *mpctx, enum stream_type mediatype,
                        const char *cmd, const char *arg);
static int set_filters(struct MPContext *mpctx, enum stream_type mediatype,
                       struct m_obj_settings *new_chain);

// Call before a seek, in order to allow revert_seek to undo the seek.
static void mark_seek(struct MPContext *mpctx)
{
    struct command_ctx *cmd = mpctx->command_ctx;
    double now = mp_time_sec();
    if (now > cmd->last_seek_time + 2.0 || cmd->last_seek_pts == MP_NOPTS_VALUE)
        cmd->last_seek_pts = get_current_time(mpctx);
    cmd->last_seek_time = now;
}

static char *format_bitrate(int rate)
{
    return talloc_asprintf(NULL, "%d kbps", rate * 8 / 1000);
}

static char *format_delay(double time)
{
    return talloc_asprintf(NULL, "%d ms", ROUND(time * 1000));
}

// Property-option bridge.
static int mp_property_generic_option(struct m_option *prop, int action,
                                      void *arg, MPContext *mpctx)
{
    char *optname = prop->priv;
    struct m_config_option *opt = m_config_get_co(mpctx->mconfig,
                                                  bstr0(optname));
    void *valptr = opt->data;

    switch (action) {
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = *(opt->opt);
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
        m_option_copy(opt->opt, arg, valptr);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        m_option_copy(opt->opt, valptr, arg);
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Playback speed (RW)
static int mp_property_playback_speed(m_option_t *prop, int action,
                                      void *arg, MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    double orig_speed = opts->playback_speed;
    switch (action) {
    case M_PROPERTY_SET: {
        opts->playback_speed = *(double *) arg;
        // Adjust time until next frame flip for nosound mode
        mpctx->time_frame *= orig_speed / opts->playback_speed;
        if (mpctx->d_audio)
            reinit_audio_chain(mpctx);
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_PRINT:
        *(char **)arg = talloc_asprintf(NULL, "x %6.2f", orig_speed);
        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(prop, action, arg, mpctx);
}

/// filename with path (RO)
static int mp_property_path(m_option_t *prop, int action, void *arg,
                            MPContext *mpctx)
{
    if (!mpctx->filename)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_strdup_ro(prop, action, arg, mpctx->filename);
}

static int mp_property_filename(m_option_t *prop, int action, void *arg,
                                MPContext *mpctx)
{
    if (!mpctx->filename)
        return M_PROPERTY_UNAVAILABLE;
    char *filename = talloc_strdup(NULL, mpctx->filename);
    if (mp_is_url(bstr0(filename)))
        mp_url_unescape_inplace(filename);
    char *f = (char *)mp_basename(filename);
    int r = m_property_strdup_ro(prop, action, arg, f[0] ? f : filename);
    talloc_free(filename);
    return r;
}

static int mp_property_media_title(m_option_t *prop, int action, void *arg,
                                   MPContext *mpctx)
{
    char *name = NULL;
    if (mpctx->resolve_result)
        name = mpctx->resolve_result->title;
    if (name && name[0])
        return m_property_strdup_ro(prop, action, arg, name);
    if (mpctx->master_demuxer) {
        name = demux_info_get(mpctx->master_demuxer, "title");
        if (name && name[0])
            return m_property_strdup_ro(prop, action, arg, name);
        struct stream *stream = mpctx->master_demuxer->stream;
        if (stream_control(stream, STREAM_CTRL_GET_DISC_NAME, &name) > 0
                && name) {
            int r = m_property_strdup_ro(prop, action, arg, name);
            talloc_free(name);
            return r;
        }
    }
    return mp_property_filename(prop, action, arg, mpctx);
}

static int mp_property_stream_path(m_option_t *prop, int action, void *arg,
                                   MPContext *mpctx)
{
    struct stream *stream = mpctx->stream;
    if (!stream || !stream->url)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_strdup_ro(prop, action, arg, stream->url);
}

static int mp_property_stream_capture(m_option_t *prop, int action,
                                      void *arg, MPContext *mpctx)
{
    if (!mpctx->stream)
        return M_PROPERTY_UNAVAILABLE;

    if (action == M_PROPERTY_SET) {
        char *filename = *(char **)arg;
        stream_set_capture_file(mpctx->stream, filename);
        // fall through to mp_property_generic_option
    }
    return mp_property_generic_option(prop, action, arg, mpctx);
}

/// Demuxer name (RO)
static int mp_property_demuxer(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->master_demuxer;
    if (!demuxer)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_strdup_ro(prop, action, arg, demuxer->desc->name);
}

/// Position in the stream (RW)
static int mp_property_stream_pos(m_option_t *prop, int action, void *arg,
                                  MPContext *mpctx)
{
    struct stream *stream = mpctx->stream;
    if (!stream)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_GET:
        *(int64_t *) arg = stream_tell(stream);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        stream_seek(stream, *(int64_t *) arg);
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Stream start offset (RO)
static int mp_property_stream_start(m_option_t *prop, int action,
                                    void *arg, MPContext *mpctx)
{
    struct stream *stream = mpctx->stream;
    if (!stream)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_int64_ro(prop, action, arg, stream->start_pos);
}

/// Stream end offset (RO)
static int mp_property_stream_end(m_option_t *prop, int action, void *arg,
                                  MPContext *mpctx)
{
    struct stream *stream = mpctx->stream;
    if (!stream)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_int64_ro(prop, action, arg, stream->end_pos);
}

/// Stream length (RO)
static int mp_property_stream_length(m_option_t *prop, int action,
                                     void *arg, MPContext *mpctx)
{
    struct stream *stream = mpctx->stream;
    if (!stream)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_int64_ro(prop, action, arg,
                               stream->end_pos - stream->start_pos);
}

// Does some magic to handle "<name>/full" as time formatted with milliseconds.
// Assumes prop is the type of the actual property.
static int property_time(m_option_t *prop, int action, void *arg, double time)
{
    switch (action) {
    case M_PROPERTY_GET:
        *(double *)arg = time;
        return M_PROPERTY_OK;
    case M_PROPERTY_KEY_ACTION: {
        struct m_property_action_arg *ka = arg;

        if (strcmp(ka->key, "full") != 0)
            return M_PROPERTY_UNKNOWN;

        switch (ka->action) {
        case M_PROPERTY_GET:
            *(double *)ka->arg = time;
            return M_PROPERTY_OK;
        case M_PROPERTY_PRINT:
            *(char **)ka->arg = mp_format_time(time, true);
            return M_PROPERTY_OK;
        case M_PROPERTY_GET_TYPE:
            *(struct m_option *)ka->arg = *prop;
            return M_PROPERTY_OK;
        }
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Current stream position in seconds (RO)
static int mp_property_stream_time_pos(m_option_t *prop, int action,
                                       void *arg, MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return M_PROPERTY_UNAVAILABLE;
    double pts = demuxer->stream_pts;
    if (pts == MP_NOPTS_VALUE)
        return M_PROPERTY_UNAVAILABLE;

    return property_time(prop, action, arg, pts);
}


/// Media length in seconds (RO)
static int mp_property_length(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{
    double len;

    if (!(int) (len = get_time_length(mpctx)))
        return M_PROPERTY_UNAVAILABLE;

    return property_time(prop, action, arg, len);
}

static int mp_property_avsync(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{
    if (!mpctx->d_audio || !mpctx->d_video)
        return M_PROPERTY_UNAVAILABLE;
    if (mpctx->last_av_difference == MP_NOPTS_VALUE)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_double_ro(prop, action, arg, mpctx->last_av_difference);
}

static int mp_property_total_avsync_change(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{
    if (!mpctx->d_audio || !mpctx->d_video)
        return M_PROPERTY_UNAVAILABLE;
    if (mpctx->total_avsync_change == MP_NOPTS_VALUE)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_double_ro(prop, action, arg, mpctx->total_avsync_change);
}


/// Late frames
static int mp_property_drop_frame_cnt(m_option_t *prop, int action, void *arg,
                                   MPContext *mpctx)
{
     if (!mpctx->d_video)
        return M_PROPERTY_UNAVAILABLE;

    return m_property_int_ro(prop, action, arg, mpctx->drop_frame_cnt);
}

/// Current position in percent (RW)
static int mp_property_percent_pos(m_option_t *prop, int action,
                                   void *arg, MPContext *mpctx)
{
    if (!mpctx->num_sources)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET: {
        double pos = *(double *)arg;
        queue_seek(mpctx, MPSEEK_FACTOR, pos / 100.0, 0, true);
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET: {
        double pos = get_current_pos_ratio(mpctx, false) * 100.0;
        if (pos < 0)
            return M_PROPERTY_UNAVAILABLE;
        *(double *)arg = pos;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_PRINT:
        *(char **)arg = talloc_asprintf(NULL, "%d", get_percent_pos(mpctx));
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_time_start(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{
    double start = get_start_time(mpctx);
    if (start < 0)
        return M_PROPERTY_UNAVAILABLE;
    return property_time(prop, action, arg, start);
}

/// Current position in seconds (RW)
static int mp_property_time_pos(m_option_t *prop, int action,
                                void *arg, MPContext *mpctx)
{
    if (!mpctx->num_sources)
        return M_PROPERTY_UNAVAILABLE;

    if (action == M_PROPERTY_SET) {
        queue_seek(mpctx, MPSEEK_ABSOLUTE, *(double *)arg, 0, true);
        return M_PROPERTY_OK;
    }
    return property_time(prop, action, arg, get_current_time(mpctx));
}

static bool time_remaining(MPContext *mpctx, double *remaining)
{
    double len = get_time_length(mpctx);
    double pos = get_current_time(mpctx);
    double start = get_start_time(mpctx);

    *remaining = len - (pos - start);

    return len > 0;
}

static int mp_property_remaining(m_option_t *prop, int action,
                                 void *arg, MPContext *mpctx)
{
    double remaining;
    if (!time_remaining(mpctx, &remaining))
        return M_PROPERTY_UNAVAILABLE;

    return property_time(prop, action, arg, remaining);
}

static int mp_property_playtime_remaining(m_option_t *prop, int action,
                                      void *arg, MPContext *mpctx)
{
    double remaining;
    if (!time_remaining(mpctx, &remaining))
        return M_PROPERTY_UNAVAILABLE;

    double speed = mpctx->opts->playback_speed;
    return property_time(prop, action, arg, remaining / speed);
}

/// Current BD/DVD title (RW)
static int mp_property_disc_title(m_option_t *prop, int action, void *arg,
                                 MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->master_demuxer;
    if (!demuxer || !demuxer->stream)
        return M_PROPERTY_UNAVAILABLE;
    struct stream *stream = demuxer->stream;
    unsigned int title = -1;
    switch (action) {
    case M_PROPERTY_GET:
        if (stream_control(stream, STREAM_CTRL_GET_CURRENT_TITLE, &title) <= 0)
            return M_PROPERTY_UNAVAILABLE;
        *(int*)arg = title;
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        title = *(int*)arg;
        if (stream_control(stream, STREAM_CTRL_SET_CURRENT_TITLE, &title) <= 0)
            return M_PROPERTY_NOT_IMPLEMENTED;
        return M_PROPERTY_OK;
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }
}

/// Current chapter (RW)
static int mp_property_chapter(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    int chapter = get_current_chapter(mpctx);
    if (chapter < -1)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_GET:
        *(int *) arg = chapter;
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT: {
        *(char **) arg = chapter_display_name(mpctx, chapter);
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_SWITCH:
    case M_PROPERTY_SET: ;
        mark_seek(mpctx);
        int step_all;
        if (action == M_PROPERTY_SWITCH) {
            struct m_property_switch_arg *sarg = arg;
            step_all = ROUND(sarg->inc);
            // Check threshold for relative backward seeks
            if (mpctx->opts->chapter_seek_threshold >= 0 && step_all < 0) {
                double current_chapter_start =
                    chapter_start_time(mpctx, chapter);
                // If we are far enough into a chapter, seek back to the
                // beginning of current chapter instead of previous one
                if (current_chapter_start != MP_NOPTS_VALUE &&
                    get_current_time(mpctx) - current_chapter_start >
                    mpctx->opts->chapter_seek_threshold)
                    step_all++;
            }
        } else // Absolute set
            step_all = *(int *)arg - chapter;
        chapter += step_all;
        if (chapter < -1)
            chapter = -1;
        if (chapter >= get_chapter_count(mpctx) && step_all > 0) {
            mpctx->stop_play = PT_NEXT_ENTRY;
        } else {
            mp_seek_chapter(mpctx, chapter);
        }
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int get_chapter_entry(int item, int action, void *arg, void *ctx)
{
    struct MPContext *mpctx = ctx;
    char *name = chapter_name(mpctx, item);
    double time = chapter_start_time(mpctx, item);
    struct m_sub_property props[] = {
        {"title",       SUB_PROP_STR(name)},
        {"time",        CONF_TYPE_TIME, {.time = time}},
        {0}
    };

    int r = m_property_read_sub(props, action, arg);
    talloc_free(name);
    return r;
}

static int mp_property_list_chapters(m_option_t *prop, int action, void *arg,
                                     MPContext *mpctx)
{
    int count = get_chapter_count(mpctx);
    if (action == M_PROPERTY_PRINT) {
        int cur = mpctx->num_sources ? get_current_chapter(mpctx) : -1;
        char *res = NULL;
        int n;

        if (count < 1) {
            res = talloc_asprintf_append(res, "No chapters.");
        }

        for (n = 0; n < count; n++) {
            char *name = chapter_display_name(mpctx, n);
            double t = chapter_start_time(mpctx, n);
            char* time = mp_format_time(t, false);
            res = talloc_asprintf_append(res, "%s", time);
            talloc_free(time);
            char *m1 = "> ", *m2 = " <";
            if (n != cur)
                m1 = m2 = "";
            res = talloc_asprintf_append(res, "   %s%s%s\n", m1, name, m2);
            talloc_free(name);
        }

        *(char **)arg = res;
        return M_PROPERTY_OK;
    }
    return m_property_read_list(action, arg, count, get_chapter_entry, mpctx);
}

static int mp_property_edition(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct demuxer *demuxer = mpctx->master_demuxer;
    if (!demuxer)
        return M_PROPERTY_UNAVAILABLE;
    if (demuxer->num_editions <= 0)
        return M_PROPERTY_UNAVAILABLE;

    int edition = demuxer->edition;

    switch (action) {
    case M_PROPERTY_GET:
        *(int *)arg = edition;
        return M_PROPERTY_OK;
    case M_PROPERTY_SET: {
        edition = *(int *)arg;
        if (edition != demuxer->edition) {
            opts->edition_id = edition;
            mpctx->stop_play = PT_RESTART;
        }
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET_TYPE: {
        struct m_option opt = {
            .type = CONF_TYPE_INT,
            .flags = CONF_RANGE,
            .min = 0,
            .max = demuxer->num_editions - 1,
        };
        *(struct m_option *)arg = opt;
        return M_PROPERTY_OK;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}


static int get_edition_entry(int item, int action, void *arg, void *ctx)
{
    struct MPContext *mpctx = ctx;

    struct demuxer *demuxer = mpctx->master_demuxer;
    struct demux_edition *ed = &demuxer->editions[item];

    char *title = mp_tags_get_str(ed->metadata, "title");

    struct m_sub_property props[] = {
        {"id",          SUB_PROP_INT(item)},
        {"title",       SUB_PROP_STR(title),
                        .unavailable = !title},
        {"default",     SUB_PROP_FLAG(ed->default_edition)},
        {0}
    };

    return m_property_read_sub(props, action, arg);
}

static int property_list_editions(m_option_t *prop, int action, void *arg,
                                  MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->master_demuxer;
    if (!demuxer)
        return M_PROPERTY_UNAVAILABLE;

    if (action == M_PROPERTY_PRINT) {
        char *res = NULL;

        struct demux_edition *editions = demuxer->editions;
        int num_editions = demuxer->num_editions;
        int current = demuxer->edition;

        if (!num_editions)
            res = talloc_asprintf_append(res, "No editions.");

        for (int n = 0; n < num_editions; n++) {
            struct demux_edition *ed = &editions[n];

            if (n == current)
                res = talloc_asprintf_append(res, "> ");
            res = talloc_asprintf_append(res, "%d: ", n);
            char *title = mp_tags_get_str(ed->metadata, "title");
            if (!title)
                title = "unnamed";
            res = talloc_asprintf_append(res, "'%s' ", title);
            if (n == current)
                res = talloc_asprintf_append(res, "<");
            res = talloc_asprintf_append(res, "\n");
        }

        *(char **)arg = res;
        return M_PROPERTY_OK;
    }
    return m_property_read_list(action, arg, demuxer->num_editions,
                                get_edition_entry, mpctx);
}

static struct mp_resolve_src *find_source(struct mp_resolve_result *res,
                                          char *encid, char *url)
{
    if (res->num_srcs == 0)
        return NULL;

    int src = 0;
    for (int n = 0; n < res->num_srcs; n++) {
        char *s_url = res->srcs[n]->url;
        char *s_encid = res->srcs[n]->encid;
        if (url && s_url && strcmp(url, s_url) == 0) {
            src = n;
            break;
        }
        // Prefer source URL if possible; so continue in case encid isn't unique
        if (encid && s_encid && strcmp(encid, s_encid) == 0)
            src = n;
    }
    return res->srcs[src];
}

static int mp_property_quvi_format(m_option_t *prop, int action, void *arg,
                                   MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct mp_resolve_result *res = mpctx->resolve_result;
    if (!res || !res->num_srcs)
        return M_PROPERTY_UNAVAILABLE;

    struct mp_resolve_src *cur = find_source(res, opts->quvi_format, res->url);
    if (!cur)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_GET:
        *(char **)arg = talloc_strdup(NULL, cur->encid);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET: {
        mpctx->stop_play = PT_RESTART;
        // Make it restart at the same position. This will have disastrous
        // consequences if the stream is not arbitrarily seekable, but whatever.
        m_config_backup_opt(mpctx->mconfig, "start");
        opts->play_start = (struct m_rel_time) {
            .type = REL_TIME_ABSOLUTE,
            .pos = get_current_time(mpctx),
        };
        break;
    }
    case M_PROPERTY_SWITCH: {
        struct m_property_switch_arg *sarg = arg;
        int pos = 0;
        for (int n = 0; n < res->num_srcs; n++) {
            if (res->srcs[n] == cur) {
                pos = n;
                break;
            }
        }
        pos += sarg->inc;
        if (pos < 0 || pos >= res->num_srcs) {
            if (sarg->wrap) {
                pos = (res->num_srcs + pos) % res->num_srcs;
            } else {
                pos = av_clip(pos, 0, res->num_srcs);
            }
        }
        char *fmt = res->srcs[pos]->encid;
        return mp_property_quvi_format(prop, M_PROPERTY_SET, &fmt, mpctx);
    }
    }
    return mp_property_generic_option(prop, action, arg, mpctx);
}

/// Number of titles in BD/DVD
static int mp_property_disc_titles(m_option_t *prop, int action, void *arg,
                                   MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->master_demuxer;
    unsigned int num_titles;
    if (!demuxer || stream_control(demuxer->stream, STREAM_CTRL_GET_NUM_TITLES,
                                   &num_titles) < 1)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(prop, action, arg, num_titles);
}

/// Number of chapters in file
static int mp_property_chapters(m_option_t *prop, int action, void *arg,
                                MPContext *mpctx)
{
    if (!mpctx->num_sources)
        return M_PROPERTY_UNAVAILABLE;
    int count = get_chapter_count(mpctx);
    return m_property_int_ro(prop, action, arg, count);
}

static int mp_property_editions(m_option_t *prop, int action, void *arg,
                                MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->master_demuxer;
    if (!demuxer)
        return M_PROPERTY_UNAVAILABLE;
    if (demuxer->num_editions <= 0)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(prop, action, arg, demuxer->num_editions);
}

/// Current dvd angle (RW)
static int mp_property_angle(m_option_t *prop, int action, void *arg,
                             MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->master_demuxer;
    int angle = -1;
    int angles;

    if (demuxer)
        angle = demuxer_get_current_angle(demuxer);
    if (angle < 0)
        return M_PROPERTY_UNAVAILABLE;
    angles = demuxer_angles_count(demuxer);
    if (angles <= 1)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_GET:
        *(int *) arg = angle;
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT: {
        *(char **) arg = talloc_asprintf(NULL, "%d/%d", angle, angles);
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_SET:
        angle = demuxer_set_angle(demuxer, *(int *)arg);
        if (angle >= 0) {
            if (mpctx->d_video)
                video_reset_decoding(mpctx->d_video);

            if (mpctx->d_audio)
                audio_reset_decoding(mpctx->d_audio);
        }
        return M_PROPERTY_OK;
    case M_PROPERTY_GET_TYPE: {
        struct m_option opt = {
            .type = CONF_TYPE_INT,
            .flags = CONF_RANGE,
            .min = 1,
            .max = angles,
        };
        *(struct m_option *)arg = opt;
        return M_PROPERTY_OK;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int get_tag_entry(int item, int action, void *arg, void *ctx)
{
    struct mp_tags *tags = ctx;

    struct m_sub_property props[] = {
        {"key",     SUB_PROP_STR(tags->keys[item])},
        {"value",   SUB_PROP_STR(tags->values[item])},
        {0}
    };

    return m_property_read_sub(props, action, arg);
}

static int tag_property(m_option_t *prop, int action, void *arg,
                        struct mp_tags *tags)
{
    switch (action) {
    case M_PROPERTY_GET: {
        mpv_node_list *list = talloc_zero(NULL, mpv_node_list);
        mpv_node node = {
            .format = MPV_FORMAT_NODE_MAP,
            .u.list = list,
        };
        list->num = tags->num_keys;
        list->values = talloc_array(list, mpv_node, list->num);
        list->keys = talloc_array(list, char*, list->num);
        for (int n = 0; n < tags->num_keys; n++) {
            list->keys[n] = talloc_strdup(list, tags->keys[n]);
            list->values[n] = (struct mpv_node){
                .format = MPV_FORMAT_STRING,
                .u.string = talloc_strdup(list, tags->values[n]),
            };
        }
        *(mpv_node*)arg = node;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET_TYPE: {
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_NODE};
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_PRINT: {
        char *res = NULL;
        for (int n = 0; n < tags->num_keys; n++) {
            res = talloc_asprintf_append_buffer(res, "%s: %s\n",
                                                tags->keys[n], tags->values[n]);
        }
        *(char **)arg = res;
        return res ? M_PROPERTY_OK : M_PROPERTY_UNAVAILABLE;
    }
    case M_PROPERTY_KEY_ACTION: {
        struct m_property_action_arg *ka = arg;
        bstr key;
        char *rem;
        m_property_split_path(ka->key, &key, &rem);
        if (bstr_equals0(key, "list")) {
            struct m_property_action_arg nka = *ka;
            nka.key = rem;
            return m_property_read_list(action, &nka, tags->num_keys,
                                        get_tag_entry, tags);
        }
        // Direct access without this prefix is allowed for compatibility.
        bstr k = bstr0(ka->key);
        bstr_eatstart0(&k, "by-key/");
        char *meta = mp_tags_get_bstr(tags, k);
        if (!meta)
            return M_PROPERTY_UNKNOWN;
        switch (ka->action) {
        case M_PROPERTY_GET:
            *(char **)ka->arg = talloc_strdup(NULL, meta);
            return M_PROPERTY_OK;
        case M_PROPERTY_GET_TYPE:
            *(struct m_option *)ka->arg = (struct m_option){
                .type = CONF_TYPE_STRING,
            };
            return M_PROPERTY_OK;
        }
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Demuxer meta data
static int mp_property_metadata(m_option_t *prop, int action, void *arg,
                                MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->master_demuxer;
    if (!demuxer)
        return M_PROPERTY_UNAVAILABLE;

    return tag_property(prop, action, arg, demuxer->metadata);
}

static int mp_property_chapter_metadata(m_option_t *prop, int action, void *arg,
                                        MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->master_demuxer;
    int chapter = get_current_chapter(mpctx);
    if (!demuxer || chapter < 0 || chapter >= demuxer->num_chapters)
        return M_PROPERTY_UNAVAILABLE;

    return tag_property(prop, action, arg, demuxer->chapters[chapter].metadata);
}

static int mp_property_pause(m_option_t *prop, int action, void *arg,
                             void *ctx)
{
    MPContext *mpctx = ctx;

    if (action == M_PROPERTY_SET) {
        if (*(int *)arg) {
            pause_player(mpctx, PAUSE_BY_COMMAND);
        } else {
            unpause_player(mpctx, PAUSE_BY_COMMAND);
        }
        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(prop, action, arg, ctx);
}

static int mp_property_cache(m_option_t *prop, int action, void *arg,
                             void *ctx)
{
    MPContext *mpctx = ctx;
    int cache = mp_get_cache_percent(mpctx);
    if (cache < 0)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(prop, action, arg, cache);
}

static int mp_property_cache_size(m_option_t *prop, int action, void *arg,
                                  void *ctx)
{
    MPContext *mpctx = ctx;
    if (!mpctx->stream)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_GET: {
        int64_t size = -1;
        stream_control(mpctx->stream, STREAM_CTRL_GET_CACHE_SIZE, &size);
        if (size <= 0)
            break;
        *(int *)arg = size / 1024;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_SET: {
        int64_t size = *(int *)arg * 1024LL;
        int r = stream_control(mpctx->stream, STREAM_CTRL_SET_CACHE_SIZE, &size);
        if (r == STREAM_UNSUPPORTED)
            break;
        if (r == STREAM_OK)
            return M_PROPERTY_OK;
        return M_PROPERTY_ERROR;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_paused_for_cache(m_option_t *prop, int action, void *arg,
                            MPContext *mpctx)
{
    return m_property_int_ro(prop, action, arg, mpctx->paused_for_cache);
}

static int mp_property_clock(m_option_t *prop, int action, void *arg,
                             MPContext *mpctx)
{
    char outstr[6];
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);

    if ((tmp != NULL) && (strftime(outstr, sizeof(outstr), "%H:%M", tmp) == 5))
        return m_property_strdup_ro(prop, action, arg, outstr);
    return M_PROPERTY_UNAVAILABLE;
}

static int mp_property_seekable(m_option_t *prop, int action, void *arg,
                            MPContext *mpctx)
{
    if (!mpctx->demuxer)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(prop, action, arg, !!mpctx->demuxer->seekable);
}

/// Volume (RW)
static int mp_property_volume(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{
    switch (action) {
    case M_PROPERTY_GET:
        mixer_getbothvolume(mpctx->mixer, arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT: {
        float val;
        mixer_getbothvolume(mpctx->mixer, &val);
        *(char **)arg = talloc_asprintf(NULL, "%i", (int)val);
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_SET:
        if (!mixer_audio_initialized(mpctx->mixer))
            return M_PROPERTY_ERROR;
        mixer_setvolume(mpctx->mixer, *(float *) arg, *(float *) arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_SWITCH: {
        if (!mixer_audio_initialized(mpctx->mixer))
            return M_PROPERTY_ERROR;
        struct m_property_switch_arg *sarg = arg;
        mixer_addvolume(mpctx->mixer, mpctx->opts->volstep * sarg->inc);
        return M_PROPERTY_OK;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Mute (RW)
static int mp_property_mute(m_option_t *prop, int action, void *arg,
                            MPContext *mpctx)
{
    switch (action) {
    case M_PROPERTY_SET:
        if (!mixer_audio_initialized(mpctx->mixer))
            return M_PROPERTY_ERROR;
        mixer_setmute(mpctx->mixer, *(int *) arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
        *(int *)arg =  mixer_getmute(mpctx->mixer);
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_volrestore(m_option_t *prop, int action,
                                   void *arg, MPContext *mpctx)
{
    switch (action) {
    case M_PROPERTY_GET: {
        char *s = mixer_get_volume_restore_data(mpctx->mixer);
        *(char **)arg = s;
        return s ? M_PROPERTY_OK : M_PROPERTY_UNAVAILABLE;
    }
    case M_PROPERTY_SET:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }
    return mp_property_generic_option(prop, action, arg, mpctx);
}

/// Audio delay (RW)
static int mp_property_audio_delay(m_option_t *prop, int action,
                                   void *arg, MPContext *mpctx)
{
    if (!(mpctx->d_audio && mpctx->d_video))
        return M_PROPERTY_UNAVAILABLE;
    float delay = mpctx->opts->audio_delay;
    switch (action) {
    case M_PROPERTY_PRINT:
        *(char **)arg = format_delay(delay);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        mpctx->audio_delay = mpctx->opts->audio_delay = *(float *)arg;
        mpctx->delay += mpctx->audio_delay - delay;
        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(prop, action, arg, mpctx);
}

/// Audio codec tag (RO)
static int mp_property_audio_format(m_option_t *prop, int action,
                                    void *arg, MPContext *mpctx)
{
    const char *c = mpctx->d_audio ? mpctx->d_audio->header->codec : NULL;
    return m_property_strdup_ro(prop, action, arg, c);
}

/// Audio codec name (RO)
static int mp_property_audio_codec(m_option_t *prop, int action,
                                   void *arg, MPContext *mpctx)
{
    const char *c = mpctx->d_audio ? mpctx->d_audio->decoder_desc : NULL;
    return m_property_strdup_ro(prop, action, arg, c);
}

/// Audio bitrate (RO)
static int mp_property_audio_bitrate(m_option_t *prop, int action,
                                     void *arg, MPContext *mpctx)
{
    if (!mpctx->d_audio)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_PRINT:
        *(char **)arg = format_bitrate(mpctx->d_audio->i_bps);
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
        *(int *)arg = mpctx->d_audio->i_bps;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Samplerate (RO)
static int mp_property_samplerate(m_option_t *prop, int action, void *arg,
                                  MPContext *mpctx)
{
    struct mp_audio fmt = {0};
    if (mpctx->d_audio)
        mp_audio_buffer_get_format(mpctx->d_audio->decode_buffer, &fmt);
    if (!fmt.rate)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_PRINT:
        *(char **)arg = talloc_asprintf(NULL, "%d kHz", fmt.rate / 1000);
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
        *(int *)arg = fmt.rate;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Number of channels (RO)
static int mp_property_channels(m_option_t *prop, int action, void *arg,
                                MPContext *mpctx)
{
    struct mp_audio fmt = {0};
    if (mpctx->d_audio)
        mp_audio_buffer_get_format(mpctx->d_audio->decode_buffer, &fmt);
    if (!fmt.channels.num)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_PRINT:
        *(char **) arg = mp_chmap_to_str(&fmt.channels);
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
        *(int *)arg = fmt.channels.num;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Balance (RW)
static int mp_property_balance(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    float bal;

    switch (action) {
    case M_PROPERTY_GET:
        mixer_getbalance(mpctx->mixer, arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT: {
        char **str = arg;
        mixer_getbalance(mpctx->mixer, &bal);
        if (bal == 0.f)
            *str = talloc_strdup(NULL, "center");
        else if (bal == -1.f)
            *str = talloc_strdup(NULL, "left only");
        else if (bal == 1.f)
            *str = talloc_strdup(NULL, "right only");
        else {
            unsigned right = (bal + 1.f) / 2.f * 100.f;
            *str = talloc_asprintf(NULL, "left %d%%, right %d%%",
                                   100 - right, right);
        }
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_SET:
        mixer_setbalance(mpctx->mixer, *(float *)arg);
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static struct track* track_next(struct MPContext *mpctx, int order,
                                enum stream_type type, int direction,
                                struct track *track)
{
    assert(direction == -1 || direction == +1);
    struct track *prev = NULL, *next = NULL;
    bool seen = track == NULL;
    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *cur = mpctx->tracks[n];
        // One track can be selected only one time - pretend already selected
        // tracks don't exist.
        for (int r = 0; r < NUM_PTRACKS; r++) {
            if (r != order && mpctx->current_track[r][type] == cur)
                cur = NULL;
        }
        if (!cur)
            continue;
        if (cur->type == type) {
            if (cur == track) {
                seen = true;
            } else {
                if (seen && !next) {
                    next = cur;
                }
                if (!seen || !track) {
                    prev = cur;
                }
            }
        }
    }
    return direction > 0 ? next : prev;
}

static int property_switch_track(m_option_t *prop, int action, void *arg,
                                 MPContext *mpctx, int order,
                                 enum stream_type type)
{
    if (!mpctx->num_sources)
        return M_PROPERTY_UNAVAILABLE;
    struct track *track = mpctx->current_track[order][type];

    switch (action) {
    case M_PROPERTY_GET:
        *(int *) arg = track ? track->user_tid : -2;
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
        if (!track)
            *(char **) arg = talloc_strdup(NULL, "no");
        else {
            char *lang = track->lang;
            if (!lang)
                lang = "unknown";

            if (track->title)
                *(char **)arg = talloc_asprintf(NULL, "(%d) %s (\"%s\")",
                                           track->user_tid, lang, track->title);
            else
                *(char **)arg = talloc_asprintf(NULL, "(%d) %s",
                                                track->user_tid, lang);
        }
        return M_PROPERTY_OK;

    case M_PROPERTY_SWITCH: {
        struct m_property_switch_arg *sarg = arg;
        mp_switch_track_n(mpctx, order, type,
            track_next(mpctx, order, type, sarg->inc >= 0 ? +1 : -1, track));
        mp_mark_user_track_selection(mpctx, order, type);
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_SET:
        track = mp_track_by_tid(mpctx, type, *(int *)arg);
        mp_switch_track_n(mpctx, order, type, track);
        mp_mark_user_track_selection(mpctx, order, type);
        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(prop, action, arg, mpctx);
}

static int get_track_entry(int item, int action, void *arg, void *ctx)
{
    struct MPContext *mpctx = ctx;
    struct track *track = mpctx->tracks[item];

    const char *codec = track->stream ? track->stream->codec : NULL;

    struct m_sub_property props[] = {
        {"id",          SUB_PROP_INT(track->user_tid)},
        {"type",        SUB_PROP_STR(stream_type_name(track->type)),
                        .unavailable = !stream_type_name(track->type)},
        {"src-id",      SUB_PROP_INT(track->demuxer_id),
                        .unavailable = track->demuxer_id == -1},
        {"title",       SUB_PROP_STR(track->title),
                        .unavailable = !track->title},
        {"lang",        SUB_PROP_STR(track->lang),
                        .unavailable = !track->lang},
        {"albumart",    SUB_PROP_FLAG(track->attached_picture)},
        {"default",     SUB_PROP_FLAG(track->default_track)},
        {"external",    SUB_PROP_FLAG(track->is_external)},
        {"selected",    SUB_PROP_FLAG(track->selected)},
        {"external-filename", SUB_PROP_STR(track->external_filename),
                        .unavailable = !track->external_filename},
        {"codec",       SUB_PROP_STR(codec),
                        .unavailable = !codec},
        {0}
    };

    return m_property_read_sub(props, action, arg);
}

static const char *track_type_name(enum stream_type t)
{
    switch (t) {
    case STREAM_VIDEO: return "Video";
    case STREAM_AUDIO: return "Audio";
    case STREAM_SUB: return "Sub";
    }
    return NULL;
}

static int property_list_tracks(m_option_t *prop, int action, void *arg,
                                MPContext *mpctx)
{
    if (action == M_PROPERTY_PRINT) {
        char *res = NULL;

        for (int type = 0; type < STREAM_TYPE_COUNT; type++) {
            for (int n = 0; n < mpctx->num_tracks; n++) {
                struct track *track = mpctx->tracks[n];
                if (track->type != type)
                    continue;

                res = talloc_asprintf_append(res, "%s: ",
                                             track_type_name(track->type));
                if (track->selected)
                    res = talloc_asprintf_append(res, "> ");
                res = talloc_asprintf_append(res, "(%d) ", track->user_tid);
                if (track->title)
                    res = talloc_asprintf_append(res, "'%s' ", track->title);
                if (track->lang)
                    res = talloc_asprintf_append(res, "(%s) ", track->lang);
                if (track->is_external)
                    res = talloc_asprintf_append(res, "(external) ");
                if (track->selected)
                    res = talloc_asprintf_append(res, "<");
                res = talloc_asprintf_append(res, "\n");
            }

            res = talloc_asprintf_append(res, "\n");
        }

        struct demuxer *demuxer = mpctx->master_demuxer;
        if (demuxer && demuxer->num_editions > 1)
            res = talloc_asprintf_append(res, "\nEdition: %d of %d\n",
                                        demuxer->edition + 1,
                                        demuxer->num_editions);

        *(char **)arg = res;
        return M_PROPERTY_OK;
    }
    return m_property_read_list(action, arg, mpctx->num_tracks,
                                get_track_entry, mpctx);
}

/// Selected audio id (RW)
static int mp_property_audio(m_option_t *prop, int action, void *arg,
                             MPContext *mpctx)
{
    return property_switch_track(prop, action, arg, mpctx, 0, STREAM_AUDIO);
}

/// Selected video id (RW)
static int mp_property_video(m_option_t *prop, int action, void *arg,
                             MPContext *mpctx)
{
    return property_switch_track(prop, action, arg, mpctx, 0, STREAM_VIDEO);
}

static struct track *find_track_by_demuxer_id(MPContext *mpctx,
                                              enum stream_type type,
                                              int demuxer_id)
{
    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *track = mpctx->tracks[n];
        if (track->type == type && track->demuxer_id == demuxer_id)
            return track;
    }
    return NULL;
}

static int mp_property_program(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    demux_program_t prog;

    struct demuxer *demuxer = mpctx->master_demuxer;
    if (!demuxer)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SWITCH:
    case M_PROPERTY_SET:
        if (action == M_PROPERTY_SET && arg)
            prog.progid = *((int *) arg);
        else
            prog.progid = -1;
        if (demux_control(demuxer, DEMUXER_CTRL_IDENTIFY_PROGRAM, &prog) ==
            DEMUXER_CTRL_NOTIMPL)
            return M_PROPERTY_ERROR;

        if (prog.aid < 0 && prog.vid < 0) {
            MP_ERR(mpctx, "Selected program contains no audio or video streams!\n");
            return M_PROPERTY_ERROR;
        }
        mp_switch_track(mpctx, STREAM_VIDEO,
                find_track_by_demuxer_id(mpctx, STREAM_VIDEO, prog.vid));
        mp_switch_track(mpctx, STREAM_AUDIO,
                find_track_by_demuxer_id(mpctx, STREAM_AUDIO, prog.aid));
        mp_switch_track(mpctx, STREAM_SUB,
                find_track_by_demuxer_id(mpctx, STREAM_VIDEO, prog.sid));
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}


/// Fullscreen state (RW)
static int mp_property_fullscreen(m_option_t *prop,
                                  int action,
                                  void *arg,
                                  MPContext *mpctx)
{
    if (!mpctx->video_out)
        return M_PROPERTY_UNAVAILABLE;
    struct mp_vo_opts *opts = mpctx->video_out->opts;

    if (action == M_PROPERTY_SET) {
        int val = *(int *)arg;
        opts->fullscreen = val;
        if (mpctx->video_out->config_ok)
            vo_control(mpctx->video_out, VOCTRL_FULLSCREEN, 0);
        return opts->fullscreen == val ? M_PROPERTY_OK : M_PROPERTY_ERROR;
    }
    return mp_property_generic_option(prop, action, arg, mpctx);
}

#define VF_DEINTERLACE_LABEL "deinterlace"

static const char *deint_filters[] = {
    "yadif",
#if HAVE_VAAPI_VPP
    "vavpp",
#endif
    NULL
};

static int probe_deint_filters(struct MPContext *mpctx, const char *cmd)
{
    for (int n = 0; deint_filters[n]; n++) {
        char filter[80];
        // add a label so that removing the filter is easier
        snprintf(filter, sizeof(filter), "@%s:%s", VF_DEINTERLACE_LABEL,
                 deint_filters[n]);
        if (edit_filters(mpctx, STREAM_VIDEO, cmd, filter) >= 0)
            return 0;
    }
    return -1;
}

static int get_deinterlacing(struct MPContext *mpctx)
{
    struct dec_video *vd = mpctx->d_video;
    int enabled = 0;
    if (video_vf_vo_control(vd, VFCTRL_GET_DEINTERLACE, &enabled) != CONTROL_OK)
        enabled = -1;
    if (enabled < 0) {
        // vf_lavfi doesn't support VFCTRL_GET_DEINTERLACE
        if (vf_find_by_label(vd->vfilter, VF_DEINTERLACE_LABEL))
            enabled = 1;
    }
    return enabled;
}

static void set_deinterlacing(struct MPContext *mpctx, bool enable)
{
    struct dec_video *vd = mpctx->d_video;
    if (vf_find_by_label(vd->vfilter, VF_DEINTERLACE_LABEL)) {
        if (!enable)
            edit_filters(mpctx, STREAM_VIDEO, "del", "@" VF_DEINTERLACE_LABEL);
    } else {
        if ((get_deinterlacing(mpctx) > 0) != enable) {
            int arg = enable;
            if (video_vf_vo_control(vd, VFCTRL_SET_DEINTERLACE, &arg) != CONTROL_OK)
                probe_deint_filters(mpctx, "pre");
        }
    }
    mpctx->opts->deinterlace = get_deinterlacing(mpctx) > 0;
}

static int mp_property_deinterlace(m_option_t *prop, int action,
                                   void *arg, MPContext *mpctx)
{
    if (!mpctx->d_video || !mpctx->d_video->vfilter)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_GET:
        *(int *)arg = get_deinterlacing(mpctx) > 0;
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        set_deinterlacing(mpctx, *(int *)arg);
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

// Generic option + requires hard refresh to make changes take effect.
static int video_refresh_property_helper(m_option_t *prop, int action,
                                         void *arg, MPContext *mpctx)
{
    int r = mp_property_generic_option(prop, action, arg, mpctx);
    if (action == M_PROPERTY_SET) {
        if (mpctx->d_video) {
            reinit_video_filters(mpctx);
            mp_force_video_refresh(mpctx);
        }
    }
    return r;
}

static void append_csp(char **ptr, const char *name, const char *const *names,
                       int value)
{
    const char *cspname = names[value];
    if (name[0] == '*') {
        name++;
    } else if (value == 0) {
        cspname = "unknown";
    }
    *ptr = talloc_asprintf_append(*ptr, "%s: %s\n", name, cspname);
}

static int mp_property_colormatrix(m_option_t *prop, int action, void *arg,
                                   MPContext *mpctx)
{
    if (action != M_PROPERTY_PRINT)
        return video_refresh_property_helper(prop, action, arg, mpctx);

    struct MPOpts *opts = mpctx->opts;

    struct mp_image_params vo_csp = {0};
    if (mpctx->video_out)
        vo_control(mpctx->video_out, VOCTRL_GET_COLORSPACE, &vo_csp);

    struct mp_image_params vd_csp = {0};
    if (mpctx->d_video)
        vd_csp = mpctx->d_video->decoder_output;

    char *res = talloc_strdup(NULL, "");
    append_csp(&res, "*Requested", mp_csp_names, opts->requested_colorspace);
    append_csp(&res, "Video decoder", mp_csp_names, vd_csp.colorspace);
    append_csp(&res, "Video output", mp_csp_names, vo_csp.colorspace);
    *(char **)arg = res;
    return M_PROPERTY_OK;
}

static int mp_property_colormatrix_input_range(m_option_t *prop, int action,
                                               void *arg, MPContext *mpctx)
{
    if (action != M_PROPERTY_PRINT)
        return video_refresh_property_helper(prop, action, arg, mpctx);

    struct MPOpts *opts = mpctx->opts;

    struct mp_image_params vo_csp = {0};
    if (mpctx->video_out)
        vo_control(mpctx->video_out, VOCTRL_GET_COLORSPACE, &vo_csp);

    struct mp_image_params vd_csp = {0};
    if (mpctx->d_video)
        vd_csp = mpctx->d_video->decoder_output;

    char *res = talloc_strdup(NULL, "");
    append_csp(&res, "*Requested", mp_csp_levels_names,
               opts->requested_input_range);
    append_csp(&res, "Video decoder", mp_csp_levels_names, vd_csp.colorlevels);
    append_csp(&res, "Video output", mp_csp_levels_names, vo_csp.colorlevels);
    *(char **)arg = res;
    return M_PROPERTY_OK;
}

static int mp_property_colormatrix_output_range(m_option_t *prop, int action,
                                                void *arg, MPContext *mpctx)
{
    if (action != M_PROPERTY_PRINT)
        return video_refresh_property_helper(prop, action, arg, mpctx);

    struct MPOpts *opts = mpctx->opts;

    struct mp_image_params actual = {0};
    if (mpctx->video_out)
        vo_control(mpctx->video_out, VOCTRL_GET_COLORSPACE, &actual);

    char *res = talloc_strdup(NULL, "");
    append_csp(&res, "*Requested", mp_csp_levels_names,
               opts->requested_output_range);
    append_csp(&res, "Video output", mp_csp_levels_names, actual.outputlevels);
    *(char **)arg = res;
    return M_PROPERTY_OK;
}

// Update options which are managed through VOCTRL_GET/SET_PANSCAN.
static int panscan_property_helper(m_option_t *prop, int action, void *arg,
                                   MPContext *mpctx)
{

    if (!mpctx->video_out
        || vo_control(mpctx->video_out, VOCTRL_GET_PANSCAN, NULL) != VO_TRUE)
        return M_PROPERTY_UNAVAILABLE;

    int r = mp_property_generic_option(prop, action, arg, mpctx);
    if (action == M_PROPERTY_SET)
        vo_control(mpctx->video_out, VOCTRL_SET_PANSCAN, NULL);
    return r;
}

/// Helper to set vo flags.
/** \ingroup PropertyImplHelper
 */
static int mp_property_vo_flag(m_option_t *prop, int action, void *arg,
                               int vo_ctrl, int *vo_var, MPContext *mpctx)
{

    if (!mpctx->video_out)
        return M_PROPERTY_UNAVAILABLE;

    if (action == M_PROPERTY_SET) {
        if (*vo_var == !!*(int *) arg)
            return M_PROPERTY_OK;
        if (mpctx->video_out->config_ok)
            vo_control(mpctx->video_out, vo_ctrl, 0);
        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(prop, action, arg, mpctx);
}

/// Window always on top (RW)
static int mp_property_ontop(m_option_t *prop, int action, void *arg,
                             MPContext *mpctx)
{
    return mp_property_vo_flag(prop, action, arg, VOCTRL_ONTOP,
                               &mpctx->opts->vo.ontop, mpctx);
}

/// Show window borders (RW)
static int mp_property_border(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{
    return mp_property_vo_flag(prop, action, arg, VOCTRL_BORDER,
                               &mpctx->opts->vo.border, mpctx);
}

static int mp_property_framedrop(m_option_t *prop, int action,
                                 void *arg, MPContext *mpctx)
{
    if (!mpctx->d_video)
        return M_PROPERTY_UNAVAILABLE;

    return mp_property_generic_option(prop, action, arg, mpctx);
}

static int mp_property_video_color(m_option_t *prop, int action, void *arg,
                                   MPContext *mpctx)
{
    if (!mpctx->d_video)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET: {
        if (video_set_colors(mpctx->d_video, prop->name, *(int *) arg) <= 0)
            return M_PROPERTY_UNAVAILABLE;
        break;
    }
    case M_PROPERTY_GET:
        if (video_get_colors(mpctx->d_video, prop->name, (int *)arg) <= 0)
            return M_PROPERTY_UNAVAILABLE;
        // Write new value to option variable
        mp_property_generic_option(prop, M_PROPERTY_SET, arg, mpctx);
        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(prop, action, arg, mpctx);
}

/// Video codec tag (RO)
static int mp_property_video_format(m_option_t *prop, int action,
                                    void *arg, MPContext *mpctx)
{
    const char *c = mpctx->d_video ? mpctx->d_video->header->codec : NULL;
    return m_property_strdup_ro(prop, action, arg, c);
}

/// Video codec name (RO)
static int mp_property_video_codec(m_option_t *prop, int action,
                                   void *arg, MPContext *mpctx)
{
    const char *c = mpctx->d_video ? mpctx->d_video->decoder_desc : NULL;
    return m_property_strdup_ro(prop, action, arg, c);
}


/// Video bitrate (RO)
static int mp_property_video_bitrate(m_option_t *prop, int action,
                                     void *arg, MPContext *mpctx)
{
    if (!mpctx->d_video)
        return M_PROPERTY_UNAVAILABLE;
    if (action == M_PROPERTY_PRINT) {
        *(char **)arg = format_bitrate(mpctx->d_video->i_bps);
        return M_PROPERTY_OK;
    }
    return m_property_int_ro(prop, action, arg, mpctx->d_video->i_bps);
}

static int property_imgparams(struct mp_image_params p, int action, void *arg)
{
    if (!p.imgfmt)
        return M_PROPERTY_UNAVAILABLE;

    double dar = p.d_w / (double)p.d_h;
    double sar = p.w / (double)p.h;

    struct m_sub_property props[] = {
        {"pixelformat",     SUB_PROP_STR(mp_imgfmt_to_name(p.imgfmt))},
        {"w",               SUB_PROP_INT(p.w)},
        {"h",               SUB_PROP_INT(p.h)},
        {"dw",              SUB_PROP_INT(p.d_w)},
        {"dh",              SUB_PROP_INT(p.d_h)},
        {"aspect",          SUB_PROP_FLOAT(dar)},
        {"par",             SUB_PROP_FLOAT(dar / sar)},
        {"colormatrix",     SUB_PROP_STR(mp_csp_names[p.colorspace])},
        {"colorlevels",     SUB_PROP_STR(mp_csp_levels_names[p.colorlevels])},
        {"chroma-location", SUB_PROP_STR(mp_chroma_names[p.chroma_location])},
        {0}
    };

    return m_property_read_sub(props, action, arg);
}

static struct mp_image_params get_video_out_params(struct MPContext *mpctx)
{
    if (!mpctx->d_video || !mpctx->d_video->vfilter ||
        mpctx->d_video->vfilter->initialized < 1)
        return (struct mp_image_params){0};

    return mpctx->d_video->vfilter->output_params;
}

static int mp_property_vo_imgparams(m_option_t *prop, int action, void *arg,
                                    MPContext *mpctx)
{
    return property_imgparams(get_video_out_params(mpctx), action, arg);
}

static int mp_property_vd_imgparams(m_option_t *prop, int action, void *arg,
                                    MPContext *mpctx)
{
    struct dec_video *vd = mpctx->d_video;
    if (!vd)
        return M_PROPERTY_UNAVAILABLE;
    struct sh_video *sh = vd->header->video;
    if (vd->vf_input.imgfmt) {
        return property_imgparams(vd->vf_input, action, arg);
    } else if (sh->disp_w && sh->disp_h) {
        // Simplistic fallback for stupid scripts querying "width"/"height"
        // before the first frame is decoded.
        struct m_sub_property props[] = {
            {"w", SUB_PROP_INT(sh->disp_w)},
            {"h", SUB_PROP_INT(sh->disp_h)},
            {0}
        };
        return m_property_read_sub(props, action, arg);
    }
    return M_PROPERTY_UNAVAILABLE;
}

static int mp_property_window_scale(m_option_t *prop, int action, void *arg,
                                    MPContext *mpctx)
{
    struct vo *vo = mpctx->video_out;
    if (!vo)
        return M_PROPERTY_UNAVAILABLE;

    struct mp_image_params params = get_video_out_params(mpctx);
    int vid_w = params.d_w;
    int vid_h = params.d_h;
    if (vid_w < 1 || vid_h < 1)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET: {
        double scale = *(double *)arg;
        int s[2] = {vid_w * scale, vid_h * scale};
        if (s[0] > 0 && s[1] > 0 && vo_control(vo, VOCTRL_SET_WINDOW_SIZE, s) > 0)
            return M_PROPERTY_OK;
        return M_PROPERTY_UNAVAILABLE;
    }
    case M_PROPERTY_GET: {
        int s[2];
        if (vo_control(vo, VOCTRL_GET_WINDOW_SIZE, s) <= 0 || s[0] < 1 || s[1] < 1)
            return M_PROPERTY_UNAVAILABLE;
        double xs = (double)s[0] / vid_w;
        double ys = (double)s[1] / vid_h;
        *(double *)arg = (xs + ys) / 2;
        return M_PROPERTY_OK;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_osd_w(m_option_t *prop, int action, void *arg,
                             MPContext *mpctx)
{
    struct mp_osd_res vo_res = osd_get_vo_res(mpctx->osd, OSDTYPE_OSD);
    return m_property_int_ro(prop, action, arg, vo_res.w);
}

static int mp_property_osd_h(m_option_t *prop, int action, void *arg,
                             MPContext *mpctx)
{
    struct mp_osd_res vo_res = osd_get_vo_res(mpctx->osd, OSDTYPE_OSD);
    return m_property_int_ro(prop, action, arg, vo_res.h);
}

static int mp_property_osd_par(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    struct mp_osd_res vo_res = osd_get_vo_res(mpctx->osd, OSDTYPE_OSD);
    return m_property_double_ro(prop, action, arg, vo_res.display_par);
}

/// Video fps (RO)
static int mp_property_fps(m_option_t *prop, int action, void *arg,
                           MPContext *mpctx)
{
    if (!mpctx->d_video)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_float_ro(prop, action, arg, mpctx->d_video->fps);
}

/// Video aspect (RO)
static int mp_property_aspect(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{
    if (!mpctx->d_video)
        return M_PROPERTY_UNAVAILABLE;
    struct dec_video *d_video = mpctx->d_video;
    struct sh_video *sh_video = d_video->header->video;
    switch (action) {
    case M_PROPERTY_SET: {
        mpctx->opts->movie_aspect = *(float *)arg;
        reinit_video_filters(mpctx);
        mp_force_video_refresh(mpctx);
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET: {
        float aspect = -1;
        struct mp_image_params *params = &d_video->vf_input;
        if (params && params->d_w && params->d_h) {
            aspect = (float)params->d_w / params->d_h;
        } else if (sh_video->disp_w && sh_video->disp_h) {
            aspect = (float)sh_video->disp_w / sh_video->disp_h;
        }
        if (aspect <= 0)
            return M_PROPERTY_UNAVAILABLE;
        *(float *)arg = aspect;
        return M_PROPERTY_OK;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

// For OSD and subtitle related properties using the generic option bridge.
// - Fail as unavailable if no video is active
// - Trigger OSD state update when property is set
static int property_osd_helper(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    if (!mpctx->video_out)
        return M_PROPERTY_UNAVAILABLE;
    if (action == M_PROPERTY_SET)
        osd_changed_all(mpctx->osd);
    return mp_property_generic_option(prop, action, arg, mpctx);
}

/// Selected subtitles (RW)
static int mp_property_sub(m_option_t *prop, int action, void *arg,
                           MPContext *mpctx)
{
    return property_switch_track(prop, action, arg, mpctx, 0, STREAM_SUB);
}

static int mp_property_sub2(m_option_t *prop, int action, void *arg,
                            MPContext *mpctx)
{
    return property_switch_track(prop, action, arg, mpctx, 1, STREAM_SUB);
}

/// Subtitle delay (RW)
static int mp_property_sub_delay(m_option_t *prop, int action, void *arg,
                                 MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    if (!mpctx->video_out)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_PRINT:
        *(char **)arg = format_delay(opts->sub_delay);
        return M_PROPERTY_OK;
    }
    return property_osd_helper(prop, action, arg, mpctx);
}

static int mp_property_sub_pos(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    if (!mpctx->video_out)
        return M_PROPERTY_UNAVAILABLE;
    if (action == M_PROPERTY_PRINT) {
        *(char **)arg = talloc_asprintf(NULL, "%d/100", opts->sub_pos);
        return M_PROPERTY_OK;
    }
    return property_osd_helper(prop, action, arg, mpctx);
}

#if HAVE_TV

static tvi_handle_t *get_tvh(struct MPContext *mpctx)
{
    if (!(mpctx->master_demuxer && mpctx->master_demuxer->type == DEMUXER_TYPE_TV))
        return NULL;
    return mpctx->master_demuxer->priv;
}

/// TV color settings (RW)
static int mp_property_tv_color(m_option_t *prop, int action, void *arg,
                                MPContext *mpctx)
{
    tvi_handle_t *tvh = get_tvh(mpctx);
    if (!tvh)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET:
        return tv_set_color_options(tvh, prop->offset, *(int *) arg);
    case M_PROPERTY_GET:
        return tv_get_color_options(tvh, prop->offset, arg);
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

#endif

static int mp_property_playlist_pos(m_option_t *prop, int action, void *arg,
                                    MPContext *mpctx)
{
    struct playlist *pl = mpctx->playlist;
    if (!pl->first)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_GET: {
        int pos = playlist_entry_to_index(pl, pl->current);
        if (pos < 0)
            return M_PROPERTY_UNAVAILABLE;
        *(int *)arg = pos;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_SET: {
        struct playlist_entry *e = playlist_entry_from_index(pl, *(int *)arg);
        if (!e)
            return M_PROPERTY_ERROR;
        mp_set_playlist_entry(mpctx, e);
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET_TYPE: {
        struct m_option opt = {
            .type = CONF_TYPE_INT,
            .flags = CONF_RANGE,
            .min = 0,
            .max = playlist_entry_count(pl) - 1,
        };
        *(struct m_option *)arg = opt;
        return M_PROPERTY_OK;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int get_playlist_entry(int item, int action, void *arg, void *ctx)
{
    struct MPContext *mpctx = ctx;
    struct playlist_entry *e = playlist_entry_from_index(mpctx->playlist, item);
    if (!e)
        return M_PROPERTY_ERROR;

    struct m_sub_property props[] = {
        {"filename",    SUB_PROP_STR(e->filename)},
        {0}
    };

    return m_property_read_sub(props, action, arg);
}

static int mp_property_playlist(m_option_t *prop, int action, void *arg,
                                MPContext *mpctx)
{
    if (action == M_PROPERTY_PRINT) {
        char *res = talloc_strdup(NULL, "");

        for (struct playlist_entry *e = mpctx->playlist->first; e; e = e->next)
        {
            if (mpctx->playlist->current == e) {
                res = talloc_asprintf_append(res, "> %s <\n", e->filename);
            } else {
                res = talloc_asprintf_append(res, "%s\n", e->filename);
            }
        }

        *(char **)arg = res;
        return M_PROPERTY_OK;
    }
    return m_property_read_list(action, arg, playlist_entry_count(mpctx->playlist),
                                get_playlist_entry, mpctx);
}

static char *print_obj_osd_list(struct m_obj_settings *list)
{
    char *res = NULL;
    for (int n = 0; list && list[n].name; n++) {
        res = talloc_asprintf_append(res, "%s [", list[n].name);
        for (int i = 0; list[n].attribs && list[n].attribs[i]; i += 2) {
            res = talloc_asprintf_append(res, "%s%s=%s", i > 0 ? " " : "",
                                         list[n].attribs[i],
                                         list[n].attribs[i + 1]);
        }
        res = talloc_asprintf_append(res, "]\n");
    }
    if (!res)
        res = talloc_strdup(NULL, "(empty)");
    return res;
}

static int property_filter(m_option_t *prop, int action, void *arg,
                           MPContext *mpctx, enum stream_type mt)
{
    switch (action) {
    case M_PROPERTY_PRINT: {
        struct m_config_option *opt = m_config_get_co(mpctx->mconfig,
                                                      bstr0(prop->name));
        *(char **)arg = print_obj_osd_list(*(struct m_obj_settings **)opt->data);
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_SET:
        return set_filters(mpctx, mt, *(struct m_obj_settings **)arg) >= 0
            ? M_PROPERTY_OK : M_PROPERTY_ERROR;
    }
    return mp_property_generic_option(prop, action, arg, mpctx);
}

static int mp_property_vf(m_option_t *prop, int action, void *arg,
                          MPContext *mpctx)
{
    return property_filter(prop, action, arg, mpctx, STREAM_VIDEO);
}

static int mp_property_af(m_option_t *prop, int action, void *arg,
                          MPContext *mpctx)
{
    return property_filter(prop, action, arg, mpctx, STREAM_AUDIO);
}

static int mp_property_alias(m_option_t *prop, int action, void *arg,
                             MPContext *mpctx)
{
    const char *real_property = prop->priv;
    return mp_property_do(real_property, action, arg, mpctx);
}

static int access_options(struct m_property_action_arg *ka, MPContext *mpctx)
{
    struct m_config_option *opt = m_config_get_co(mpctx->mconfig,
                                                  bstr0(ka->key));
    if (!opt)
        return M_PROPERTY_UNKNOWN;
    if (!opt->data)
        return M_PROPERTY_UNAVAILABLE;

    switch (ka->action) {
    case M_PROPERTY_GET:
        m_option_copy(opt->opt, ka->arg, opt->data);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        if (!(opt->opt->flags & (M_OPT_PRE_PARSE | M_OPT_FIXED))) {
            m_option_copy(opt->opt, opt->data, ka->arg);
            return M_PROPERTY_OK;
        }
        return M_PROPERTY_ERROR;
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)ka->arg = *opt->opt;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_options(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    switch (action) {
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_STRING_LIST};
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
        *(char ***)arg = m_config_list_options(NULL, mpctx->mconfig);
        return M_PROPERTY_OK;
    case M_PROPERTY_KEY_ACTION:
        return access_options(arg, mpctx);
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

// Use option-to-property-bridge. (The property and option have the same names.)
#define M_OPTION_PROPERTY(name) \
    {(name), mp_property_generic_option, &m_option_type_dummy, 0, 0, 0, (name)}

// OPTION_PROPERTY(), but with a custom property handler. The custom handler
// must let unknown operations fall back to mp_property_generic_option().
#define M_OPTION_PROPERTY_CUSTOM(name, handler) \
    {(name), (handler), &m_option_type_dummy, 0, 0, 0, (name)}
#define M_OPTION_PROPERTY_CUSTOM_(name, handler, ...) \
    {(name), (handler), &m_option_type_dummy, 0, 0, 0, (name), __VA_ARGS__}

// Redirect a property name to another
#define M_PROPERTY_ALIAS(name, real_property) \
    {(name), mp_property_alias, &m_option_type_dummy, 0, 0, 0, (real_property)}

// A generic property; the property callback must implement M_PROPERTY_GET_TYPE.
#define M_PROPERTY(name, handler) \
    {(name), handler, &m_option_type_dummy}

/// All properties available in MPlayer.
/** \ingroup Properties
 */
static const m_option_t mp_properties[] = {
    // General
    M_OPTION_PROPERTY("osd-level"),
    M_OPTION_PROPERTY_CUSTOM("osd-scale", property_osd_helper),
    M_OPTION_PROPERTY("loop"),
    M_OPTION_PROPERTY_CUSTOM("speed", mp_property_playback_speed),
    { "filename", mp_property_filename, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "path", mp_property_path, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "media-title", mp_property_media_title, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "stream-path", mp_property_stream_path, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    M_OPTION_PROPERTY_CUSTOM("stream-capture", mp_property_stream_capture),
    { "demuxer", mp_property_demuxer, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "stream-pos", mp_property_stream_pos, CONF_TYPE_INT64,
      M_OPT_MIN, 0, 0, NULL },
    { "stream-start", mp_property_stream_start, CONF_TYPE_INT64,
      M_OPT_MIN, 0, 0, NULL },
    { "stream-end", mp_property_stream_end, CONF_TYPE_INT64,
      M_OPT_MIN, 0, 0, NULL },
    { "stream-length", mp_property_stream_length, CONF_TYPE_INT64,
      M_OPT_MIN, 0, 0, NULL },
    { "stream-time-pos", mp_property_stream_time_pos, CONF_TYPE_TIME,
      M_OPT_MIN, 0, 0, NULL },
    { "length", mp_property_length, CONF_TYPE_TIME,
      M_OPT_MIN, 0, 0, NULL },
    { "avsync", mp_property_avsync, CONF_TYPE_DOUBLE },
    { "total-avsync-change", mp_property_total_avsync_change,
      CONF_TYPE_DOUBLE },
    { "drop-frame-count", mp_property_drop_frame_cnt, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "percent-pos", mp_property_percent_pos, CONF_TYPE_DOUBLE,
      M_OPT_RANGE, 0, 100, NULL },
    { "time-start", mp_property_time_start, CONF_TYPE_TIME,
      M_OPT_MIN, 0, 0, NULL },
    { "time-pos", mp_property_time_pos, CONF_TYPE_TIME,
      M_OPT_MIN, 0, 0, NULL },
    { "time-remaining", mp_property_remaining, CONF_TYPE_TIME },
    { "playtime-remaining", mp_property_playtime_remaining, CONF_TYPE_TIME },
    { "disc-title", mp_property_disc_title, CONF_TYPE_INT, M_OPT_MIN, -1, 0, NULL },
    { "chapter", mp_property_chapter, CONF_TYPE_INT,
      M_OPT_MIN, -1, 0, NULL },
    M_OPTION_PROPERTY_CUSTOM("edition", mp_property_edition),
    M_OPTION_PROPERTY_CUSTOM("quvi-format", mp_property_quvi_format),
    { "disc-titles", mp_property_disc_titles, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "chapters", mp_property_chapters, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "editions", mp_property_editions, CONF_TYPE_INT },
    { "angle", mp_property_angle, &m_option_type_dummy },
    M_PROPERTY("metadata", mp_property_metadata),
    M_PROPERTY("chapter-metadata", mp_property_chapter_metadata),
    M_OPTION_PROPERTY_CUSTOM("pause", mp_property_pause),
    { "cache", mp_property_cache, CONF_TYPE_INT },
    { "cache-size", mp_property_cache_size, CONF_TYPE_INT, M_OPT_MIN, 0 },
    { "paused-for-cache", mp_property_paused_for_cache, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    M_OPTION_PROPERTY("pts-association-mode"),
    M_OPTION_PROPERTY("hr-seek"),
    { "clock", mp_property_clock, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "seekable", mp_property_seekable, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },

    M_PROPERTY("chapter-list", mp_property_list_chapters),
    M_PROPERTY("track-list", property_list_tracks),
    M_PROPERTY("edition-list", property_list_editions),

    M_PROPERTY("playlist", mp_property_playlist),
    { "playlist-pos", mp_property_playlist_pos, CONF_TYPE_INT },
    M_PROPERTY_ALIAS("playlist-count", "playlist/count"),

    // Audio
    { "volume", mp_property_volume, CONF_TYPE_FLOAT,
      M_OPT_RANGE, 0, 100, NULL },
    { "mute", mp_property_mute, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    M_OPTION_PROPERTY_CUSTOM("audio-delay", mp_property_audio_delay),
    { "audio-format", mp_property_audio_format, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "audio-codec", mp_property_audio_codec, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "audio-bitrate", mp_property_audio_bitrate, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "samplerate", mp_property_samplerate, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "channels", mp_property_channels, CONF_TYPE_INT,
      0, 0, 0, NULL },
    M_OPTION_PROPERTY_CUSTOM("aid", mp_property_audio),
    { "balance", mp_property_balance, CONF_TYPE_FLOAT,
      M_OPT_RANGE, -1, 1, NULL },
    M_OPTION_PROPERTY_CUSTOM("volume-restore-data", mp_property_volrestore),

    // Video
    M_OPTION_PROPERTY_CUSTOM("fullscreen", mp_property_fullscreen),
    { "deinterlace", mp_property_deinterlace, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    M_OPTION_PROPERTY_CUSTOM("colormatrix", mp_property_colormatrix),
    M_OPTION_PROPERTY_CUSTOM("colormatrix-input-range",
                             mp_property_colormatrix_input_range),
    M_OPTION_PROPERTY_CUSTOM("colormatrix-output-range",
                             mp_property_colormatrix_output_range),
    M_OPTION_PROPERTY_CUSTOM("ontop", mp_property_ontop),
    M_OPTION_PROPERTY_CUSTOM("border", mp_property_border),
    M_OPTION_PROPERTY_CUSTOM("framedrop", mp_property_framedrop),
    M_OPTION_PROPERTY_CUSTOM("gamma", mp_property_video_color),
    M_OPTION_PROPERTY_CUSTOM("brightness", mp_property_video_color),
    M_OPTION_PROPERTY_CUSTOM("contrast", mp_property_video_color),
    M_OPTION_PROPERTY_CUSTOM("saturation", mp_property_video_color),
    M_OPTION_PROPERTY_CUSTOM("hue", mp_property_video_color),
    M_OPTION_PROPERTY_CUSTOM("panscan", panscan_property_helper),
    M_OPTION_PROPERTY_CUSTOM("video-zoom", panscan_property_helper),
    M_OPTION_PROPERTY_CUSTOM("video-align-x", panscan_property_helper),
    M_OPTION_PROPERTY_CUSTOM("video-align-y", panscan_property_helper),
    M_OPTION_PROPERTY_CUSTOM("video-pan-x", panscan_property_helper),
    M_OPTION_PROPERTY_CUSTOM("video-pan-y", panscan_property_helper),
    M_OPTION_PROPERTY_CUSTOM("video-unscaled", panscan_property_helper),
    M_PROPERTY("video-out-params", mp_property_vo_imgparams),
    M_PROPERTY("video-params", mp_property_vd_imgparams),
    { "video-format", mp_property_video_format, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "video-codec", mp_property_video_codec, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "video-bitrate", mp_property_video_bitrate, CONF_TYPE_INT,
      0, 0, 0, NULL },
    M_PROPERTY_ALIAS("dwidth", "video-out-params/dw"),
    M_PROPERTY_ALIAS("dheight", "video-out-params/dh"),
    M_PROPERTY_ALIAS("width", "video-params/w"),
    M_PROPERTY_ALIAS("height", "video-params/h"),
    { "window-scale", mp_property_window_scale, CONF_TYPE_DOUBLE,
      CONF_RANGE, 0.125, 8 },
    { "fps", mp_property_fps, CONF_TYPE_FLOAT,
      0, 0, 0, NULL },
    { "aspect", mp_property_aspect, CONF_TYPE_FLOAT,
      CONF_RANGE, -1, 10, NULL },
    M_OPTION_PROPERTY_CUSTOM("vid", mp_property_video),
    { "program", mp_property_program, CONF_TYPE_INT,
      CONF_RANGE, -1, 65535, NULL },

    { "osd-width", mp_property_osd_w, CONF_TYPE_INT },
    { "osd-height", mp_property_osd_h, CONF_TYPE_INT },
    { "osd-par", mp_property_osd_par, CONF_TYPE_DOUBLE },

    // Subs
    M_OPTION_PROPERTY_CUSTOM("sid", mp_property_sub),
    M_OPTION_PROPERTY_CUSTOM("secondary-sid", mp_property_sub2),
    M_OPTION_PROPERTY_CUSTOM("sub-delay", mp_property_sub_delay),
    M_OPTION_PROPERTY_CUSTOM("sub-pos", mp_property_sub_pos),
    M_OPTION_PROPERTY_CUSTOM("sub-visibility", property_osd_helper),
    M_OPTION_PROPERTY_CUSTOM("sub-forced-only", property_osd_helper),
    M_OPTION_PROPERTY_CUSTOM("sub-scale", property_osd_helper),
#if HAVE_LIBASS
    M_OPTION_PROPERTY_CUSTOM("ass-use-margins", property_osd_helper),
    M_OPTION_PROPERTY_CUSTOM("ass-vsfilter-aspect-compat", property_osd_helper),
    M_OPTION_PROPERTY_CUSTOM("ass-style-override", property_osd_helper),
#endif

    M_OPTION_PROPERTY_CUSTOM("vf", mp_property_vf),
    M_OPTION_PROPERTY_CUSTOM("af", mp_property_af),

#if HAVE_TV
    { "tv-brightness", mp_property_tv_color, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, .offset = TV_COLOR_BRIGHTNESS },
    { "tv-contrast", mp_property_tv_color, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, .offset = TV_COLOR_CONTRAST },
    { "tv-saturation", mp_property_tv_color, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, .offset = TV_COLOR_SATURATION },
    { "tv-hue", mp_property_tv_color, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, .offset = TV_COLOR_HUE },
#endif

    M_PROPERTY_ALIAS("video", "vid"),
    M_PROPERTY_ALIAS("audio", "aid"),
    M_PROPERTY_ALIAS("sub", "sid"),

    { "options", mp_property_options, &m_option_type_dummy },

    {0},
};

// Each entry describes which properties an event (possibly) changes.
#define E(x, ...) [x] = (const char*[]){__VA_ARGS__, NULL}
const char **mp_event_property_change[] = {
    E(MPV_EVENT_START_FILE, "*"),
    E(MPV_EVENT_END_FILE, "*"),
    E(MPV_EVENT_FILE_LOADED, "*"),
    E(MPV_EVENT_TRACKS_CHANGED, "track-list"),
    E(MPV_EVENT_TRACK_SWITCHED, "vid", "video", "aid", "audio", "sid", "sub",
      "secondary-sid"),
    E(MPV_EVENT_IDLE, "*"),
    E(MPV_EVENT_PAUSE, "pause"),
    E(MPV_EVENT_UNPAUSE, "pause"),
    E(MPV_EVENT_TICK, "time-pos", "stream-pos", "stream-time-pos", "avsync",
      "percent-pos", "time-remaining", "playtime-remaining"),
    E(MPV_EVENT_VIDEO_RECONFIG, "video-out-params", "video-params",
      "video-format", "video-codec", "video-bitrate", "dwidth", "dheight",
      "width", "height", "fps", "aspect"),
    E(MPV_EVENT_AUDIO_RECONFIG, "audio-format", "audio-codec", "audio-bitrate",
      "samplerate", "channels", "audio"),
    E(MPV_EVENT_METADATA_UPDATE, "metadata"),
};
#undef E

const struct m_option *mp_get_property_list(void)
{
    return mp_properties;
}

static bool is_property_set(int action, void *val)
{
    switch (action) {
    case M_PROPERTY_SET:
    case M_PROPERTY_SWITCH:
    case M_PROPERTY_SET_STRING:
    case M_PROPERTY_SET_NODE:
        return true;
    case M_PROPERTY_KEY_ACTION: {
        struct m_property_action_arg *key = val;
        return is_property_set(key->action, key->arg);
    }
    default:
        return false;
    }
}

int mp_property_do(const char *name, int action, void *val,
                   struct MPContext *ctx)
{
    int r = m_property_do(ctx->log, mp_properties, name, action, val, ctx);
    if (r == M_PROPERTY_OK && is_property_set(action, val))
        mp_notify_property(ctx, (char *)name);
    return r;
}

char *mp_property_expand_string(struct MPContext *mpctx, const char *str)
{
    return m_properties_expand_string(mp_properties, str, mpctx);
}

// Before expanding properties, parse C-style escapes like "\n"
char *mp_property_expand_escaped_string(struct MPContext *mpctx, const char *str)
{
    void *tmp = talloc_new(NULL);
    bstr strb = bstr0(str);
    bstr dst = {0};
    while (strb.len) {
        if (!mp_append_escaped_string(tmp, &dst, &strb)) {
            talloc_free(tmp);
            return talloc_strdup(NULL, "(broken escape sequences)");
        }
        // pass " through literally
        if (!bstr_eatstart0(&strb, "\""))
            break;
        bstr_xappend(tmp, &dst, bstr0("\""));
    }
    char *r = mp_property_expand_string(mpctx, dst.start);
    talloc_free(tmp);
    return r;
}

void property_print_help(struct mp_log *log)
{
    m_properties_print_help_list(log, mp_properties);
}


/* List of default ways to show a property on OSD.
 *
 * If osd_progbar is set, a bar showing the current position between min/max
 * values of the property is shown. In this case osd_msg is only used for
 * terminal output if there is no video; it'll be a label shown together with
 * percentage.
 */
static struct property_osd_display {
    // property name
    const char *name;
    // name used on OSD
    const char *osd_name;
    // progressbar type
    int osd_progbar;
    // Needs special ways to display the new value (seeks are delayed)
    int seek_msg, seek_bar;
    // Free-form message (if NULL, osd_name or the property name is used)
    const char *msg;
    // Extra free-from message (just for volume)
    const char *extra_msg;
} property_osd_display[] = {
    // general
    { "loop", "Loop" },
    { "chapter", .seek_msg = OSD_SEEK_INFO_CHAPTER_TEXT,
                 .seek_bar = OSD_SEEK_INFO_BAR },
    { "edition", .seek_msg = OSD_SEEK_INFO_EDITION },
    { "pts-association-mode", "PTS association mode" },
    { "hr-seek", "hr-seek" },
    { "speed", "Speed" },
    { "clock", "Clock" },
    // audio
    { "volume", "Volume",
      .extra_msg = "${?mute==yes:(Muted)}", .osd_progbar = OSD_VOLUME },
    { "mute", "Mute" },
    { "audio-delay", "A-V delay" },
    { "audio", "Audio" },
    { "balance", "Balance", .osd_progbar = OSD_BALANCE },
    // video
    { "panscan", "Panscan", .osd_progbar = OSD_PANSCAN },
    { "ontop", "Stay on top" },
    { "border", "Border" },
    { "framedrop", "Framedrop" },
    { "deinterlace", "Deinterlace" },
    { "colormatrix",
       .msg = "YUV colormatrix:\n${colormatrix}" },
    { "colormatrix-input-range",
       .msg = "YUV input range:\n${colormatrix-input-range}" },
    { "colormatrix-output-range",
       .msg = "RGB output range:\n${colormatrix-output-range}" },
    { "gamma", "Gamma", .osd_progbar = OSD_BRIGHTNESS },
    { "brightness", "Brightness", .osd_progbar = OSD_BRIGHTNESS },
    { "contrast", "Contrast", .osd_progbar = OSD_CONTRAST },
    { "saturation", "Saturation", .osd_progbar = OSD_SATURATION },
    { "hue", "Hue", .osd_progbar = OSD_HUE },
    { "angle", "Angle" },
    // subs
    { "sub", "Subtitles" },
    { "secondary-sid", "Secondary subtitles" },
    { "sub-pos", "Sub position" },
    { "sub-delay", "Sub delay" },
    { "sub-visibility", "Subtitles" },
    { "sub-forced-only", "Forced sub only" },
    { "sub-scale", "Sub Scale"},
    { "ass-vsfilter-aspect-compat", "Subtitle VSFilter aspect compat"},
    { "ass-style-override", "ASS subtitle style override"},
    { "vf", "Video filters", .msg = "Video filters:\n${vf}"},
    { "af", "Audio filters", .msg = "Audio filters:\n${af}"},
#if HAVE_TV
    { "tv-brightness", "Brightness", .osd_progbar = OSD_BRIGHTNESS },
    { "tv-hue", "Hue", .osd_progbar = OSD_HUE},
    { "tv-saturation", "Saturation", .osd_progbar = OSD_SATURATION },
    { "tv-contrast", "Contrast", .osd_progbar = OSD_CONTRAST },
#endif
    {0}
};

static void show_property_osd(MPContext *mpctx, const char *pname, int osd_mode)
{
    struct MPOpts *opts = mpctx->opts;
    struct m_option prop = {0};
    struct property_osd_display *p;
    const char *name = pname;

    if (mp_property_do(pname, M_PROPERTY_GET_TYPE, &prop, mpctx) <= 0)
        return;

    int osd_progbar = 0;
    const char *osd_name = NULL;
    const char *msg = NULL;
    const char *extra_msg = NULL;

    // look for the command
    for (p = property_osd_display; p->name; p++) {
        if (!strcmp(p->name, name)) {
            osd_progbar = p->seek_bar ? 1 : p->osd_progbar;
            osd_name = p->seek_msg ? "" : p->osd_name;
            break;
        }
    }
    if (!p->name)
        p = NULL;

    if (p) {
        msg = p->msg;
        extra_msg = p->extra_msg;
    }

    if (osd_mode != MP_ON_OSD_AUTO) {
        osd_name = osd_name ? osd_name : name;
        if (!(osd_mode & MP_ON_OSD_MSG)) {
            osd_name = NULL;
            msg = NULL;
            extra_msg = NULL;
        }
        osd_progbar = osd_progbar ? osd_progbar : ' ';
        if (!(osd_mode & MP_ON_OSD_BAR))
            osd_progbar = 0;
    }

    if (p && (p->seek_msg || p->seek_bar)) {
        mpctx->add_osd_seek_info |=
            (osd_name ? p->seek_msg : 0) | (osd_progbar ? p->seek_bar : 0);
        return;
    }

    void *tmp = talloc_new(NULL);

    if (!msg && osd_name)
        msg = talloc_asprintf(tmp, "%s: ${%s}", osd_name, name);

    if (osd_progbar && (prop.flags & CONF_RANGE) == CONF_RANGE) {
        bool ok = false;
        if (prop.type == CONF_TYPE_INT) {
            int i;
            ok = mp_property_do(name, M_PROPERTY_GET, &i, mpctx) > 0;
            if (ok)
                set_osd_bar(mpctx, osd_progbar, osd_name, prop.min, prop.max, i);
        } else if (prop.type == CONF_TYPE_FLOAT) {
            float f;
            ok = mp_property_do(name, M_PROPERTY_GET, &f, mpctx) > 0;
            if (ok)
                set_osd_bar(mpctx, osd_progbar, osd_name, prop.min, prop.max, f);
        }
        if (ok && osd_mode == MP_ON_OSD_AUTO && opts->osd_bar_visible)
            msg = NULL;
    }

    char *osd_msg = NULL;
    if (msg)
        osd_msg = talloc_steal(tmp, mp_property_expand_string(mpctx, msg));
    if (extra_msg) {
        char *t = talloc_steal(tmp, mp_property_expand_string(mpctx, extra_msg));
        osd_msg = talloc_asprintf(tmp, "%s%s%s", osd_msg ? osd_msg : "",
                                  osd_msg && osd_msg[0] ? " " : "", t);
    }

    if (osd_msg && osd_msg[0])
        set_osd_msg(mpctx, 1, opts->osd_duration, "%s", osd_msg);

    talloc_free(tmp);
}

static const char *property_error_string(int error_value)
{
    switch (error_value) {
    case M_PROPERTY_ERROR:
        return "ERROR";
    case M_PROPERTY_UNAVAILABLE:
        return "PROPERTY_UNAVAILABLE";
    case M_PROPERTY_NOT_IMPLEMENTED:
        return "NOT_IMPLEMENTED";
    case M_PROPERTY_UNKNOWN:
        return "PROPERTY_UNKNOWN";
    }
    return "UNKNOWN";
}

static bool reinit_filters(MPContext *mpctx, enum stream_type mediatype)
{
    switch (mediatype) {
    case STREAM_VIDEO:
        return reinit_video_filters(mpctx) >= 0;
    case STREAM_AUDIO:
        return reinit_audio_filters(mpctx) >= 0;
    }
    return false;
}

static const char *filter_opt[STREAM_TYPE_COUNT] = {
    [STREAM_VIDEO] = "vf",
    [STREAM_AUDIO] = "af",
};

static int set_filters(struct MPContext *mpctx, enum stream_type mediatype,
                       struct m_obj_settings *new_chain)
{
    bstr option = bstr0(filter_opt[mediatype]);
    struct m_config_option *co = m_config_get_co(mpctx->mconfig, option);
    if (!co)
        return -1;

    struct m_obj_settings **list = co->data;
    struct m_obj_settings *old_settings = *list;
    *list = NULL;
    m_option_copy(co->opt, list, &new_chain);

    bool success = reinit_filters(mpctx, mediatype);

    if (success) {
        m_option_free(co->opt, &old_settings);
    } else {
        m_option_free(co->opt, list);
        *list = old_settings;
        reinit_filters(mpctx, mediatype);
    }

    if (mediatype == STREAM_VIDEO)
        mp_force_video_refresh(mpctx);

    return success ? 0 : -1;
}

static int edit_filters(struct MPContext *mpctx, enum stream_type mediatype,
                        const char *cmd, const char *arg)
{
    bstr option = bstr0(filter_opt[mediatype]);
    struct m_config_option *co = m_config_get_co(mpctx->mconfig, option);
    if (!co)
        return -1;

    // The option parser is used to modify the filter list itself.
    char optname[20];
    snprintf(optname, sizeof(optname), "%.*s-%s", BSTR_P(option), cmd);

    struct m_obj_settings *new_chain = NULL;
    m_option_copy(co->opt, &new_chain, co->data);

    int r = m_option_parse(mpctx->log, co->opt, bstr0(optname), bstr0(arg),
                           &new_chain);
    if (r >= 0)
        r = set_filters(mpctx, mediatype, new_chain);

    m_option_free(co->opt, &new_chain);

    return r >= 0 ? 0 : -1;
}

static int edit_filters_osd(struct MPContext *mpctx, enum stream_type mediatype,
                            const char *cmd, const char *arg, bool on_osd)
{
    int r = edit_filters(mpctx, mediatype, cmd, arg);
    if (on_osd) {
        if (r >= 0) {
            const char *prop = filter_opt[mediatype];
            show_property_osd(mpctx, prop, MP_ON_OSD_MSG);
        } else {
            set_osd_msg(mpctx, 1, mpctx->opts->osd_duration,
                         "Changing filters failed!");
        }
    }
    return r;
}

#if HAVE_SYS_MMAN_H

static int ext2_sub_find(struct MPContext *mpctx, int id)
{
    struct command_ctx *cmd = mpctx->command_ctx;
    struct sub_bitmaps *sub = &cmd->external2;
    void *p = NULL;
    if (id >= 0 && id < OVERLAY_MAX_ID)
        p = cmd->overlay_map[id];
    if (sub && p) {
        for (int n = 0; n < sub->num_parts; n++) {
            if (sub->parts[n].bitmap == p)
                return n;
        }
    }
    return -1;
}

static int ext2_sub_alloc(struct MPContext *mpctx)
{
    struct command_ctx *cmd = mpctx->command_ctx;
    struct sub_bitmaps *sub = &cmd->external2;
    struct sub_bitmap b = {0};
    MP_TARRAY_APPEND(cmd, sub->parts, sub->num_parts, b);
    return sub->num_parts - 1;
}

static int overlay_add(struct MPContext *mpctx, int id, int x, int y,
                       char *file, int offset, char *fmt, int w, int h,
                       int stride)
{
    struct command_ctx *cmd = mpctx->command_ctx;
    int r = -1;
    // Temporarily unmap them to avoid race condition with concurrent access.
    osd_set_external2(mpctx->osd, NULL);
    if (strcmp(fmt, "bgra") != 0) {
        MP_ERR(mpctx, "overlay_add: unsupported OSD format '%s'\n", fmt);
        goto error;
    }
    if (id < 0 || id >= OVERLAY_MAX_ID) {
        MP_ERR(mpctx, "overlay_add: invalid id %d\n", id);
        goto error;
    }
    int fd = -1;
    bool close_fd = true;
    if (file[0] == '@') {
        char *end;
        fd = strtol(&file[1], &end, 10);
        if (!file[1] || end[0])
            fd = -1;
        close_fd = false;
    } else {
        fd = open(file, O_RDONLY | O_BINARY | O_CLOEXEC);
    }
    void *p = mmap(NULL, h * stride, PROT_READ, MAP_SHARED, fd, offset);
    if (fd >= 0 && close_fd)
        close(fd);
    if (!p) {
        MP_ERR(mpctx, "overlay_add: could not open or map '%s'\n", file);
        goto error;
    }
    int index = ext2_sub_find(mpctx, id);
    if (index < 0)
        index = ext2_sub_alloc(mpctx);
    if (index < 0) {
        munmap(p, h * stride);
        goto error;
    }
    cmd->overlay_map[id] = p;
    cmd->external2.parts[index] = (struct sub_bitmap) {
        .bitmap = p,
        .stride = stride,
        .x = x, .y = y,
        .w = w, .h = h,
        .dw = w, .dh = h,
    };
    cmd->external2.bitmap_id = cmd->external2.bitmap_pos_id = 1;
    cmd->external2.format = SUBBITMAP_RGBA;
    r = 0;
error:
    osd_set_external2(mpctx->osd, &cmd->external2);
    return r;
}

static void overlay_remove(struct MPContext *mpctx, int id)
{
    struct command_ctx *cmd = mpctx->command_ctx;
    osd_set_external2(mpctx->osd, NULL);
    int index = ext2_sub_find(mpctx, id);
    if (index >= 0) {
        struct sub_bitmaps *sub = &cmd->external2;
        struct sub_bitmap *part = &sub->parts[index];
        munmap(part->bitmap, part->h * part->stride);
        MP_TARRAY_REMOVE_AT(sub->parts, sub->num_parts, index);
        cmd->overlay_map[id] = NULL;
        sub->bitmap_id = sub->bitmap_pos_id = 1;
    }
    osd_set_external2(mpctx->osd, &cmd->external2);
}

static void overlay_uninit(struct MPContext *mpctx)
{
    if (!mpctx->osd)
        return;
    for (int id = 0; id < OVERLAY_MAX_ID; id++)
        overlay_remove(mpctx, id);
    osd_set_external2(mpctx->osd, NULL);
}

#else

static void overlay_uninit(struct MPContext *mpctx){}

#endif

struct cycle_counter {
    char **args;
    int counter;
};

static bool stringlist_equals(char **l1, char **l2)
{
    assert(l1 && l2);
    for (int i = 0; ; i++) {
        if (!l1[i] && !l2[i])
            return true;
        if (!l1[i] || !l2[i])
            return false;
        if (strcmp(l1[i], l2[i]) != 0)
            return false;
    }
}

static char **stringlist_dup(void *talloc_ctx, char **list)
{
    int num = 0;
    char **res = NULL;
    for (int i = 0; list && list[i]; i++)
        MP_TARRAY_APPEND(talloc_ctx, res, num, talloc_strdup(talloc_ctx, list[i]));
    MP_TARRAY_APPEND(talloc_ctx, res, num, NULL);
    return res;
}

static int *get_cmd_cycle_counter(struct MPContext *mpctx, char **args)
{
    struct command_ctx *cmd = mpctx->command_ctx;
    for (int n = 0; n < cmd->num_cycle_counters; n++) {
        struct cycle_counter *ctr = &cmd->cycle_counters[n];
        if (stringlist_equals(ctr->args, args))
            return &ctr->counter;
    }
    struct cycle_counter ctr = {stringlist_dup(cmd, args), 0};
    MP_TARRAY_APPEND(cmd, cmd->cycle_counters, cmd->num_cycle_counters, ctr);
    return &cmd->cycle_counters[cmd->num_cycle_counters - 1].counter;
}

static int mp_property_multiply(char *property, double f, struct MPContext *mpctx)
{
    union m_option_value val = {0};
    struct m_option opt = {0};
    int r;

    r = mp_property_do(property, M_PROPERTY_GET_TYPE, &opt, mpctx);
    if (r != M_PROPERTY_OK)
        return r;
    assert(opt.type);

    if (!opt.type->multiply)
        return M_PROPERTY_NOT_IMPLEMENTED;

    r = mp_property_do(property, M_PROPERTY_GET, &val, mpctx);
    if (r != M_PROPERTY_OK)
        return r;
    opt.type->multiply(&opt, &val, f);
    r = mp_property_do(property, M_PROPERTY_SET, &val, mpctx);
    m_option_free(&opt, &val);
    return r;
}

// Whether this property should react to key events generated by auto-repeat.
static bool check_property_autorepeat(char *property,  struct MPContext *mpctx)
{
    struct m_option prop = {0};
    if (mp_property_do(property, M_PROPERTY_GET_TYPE, &prop, mpctx) <= 0)
        return true;

    // This is a heuristic at best.
    if (prop.type == &m_option_type_flag || prop.type == &m_option_type_choice)
        return false;

    return true;
}

void run_command(MPContext *mpctx, mp_cmd_t *cmd)
{
    struct command_ctx *cmdctx = mpctx->command_ctx;
    struct MPOpts *opts = mpctx->opts;
    int osd_duration = opts->osd_duration;
    int on_osd = cmd->flags & MP_ON_OSD_FLAGS;
    bool auto_osd = on_osd == MP_ON_OSD_AUTO;
    bool msg_osd = auto_osd || (on_osd & MP_ON_OSD_MSG);
    bool bar_osd = auto_osd || (on_osd & MP_ON_OSD_BAR);
    bool msg_or_nobar_osd = msg_osd && !(auto_osd && opts->osd_bar_visible);
    int osdl = msg_osd ? 1 : OSD_LEVEL_INVISIBLE;

    if (cmd->flags & MP_EXPAND_PROPERTIES) {
        for (int n = 0; n < cmd->nargs; n++) {
            if (cmd->args[n].type->type == CONF_TYPE_STRING) {
                char *s = mp_property_expand_string(mpctx, cmd->args[n].v.s);
                if (!s)
                    return;
                talloc_free(cmd->args[n].v.s);
                cmd->args[n].v.s = s;
            }
        }
    }

    switch (cmd->id) {
    case MP_CMD_SEEK: {
        double v = cmd->args[0].v.d * cmd->scale;
        int abs = cmd->args[1].v.i;
        int exact = cmd->args[2].v.i;
        mark_seek(mpctx);
        if (abs == 2) {   // Absolute seek to a timestamp in seconds
            queue_seek(mpctx, MPSEEK_ABSOLUTE, v, exact, false);
            set_osd_function(mpctx,
                             v > get_current_time(mpctx) ? OSD_FFW : OSD_REW);
        } else if (abs) {           /* Absolute seek by percentage */
            queue_seek(mpctx, MPSEEK_FACTOR, v / 100.0, exact, false);
            set_osd_function(mpctx, OSD_FFW); // Direction isn't set correctly
        } else {
            queue_seek(mpctx, MPSEEK_RELATIVE, v, exact, false);
            set_osd_function(mpctx, (v > 0) ? OSD_FFW : OSD_REW);
        }
        if (bar_osd)
            mpctx->add_osd_seek_info |= OSD_SEEK_INFO_BAR;
        if (msg_or_nobar_osd)
            mpctx->add_osd_seek_info |= OSD_SEEK_INFO_TEXT;
        break;
    }

    case MP_CMD_REVERT_SEEK: {
        double oldpts = cmdctx->last_seek_pts;
        if (oldpts != MP_NOPTS_VALUE) {
            cmdctx->last_seek_pts = get_current_time(mpctx);
            queue_seek(mpctx, MPSEEK_ABSOLUTE, oldpts, 1, false);
            set_osd_function(mpctx, OSD_REW);
            if (bar_osd)
                mpctx->add_osd_seek_info |= OSD_SEEK_INFO_BAR;
            if (msg_or_nobar_osd)
                mpctx->add_osd_seek_info |= OSD_SEEK_INFO_TEXT;
        }
        break;
    }

    case MP_CMD_SET: {
        int r = mp_property_do(cmd->args[0].v.s, M_PROPERTY_SET_STRING,
                               cmd->args[1].v.s, mpctx);
        if (r == M_PROPERTY_OK || r == M_PROPERTY_UNAVAILABLE) {
            show_property_osd(mpctx, cmd->args[0].v.s, on_osd);
        } else if (r == M_PROPERTY_UNKNOWN) {
            set_osd_msg(mpctx, osdl, osd_duration,
                        "Unknown property: '%s'", cmd->args[0].v.s);
        } else if (r <= 0) {
            set_osd_msg(mpctx, osdl, osd_duration,
                        "Failed to set property '%s' to '%s'",
                        cmd->args[0].v.s, cmd->args[1].v.s);
        }
        break;
    }

    case MP_CMD_ADD:
    case MP_CMD_CYCLE:
    {
        struct m_property_switch_arg s = {
            .inc = 1,
            .wrap = cmd->id == MP_CMD_CYCLE,
        };
        if (cmd->args[1].v.d)
            s.inc = cmd->args[1].v.d * cmd->scale;
        char *property = cmd->args[0].v.s;
        if (cmd->repeated && !check_property_autorepeat(property, mpctx)) {
            MP_VERBOSE(mpctx, "Dropping command '%.*s' from auto-repeated key.\n",
                       BSTR_P(cmd->original));
            break;
        }
        int r = mp_property_do(property, M_PROPERTY_SWITCH, &s, mpctx);
        if (r == M_PROPERTY_OK || r == M_PROPERTY_UNAVAILABLE) {
            show_property_osd(mpctx, property, on_osd);
        } else if (r == M_PROPERTY_UNKNOWN) {
            set_osd_msg(mpctx, osdl, osd_duration,
                        "Unknown property: '%s'", property);
        } else if (r <= 0) {
            set_osd_msg(mpctx, osdl, osd_duration,
                        "Failed to increment property '%s' by %g",
                        property, s.inc);
        }
        break;
    }

    case MP_CMD_MULTIPLY: {
        char *property = cmd->args[0].v.s;
        double f = cmd->args[1].v.d;
        int r = mp_property_multiply(property, f, mpctx);

        if (r == M_PROPERTY_OK || r == M_PROPERTY_UNAVAILABLE) {
            show_property_osd(mpctx, property, on_osd);
        } else if (r == M_PROPERTY_UNKNOWN) {
            set_osd_msg(mpctx, osdl, osd_duration,
                        "Unknown property: '%s'", property);
        } else if (r <= 0) {
            set_osd_msg(mpctx, osdl, osd_duration,
                        "Failed to multiply property '%s' by %g", property, f);
        }
        break;
    }

    case MP_CMD_CYCLE_VALUES: {
        char *args[MP_CMD_MAX_ARGS + 1] = {0};
        for (int n = 0; n < cmd->nargs; n++)
            args[n] = cmd->args[n].v.s;
        int first = 1, dir = 1;
        if (strcmp(args[0], "!reverse") == 0) {
            first += 1;
            dir = -1;
        }
        int *ptr = get_cmd_cycle_counter(mpctx, &args[first - 1]);
        int count = cmd->nargs - first;
        if (ptr && count > 0) {
            int next = *ptr;
            *ptr += dir;
            if (*ptr >= count)
                *ptr = 0;
            if (*ptr < 0)
                *ptr = count - 1;
            char *property = args[first - 1];
            char *value = args[first + next];
            int r = mp_property_do(property, M_PROPERTY_SET_STRING, value, mpctx);
            if (r == M_PROPERTY_OK || r == M_PROPERTY_UNAVAILABLE) {
                show_property_osd(mpctx, property, on_osd);
            } else if (r == M_PROPERTY_UNKNOWN) {
                set_osd_msg(mpctx, osdl, osd_duration,
                            "Unknown property: '%s'", property);
            } else if (r <= 0) {
                set_osd_msg(mpctx, osdl, osd_duration,
                            "Failed to set property '%s' to '%s'",
                            property, value);
            }
        }
        break;
    }

    case MP_CMD_GET_PROPERTY: {
        char *tmp;
        int r = mp_property_do(cmd->args[0].v.s, M_PROPERTY_GET_STRING,
                               &tmp, mpctx);
        if (r <= 0) {
            MP_WARN(mpctx, "Failed to get value of property '%s'.\n",
                    cmd->args[0].v.s);
            MP_INFO(mpctx, "ANS_ERROR=%s\n", property_error_string(r));
            break;
        }
        MP_INFO(mpctx, "ANS_%s=%s\n", cmd->args[0].v.s, tmp);
        talloc_free(tmp);
        break;
    }

    case MP_CMD_FRAME_STEP:
        add_step_frame(mpctx, 1);
        break;

    case MP_CMD_FRAME_BACK_STEP:
        add_step_frame(mpctx, -1);
        break;

    case MP_CMD_QUIT:
        mpctx->stop_play = PT_QUIT;
        mpctx->quit_custom_rc = cmd->args[0].v.i;
        mpctx->has_quit_custom_rc = true;
        break;

    case MP_CMD_QUIT_WATCH_LATER:
        mp_write_watch_later_conf(mpctx);
        mpctx->stop_play = PT_QUIT;
        mpctx->quit_player_rc = 0;
        break;

    case MP_CMD_PLAYLIST_NEXT:
    case MP_CMD_PLAYLIST_PREV:
    {
        int dir = cmd->id == MP_CMD_PLAYLIST_PREV ? -1 : +1;
        int force = cmd->args[0].v.i;

        struct playlist_entry *e = mp_next_file(mpctx, dir, force);
        if (!e && !force)
            break;
        mpctx->playlist->current = e;
        mpctx->playlist->current_was_replaced = false;
        mpctx->stop_play = PT_CURRENT_ENTRY;
        break;
    }

    case MP_CMD_SUB_STEP:
    case MP_CMD_SUB_SEEK: {
        struct osd_sub_state state;
        osd_get_sub(mpctx->osd, OSDTYPE_SUB, &state);
        if (state.dec_sub) {
            double a[2];
            a[0] = mpctx->video_pts - state.video_offset - opts->sub_delay;
            a[1] = cmd->args[0].v.i;
            if (sub_control(state.dec_sub, SD_CTRL_SUB_STEP, a) > 0) {
                if (cmd->id == MP_CMD_SUB_STEP) {
                    opts->sub_delay -= a[0];
                    osd_changed_all(mpctx->osd);
                    set_osd_msg(mpctx, osdl, osd_duration,
                                 "Sub delay: %d ms", ROUND(opts->sub_delay * 1000));
                } else {
                    // We can easily get stuck by failing to seek to the video
                    // frame which actually shows the sub first (because video
                    // frame PTS and sub PTS rarely match exactly). Add some
                    // rounding for the mess of it.
                    a[0] += 0.01 * (a[1] > 0 ? 1 : -1);
                    mark_seek(mpctx);
                    queue_seek(mpctx, MPSEEK_RELATIVE, a[0], 1, false);
                    set_osd_function(mpctx, (a[0] > 0) ? OSD_FFW : OSD_REW);
                    if (bar_osd)
                        mpctx->add_osd_seek_info |= OSD_SEEK_INFO_BAR;
                    if (msg_or_nobar_osd)
                        mpctx->add_osd_seek_info |= OSD_SEEK_INFO_TEXT;
                }
            }
        }
        break;
    }

    case MP_CMD_OSD: {
        int v = cmd->args[0].v.i;
        int max = (opts->term_osd && !mpctx->video_out) ? MAX_TERM_OSD_LEVEL
                                                        : MAX_OSD_LEVEL;
        if (opts->osd_level > max)
            opts->osd_level = max;
        if (v < 0)
            opts->osd_level = (opts->osd_level + 1) % (max + 1);
        else
            opts->osd_level = v > max ? max : v;
        if (msg_osd && opts->osd_level <= 1)
            set_osd_msg(mpctx, 0, osd_duration,
                         "OSD: %s", opts->osd_level ? "yes" : "no");
        break;
    }

    case MP_CMD_PRINT_TEXT: {
        MP_INFO(mpctx, "%s\n", cmd->args[0].v.s);
        break;
    }

    case MP_CMD_SHOW_TEXT: {
        // if no argument supplied use default osd_duration, else <arg> ms.
        set_osd_msg(mpctx, cmd->args[2].v.i,
                    (cmd->args[1].v.i < 0 ? osd_duration : cmd->args[1].v.i),
                    "%s", cmd->args[0].v.s);
        break;
    }

    case MP_CMD_LOADFILE: {
        char *filename = cmd->args[0].v.s;
        bool append = cmd->args[1].v.i;

        if (!append)
            playlist_clear(mpctx->playlist);

        struct playlist_entry *entry = playlist_entry_new(filename);
        if (cmd->args[2].v.str_list) {
            char **pairs = cmd->args[2].v.str_list;
            for (int i = 0; pairs[i] && pairs[i + 1]; i += 2) {
                playlist_entry_add_param(entry, bstr0(pairs[i]),
                                         bstr0(pairs[i + 1]));
            }
        }
        playlist_add(mpctx->playlist, entry);

        if (!append)
            mp_set_playlist_entry(mpctx, mpctx->playlist->first);
        break;
    }

    case MP_CMD_LOADLIST: {
        char *filename = cmd->args[0].v.s;
        bool append = cmd->args[1].v.i;
        struct playlist *pl = playlist_parse_file(filename, mpctx->global);
        if (pl) {
            if (!append)
                playlist_clear(mpctx->playlist);
            playlist_transfer_entries(mpctx->playlist, pl);
            talloc_free(pl);

            if (!append && mpctx->playlist->first) {
                struct playlist_entry *e =
                    mp_check_playlist_resume(mpctx, mpctx->playlist);
                mp_set_playlist_entry(mpctx, e ? e : mpctx->playlist->first);
            }
        } else {
            MP_ERR(mpctx, "Unable to load playlist %s.\n", filename);
        }
        break;
    }

    case MP_CMD_PLAYLIST_CLEAR: {
        // Supposed to clear the playlist, except the currently played item.
        if (mpctx->playlist->current_was_replaced)
            mpctx->playlist->current = NULL;
        while (mpctx->playlist->first) {
            struct playlist_entry *e = mpctx->playlist->first;
            if (e == mpctx->playlist->current) {
                e = e->next;
                if (!e)
                    break;
            }
            playlist_remove(mpctx->playlist, e);
        }
        break;
    }

    case MP_CMD_PLAYLIST_REMOVE: {
        struct playlist_entry *e = playlist_entry_from_index(mpctx->playlist,
                                                             cmd->args[0].v.i);
        if (cmd->args[0].v.i < 0)
            e = mpctx->playlist->current;
        if (e) {
            // Can't play a removed entry
            if (mpctx->playlist->current == e)
                mpctx->stop_play = PT_CURRENT_ENTRY;
            playlist_remove(mpctx->playlist, e);
        }
        break;
    }

    case MP_CMD_PLAYLIST_MOVE: {
        struct playlist_entry *e1 = playlist_entry_from_index(mpctx->playlist,
                                                              cmd->args[0].v.i);
        struct playlist_entry *e2 = playlist_entry_from_index(mpctx->playlist,
                                                              cmd->args[1].v.i);
        if (e1) {
            playlist_move(mpctx->playlist, e1, e2);
        }
        break;
    }

    case MP_CMD_STOP:
        // Go back to the starting point.
        mpctx->stop_play = PT_STOP;
        break;

    case MP_CMD_SHOW_PROGRESS:
        mpctx->add_osd_seek_info |=
                (msg_osd ? OSD_SEEK_INFO_TEXT : 0) |
                (bar_osd ? OSD_SEEK_INFO_BAR : 0);
        break;

#if HAVE_TV
    case MP_CMD_TV_START_SCAN:
        if (get_tvh(mpctx))
            tv_start_scan(get_tvh(mpctx), 1);
        break;
    case MP_CMD_TV_SET_FREQ:
        if (get_tvh(mpctx))
            tv_set_freq(get_tvh(mpctx), cmd->args[0].v.f * 16.0);
#if HAVE_PVR
        else if (mpctx->stream && mpctx->stream->type == STREAMTYPE_PVR) {
            pvr_set_freq(mpctx->stream, ROUND(cmd->args[0].v.f));
            set_osd_msg(mpctx, osdl, osd_duration, "%s: %s",
                        pvr_get_current_channelname(mpctx->stream),
                        pvr_get_current_stationname(mpctx->stream));
        }
#endif /* HAVE_PVR */
        break;

    case MP_CMD_TV_STEP_FREQ:
        if (get_tvh(mpctx))
            tv_step_freq(get_tvh(mpctx), cmd->args[0].v.f * 16.0);
#if HAVE_PVR
        else if (mpctx->stream && mpctx->stream->type == STREAMTYPE_PVR) {
            pvr_force_freq_step(mpctx->stream, ROUND(cmd->args[0].v.f));
            set_osd_msg(mpctx, osdl, osd_duration, "%s: f %d",
                        pvr_get_current_channelname(mpctx->stream),
                        pvr_get_current_frequency(mpctx->stream));
        }
#endif /* HAVE_PVR */
        break;

    case MP_CMD_TV_SET_NORM:
        if (get_tvh(mpctx))
            tv_set_norm(get_tvh(mpctx), cmd->args[0].v.s);
        break;

    case MP_CMD_TV_STEP_CHANNEL:
        if (get_tvh(mpctx)) {
            int v = cmd->args[0].v.i;
            if (v > 0) {
                tv_step_channel(get_tvh(mpctx), TV_CHANNEL_HIGHER);
            } else {
                tv_step_channel(get_tvh(mpctx), TV_CHANNEL_LOWER);
            }
            if (tv_channel_list) {
                set_osd_msg(mpctx, osdl, osd_duration,
                             "Channel: %s", tv_channel_current->name);
            }
        }
#if HAVE_PVR
        else if (mpctx->stream &&
                 mpctx->stream->type == STREAMTYPE_PVR) {
            pvr_set_channel_step(mpctx->stream, cmd->args[0].v.i);
            set_osd_msg(mpctx, osdl, osd_duration, "%s: %s",
                        pvr_get_current_channelname(mpctx->stream),
                        pvr_get_current_stationname(mpctx->stream));
        }
#endif /* HAVE_PVR */
#if HAVE_DVBIN
        if (mpctx->stream && mpctx->stream->type == STREAMTYPE_DVB) {
            int dir;
            int v = cmd->args[0].v.i;

            mpctx->last_dvb_step = v;
            if (v > 0)
                dir = DVB_CHANNEL_HIGHER;
            else
                dir = DVB_CHANNEL_LOWER;


            if (dvb_step_channel(mpctx->stream, dir)) {
                mpctx->stop_play = PT_RELOAD_DEMUXER;
            }
        }
#endif /* HAVE_DVBIN */
        break;

    case MP_CMD_TV_SET_CHANNEL:
        if (get_tvh(mpctx)) {
            tv_set_channel(get_tvh(mpctx), cmd->args[0].v.s);
            if (tv_channel_list) {
                set_osd_msg(mpctx, osdl, osd_duration,
                             "Channel: %s", tv_channel_current->name);
            }
        }
#if HAVE_PVR
        else if (mpctx->stream && mpctx->stream->type == STREAMTYPE_PVR) {
            pvr_set_channel(mpctx->stream, cmd->args[0].v.s);
            set_osd_msg(mpctx, osdl, osd_duration, "%s: %s",
                        pvr_get_current_channelname(mpctx->stream),
                        pvr_get_current_stationname(mpctx->stream));
        }
#endif /* HAVE_PVR */
        break;

#if HAVE_DVBIN
    case MP_CMD_DVB_SET_CHANNEL:
        if (mpctx->stream && mpctx->stream->type == STREAMTYPE_DVB) {
            mpctx->last_dvb_step = 1;

            if (dvb_set_channel(mpctx->stream, cmd->args[1].v.i,
                                cmd->args[0].v.i)) {
                mpctx->stop_play = PT_RELOAD_DEMUXER;
            }
        }
        break;
#endif /* HAVE_DVBIN */

    case MP_CMD_TV_LAST_CHANNEL:
        if (get_tvh(mpctx)) {
            tv_last_channel(get_tvh(mpctx));
            if (tv_channel_list) {
                set_osd_msg(mpctx, osdl, osd_duration,
                             "Channel: %s", tv_channel_current->name);
            }
        }
#if HAVE_PVR
        else if (mpctx->stream && mpctx->stream->type == STREAMTYPE_PVR) {
            pvr_set_lastchannel(mpctx->stream);
            set_osd_msg(mpctx, osdl, osd_duration, "%s: %s",
                        pvr_get_current_channelname(mpctx->stream),
                        pvr_get_current_stationname(mpctx->stream));
        }
#endif /* HAVE_PVR */
        break;

    case MP_CMD_TV_STEP_NORM:
        if (get_tvh(mpctx))
            tv_step_norm(get_tvh(mpctx));
        break;

    case MP_CMD_TV_STEP_CHANNEL_LIST:
        if (get_tvh(mpctx))
            tv_step_chanlist(get_tvh(mpctx));
        break;
#endif /* HAVE_TV */

    case MP_CMD_SUB_ADD: {
        struct track *sub = mp_add_subtitles(mpctx, cmd->args[0].v.s);
        if (sub) {
            mp_switch_track(mpctx, sub->type, sub);
            mp_mark_user_track_selection(mpctx, 0, sub->type);
        }
        break;
    }

    case MP_CMD_SUB_REMOVE: {
        struct track *sub = mp_track_by_tid(mpctx, STREAM_SUB, cmd->args[0].v.i);
        if (sub)
            mp_remove_track(mpctx, sub);
        break;
    }

    case MP_CMD_SUB_RELOAD: {
        struct track *sub = mp_track_by_tid(mpctx, STREAM_SUB, cmd->args[0].v.i);
        if (sub && sub->is_external && sub->external_filename) {
            struct track *nsub = mp_add_subtitles(mpctx, sub->external_filename);
            if (nsub) {
                mp_remove_track(mpctx, sub);
                mp_switch_track(mpctx, nsub->type, nsub);
            }
        }
        break;
    }

    case MP_CMD_SCREENSHOT:
        screenshot_request(mpctx, cmd->args[0].v.i, cmd->args[1].v.i, msg_osd);
        break;

    case MP_CMD_SCREENSHOT_TO_FILE:
        screenshot_to_file(mpctx, cmd->args[0].v.s, cmd->args[1].v.i, msg_osd);
        break;

    case MP_CMD_RUN: {
#ifndef __MINGW32__
        mp_msg_flush_status_line(mpctx->global);
        char *args[MP_CMD_MAX_ARGS + 1] = {0};
        for (int n = 0; n < cmd->nargs; n++)
            args[n] = cmd->args[n].v.s;
        pid_t child = fork();
        if (child == 0) {
            // Fork twice; the second process will be made child of pid 1 as
            // soon as the first process exists, and we don't have to care
            // about having to wait for the second process to terminate.
            if (fork() == 0) {
                execvp(args[0], args);
                // mp_msg() is not safe to be called from a forked process.
                char s[] = "Executing program failed.\n";
                write(2, s, sizeof(s) - 1);
                _exit(1);
            }
            _exit(0);
        }
        int st;
        while (child != -1 && waitpid(child, &st, 0) < 0 && errno == EINTR) {}
#endif
        break;
    }

    case MP_CMD_ENABLE_INPUT_SECTION:
        mp_input_enable_section(mpctx->input, cmd->args[0].v.s,
                                cmd->args[1].v.i == 1 ? MP_INPUT_EXCLUSIVE : 0);
        break;

    case MP_CMD_DISABLE_INPUT_SECTION:
        mp_input_disable_section(mpctx->input, cmd->args[0].v.s);
        break;

    case MP_CMD_DISCNAV:
        mp_nav_user_input(mpctx, cmd->args[0].v.s);
        break;

    case MP_CMD_VO_CMDLINE:
        if (mpctx->video_out) {
            char *s = cmd->args[0].v.s;
            MP_INFO(mpctx, "Setting vo cmd line to '%s'.\n", s);
            if (vo_control(mpctx->video_out, VOCTRL_SET_COMMAND_LINE, s) > 0) {
                set_osd_msg(mpctx, osdl, osd_duration, "vo='%s'", s);
            } else {
                set_osd_msg(mpctx, osdl, osd_duration, "Failed!");
            }
        }
        break;

    case MP_CMD_AF:
        edit_filters_osd(mpctx, STREAM_AUDIO, cmd->args[0].v.s,
                         cmd->args[1].v.s, msg_osd);
        break;

    case MP_CMD_VF:
        edit_filters_osd(mpctx, STREAM_VIDEO, cmd->args[0].v.s,
                         cmd->args[1].v.s, msg_osd);
        break;

    case MP_CMD_SCRIPT_DISPATCH: {
        mpv_event_script_input_dispatch *event = talloc_ptrtype(NULL, event);
        *event = (mpv_event_script_input_dispatch){
            .arg0 = cmd->args[1].v.i,
            .type = cmd->key_up_follows ? "keyup_follows" : "press",
        };
        if (mp_client_send_event(mpctx, cmd->args[0].v.s,
                                 MPV_EVENT_SCRIPT_INPUT_DISPATCH, event) < 0)
        {
            MP_VERBOSE(mpctx, "Can't find script '%s' when handling input.\n",
                       cmd->args[0].v.s);
        }
        break;
    }

    case MP_CMD_SCRIPT_MESSAGE_TO: {
        mpv_event_client_message *event = talloc_ptrtype(NULL, event);
        *event = (mpv_event_client_message){0};
        for (int n = 1; n < cmd->nargs; n++) {
            MP_TARRAY_APPEND(event, event->args, event->num_args,
                             cmd->args[n].v.s);
        }
        if (mp_client_send_event(mpctx, cmd->args[0].v.s,
                                 MPV_EVENT_CLIENT_MESSAGE, event) < 0)
        {
            MP_VERBOSE(mpctx, "Can't find script '%s' for %s.\n",
                       cmd->args[0].v.s, cmd->name);
        }
        break;
    }
    case MP_CMD_SCRIPT_MESSAGE: {
        const char *args[MP_CMD_MAX_ARGS];
        mpv_event_client_message event = {.args = args};
        for (int n = 0; n < cmd->nargs; n++)
            event.args[event.num_args++] = cmd->args[n].v.s;
        mp_client_broadcast_event(mpctx, MPV_EVENT_CLIENT_MESSAGE, &event);
        break;
    }

#if HAVE_SYS_MMAN_H
    case MP_CMD_OVERLAY_ADD:
        overlay_add(mpctx,
                    cmd->args[0].v.i, cmd->args[1].v.i, cmd->args[2].v.i,
                    cmd->args[3].v.s, cmd->args[4].v.i, cmd->args[5].v.s,
                    cmd->args[6].v.i, cmd->args[7].v.i, cmd->args[8].v.i);
        break;

    case MP_CMD_OVERLAY_REMOVE:
        overlay_remove(mpctx, cmd->args[0].v.i);
        break;
#endif

    case MP_CMD_COMMAND_LIST: {
        for (struct mp_cmd *sub = cmd->args[0].v.p; sub; sub = sub->queue_next)
            run_command(mpctx, sub);
        break;
    }

    case MP_CMD_IGNORE:
        break;

    default:
        MP_VERBOSE(mpctx, "Received unknown cmd %s\n", cmd->name);
    }

    if (cmd->flags & MP_PAUSING)
        pause_player(mpctx, PAUSE_BY_COMMAND);
    if (cmd->flags & MP_PAUSING_TOGGLE) {
        if (opts->pause)
            unpause_player(mpctx, PAUSE_BY_COMMAND);
        else
            pause_player(mpctx, PAUSE_BY_COMMAND);
    }
}

void command_uninit(struct MPContext *mpctx)
{
    overlay_uninit(mpctx);
    talloc_free(mpctx->command_ctx);
    mpctx->command_ctx = NULL;
}

void command_init(struct MPContext *mpctx)
{
    mpctx->command_ctx = talloc(NULL, struct command_ctx);
    *mpctx->command_ctx = (struct command_ctx){
        .last_seek_pts = MP_NOPTS_VALUE,
    };
}

void mp_notify(struct MPContext *mpctx, int event, void *arg)
{
    struct command_ctx *ctx = mpctx->command_ctx;
    if (event == MPV_EVENT_START_FILE)
        ctx->last_seek_pts = MP_NOPTS_VALUE;

    mp_client_broadcast_event(mpctx, event, arg);
    if (event >= 0 && event < MP_ARRAY_SIZE(mp_event_property_change))
        mp_client_property_change(mpctx, mp_event_property_change[event]);
}

void mp_notify_property(struct MPContext *mpctx, char *property)
{
    mp_client_property_change(mpctx, (const char*[]){property, NULL});
}
