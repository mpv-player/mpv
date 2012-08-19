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
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "codec-cfg.h"
#include "mplayer.h"
#include "playlist.h"
#include "playlist_parser.h"
#include "sub/sub.h"
#include "sub/dec_sub.h"
#include "m_option.h"
#include "m_property.h"
#include "m_config.h"
#include "metadata.h"
#include "libmpcodecs/vf.h"
#include "libmpcodecs/vd.h"
#include "mp_osd.h"
#include "libvo/video_out.h"
#include "libvo/csputils.h"
#include "playlist.h"
#include "libao2/audio_out.h"
#include "mpcommon.h"
#include "mixer.h"
#include "libmpcodecs/dec_video.h"
#include "libmpcodecs/dec_audio.h"
#include "osdep/strsep.h"
#include "sub/vobsub.h"
#include "sub/spudec.h"
#include "path.h"
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
#include "m_struct.h"
#include "screenshot.h"

#include "mp_core.h"
#include "mp_fifo.h"
#include "libavutil/avstring.h"

static void rescale_input_coordinates(struct MPContext *mpctx, int ix, int iy,
                                      double *dx, double *dy)
{
    struct MPOpts *opts = &mpctx->opts;
    struct vo *vo = mpctx->video_out;
    //remove the borders, if any, and rescale to the range [0,1],[0,1]
    if (vo_fs) {                //we are in full-screen mode
        if (opts->vo_screenwidth > vo->dwidth)
            // there are borders along the x axis
            ix -= (opts->vo_screenwidth - vo->dwidth) / 2;
        if (opts->vo_screenheight > vo->dheight)
            // there are borders along the y axis (usual way)
            iy -= (opts->vo_screenheight - vo->dheight) / 2;

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
           *dx, *dy, opts->vo_screenwidth, opts->vo_screenheight, vo->dwidth,
           vo->dheight, vo_fs);
}

static int sub_pos_by_source(MPContext *mpctx, int src)
{
    int i, cnt = 0;
    if (src >= SUB_SOURCES || mpctx->sub_counts[src] == 0)
        return -1;
    for (i = 0; i < src; i++)
        cnt += mpctx->sub_counts[i];
    return cnt;
}

static int sub_source_and_index_by_pos(MPContext *mpctx, int *pos)
{
    int start = 0;
    int i;
    for (i = 0; i < SUB_SOURCES; i++) {
        int cnt = mpctx->sub_counts[i];
        if (*pos >= start && *pos < start + cnt) {
            *pos -= start;
            return i;
        }
        start += cnt;
    }
    *pos = -1;
    return -1;
}

static int sub_source_by_pos(MPContext *mpctx, int pos)
{
    return sub_source_and_index_by_pos(mpctx, &pos);
}

static int sub_source_pos(MPContext *mpctx)
{
    int pos = mpctx->global_sub_pos;
    sub_source_and_index_by_pos(mpctx, &pos);
    return pos;
}

static int sub_source(MPContext *mpctx)
{
    return sub_source_by_pos(mpctx, mpctx->global_sub_pos);
}

static void update_global_sub_size(MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    int i;
    int cnt = 0;

    if (!mpctx->demuxer) {
        mpctx->global_sub_size = -1;
        mpctx->global_sub_pos = -1;
        return;
    }

    // update number of demuxer sub streams
    for (i = 0; i < MAX_S_STREAMS; i++)
        if (mpctx->d_sub->demuxer->s_streams[i])
            cnt++;
    if (cnt > mpctx->sub_counts[SUB_SOURCE_DEMUX])
        mpctx->sub_counts[SUB_SOURCE_DEMUX] = cnt;

    // update global size
    mpctx->global_sub_size = 0;
    for (i = 0; i < SUB_SOURCES; i++)
        mpctx->global_sub_size += mpctx->sub_counts[i];

    // update global_sub_pos if we auto-detected a demuxer sub
    if (mpctx->global_sub_pos == -1) {
        int sub_id = -1;
        if (mpctx->demuxer->sub)
            sub_id = mpctx->demuxer->sub->id;
        if (sub_id < 0)
            sub_id = opts->sub_id;
        if (sub_id >= 0 && sub_id < mpctx->sub_counts[SUB_SOURCE_DEMUX])
            mpctx->global_sub_pos = sub_pos_by_source(mpctx, SUB_SOURCE_DEMUX) +
                                    sub_id;
    }
}

