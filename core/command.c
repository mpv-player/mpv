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

#include "config.h"
#include "talloc.h"
#include "command.h"
#include "input/input.h"
#include "stream/stream.h"
#include "demux/demux.h"
#include "demux/stheader.h"
#include "mplayer.h"
#include "playlist.h"
#include "playlist_parser.h"
#include "sub/sub.h"
#include "sub/dec_sub.h"
#include "core/m_option.h"
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
#include "core/mp_common.h"
#include "audio/filter/af.h"
#include "video/decode/dec_video.h"
#include "audio/decode/dec_audio.h"
#include "sub/spudec.h"
#include "core/path.h"
#include "sub/ass_mp.h"
#include "stream/tv.h"
#include "stream/stream_radio.h"
#include "stream/pvr.h"
#ifdef CONFIG_DVBIN
#include "stream/dvbin.h"
#endif
#ifdef CONFIG_DVDREAD
#include "stream/stream_dvd.h"
#endif
#include "core/m_struct.h"
#include "screenshot.h"

#include "core/mp_core.h"
#include "mp_fifo.h"
#include "libavutil/avstring.h"

static char *format_bitrate(int rate)
{
    return talloc_asprintf(NULL, "%d kbps", rate * 8 / 1000);
}

static char *format_delay(double time)
{
    return talloc_asprintf(NULL, "%d ms", ROUND(time * 1000));
}

