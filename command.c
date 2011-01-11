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

#include "config.h"
#include "talloc.h"
#include "command.h"
#include "input/input.h"
#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "codec-cfg.h"
#include "mplayer.h"
#include "libvo/sub.h"
#include "m_option.h"
#include "m_property.h"
#include "m_config.h"
#include "metadata.h"
#include "libmpcodecs/vf.h"
#include "libmpcodecs/vd.h"
#include "mp_osd.h"
#include "libvo/video_out.h"
#include "libvo/font_load.h"
#include "playtree.h"
#include "libao2/audio_out.h"
#include "mpcommon.h"
#include "mixer.h"
#include "libmpcodecs/dec_video.h"
#include "libmpcodecs/dec_audio.h"
#include "libmpcodecs/dec_teletext.h"
#include "vobsub.h"
#include "spudec.h"
#include "path.h"
#include "ass_mp.h"
#include "stream/tv.h"
#include "stream/stream_radio.h"
#include "stream/pvr.h"
#ifdef CONFIG_DVBIN
#include "stream/dvbin.h"
#endif
#ifdef CONFIG_DVDREAD
#include "stream/stream_dvd.h"
#endif
#include "stream/stream_dvdnav.h"
#include "m_struct.h"
#include "libmenu/menu.h"

#include "mp_core.h"
#include "mp_fifo.h"
#include "libavutil/avstring.h"

#define ROUND(x) ((int)((x)<0 ? (x)-0.5 : (x)+0.5))

extern int use_menu;