static int mp_property_generic_option(struct m_option *prop, int action,
                                      void *arg, MPContext *mpctx)
{
    char *optname = prop->priv;
    const struct m_option *opt = m_config_get_option(mpctx->mconfig,
                                                     bstr0(optname));
    void *valptr = m_option_get_ptr(opt, &mpctx->opts);

    switch (action) {
    case M_PROPERTY_GET_TYPE:
        *(const struct m_option **)arg = opt;
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
        m_option_copy(opt, arg, valptr);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        m_option_copy(opt, valptr, arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
        if (opt->type == &m_option_type_choice) {
            int v = *(int *) valptr;
            int best = v;
            struct m_opt_choice_alternatives *alt;
            for (alt = opt->priv; alt->name; alt++)
                if ((unsigned) alt->value - v - 1 < (unsigned) best - v - 1)
                    best = alt->value;
            *(int *) valptr = best;
            return M_PROPERTY_OK;
        }
        break;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// OSD level (RW)
static int mp_property_osdlevel(m_option_t *prop, int action, void *arg,
                                MPContext *mpctx)
{
    return m_property_choice(prop, action, arg, &mpctx->opts.osd_level);
}

/// Loop (RW)
static int mp_property_loop(m_option_t *prop, int action, void *arg,
                            MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    switch (action) {
    case M_PROPERTY_PRINT:
        if (!arg)
            return M_PROPERTY_ERROR;
        if (opts->loop_times < 0)
            *(char **)arg = talloc_strdup(NULL, "off");
        else if (opts->loop_times == 0)
            *(char **)arg = talloc_strdup(NULL, "inf");
        else
            break;
        return M_PROPERTY_OK;
    }
    return m_property_int_range(prop, action, arg, &opts->loop_times);
}

/// Playback speed (RW)
static int mp_property_playback_speed(m_option_t *prop, int action,
                                      void *arg, MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    double orig_speed = opts->playback_speed;
    switch (action) {
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        opts->playback_speed = *(float *) arg;
        goto set;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        opts->playback_speed += (arg ? *(float *) arg : 0.1) *
                                (action == M_PROPERTY_STEP_DOWN ? -1 : 1);
    set:
        M_PROPERTY_CLAMP(prop, opts->playback_speed);
        // Adjust time until next frame flip for nosound mode
        mpctx->time_frame *= orig_speed / opts->playback_speed;
        reinit_audio_chain(mpctx);
        return M_PROPERTY_OK;
    }
    return m_property_float_range(prop, action, arg, &opts->playback_speed);
}

/// filename with path (RO)
static int mp_property_path(m_option_t *prop, int action, void *arg,
                            MPContext *mpctx)
{
    return m_property_string_ro(prop, action, arg, mpctx->filename);
}

/// filename without path (RO)
static int mp_property_filename(m_option_t *prop, int action, void *arg,
                                MPContext *mpctx)
{
    char *f;
    if (!mpctx->filename)
        return M_PROPERTY_UNAVAILABLE;
    f = (char *)mp_basename(mpctx->filename);
    if (!*f)
        f = mpctx->filename;
    return m_property_string_ro(prop, action, arg, f);
}

/// Demuxer name (RO)
static int mp_property_demuxer(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->master_demuxer;
    if (!demuxer)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_string_ro(prop, action, arg, (char *)demuxer->desc->name);
}

/// Position in the stream (RW)
static int mp_property_stream_pos(m_option_t *prop, int action, void *arg,
                                  MPContext *mpctx)
{
    struct stream *stream = mpctx->stream;
    if (!stream)
        return M_PROPERTY_UNAVAILABLE;
    if (!arg)
        return M_PROPERTY_ERROR;
    switch (action) {
    case M_PROPERTY_GET:
        *(off_t *) arg = stream_tell(stream);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        M_PROPERTY_CLAMP(prop, *(off_t *) arg);
        stream_seek(stream, *(off_t *) arg);
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
    switch (action) {
    case M_PROPERTY_GET:
        *(off_t *) arg = stream->start_pos;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Stream end offset (RO)
static int mp_property_stream_end(m_option_t *prop, int action, void *arg,
                                  MPContext *mpctx)
{
    struct stream *stream = mpctx->stream;
    if (!stream)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_GET:
        *(off_t *) arg = stream->end_pos;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Stream length (RO)
static int mp_property_stream_length(m_option_t *prop, int action,
                                     void *arg, MPContext *mpctx)
{
    struct stream *stream = mpctx->stream;
    if (!stream)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_GET:
        *(off_t *) arg = stream->end_pos - stream->start_pos;
        return M_PROPERTY_OK;
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

    return m_property_time_ro(prop, action, arg, pts);
}


/// Media length in seconds (RO)
static int mp_property_length(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{
    double len;

    if (!(int) (len = get_time_length(mpctx)))
        return M_PROPERTY_UNAVAILABLE;

    return m_property_time_ro(prop, action, arg, len);
}

/// Current position in percent (RW)
static int mp_property_percent_pos(m_option_t *prop, int action,
                                   void *arg, MPContext *mpctx)
{
    int pos;

    if (!mpctx->num_sources)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(int *)arg);
        pos = *(int *)arg;
        break;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        pos = get_percent_pos(mpctx);
        pos += (arg ? *(int *)arg : 10) *
               (action == M_PROPERTY_STEP_UP ? 1 : -1);
        M_PROPERTY_CLAMP(prop, pos);
        break;
    default:
        return m_property_int_ro(prop, action, arg, get_percent_pos(mpctx));
    }

    queue_seek(mpctx, MPSEEK_FACTOR, pos / 100.0, 0);
    return M_PROPERTY_OK;
}

/// Current position in seconds (RW)
static int mp_property_time_pos(m_option_t *prop, int action,
                                void *arg, MPContext *mpctx)
{
    if (!mpctx->num_sources)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(double *)arg);
        queue_seek(mpctx, MPSEEK_ABSOLUTE, *(double *)arg, 0);
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        queue_seek(mpctx, MPSEEK_RELATIVE, (arg ? *(double *)arg : 10.0) *
                   (action == M_PROPERTY_STEP_UP ? 1.0 : -1.0), 0);
        return M_PROPERTY_OK;
    }
    return m_property_time_ro(prop, action, arg, get_current_time(mpctx));
}

/// Current chapter (RW)
static int mp_property_chapter(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    int step_all;
    char *chapter_name = NULL;

    int chapter = get_current_chapter(mpctx);
    if (chapter < -1)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_GET:
        if (!arg)
            return M_PROPERTY_ERROR;
        *(int *) arg = chapter;
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT: {
        if (!arg)
            return M_PROPERTY_ERROR;
        chapter_name = chapter_display_name(mpctx, chapter);
        if (!chapter_name)
            return M_PROPERTY_UNAVAILABLE;
        *(char **) arg = chapter_name;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(int *)arg);
        step_all = *(int *)arg - chapter;
        chapter += step_all;
        break;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN: {
        step_all = (arg && *(int *)arg != 0 ? *(int *)arg : 1)
                   * (action == M_PROPERTY_STEP_UP ? 1 : -1);
        chapter += step_all;
        if (chapter < 0)
            chapter = 0;
        break;
    }
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }

    double next_pts = 0;
    queue_seek(mpctx, MPSEEK_NONE, 0, 0);
    chapter = seek_chapter(mpctx, chapter, &next_pts);
    if (chapter >= 0) {
        if (next_pts > -1.0)
            queue_seek(mpctx, MPSEEK_ABSOLUTE, next_pts, 0);
        chapter_name = chapter_display_name(mpctx, chapter);
        set_osd_tmsg(mpctx, OSD_MSG_TEXT, 1, opts->osd_duration,
                     "Chapter: %s", chapter_name);
    } else if (step_all > 0)
        mpctx->stop_play = PT_NEXT_ENTRY;
    else
        set_osd_tmsg(mpctx, OSD_MSG_TEXT, 1, opts->osd_duration,
                     "Chapter: (%d) %s", 0, mp_gtext("unknown"));
    talloc_free(chapter_name);
    return M_PROPERTY_OK;
}

/// Number of titles in file
static int mp_property_titles(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->master_demuxer;
    if (!demuxer)
        return M_PROPERTY_UNAVAILABLE;
    if (demuxer->num_titles == 0)
        stream_control(demuxer->stream, STREAM_CTRL_GET_NUM_TITLES,
                       &demuxer->num_titles);
    return m_property_int_ro(prop, action, arg, demuxer->num_titles);
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

/// Current dvd angle (RW)
static int mp_property_angle(m_option_t *prop, int action, void *arg,
                             MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
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
        if (!arg)
            return M_PROPERTY_ERROR;
        *(int *) arg = angle;
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT: {
        if (!arg)
            return M_PROPERTY_ERROR;
        *(char **) arg = talloc_asprintf(NULL, "%d/%d", angle, angles);
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        angle = *(int *)arg;
        M_PROPERTY_CLAMP(prop, angle);
        break;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN: {
        int step = 0;
        if (arg)
            step = *(int *)arg;
        if (!step)
            step = 1;
        step *= (action == M_PROPERTY_STEP_UP ? 1 : -1);
        angle += step;
        if (angle < 1) //cycle
            angle = angles;
        else if (angle > angles)
            angle = 1;
        break;
    }
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }
    angle = demuxer_set_angle(demuxer, angle);
    if (angle >= 0) {
        struct sh_video *sh_video = demuxer->video->sh;
        if (sh_video)
            resync_video_stream(sh_video);

        struct sh_audio *sh_audio = demuxer->audio->sh;
        if (sh_audio)
            resync_audio_stream(sh_audio);
    }

    set_osd_tmsg(mpctx, OSD_MSG_TEXT, 1, opts->osd_duration,
                 "Angle: %d/%d", angle, angles);
    return M_PROPERTY_OK;
}

/// Demuxer meta data
static int mp_property_metadata(m_option_t *prop, int action, void *arg,
                                MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->master_demuxer;
    if (!demuxer)
        return M_PROPERTY_UNAVAILABLE;

    m_property_action_t *ka;
    char *meta;
    static const m_option_t key_type =
    {
        "metadata", NULL, CONF_TYPE_STRING, 0, 0, 0, NULL
    };

    switch (action) {
    case M_PROPERTY_GET:
        if (!arg)
            return M_PROPERTY_ERROR;
        *(char ***)arg = demuxer->info;
        return M_PROPERTY_OK;
    case M_PROPERTY_KEY_ACTION:
        if (!arg)
            return M_PROPERTY_ERROR;
        ka = arg;
        if (!(meta = demux_info_get(demuxer, ka->key)))
            return M_PROPERTY_UNKNOWN;
        switch (ka->action) {
        case M_PROPERTY_GET:
            if (!ka->arg)
                return M_PROPERTY_ERROR;
            *(char **)ka->arg = meta;
            return M_PROPERTY_OK;
        case M_PROPERTY_GET_TYPE:
            if (!ka->arg)
                return M_PROPERTY_ERROR;
            *(const m_option_t **)ka->arg = &key_type;
            return M_PROPERTY_OK;
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
        if (!arg)
            return M_PROPERTY_ERROR;
        if (mpctx->paused == (bool) * (int *)arg)
            return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        if (mpctx->paused) {
            unpause_player(mpctx);
        } else {
            pause_player(mpctx);
        }
        return M_PROPERTY_OK;
    default:
        return m_property_flag(prop, action, arg, &mpctx->paused);
    }
}


/// Volume (RW)
static int mp_property_volume(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{

    if (!mpctx->sh_audio)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_GET:
        if (!arg)
            return M_PROPERTY_ERROR;
        mixer_getbothvolume(&mpctx->mixer, arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT: {
        float vol;
        if (!arg)
            return M_PROPERTY_ERROR;
        mixer_getbothvolume(&mpctx->mixer, &vol);
        return m_property_float_range(prop, action, arg, &vol);
    }
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
    case M_PROPERTY_SET:
        break;
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }

    switch (action) {
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(float *) arg);
        mixer_setvolume(&mpctx->mixer, *(float *) arg, *(float *) arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
        if (arg && *(float *) arg <= 0)
            mixer_decvolume(&mpctx->mixer);
        else
            mixer_incvolume(&mpctx->mixer);
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_DOWN:
        if (arg && *(float *) arg <= 0)
            mixer_incvolume(&mpctx->mixer);
        else
            mixer_decvolume(&mpctx->mixer);
        return M_PROPERTY_OK;
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
        if (!arg)
            return M_PROPERTY_ERROR;
        mixer_setmute(&mpctx->mixer, *(int *) arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        mixer_setmute(&mpctx->mixer, !mixer_getmute(&mpctx->mixer));
        return M_PROPERTY_OK;
    default:
        return m_property_flag_ro(prop, action, arg,
                                  mixer_getmute(&mpctx->mixer));
    }
}

/// Audio delay (RW)
static int mp_property_audio_delay(m_option_t *prop, int action,
                                   void *arg, MPContext *mpctx)
{
    if (!(mpctx->sh_audio && mpctx->sh_video))
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_SET:
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN: {
        int ret;
        float delay = audio_delay;
        ret = m_property_delay(prop, action, arg, &audio_delay);
        if (ret != M_PROPERTY_OK)
            return ret;
        if (mpctx->sh_audio)
            mpctx->delay -= audio_delay - delay;
    }
        return M_PROPERTY_OK;
    default:
        return m_property_delay(prop, action, arg, &audio_delay);
    }
}

/// Audio codec tag (RO)
static int mp_property_audio_format(m_option_t *prop, int action,
                                    void *arg, MPContext *mpctx)
{
    if (!mpctx->sh_audio)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(prop, action, arg, mpctx->sh_audio->format);
}

/// Audio codec name (RO)
static int mp_property_audio_codec(m_option_t *prop, int action,
                                   void *arg, MPContext *mpctx)
{
    if (!mpctx->sh_audio || !mpctx->sh_audio->codec)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_string_ro(prop, action, arg,
                                mpctx->sh_audio->codec->name);
}

/// Audio bitrate (RO)
static int mp_property_audio_bitrate(m_option_t *prop, int action,
                                     void *arg, MPContext *mpctx)
{
    if (!mpctx->sh_audio)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_bitrate(prop, action, arg, mpctx->sh_audio->i_bps);
}

/// Samplerate (RO)
static int mp_property_samplerate(m_option_t *prop, int action, void *arg,
                                  MPContext *mpctx)
{
    if (!mpctx->sh_audio)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_PRINT:
        if (!arg)
            return M_PROPERTY_ERROR;
        *(char **)arg = talloc_asprintf(NULL, "%d kHz",
                                        mpctx->sh_audio->samplerate / 1000);
        return M_PROPERTY_OK;
    }
    return m_property_int_ro(prop, action, arg, mpctx->sh_audio->samplerate);
}

/// Number of channels (RO)
static int mp_property_channels(m_option_t *prop, int action, void *arg,
                                MPContext *mpctx)
{
    if (!mpctx->sh_audio)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_PRINT:
        if (!arg)
            return M_PROPERTY_ERROR;
        switch (mpctx->sh_audio->channels) {
        case 1:
            *(char **) arg = talloc_strdup(NULL, "mono");
            break;
        case 2:
            *(char **) arg = talloc_strdup(NULL, "stereo");
            break;
        default:
            *(char **) arg = talloc_asprintf(NULL, "%d channels",
                                             mpctx->sh_audio->channels);
        }
        return M_PROPERTY_OK;
    }
    return m_property_int_ro(prop, action, arg, mpctx->sh_audio->channels);
}

/// Balance (RW)
static int mp_property_balance(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    float bal;

    switch (action) {
    case M_PROPERTY_GET:
        if (!arg)
            return M_PROPERTY_ERROR;
        mixer_getbalance(&mpctx->mixer, arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT: {
        char **str = arg;
        if (!arg)
            return M_PROPERTY_ERROR;
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
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        mixer_getbalance(&mpctx->mixer, &bal);
        bal += (arg ? *(float *)arg : .1f) *
               (action == M_PROPERTY_STEP_UP ? 1.f : -1.f);
        M_PROPERTY_CLAMP(prop, bal);
        mixer_setbalance(&mpctx->mixer, bal);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(float *)arg);
        mixer_setbalance(&mpctx->mixer, *(float *)arg);
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Selected audio id (RW)
static int mp_property_audio(m_option_t *prop, int action, void *arg,
                             MPContext *mpctx)
{
    int current_id, tmp;
    if (!mpctx->num_sources)
        return M_PROPERTY_UNAVAILABLE;
    struct sh_audio *sh = mpctx->sh_audio;
    current_id = sh ? sh->aid : -2;

    switch (action) {
    case M_PROPERTY_GET:
        if (!arg)
            return M_PROPERTY_ERROR;
        *(int *) arg = current_id;
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
        if (!arg)
            return M_PROPERTY_ERROR;

        if (!sh || current_id < 0)
            *(char **) arg = talloc_strdup(NULL, mp_gtext("disabled"));
        else {
            char *lang = demuxer_stream_lang(sh->ds->demuxer, sh->gsh);
            if (!lang)
                lang = talloc_strdup(NULL, mp_gtext("unknown"));

            if (sh->gsh->title)
                *(char **)arg = talloc_asprintf(NULL, "(%d) %s (\"%s\")",
                                            current_id, lang, sh->gsh->title);
            else
                *(char **)arg = talloc_asprintf(NULL, "(%d) %s", current_id,
                                                lang);

            talloc_free(lang);
        }
        return M_PROPERTY_OK;

    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_SET:
        if (action == M_PROPERTY_SET && arg)
            tmp = *((int *) arg);
        else
            tmp = -1;
        int new_id = demuxer_switch_audio(mpctx->d_audio->demuxer, tmp);
        if (new_id != current_id)
            uninit_player(mpctx, INITIALIZED_AO | INITIALIZED_ACODEC);
        if (new_id != current_id && new_id >= 0) {
            mpctx->opts.audio_id = new_id;
            sh_audio_t *sh2;
            sh2 = mpctx->d_audio->demuxer->a_streams[mpctx->d_audio->id];
            sh2->ds = mpctx->d_audio;
            mpctx->sh_audio = sh2;
            reinit_audio_chain(mpctx);
        }
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AUDIO_TRACK=%d\n", new_id);
        return M_PROPERTY_OK;
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }

}

/// Selected video id (RW)
static int mp_property_video(m_option_t *prop, int action, void *arg,
                             MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    int current_id, tmp;
    if (!mpctx->num_sources)
        return M_PROPERTY_UNAVAILABLE;
    current_id = mpctx->sh_video ? mpctx->sh_video->vid : -2;

    switch (action) {
    case M_PROPERTY_GET:
        if (!arg)
            return M_PROPERTY_ERROR;
        *(int *) arg = current_id;
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
        if (!arg)
            return M_PROPERTY_ERROR;

        if (current_id < 0)
            *(char **) arg = talloc_strdup(NULL, mp_gtext("disabled"));
        else {
            *(char **) arg = talloc_asprintf(NULL, "(%d) %s", current_id,
                                             mp_gtext("unknown"));
        }
        return M_PROPERTY_OK;

    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_SET:
        if (action == M_PROPERTY_SET && arg)
            tmp = *((int *) arg);
        else
            tmp = -1;
        int new_id = demuxer_switch_video(mpctx->d_video->demuxer, tmp);
        if (new_id != current_id)
            uninit_player(mpctx, INITIALIZED_VCODEC |
                          (opts->fixed_vo && new_id >= 0 ? 0 : INITIALIZED_VO));
        if (new_id != current_id && new_id >= 0) {
            sh_video_t *sh2;
            sh2 = mpctx->d_video->demuxer->v_streams[mpctx->d_video->id];
            sh2->ds = mpctx->d_video;
            mpctx->sh_video = sh2;
            reinit_video_chain(mpctx);
        }
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_TRACK=%d\n", new_id);
        return M_PROPERTY_OK;

    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }
}

static int mp_property_program(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    demux_program_t prog;

    struct demuxer *demuxer = mpctx->master_demuxer;
    if (!demuxer)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_STEP_UP:
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
        mp_property_do("switch_audio", M_PROPERTY_SET, &prog.aid, mpctx);
        mp_property_do("switch_video", M_PROPERTY_SET, &prog.vid, mpctx);
        return M_PROPERTY_OK;

    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }
}


/// Fullscreen state (RW)
static int mp_property_fullscreen(m_option_t *prop, int action, void *arg,
                                  MPContext *mpctx)
{

    if (!mpctx->video_out)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(int *) arg);
        if (vo_fs == !!*(int *) arg)
            return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        if (mpctx->video_out->config_ok)
            vo_control(mpctx->video_out, VOCTRL_FULLSCREEN, 0);
        mpctx->opts.fullscreen = vo_fs;
        return M_PROPERTY_OK;
    default:
        return m_property_flag(prop, action, arg, &vo_fs);
    }
}

static int mp_property_deinterlace(m_option_t *prop, int action,
                                   void *arg, MPContext *mpctx)
{
    int deinterlace;
    vf_instance_t *vf;
    if (!mpctx->sh_video || !mpctx->sh_video->vfilter)
        return M_PROPERTY_UNAVAILABLE;
    vf = mpctx->sh_video->vfilter;
    switch (action) {
    case M_PROPERTY_GET:
        if (!arg)
            return M_PROPERTY_ERROR;
        vf->control(vf, VFCTRL_GET_DEINTERLACE, arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(int *) arg);
        vf->control(vf, VFCTRL_SET_DEINTERLACE, arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        vf->control(vf, VFCTRL_GET_DEINTERLACE, &deinterlace);
        deinterlace = !deinterlace;
        vf->control(vf, VFCTRL_SET_DEINTERLACE, &deinterlace);
        return M_PROPERTY_OK;
    }
    int value = 0;
    vf->control(vf, VFCTRL_GET_DEINTERLACE, &value);
    return m_property_flag_ro(prop, action, arg, value);
}

static int colormatrix_property_helper(m_option_t *prop, int action,
                                      void *arg, MPContext *mpctx)
{
    int r = mp_property_generic_option(prop, action, arg, mpctx);
    // testing for an actual change is too much effort
    switch (action) {
    case M_PROPERTY_SET:
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        if (mpctx->sh_video)
            set_video_colorspace(mpctx->sh_video);
        break;
    }
    return r;
}

static int mp_property_colormatrix(m_option_t *prop, int action, void *arg,
                                   MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    switch (action) {
    case M_PROPERTY_PRINT:
        if (!arg)
            return M_PROPERTY_ERROR;
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
    default:;
        return colormatrix_property_helper(prop, action, arg, mpctx);
    }
}

static int levels_property_helper(int offset, m_option_t *prop, int action,
                                  void *arg, MPContext *mpctx)
{
    char *optname = prop->priv;
    const struct m_option *opt = m_config_get_option(mpctx->mconfig,
                                                     bstr0(optname));
    int *valptr = (int *)m_option_get_ptr(opt, &mpctx->opts);

    switch (action) {
    case M_PROPERTY_PRINT:
        if (!arg)
            return M_PROPERTY_ERROR;
        struct mp_csp_details actual = {0};
        int actual_level = -1;
        char *req_level = m_option_print(opt, valptr);
        char *real_level = NULL;
        if (mpctx->sh_video) {
            struct vf_instance *vf = mpctx->sh_video->vfilter;
            if (vf->control(vf, VFCTRL_GET_YUV_COLORSPACE, &actual) == true) {
                actual_level = *(enum mp_csp_levels *)(((char *)&actual) + offset);
                real_level = m_option_print(opt, &actual_level);
            } else {
                real_level = talloc_strdup(NULL, "Unknown");
            }
        }
        char *res;
        if (*valptr == MP_CSP_LEVELS_AUTO && real_level) {
            res = talloc_asprintf(NULL, "Auto (%s)", real_level);
        } else if (*valptr == actual_level || !real_level) {
            res = talloc_strdup(NULL, real_level);
        } else
            res = talloc_asprintf(NULL, mp_gtext("%s, but %s used"),
                                  req_level, real_level);
        talloc_free(req_level);
        talloc_free(real_level);
        *(char **)arg = res;
        return M_PROPERTY_OK;
    default:;
        return colormatrix_property_helper(prop, action, arg, mpctx);
    }
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

    switch (action) {
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(float *) arg);
        vo_panscan = *(float *) arg;
        vo_control(mpctx->video_out, VOCTRL_SET_PANSCAN, NULL);
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        vo_panscan += (arg ? *(float *) arg : 0.1) *
                      (action == M_PROPERTY_STEP_DOWN ? -1 : 1);
        if (vo_panscan > 1)
            vo_panscan = 1;
        else if (vo_panscan < 0)
            vo_panscan = 0;
        vo_control(mpctx->video_out, VOCTRL_SET_PANSCAN, NULL);
        return M_PROPERTY_OK;
    default:
        return m_property_float_range(prop, action, arg, &vo_panscan);
    }
}

/// Helper to set vo flags.
/** \ingroup PropertyImplHelper
 */
static int mp_property_vo_flag(m_option_t *prop, int action, void *arg,
                               int vo_ctrl, int *vo_var, MPContext *mpctx)
{

    if (!mpctx->video_out)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(int *) arg);
        if (*vo_var == !!*(int *) arg)
            return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        if (mpctx->video_out->config_ok)
            vo_control(mpctx->video_out, vo_ctrl, 0);
        return M_PROPERTY_OK;
    default:
        return m_property_flag(prop, action, arg, vo_var);
    }
}

/// Window always on top (RW)
static int mp_property_ontop(m_option_t *prop, int action, void *arg,
                             MPContext *mpctx)
{
    return mp_property_vo_flag(prop, action, arg, VOCTRL_ONTOP,
                               &mpctx->opts.vo_ontop, mpctx);
}

/// Display in the root window (RW)
static int mp_property_rootwin(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    return mp_property_vo_flag(prop, action, arg, VOCTRL_ROOTWIN,
                               &vo_rootwin, mpctx);
}

/// Show window borders (RW)
static int mp_property_border(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{
    return mp_property_vo_flag(prop, action, arg, VOCTRL_BORDER,
                               &vo_border, mpctx);
}

/// Framedropping state (RW)
static int mp_property_framedropping(m_option_t *prop, int action,
                                     void *arg, MPContext *mpctx)
{

    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_PRINT:
        if (!arg)
            return M_PROPERTY_ERROR;
        *(char **) arg = talloc_strdup(NULL, frame_dropping == 1 ?
                                       mp_gtext("enabled") :
                                       (frame_dropping == 2 ? mp_gtext("hard") :
                                        mp_gtext("disabled")));
        return M_PROPERTY_OK;
    default:
        return m_property_choice(prop, action, arg, &frame_dropping);
    }
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
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(int *) arg);
        *gamma = *(int *) arg;
        r = set_video_colors(mpctx->sh_video, prop->name, *gamma);
        if (r <= 0)
            break;
        return r;
    case M_PROPERTY_GET:
        if (get_video_colors(mpctx->sh_video, prop->name, &val) > 0) {
            if (!arg)
                return M_PROPERTY_ERROR;
            *(int *)arg = val;
            return M_PROPERTY_OK;
        }
        break;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        *gamma += (arg ? *(int *) arg : 1) *
                  (action == M_PROPERTY_STEP_DOWN ? -1 : 1);
        M_PROPERTY_CLAMP(prop, *gamma);
        r = set_video_colors(mpctx->sh_video, prop->name, *gamma);
        if (r <= 0)
            break;
        return r;
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }

#ifdef CONFIG_TV
    if (mpctx->sh_video->ds->demuxer->type == DEMUXER_TYPE_TV) {
        int l = strlen(prop->name);
        char tv_prop[3 + l + 1];
        sprintf(tv_prop, "tv_%s", prop->name);
        return mp_property_do(tv_prop, action, arg, mpctx);
    }
#endif

    return M_PROPERTY_UNAVAILABLE;
}