static void rescale_input_coordinates(struct MPContext *mpctx, int ix, int iy,
                                      double *dx, double *dy)
{
    struct MPOpts *opts = &mpctx->opts;
    struct vo *vo = mpctx->video_out;
    //remove the borders, if any, and rescale to the range [0,1],[0,1]
    if (opts->vo.fs) {                //we are in full-screen mode
        if (opts->vo.screenwidth > vo->dwidth)
            // there are borders along the x axis
            ix -= (opts->vo.screenwidth - vo->dwidth) / 2;
        if (opts->vo.screenheight > vo->dheight)
            // there are borders along the y axis (usual way)
            iy -= (opts->vo.screenheight - vo->dheight) / 2;

        if (ix < 0 || ix > vo->dwidth) {
            *dx = *dy = -1.0;
            return;
        }                       //we are on one of the borders
        if (iy < 0 || iy > vo->dheight) {
            *dx = *dy = -1.0;
            return;
        }                       //we are on one of the borders
    }

    *dx = (double) ix / (double) vo->dwidth;
    *dy = (double) iy / (double) vo->dheight;

    mp_msg(MSGT_CPLAYER, MSGL_V,
           "\r\nrescaled coordinates: %.3f, %.3f, screen (%d x %d), vodisplay: (%d, %d), fullscreen: %d\r\n",
           *dx, *dy, opts->vo.screenwidth, opts->vo.screenheight, vo->dwidth,
           vo->dheight, opts->vo.fs);
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
    struct MPOpts *opts = &mpctx->opts;
    double orig_speed = opts->playback_speed;
    switch (action) {
    case M_PROPERTY_SET: {
        opts->playback_speed = *(float *) arg;
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
    char *f = (char *)mp_basename(mpctx->filename);
    return m_property_strdup_ro(prop, action, arg, (*f) ? f : mpctx->filename);
}

static int mp_property_media_title(m_option_t *prop, int action, void *arg,
                                   MPContext *mpctx)
{
    char *name = NULL;
    if (mpctx->resolve_result)
        name = mpctx->resolve_result->title;
    if (name && name[0]) {
        return m_property_strdup_ro(prop, action, arg, name);
    } else {
        return mp_property_filename(prop, action, arg, mpctx);
    }
}

static int mp_property_stream_path(m_option_t *prop, int action, void *arg,
                                   MPContext *mpctx)
{
    struct stream *stream = mpctx->stream;
    if (!stream || !stream->url)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_strdup_ro(prop, action, arg, stream->url);
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

    return m_property_double_ro(prop, action, arg, pts);
}


/// Media length in seconds (RO)
static int mp_property_length(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{
    double len;

    if (!(int) (len = get_time_length(mpctx)))
        return M_PROPERTY_UNAVAILABLE;

    return m_property_double_ro(prop, action, arg, len);
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
        int pos = *(int *)arg;
        queue_seek(mpctx, MPSEEK_FACTOR, pos / 100.0, 0);
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
        *(int *)arg = get_percent_pos(mpctx);
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

    switch (action) {
    case M_PROPERTY_SET:
        queue_seek(mpctx, MPSEEK_ABSOLUTE, *(double *)arg, 0);
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
        *(double *)arg = get_current_time(mpctx);
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
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
        char *chapter_name = chapter_display_name(mpctx, chapter);
        if (!chapter_name)
            return M_PROPERTY_UNAVAILABLE;
        *(char **) arg = chapter_name;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_SET: ;
        int step_all = *(int *)arg - chapter;
        chapter += step_all;
        double next_pts = 0;
        queue_seek(mpctx, MPSEEK_NONE, 0, 0);
        chapter = seek_chapter(mpctx, chapter, &next_pts);
        if (chapter >= 0) {
            if (next_pts > -1.0)
                queue_seek(mpctx, MPSEEK_ABSOLUTE, next_pts, 0);
        } else if (step_all > 0)
            mpctx->stop_play = PT_NEXT_ENTRY;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_edition(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
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

/// Number of titles in file
static int mp_property_titles(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->master_demuxer;
    if (!demuxer)
        return M_PROPERTY_UNAVAILABLE;
    int num_titles = 0;
    stream_control(demuxer->stream, STREAM_CTRL_GET_NUM_TITLES, &num_titles);
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
            struct sh_video *sh_video = demuxer->video->sh;
            if (sh_video)
                resync_video_stream(sh_video);

            struct sh_audio *sh_audio = demuxer->audio->sh;
            if (sh_audio)
                resync_audio_stream(sh_audio);
        }
        return M_PROPERTY_OK;
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

    static const m_option_t key_type =
    {
        "metadata", NULL, CONF_TYPE_STRING, 0, 0, 0, NULL
    };

    switch (action) {
    case M_PROPERTY_GET: {
        char **slist = NULL;
        m_option_copy(prop, &slist, &demuxer->info);
        *(char ***)arg = slist;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_KEY_ACTION: {
        struct m_property_action_arg *ka = arg;
        char *meta = demux_info_get(demuxer, ka->key);
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

static int mp_property_pause(m_option_t *prop, int action, void *arg,
                             void *ctx)
{
    MPContext *mpctx = ctx;

    switch (action) {
    case M_PROPERTY_SET:
        if (*(int *)arg) {
            pause_player(mpctx);
        } else {
            unpause_player(mpctx);
        }
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
        *(int *)arg = mpctx->paused;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
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

/// Volume (RW)
static int mp_property_volume(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{

    if (!mpctx->sh_audio)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_GET:
        mixer_getbothvolume(&mpctx->mixer, arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        mixer_setvolume(&mpctx->mixer, *(float *) arg, *(float *) arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_SWITCH: {
        struct m_property_switch_arg *sarg = arg;
        if (sarg->inc <= 0)
            mixer_decvolume(&mpctx->mixer);
        else
            mixer_incvolume(&mpctx->mixer);
        return M_PROPERTY_OK;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Mute (RW)
static int mp_property_mute(m_option_t *prop, int action, void *arg,
                            MPContext *mpctx)
{

    if (!mpctx->sh_audio)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET:
        mixer_setmute(&mpctx->mixer, *(int *) arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
        *(int *)arg =  mixer_getmute(&mpctx->mixer);
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Audio delay (RW)
static int mp_property_audio_delay(m_option_t *prop, int action,
                                   void *arg, MPContext *mpctx)
{
    if (!(mpctx->sh_audio && mpctx->sh_video))
        return M_PROPERTY_UNAVAILABLE;
    float delay = mpctx->opts.audio_delay;
    switch (action) {
    case M_PROPERTY_PRINT:
        *(char **)arg = format_delay(delay);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        mpctx->audio_delay = mpctx->opts.audio_delay = *(float *)arg;
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
        mixer_getbalance(&mpctx->mixer, arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT: {
        char **str = arg;
        mixer_getbalance(&mpctx->mixer, &bal);
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
        mixer_setbalance(&mpctx->mixer, *(float *)arg);
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
                } else if (!seen || !track) {
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
        *(int *) arg = track ? track->user_tid : -1;
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
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }
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
        mp_switch_track(mpctx, STREAM_AUDIO,
                find_track_by_demuxer_id(mpctx, STREAM_AUDIO, prog.aid));
        mp_switch_track(mpctx, STREAM_VIDEO,
                find_track_by_demuxer_id(mpctx, STREAM_VIDEO, prog.vid));
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
        if (opts->fs == !!*(int *) arg)
            return M_PROPERTY_OK;
        if (mpctx->video_out->config_ok)
            vo_control(mpctx->video_out, VOCTRL_FULLSCREEN, 0);
        mpctx->opts.fullscreen = opts->fs;
        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(prop, action, arg, mpctx);
}

static int mp_property_deinterlace(m_option_t *prop, int action,
                                   void *arg, MPContext *mpctx)
{
    if (!mpctx->sh_video || !mpctx->sh_video->vfilter)
        return M_PROPERTY_UNAVAILABLE;
    vf_instance_t *vf = mpctx->sh_video->vfilter;
    int enabled = 0;
    if (vf->control(vf, VFCTRL_GET_DEINTERLACE, &enabled) != CONTROL_OK)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_GET:
        *(int *)arg = !!enabled;
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        vf->control(vf, VFCTRL_SET_DEINTERLACE, arg);
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int colormatrix_property_helper(m_option_t *prop, int action,
                                      void *arg, MPContext *mpctx)
{
    int r = mp_property_generic_option(prop, action, arg, mpctx);
    if (action == M_PROPERTY_SET) {
        if (mpctx->sh_video)
            set_video_colorspace(mpctx->sh_video);
    }
    return r;
}

static int mp_property_colormatrix(m_option_t *prop, int action, void *arg,
                                   MPContext *mpctx)
{
    if (action != M_PROPERTY_PRINT)
        return colormatrix_property_helper(prop, action, arg, mpctx);

    struct MPOpts *opts = &mpctx->opts;
    struct mp_csp_details actual = { .format = -1 };
    char *req_csp = mp_csp_names[opts->requested_colorspace];
    char *real_csp = NULL;
    if (mpctx->sh_video) {
        struct vf_instance *vf = mpctx->sh_video->vfilter;
        if (vf->control(vf, VFCTRL_GET_YUV_COLORSPACE, &actual) == true) {
            real_csp = mp_csp_names[actual.format];
        } else {
            real_csp = "Unknown";
        }
    }
    char *res;
    if (opts->requested_colorspace == MP_CSP_AUTO && real_csp) {
        // Caveat: doesn't handle the case when the autodetected colorspace
        // is different from the actual colorspace as used by the
        // VO - the OSD will display the VO colorspace without
        // indication that it doesn't match the requested colorspace.
        res = talloc_asprintf(NULL, "Auto (%s)", real_csp);
    } else if (opts->requested_colorspace == actual.format || !real_csp) {
        res = talloc_strdup(NULL, req_csp);
    } else
        res = talloc_asprintf(NULL, mp_gtext("%s, but %s used"),
                                req_csp, real_csp);
    *(char **)arg = res;
    return M_PROPERTY_OK;
}

static int levels_property_helper(int offset, m_option_t *prop, int action,
                                  void *arg, MPContext *mpctx)
{
    if (action != M_PROPERTY_PRINT)
        return colormatrix_property_helper(prop, action, arg, mpctx);

    struct m_option opt = {0};
    mp_property_generic_option(prop, M_PROPERTY_GET_TYPE, &opt, mpctx);
    assert(opt.type);

    int requested = 0;
    mp_property_generic_option(prop, M_PROPERTY_GET, &requested, mpctx);

    struct mp_csp_details actual = {0};
    int actual_level = -1;
    char *req_level = m_option_print(&opt, &requested);
    char *real_level = NULL;
    if (mpctx->sh_video) {
        struct vf_instance *vf = mpctx->sh_video->vfilter;
        if (vf->control(vf, VFCTRL_GET_YUV_COLORSPACE, &actual) == true) {
            actual_level = *(enum mp_csp_levels *)(((char *)&actual) + offset);
            real_level = m_option_print(&opt, &actual_level);
        } else {
            real_level = talloc_strdup(NULL, "Unknown");
        }
    }
    char *res;
    if (requested == MP_CSP_LEVELS_AUTO && real_level) {
        res = talloc_asprintf(NULL, "Auto (%s)", real_level);
    } else if (requested == actual_level || !real_level) {
        res = talloc_strdup(NULL, real_level);
    } else
        res = talloc_asprintf(NULL, mp_gtext("%s, but %s used"),
                                req_level, real_level);
    talloc_free(req_level);
    talloc_free(real_level);
    *(char **)arg = res;
    return M_PROPERTY_OK;
}

static int mp_property_colormatrix_input_range(m_option_t *prop, int action,
                                               void *arg, MPContext *mpctx)
{
    return levels_property_helper(offsetof(struct mp_csp_details, levels_in),
                                  prop, action, arg, mpctx);
}

static int mp_property_colormatrix_output_range(m_option_t *prop, int action,
                                                void *arg, MPContext *mpctx)
{
    return levels_property_helper(offsetof(struct mp_csp_details, levels_out),
                                  prop, action, arg, mpctx);
}

/// Panscan (RW)
static int mp_property_panscan(m_option_t *prop, int action, void *arg,
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
                               &mpctx->opts.vo.ontop, mpctx);
}

/// Show window borders (RW)
static int mp_property_border(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{
    return mp_property_vo_flag(prop, action, arg, VOCTRL_BORDER,
                               &mpctx->opts.vo.border, mpctx);
}

static int mp_property_framedrop(m_option_t *prop, int action,
                                 void *arg, MPContext *mpctx)
{
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;

    return mp_property_generic_option(prop, action, arg, mpctx);
}

/// Color settings, try to use vf/vo then fall back on TV. (RW)
static int mp_property_gamma(m_option_t *prop, int action, void *arg,
                             MPContext *mpctx)
{
    int *gamma = (int *)((char *)&mpctx->opts + prop->offset);
    int r, val;

    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;

    if (gamma[0] == 1000) {
        gamma[0] = 0;
        get_video_colors(mpctx->sh_video, prop->name, gamma);
    }

    switch (action) {
    case M_PROPERTY_SET:
        *gamma = *(int *) arg;
        r = set_video_colors(mpctx->sh_video, prop->name, *gamma);
        if (r <= 0)
            break;
        return r;
    case M_PROPERTY_GET:
        if (get_video_colors(mpctx->sh_video, prop->name, &val) > 0) {
            *(int *)arg = val;
            return M_PROPERTY_OK;
        }
        break;
    default:
        return mp_property_generic_option(prop, action, arg, mpctx);
    }

#ifdef CONFIG_TV
    if (mpctx->sh_video->gsh->demuxer->type == DEMUXER_TYPE_TV) {
        int l = strlen(prop->name);
        char tv_prop[3 + l + 1];
        sprintf(tv_prop, "tv-%s", prop->name);
        return mp_property_do(tv_prop, action, arg, mpctx);
    }
#endif

    return M_PROPERTY_UNAVAILABLE;
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
        mpctx->opts.movie_aspect = f;
        video_reset_aspect(mpctx->sh_video);
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET:
        *(float *)arg = mpctx->sh_video->aspect;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

// For subtitle related properties using the generic option bridge.
// - Fail as unavailable if no video is active
// - Trigger OSD state update when property is set
static int property_sub_helper(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;
    if (action == M_PROPERTY_SET)
        osd_subs_changed(mpctx->osd);
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
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_PRINT:
        *(char **)arg = format_delay(sub_delay);
        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(prop, action, arg, mpctx);
}

static int mp_property_sub_pos(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;
    if (action == M_PROPERTY_PRINT) {
        *(char **)arg = talloc_asprintf(NULL, "%d/100", sub_pos);
        return M_PROPERTY_OK;
    }
    return property_sub_helper(prop, action, arg, mpctx);
}

/// Subtitle visibility (RW)
static int mp_property_sub_visibility(m_option_t *prop, int action,
                                      void *arg, MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;

    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET:
        opts->sub_visibility = *(int *)arg;
        vo_osd_changed(OSDTYPE_SUBTITLE);
        if (vo_spudec)
            vo_osd_changed(OSDTYPE_SPU);
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
        *(int *)arg = opts->sub_visibility;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Show only forced subtitles (RW)
static int mp_property_sub_forced_only(m_option_t *prop, int action,
                                       void *arg, MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;

    if (!vo_spudec)
        return M_PROPERTY_UNAVAILABLE;

    if (action == M_PROPERTY_SET) {
        opts->forced_subs_only = *(int *)arg;
        spudec_set_forced_subs_only(vo_spudec, opts->forced_subs_only);
        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(prop, action, arg, mpctx);
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

// Use option-to-property-bridge. (The property and option have the same names.)
#define M_OPTION_PROPERTY(name) \
    {(name), mp_property_generic_option, &m_option_type_dummy, 0, 0, 0, (name)}

// OPTION_PROPERTY(), but with a custom property handler. The custom handler
// must let unknown operations fall back to mp_property_generic_option().
#define M_OPTION_PROPERTY_CUSTOM(name, handler) \
    {(name), (handler), &m_option_type_dummy, 0, 0, 0, (name)}
#define M_OPTION_PROPERTY_CUSTOM_(name, handler, ...) \
    {(name), (handler), &m_option_type_dummy, 0, 0, 0, (name), __VA_ARGS__}

/// All properties available in MPlayer.
/** \ingroup Properties
 */
static const m_option_t mp_properties[] = {
    // General
    M_OPTION_PROPERTY("osd-level"),
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
    { "percent-pos", mp_property_percent_pos, CONF_TYPE_INT,
      M_OPT_RANGE, 0, 100, NULL },
    { "time-pos", mp_property_time_pos, CONF_TYPE_TIME,
      M_OPT_MIN, 0, 0, NULL },
    { "chapter", mp_property_chapter, CONF_TYPE_INT,
      M_OPT_MIN, 0, 0, NULL },
    M_OPTION_PROPERTY_CUSTOM("edition", mp_property_edition),
    { "titles", mp_property_titles, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "chapters", mp_property_chapters, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "editions", mp_property_editions, CONF_TYPE_INT },
    { "angle", mp_property_angle, CONF_TYPE_INT,
      CONF_RANGE, -2, 10, NULL },
    { "metadata", mp_property_metadata, CONF_TYPE_STRING_LIST,
      0, 0, 0, NULL },
    { "pause", mp_property_pause, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "cache", mp_property_cache, CONF_TYPE_INT },
    M_OPTION_PROPERTY("pts-association-mode"),
    M_OPTION_PROPERTY("hr-seek"),

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
    { "audio", mp_property_audio, CONF_TYPE_INT,
      CONF_RANGE, -2, 65535, NULL },
    { "balance", mp_property_balance, CONF_TYPE_FLOAT,
      M_OPT_RANGE, -1, 1, NULL },

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
    M_OPTION_PROPERTY_CUSTOM_("gamma", mp_property_gamma,
                    .offset = offsetof(struct MPOpts, gamma_gamma)),
    M_OPTION_PROPERTY_CUSTOM_("brightness", mp_property_gamma,
                    .offset = offsetof(struct MPOpts, gamma_brightness)),
    M_OPTION_PROPERTY_CUSTOM_("contrast", mp_property_gamma,
                    .offset = offsetof(struct MPOpts, gamma_contrast)),
    M_OPTION_PROPERTY_CUSTOM_("saturation", mp_property_gamma,
                    .offset = offsetof(struct MPOpts, gamma_saturation)),
    M_OPTION_PROPERTY_CUSTOM_("hue", mp_property_gamma,
                    .offset = offsetof(struct MPOpts, gamma_hue)),
    M_OPTION_PROPERTY_CUSTOM("panscan", mp_property_panscan),
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
    { "video", mp_property_video, CONF_TYPE_INT,
      CONF_RANGE, -2, 65535, NULL },
    { "program", mp_property_program, CONF_TYPE_INT,
      CONF_RANGE, -1, 65535, NULL },

    // Subs
    { "sub", mp_property_sub, CONF_TYPE_INT,
      M_OPT_MIN, -1, 0, NULL },
    M_OPTION_PROPERTY_CUSTOM("sub-delay", mp_property_sub_delay),
    M_OPTION_PROPERTY_CUSTOM("sub-pos", mp_property_sub_pos),
    { "sub-visibility", mp_property_sub_visibility, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    M_OPTION_PROPERTY_CUSTOM("sub-forced-only", mp_property_sub_forced_only),
    M_OPTION_PROPERTY_CUSTOM("sub-scale", property_sub_helper),
#ifdef CONFIG_ASS
    M_OPTION_PROPERTY_CUSTOM("ass-use-margins", property_sub_helper),
    M_OPTION_PROPERTY_CUSTOM("ass-vsfilter-aspect-compat", property_sub_helper),
    M_OPTION_PROPERTY_CUSTOM("ass-style-override", property_sub_helper),
#endif

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
} property_osd_display[] = {
    // general
    { "loop", _("Loop") },
    { "chapter", .seek_msg = OSD_SEEK_INFO_CHAPTER_TEXT,
                 .seek_bar = OSD_SEEK_INFO_BAR },
    { "edition", .seek_msg = OSD_SEEK_INFO_EDITION },
    { "pts-association-mode", "PTS association mode" },
    { "hr-seek", "hr-seek" },
    { "speed", _("Speed") },
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
    struct MPOpts *opts = &mpctx->opts;
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
            if (p) {
                int index = p - property_osd_display;
                osd_id = p->osd_id ? p->osd_id : OSD_MSG_PROPERTY + index;
            }
            set_osd_tmsg(mpctx, osd_id, 1, opts->osd_duration,
                         "%s: %s", osd_name, val);
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

static void show_chapters_on_osd(MPContext *mpctx)
{
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

    set_osd_msg(mpctx, OSD_MSG_TEXT, 1, mpctx->opts.osd_duration, "%s", res);
    talloc_free(res);
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

static void show_tracks_on_osd(MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    char *res = NULL;

    for (int type = 0; type < STREAM_TYPE_COUNT; type++) {
        for (int n = 0; n < mpctx->num_tracks; n++) {
            struct track *track = mpctx->tracks[n];
            if (track->type != type)
                continue;

            bool selected = mpctx->current_track[track->type] == track;
            res = talloc_asprintf_append(res, "%s: ", track_type_name(track->type));
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

    set_osd_msg(mpctx, OSD_MSG_TEXT, 1, opts->osd_duration, "%s", res);
    talloc_free(res);
}

static void show_playlist_on_osd(MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    char *res = NULL;

    for (struct playlist_entry *e = mpctx->playlist->first; e; e = e->next) {
        if (mpctx->playlist->current == e) {
            res = talloc_asprintf_append(res, "> %s <\n", e->filename);
        } else {
            res = talloc_asprintf_append(res, "%s\n", e->filename);
        }
    }

    set_osd_msg(mpctx, OSD_MSG_TEXT, 1, opts->osd_duration, "%s", res);
    talloc_free(res);
}

void run_command(MPContext *mpctx, mp_cmd_t *cmd)
{
    struct MPOpts *opts = &mpctx->opts;
    sh_audio_t *const sh_audio = mpctx->sh_audio;
    sh_video_t *const sh_video = mpctx->sh_video;
    int osd_duration = opts->osd_duration;
    bool auto_osd = cmd->on_osd == MP_ON_OSD_AUTO;
    bool msg_osd = auto_osd || (cmd->on_osd & MP_ON_OSD_MSG);
    bool bar_osd = auto_osd || (cmd->on_osd & MP_ON_OSD_BAR);
    int osdl = msg_osd ? 1 : OSD_LEVEL_INVISIBLE;
    switch (cmd->id) {
    case MP_CMD_SEEK: {
        double v = cmd->args[0].v.d;
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
        if (cmd->args[1].v.f)
            s.inc = cmd->args[1].v.f;
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
        float v = cmd->args[0].v.f;
        v *= mpctx->opts.playback_speed;
        mp_property_do("speed", M_PROPERTY_SET, &v, mpctx);
        show_property_osd(mpctx, "speed", cmd->on_osd);
        break;
    }

    case MP_CMD_FRAME_STEP:
        add_step_frame(mpctx);
        break;

    case MP_CMD_QUIT:
        mpctx->stop_play = PT_QUIT;
        mpctx->quit_player_rc = cmd->args[0].v.i;
        break;

    case MP_CMD_PLAYLIST_NEXT:
    case MP_CMD_PLAYLIST_PREV:
    {
        int dir = cmd->id == MP_CMD_PLAYLIST_PREV ? -1 : +1;
        int force = cmd->args[0].v.i;

        struct playlist_entry *e = mp_next_file(mpctx, dir);
        if (!e && !force)
            break;
        mpctx->playlist->current = e;
        mpctx->playlist->current_was_replaced = false;
        mpctx->stop_play = PT_CURRENT_ENTRY;
        break;
    }

    case MP_CMD_SUB_STEP:
        if (sh_video) {
            int movement = cmd->args[0].v.i;
            struct track *track = mpctx->current_track[STREAM_SUB];
            bool available = false;
            if (track && track->subdata) {
                available = true;
                step_sub(track->subdata, mpctx->video_pts, movement);
            }
#ifdef CONFIG_ASS
            struct ass_track *ass_track = sub_get_ass_track(mpctx->osd);
            if (ass_track) {
                available = true;
                sub_delay += ass_step_sub(ass_track,
                  (mpctx->video_pts + sub_delay) * 1000 + .5, movement) / 1000.;
            }
#endif
            if (available)
                set_osd_tmsg(mpctx, OSD_MSG_SUB_DELAY, osdl, osd_duration,
                             "Sub delay: %d ms", ROUND(sub_delay * 1000));
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
        char *txt = mp_property_expand_string(mpctx, cmd->args[0].v.s);
        if (txt) {
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "%s\n", txt);
            talloc_free(txt);
        }
        break;
    }

    case MP_CMD_SHOW_TEXT: {
        char *txt = mp_property_expand_string(mpctx, cmd->args[0].v.s);
        if (txt) {
            // if no argument supplied use default osd_duration, else <arg> ms.
            set_osd_msg(mpctx, OSD_MSG_TEXT, cmd->args[2].v.i,
                        (cmd->args[1].v.i < 0 ? osd_duration : cmd->args[1].v.i),
                        "%s", txt);
            talloc_free(txt);
        }
        break;
    }

    case MP_CMD_LOADFILE: {
        char *filename = cmd->args[0].v.s;
        bool append = cmd->args[1].v.i;

        if (!append)
            playlist_clear(mpctx->playlist);

        playlist_add(mpctx->playlist, playlist_entry_new(filename));

        if (!append) {
            mpctx->playlist->current = mpctx->playlist->first;
            mpctx->playlist->current_was_replaced = false;
            mpctx->stop_play = PT_CURRENT_ENTRY;
        }
        break;
    }

    case MP_CMD_LOADLIST: {
        char *filename = cmd->args[0].v.s;
        bool append = cmd->args[1].v.i;
        struct playlist *pl = playlist_parse_file(filename);
        if (pl) {
            if (!append)
                playlist_clear(mpctx->playlist);
            playlist_transfer_entries(mpctx->playlist, pl);
            talloc_free(pl);

            if (!append) {
                mpctx->playlist->current = mpctx->playlist->first;
                mpctx->stop_play = PT_CURRENT_ENTRY;
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
                set_osd_tmsg(OSD_MSG_RADIO_CHANNEL, osdl, osd_duration,
                             "Channel: %s",
                             radio_get_channel_name(mpctx->stream));
            }
        }
        break;

    case MP_CMD_RADIO_SET_CHANNEL:
        if (mpctx->stream && mpctx->stream->type == STREAMTYPE_RADIO) {
            radio_set_channel(mpctx->stream, cmd->args[0].v.s);
            if (radio_get_channel_name(mpctx->stream)) {
                set_osd_tmsg(OSD_MSG_RADIO_CHANNEL, osdl, osd_duration,
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
                //vo_osd_changed(OSDTYPE_SUBTITLE);
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
                //vo_osd_changed(OSDTYPE_SUBTITLE);
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
                //vo_osd_changed(OSDTYPE_SUBTITLE);
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
            mp_add_subtitles(mpctx, cmd->args[0].v.s, sh_video->fps, 0);
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
            struct track *nsub = mp_add_subtitles(mpctx, sub->external_filename,
                                                  sh_video->fps, 0);
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

    case MP_CMD_RUN:
#ifndef __MINGW32__
        if (!fork()) {
            char *s = mp_property_expand_string(mpctx, cmd->args[0].v.s);
            if (s) {
                execl("/bin/sh", "sh", "-c", s, NULL);
                talloc_free(s);
            }
            exit(0);
        }
#endif
        break;

    case MP_CMD_KEYDOWN_EVENTS:
        mplayer_put_key(mpctx->key_fifo, cmd->args[0].v.i);
        break;

    case MP_CMD_SET_MOUSE_POS: {
        int pointer_x, pointer_y;
        double dx, dy;
        pointer_x = cmd->args[0].v.i;
        pointer_y = cmd->args[1].v.i;
        rescale_input_coordinates(mpctx, pointer_x, pointer_y, &dx, &dy);
        break;
    }

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

    case MP_CMD_AF_SWITCH:
        if (sh_audio) {
            af_uninit(mpctx->mixer.afilter);
            af_init(mpctx->mixer.afilter);
        }
    case MP_CMD_AF_ADD:
    case MP_CMD_AF_DEL: {
        if (!sh_audio)
            break;
        char *af_args = strdup(cmd->args[0].v.s);
        bstr af_commands = bstr0(af_args);
        struct af_instance *af;
        while (af_commands.len) {
            bstr af_command;
            bstr_split_tok(af_commands, ",", &af_command, &af_commands);
            char *af_command0 = bstrdup0(NULL, af_command);
            if (cmd->id == MP_CMD_AF_DEL) {
                af = af_get(mpctx->mixer.afilter, af_command0);
                if (af != NULL)
                    af_remove(mpctx->mixer.afilter, af);
            } else
                af_add(mpctx->mixer.afilter, af_command0);
            talloc_free(af_command0);
        }
        reinit_audio_chain(mpctx);
        free(af_args);
        break;
    }
    case MP_CMD_AF_CLR:
        if (!sh_audio)
            break;
        af_uninit(mpctx->mixer.afilter);
        af_init(mpctx->mixer.afilter);
        reinit_audio_chain(mpctx);
        break;
    case MP_CMD_AF_CMDLINE:
        if (sh_audio) {
            struct af_instance *af = af_get(sh_audio->afilter, cmd->args[0].v.s);
            if (!af) {
                mp_msg(MSGT_CPLAYER, MSGL_WARN,
                       "Filter '%s' not found in chain.\n", cmd->args[0].v.s);
                break;
            }
            af->control(af, AF_CONTROL_COMMAND_LINE, cmd->args[1].v.s);
            af_reinit(sh_audio->afilter);
        }
        break;
    case MP_CMD_SHOW_CHAPTERS:
        show_chapters_on_osd(mpctx);
        break;
    case MP_CMD_SHOW_TRACKS:
        show_tracks_on_osd(mpctx);
        break;
    case MP_CMD_SHOW_PLAYLIST:
        show_playlist_on_osd(mpctx);
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
        if (mpctx->paused)
            unpause_player(mpctx);
        else
            pause_player(mpctx);
        break;
    }
}
