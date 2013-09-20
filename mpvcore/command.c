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

#include <libavutil/avstring.h>
#include <libavutil/common.h>

#include "config.h"
#include "talloc.h"
#include "command.h"
#include "input/input.h"
#include "stream/stream.h"
#include "demux/demux.h"
#include "demux/stheader.h"
#include "resolve.h"
#include "playlist.h"
#include "playlist_parser.h"
#include "sub/sub.h"
#include "sub/dec_sub.h"
#include "mpvcore/m_option.h"
#include "m_property.h"
#include "m_config.h"
#include "video/filter/vf.h"
#include "video/decode/vd.h"
#include "mp_osd.h"
#include "video/out/vo.h"
#include "video/csputils.h"
#include "playlist.h"
#include "audio/mixer.h"
#include "audio/out/ao.h"
#include "mpvcore/mp_common.h"
#include "audio/filter/af.h"
#include "video/decode/dec_video.h"
#include "audio/decode/dec_audio.h"
#include "mpvcore/path.h"
#include "stream/tv.h"
#include "stream/stream_radio.h"
#include "stream/pvr.h"
#ifdef CONFIG_DVBIN
#include "stream/dvbin.h"
#endif
#ifdef CONFIG_DVDREAD
#include "stream/stream_dvd.h"
#endif
#include "screenshot.h"

#include "mpvcore/mp_core.h"

static int edit_filters(struct MPContext *mpctx, enum stream_type mediatype,
                        const char *cmd, const char *arg);
static int set_filters(struct MPContext *mpctx, enum stream_type mediatype,
                       struct m_obj_settings *new_chain);

static char *format_bitrate(int rate)
{
    return talloc_asprintf(NULL, "%d kbps", rate * 8 / 1000);
}

static char *format_delay(double time)
{
    return talloc_asprintf(NULL, "%d ms", ROUND(time * 1000));
}