/// VSync (RW)
static int mp_property_vsync(m_option_t *prop, int action, void *arg,
                             MPContext *mpctx)
{
    return m_property_flag(prop, action, arg, &vo_vsync);
}

/// Video codec tag (RO)
static int mp_property_video_format(m_option_t *prop, int action,
                                    void *arg, MPContext *mpctx)
{
    char *meta;
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_PRINT:
        if (!arg)
            return M_PROPERTY_ERROR;
        switch (mpctx->sh_video->format) {
        case 0x10000001:
            meta = talloc_strdup(NULL, "mpeg1");
            break;
        case 0x10000002:
            meta = talloc_strdup(NULL, "mpeg2");
            break;
        case 0x10000004:
            meta = talloc_strdup(NULL, "mpeg4");
            break;
        case 0x10000005:
            meta = talloc_strdup(NULL, "h264");
            break;
        default:
            if (mpctx->sh_video->format >= 0x20202020) {
                meta = talloc_asprintf(NULL, "%.4s",
                                       (char *) &mpctx->sh_video->format);
            } else
                meta = talloc_asprintf(NULL, "0x%08X", mpctx->sh_video->format);
        }
        *(char **)arg = meta;
        return M_PROPERTY_OK;
    }
    return m_property_int_ro(prop, action, arg, mpctx->sh_video->format);
}

/// Video codec name (RO)
static int mp_property_video_codec(m_option_t *prop, int action,
                                   void *arg, MPContext *mpctx)
{
    if (!mpctx->sh_video || !mpctx->sh_video->codec)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_string_ro(prop, action, arg,
                                mpctx->sh_video->codec->name);
}