static void rescale_input_coordinates(struct MPContext *mpctx, int ix, int iy,
                                      double *dx, double *dy)
{
    struct MPOpts *opts = &mpctx->opts;
    struct vo *vo = mpctx->video_out;
    //remove the borders, if any, and rescale to the range [0,1],[0,1]
    if (vo_fs) {                //we are in full-screen mode
        if (opts->vo_screenwidth > vo->dwidth)  //there are borders along the x axis
            ix -= (opts->vo_screenwidth - vo->dwidth) / 2;
        if (opts->vo_screenheight > vo->dheight)  //there are borders along the y axis (usual way)
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

/**
 * \brief Log the currently displayed subtitle to a file
 *
 * Logs the current or last displayed subtitle together with filename
 * and time information to ~/.mplayer/subtitle_log
 *
 * Intended purpose is to allow convenient marking of bogus subtitles
 * which need to be fixed while watching the movie.
 */

static void log_sub(struct MPContext *mpctx)
{
    char *fname;
    FILE *f;
    int i;
    struct subtitle *vo_sub_last = mpctx->vo_sub_last;

    if (mpctx->subdata == NULL || vo_sub_last == NULL)
        return;
    fname = get_path("subtitle_log");
    f = fopen(fname, "a");
    if (!f)
        return;
    fprintf(f, "----------------------------------------------------------\n");
    if (mpctx->subdata->sub_uses_time) {
        fprintf(f,
                "N: %s S: %02ld:%02ld:%02ld.%02ld E: %02ld:%02ld:%02ld.%02ld\n",
                mpctx->filename, vo_sub_last->start / 360000,
                (vo_sub_last->start / 6000) % 60,
                (vo_sub_last->start / 100) % 60, vo_sub_last->start % 100,
                vo_sub_last->end / 360000, (vo_sub_last->end / 6000) % 60,
                (vo_sub_last->end / 100) % 60, vo_sub_last->end % 100);
    } else {
        fprintf(f, "N: %s S: %ld E: %ld\n", mpctx->filename,
                vo_sub_last->start, vo_sub_last->end);
    }
    for (i = 0; i < vo_sub_last->lines; i++) {
        fprintf(f, "%s\n", vo_sub_last->text[i]);
    }
    fclose(f);
}


/// \defgroup Properties
///@{

/// \defgroup GeneralProperties General properties
/// \ingroup Properties
///@{

static int mp_property_generic_option(struct m_option *prop, int action,
                                      void *arg, MPContext *mpctx)
{
    char *optname = prop->priv;
    const struct m_option *opt = m_config_get_option(mpctx->mconfig, optname);
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
        if (!arg) return M_PROPERTY_ERROR;
        if (opts->loop_times < 0)
            *(char**)arg = strdup("off");
        else if (opts->loop_times == 0)
            *(char**)arg = strdup("inf");
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
    switch (action) {
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(float *) arg);
        opts->playback_speed = *(float *) arg;
        reinit_audio_chain(mpctx);
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        opts->playback_speed += (arg ? *(float *) arg : 0.1) *
            (action == M_PROPERTY_STEP_DOWN ? -1 : 1);
        M_PROPERTY_CLAMP(prop, opts->playback_speed);
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
    if (!mpctx->demuxer)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_string_ro(prop, action, arg,
                                (char *) mpctx->demuxer->desc->name);
}

/// Position in the stream (RW)
static int mp_property_stream_pos(m_option_t *prop, int action, void *arg,
                                  MPContext *mpctx)
{
    if (!mpctx->demuxer || !mpctx->demuxer->stream)
        return M_PROPERTY_UNAVAILABLE;
    if (!arg)
        return M_PROPERTY_ERROR;
    switch (action) {
    case M_PROPERTY_GET:
        *(off_t *) arg = stream_tell(mpctx->demuxer->stream);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        M_PROPERTY_CLAMP(prop, *(off_t *) arg);
        stream_seek(mpctx->demuxer->stream, *(off_t *) arg);
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Stream start offset (RO)
static int mp_property_stream_start(m_option_t *prop, int action,
                                    void *arg, MPContext *mpctx)
{
    if (!mpctx->demuxer || !mpctx->demuxer->stream)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_GET:
        *(off_t *) arg = mpctx->demuxer->stream->start_pos;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Stream end offset (RO)
static int mp_property_stream_end(m_option_t *prop, int action, void *arg,
                                  MPContext *mpctx)
{
    if (!mpctx->demuxer || !mpctx->demuxer->stream)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_GET:
        *(off_t *) arg = mpctx->demuxer->stream->end_pos;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Stream length (RO)
static int mp_property_stream_length(m_option_t *prop, int action,
                                     void *arg, MPContext *mpctx)
{
    if (!mpctx->demuxer || !mpctx->demuxer->stream)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_GET:
        *(off_t *) arg =
            mpctx->demuxer->stream->end_pos - mpctx->demuxer->stream->start_pos;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Current stream position in seconds (RO)
static int mp_property_stream_time_pos(m_option_t *prop, int action,
                                       void *arg, MPContext *mpctx)
{
    if (!mpctx->demuxer || mpctx->demuxer->stream_pts == MP_NOPTS_VALUE)
        return M_PROPERTY_UNAVAILABLE;

    return m_property_time_ro(prop, action, arg, mpctx->demuxer->stream_pts);
}


/// Media length in seconds (RO)
static int mp_property_length(m_option_t *prop, int action, void *arg,
                              MPContext *mpctx)
{
    double len;

    if (!mpctx->demuxer ||
        !(int) (len = get_time_length(mpctx)))
        return M_PROPERTY_UNAVAILABLE;

    return m_property_time_ro(prop, action, arg, len);
}

/// Current position in percent (RW)
static int mp_property_percent_pos(m_option_t *prop, int action,
                                   void *arg, MPContext *mpctx) {
    int pos;

    if (!mpctx->demuxer)
        return M_PROPERTY_UNAVAILABLE;

    switch(action) {
    case M_PROPERTY_SET:
        if(!arg) return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(int*)arg);
        pos = *(int*)arg;
        break;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        pos = get_percent_pos(mpctx);
        pos += (arg ? *(int*)arg : 10) *
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
                                void *arg, MPContext *mpctx) {
    if (!(mpctx->sh_video || (mpctx->sh_audio && mpctx->audio_out)))
        return M_PROPERTY_UNAVAILABLE;

    switch(action) {
    case M_PROPERTY_SET:
        if(!arg) return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(double*)arg);
        queue_seek(mpctx, MPSEEK_ABSOLUTE, *(double*)arg, 0);
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        queue_seek(mpctx, MPSEEK_RELATIVE, (arg ? *(double*)arg : 10.0) *
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
    int chapter = -1;
    int step_all;
    char *chapter_name = NULL;

    if (mpctx->demuxer)
        chapter = get_current_chapter(mpctx);
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
        M_PROPERTY_CLAMP(prop, *(int*)arg);
        step_all = *(int *)arg - chapter;
        chapter += step_all;
        break;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN: {
        step_all = (arg && *(int*)arg != 0 ? *(int*)arg : 1)
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
    chapter = seek_chapter(mpctx, chapter, &next_pts, &chapter_name);
    if (chapter >= 0) {
        if (next_pts > -1.0)
            queue_seek(mpctx, MPSEEK_ABSOLUTE, next_pts, 0);
        if (chapter_name)
            set_osd_tmsg(OSD_MSG_TEXT, 1, opts->osd_duration,
                         "Chapter: (%d) %s", chapter + 1, chapter_name);
    } else if (step_all > 0)
        queue_seek(mpctx, MPSEEK_RELATIVE, 1000000000, 0);
    else
        set_osd_tmsg(OSD_MSG_TEXT, 1, opts->osd_duration,
                     "Chapter: (%d) %s", 0, mp_gtext("unknown"));
    talloc_free(chapter_name);
    return M_PROPERTY_OK;
}

/// Number of chapters in file
static int mp_property_chapters(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    if (!mpctx->demuxer)
        return M_PROPERTY_UNAVAILABLE;
    if (mpctx->demuxer->num_chapters == 0)
        stream_control(mpctx->demuxer->stream, STREAM_CTRL_GET_NUM_CHAPTERS, &mpctx->demuxer->num_chapters);
    return m_property_int_ro(prop, action, arg, mpctx->demuxer->num_chapters);
}

/// Current dvd angle (RW)
static int mp_property_angle(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    int angle = -1;
    int angles;
    char *angle_name = NULL;

    if (mpctx->demuxer)
        angle = demuxer_get_current_angle(mpctx->demuxer);
    if (angle < 0)
        return M_PROPERTY_UNAVAILABLE;
    angles = demuxer_angles_count(mpctx->demuxer);
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
        angle_name = calloc(1, 64);
        if (!angle_name)
            return M_PROPERTY_UNAVAILABLE;
        snprintf(angle_name, 64, "%d/%d", angle, angles);
        *(char **) arg = angle_name;
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
        if(arg)
            step = *(int*)arg;
        if(!step)
            step = 1;
        step *= (action == M_PROPERTY_STEP_UP ? 1 : -1);
        angle += step;
        if (angle < 1) //cycle
            angle = angles;
        break;
    }
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }
    angle = demuxer_set_angle(mpctx->demuxer, angle);
    if (angle >= 0) {
        struct sh_video *sh_video = mpctx->demuxer->video->sh;
        if (sh_video)
            resync_video_stream(sh_video);

        struct sh_audio *sh_audio = mpctx->demuxer->audio->sh;
        if (sh_audio)
            resync_audio_stream(sh_audio);
    }

    set_osd_tmsg(OSD_MSG_TEXT, 1, opts->osd_duration,
                 "Angle: %d/%d", angle, angles);
    free(angle_name);
    return M_PROPERTY_OK;
}

/// Demuxer meta data
static int mp_property_metadata(m_option_t *prop, int action, void *arg,
                                MPContext *mpctx) {
    m_property_action_t* ka;
    char* meta;
    static const m_option_t key_type =
        { "metadata", NULL, CONF_TYPE_STRING, 0, 0, 0, NULL };
    if (!mpctx->demuxer)
        return M_PROPERTY_UNAVAILABLE;

    switch(action) {
    case M_PROPERTY_GET:
        if(!arg) return M_PROPERTY_ERROR;
        *(char***)arg = mpctx->demuxer->info;
        return M_PROPERTY_OK;
    case M_PROPERTY_KEY_ACTION:
        if(!arg) return M_PROPERTY_ERROR;
        ka = arg;
        if(!(meta = demux_info_get(mpctx->demuxer,ka->key)))
            return M_PROPERTY_UNKNOWN;
        switch(ka->action) {
        case M_PROPERTY_GET:
            if(!ka->arg) return M_PROPERTY_ERROR;
            *(char**)ka->arg = meta;
            return M_PROPERTY_OK;
        case M_PROPERTY_GET_TYPE:
            if(!ka->arg) return M_PROPERTY_ERROR;
            *(const m_option_t**)ka->arg = &key_type;
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
        if (mpctx->paused == (bool)*(int *) arg)
            return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        if (mpctx->paused) {
            unpause_player(mpctx);
            mpctx->osd_function = OSD_PLAY;
        }
        else {
            pause_player(mpctx);
            mpctx->osd_function = OSD_PAUSE;
        }
        return M_PROPERTY_OK;
    default:
        return m_property_flag(prop, action, arg, &mpctx->paused);
    }
}


///@}

/// \defgroup AudioProperties Audio properties
/// \ingroup Properties
///@{

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
    case M_PROPERTY_PRINT:{
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

    if (mpctx->edl_muted)
        return M_PROPERTY_DISABLED;
    mpctx->user_muted = 0;

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
        if (mpctx->edl_muted)
            return M_PROPERTY_DISABLED;
        if (!arg)
            return M_PROPERTY_ERROR;
        if ((!!*(int *) arg) != mpctx->mixer.muted)
            mixer_mute(&mpctx->mixer);
        mpctx->user_muted = mpctx->mixer.muted;
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        if (mpctx->edl_muted)
            return M_PROPERTY_DISABLED;
        mixer_mute(&mpctx->mixer);
        mpctx->user_muted = mpctx->mixer.muted;
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
        if (!arg)
            return M_PROPERTY_ERROR;
        if (mpctx->edl_muted) {
            *(char **) arg = strdup(mp_gtext("enabled (EDL)"));
            return M_PROPERTY_OK;
        }
    default:
        return m_property_flag(prop, action, arg, &mpctx->mixer.muted);

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
    return m_property_string_ro(prop, action, arg, mpctx->sh_audio->codec->name);
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
    switch(action) {
    case M_PROPERTY_PRINT:
        if(!arg) return M_PROPERTY_ERROR;
        *(char**)arg = malloc(16);
        sprintf(*(char**)arg,"%d kHz",mpctx->sh_audio->samplerate/1000);
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
            *(char **) arg = strdup("mono");
            break;
        case 2:
            *(char **) arg = strdup("stereo");
            break;
        default:
            *(char **) arg = malloc(32);
            sprintf(*(char **) arg, "%d channels", mpctx->sh_audio->channels);
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

    if (!mpctx->sh_audio || mpctx->sh_audio->channels < 2)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_GET:
        if (!arg)
            return M_PROPERTY_ERROR;
        mixer_getbalance(&mpctx->mixer, arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT: {
            char** str = arg;
            if (!arg)
                return M_PROPERTY_ERROR;
            mixer_getbalance(&mpctx->mixer, &bal);
            if (bal == 0.f)
                *str = strdup("center");
            else if (bal == -1.f)
                *str = strdup("left only");
            else if (bal == 1.f)
                *str = strdup("right only");
            else {
                unsigned right = (bal + 1.f) / 2.f * 100.f;
                *str = malloc(sizeof("left xxx%, right xxx%"));
                sprintf(*str, "left %d%%, right %d%%", 100 - right, right);
            }
            return M_PROPERTY_OK;
        }
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        mixer_getbalance(&mpctx->mixer, &bal);
        bal += (arg ? *(float*)arg : .1f) *
            (action == M_PROPERTY_STEP_UP ? 1.f : -1.f);
        M_PROPERTY_CLAMP(prop, bal);
        mixer_setbalance(&mpctx->mixer, bal);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(float*)arg);
        mixer_setbalance(&mpctx->mixer, *(float*)arg);
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Selected audio id (RW)
static int mp_property_audio(m_option_t *prop, int action, void *arg,
                             MPContext *mpctx)
{
    int current_id, tmp;
    if (!mpctx->demuxer || !mpctx->d_audio)
        return M_PROPERTY_UNAVAILABLE;
    current_id = mpctx->sh_audio ? mpctx->sh_audio->aid : -2;

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
            *(char **) arg = strdup(mp_gtext("disabled"));
        else {
            char lang[40];
            strncpy(lang, mp_gtext("unknown"), sizeof(lang));
            sh_audio_t* sh = mpctx->sh_audio;
            if (sh && sh->lang)
                av_strlcpy(lang, sh->lang, 40);
#ifdef CONFIG_DVDREAD
            else if (mpctx->stream->type == STREAMTYPE_DVD) {
                int code = dvd_lang_from_aid(mpctx->stream, current_id);
                if (code) {
                    lang[0] = code >> 8;
                    lang[1] = code;
                    lang[2] = 0;
                }
            }
#endif

#ifdef CONFIG_DVDNAV
            else if (mpctx->stream->type == STREAMTYPE_DVDNAV)
                mp_dvdnav_lang_from_aid(mpctx->stream, current_id, lang);
#endif
            *(char **) arg = malloc(64);
            snprintf(*(char **) arg, 64, "(%d) %s", current_id, lang);
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
            sh_audio_t *sh2;
            sh2 = mpctx->d_audio->demuxer->a_streams[mpctx->demuxer->audio->id];
            sh2->ds = mpctx->demuxer->audio;
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
    if (!mpctx->demuxer || !mpctx->d_video)
        return M_PROPERTY_UNAVAILABLE;
    current_id = mpctx->d_video->id;

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
            *(char **) arg = strdup(mp_gtext("disabled"));
        else {
            char lang[40];
            strncpy(lang, mp_gtext("unknown"), sizeof(lang));
            *(char **) arg = malloc(64);
            snprintf(*(char **) arg, 64, "(%d) %s", current_id, lang);
        }
        return M_PROPERTY_OK;

    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_SET:
        if (action == M_PROPERTY_SET && arg)
            tmp = *((int *) arg);
        else
            tmp = -1;
        opts->video_id = demuxer_switch_video(mpctx->d_video->demuxer, tmp);
        if (opts->video_id == -2
            || (opts->video_id > -1 && mpctx->d_video->id != current_id
                && current_id != -2))
            uninit_player(mpctx, INITIALIZED_VCODEC |
                          (mpctx->opts.fixed_vo && opts->video_id != -2 ? 0 : INITIALIZED_VO));
        if (opts->video_id > -1 && mpctx->d_video->id != current_id) {
            sh_video_t *sh2;
            sh2 = mpctx->d_video->demuxer->v_streams[mpctx->d_video->id];
            if (sh2) {
                sh2->ds = mpctx->d_video;
                mpctx->sh_video = sh2;
                reinit_video_chain(mpctx);
            }
        }
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_TRACK=%d\n", opts->video_id);
        return M_PROPERTY_OK;

    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }
}

static int mp_property_program(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    demux_program_t prog;

    switch (action) {
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_SET:
        if (action == M_PROPERTY_SET && arg)
            prog.progid = *((int *) arg);
        else
            prog.progid = -1;
        if (demux_control
            (mpctx->demuxer, DEMUXER_CTRL_IDENTIFY_PROGRAM,
             &prog) == DEMUXER_CTRL_NOTIMPL)
            return M_PROPERTY_ERROR;

        if (prog.aid < 0 && prog.vid < 0) {
            mp_msg(MSGT_CPLAYER, MSGL_ERR, "Selected program contains no audio or video streams!\n");
            return M_PROPERTY_ERROR;
        }
        mp_property_do("switch_audio", M_PROPERTY_SET, &prog.aid, mpctx);
        mp_property_do("switch_video", M_PROPERTY_SET, &prog.vid, mpctx);
        return M_PROPERTY_OK;

    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }
}

///@}

/// \defgroup VideoProperties Video properties
/// \ingroup Properties
///@{

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

static int mp_property_yuv_colorspace(m_option_t *prop, int action,
                                     void *arg, MPContext *mpctx)
{
    if (!mpctx->sh_video || !mpctx->sh_video->vfilter)
        return M_PROPERTY_UNAVAILABLE;

    struct vf_instance *vf = mpctx->sh_video->vfilter;
    int colorspace;
    switch (action) {
    case M_PROPERTY_GET:
        if (!arg)
            return M_PROPERTY_ERROR;
        if (vf->control(vf, VFCTRL_GET_YUV_COLORSPACE, arg) != true)
            return M_PROPERTY_UNAVAILABLE;
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
        if (!arg)
            return M_PROPERTY_ERROR;
        if (vf->control(vf, VFCTRL_GET_YUV_COLORSPACE, &colorspace) != true)
            return M_PROPERTY_UNAVAILABLE;
        char * const names[] = {"BT.601 (SD)", "BT.709 (HD)", "SMPTE-240M"};
        if (colorspace < 0 || colorspace >= sizeof(names) / sizeof(names[0]))
            *(char **)arg = strdup("Unknown");
        else
            *(char**)arg = strdup(names[colorspace]);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(int *) arg);
        vf->control(vf, VFCTRL_SET_YUV_COLORSPACE, arg);
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:;
        if (vf->control(vf, VFCTRL_GET_YUV_COLORSPACE, &colorspace) != true)
            return M_PROPERTY_UNAVAILABLE;
        colorspace += 1;
        vf->control(vf, VFCTRL_SET_YUV_COLORSPACE, &colorspace);
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_capture(m_option_t *prop, int action,
                               void *arg, MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;

    if (!mpctx->stream)
        return M_PROPERTY_UNAVAILABLE;

    if (!opts->capture_dump) {
        mp_tmsg(MSGT_GLOBAL, MSGL_ERR,
                "Capturing not enabled (forgot -capture parameter?)\n");
        return M_PROPERTY_ERROR;
    }

    int capturing = !!mpctx->stream->capture_file;

    int ret = m_property_flag(prop, action, arg, &capturing);
    if (ret == M_PROPERTY_OK && capturing != !!mpctx->stream->capture_file) {
        if (capturing) {
            mpctx->stream->capture_file = fopen(opts->stream_dump_name, "wb");
            if (!mpctx->stream->capture_file) {
                mp_tmsg(MSGT_GLOBAL, MSGL_ERR,
                        "Error opening capture file: %s\n", strerror(errno));
                ret = M_PROPERTY_ERROR;
            }
        } else {
            fclose(mpctx->stream->capture_file);
            mpctx->stream->capture_file = NULL;
        }
    }

    return ret;
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
        *(char **) arg = strdup(frame_dropping == 1 ? mp_gtext("enabled") :
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
    if (mpctx->demuxer->type == DEMUXER_TYPE_TV) {
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
    char* meta;
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;
    switch(action) {
    case M_PROPERTY_PRINT:
        if (!arg)
            return M_PROPERTY_ERROR;
        switch(mpctx->sh_video->format) {
        case 0x10000001:
            meta = strdup ("mpeg1"); break;
        case 0x10000002:
            meta = strdup ("mpeg2"); break;
        case 0x10000004:
            meta = strdup ("mpeg4"); break;
        case 0x10000005:
            meta = strdup ("h264"); break;
        default:
            if(mpctx->sh_video->format >= 0x20202020) {
                meta = malloc(5);
                sprintf (meta, "%.4s", (char *) &mpctx->sh_video->format);
            } else   {
                meta = malloc(20);
                sprintf (meta, "0x%08X", mpctx->sh_video->format);
            }
        }
        *(char**)arg = meta;
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
    return m_property_string_ro(prop, action, arg, mpctx->sh_video->codec->name);
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

///@}

/// \defgroup SubProprties Subtitles properties
/// \ingroup Properties
///@{

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
    int source = -1, reset_spu = 0;
    int source_pos = -1;
    char *sub_name;

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
        *(char **) arg = malloc(64);
        (*(char **) arg)[63] = 0;
        sub_name = 0;
        if (mpctx->subdata)
            sub_name = mpctx->subdata->filename;
#ifdef CONFIG_ASS
        if (ass_track && ass_track->name)
            sub_name = ass_track->name;
#endif
        if (sub_name) {
            const char *tmp = mp_basename(sub_name);

            snprintf(*(char **) arg, 63, "(%d) %s%s",
                     mpctx->set_of_sub_pos + 1,
                     strlen(tmp) < 20 ? "" : "...",
                     strlen(tmp) < 20 ? tmp : tmp + strlen(tmp) - 19);
            return M_PROPERTY_OK;
        }
#ifdef CONFIG_DVDNAV
        if (mpctx->stream->type == STREAMTYPE_DVDNAV) {
            if (vo_spudec && opts->sub_id >= 0) {
                unsigned char lang[3];
                if (mp_dvdnav_lang_from_sid(mpctx->stream, opts->sub_id, lang)) {
                    snprintf(*(char **) arg, 63, "(%d) %s", opts->sub_id, lang);
                    return M_PROPERTY_OK;
                }
            }
        }
#endif

        if ((d_sub->demuxer->type == DEMUXER_TYPE_MATROSKA
             || d_sub->demuxer->type == DEMUXER_TYPE_LAVF
             || d_sub->demuxer->type == DEMUXER_TYPE_LAVF_PREFERRED
             || d_sub->demuxer->type == DEMUXER_TYPE_OGG)
             && d_sub->sh && opts->sub_id >= 0) {
            const char* lang = ((sh_sub_t*)d_sub->sh)->lang;
            if (!lang) lang = mp_gtext("unknown");
            snprintf(*(char **) arg, 63, "(%d) %s", opts->sub_id, lang);
            return M_PROPERTY_OK;
        }

        if (vo_vobsub && vobsub_id >= 0) {
            const char *language = mp_gtext("unknown");
            language = vobsub_get_id(vo_vobsub, (unsigned int) vobsub_id);
            snprintf(*(char **) arg, 63, "(%d) %s",
                     vobsub_id, language ? language : mp_gtext("unknown"));
            return M_PROPERTY_OK;
        }
#ifdef CONFIG_DVDREAD
        if (vo_spudec && mpctx->stream->type == STREAMTYPE_DVD
            && opts->sub_id >= 0) {
            char lang[3];
            int code = dvd_lang_from_sid(mpctx->stream, opts->sub_id);
            lang[0] = code >> 8;
            lang[1] = code;
            lang[2] = 0;
            snprintf(*(char **) arg, 63, "(%d) %s", opts->sub_id, lang);
            return M_PROPERTY_OK;
        }
#endif
        if (opts->sub_id >= 0) {
            snprintf(*(char **) arg, 63, "(%d) %s", opts->sub_id,
                     mp_gtext("unknown"));
            return M_PROPERTY_OK;
        }
        snprintf(*(char **) arg, 63, mp_gtext("disabled"));
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
#ifdef CONFIG_ASS
    ass_track = 0;
#endif

    if (source == SUB_SOURCE_VOBSUB) {
        vobsub_id = vobsub_get_id_by_index(vo_vobsub, source_pos);
    } else if (source == SUB_SOURCE_SUBS) {
        mpctx->set_of_sub_pos = source_pos;
#ifdef CONFIG_ASS
        if (opts->ass_enabled && mpctx->set_of_ass_tracks[mpctx->set_of_sub_pos])
            ass_track = mpctx->set_of_ass_tracks[mpctx->set_of_sub_pos];
        else
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
#ifdef CONFIG_ASS
                else if (opts->ass_enabled)
                    ass_track = sh->ass_track;
#endif
            } else {
              d_sub->id = -2;
              d_sub->sh = NULL;
            }
        }
    }
#ifdef CONFIG_DVDREAD
    if (vo_spudec
        && (mpctx->stream->type == STREAMTYPE_DVD
            || mpctx->stream->type == STREAMTYPE_DVDNAV)
        && opts->sub_id < 0 && reset_spu) {
        d_sub->id = -2;
        d_sub->sh = NULL;
    }
#endif

    update_subtitles(mpctx, 0, 0, true);

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
        *(char **) arg = malloc(64);
        (*(char **) arg)[63] = 0;
        switch (sub_source(mpctx))
        {
        case SUB_SOURCE_SUBS:
            snprintf(*(char **) arg, 63, mp_gtext("file"));
            break;
        case SUB_SOURCE_VOBSUB:
            snprintf(*(char **) arg, 63, mp_gtext("vobsub"));
            break;
        case SUB_SOURCE_DEMUX:
            snprintf(*(char **) arg, 63, mp_gtext("embedded"));
            break;
        default:
            snprintf(*(char **) arg, 63, mp_gtext("disabled"));
        }
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(int*)arg);
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
        int step_all = (arg && *(int*)arg != 0 ? *(int*)arg : 1)
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
        }
        else
            *(int *) arg = -1;
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
        if (!arg)
            return M_PROPERTY_ERROR;
        if (is_cur_source)
            return mp_property_sub(prop, M_PROPERTY_PRINT, arg, mpctx);
        *(char **) arg = malloc(64);
        (*(char **) arg)[63] = 0;
        snprintf(*(char **) arg, 63, mp_gtext("disabled"));
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
        }
        else
            new_pos = -1;
        break;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN: {
        int step_all = (arg && *(int*)arg != 0 ? *(int*)arg : 1)
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
                    while (new_pos >= 0
                            && sub_source(mpctx) != source)
                        new_pos--;
                }
                else
                    new_pos = max_sub_pos_for_source;
            }
            else {
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
    char *name[] = { _("top"), _("center"), _("bottom") };

    if (!mpctx->sh_video || mpctx->global_sub_pos < 0
        || sub_source(mpctx) != SUB_SOURCE_SUBS)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_PRINT:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, sub_alignment);
        *(char **) arg = strdup(mp_gtext(name[sub_alignment]));
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
    if (!mpctx->sh_video)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        ass_force_reload = 1;
    default:
        return m_property_flag(prop, action, arg, &ass_use_margins);
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

#ifdef CONFIG_FREETYPE
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
                ass_font_scale = *(float *) arg;
                ass_force_reload = 1;
            }
#endif
            text_font_scale_factor = *(float *) arg;
            force_load_font = 1;
            return M_PROPERTY_OK;
        case M_PROPERTY_STEP_UP:
        case M_PROPERTY_STEP_DOWN:
#ifdef CONFIG_ASS
            if (opts->ass_enabled) {
                ass_font_scale += (arg ? *(float *) arg : 0.1)*
                  (action == M_PROPERTY_STEP_UP ? 1.0 : -1.0);
                M_PROPERTY_CLAMP(prop, ass_font_scale);
                ass_force_reload = 1;
            }
#endif
            text_font_scale_factor += (arg ? *(float *) arg : 0.1)*
              (action == M_PROPERTY_STEP_UP ? 1.0 : -1.0);
            M_PROPERTY_CLAMP(prop, text_font_scale_factor);
            force_load_font = 1;
            return M_PROPERTY_OK;
        default:
#ifdef CONFIG_ASS
            if (opts->ass_enabled)
                return m_property_float_ro(prop, action, arg, ass_font_scale);
            else
#endif
                return m_property_float_ro(prop, action, arg, text_font_scale_factor);
    }
}
#endif

///@}

/// \defgroup TVProperties TV properties
/// \ingroup Properties
///@{

#ifdef CONFIG_TV

/// TV color settings (RW)
static int mp_property_tv_color(m_option_t *prop, int action, void *arg,
                                MPContext *mpctx)
{
    int r, val;
    tvi_handle_t *tvh = mpctx->demuxer->priv;
    if (mpctx->demuxer->type != DEMUXER_TYPE_TV || !tvh)
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

static int mp_property_teletext_common(m_option_t *prop, int action, void *arg,
                  MPContext *mpctx)
{
    int val,result;
    int base_ioctl = prop->offset;
    /*
      for teletext's GET,SET,STEP ioctls this is not 0
      SET is GET+1
      STEP is GET+2
    */
    if (!mpctx->demuxer || !mpctx->demuxer->teletext)
        return M_PROPERTY_UNAVAILABLE;
    if(!base_ioctl)
        return M_PROPERTY_ERROR;

    switch (action) {
    case M_PROPERTY_GET:
        if (!arg)
            return M_PROPERTY_ERROR;
        result=teletext_control(mpctx->demuxer->teletext, base_ioctl, arg);
        break;
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(int *) arg);
        result=teletext_control(mpctx->demuxer->teletext, base_ioctl+1, arg);
        break;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        result=teletext_control(mpctx->demuxer->teletext, base_ioctl, &val);
        val += (arg ? *(int *) arg : 1) * (action == M_PROPERTY_STEP_DOWN ? -1 : 1);
        result=teletext_control(mpctx->demuxer->teletext, base_ioctl+1, &val);
        break;
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }

    return result == VBI_CONTROL_TRUE ? M_PROPERTY_OK : M_PROPERTY_ERROR;
}

static int mp_property_teletext_mode(m_option_t *prop, int action, void *arg,
                  MPContext *mpctx)
{
    int result;
    int val;

    //with tvh==NULL will fail too
    result=mp_property_teletext_common(prop,action,arg,mpctx);
    if(result!=M_PROPERTY_OK)
        return result;

    if(teletext_control(mpctx->demuxer->teletext,
                        prop->offset, &val)==VBI_CONTROL_TRUE && val)
        mp_input_set_section(mpctx->input, "teletext");
    else
        mp_input_set_section(mpctx->input, "tv");
    return M_PROPERTY_OK;
}

static int mp_property_teletext_page(m_option_t *prop, int action, void *arg,
                  MPContext *mpctx)
{
    int result;
    int val;
    if (!mpctx->demuxer->teletext)
        return M_PROPERTY_UNAVAILABLE;
    switch(action){
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        //This should be handled separately
        val = (arg ? *(int *) arg : 1) * (action == M_PROPERTY_STEP_DOWN ? -1 : 1);
        result=teletext_control(mpctx->demuxer->teletext,
                                TV_VBI_CONTROL_STEP_PAGE, &val);
        break;
    default:
        result=mp_property_teletext_common(prop,action,arg,mpctx);
    }
    return result;
}

///@}

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
    { "chapters", mp_property_chapters, CONF_TYPE_INT,
     0, 0, 0, NULL },
    { "angle", mp_property_angle, CONF_TYPE_INT,
     CONF_RANGE, -2, 10, NULL },
    { "metadata", mp_property_metadata, CONF_TYPE_STRING_LIST,
     0, 0, 0, NULL },
    { "pause", mp_property_pause, CONF_TYPE_FLAG,
     M_OPT_RANGE, 0, 1, NULL },
    { "capturing", mp_property_capture, CONF_TYPE_FLAG,
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
    { "yuv_colorspace", mp_property_yuv_colorspace, CONF_TYPE_INT,
     M_OPT_RANGE, 0, 2, NULL },
    { "ontop", mp_property_ontop, CONF_TYPE_FLAG,
     M_OPT_RANGE, 0, 1, NULL },
    { "rootwin", mp_property_rootwin, CONF_TYPE_FLAG,
     M_OPT_RANGE, 0, 1, NULL },
    { "border", mp_property_border, CONF_TYPE_FLAG,
     M_OPT_RANGE, 0, 1, NULL },
    { "framedropping", mp_property_framedropping, CONF_TYPE_INT,
     M_OPT_RANGE, 0, 2, NULL },
    { "gamma", mp_property_gamma, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, .offset=offsetof(struct MPOpts, vo_gamma_gamma)},
    { "brightness", mp_property_gamma, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, .offset=offsetof(struct MPOpts, vo_gamma_brightness) },
    { "contrast", mp_property_gamma, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, .offset=offsetof(struct MPOpts, vo_gamma_contrast) },
    { "saturation", mp_property_gamma, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, .offset=offsetof(struct MPOpts, vo_gamma_saturation) },
    { "hue", mp_property_gamma, CONF_TYPE_INT,
      M_OPT_RANGE, -100, 100, .offset=offsetof(struct MPOpts, vo_gamma_hue) },
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
#ifdef CONFIG_FREETYPE
    { "sub_scale", mp_property_sub_scale, CONF_TYPE_FLOAT,
     M_OPT_RANGE, 0, 100, NULL },
#endif
#ifdef CONFIG_ASS
    { "ass_use_margins", mp_property_ass_use_margins, CONF_TYPE_FLAG,
     M_OPT_RANGE, 0, 1, NULL },
#endif

#ifdef CONFIG_TV
    { "tv_brightness", mp_property_tv_color, CONF_TYPE_INT,
     M_OPT_RANGE, -100, 100, .offset=TV_COLOR_BRIGHTNESS },
    { "tv_contrast", mp_property_tv_color, CONF_TYPE_INT,
     M_OPT_RANGE, -100, 100, .offset=TV_COLOR_CONTRAST },
    { "tv_saturation", mp_property_tv_color, CONF_TYPE_INT,
     M_OPT_RANGE, -100, 100, .offset=TV_COLOR_SATURATION },
    { "tv_hue", mp_property_tv_color, CONF_TYPE_INT,
     M_OPT_RANGE, -100, 100, .offset=TV_COLOR_HUE },
#endif
    { "teletext_page", mp_property_teletext_page, CONF_TYPE_INT,
     M_OPT_RANGE, 100, 899, .offset=TV_VBI_CONTROL_GET_PAGE },
    { "teletext_subpage", mp_property_teletext_common, CONF_TYPE_INT,
     M_OPT_RANGE, 0, 64, .offset=TV_VBI_CONTROL_GET_SUBPAGE },
    { "teletext_mode", mp_property_teletext_mode, CONF_TYPE_FLAG,
     M_OPT_RANGE, 0, 1, .offset=TV_VBI_CONTROL_GET_MODE },
    { "teletext_format", mp_property_teletext_common, CONF_TYPE_INT,
     M_OPT_RANGE, 0, 3, .offset=TV_VBI_CONTROL_GET_FORMAT },
    { "teletext_half_page", mp_property_teletext_common, CONF_TYPE_INT,
     M_OPT_RANGE, 0, 2, .offset=TV_VBI_CONTROL_GET_HALF_PAGE },
    { NULL, NULL, NULL, 0, 0, 0, NULL }
};


int mp_property_do(const char *name, int action, void *val, void *ctx)
{
    return m_property_do(mp_properties, name, action, val, ctx);
}

char* mp_property_print(const char *name, void* ctx)
{
    char* ret = NULL;
    if(mp_property_do(name,M_PROPERTY_PRINT,&ret,ctx) <= 0)
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
    { "capturing", 0, -1, _("Capturing: %s") },
    { "pts_association_mode", 0, -1, "PTS association mode: %s" },
    { "hr_seek", 0, -1, "hr-seek: %s" },
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
    { "yuv_colorspace", 0, -1, _("YUV colorspace: %s") },
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
#ifdef CONFIG_FREETYPE
    { "sub_scale", 0, -1, _("Sub Scale: %s")},
#endif
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
    m_option_t* prop;
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
            set_osd_tmsg(p->osd_id >= 0 ? p->osd_id : OSD_MSG_PROPERTY + index,
                         1, opts->osd_duration, p->osd_msg, val);
            free(val);
        }
    }
    return 0;
}


/**
 * \defgroup Command2Property Command to property bridge
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
 *
 *@{
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
    { "capturing", MP_CMD_CAPTURING, 1},
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
#ifdef CONFIG_FREETYPE
    { "sub_scale", MP_CMD_SUB_SCALE, 0},
#endif
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
static int set_property_command(MPContext *mpctx, mp_cmd_t *cmd)
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

    if (mp_property_do(pname,M_PROPERTY_GET_TYPE,&prop,mpctx) <= 0 || !prop)
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

#ifdef CONFIG_DVDNAV
static const struct {
  const char *name;
  const mp_command_type cmd;
} mp_dvdnav_bindings[] = {
  { "up",       MP_CMD_DVDNAV_UP              },
  { "down",     MP_CMD_DVDNAV_DOWN            },
  { "left",     MP_CMD_DVDNAV_LEFT            },
  { "right",    MP_CMD_DVDNAV_RIGHT           },
  { "menu",     MP_CMD_DVDNAV_MENU            },
  { "select",   MP_CMD_DVDNAV_SELECT          },
  { "prev",     MP_CMD_DVDNAV_PREVMENU        },
  { "mouse",    MP_CMD_DVDNAV_MOUSECLICK      },

  /*
   * keep old dvdnav sub-command options for a while in order not to
   *  break slave-mode API too suddenly.
   */
  { "1",        MP_CMD_DVDNAV_UP              },
  { "2",        MP_CMD_DVDNAV_DOWN            },
  { "3",        MP_CMD_DVDNAV_LEFT            },
  { "4",        MP_CMD_DVDNAV_RIGHT           },
  { "5",        MP_CMD_DVDNAV_MENU            },
  { "6",        MP_CMD_DVDNAV_SELECT          },
  { "7",        MP_CMD_DVDNAV_PREVMENU        },
  { "8",        MP_CMD_DVDNAV_MOUSECLICK      },
  { NULL,       0                             }
};
#endif

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

static void remove_subtitle_range(MPContext *mpctx, int start, int count)
{
    int idx;
    int end = start + count;
    int after = mpctx->set_of_sub_size - end;
    sub_data **subs = mpctx->set_of_subtitles;
#ifdef CONFIG_ASS
    struct ass_track **ass_tracks = mpctx->set_of_ass_tracks;
#endif
    if (count < 0 || count > mpctx->set_of_sub_size ||
        start < 0 || start > mpctx->set_of_sub_size - count) {
        mp_msg(MSGT_CPLAYER, MSGL_ERR,
               "Cannot remove invalid subtitle range %i +%i\n", start, count);
        return;
    }
    for (idx = start; idx < end; idx++) {
        sub_data *subd = subs[idx];
        mp_msg(MSGT_CPLAYER, MSGL_STATUS,
               "SUB: Removed subtitle file (%d): %s\n", idx + 1,
               filename_recode(subd->filename));
        sub_free(subd);
        subs[idx] = NULL;
#ifdef CONFIG_ASS
        if (ass_tracks[idx])
            ass_free_track(ass_tracks[idx]);
        ass_tracks[idx] = NULL;
#endif
    }

    mpctx->global_sub_size -= count;
    mpctx->set_of_sub_size -= count;
    if (mpctx->set_of_sub_size <= 0)
        mpctx->sub_counts[SUB_SOURCE_SUBS] = 0;

    memmove(subs + start, subs + end, after * sizeof(*subs));
    memset(subs + start + after, 0, count * sizeof(*subs));
#ifdef CONFIG_ASS
    memmove(ass_tracks + start, ass_tracks + end, after * sizeof(*ass_tracks));
    memset(ass_tracks + start + after, 0, count * sizeof(*ass_tracks));
#endif

    if (mpctx->set_of_sub_pos >= start && mpctx->set_of_sub_pos < end) {
        mpctx->global_sub_pos = -2;
        mpctx->subdata = NULL;
#ifdef CONFIG_ASS
        ass_track = NULL;
#endif
        mp_input_queue_cmd(mpctx->input, mp_input_parse_cmd("sub_select"));
    } else if (mpctx->set_of_sub_pos >= end) {
        mpctx->set_of_sub_pos -= count;
        mpctx->global_sub_pos -= count;
    }
}

void run_command(MPContext *mpctx, mp_cmd_t *cmd)
{
    struct MPOpts *opts = &mpctx->opts;
    sh_audio_t * const sh_audio = mpctx->sh_audio;
    sh_video_t * const sh_video = mpctx->sh_video;
    int osd_duration = opts->osd_duration;
    int case_fallthrough_hack = 0;
    if (!set_property_command(mpctx, cmd))
        switch (cmd->id) {
        case MP_CMD_SEEK:{
                mpctx->add_osd_seek_info = true;
                float v = cmd->args[0].v.f;
                int abs = (cmd->nargs > 1) ? cmd->args[1].v.i : 0;
                int exact = (cmd->nargs > 2) ? cmd->args[2].v.i : 0;
                if (abs == 2) { /* Absolute seek to a specific timestamp in seconds */
                    queue_seek(mpctx, MPSEEK_ABSOLUTE, v, exact);
                    mpctx->osd_function = v > get_current_time(mpctx) ?
                        OSD_FFW : OSD_REW;
                } else if (abs) {       /* Absolute seek by percentage */
                    queue_seek(mpctx, MPSEEK_FACTOR, v / 100.0, exact);
                    mpctx->osd_function = OSD_FFW;  // Direction isn't set correctly
                } else {
                    queue_seek(mpctx, MPSEEK_RELATIVE, v, exact);
                    mpctx->osd_function = (v > 0) ? OSD_FFW : OSD_REW;
                }
            }
            break;

        case MP_CMD_SET_PROPERTY_OSD:
            case_fallthrough_hack = 1;

        case MP_CMD_SET_PROPERTY:{
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
                    mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_ERROR=%s\n", property_error_string(r));
            }
            break;

        case MP_CMD_STEP_PROPERTY_OSD:
            case_fallthrough_hack = 1;

        case MP_CMD_STEP_PROPERTY:{
                void* arg = NULL;
                int r,i;
                double d;
                off_t o;
                if (cmd->args[1].v.f) {
                    m_option_t* prop;
                    if((r = mp_property_do(cmd->args[0].v.s,
                                           M_PROPERTY_GET_TYPE,
                                           &prop, mpctx)) <= 0)
                        goto step_prop_err;
                    if(prop->type == CONF_TYPE_INT ||
                       prop->type == CONF_TYPE_FLAG)
                        i = cmd->args[1].v.f, arg = &i;
                    else if(prop->type == CONF_TYPE_FLOAT)
                        arg = &cmd->args[1].v.f;
                    else if(prop->type == CONF_TYPE_DOUBLE ||
                            prop->type == CONF_TYPE_TIME)
                        d = cmd->args[1].v.f, arg = &d;
                    else if(prop->type == CONF_TYPE_POSITION)
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
                    mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_ERROR=%s\n", property_error_string(r));
            }
            break;

        case MP_CMD_GET_PROPERTY:{
                char *tmp;
                int r = mp_property_do(cmd->args[0].v.s, M_PROPERTY_TO_STRING,
                                       &tmp, mpctx);
                if (r <= 0) {
                    mp_msg(MSGT_CPLAYER, MSGL_WARN,
                           "Failed to get value of property '%s'.\n",
                           cmd->args[0].v.s);
                    mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_ERROR=%s\n", property_error_string(r));
                    break;
                }
                mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_%s=%s\n",
                       cmd->args[0].v.s, tmp);
                free(tmp);
            }
            break;

        case MP_CMD_EDL_MARK:
            if (edl_fd) {
                float v = get_current_time(mpctx);
                if (mpctx->begin_skip == MP_NOPTS_VALUE) {
                    mpctx->begin_skip = v;
                    mp_tmsg(MSGT_CPLAYER, MSGL_INFO, "EDL skip start, press 'i' again to end block.\n");
                } else {
                    if (mpctx->begin_skip > v)
                        mp_tmsg(MSGT_CPLAYER, MSGL_WARN, "EDL skip canceled, last start > stop\n");
                    else {
                        fprintf(edl_fd, "%f %f %d\n", mpctx->begin_skip, v, 0);
                        mp_tmsg(MSGT_CPLAYER, MSGL_INFO, "EDL skip end, line written.\n");
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
            mpcodecs_config_vo(sh_video, sh_video->disp_w, sh_video->disp_h, 0);
            break;

        case MP_CMD_SPEED_INCR:{
                float v = cmd->args[0].v.f;
                opts->playback_speed += v;
                reinit_audio_chain(mpctx);
                set_osd_tmsg(OSD_MSG_SPEED, 1, osd_duration, "Speed: x %6.2f",
                             opts->playback_speed);
            } break;

        case MP_CMD_SPEED_MULT:{
                float v = cmd->args[0].v.f;
                opts->playback_speed *= v;
                reinit_audio_chain(mpctx);
                set_osd_tmsg(OSD_MSG_SPEED, 1, osd_duration, "Speed: x %6.2f",
                             opts->playback_speed);
            } break;

        case MP_CMD_SPEED_SET:{
                float v = cmd->args[0].v.f;
                opts->playback_speed = v;
                reinit_audio_chain(mpctx);
                set_osd_tmsg(OSD_MSG_SPEED, 1, osd_duration, "Speed: x %6.2f",
                             opts->playback_speed);
            } break;

        case MP_CMD_FRAME_STEP:
            add_step_frame(mpctx);
            break;

        case MP_CMD_FILE_FILTER:
            file_filter = cmd->args[0].v.i;
            break;

        case MP_CMD_QUIT:
            exit_player_with_rc(mpctx, EXIT_QUIT,
                                (cmd->nargs > 0) ? cmd->args[0].v.i : 0);

        case MP_CMD_PLAY_TREE_STEP:{
                int n = cmd->args[0].v.i == 0 ? 1 : cmd->args[0].v.i;
                int force = cmd->args[1].v.i;

                {
                    if (!force && mpctx->playtree_iter) {
                        play_tree_iter_t *i =
                            play_tree_iter_new_copy(mpctx->playtree_iter);
                        if (play_tree_iter_step(i, n, 0) ==
                            PLAY_TREE_ITER_ENTRY)
                            mpctx->stop_play =
                                (n > 0) ? PT_NEXT_ENTRY : PT_PREV_ENTRY;
                        play_tree_iter_free(i);
                    } else
                        mpctx->stop_play = (n > 0) ? PT_NEXT_ENTRY : PT_PREV_ENTRY;
                    if (mpctx->stop_play)
                        mpctx->play_tree_step = n;
                }
            }
            break;

        case MP_CMD_PLAY_TREE_UP_STEP:{
                int n = cmd->args[0].v.i > 0 ? 1 : -1;
                int force = cmd->args[1].v.i;

                if (!force && mpctx->playtree_iter) {
                    play_tree_iter_t *i =
                        play_tree_iter_new_copy(mpctx->playtree_iter);
                    if (play_tree_iter_up_step(i, n, 0) == PLAY_TREE_ITER_ENTRY)
                        mpctx->stop_play = (n > 0) ? PT_UP_NEXT : PT_UP_PREV;
                    play_tree_iter_free(i);
                } else
                    mpctx->stop_play = (n > 0) ? PT_UP_NEXT : PT_UP_PREV;
            }
            break;

        case MP_CMD_PLAY_ALT_SRC_STEP:
            if (mpctx->playtree_iter && mpctx->playtree_iter->num_files > 1) {
                int v = cmd->args[0].v.i;
                if (v > 0
                    && mpctx->playtree_iter->file <
                    mpctx->playtree_iter->num_files)
                    mpctx->stop_play = PT_NEXT_SRC;
                else if (v < 0 && mpctx->playtree_iter->file > 1)
                    mpctx->stop_play = PT_PREV_SRC;
            }
            break;

        case MP_CMD_SUB_STEP:
            if (sh_video) {
                int movement = cmd->args[0].v.i;
                step_sub(mpctx->subdata, mpctx->video_pts, movement);
#ifdef CONFIG_ASS
                if (ass_track)
                    sub_delay +=
                        ass_step_sub(ass_track,
                                     (mpctx->video_pts +
                                      sub_delay) * 1000 + .5, movement) / 1000.;
#endif
                set_osd_tmsg(OSD_MSG_SUB_DELAY, 1, osd_duration,
                             "Sub delay: %d ms", ROUND(sub_delay * 1000));
            }
            break;

        case MP_CMD_SUB_LOG:
            log_sub(mpctx);
            break;

        case MP_CMD_OSD:{
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
                    set_osd_tmsg(OSD_MSG_OSD_STATUS, 0, osd_duration,
                                 "OSD: %s",
                                 opts->osd_level ? mp_gtext("enabled") :
                                 mp_gtext("disabled"));
                else
                    rm_osd_msg(OSD_MSG_OSD_STATUS);
            }
            break;

        case MP_CMD_OSD_SHOW_TEXT:
            set_osd_msg(OSD_MSG_TEXT, cmd->args[2].v.i,
                        (cmd->args[1].v.i <
                         0 ? osd_duration : cmd->args[1].v.i),
                        "%-.63s", cmd->args[0].v.s);
            break;

        case MP_CMD_OSD_SHOW_PROPERTY_TEXT:{
                char *txt = m_properties_expand_string(mp_properties,
                                                       cmd->args[0].v.s,
                                                       mpctx);
                /* if no argument supplied take default osd_duration, else <arg> ms. */
                if (txt) {
                    set_osd_msg(OSD_MSG_TEXT, cmd->args[2].v.i,
                                (cmd->args[1].v.i <
                                 0 ? osd_duration : cmd->args[1].v.i),
                                "%-.63s", txt);
                    free(txt);
                }
            }
            break;

        case MP_CMD_LOADFILE:{
                play_tree_t *e = play_tree_new();
                play_tree_add_file(e, cmd->args[0].v.s);

                if (cmd->args[1].v.i)   // append
                    play_tree_append_entry(mpctx->playtree->child, e);
                else {
                    // Go back to the starting point.
                    while (play_tree_iter_up_step
                           (mpctx->playtree_iter, 0, 1) != PLAY_TREE_ITER_END)
                        /* NOP */ ;
                    play_tree_free_list(mpctx->playtree->child, 1);
                    play_tree_set_child(mpctx->playtree, e);
                    pt_iter_goto_head(mpctx->playtree_iter);
                    mpctx->stop_play = PT_NEXT_SRC;
                }
            }
            break;

        case MP_CMD_LOADLIST:{
            play_tree_t *e = parse_playlist_file(mpctx->mconfig, cmd->args[0].v.s);
                if (!e)
                    mp_tmsg(MSGT_CPLAYER, MSGL_ERR,
                           "\nUnable to load playlist %s.\n", cmd->args[0].v.s);
                else {
                    if (cmd->args[1].v.i)       // append
                        play_tree_append_entry(mpctx->playtree->child, e);
                    else {
                        // Go back to the starting point.
                        while (play_tree_iter_up_step
                               (mpctx->playtree_iter, 0, 1)
                               != PLAY_TREE_ITER_END)
                            /* NOP */ ;
                        play_tree_free_list(mpctx->playtree->child, 1);
                        play_tree_set_child(mpctx->playtree, e);
                        pt_iter_goto_head(mpctx->playtree_iter);
                        mpctx->stop_play = PT_NEXT_SRC;
                    }
                }
            }
            break;

        case MP_CMD_STOP:
            // Go back to the starting point.
            while (play_tree_iter_up_step
                   (mpctx->playtree_iter, 0, 1) != PLAY_TREE_ITER_END)
                /* NOP */ ;
            mpctx->stop_play = PT_STOP;
            break;

        case MP_CMD_OSD_SHOW_PROGRESSION:{
                int len = get_time_length(mpctx);
                int pts = get_current_time(mpctx);
                set_osd_bar(mpctx, 0, "Position", 0, 100, get_percent_pos(mpctx));
                set_osd_msg(OSD_MSG_TEXT, 1, osd_duration,
                            "%c %02d:%02d:%02d / %02d:%02d:%02d",
                            mpctx->osd_function, pts/3600, (pts/60)%60, pts%60,
                            len/3600, (len/60)%60, len%60);
            }
            break;

#ifdef CONFIG_RADIO
        case MP_CMD_RADIO_STEP_CHANNEL:
            if (mpctx->demuxer->stream->type == STREAMTYPE_RADIO) {
                int v = cmd->args[0].v.i;
                if (v > 0)
                    radio_step_channel(mpctx->demuxer->stream,
                                       RADIO_CHANNEL_HIGHER);
                else
                    radio_step_channel(mpctx->demuxer->stream,
                                       RADIO_CHANNEL_LOWER);
                if (radio_get_channel_name(mpctx->demuxer->stream)) {
                    set_osd_tmsg(OSD_MSG_RADIO_CHANNEL, 1, osd_duration,
                                 "Channel: %s",
                                 radio_get_channel_name(mpctx->demuxer->stream));
                }
            }
            break;

        case MP_CMD_RADIO_SET_CHANNEL:
            if (mpctx->demuxer->stream->type == STREAMTYPE_RADIO) {
                radio_set_channel(mpctx->demuxer->stream, cmd->args[0].v.s);
                if (radio_get_channel_name(mpctx->demuxer->stream)) {
                    set_osd_tmsg(OSD_MSG_RADIO_CHANNEL, 1, osd_duration,
                                 "Channel: %s",
                                 radio_get_channel_name(mpctx->demuxer->stream));
                }
            }
            break;

        case MP_CMD_RADIO_SET_FREQ:
            if (mpctx->demuxer->stream->type == STREAMTYPE_RADIO)
                radio_set_freq(mpctx->demuxer->stream, cmd->args[0].v.f);
            break;

        case MP_CMD_RADIO_STEP_FREQ:
            if (mpctx->demuxer->stream->type == STREAMTYPE_RADIO)
                radio_step_freq(mpctx->demuxer->stream, cmd->args[0].v.f);
            break;
#endif

#ifdef CONFIG_TV
        case MP_CMD_TV_START_SCAN:
            if (mpctx->file_format == DEMUXER_TYPE_TV)
                tv_start_scan((tvi_handle_t *) (mpctx->demuxer->priv),1);
            break;
        case MP_CMD_TV_SET_FREQ:
            if (mpctx->file_format == DEMUXER_TYPE_TV)
                tv_set_freq((tvi_handle_t *) (mpctx->demuxer->priv),
                            cmd->args[0].v.f * 16.0);
#ifdef CONFIG_PVR
            else if (mpctx->stream && mpctx->stream->type == STREAMTYPE_PVR) {
              pvr_set_freq (mpctx->stream, ROUND (cmd->args[0].v.f));
              set_osd_msg (OSD_MSG_TV_CHANNEL, 1, osd_duration, "%s: %s",
                           pvr_get_current_channelname (mpctx->stream),
                           pvr_get_current_stationname (mpctx->stream));
            }
#endif /* CONFIG_PVR */
            break;

        case MP_CMD_TV_STEP_FREQ:
            if (mpctx->file_format == DEMUXER_TYPE_TV)
                tv_step_freq((tvi_handle_t *) (mpctx->demuxer->priv),
                            cmd->args[0].v.f * 16.0);
#ifdef CONFIG_PVR
            else if (mpctx->stream && mpctx->stream->type == STREAMTYPE_PVR) {
              pvr_force_freq_step (mpctx->stream, ROUND (cmd->args[0].v.f));
              set_osd_msg (OSD_MSG_TV_CHANNEL, 1, osd_duration, "%s: f %d",
                           pvr_get_current_channelname (mpctx->stream),
                           pvr_get_current_frequency (mpctx->stream));
            }
#endif /* CONFIG_PVR */
            break;

        case MP_CMD_TV_SET_NORM:
            if (mpctx->file_format == DEMUXER_TYPE_TV)
                tv_set_norm((tvi_handle_t *) (mpctx->demuxer->priv),
                            cmd->args[0].v.s);
            break;

        case MP_CMD_TV_STEP_CHANNEL:{
                if (mpctx->file_format == DEMUXER_TYPE_TV) {
                    int v = cmd->args[0].v.i;
                    if (v > 0) {
                        tv_step_channel((tvi_handle_t *) (mpctx->
                                                          demuxer->priv),
                                        TV_CHANNEL_HIGHER);
                    } else {
                        tv_step_channel((tvi_handle_t *) (mpctx->
                                                          demuxer->priv),
                                        TV_CHANNEL_LOWER);
                    }
                    if (tv_channel_list) {
                        set_osd_tmsg(OSD_MSG_TV_CHANNEL, 1, osd_duration,
                                     "Channel: %s", tv_channel_current->name);
                        //vo_osd_changed(OSDTYPE_SUBTITLE);
                    }
                }
#ifdef CONFIG_PVR
                else if (mpctx->stream &&
                         mpctx->stream->type == STREAMTYPE_PVR) {
                  pvr_set_channel_step (mpctx->stream, cmd->args[0].v.i);
                  set_osd_msg (OSD_MSG_TV_CHANNEL, 1, osd_duration, "%s: %s",
                               pvr_get_current_channelname (mpctx->stream),
                               pvr_get_current_stationname (mpctx->stream));
                }
#endif /* CONFIG_PVR */
            }
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
            if (mpctx->file_format == DEMUXER_TYPE_TV) {
                tv_set_channel((tvi_handle_t *) (mpctx->demuxer->priv),
                               cmd->args[0].v.s);
                if (tv_channel_list) {
                    set_osd_tmsg(OSD_MSG_TV_CHANNEL, 1, osd_duration,
                                 "Channel: %s", tv_channel_current->name);
                    //vo_osd_changed(OSDTYPE_SUBTITLE);
                }
            }
#ifdef CONFIG_PVR
            else if (mpctx->stream && mpctx->stream->type == STREAMTYPE_PVR) {
              pvr_set_channel (mpctx->stream, cmd->args[0].v.s);
              set_osd_msg (OSD_MSG_TV_CHANNEL, 1, osd_duration, "%s: %s",
                           pvr_get_current_channelname (mpctx->stream),
                           pvr_get_current_stationname (mpctx->stream));
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
            if (mpctx->file_format == DEMUXER_TYPE_TV) {
                tv_last_channel((tvi_handle_t *) (mpctx->demuxer->priv));
                if (tv_channel_list) {
                    set_osd_tmsg(OSD_MSG_TV_CHANNEL, 1, osd_duration,
                                 "Channel: %s", tv_channel_current->name);
                    //vo_osd_changed(OSDTYPE_SUBTITLE);
                }
            }
#ifdef CONFIG_PVR
            else if (mpctx->stream && mpctx->stream->type == STREAMTYPE_PVR) {
              pvr_set_lastchannel (mpctx->stream);
              set_osd_msg (OSD_MSG_TV_CHANNEL, 1, osd_duration, "%s: %s",
                           pvr_get_current_channelname (mpctx->stream),
                           pvr_get_current_stationname (mpctx->stream));
            }
#endif /* CONFIG_PVR */
            break;

        case MP_CMD_TV_STEP_NORM:
            if (mpctx->file_format == DEMUXER_TYPE_TV)
                tv_step_norm((tvi_handle_t *) (mpctx->demuxer->priv));
            break;

        case MP_CMD_TV_STEP_CHANNEL_LIST:
            if (mpctx->file_format == DEMUXER_TYPE_TV)
                tv_step_chanlist((tvi_handle_t *) (mpctx->demuxer->priv));
            break;
#endif /* CONFIG_TV */
        case MP_CMD_TV_TELETEXT_ADD_DEC:
            if (mpctx->demuxer->teletext)
                teletext_control(mpctx->demuxer->teletext,TV_VBI_CONTROL_ADD_DEC,
                                 &(cmd->args[0].v.s));
            break;
        case MP_CMD_TV_TELETEXT_GO_LINK:
            if (mpctx->demuxer->teletext)
                teletext_control(mpctx->demuxer->teletext,TV_VBI_CONTROL_GO_LINK,
                                 &(cmd->args[0].v.i));
            break;

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

        case MP_CMD_SUB_REMOVE:
            if (sh_video) {
                int v = cmd->args[0].v.i;
                if (v < 0) {
                    remove_subtitle_range(mpctx, 0, mpctx->set_of_sub_size);
                } else if (v < mpctx->set_of_sub_size) {
                    remove_subtitle_range(mpctx, v, 1);
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
            if (mpctx->video_out && mpctx->video_out->config_ok) {
                mp_msg(MSGT_CPLAYER, MSGL_INFO, "sending VFCTRL_SCREENSHOT!\n");
                if (CONTROL_OK !=
                    ((vf_instance_t *) sh_video->vfilter)->
                    control(sh_video->vfilter, VFCTRL_SCREENSHOT,
                            &cmd->args[0].v.i))
                    mp_msg(MSGT_CPLAYER, MSGL_INFO, "failed (forgot -vf screenshot?)\n");
            }
            break;

        case MP_CMD_VF_CHANGE_RECTANGLE:
            if (!sh_video)
                break;
            set_rectangle(sh_video, cmd->args[0].v.i, cmd->args[1].v.i);
            break;

        case MP_CMD_GET_TIME_LENGTH:{
                mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_LENGTH=%.2f\n",
                       get_time_length(mpctx));
            }
            break;

        case MP_CMD_GET_FILENAME:{
            char *inf = get_metadata(mpctx, META_NAME);
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_FILENAME='%s'\n", inf);
            talloc_free(inf);
            }
            break;

        case MP_CMD_GET_VIDEO_CODEC:{
            char *inf = get_metadata(mpctx, META_VIDEO_CODEC);
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_VIDEO_CODEC='%s'\n", inf);
            talloc_free(inf);
            }
            break;

        case MP_CMD_GET_VIDEO_BITRATE:{
            char *inf = get_metadata(mpctx, META_VIDEO_BITRATE);
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_VIDEO_BITRATE='%s'\n", inf);
            talloc_free(inf);
            }
            break;

        case MP_CMD_GET_VIDEO_RESOLUTION:{
            char *inf = get_metadata(mpctx, META_VIDEO_RESOLUTION);
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_VIDEO_RESOLUTION='%s'\n", inf);
            talloc_free(inf);
            }
            break;

        case MP_CMD_GET_AUDIO_CODEC:{
            char *inf = get_metadata(mpctx, META_AUDIO_CODEC);
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_AUDIO_CODEC='%s'\n", inf);
            talloc_free(inf);
            }
            break;

        case MP_CMD_GET_AUDIO_BITRATE:{
            char *inf = get_metadata(mpctx, META_AUDIO_BITRATE);
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_AUDIO_BITRATE='%s'\n", inf);
            talloc_free(inf);
            }
            break;

        case MP_CMD_GET_AUDIO_SAMPLES:{
            char *inf = get_metadata(mpctx, META_AUDIO_SAMPLES);
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_AUDIO_SAMPLES='%s'\n", inf);
            talloc_free(inf);
            }
            break;

        case MP_CMD_GET_META_TITLE:{
            char *inf = get_metadata(mpctx, META_INFO_TITLE);
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_TITLE='%s'\n", inf);
            talloc_free(inf);
            }
            break;

        case MP_CMD_GET_META_ARTIST:{
            char *inf = get_metadata(mpctx, META_INFO_ARTIST);
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_ARTIST='%s'\n", inf);
            talloc_free(inf);
            }
            break;

        case MP_CMD_GET_META_ALBUM:{
            char *inf = get_metadata(mpctx, META_INFO_ALBUM);
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_ALBUM='%s'\n", inf);
            talloc_free(inf);
            }
            break;

        case MP_CMD_GET_META_YEAR:{
            char *inf = get_metadata(mpctx, META_INFO_YEAR);
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_YEAR='%s'\n", inf);
            talloc_free(inf);
            }
            break;

        case MP_CMD_GET_META_COMMENT:{
            char *inf = get_metadata(mpctx, META_INFO_COMMENT);
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_COMMENT='%s'\n", inf);
            talloc_free(inf);
            }
            break;

        case MP_CMD_GET_META_TRACK:{
            char *inf = get_metadata(mpctx, META_INFO_TRACK);
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_TRACK='%s'\n", inf);
            talloc_free(inf);
            }
            break;

        case MP_CMD_GET_META_GENRE:{
            char *inf = get_metadata(mpctx, META_INFO_GENRE);
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_GENRE='%s'\n", inf);
            talloc_free(inf);
            }
            break;

        case MP_CMD_GET_VO_FULLSCREEN:
            if (mpctx->video_out && mpctx->video_out->config_ok)
                mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_VO_FULLSCREEN=%d\n", vo_fs);
            break;

        case MP_CMD_GET_PERCENT_POS:
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_PERCENT_POSITION=%d\n",
                   get_percent_pos(mpctx));
            break;

        case MP_CMD_GET_TIME_POS:{
                float pos = get_current_time(mpctx);
                mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_TIME_POSITION=%.1f\n", pos);
            }
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
            mplayer_put_key(mpctx->key_fifo, cmd->args[0].v.i);
            break;

        case MP_CMD_SET_MOUSE_POS:{
                int pointer_x, pointer_y;
                double dx, dy;
                pointer_x = cmd->args[0].v.i;
                pointer_y = cmd->args[1].v.i;
                rescale_input_coordinates(mpctx, pointer_x, pointer_y, &dx, &dy);
#ifdef CONFIG_DVDNAV
                if (mpctx->stream->type == STREAMTYPE_DVDNAV
                    && dx > 0.0 && dy > 0.0) {
                    int button = -1;
                    pointer_x = (int) (dx * (double) sh_video->disp_w);
                    pointer_y = (int) (dy * (double) sh_video->disp_h);
                    mp_dvdnav_update_mouse_pos(mpctx->stream,
                                               pointer_x, pointer_y, &button);
                    if (opts->osd_level > 1 && button > 0)
                        set_osd_msg(OSD_MSG_TEXT, 1, osd_duration,
                                    "Selected button number %d", button);
                }
#endif
#ifdef CONFIG_MENU
                if (use_menu && dx >= 0.0 && dy >= 0.0)
                    menu_update_mouse_pos(dx, dy);
#endif
            }
            break;

#ifdef CONFIG_DVDNAV
        case MP_CMD_DVDNAV:{
                int button = -1;
                int i;
                mp_command_type command = 0;
                if (mpctx->stream->type != STREAMTYPE_DVDNAV)
                    break;

                for (i = 0; mp_dvdnav_bindings[i].name; i++)
                  if (cmd->args[0].v.s &&
                      !strcasecmp (cmd->args[0].v.s,
                                   mp_dvdnav_bindings[i].name))
                    command = mp_dvdnav_bindings[i].cmd;

                mp_dvdnav_handle_input(mpctx->stream,command,&button);
                if (opts->osd_level > 1 && button > 0)
                    set_osd_msg(OSD_MSG_TEXT, 1, osd_duration,
                                "Selected button number %d", button);
            }
            break;

        case MP_CMD_SWITCH_TITLE:
            if (mpctx->stream->type == STREAMTYPE_DVDNAV)
                mp_dvdnav_switch_title(mpctx->stream, cmd->args[0].v.i);
            break;

#endif

    case MP_CMD_AF_SWITCH:
        if (sh_audio)
        {
            af_uninit(mpctx->mixer.afilter);
            af_init(mpctx->mixer.afilter);
        }
    case MP_CMD_AF_ADD:
    case MP_CMD_AF_DEL:
        if (!sh_audio)
            break;
        {
            char* af_args = strdup(cmd->args[0].v.s);
            char* af_commands = af_args;
            char* af_command;
            af_instance_t* af;
            while ((af_command = strsep(&af_commands, ",")) != NULL)
            {
                if (cmd->id == MP_CMD_AF_DEL)
                {
                    af = af_get(mpctx->mixer.afilter, af_command);
                    if (af != NULL)
                        af_remove(mpctx->mixer.afilter, af);
                }
                else
                    af_add(mpctx->mixer.afilter, af_command);
            }
            reinit_audio_chain(mpctx);
            free(af_args);
        }
        break;
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