// Get current mouse position in OSD coordinate space.
void mp_get_osd_mouse_pos(struct MPContext *mpctx, float *x, float *y)
{
    int wx, wy;
    mp_input_get_mouse_pos(mpctx->input, &wx, &wy);
    float p[2] = {wx, wy};
    // Raw window coordinates (VO mouse events) to OSD resolution.
    if (mpctx->video_out)
        vo_control(mpctx->video_out, VOCTRL_WINDOW_TO_OSD_COORDS, p);
    *x = p[0];
    *y = p[1];
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
        if (mpctx->sh_audio)
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
    if (!mpctx->sh_audio || !mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_double_ro(prop, action, arg, mpctx->last_av_difference);
}

/// Current position in percent (RW)
static int mp_property_percent_pos(m_option_t *prop, int action,
                                   void *arg, MPContext *mpctx)
{
    if (!mpctx->num_sources)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET: ;
        double pos = *(double *)arg;
        queue_seek(mpctx, MPSEEK_FACTOR, pos / 100.0, 0);
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
        *(double *)arg = get_current_pos_ratio(mpctx, false) * 100.0;
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
        *(char **)arg = talloc_asprintf(NULL, "%d", get_percent_pos(mpctx));
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Current position in seconds (RW)
static int mp_property_time_pos(m_option_t *prop, int action,
                                void *arg, MPContext *mpctx)
{
    if (!mpctx->num_sources)
        return M_PROPERTY_UNAVAILABLE;

    if (action == M_PROPERTY_SET) {
        queue_seek(mpctx, MPSEEK_ABSOLUTE, *(double *)arg, 0);
        return M_PROPERTY_OK;
    }
    return property_time(prop, action, arg, get_current_time(mpctx));
}

static int mp_property_remaining(m_option_t *prop, int action,
                                 void *arg, MPContext *mpctx)
{
    double len = get_time_length(mpctx);
    double pos = get_current_time(mpctx);
    double start = get_start_time(mpctx);

    if (!(int)len)
        return M_PROPERTY_UNAVAILABLE;

    return property_time(prop, action, arg, len - (pos - start));
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
                if (current_chapter_start >= 0 &&
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

static int mp_property_list_chapters(m_option_t *prop, int action, void *arg,
                                     MPContext *mpctx)
{
    if (action == M_PROPERTY_GET) {
        int count = get_chapter_count(mpctx);
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
    return M_PROPERTY_NOT_IMPLEMENTED;
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
            .name = prop->name,
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

/// Number of titles in file
static int mp_property_titles(m_option_t *prop, int action, void *arg,
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
            if (mpctx->sh_video)
                resync_video_stream(mpctx->sh_video);

            if (mpctx->sh_audio)
                resync_audio_stream(mpctx->sh_audio);
        }
        return M_PROPERTY_OK;
    case M_PROPERTY_GET_TYPE: {
        struct m_option opt = {
            .name = prop->name,
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

static int tag_property(m_option_t *prop, int action, void *arg,
                        struct mp_tags *tags)
{
    static const m_option_t key_type =
    {
        "tags", NULL, CONF_TYPE_STRING, 0, 0, 0, NULL
    };

    switch (action) {
    case M_PROPERTY_GET: {
        char **slist = NULL;
        int num = 0;
        for (int n = 0; n < tags->num_keys; n++) {
            MP_TARRAY_APPEND(NULL, slist, num, tags->keys[n]);
            MP_TARRAY_APPEND(NULL, slist, num, tags->values[n]);
        }
        MP_TARRAY_APPEND(NULL, slist, num, NULL);
        *(char ***)arg = slist;
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
        char *meta = mp_tags_get_str(tags, ka->key);
        if (!meta)
            return M_PROPERTY_UNKNOWN;
        switch (ka->action) {
        case M_PROPERTY_GET:
            *(char **)ka->arg = talloc_strdup(NULL, meta);
            return M_PROPERTY_OK;
        case M_PROPERTY_GET_TYPE:
            *(struct m_option *)ka->arg = key_type;
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
    if (!demuxer || chapter < 0)
        return M_PROPERTY_UNAVAILABLE;

    assert(chapter < demuxer->num_chapters);

    return tag_property(prop, action, arg, demuxer->chapters[chapter].metadata);
}

static int mp_property_pause(m_option_t *prop, int action, void *arg,
                             void *ctx)
{
    MPContext *mpctx = ctx;

    if (action == M_PROPERTY_SET) {
        if (*(int *)arg) {
            pause_player(mpctx);
        } else {
            unpause_player(mpctx);
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

/// Volume (RW)
static int mp_property_volume(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{
    switch (action) {
    case M_PROPERTY_GET:
        mixer_getbothvolume(mpctx->mixer, arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        mixer_setvolume(mpctx->mixer, *(float *) arg, *(float *) arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_SWITCH: {
        struct m_property_switch_arg *sarg = arg;
        if (sarg->inc <= 0)
            mixer_decvolume(mpctx->mixer);
        else
            mixer_incvolume(mpctx->mixer);
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
    if (!(mpctx->sh_audio && mpctx->sh_video))
        return M_PROPERTY_UNAVAILABLE;
    float delay = mpctx->opts->audio_delay;
    switch (action) {
    case M_PROPERTY_PRINT:
        *(char **)arg = format_delay(delay);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        mpctx->audio_delay = mpctx->opts->audio_delay = *(float *)arg;
        mpctx->delay -= mpctx->audio_delay - delay;
        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(prop, action, arg, mpctx);
}

/// Audio codec tag (RO)
static int mp_property_audio_format(m_option_t *prop, int action,
                                    void *arg, MPContext *mpctx)
{
    const char *c = mpctx->sh_audio ? mpctx->sh_audio->gsh->codec : NULL;
    return m_property_strdup_ro(prop, action, arg, c);
}

/// Audio codec name (RO)
static int mp_property_audio_codec(m_option_t *prop, int action,
                                   void *arg, MPContext *mpctx)
{
    const char *c = mpctx->sh_audio ? mpctx->sh_audio->gsh->decoder_desc : NULL;
    return m_property_strdup_ro(prop, action, arg, c);
}

/// Audio bitrate (RO)
static int mp_property_audio_bitrate(m_option_t *prop, int action,
                                     void *arg, MPContext *mpctx)
{
    if (!mpctx->sh_audio)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_PRINT:
        *(char **)arg = format_bitrate(mpctx->sh_audio->i_bps);
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
        *(int *)arg = mpctx->sh_audio->i_bps;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Samplerate (RO)
static int mp_property_samplerate(m_option_t *prop, int action, void *arg,
                                  MPContext *mpctx)
{
    if (!mpctx->sh_audio)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_PRINT:
        *(char **)arg = talloc_asprintf(NULL, "%d kHz",
                                        mpctx->sh_audio->samplerate / 1000);
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
        *(int *)arg = mpctx->sh_audio->samplerate;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Number of channels (RO)
static int mp_property_channels(m_option_t *prop, int action, void *arg,
                                MPContext *mpctx)
{
    if (!mpctx->sh_audio)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_PRINT:
        *(char **) arg = mp_chmap_to_str(&mpctx->sh_audio->channels);
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
        *(int *)arg = mpctx->sh_audio->channels.num;
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

static struct track* track_next(struct MPContext *mpctx, enum stream_type type,
                                int direction, struct track *track)
{
    assert(direction == -1 || direction == +1);
    struct track *prev = NULL, *next = NULL;
    bool seen = track == NULL;
    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *cur = mpctx->tracks[n];
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
                                 MPContext *mpctx, enum stream_type type)
{
    if (!mpctx->num_sources)
        return M_PROPERTY_UNAVAILABLE;
    struct track *track = mpctx->current_track[type];

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
                lang = mp_gtext("unknown");

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
        mp_switch_track(mpctx, type,
            track_next(mpctx, type, sarg->inc >= 0 ? +1 : -1, track));
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_SET:
        mp_switch_track(mpctx, type, mp_track_by_tid(mpctx, type, *(int *)arg));
        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(prop, action, arg, mpctx);
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
    if (action == M_PROPERTY_GET) {
        char *res = NULL;

        for (int type = 0; type < STREAM_TYPE_COUNT; type++) {
            for (int n = 0; n < mpctx->num_tracks; n++) {
                struct track *track = mpctx->tracks[n];
                if (track->type != type)
                    continue;

                bool selected = mpctx->current_track[track->type] == track;
                res = talloc_asprintf_append(res, "%s: ",
                                             track_type_name(track->type));
                if (selected)
                    res = talloc_asprintf_append(res, "> ");
                res = talloc_asprintf_append(res, "(%d) ", track->user_tid);
                if (track->title)
                    res = talloc_asprintf_append(res, "'%s' ", track->title);
                if (track->lang)
                    res = talloc_asprintf_append(res, "(%s) ", track->lang);
                if (track->is_external)
                    res = talloc_asprintf_append(res, "(external) ");
                if (selected)
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
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Selected audio id (RW)
static int mp_property_audio(m_option_t *prop, int action, void *arg,
                             MPContext *mpctx)
{
    return property_switch_track(prop, action, arg, mpctx, STREAM_AUDIO);
}

/// Selected video id (RW)
static int mp_property_video(m_option_t *prop, int action, void *arg,
                             MPContext *mpctx)
{
    return property_switch_track(prop, action, arg, mpctx, STREAM_VIDEO);
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
            mp_msg(MSGT_CPLAYER, MSGL_ERR,
                   "Selected program contains no audio or video streams!\n");
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
#ifdef CONFIG_VF_LAVFI
    "lavfi=yadif",
#endif
    "yadif",
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
    vf_instance_t *vf = mpctx->sh_video->vfilter;
    int enabled = 0;
    if (vf->control(vf, VFCTRL_GET_DEINTERLACE, &enabled) != CONTROL_OK)
        enabled = -1;
    if (enabled < 0) {
        // vf_lavfi doesn't support VFCTRL_GET_DEINTERLACE
        if (vf_find_by_label(vf, VF_DEINTERLACE_LABEL))
            enabled = 1;
    }
    return enabled;
}

static void set_deinterlacing(struct MPContext *mpctx, bool enable)
{
    vf_instance_t *vf = mpctx->sh_video->vfilter;
    if (vf_find_by_label(vf, VF_DEINTERLACE_LABEL)) {
        if (!enable)
            edit_filters(mpctx, STREAM_VIDEO, "del", "@" VF_DEINTERLACE_LABEL);
    } else {
        if ((get_deinterlacing(mpctx) > 0) != enable) {
            int arg = enable;
            if (vf->control(vf, VFCTRL_SET_DEINTERLACE, &arg) != CONTROL_OK)
                probe_deint_filters(mpctx, "pre");
        }
    }
    mpctx->opts->deinterlace = get_deinterlacing(mpctx) > 0;
}

static int mp_property_deinterlace(m_option_t *prop, int action,
                                   void *arg, MPContext *mpctx)
{
    if (!mpctx->sh_video || !mpctx->sh_video->vfilter)
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
        if (mpctx->sh_video) {
            reinit_video_filters(mpctx);
            mp_force_video_refresh(mpctx);
        }
    }
    return r;
}

static int mp_property_colormatrix(m_option_t *prop, int action, void *arg,
                                   MPContext *mpctx)
{
    if (action != M_PROPERTY_PRINT)
        return video_refresh_property_helper(prop, action, arg, mpctx);

    struct MPOpts *opts = mpctx->opts;

    struct mp_csp_details vo_csp = {0};
    if (mpctx->video_out)
        vo_control(mpctx->video_out, VOCTRL_GET_YUV_COLORSPACE, &vo_csp);

    struct mp_image_params vd_csp = {0};
    if (mpctx->sh_video)
        vd_control(mpctx->sh_video, VDCTRL_GET_PARAMS, &vd_csp);

    char *res = talloc_asprintf(NULL, "%s",
                                mp_csp_names[opts->requested_colorspace]);
    if (!vo_csp.format) {
        res = talloc_asprintf_append(res, " (VO: unknown)");
    } else if (vo_csp.format != opts->requested_colorspace) {
        res = talloc_asprintf_append(res, " (VO: %s)",
                                     mp_csp_names[vo_csp.format]);
    }
    if (!vd_csp.colorspace) {
        res = talloc_asprintf_append(res, " (VD: unknown)");
    } else if (!vo_csp.format || vd_csp.colorspace != vo_csp.format) {
        res = talloc_asprintf_append(res, " (VD: %s)",
                                     mp_csp_names[vd_csp.colorspace]);
    }
    *(char **)arg = res;
    return M_PROPERTY_OK;
}

static int mp_property_colormatrix_input_range(m_option_t *prop, int action,
                                               void *arg, MPContext *mpctx)
{
    if (action != M_PROPERTY_PRINT)
        return video_refresh_property_helper(prop, action, arg, mpctx);

    struct MPOpts *opts = mpctx->opts;

    struct mp_csp_details vo_csp = {0};
    if (mpctx->video_out)
        vo_control(mpctx->video_out, VOCTRL_GET_YUV_COLORSPACE, &vo_csp );

    struct mp_image_params vd_csp = {0};
    if (mpctx->sh_video)
        vd_control(mpctx->sh_video, VDCTRL_GET_PARAMS, &vd_csp);

    char *res = talloc_asprintf(NULL, "%s",
                                mp_csp_levels_names[opts->requested_input_range]);
    if (!vo_csp.levels_in) {
        res = talloc_asprintf_append(res, " (VO: unknown)");
    } else if (vo_csp.levels_in != opts->requested_input_range) {
        res = talloc_asprintf_append(res, " (VO: %s)",
                                     mp_csp_levels_names[vo_csp.levels_in]);
    }
    if (!vd_csp.colorlevels) {
        res = talloc_asprintf_append(res, " (VD: unknown)");
    } else if (!vo_csp.levels_in || vd_csp.colorlevels != vo_csp.levels_in) {
        res = talloc_asprintf_append(res, " (VD: %s)",
                                     mp_csp_levels_names[vd_csp.colorlevels]);
    }
    *(char **)arg = res;
    return M_PROPERTY_OK;
}

static int mp_property_colormatrix_output_range(m_option_t *prop, int action,
                                                void *arg, MPContext *mpctx)
{
    if (action != M_PROPERTY_PRINT)
        return video_refresh_property_helper(prop, action, arg, mpctx);

    struct MPOpts *opts = mpctx->opts;

    int req = opts->requested_output_range;
    struct mp_csp_details actual = {0};
    if (mpctx->video_out)
        vo_control(mpctx->video_out, VOCTRL_GET_YUV_COLORSPACE, &actual);

    char *res = talloc_asprintf(NULL, "%s", mp_csp_levels_names[req]);
    if (!actual.levels_out) {
        res = talloc_asprintf_append(res, " (Actual: unknown)");
    } else if (actual.levels_out != req) {
        res = talloc_asprintf_append(res, " (Actual: %s)",
                                     mp_csp_levels_names[actual.levels_out]);
    }
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
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;

    return mp_property_generic_option(prop, action, arg, mpctx);
}

static int mp_property_video_color(m_option_t *prop, int action, void *arg,
                                   MPContext *mpctx)
{
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET: {
        if (set_video_colors(mpctx->sh_video, prop->name, *(int *) arg) <= 0)
            return M_PROPERTY_UNAVAILABLE;
        break;
    }
    case M_PROPERTY_GET:
        if (get_video_colors(mpctx->sh_video, prop->name, (int *)arg) <= 0)
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
    const char *c = mpctx->sh_video ? mpctx->sh_video->gsh->codec : NULL;
    return m_property_strdup_ro(prop, action, arg, c);
}

/// Video codec name (RO)
static int mp_property_video_codec(m_option_t *prop, int action,
                                   void *arg, MPContext *mpctx)
{
    const char *c = mpctx->sh_video ? mpctx->sh_video->gsh->decoder_desc : NULL;
    return m_property_strdup_ro(prop, action, arg, c);
}


/// Video bitrate (RO)
static int mp_property_video_bitrate(m_option_t *prop, int action,
                                     void *arg, MPContext *mpctx)
{
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;
    if (action == M_PROPERTY_PRINT) {
        *(char **)arg = format_bitrate(mpctx->sh_video->i_bps);
        return M_PROPERTY_OK;
    }
    return m_property_int_ro(prop, action, arg, mpctx->sh_video->i_bps);
}

/// Video display width (RO)
static int mp_property_width(m_option_t *prop, int action, void *arg,
                             MPContext *mpctx)
{
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(prop, action, arg, mpctx->sh_video->disp_w);
}

/// Video display height (RO)
static int mp_property_height(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(prop, action, arg, mpctx->sh_video->disp_h);
}

static int property_vo_wh(m_option_t *prop, int action, void *arg,
                          MPContext *mpctx, bool get_w)
{
    struct vo *vo = mpctx->video_out;
    if (!mpctx->sh_video && !vo || !vo->hasframe)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(prop, action, arg,
                             get_w ? vo->aspdat.prew : vo->aspdat.preh);
}

static int mp_property_dwidth(m_option_t *prop, int action, void *arg,
                                MPContext *mpctx)
{
    return property_vo_wh(prop, action, arg, mpctx, true);
}

static int mp_property_dheight(m_option_t *prop, int action, void *arg,
                                 MPContext *mpctx)
{
    return property_vo_wh(prop, action, arg, mpctx, false);
}

/// Video fps (RO)
static int mp_property_fps(m_option_t *prop, int action, void *arg,
                           MPContext *mpctx)
{
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_float_ro(prop, action, arg, mpctx->sh_video->fps);
}

/// Video aspect (RO)
static int mp_property_aspect(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_SET: {
        float f = *(float *)arg;
        if (f < 0.1)
            f = (float)mpctx->sh_video->disp_w / mpctx->sh_video->disp_h;
        mpctx->opts->movie_aspect = f;
        reinit_video_filters(mpctx);
        mp_force_video_refresh(mpctx);
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET:
        *(float *)arg = mpctx->sh_video->aspect;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

// For OSD and subtitle related properties using the generic option bridge.
// - Fail as unavailable if no video is active
// - Trigger OSD state update when property is set
static int property_osd_helper(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;
    if (action == M_PROPERTY_SET)
        osd_changed_all(mpctx->osd);
    return mp_property_generic_option(prop, action, arg, mpctx);
}

/// Selected subtitles (RW)
static int mp_property_sub(m_option_t *prop, int action, void *arg,
                           MPContext *mpctx)
{
    return property_switch_track(prop, action, arg, mpctx, STREAM_SUB);
}

/// Subtitle delay (RW)
static int mp_property_sub_delay(m_option_t *prop, int action, void *arg,
                                 MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    if (!mpctx->sh_video)
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
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;
    if (action == M_PROPERTY_PRINT) {
        *(char **)arg = talloc_asprintf(NULL, "%d/100", opts->sub_pos);
        return M_PROPERTY_OK;
    }
    return property_osd_helper(prop, action, arg, mpctx);
}

#ifdef CONFIG_TV

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
            .name = prop->name,
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

static int mp_property_playlist_count(m_option_t *prop, int action, void *arg,
                                      MPContext *mpctx)
{
    if (action == M_PROPERTY_GET) {
        *(int *)arg = playlist_entry_count(mpctx->playlist);
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_playlist(m_option_t *prop, int action, void *arg,
                                MPContext *mpctx)
{
    if (action == M_PROPERTY_GET) {
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
    return M_PROPERTY_NOT_IMPLEMENTED;
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
    int r = mp_property_do(real_property, action, arg, mpctx);
    if (action == M_PROPERTY_GET_TYPE && r >= 0) {
        // Fix the property name
        struct m_option *type = arg;
        type->name = prop->name;
    }
    return r;
}

static int mp_property_options(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    if (action != M_PROPERTY_KEY_ACTION)
        return M_PROPERTY_NOT_IMPLEMENTED;

    struct m_property_action_arg *ka = arg;

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
        if (!(mpctx->initialized_flags & INITIALIZED_PLAYBACK) &&
            !(opt->opt->flags & (M_OPT_PRE_PARSE | M_OPT_GLOBAL)))
        {
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
    { "percent-pos", mp_property_percent_pos, CONF_TYPE_DOUBLE,
      M_OPT_RANGE, 0, 100, NULL },
    { "time-pos", mp_property_time_pos, CONF_TYPE_TIME,
      M_OPT_MIN, 0, 0, NULL },
    { "time-remaining", mp_property_remaining, CONF_TYPE_TIME },
    { "chapter", mp_property_chapter, CONF_TYPE_INT,
      M_OPT_MIN, -1, 0, NULL },
    M_OPTION_PROPERTY_CUSTOM("edition", mp_property_edition),
    M_OPTION_PROPERTY_CUSTOM("quvi-format", mp_property_quvi_format),
    { "titles", mp_property_titles, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "chapters", mp_property_chapters, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "editions", mp_property_editions, CONF_TYPE_INT },
    { "angle", mp_property_angle, &m_option_type_dummy },
    { "metadata", mp_property_metadata, CONF_TYPE_STRING_LIST },
    { "chapter-metadata", mp_property_chapter_metadata, CONF_TYPE_STRING_LIST },
    M_OPTION_PROPERTY_CUSTOM("pause", mp_property_pause),
    { "cache", mp_property_cache, CONF_TYPE_INT },
    M_OPTION_PROPERTY("pts-association-mode"),
    M_OPTION_PROPERTY("hr-seek"),
    { "clock", mp_property_clock, CONF_TYPE_STRING,
      0, 0, 0, NULL },

    { "chapter-list", mp_property_list_chapters, CONF_TYPE_STRING },
    { "track-list", property_list_tracks, CONF_TYPE_STRING },

    { "playlist", mp_property_playlist, CONF_TYPE_STRING },
    { "playlist-pos", mp_property_playlist_pos, CONF_TYPE_INT },
    { "playlist-count", mp_property_playlist_count, CONF_TYPE_INT },

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
    { "video-format", mp_property_video_format, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "video-codec", mp_property_video_codec, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "video-bitrate", mp_property_video_bitrate, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "width", mp_property_width, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "height", mp_property_height, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "dwidth", mp_property_dwidth, CONF_TYPE_INT },
    { "dheight", mp_property_dheight, CONF_TYPE_INT },
    { "fps", mp_property_fps, CONF_TYPE_FLOAT,
      0, 0, 0, NULL },
    { "aspect", mp_property_aspect, CONF_TYPE_FLOAT,
      CONF_RANGE, 0, 10, NULL },
    M_OPTION_PROPERTY_CUSTOM("vid", mp_property_video),
    { "program", mp_property_program, CONF_TYPE_INT,
      CONF_RANGE, -1, 65535, NULL },

    // Subs
    M_OPTION_PROPERTY_CUSTOM("sid", mp_property_sub),
    M_OPTION_PROPERTY_CUSTOM("sub-delay", mp_property_sub_delay),
    M_OPTION_PROPERTY_CUSTOM("sub-pos", mp_property_sub_pos),
    M_OPTION_PROPERTY_CUSTOM("sub-visibility", property_osd_helper),
    M_OPTION_PROPERTY_CUSTOM("sub-forced-only", property_osd_helper),
    M_OPTION_PROPERTY_CUSTOM("sub-scale", property_osd_helper),
#ifdef CONFIG_ASS
    M_OPTION_PROPERTY_CUSTOM("ass-use-margins", property_osd_helper),
    M_OPTION_PROPERTY_CUSTOM("ass-vsfilter-aspect-compat", property_osd_helper),
    M_OPTION_PROPERTY_CUSTOM("ass-style-override", property_osd_helper),
#endif

    M_OPTION_PROPERTY_CUSTOM("vf*", mp_property_vf),
    M_OPTION_PROPERTY_CUSTOM("af*", mp_property_af),

#ifdef CONFIG_TV
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

int mp_property_do(const char *name, int action, void *val,
                   struct MPContext *ctx)
{
    return m_property_do(mp_properties, name, action, val, ctx);
}

char *mp_property_expand_string(struct MPContext *mpctx, char *str)
{
    return m_properties_expand_string(mp_properties, str, mpctx);
}

void property_print_help(void)
{
    m_properties_print_help_list(mp_properties);
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
    // osd msg id if it must be shared
    int osd_id;
    // Needs special ways to display the new value (seeks are delayed)
    int seek_msg, seek_bar;
    // Separator between option name and value (default: ": ")
    const char *sep;
} property_osd_display[] = {
    // general
    { "loop", _("Loop") },
    { "chapter", .seek_msg = OSD_SEEK_INFO_CHAPTER_TEXT,
                 .seek_bar = OSD_SEEK_INFO_BAR },
    { "edition", .seek_msg = OSD_SEEK_INFO_EDITION },
    { "pts-association-mode", "PTS association mode" },
    { "hr-seek", "hr-seek" },
    { "speed", _("Speed") },
    { "clock", _("Clock") },
    // audio
    { "volume", _("Volume"), .osd_progbar = OSD_VOLUME },
    { "mute", _("Mute") },
    { "audio-delay", _("A-V delay") },
    { "audio", _("Audio") },
    { "balance", _("Balance"), .osd_progbar = OSD_BALANCE },
    // video
    { "panscan", _("Panscan"), .osd_progbar = OSD_PANSCAN },
    { "ontop", _("Stay on top") },
    { "border", _("Border") },
    { "framedrop", _("Framedrop") },
    { "deinterlace", _("Deinterlace") },
    { "colormatrix", _("YUV colormatrix") },
    { "colormatrix-input-range", _("YUV input range") },
    { "colormatrix-output-range", _("RGB output range") },
    { "gamma", _("Gamma"), .osd_progbar = OSD_BRIGHTNESS },
    { "brightness", _("Brightness"), .osd_progbar = OSD_BRIGHTNESS },
    { "contrast", _("Contrast"), .osd_progbar = OSD_CONTRAST },
    { "saturation", _("Saturation"), .osd_progbar = OSD_SATURATION },
    { "hue", _("Hue"), .osd_progbar = OSD_HUE },
    { "angle", _("Angle") },
    // subs
    { "sub", _("Subtitles") },
    { "sub-pos", _("Sub position") },
    { "sub-delay", _("Sub delay"), .osd_id = OSD_MSG_SUB_DELAY },
    { "sub-visibility", _("Subtitles") },
    { "sub-forced-only", _("Forced sub only") },
    { "sub-scale", _("Sub Scale")},
    { "ass-vsfilter-aspect-compat", _("Subtitle VSFilter aspect compat")},
    { "ass-style-override", _("ASS subtitle style override")},
    { "vf*", _("Video filters"), .sep = ":\n"},
    { "af*", _("Audio filters"), .sep = ":\n"},
#ifdef CONFIG_TV
    { "tv-brightness", _("Brightness"), .osd_progbar = OSD_BRIGHTNESS },
    { "tv-hue", _("Hue"), .osd_progbar = OSD_HUE},
    { "tv-saturation", _("Saturation"), .osd_progbar = OSD_SATURATION },
    { "tv-contrast", _("Contrast"), .osd_progbar = OSD_CONTRAST },
#endif
    {0}
};

static void show_property_osd(MPContext *mpctx, const char *pname,
                              enum mp_on_osd osd_mode)
{
    struct MPOpts *opts = mpctx->opts;
    struct m_option prop = {0};
    struct property_osd_display *p;

    if (mp_property_do(pname, M_PROPERTY_GET_TYPE, &prop, mpctx) <= 0)
        return;

    int osd_progbar = 0;
    const char *osd_name = NULL;

    // look for the command
    for (p = property_osd_display; p->name; p++) {
        if (!strcmp(p->name, prop.name)) {
            osd_progbar = p->seek_bar ? 1 : p->osd_progbar;
            osd_name = p->seek_msg ? "" : mp_gtext(p->osd_name);
            break;
        }
    }
    if (!p->name)
        p = NULL;

    if (osd_mode != MP_ON_OSD_AUTO) {
        osd_name = osd_name ? osd_name : prop.name;
        if (!(osd_mode & MP_ON_OSD_MSG))
            osd_name = NULL;
        osd_progbar = osd_progbar ? osd_progbar : ' ';
        if (!(osd_mode & MP_ON_OSD_BAR))
            osd_progbar = 0;
    }

    if (p && (p->seek_msg || p->seek_bar)) {
        mpctx->add_osd_seek_info |=
            (osd_name ? p->seek_msg : 0) | (osd_progbar ? p->seek_bar : 0);
        return;
    }

    if (osd_progbar && (prop.flags & CONF_RANGE) == CONF_RANGE) {
        bool ok = false;
        if (prop.type == CONF_TYPE_INT) {
            int i;
            ok = mp_property_do(prop.name, M_PROPERTY_GET, &i, mpctx) > 0;
            if (ok)
                set_osd_bar(mpctx, osd_progbar, osd_name, prop.min, prop.max, i);
        } else if (prop.type == CONF_TYPE_FLOAT) {
            float f;
            ok = mp_property_do(prop.name, M_PROPERTY_GET, &f, mpctx) > 0;
            if (ok)
                set_osd_bar(mpctx, osd_progbar, osd_name, prop.min, prop.max, f);
        }
        if (ok && osd_mode == MP_ON_OSD_AUTO && opts->osd_bar_visible)
            return;
    }

    if (osd_name) {
        char *val = NULL;
        int r = mp_property_do(prop.name, M_PROPERTY_PRINT, &val, mpctx);
        if (r == M_PROPERTY_UNAVAILABLE) {
            set_osd_tmsg(mpctx, OSD_MSG_TEXT, 1, opts->osd_duration,
                         "%s: (unavailable)", osd_name);
        } else if (r >= 0 && val) {
            int osd_id = 0;
            const char *sep = NULL;
            if (p) {
                int index = p - property_osd_display;
                osd_id = p->osd_id ? p->osd_id : OSD_MSG_PROPERTY + index;
                sep = p->sep;
            }
            if (!sep)
                sep = ": ";
            set_osd_tmsg(mpctx, osd_id, 1, opts->osd_duration,
                         "%s%s%s", osd_name, sep, val);
            talloc_free(val);
        }
    }
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

    int r = m_option_parse(co->opt, bstr0(optname), bstr0(arg), &new_chain);
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
            set_osd_tmsg(mpctx, OSD_MSG_TEXT, 1, mpctx->opts->osd_duration,
                         "Changing filters failed!");
        }
    }
    return r;
}

void run_command(MPContext *mpctx, mp_cmd_t *cmd)
{
    struct MPOpts *opts = mpctx->opts;
    sh_video_t *const sh_video = mpctx->sh_video;
    int osd_duration = opts->osd_duration;
    bool auto_osd = cmd->on_osd == MP_ON_OSD_AUTO;
    bool msg_osd = auto_osd || (cmd->on_osd & MP_ON_OSD_MSG);
    bool bar_osd = auto_osd || (cmd->on_osd & MP_ON_OSD_BAR);
    int osdl = msg_osd ? 1 : OSD_LEVEL_INVISIBLE;

    if (!cmd->raw_args) {
        for (int n = 0; n < cmd->nargs; n++) {
            if (cmd->args[n].type.type == CONF_TYPE_STRING) {
                cmd->args[n].v.s =
                    mp_property_expand_string(mpctx, cmd->args[n].v.s);
                if (!cmd->args[n].v.s)
                    return;
                talloc_steal(cmd, cmd->args[n].v.s);
            }
        }
    }

    switch (cmd->id) {
    case MP_CMD_SEEK: {
        double v = cmd->args[0].v.d * cmd->scale;
        int abs = cmd->args[1].v.i;
        int exact = cmd->args[2].v.i;
        if (abs == 2) {   // Absolute seek to a timestamp in seconds
            queue_seek(mpctx, MPSEEK_ABSOLUTE, v, exact);
            set_osd_function(mpctx,
                             v > get_current_time(mpctx) ? OSD_FFW : OSD_REW);
        } else if (abs) {           /* Absolute seek by percentage */
            queue_seek(mpctx, MPSEEK_FACTOR, v / 100.0, exact);
            set_osd_function(mpctx, OSD_FFW); // Direction isn't set correctly
        } else {
            queue_seek(mpctx, MPSEEK_RELATIVE, v, exact);
            set_osd_function(mpctx, (v > 0) ? OSD_FFW : OSD_REW);
        }
        if (bar_osd)
            mpctx->add_osd_seek_info |= OSD_SEEK_INFO_BAR;
        if (msg_osd && !(auto_osd && opts->osd_bar_visible))
            mpctx->add_osd_seek_info |= OSD_SEEK_INFO_TEXT;
        break;
    }

    case MP_CMD_SET: {
        int r = mp_property_do(cmd->args[0].v.s, M_PROPERTY_SET_STRING,
                               cmd->args[1].v.s, mpctx);
        if (r == M_PROPERTY_OK || r == M_PROPERTY_UNAVAILABLE) {
            show_property_osd(mpctx, cmd->args[0].v.s, cmd->on_osd);
        } else if (r == M_PROPERTY_UNKNOWN) {
            mp_msg(MSGT_CPLAYER, MSGL_WARN,
                   "Unknown property: '%s'\n", cmd->args[0].v.s);
        } else if (r <= 0) {
            mp_msg(MSGT_CPLAYER, MSGL_WARN,
                   "Failed to set property '%s' to '%s'.\n",
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
        int r = mp_property_do(cmd->args[0].v.s, M_PROPERTY_SWITCH, &s, mpctx);
        if (r == M_PROPERTY_OK || r == M_PROPERTY_UNAVAILABLE) {
            show_property_osd(mpctx, cmd->args[0].v.s, cmd->on_osd);
        } else if (r == M_PROPERTY_UNKNOWN) {
            mp_msg(MSGT_CPLAYER, MSGL_WARN,
                   "Unknown property: '%s'\n", cmd->args[0].v.s);
        } else if (r <= 0) {
            mp_msg(MSGT_CPLAYER, MSGL_WARN,
                   "Failed to increment property '%s' by %g.\n",
                   cmd->args[0].v.s, s.inc);
        }
        break;
    }

    case MP_CMD_GET_PROPERTY: {
        char *tmp;
        int r = mp_property_do(cmd->args[0].v.s, M_PROPERTY_GET_STRING,
                               &tmp, mpctx);
        if (r <= 0) {
            mp_msg(MSGT_CPLAYER, MSGL_WARN,
                   "Failed to get value of property '%s'.\n",
                   cmd->args[0].v.s);
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_ERROR=%s\n",
                   property_error_string(r));
            break;
        }
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_%s=%s\n",
               cmd->args[0].v.s, tmp);
        talloc_free(tmp);
        break;
    }

    case MP_CMD_SPEED_MULT: {
        double v = cmd->args[0].v.d * cmd->scale;
        v *= mpctx->opts->playback_speed;
        mp_property_do("speed", M_PROPERTY_SET, &v, mpctx);
        show_property_osd(mpctx, "speed", cmd->on_osd);
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
        if (mpctx->osd->dec_sub) {
            double a[2];
            a[0] = mpctx->video_pts - mpctx->osd->video_offset + opts->sub_delay;
            a[1] = cmd->args[0].v.i;
            if (sub_control(mpctx->osd->dec_sub, SD_CTRL_SUB_STEP, a) > 0) {
                opts->sub_delay += a[0];

                osd_changed_all(mpctx->osd);
                set_osd_tmsg(mpctx, OSD_MSG_SUB_DELAY, osdl, osd_duration,
                             "Sub delay: %d ms", ROUND(opts->sub_delay * 1000));
            }
        }
        break;

    case MP_CMD_OSD: {
        int v = cmd->args[0].v.i;
        int max = (opts->term_osd
                   && !sh_video) ? MAX_TERM_OSD_LEVEL : MAX_OSD_LEVEL;
        if (opts->osd_level > max)
            opts->osd_level = max;
        if (v < 0)
            opts->osd_level = (opts->osd_level + 1) % (max + 1);
        else
            opts->osd_level = v > max ? max : v;
        if (msg_osd && opts->osd_level <= 1)
            set_osd_tmsg(mpctx, OSD_MSG_OSD_STATUS, 0, osd_duration,
                         "OSD: %s", opts->osd_level ? "yes" : "no");
        else
            rm_osd_msg(mpctx, OSD_MSG_OSD_STATUS);
        break;
    }

    case MP_CMD_PRINT_TEXT: {
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "%s\n", cmd->args[0].v.s);
        break;
    }

    case MP_CMD_SHOW_TEXT: {
        // if no argument supplied use default osd_duration, else <arg> ms.
        set_osd_msg(mpctx, OSD_MSG_TEXT, cmd->args[2].v.i,
                    (cmd->args[1].v.i < 0 ? osd_duration : cmd->args[1].v.i),
                    "%s", cmd->args[0].v.s);
        break;
    }

    case MP_CMD_LOADFILE: {
        char *filename = cmd->args[0].v.s;
        bool append = cmd->args[1].v.i;

        if (!append)
            playlist_clear(mpctx->playlist);

        playlist_add(mpctx->playlist, playlist_entry_new(filename));

        if (!append)
            mp_set_playlist_entry(mpctx, mpctx->playlist->first);
        break;
    }

    case MP_CMD_LOADLIST: {
        char *filename = cmd->args[0].v.s;
        bool append = cmd->args[1].v.i;
        struct playlist *pl = playlist_parse_file(filename, opts);
        if (pl) {
            if (!append)
                playlist_clear(mpctx->playlist);
            playlist_transfer_entries(mpctx->playlist, pl);
            talloc_free(pl);

            if (!append && mpctx->playlist->first) {
                struct playlist_entry *e = mp_resume_playlist(mpctx->playlist);
                mp_set_playlist_entry(mpctx, e ? e : mpctx->playlist->first);
            }
        } else {
            mp_tmsg(MSGT_CPLAYER, MSGL_ERR,
                    "\nUnable to load playlist %s.\n", filename);
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

#ifdef CONFIG_RADIO
    case MP_CMD_RADIO_STEP_CHANNEL:
        if (mpctx->stream && mpctx->stream->type == STREAMTYPE_RADIO) {
            int v = cmd->args[0].v.i;
            if (v > 0)
                radio_step_channel(mpctx->stream, RADIO_CHANNEL_HIGHER);
            else
                radio_step_channel(mpctx->stream, RADIO_CHANNEL_LOWER);
            if (radio_get_channel_name(mpctx->stream)) {
                set_osd_tmsg(mpctx, OSD_MSG_RADIO_CHANNEL, osdl, osd_duration,
                             "Channel: %s",
                             radio_get_channel_name(mpctx->stream));
            }
        }
        break;

    case MP_CMD_RADIO_SET_CHANNEL:
        if (mpctx->stream && mpctx->stream->type == STREAMTYPE_RADIO) {
            radio_set_channel(mpctx->stream, cmd->args[0].v.s);
            if (radio_get_channel_name(mpctx->stream)) {
                set_osd_tmsg(mpctx, OSD_MSG_RADIO_CHANNEL, osdl, osd_duration,
                             "Channel: %s",
                             radio_get_channel_name(mpctx->stream));
            }
        }
        break;

    case MP_CMD_RADIO_SET_FREQ:
        if (mpctx->stream && mpctx->stream->type == STREAMTYPE_RADIO)
            radio_set_freq(mpctx->stream, cmd->args[0].v.f);
        break;

    case MP_CMD_RADIO_STEP_FREQ:
        if (mpctx->stream && mpctx->stream->type == STREAMTYPE_RADIO)
            radio_step_freq(mpctx->stream, cmd->args[0].v.f);
        break;
#endif

#ifdef CONFIG_TV
    case MP_CMD_TV_START_SCAN:
        if (get_tvh(mpctx))
            tv_start_scan(get_tvh(mpctx), 1);
        break;
    case MP_CMD_TV_SET_FREQ:
        if (get_tvh(mpctx))
            tv_set_freq(get_tvh(mpctx), cmd->args[0].v.f * 16.0);
#ifdef CONFIG_PVR
        else if (mpctx->stream && mpctx->stream->type == STREAMTYPE_PVR) {
            pvr_set_freq(mpctx->stream, ROUND(cmd->args[0].v.f));
            set_osd_msg(mpctx, OSD_MSG_TV_CHANNEL, osdl, osd_duration, "%s: %s",
                        pvr_get_current_channelname(mpctx->stream),
                        pvr_get_current_stationname(mpctx->stream));
        }
#endif /* CONFIG_PVR */
        break;

    case MP_CMD_TV_STEP_FREQ:
        if (get_tvh(mpctx))
            tv_step_freq(get_tvh(mpctx), cmd->args[0].v.f * 16.0);
#ifdef CONFIG_PVR
        else if (mpctx->stream && mpctx->stream->type == STREAMTYPE_PVR) {
            pvr_force_freq_step(mpctx->stream, ROUND(cmd->args[0].v.f));
            set_osd_msg(mpctx, OSD_MSG_TV_CHANNEL, osdl, osd_duration, "%s: f %d",
                        pvr_get_current_channelname(mpctx->stream),
                        pvr_get_current_frequency(mpctx->stream));
        }
#endif /* CONFIG_PVR */
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
                set_osd_tmsg(mpctx, OSD_MSG_TV_CHANNEL, osdl, osd_duration,
                             "Channel: %s", tv_channel_current->name);
            }
        }
#ifdef CONFIG_PVR
        else if (mpctx->stream &&
                 mpctx->stream->type == STREAMTYPE_PVR) {
            pvr_set_channel_step(mpctx->stream, cmd->args[0].v.i);
            set_osd_msg(mpctx, OSD_MSG_TV_CHANNEL, osdl, osd_duration, "%s: %s",
                        pvr_get_current_channelname(mpctx->stream),
                        pvr_get_current_stationname(mpctx->stream));
        }
#endif /* CONFIG_PVR */
#ifdef CONFIG_DVBIN
        if (mpctx->stream->type == STREAMTYPE_DVB) {
            int dir;
            int v = cmd->args[0].v.i;

            mpctx->last_dvb_step = v;
            if (v > 0)
                dir = DVB_CHANNEL_HIGHER;
            else
                dir = DVB_CHANNEL_LOWER;


            if (dvb_step_channel(mpctx->stream, dir)) {
                mpctx->stop_play = PT_NEXT_ENTRY;
                mpctx->dvbin_reopen = 1;
            }
        }
#endif /* CONFIG_DVBIN */
        break;

    case MP_CMD_TV_SET_CHANNEL:
        if (get_tvh(mpctx)) {
            tv_set_channel(get_tvh(mpctx), cmd->args[0].v.s);
            if (tv_channel_list) {
                set_osd_tmsg(mpctx, OSD_MSG_TV_CHANNEL, osdl, osd_duration,
                             "Channel: %s", tv_channel_current->name);
            }
        }
#ifdef CONFIG_PVR
        else if (mpctx->stream && mpctx->stream->type == STREAMTYPE_PVR) {
            pvr_set_channel(mpctx->stream, cmd->args[0].v.s);
            set_osd_msg(mpctx, OSD_MSG_TV_CHANNEL, osdl, osd_duration, "%s: %s",
                        pvr_get_current_channelname(mpctx->stream),
                        pvr_get_current_stationname(mpctx->stream));
        }
#endif /* CONFIG_PVR */
        break;

#ifdef CONFIG_DVBIN
    case MP_CMD_DVB_SET_CHANNEL:
        if (mpctx->stream->type == STREAMTYPE_DVB) {
            mpctx->last_dvb_step = 1;

            if (dvb_set_channel(mpctx->stream, cmd->args[1].v.i,
                                cmd->args[0].v.i)) {
                mpctx->stop_play = PT_NEXT_ENTRY;
                mpctx->dvbin_reopen = 1;
            }
        }
        break;
#endif /* CONFIG_DVBIN */

    case MP_CMD_TV_LAST_CHANNEL:
        if (get_tvh(mpctx)) {
            tv_last_channel(get_tvh(mpctx));
            if (tv_channel_list) {
                set_osd_tmsg(mpctx, OSD_MSG_TV_CHANNEL, osdl, osd_duration,
                             "Channel: %s", tv_channel_current->name);
            }
        }
#ifdef CONFIG_PVR
        else if (mpctx->stream && mpctx->stream->type == STREAMTYPE_PVR) {
            pvr_set_lastchannel(mpctx->stream);
            set_osd_msg(mpctx, OSD_MSG_TV_CHANNEL, osdl, osd_duration, "%s: %s",
                        pvr_get_current_channelname(mpctx->stream),
                        pvr_get_current_stationname(mpctx->stream));
        }
#endif /* CONFIG_PVR */
        break;

    case MP_CMD_TV_STEP_NORM:
        if (get_tvh(mpctx))
            tv_step_norm(get_tvh(mpctx));
        break;

    case MP_CMD_TV_STEP_CHANNEL_LIST:
        if (get_tvh(mpctx))
            tv_step_chanlist(get_tvh(mpctx));
        break;
#endif /* CONFIG_TV */

    case MP_CMD_SUB_ADD:
        if (sh_video) {
            mp_add_subtitles(mpctx, cmd->args[0].v.s);
        }
        break;

    case MP_CMD_SUB_REMOVE: {
        struct track *sub = mp_track_by_tid(mpctx, STREAM_SUB, cmd->args[0].v.i);
        if (sub)
            mp_remove_track(mpctx, sub);
        break;
    }

    case MP_CMD_SUB_RELOAD: {
        struct track *sub = mp_track_by_tid(mpctx, STREAM_SUB, cmd->args[0].v.i);
        if (sh_video && sub && sub->is_external && sub->external_filename)
        {
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

    case MP_CMD_RUN:
#ifndef __MINGW32__
        if (!fork()) {
            execl("/bin/sh", "sh", "-c", cmd->args[0].v.s, NULL);
            exit(0);
        }
#endif
        break;

    case MP_CMD_KEYDOWN_EVENTS:
        mp_input_put_key(mpctx->input, cmd->args[0].v.i);
        break;

    case MP_CMD_ENABLE_INPUT_SECTION:
        mp_input_enable_section(mpctx->input, cmd->args[0].v.s,
                                cmd->args[1].v.i == 1 ? MP_INPUT_EXCLUSIVE : 0);
        break;

    case MP_CMD_DISABLE_INPUT_SECTION:
        mp_input_disable_section(mpctx->input, cmd->args[0].v.s);
        break;

    case MP_CMD_VO_CMDLINE:
        if (mpctx->video_out) {
            char *s = cmd->args[0].v.s;
            mp_msg(MSGT_CPLAYER, MSGL_INFO, "Setting vo cmd line to '%s'.\n",
                   s);
            if (vo_control(mpctx->video_out, VOCTRL_SET_COMMAND_LINE, s) > 0) {
                set_osd_msg(mpctx, OSD_MSG_TEXT, osdl, osd_duration, "vo='%s'", s);
            } else {
                set_osd_msg(mpctx, OSD_MSG_TEXT, osdl, osd_duration, "Failed!");
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

    case MP_CMD_COMMAND_LIST: {
        for (struct mp_cmd *sub = cmd->args[0].v.p; sub; sub = sub->queue_next)
            run_command(mpctx, sub);
        break;
    }

    case MP_CMD_IGNORE:
        break;

    default:
        mp_msg(MSGT_CPLAYER, MSGL_V,
               "Received unknown cmd %s\n", cmd->name);
    }

    switch (cmd->pausing) {
    case 1:     // "pausing"
        pause_player(mpctx);
        break;
    case 3:     // "pausing_toggle"
        if (opts->pause)
            unpause_player(mpctx);
        else
            pause_player(mpctx);
        break;
    }
}