/// Video bitrate (RO)
static int mp_property_video_bitrate(m_option_t *prop, int action,
                                     void *arg, MPContext *mpctx)
{
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_bitrate(prop, action, arg, mpctx->sh_video->i_bps);
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
    return m_property_float_ro(prop, action, arg, mpctx->sh_video->aspect);
}


/// Text subtitle position (RW)
static int mp_property_sub_pos(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    switch (action) {
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        vo_osd_changed(OSDTYPE_SUBTITLE);
    default:
        return m_property_int_range(prop, action, arg, &sub_pos);
    }
}

/// Selected subtitles (RW)
static int mp_property_sub(m_option_t *prop, int action, void *arg,
                           MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    demux_stream_t *const d_sub = mpctx->d_sub;
    int source = -1, reset_spu av_unused = 0;  // used under CONFIG_DVDREAD
    int source_pos = -1;

    update_global_sub_size(mpctx);
    const int global_sub_size = mpctx->global_sub_size;

    if (global_sub_size <= 0)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_GET:
        if (!arg)
            return M_PROPERTY_ERROR;
        *(int *) arg = mpctx->global_sub_pos;
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
        if (!arg)
            return M_PROPERTY_ERROR;
        char *sub_name = NULL;
        if (mpctx->subdata)
            sub_name = mpctx->subdata->filename;
#ifdef CONFIG_ASS
        if (mpctx->osd->ass_track)
            sub_name = mpctx->osd->ass_track->name;
#endif
        if (!sub_name && mpctx->subdata)
            sub_name = mpctx->subdata->filename;
        if (sub_name) {
            const char *tmp = mp_basename(sub_name);

            *(char **) arg = talloc_asprintf(NULL, "(%d) %s%s",
                                             mpctx->set_of_sub_pos + 1,
                                             strlen(tmp) < 20 ? "" : "...",
                                             strlen(tmp) < 20 ? tmp : tmp + strlen(tmp) - 19);
            return M_PROPERTY_OK;
        }

        if (vo_vobsub && vobsub_id >= 0) {
            const char *language = mp_gtext("unknown");
            language = vobsub_get_id(vo_vobsub, (unsigned int) vobsub_id);
            *(char **) arg = talloc_asprintf(NULL, "(%d) %s",
                                             vobsub_id, language ? language : mp_gtext("unknown"));
            return M_PROPERTY_OK;
        }
        if (opts->sub_id >= 0 && mpctx->d_sub && mpctx->d_sub->sh) {
            struct sh_stream *sh = ((struct sh_sub *)mpctx->d_sub->sh)->gsh;
            char *lang = demuxer_stream_lang(sh->common_header->ds->demuxer, sh);
            if (!lang)
                lang = talloc_strdup(NULL, mp_gtext("unknown"));
            if (sh->title)
                *(char **)arg = talloc_asprintf(NULL, "(%d) %s (\"%s\")",
                                                opts->sub_id, lang, sh->title);
            else
                *(char **) arg = talloc_asprintf(NULL, "(%d) %s", opts->sub_id,
                                                 lang);
            talloc_free(lang);
            return M_PROPERTY_OK;
        }
        *(char **) arg = talloc_strdup(NULL, mp_gtext("disabled"));
        return M_PROPERTY_OK;

    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        if (*(int *) arg < -1)
            *(int *) arg = -1;
        else if (*(int *) arg >= global_sub_size)
            *(int *) arg = global_sub_size - 1;
        mpctx->global_sub_pos = *(int *) arg;
        break;
    case M_PROPERTY_STEP_UP:
        mpctx->global_sub_pos += 2;
        mpctx->global_sub_pos =
            (mpctx->global_sub_pos % (global_sub_size + 1)) - 1;
        break;
    case M_PROPERTY_STEP_DOWN:
        mpctx->global_sub_pos += global_sub_size + 1;
        mpctx->global_sub_pos =
            (mpctx->global_sub_pos % (global_sub_size + 1)) - 1;
        break;
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }

    if (mpctx->global_sub_pos >= 0) {
        source = sub_source(mpctx);
        source_pos = sub_source_pos(mpctx);
    }

    mp_msg(MSGT_CPLAYER, MSGL_DBG3,
           "subtitles: %d subs, (v@%d s@%d d@%d), @%d, source @%d\n",
           global_sub_size,
           mpctx->sub_counts[SUB_SOURCE_VOBSUB],
           mpctx->sub_counts[SUB_SOURCE_SUBS],
           mpctx->sub_counts[SUB_SOURCE_DEMUX],
           mpctx->global_sub_pos, source);

    mpctx->set_of_sub_pos = -1;
    mpctx->subdata = NULL;

    vobsub_id = -1;
    opts->sub_id = -1;
    if (d_sub) {
        if (d_sub->id > -2)
            reset_spu = 1;
        d_sub->id = -2;
    }
    mpctx->osd->ass_track = NULL;
    uninit_player(mpctx, INITIALIZED_SUB);

    if (source == SUB_SOURCE_VOBSUB)
        vobsub_id = vobsub_get_id_by_index(vo_vobsub, source_pos);
    else if (source == SUB_SOURCE_SUBS) {
        mpctx->set_of_sub_pos = source_pos;
#ifdef CONFIG_ASS
        if (opts->ass_enabled
            && mpctx->set_of_ass_tracks[mpctx->set_of_sub_pos]) {
            mpctx->osd->ass_track =
                mpctx->set_of_ass_tracks[mpctx->set_of_sub_pos];
            mpctx->osd->ass_track_changed = true;
            mpctx->osd->vsfilter_aspect =
                mpctx->track_was_native_ass[mpctx->set_of_sub_pos];
        } else
#endif
        {
            mpctx->subdata = mpctx->set_of_subtitles[mpctx->set_of_sub_pos];
            vo_osd_changed(OSDTYPE_SUBTITLE);
        }
    } else if (source == SUB_SOURCE_DEMUX) {
        opts->sub_id = source_pos;
        if (d_sub && opts->sub_id < MAX_S_STREAMS) {
            int i = 0;
            // default: assume 1:1 mapping of sid and stream id
            d_sub->id = opts->sub_id;
            d_sub->sh = mpctx->d_sub->demuxer->s_streams[d_sub->id];
            ds_free_packs(d_sub);
            for (i = 0; i < MAX_S_STREAMS; i++) {
                sh_sub_t *sh = mpctx->d_sub->demuxer->s_streams[i];
                if (sh && sh->sid == opts->sub_id) {
                    d_sub->id = i;
                    d_sub->sh = sh;
                    break;
                }
            }
            if (d_sub->sh && d_sub->id >= 0) {
                sh_sub_t *sh = d_sub->sh;
                if (sh->type == 'v')
                    init_vo_spudec(mpctx);
                else {
                    sub_init(sh, mpctx->osd);
                    mpctx->initialized_flags |= INITIALIZED_SUB;
                }
            } else {
                d_sub->id = -2;
                d_sub->sh = NULL;
            }
        }
    }
#ifdef CONFIG_DVDREAD
    if (vo_spudec && (mpctx->stream->type == STREAMTYPE_DVD)
        && opts->sub_id < 0 && reset_spu)
    {
        d_sub->id = -2;
        d_sub->sh = NULL;
    }
#endif

    update_subtitles(mpctx, 0, true);

    return M_PROPERTY_OK;
}

/// Selected sub source (RW)
static int mp_property_sub_source(m_option_t *prop, int action, void *arg,
                                  MPContext *mpctx)
{
    int source;
    update_global_sub_size(mpctx);
    if (!mpctx->sh_video || mpctx->global_sub_size <= 0)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_GET:
        if (!arg)
            return M_PROPERTY_ERROR;
        *(int *) arg = sub_source(mpctx);
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
        if (!arg)
            return M_PROPERTY_ERROR;
        char *sourcename;
        switch (sub_source(mpctx)) {
        case SUB_SOURCE_SUBS:
            sourcename = mp_gtext("file");
            break;
        case SUB_SOURCE_VOBSUB:
            sourcename = mp_gtext("vobsub");
            break;
        case SUB_SOURCE_DEMUX:
            sourcename = mp_gtext("embedded");
            break;
        default:
            sourcename = mp_gtext("disabled");
        }
        *(char **)arg = talloc_strdup(NULL, sourcename);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(int *)arg);
        if (*(int *) arg < 0)
            mpctx->global_sub_pos = -1;
        else if (*(int *) arg != sub_source(mpctx)) {
            int new_pos = sub_pos_by_source(mpctx, *(int *)arg);
            if (new_pos == -1)
                return M_PROPERTY_UNAVAILABLE;
            mpctx->global_sub_pos = new_pos;
        }
        break;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN: {
        int step_all = (arg && *(int *)arg != 0 ? *(int *)arg : 1)
                       * (action == M_PROPERTY_STEP_UP ? 1 : -1);
        int step = (step_all > 0) ? 1 : -1;
        int cur_source = sub_source(mpctx);
        source = cur_source;
        while (step_all) {
            source += step;
            if (source >= SUB_SOURCES)
                source = -1;
            else if (source < -1)
                source = SUB_SOURCES - 1;
            if (source == cur_source || source == -1 ||
                mpctx->sub_counts[source])
                step_all -= step;
        }
        if (source == cur_source)
            return M_PROPERTY_OK;
        if (source == -1)
            mpctx->global_sub_pos = -1;
        else
            mpctx->global_sub_pos = sub_pos_by_source(mpctx, source);
        break;
    }
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }
    --mpctx->global_sub_pos;
    return mp_property_sub(prop, M_PROPERTY_STEP_UP, NULL, mpctx);
}

/// Selected subtitles from specific source (RW)
static int mp_property_sub_by_type(m_option_t *prop, int action, void *arg,
                                   MPContext *mpctx)
{
    int source, is_cur_source, offset, new_pos;
    update_global_sub_size(mpctx);
    if (!mpctx->sh_video || mpctx->global_sub_size <= 0)
        return M_PROPERTY_UNAVAILABLE;

    if (!strcmp(prop->name, "sub_file"))
        source = SUB_SOURCE_SUBS;
    else if (!strcmp(prop->name, "sub_vob"))
        source = SUB_SOURCE_VOBSUB;
    else if (!strcmp(prop->name, "sub_demux"))
        source = SUB_SOURCE_DEMUX;
    else
        return M_PROPERTY_ERROR;

    offset = sub_pos_by_source(mpctx, source);
    if (offset < 0)
        return M_PROPERTY_UNAVAILABLE;

    is_cur_source = sub_source(mpctx) == source;
    new_pos = mpctx->global_sub_pos;
    switch (action) {
    case M_PROPERTY_GET:
        if (!arg)
            return M_PROPERTY_ERROR;
        if (is_cur_source) {
            *(int *) arg = sub_source_pos(mpctx);
            if (source == SUB_SOURCE_VOBSUB)
                *(int *) arg = vobsub_get_id_by_index(vo_vobsub, *(int *) arg);
        } else
            *(int *) arg = -1;
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
        if (!arg)
            return M_PROPERTY_ERROR;
        if (is_cur_source)
            return mp_property_sub(prop, M_PROPERTY_PRINT, arg, mpctx);
        *(char **) arg = talloc_strdup(NULL, mp_gtext("disabled"));
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        if (*(int *) arg >= 0) {
            int index = *(int *)arg;
            if (source == SUB_SOURCE_VOBSUB)
                index = vobsub_get_index_by_id(vo_vobsub, index);
            new_pos = offset + index;
            if (index < 0 || index > mpctx->sub_counts[source]) {
                new_pos = -1;
                *(int *) arg = -1;
            }
        } else
            new_pos = -1;
        break;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN: {
        int step_all = (arg && *(int *)arg != 0 ? *(int *)arg : 1)
                       * (action == M_PROPERTY_STEP_UP ? 1 : -1);
        int step = (step_all > 0) ? 1 : -1;
        int max_sub_pos_for_source = -1;
        if (!is_cur_source)
            new_pos = -1;
        while (step_all) {
            if (new_pos == -1) {
                if (step > 0)
                    new_pos = offset;
                else if (max_sub_pos_for_source == -1) {
                    // Find max pos for specific source
                    new_pos = mpctx->global_sub_size - 1;
                    while (new_pos >= 0 && sub_source(mpctx) != source)
                        new_pos--;
                } else
                    new_pos = max_sub_pos_for_source;
            } else {
                new_pos += step;
                if (new_pos < offset ||
                    new_pos >= mpctx->global_sub_size ||
                    sub_source(mpctx) != source)
                    new_pos = -1;
            }
            step_all -= step;
        }
        break;
    }
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }
    return mp_property_sub(prop, M_PROPERTY_SET, &new_pos, mpctx);
}

/// Subtitle delay (RW)
static int mp_property_sub_delay(m_option_t *prop, int action, void *arg,
                                 MPContext *mpctx)
{
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_delay(prop, action, arg, &sub_delay);
}

/// Alignment of text subtitles (RW)
static int mp_property_sub_alignment(m_option_t *prop, int action,
                                     void *arg, MPContext *mpctx)
{
    char *name[] = {
        _("top"), _("center"), _("bottom")
    };

    if (!mpctx->sh_video || mpctx->global_sub_pos < 0
        || sub_source(mpctx) != SUB_SOURCE_SUBS)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_PRINT:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, sub_alignment);
        *(char **) arg = talloc_strdup(NULL, mp_gtext(name[sub_alignment]));
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        vo_osd_changed(OSDTYPE_SUBTITLE);
    default:
        return m_property_choice(prop, action, arg, &sub_alignment);
    }
}

/// Subtitle visibility (RW)
static int mp_property_sub_visibility(m_option_t *prop, int action,
                                      void *arg, MPContext *mpctx)
{
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        vo_osd_changed(OSDTYPE_SUBTITLE);
        if (vo_spudec)
            vo_osd_changed(OSDTYPE_SPU);
    default:
        return m_property_flag(prop, action, arg, &sub_visibility);
    }
}

#ifdef CONFIG_ASS
/// Use margins for libass subtitles (RW)
static int mp_property_ass_use_margins(m_option_t *prop, int action,
                                       void *arg, MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        mpctx->osd->ass_force_reload = true;
    default:
        return m_property_flag(prop, action, arg, &opts->ass_use_margins);
    }
}

static int mp_property_ass_vsfilter_aspect_compat(m_option_t *prop, int action,
                                                  void *arg, MPContext *mpctx)
{
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        //has to re-render subs with new aspect ratio
        mpctx->osd->ass_force_reload = 1;
    default:
        return m_property_flag(prop, action, arg,
                               &mpctx->opts.ass_vsfilter_aspect_compat);
    }
}

#endif

/// Show only forced subtitles (RW)
static int mp_property_sub_forced_only(m_option_t *prop, int action,
                                       void *arg, MPContext *mpctx)
{
    if (!vo_spudec)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        m_property_flag(prop, action, arg, &forced_subs_only);
        spudec_set_forced_subs_only(vo_spudec, forced_subs_only);
        return M_PROPERTY_OK;
    default:
        return m_property_flag(prop, action, arg, &forced_subs_only);
    }

}

/// Subtitle scale (RW)
static int mp_property_sub_scale(m_option_t *prop, int action, void *arg,
                                 MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;

    switch (action) {
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(float *) arg);
#ifdef CONFIG_ASS
        if (opts->ass_enabled) {
            opts->ass_font_scale = *(float *) arg;
            mpctx->osd->ass_force_reload = true;
        }
#endif
        text_font_scale_factor = *(float *) arg;
        vo_osd_resized();
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
#ifdef CONFIG_ASS
        if (opts->ass_enabled) {
            opts->ass_font_scale += (arg ? *(float *) arg : 0.1) *
                              (action == M_PROPERTY_STEP_UP ? 1.0 : -1.0);
            M_PROPERTY_CLAMP(prop, opts->ass_font_scale);
            mpctx->osd->ass_force_reload = true;
        }
#endif
        text_font_scale_factor += (arg ? *(float *) arg : 0.1) *
                                  (action == M_PROPERTY_STEP_UP ? 1.0 : -1.0);
        M_PROPERTY_CLAMP(prop, text_font_scale_factor);
        vo_osd_resized();
        return M_PROPERTY_OK;
    default:
#ifdef CONFIG_ASS
        if (opts->ass_enabled)
            return m_property_float_ro(prop, action, arg, opts->ass_font_scale);
        else
#endif
        return m_property_float_ro(prop, action, arg, text_font_scale_factor);
    }
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
    int r, val;
    tvi_handle_t *tvh = get_tvh(mpctx);
    if (!tvh)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(int *) arg);
        return tv_set_color_options(tvh, prop->offset, *(int *) arg);
    case M_PROPERTY_GET:
        return tv_get_color_options(tvh, prop->offset, arg);
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        if ((r = tv_get_color_options(tvh, prop->offset, &val)) >= 0) {
            if (!r)
                return M_PROPERTY_ERROR;
            val += (arg ? *(int *) arg : 1) *
                   (action == M_PROPERTY_STEP_DOWN ? -1 : 1);
            M_PROPERTY_CLAMP(prop, val);
            return tv_set_color_options(tvh, prop->offset, val);
        }
        return M_PROPERTY_ERROR;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

#endif

/// All properties available in MPlayer.
/** \ingroup Properties
 */
static const m_option_t mp_properties[] = {
    // General
    { "osdlevel", mp_property_osdlevel, CONF_TYPE_INT,
      M_OPT_RANGE, 0, 3, NULL },
    { "loop", mp_property_loop, CONF_TYPE_INT,
      M_OPT_MIN, -1, 0, NULL },
    { "speed", mp_property_playback_speed, CONF_TYPE_FLOAT,
      M_OPT_RANGE, 0.01, 100.0, NULL },
    { "filename", mp_property_filename, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "path", mp_property_path, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "demuxer", mp_property_demuxer, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "stream_pos", mp_property_stream_pos, CONF_TYPE_POSITION,
      M_OPT_MIN, 0, 0, NULL },
    { "stream_start", mp_property_stream_start, CONF_TYPE_POSITION,
      M_OPT_MIN, 0, 0, NULL },
    { "stream_end", mp_property_stream_end, CONF_TYPE_POSITION,
      M_OPT_MIN, 0, 0, NULL },
    { "stream_length", mp_property_stream_length, CONF_TYPE_POSITION,
      M_OPT_MIN, 0, 0, NULL },
    { "stream_time_pos", mp_property_stream_time_pos, CONF_TYPE_TIME,
      M_OPT_MIN, 0, 0, NULL },
    { "length", mp_property_length, CONF_TYPE_TIME,
      M_OPT_MIN, 0, 0, NULL },
    { "percent_pos", mp_property_percent_pos, CONF_TYPE_INT,
      M_OPT_RANGE, 0, 100, NULL },
    { "time_pos", mp_property_time_pos, CONF_TYPE_TIME,
      M_OPT_MIN, 0, 0, NULL },
    { "chapter", mp_property_chapter, CONF_TYPE_INT,
      M_OPT_MIN, 0, 0, NULL },
    { "titles", mp_property_titles, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "chapters", mp_property_chapters, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "angle", mp_property_angle, CONF_TYPE_INT,
      CONF_RANGE, -2, 10, NULL },
    { "metadata", mp_property_metadata, CONF_TYPE_STRING_LIST,
      0, 0, 0, NULL },
    { "pause", mp_property_pause, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "pts_association_mode", mp_property_generic_option, &m_option_type_choice,
      0, 0, 0, "pts-association-mode" },
    { "hr_seek", mp_property_generic_option, &m_option_type_choice,
      0, 0, 0, "hr-seek" },

    // Audio
    { "volume", mp_property_volume, CONF_TYPE_FLOAT,
      M_OPT_RANGE, 0, 100, NULL },
    { "mute", mp_property_mute, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "audio_delay", mp_property_audio_delay, CONF_TYPE_FLOAT,
      M_OPT_RANGE, -100, 100, NULL },
    { "audio_format", mp_property_audio_format, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "audio_codec", mp_property_audio_codec, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "audio_bitrate", mp_property_audio_bitrate, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "samplerate", mp_property_samplerate, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "channels", mp_property_channels, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "switch_audio", mp_property_audio, CONF_TYPE_INT,
      CONF_RANGE, -2, 65535, NULL },
    { "balance", mp_property_balance, CONF_TYPE_FLOAT,
      M_OPT_RANGE, -1, 1, NULL },

    // Video
    { "fullscreen", mp_property_fullscreen, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "deinterlace", mp_property_deinterlace, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "colormatrix", mp_property_colormatrix, &m_option_type_choice,
      0, 0, 0, "colormatrix" },
    { "colormatrix_input_range", mp_property_colormatrix_input_range, &m_option_type_choice,
      0, 0, 0, "colormatrix-input-range" },
    { "colormatrix_output_range", mp_property_colormatrix_output_range, &m_option_type_choice,
      0, 0, 0, "colormatrix-output-range" },
    { "ontop", mp_property_ontop, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "rootwin", mp_property_rootwin, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "border", mp_property_border, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "framedropping", mp_property_framedropping, CONF_TYPE_INT,
      M_OPT_RANGE, 0, 2, NULL },
    { "gamma", mp_property_gamma, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, .offset = offsetof(struct MPOpts, vo_gamma_gamma)},
    { "brightness", mp_property_gamma, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, .offset = offsetof(struct MPOpts, vo_gamma_brightness) },
    { "contrast", mp_property_gamma, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, .offset = offsetof(struct MPOpts, vo_gamma_contrast) },
    { "saturation", mp_property_gamma, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, .offset = offsetof(struct MPOpts, vo_gamma_saturation) },
    { "hue", mp_property_gamma, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, .offset = offsetof(struct MPOpts, vo_gamma_hue) },
    { "panscan", mp_property_panscan, CONF_TYPE_FLOAT,
      M_OPT_RANGE, 0, 1, NULL },
    { "vsync", mp_property_vsync, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "video_format", mp_property_video_format, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "video_codec", mp_property_video_codec, CONF_TYPE_STRING,
      0, 0, 0, NULL },
    { "video_bitrate", mp_property_video_bitrate, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "width", mp_property_width, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "height", mp_property_height, CONF_TYPE_INT,
      0, 0, 0, NULL },
    { "fps", mp_property_fps, CONF_TYPE_FLOAT,
      0, 0, 0, NULL },
    { "aspect", mp_property_aspect, CONF_TYPE_FLOAT,
      0, 0, 0, NULL },
    { "switch_video", mp_property_video, CONF_TYPE_INT,
      CONF_RANGE, -2, 65535, NULL },
    { "switch_program", mp_property_program, CONF_TYPE_INT,
      CONF_RANGE, -1, 65535, NULL },

    // Subs
    { "sub", mp_property_sub, CONF_TYPE_INT,
      M_OPT_MIN, -1, 0, NULL },
    { "sub_source", mp_property_sub_source, CONF_TYPE_INT,
      M_OPT_RANGE, -1, SUB_SOURCES - 1, NULL },
    { "sub_vob", mp_property_sub_by_type, CONF_TYPE_INT,
      M_OPT_MIN, -1, 0, NULL },
    { "sub_demux", mp_property_sub_by_type, CONF_TYPE_INT,
      M_OPT_MIN, -1, 0, NULL },
    { "sub_file", mp_property_sub_by_type, CONF_TYPE_INT,
      M_OPT_MIN, -1, 0, NULL },
    { "sub_delay", mp_property_sub_delay, CONF_TYPE_FLOAT,
      0, 0, 0, NULL },
    { "sub_pos", mp_property_sub_pos, CONF_TYPE_INT,
      M_OPT_RANGE, 0, 100, NULL },
    { "sub_alignment", mp_property_sub_alignment, CONF_TYPE_INT,
      M_OPT_RANGE, 0, 2, NULL },
    { "sub_visibility", mp_property_sub_visibility, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "sub_forced_only", mp_property_sub_forced_only, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "sub_scale", mp_property_sub_scale, CONF_TYPE_FLOAT,
      M_OPT_RANGE, 0, 100, NULL },
#ifdef CONFIG_ASS
    { "ass_use_margins", mp_property_ass_use_margins, CONF_TYPE_FLAG,
      M_OPT_RANGE, 0, 1, NULL },
    { "ass_vsfilter_aspect_compat", mp_property_ass_vsfilter_aspect_compat,
      CONF_TYPE_FLAG, M_OPT_RANGE, 0, 1, NULL },
#endif

#ifdef CONFIG_TV
    { "tv_brightness", mp_property_tv_color, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, .offset = TV_COLOR_BRIGHTNESS },
    { "tv_contrast", mp_property_tv_color, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, .offset = TV_COLOR_CONTRAST },
    { "tv_saturation", mp_property_tv_color, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, .offset = TV_COLOR_SATURATION },
    { "tv_hue", mp_property_tv_color, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, .offset = TV_COLOR_HUE },
#endif

    {0},
};


int mp_property_do(const char *name, int action, void *val, void *ctx)
{
    return m_property_do(mp_properties, name, action, val, ctx);
}

char *mp_property_print(const char *name, void *ctx)
{
    char *ret = NULL;
    if (mp_property_do(name, M_PROPERTY_PRINT, &ret, ctx) <= 0)
        return NULL;
    return ret;
}

char *property_expand_string(MPContext *mpctx, char *str)
{
    return m_properties_expand_string(mp_properties, str, mpctx);
}

void property_print_help(void)
{
    m_properties_print_help_list(mp_properties);
}


/* List of default ways to show a property on OSD.
 *
 * Setting osd_progbar to -1 displays seek bar, other nonzero displays
 * a bar showing the current position between min/max values of the
 * property. In this case osd_msg is only used for terminal output
 * if there is no video; it'll be a label shown together with percentage.
 *
 * Otherwise setting osd_msg will show the string on OSD, formatted with
 * the text value of the property as argument.
 */
static struct property_osd_display {
    /// property name
    const char *name;
    /// progressbar type
    int osd_progbar; // -1 is special value for seek indicators
    /// osd msg id if it must be shared
    int osd_id;
    /// osd msg template
    const char *osd_msg;
} property_osd_display[] = {
    // general
    { "loop", 0, -1, _("Loop: %s") },
    { "chapter", -1, -1, NULL },
    { "pts_association_mode", 0, -1, "PTS association mode: %s" },
    { "hr_seek", 0, -1, "hr-seek: %s" },
    { "speed", 0, -1, _("Speed: x %6s") },
    // audio
    { "volume", OSD_VOLUME, -1, _("Volume") },
    { "mute", 0, -1, _("Mute: %s") },
    { "audio_delay", 0, -1, _("A-V delay: %s") },
    { "switch_audio", 0, -1, _("Audio: %s") },
    { "balance", OSD_BALANCE, -1, _("Balance") },
    // video
    { "panscan", OSD_PANSCAN, -1, _("Panscan") },
    { "ontop", 0, -1, _("Stay on top: %s") },
    { "rootwin", 0, -1, _("Rootwin: %s") },
    { "border", 0, -1, _("Border: %s") },
    { "framedropping", 0, -1, _("Framedropping: %s") },
    { "deinterlace", 0, -1, _("Deinterlace: %s") },
    { "colormatrix", 0, -1, _("YUV colormatrix: %s") },
    { "colormatrix_input_range", 0, -1, _("YUV input range: %s") },
    { "colormatrix_output_range", 0, -1, _("RGB output range: %s") },
    { "gamma", OSD_BRIGHTNESS, -1, _("Gamma") },
    { "brightness", OSD_BRIGHTNESS, -1, _("Brightness") },
    { "contrast", OSD_CONTRAST, -1, _("Contrast") },
    { "saturation", OSD_SATURATION, -1, _("Saturation") },
    { "hue", OSD_HUE, -1, _("Hue") },
    { "vsync", 0, -1, _("VSync: %s") },
    // subs
    { "sub", 0, -1, _("Subtitles: %s") },
    { "sub_source", 0, -1, _("Sub source: %s") },
    { "sub_vob", 0, -1, _("Subtitles: %s") },
    { "sub_demux", 0, -1, _("Subtitles: %s") },
    { "sub_file", 0, -1, _("Subtitles: %s") },
    { "sub_pos", 0, -1, _("Sub position: %s/100") },
    { "sub_alignment", 0, -1, _("Sub alignment: %s") },
    { "sub_delay", 0, OSD_MSG_SUB_DELAY, _("Sub delay: %s") },
    { "sub_visibility", 0, -1, _("Subtitles: %s") },
    { "sub_forced_only", 0, -1, _("Forced sub only: %s") },
    { "sub_scale", 0, -1, _("Sub Scale: %s")},
    { "ass_vsfilter_aspect_compat", 0, -1,
      _("Subtitle VSFilter aspect compat: %s")},
#ifdef CONFIG_TV
    { "tv_brightness", OSD_BRIGHTNESS, -1, _("Brightness") },
    { "tv_hue", OSD_HUE, -1, _("Hue") },
    { "tv_saturation", OSD_SATURATION, -1, _("Saturation") },
    { "tv_contrast", OSD_CONTRAST, -1, _("Contrast") },
#endif
    {}
};

static int show_property_osd(MPContext *mpctx, const char *pname)
{
    struct MPOpts *opts = &mpctx->opts;
    int r;
    m_option_t *prop;
    struct property_osd_display *p;

    // look for the command
    for (p = property_osd_display; p->name; p++)
        if (!strcmp(p->name, pname))
            break;

    if (!p->name)
        return -1;

    if (mp_property_do(pname, M_PROPERTY_GET_TYPE, &prop, mpctx) <= 0 || !prop)
        return -1;

    if (p->osd_progbar == -1)
        mpctx->add_osd_seek_info = true;
    else if (p->osd_progbar) {
        if (prop->type == CONF_TYPE_INT) {
            if (mp_property_do(pname, M_PROPERTY_GET, &r, mpctx) > 0)
                set_osd_bar(mpctx, p->osd_progbar, mp_gtext(p->osd_msg),
                            prop->min, prop->max, r);
        } else if (prop->type == CONF_TYPE_FLOAT) {
            float f;
            if (mp_property_do(pname, M_PROPERTY_GET, &f, mpctx) > 0)
                set_osd_bar(mpctx, p->osd_progbar, mp_gtext(p->osd_msg),
                            prop->min, prop->max, f);
        } else {
            mp_msg(MSGT_CPLAYER, MSGL_ERR,
                   "Property use an unsupported type.\n");
            return -1;
        }
        return 0;
    }

    if (p->osd_msg) {
        char *val = mp_property_print(pname, mpctx);
        if (val) {
            int index = p - property_osd_display;
            set_osd_tmsg(mpctx, p->osd_id >= 0 ? p->osd_id : OSD_MSG_PROPERTY + index,
                         1, opts->osd_duration, p->osd_msg, val);
            talloc_free(val);
        }
    }
    return 0;
}


/**
 * Command to property bridge
 *
 * It is used to handle most commands that just set a property
 * and optionally display something on the OSD.
 * Two kinds of commands are handled: adjust or toggle.
 *
 * Adjust commands take 1 or 2 parameters: <value> <abs>
 * If <abs> is non-zero the property is set to the given value
 * otherwise it is adjusted.
 *
 * Toggle commands take 0 or 1 parameters. With no parameter
 * or a value less than the property minimum it just steps the
 * property to its next or previous value respectively.
 * Otherwise it sets it to the given value.
 */

/// List of the commands that can be handled by setting a property.
static struct {
    /// property name
    const char *name;
    /// cmd id
    int cmd;
    /// set/adjust or toggle command
    int toggle;
} set_prop_cmd[] = {
    // general
    { "loop", MP_CMD_LOOP, 0},
    { "chapter", MP_CMD_SEEK_CHAPTER, 0},
    { "angle", MP_CMD_SWITCH_ANGLE, 0},
    { "pause", MP_CMD_PAUSE, 0},
    // audio
    { "volume", MP_CMD_VOLUME, 0},
    { "mute", MP_CMD_MUTE, 1},
    { "audio_delay", MP_CMD_AUDIO_DELAY, 0},
    { "switch_audio", MP_CMD_SWITCH_AUDIO, 1},
    { "balance", MP_CMD_BALANCE, 0},
    // video
    { "fullscreen", MP_CMD_VO_FULLSCREEN, 1},
    { "panscan", MP_CMD_PANSCAN, 0},
    { "ontop", MP_CMD_VO_ONTOP, 1},
    { "rootwin", MP_CMD_VO_ROOTWIN, 1},
    { "border", MP_CMD_VO_BORDER, 1},
    { "framedropping", MP_CMD_FRAMEDROPPING, 1},
    { "gamma", MP_CMD_GAMMA, 0},
    { "brightness", MP_CMD_BRIGHTNESS, 0},
    { "contrast", MP_CMD_CONTRAST, 0},
    { "saturation", MP_CMD_SATURATION, 0},
    { "hue", MP_CMD_HUE, 0},
    { "vsync", MP_CMD_SWITCH_VSYNC, 1},
    // subs
    { "sub", MP_CMD_SUB_SELECT, 1},
    { "sub_source", MP_CMD_SUB_SOURCE, 1},
    { "sub_vob", MP_CMD_SUB_VOB, 1},
    { "sub_demux", MP_CMD_SUB_DEMUX, 1},
    { "sub_file", MP_CMD_SUB_FILE, 1},
    { "sub_pos", MP_CMD_SUB_POS, 0},
    { "sub_alignment", MP_CMD_SUB_ALIGNMENT, 1},
    { "sub_delay", MP_CMD_SUB_DELAY, 0},
    { "sub_visibility", MP_CMD_SUB_VISIBILITY, 1},
    { "sub_forced_only", MP_CMD_SUB_FORCED_ONLY, 1},
    { "sub_scale", MP_CMD_SUB_SCALE, 0},
#ifdef CONFIG_ASS
    { "ass_use_margins", MP_CMD_ASS_USE_MARGINS, 1},
#endif
#ifdef CONFIG_TV
    { "tv_brightness", MP_CMD_TV_SET_BRIGHTNESS, 0},
    { "tv_hue", MP_CMD_TV_SET_HUE, 0},
    { "tv_saturation", MP_CMD_TV_SET_SATURATION, 0},
    { "tv_contrast", MP_CMD_TV_SET_CONTRAST, 0},
#endif
    {}
};

/// Handle commands that set a property.
static bool set_property_command(MPContext *mpctx, mp_cmd_t *cmd)
{
    int i, r;
    m_option_t *prop;
    const char *pname;

    // look for the command
    for (i = 0; set_prop_cmd[i].name; i++)
        if (set_prop_cmd[i].cmd == cmd->id)
            break;
    if (!(pname = set_prop_cmd[i].name))
        return 0;

    if (mp_property_do(pname, M_PROPERTY_GET_TYPE, &prop, mpctx) <= 0 || !prop)
        return 0;

    // toggle command
    if (set_prop_cmd[i].toggle) {
        // set to value
        if (cmd->nargs > 0 && cmd->args[0].v.i >= prop->min)
            r = mp_property_do(pname, M_PROPERTY_SET, &cmd->args[0].v.i, mpctx);
        else if (cmd->nargs > 0)
            r = mp_property_do(pname, M_PROPERTY_STEP_DOWN, NULL, mpctx);
        else
            r = mp_property_do(pname, M_PROPERTY_STEP_UP, NULL, mpctx);
    } else if (cmd->args[1].v.i)        //set
        r = mp_property_do(pname, M_PROPERTY_SET, &cmd->args[0].v, mpctx);
    else                        // adjust
        r = mp_property_do(pname, M_PROPERTY_STEP_UP, &cmd->args[0].v, mpctx);

    if (r <= 0)
        return 1;

    show_property_osd(mpctx, pname);

    return 1;
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
    case M_PROPERTY_DISABLED:
        return "DISABLED";
    }
    return "UNKNOWN";
}

static char *format_time(double time)
{
    int h, m, s = time;
    h = s / 3600;
    s -= h * 3600;
    m = s / 60;
    s -= m * 60;
    return talloc_asprintf(NULL, "%02d:%02d:%02d", h, m, s);
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
        char* time = format_time(t);
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

static void show_tracks_on_osd(MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    demuxer_t *demuxer = mpctx->master_demuxer;
    char *res = NULL;

    if (!demuxer)
        return;

    struct sh_stream *cur_a = mpctx->sh_audio ? mpctx->sh_audio->gsh : NULL;
    struct sh_stream *cur_s = NULL;
    if (opts->sub_id >= 0 && mpctx->d_sub && mpctx->d_sub->sh)
        cur_s = ((struct sh_sub *)mpctx->d_sub->sh)->gsh;

    int v_count = 0;
    enum stream_type t = STREAM_AUDIO;

    for (int n = 0; n < demuxer->num_streams; n++) {
        struct sh_stream *sh = demuxer->streams[n];
        if (sh->type == STREAM_VIDEO) {
            v_count++;
            continue;
        }
        if (t != sh->type)
            res = talloc_asprintf_append(res, "\n");
        bool selected = sh == cur_a || sh == cur_s;
        res = talloc_asprintf_append(res, "%s: ",
                                sh->type == STREAM_AUDIO ? "Audio" : "Sub");
        if (selected)
            res = talloc_asprintf_append(res, "> ");
        res = talloc_asprintf_append(res, "(%d) ", sh->tid);
        if (sh->title)
            res = talloc_asprintf_append(res, "'%s' ", sh->title);
        char *lang = demuxer_stream_lang(sh->common_header->ds->demuxer, sh);
        if (lang)
            res = talloc_asprintf_append(res, "(%s) ", lang);
        talloc_free(lang);
        if (selected)
            res = talloc_asprintf_append(res, "<");
        res = talloc_asprintf_append(res, "\n");
        t = sh->type;
    }

    if (v_count > 1)
        res = talloc_asprintf_append(res, "\n(Warning: more than one video stream.)\n");

    set_osd_msg(mpctx, OSD_MSG_TEXT, 1, opts->osd_duration, "%s", res);
    talloc_free(res);
}

void run_command(MPContext *mpctx, mp_cmd_t *cmd)
{
    struct MPOpts *opts = &mpctx->opts;
    sh_audio_t *const sh_audio = mpctx->sh_audio;
    sh_video_t *const sh_video = mpctx->sh_video;
    int osd_duration = opts->osd_duration;
    int case_fallthrough_hack = 0;
    if (set_property_command(mpctx, cmd))
        goto old_pause_hack;  // was handled already
    switch (cmd->id) {
    case MP_CMD_SEEK: {
        mpctx->add_osd_seek_info = true;
        float v = cmd->args[0].v.f;
        int abs = (cmd->nargs > 1) ? cmd->args[1].v.i : 0;
        int exact = (cmd->nargs > 2) ? cmd->args[2].v.i : 0;
        if (abs == 2) {   // Absolute seek to a timestamp in seconds
            queue_seek(mpctx, MPSEEK_ABSOLUTE, v, exact);
            mpctx->osd_function = v > get_current_time(mpctx) ?
                                  OSD_FFW : OSD_REW;
        } else if (abs) {           /* Absolute seek by percentage */
            queue_seek(mpctx, MPSEEK_FACTOR, v / 100.0, exact);
            mpctx->osd_function = OSD_FFW; // Direction isn't set correctly
        } else {
            queue_seek(mpctx, MPSEEK_RELATIVE, v, exact);
            mpctx->osd_function = (v > 0) ? OSD_FFW : OSD_REW;
        }
        break;
    }

    case MP_CMD_SET_PROPERTY_OSD:
        case_fallthrough_hack = 1;

    case MP_CMD_SET_PROPERTY: {
        int r = mp_property_do(cmd->args[0].v.s, M_PROPERTY_PARSE,
                               cmd->args[1].v.s, mpctx);
        if (r == M_PROPERTY_UNKNOWN)
            mp_msg(MSGT_CPLAYER, MSGL_WARN,
                   "Unknown property: '%s'\n", cmd->args[0].v.s);
        else if (r <= 0)
            mp_msg(MSGT_CPLAYER, MSGL_WARN,
                   "Failed to set property '%s' to '%s'.\n",
                   cmd->args[0].v.s, cmd->args[1].v.s);
        else if (case_fallthrough_hack)
            show_property_osd(mpctx, cmd->args[0].v.s);
        if (r <= 0)
            mp_msg(MSGT_GLOBAL, MSGL_INFO,
                   "ANS_ERROR=%s\n", property_error_string(r));
        break;
    }

    case MP_CMD_STEP_PROPERTY_OSD:
        case_fallthrough_hack = 1;

    case MP_CMD_STEP_PROPERTY: {
        void *arg = NULL;
        int r, i;
        double d;
        off_t o;
        if (cmd->args[1].v.f) {
            m_option_t *prop;
            if ((r = mp_property_do(cmd->args[0].v.s,
                                    M_PROPERTY_GET_TYPE,
                                    &prop, mpctx)) <= 0)
                goto step_prop_err;
            if (prop->type == CONF_TYPE_INT ||
                prop->type == CONF_TYPE_FLAG)
                i = cmd->args[1].v.f, arg = &i;
            else if (prop->type == CONF_TYPE_FLOAT)
                arg = &cmd->args[1].v.f;
            else if (prop->type == CONF_TYPE_DOUBLE ||
                     prop->type == CONF_TYPE_TIME)
                d = cmd->args[1].v.f, arg = &d;
            else if (prop->type == CONF_TYPE_POSITION)
                o = cmd->args[1].v.f, arg = &o;
            else
                mp_msg(MSGT_CPLAYER, MSGL_WARN,
                       "Ignoring step size stepping property '%s'.\n",
                       cmd->args[0].v.s);
        }
        r = mp_property_do(cmd->args[0].v.s,
                           cmd->args[2].v.i < 0 ?
                           M_PROPERTY_STEP_DOWN : M_PROPERTY_STEP_UP,
                           arg, mpctx);
      step_prop_err:
        if (r == M_PROPERTY_UNKNOWN)
            mp_msg(MSGT_CPLAYER, MSGL_WARN,
                   "Unknown property: '%s'\n", cmd->args[0].v.s);
        else if (r <= 0)
            mp_msg(MSGT_CPLAYER, MSGL_WARN,
                   "Failed to increment property '%s' by %f.\n",
                   cmd->args[0].v.s, cmd->args[1].v.f);
        else if (case_fallthrough_hack)
            show_property_osd(mpctx, cmd->args[0].v.s);
        if (r <= 0)
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_ERROR=%s\n",
                   property_error_string(r));
        break;
    }

    case MP_CMD_GET_PROPERTY: {
        char *tmp;
        int r = mp_property_do(cmd->args[0].v.s, M_PROPERTY_TO_STRING,
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

    case MP_CMD_EDL_MARK:
        if (edl_fd) {
            float v = get_current_time(mpctx);
            if (mpctx->begin_skip == MP_NOPTS_VALUE) {
                mpctx->begin_skip = v;
                mp_tmsg(MSGT_CPLAYER, MSGL_INFO,
                        "EDL skip start, press 'i' again to end block.\n");
            } else {
                if (mpctx->begin_skip > v)
                    mp_tmsg(MSGT_CPLAYER, MSGL_WARN,
                            "EDL skip canceled, last start > stop\n");
                else {
                    fprintf(edl_fd, "%f %f %d\n", mpctx->begin_skip, v, 0);
                    mp_tmsg(MSGT_CPLAYER, MSGL_INFO,
                            "EDL skip end, line written.\n");
                }
                mpctx->begin_skip = MP_NOPTS_VALUE;
            }
        }
        break;

    case MP_CMD_SWITCH_RATIO:
        if (!sh_video)
            break;
        if (cmd->nargs == 0 || cmd->args[0].v.f == -1)
            opts->movie_aspect = (float) sh_video->disp_w / sh_video->disp_h;
        else
            opts->movie_aspect = cmd->args[0].v.f;
        video_reset_aspect(sh_video);
        break;

    case MP_CMD_SPEED_INCR: {
        float v = cmd->args[0].v.f;
        mp_property_do("speed", M_PROPERTY_STEP_UP, &v, mpctx);
        show_property_osd(mpctx, "speed");
        break;
    }

    case MP_CMD_SPEED_MULT:
        case_fallthrough_hack = true;

    case MP_CMD_SPEED_SET: {
        float v = cmd->args[0].v.f;
        if (case_fallthrough_hack)
            v *= mpctx->opts.playback_speed;
        mp_property_do("speed", M_PROPERTY_SET, &v, mpctx);
        show_property_osd(mpctx, "speed");
        break;
    }

    case MP_CMD_FRAME_STEP:
        add_step_frame(mpctx);
        break;

    case MP_CMD_QUIT:
        mpctx->stop_play = PT_QUIT;
        mpctx->quit_player_rc = (cmd->nargs > 0) ? cmd->args[0].v.i : 0;
        break;

    case MP_CMD_PLAYLIST_NEXT:
    case MP_CMD_PLAYLIST_PREV:
    {
        int dir = cmd->id == MP_CMD_PLAYLIST_PREV ? -1 : +1;
        int force = cmd->args[0].v.i;

        struct playlist_entry *e = playlist_get_next(mpctx->playlist, dir);
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
            step_sub(mpctx->subdata, mpctx->video_pts, movement);
#ifdef CONFIG_ASS
            if (mpctx->osd->ass_track)
                sub_delay +=
                    ass_step_sub(mpctx->osd->ass_track,
                                 (mpctx->video_pts +
                                  sub_delay) * 1000 + .5, movement) / 1000.;
#endif
            set_osd_tmsg(mpctx, OSD_MSG_SUB_DELAY, 1, osd_duration,
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
        /* Show OSD state when disabled, but not when an explicit
           argument is given to the OSD command, i.e. in slave mode. */
        if (v == -1 && opts->osd_level <= 1)
            set_osd_tmsg(mpctx, OSD_MSG_OSD_STATUS, 0, osd_duration,
                         "OSD: %s",
                         opts->osd_level ? mp_gtext("enabled") :
                         mp_gtext("disabled"));
        else
            rm_osd_msg(mpctx, OSD_MSG_OSD_STATUS);
        break;
    }

    case MP_CMD_OSD_SHOW_TEXT:
        set_osd_msg(mpctx, OSD_MSG_TEXT, cmd->args[2].v.i,
                    (cmd->args[1].v.i <
                     0 ? osd_duration : cmd->args[1].v.i),
                    "%s", cmd->args[0].v.s);
        break;

    case MP_CMD_OSD_SHOW_PROPERTY_TEXT: {
        char *txt = m_properties_expand_string(mp_properties,
                                               cmd->args[0].v.s,
                                               mpctx);
        // if no argument supplied use default osd_duration, else <arg> ms.
        if (txt) {
            set_osd_msg(mpctx, OSD_MSG_TEXT, cmd->args[2].v.i,
                        (cmd->args[1].v.i <
                         0 ? osd_duration : cmd->args[1].v.i),
                        "%s", txt);
            free(txt);
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
        if (!pl) {
            if (!append)
                playlist_clear(mpctx->playlist);
            playlist_transfer_entries(mpctx->playlist, pl);
            talloc_free(pl);

            if (!append)
                mpctx->stop_play = PT_NEXT_ENTRY;
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

    case MP_CMD_OSD_SHOW_PROGRESSION:
        mp_show_osd_progression(mpctx);
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
                set_osd_tmsg(OSD_MSG_RADIO_CHANNEL, 1, osd_duration,
                             "Channel: %s",
                             radio_get_channel_name(mpctx->stream));
            }
        }
        break;

    case MP_CMD_RADIO_SET_CHANNEL:
        if (mpctx->stream && mpctx->stream->type == STREAMTYPE_RADIO) {
            radio_set_channel(mpctx->stream, cmd->args[0].v.s);
            if (radio_get_channel_name(mpctx->stream)) {
                set_osd_tmsg(OSD_MSG_RADIO_CHANNEL, 1, osd_duration,
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
            set_osd_msg(mpctx, OSD_MSG_TV_CHANNEL, 1, osd_duration, "%s: %s",
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
            set_osd_msg(mpctx, OSD_MSG_TV_CHANNEL, 1, osd_duration, "%s: f %d",
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
                set_osd_tmsg(mpctx, OSD_MSG_TV_CHANNEL, 1, osd_duration,
                             "Channel: %s", tv_channel_current->name);
                //vo_osd_changed(OSDTYPE_SUBTITLE);
            }
        }
#ifdef CONFIG_PVR
        else if (mpctx->stream &&
                 mpctx->stream->type == STREAMTYPE_PVR) {
            pvr_set_channel_step(mpctx->stream, cmd->args[0].v.i);
            set_osd_msg(mpctx, OSD_MSG_TV_CHANNEL, 1, osd_duration, "%s: %s",
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
                set_osd_tmsg(mpctx, OSD_MSG_TV_CHANNEL, 1, osd_duration,
                             "Channel: %s", tv_channel_current->name);
                //vo_osd_changed(OSDTYPE_SUBTITLE);
            }
        }
#ifdef CONFIG_PVR
        else if (mpctx->stream && mpctx->stream->type == STREAMTYPE_PVR) {
            pvr_set_channel(mpctx->stream, cmd->args[0].v.s);
            set_osd_msg(mpctx, OSD_MSG_TV_CHANNEL, 1, osd_duration, "%s: %s",
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
                set_osd_tmsg(mpctx, OSD_MSG_TV_CHANNEL, 1, osd_duration,
                             "Channel: %s", tv_channel_current->name);
                //vo_osd_changed(OSDTYPE_SUBTITLE);
            }
        }
#ifdef CONFIG_PVR
        else if (mpctx->stream && mpctx->stream->type == STREAMTYPE_PVR) {
            pvr_set_lastchannel(mpctx->stream);
            set_osd_msg(mpctx, OSD_MSG_TV_CHANNEL, 1, osd_duration, "%s: %s",
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

    case MP_CMD_SUB_LOAD:
        if (sh_video) {
            int n = mpctx->set_of_sub_size;
            add_subtitles(mpctx, cmd->args[0].v.s, sh_video->fps, 0);
            if (n != mpctx->set_of_sub_size) {
                mpctx->sub_counts[SUB_SOURCE_SUBS]++;
                ++mpctx->global_sub_size;
            }
        }
        break;

    case MP_CMD_GET_SUB_VISIBILITY:
        if (sh_video) {
            mp_msg(MSGT_GLOBAL, MSGL_INFO,
                   "ANS_SUB_VISIBILITY=%d\n", sub_visibility);
        }
        break;

    case MP_CMD_SCREENSHOT:
        screenshot_request(mpctx, cmd->args[0].v.i, cmd->args[1].v.i);
        break;

    case MP_CMD_VF_CHANGE_RECTANGLE:
        if (!sh_video)
            break;
        set_rectangle(sh_video, cmd->args[0].v.i, cmd->args[1].v.i);
        break;

    case MP_CMD_GET_TIME_LENGTH:
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_LENGTH=%.2f\n",
               get_time_length(mpctx));
        break;

    case MP_CMD_GET_FILENAME: {
        char *inf = get_metadata(mpctx, META_NAME);
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_FILENAME='%s'\n", inf);
        talloc_free(inf);
        break;
    }

    case MP_CMD_GET_VIDEO_CODEC: {
        char *inf = get_metadata(mpctx, META_VIDEO_CODEC);
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_VIDEO_CODEC='%s'\n", inf);
        talloc_free(inf);
        break;
    }

    case MP_CMD_GET_VIDEO_BITRATE: {
        char *inf = get_metadata(mpctx, META_VIDEO_BITRATE);
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_VIDEO_BITRATE='%s'\n", inf);
        talloc_free(inf);
        break;
    }

    case MP_CMD_GET_VIDEO_RESOLUTION: {
        char *inf = get_metadata(mpctx, META_VIDEO_RESOLUTION);
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_VIDEO_RESOLUTION='%s'\n", inf);
        talloc_free(inf);
        break;
    }

    case MP_CMD_GET_AUDIO_CODEC: {
        char *inf = get_metadata(mpctx, META_AUDIO_CODEC);
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_AUDIO_CODEC='%s'\n", inf);
        talloc_free(inf);
        break;
    }

    case MP_CMD_GET_AUDIO_BITRATE: {
        char *inf = get_metadata(mpctx, META_AUDIO_BITRATE);
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_AUDIO_BITRATE='%s'\n", inf);
        talloc_free(inf);
        break;
    }

    case MP_CMD_GET_AUDIO_SAMPLES: {
        char *inf = get_metadata(mpctx, META_AUDIO_SAMPLES);
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_AUDIO_SAMPLES='%s'\n", inf);
        talloc_free(inf);
        break;
    }

    case MP_CMD_GET_META_TITLE: {
        char *inf = get_metadata(mpctx, META_INFO_TITLE);
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_TITLE='%s'\n", inf);
        talloc_free(inf);
        break;
    }

    case MP_CMD_GET_META_ARTIST: {
        char *inf = get_metadata(mpctx, META_INFO_ARTIST);
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_ARTIST='%s'\n", inf);
        talloc_free(inf);
        break;
    }

    case MP_CMD_GET_META_ALBUM: {
        char *inf = get_metadata(mpctx, META_INFO_ALBUM);
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_ALBUM='%s'\n", inf);
        talloc_free(inf);
        break;
    }

    case MP_CMD_GET_META_YEAR: {
        char *inf = get_metadata(mpctx, META_INFO_YEAR);
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_YEAR='%s'\n", inf);
        talloc_free(inf);
        break;
    }

    case MP_CMD_GET_META_COMMENT: {
        char *inf = get_metadata(mpctx, META_INFO_COMMENT);
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_COMMENT='%s'\n", inf);
        talloc_free(inf);
        break;
    }

    case MP_CMD_GET_META_TRACK: {
        char *inf = get_metadata(mpctx, META_INFO_TRACK);
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_TRACK='%s'\n", inf);
        talloc_free(inf);
        break;
    }

    case MP_CMD_GET_META_GENRE: {
        char *inf = get_metadata(mpctx, META_INFO_GENRE);
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_GENRE='%s'\n", inf);
        talloc_free(inf);
        break;
    }

    case MP_CMD_GET_VO_FULLSCREEN:
        if (mpctx->video_out && mpctx->video_out->config_ok)
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_VO_FULLSCREEN=%d\n", vo_fs);
        break;

    case MP_CMD_GET_PERCENT_POS:
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_PERCENT_POSITION=%d\n",
               get_percent_pos(mpctx));
        break;

    case MP_CMD_GET_TIME_POS: {
        float pos = get_current_time(mpctx);
        mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_TIME_POSITION=%.1f\n", pos);
        break;
    }

    case MP_CMD_RUN:
#ifndef __MINGW32__
        if (!fork()) {
            char *exp_cmd = property_expand_string(mpctx, cmd->args[0].v.s);
            if (exp_cmd) {
                execl("/bin/sh", "sh", "-c", exp_cmd, NULL);
                free(exp_cmd);
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
                set_osd_msg(mpctx, OSD_MSG_TEXT, 1, osd_duration, "vo='%s'", s);
            } else {
                set_osd_msg(mpctx, OSD_MSG_TEXT, 1, osd_duration, "Failed!");
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
        char *af_commands = af_args;
        char *af_command;
        af_instance_t *af;
        while ((af_command = strsep(&af_commands, ",")) != NULL) {
            if (cmd->id == MP_CMD_AF_DEL) {
                af = af_get(mpctx->mixer.afilter, af_command);
                if (af != NULL)
                    af_remove(mpctx->mixer.afilter, af);
            } else
                af_add(mpctx->mixer.afilter, af_command);
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
            af_instance_t *af = af_get(sh_audio->afilter, cmd->args[0].v.s);
            if (!af) {
                mp_msg(MSGT_CPLAYER, MSGL_WARN,
                       "Filter '%s' not found in chain.\n", cmd->args[0].v.s);
                break;
            }
            af->control(af, AF_CONTROL_COMMAND_LINE, cmd->args[1].v.s);
            af_reinit(sh_audio->afilter, af);
        }
        break;
    case MP_CMD_SHOW_CHAPTERS:
        show_chapters_on_osd(mpctx);
        break;
    case MP_CMD_SHOW_TRACKS:
        show_tracks_on_osd(mpctx);
        break;

    default:
        mp_msg(MSGT_CPLAYER, MSGL_V,
               "Received unknown cmd %s\n", cmd->name);
    }

old_pause_hack:
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
