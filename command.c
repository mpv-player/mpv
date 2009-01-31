#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>

#include "config.h"
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
#include "help_mp.h"
#include "metadata.h"
#include "libmpcodecs/vf.h"
#include "libmpcodecs/vd.h"
#include "libvo/video_out.h"
#include "libvo/font_load.h"
#include "playtree.h"
#include "libao2/audio_out.h"
#include "mpcommon.h"
#include "mixer.h"
#include "libmpcodecs/dec_video.h"
#include "vobsub.h"
#include "spudec.h"
#include "get_path.h"
#ifdef CONFIG_TV
#include "stream/tv.h"
#endif
#ifdef CONFIG_RADIO
#include "stream/stream_radio.h"
#endif
#ifdef CONFIG_PVR
#include "stream/pvr.h"
#endif
#ifdef CONFIG_DVBIN
#include "stream/dvbin.h"
#endif
#ifdef CONFIG_DVDREAD
#include "stream/stream_dvd.h"
#endif
#ifdef CONFIG_DVDNAV
#include "stream/stream_dvdnav.h"
#endif
#ifdef CONFIG_ASS
#include "libass/ass.h"
#include "libass/ass_mp.h"
#endif
#ifdef CONFIG_MENU
#include "m_struct.h"
#include "libmenu/menu.h"
#endif
#ifdef CONFIG_GUI
#include "gui/interface.h"
#endif

#include "mp_core.h"
#include "mp_fifo.h"
#include "libavutil/avstring.h"

#define ROUND(x) ((int)((x)<0 ? (x)-0.5 : (x)+0.5))

extern int use_menu;

static void rescale_input_coordinates(int ix, int iy, double *dx, double *dy)
{
    //remove the borders, if any, and rescale to the range [0,1],[0,1]
    if (vo_fs) {		//we are in full-screen mode
	if (vo_screenwidth > vo_dwidth)	//there are borders along the x axis
	    ix -= (vo_screenwidth - vo_dwidth) / 2;
	if (vo_screenheight > vo_dheight)	//there are borders along the y axis (usual way)
	    iy -= (vo_screenheight - vo_dheight) / 2;

	if (ix < 0 || ix > vo_dwidth) {
	    *dx = *dy = -1.0;
	    return;
	}			//we are on one of the borders
	if (iy < 0 || iy > vo_dheight) {
	    *dx = *dy = -1.0;
	    return;
	}			//we are on one of the borders
    }

    *dx = (double) ix / (double) vo_dwidth;
    *dy = (double) iy / (double) vo_dheight;

    mp_msg(MSGT_CPLAYER, MSGL_V,
	   "\r\nrescaled coordinates: %.3lf, %.3lf, screen (%d x %d), vodisplay: (%d, %d), fullscreen: %d\r\n",
	   *dx, *dy, vo_screenwidth, vo_screenheight, vo_dwidth,
	   vo_dheight, vo_fs);
}

static int sub_source_by_pos(MPContext * mpctx, int pos)
{
    int source = -1;
    int top = -1;
    int i;
    for (i = 0; i < SUB_SOURCES; i++) {
	int j = mpctx->global_sub_indices[i];
	if ((j >= 0) && (j > top) && (pos >= j)) {
	    source = i;
	    top = j;
	}
    }
    return source;
}

static int sub_source(MPContext * mpctx)
{
    return sub_source_by_pos(mpctx, mpctx->global_sub_pos);
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

static void log_sub(void)
{
    char *fname;
    FILE *f;
    int i;

    if (subdata == NULL || vo_sub_last == NULL)
	return;
    fname = get_path("subtitle_log");
    f = fopen(fname, "a");
    if (!f)
	return;
    fprintf(f, "----------------------------------------------------------\n");
    if (subdata->sub_uses_time) {
	fprintf(f,
		"N: %s S: %02ld:%02ld:%02ld.%02ld E: %02ld:%02ld:%02ld.%02ld\n",
		filename, vo_sub_last->start / 360000,
		(vo_sub_last->start / 6000) % 60,
		(vo_sub_last->start / 100) % 60, vo_sub_last->start % 100,
		vo_sub_last->end / 360000, (vo_sub_last->end / 6000) % 60,
		(vo_sub_last->end / 100) % 60, vo_sub_last->end % 100);
    } else {
	fprintf(f, "N: %s S: %ld E: %ld\n", filename, vo_sub_last->start,
		vo_sub_last->end);
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

/// OSD level (RW)
static int mp_property_osdlevel(m_option_t * prop, int action, void *arg,
				MPContext * mpctx)
{
    return m_property_choice(prop, action, arg, &osd_level);
}

/// Loop (RW)
static int mp_property_loop(m_option_t * prop, int action, void *arg,
                            MPContext * mpctx)
{
    switch (action) {
    case M_PROPERTY_PRINT:
        if (!arg) return M_PROPERTY_ERROR;
        if (mpctx->loop_times < 0)
            *(char**)arg = strdup("off");
        else if (mpctx->loop_times == 0)
            *(char**)arg = strdup("inf");
        else
            break;
        return M_PROPERTY_OK;
    }
    return m_property_int_range(prop, action, arg, &mpctx->loop_times);
}

/// Playback speed (RW)
static int mp_property_playback_speed(m_option_t * prop, int action,
				      void *arg, MPContext * mpctx)
{
    switch (action) {
    case M_PROPERTY_SET:
	if (!arg)
	    return M_PROPERTY_ERROR;
	M_PROPERTY_CLAMP(prop, *(float *) arg);
	playback_speed = *(float *) arg;
	build_afilter_chain(mpctx->sh_audio, &ao_data);
	return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
	playback_speed += (arg ? *(float *) arg : 0.1) *
	    (action == M_PROPERTY_STEP_DOWN ? -1 : 1);
	M_PROPERTY_CLAMP(prop, playback_speed);
	build_afilter_chain(mpctx->sh_audio, &ao_data);
	return M_PROPERTY_OK;
    }
    return m_property_float_range(prop, action, arg, &playback_speed);
}

/// filename with path (RO)
static int mp_property_path(m_option_t * prop, int action, void *arg,
			    MPContext * mpctx)
{
    return m_property_string_ro(prop, action, arg, filename);
}

/// filename without path (RO)
static int mp_property_filename(m_option_t * prop, int action, void *arg,
				MPContext * mpctx)
{
    char *f;
    if (!filename)
	return M_PROPERTY_UNAVAILABLE;
    if (((f = strrchr(filename, '/')) || (f = strrchr(filename, '\\'))) && f[1])
	f++;
    else
	f = filename;
    return m_property_string_ro(prop, action, arg, f);
}

/// Demuxer name (RO)
static int mp_property_demuxer(m_option_t * prop, int action, void *arg,
			       MPContext * mpctx)
{
    if (!mpctx->demuxer)
	return M_PROPERTY_UNAVAILABLE;
    return m_property_string_ro(prop, action, arg,
				(char *) mpctx->demuxer->desc->name);
}

/// Position in the stream (RW)
static int mp_property_stream_pos(m_option_t * prop, int action, void *arg,
				  MPContext * mpctx)
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
static int mp_property_stream_start(m_option_t * prop, int action,
				    void *arg, MPContext * mpctx)
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
static int mp_property_stream_end(m_option_t * prop, int action, void *arg,
				  MPContext * mpctx)
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
static int mp_property_stream_length(m_option_t * prop, int action,
				     void *arg, MPContext * mpctx)
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

/// Media length in seconds (RO)
static int mp_property_length(m_option_t * prop, int action, void *arg,
			      MPContext * mpctx)
{
    double len;

    if (!mpctx->demuxer ||
	!(int) (len = demuxer_get_time_length(mpctx->demuxer)))
	return M_PROPERTY_UNAVAILABLE;

    return m_property_time_ro(prop, action, arg, len);
}

/// Current position in percent (RW)
static int mp_property_percent_pos(m_option_t * prop, int action,
                                   void *arg, MPContext * mpctx) {
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
        pos = demuxer_get_percent_pos(mpctx->demuxer);
        pos += (arg ? *(int*)arg : 10) *
            (action == M_PROPERTY_STEP_UP ? 1 : -1);
        M_PROPERTY_CLAMP(prop, pos);
        break;
    default:
        return m_property_int_ro(prop, action, arg,
                                 demuxer_get_percent_pos(mpctx->demuxer));
    }

    abs_seek_pos = SEEK_ABSOLUTE | SEEK_FACTOR;
    rel_seek_secs = pos / 100.0;
    return M_PROPERTY_OK;
}

/// Current position in seconds (RW)
static int mp_property_time_pos(m_option_t * prop, int action,
                                void *arg, MPContext * mpctx) {
    if (!(mpctx->sh_video || (mpctx->sh_audio && mpctx->audio_out)))
        return M_PROPERTY_UNAVAILABLE;

    switch(action) {
    case M_PROPERTY_SET:
        if(!arg) return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(double*)arg);
        abs_seek_pos = SEEK_ABSOLUTE;
        rel_seek_secs = *(double*)arg;
        return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        rel_seek_secs += (arg ? *(double*)arg : 10.0) *
            (action == M_PROPERTY_STEP_UP ? 1.0 : -1.0);
        return M_PROPERTY_OK;
    }
    return m_property_time_ro(prop, action, arg,
                              mpctx->sh_video ? mpctx->sh_video->pts :
                              playing_audio_pts(mpctx->sh_audio,
                                                mpctx->d_audio,
                                                mpctx->audio_out));
}

/// Current chapter (RW)
static int mp_property_chapter(m_option_t *prop, int action, void *arg,
                               MPContext *mpctx)
{
    int chapter = -1;
    float next_pts = 0;
    int chapter_num;
    int step_all;
    char *chapter_name = NULL;

    if (mpctx->demuxer)
        chapter = demuxer_get_current_chapter(mpctx->demuxer);
    if (chapter < 0)
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
        chapter_name = demuxer_chapter_display_name(mpctx->demuxer, chapter);
        if (!chapter_name)
            return M_PROPERTY_UNAVAILABLE;
        *(char **) arg = chapter_name;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(int*)arg);
        step_all = *(int *)arg - (chapter + 1);
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
    rel_seek_secs = 0;
    abs_seek_pos = 0;
    chapter = demuxer_seek_chapter(mpctx->demuxer, chapter, 1,
                                   &next_pts, &chapter_num, &chapter_name);
    if (chapter >= 0) {
        if (next_pts > -1.0) {
            abs_seek_pos = SEEK_ABSOLUTE;
            rel_seek_secs = next_pts;
        }
        if (chapter_name)
            set_osd_msg(OSD_MSG_TEXT, 1, osd_duration,
                        MSGTR_OSDChapter, chapter + 1, chapter_name);
    }
    else if (step_all > 0)
        rel_seek_secs = 1000000000.;
    else
        set_osd_msg(OSD_MSG_TEXT, 1, osd_duration,
                    MSGTR_OSDChapter, 0, MSGTR_Unknown);
    if (chapter_name)
        free(chapter_name);
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
    set_osd_msg(OSD_MSG_TEXT, 1, osd_duration,
                        MSGTR_OSDAngle, angle, angles);
    if (angle_name)
        free(angle_name);
    return M_PROPERTY_OK;
}

/// Demuxer meta data
static int mp_property_metadata(m_option_t * prop, int action, void *arg,
                                MPContext * mpctx) {
    m_property_action_t* ka;
    char* meta;
    static m_option_t key_type =
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
            *(m_option_t**)ka->arg = &key_type;
            return M_PROPERTY_OK;
        }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_pause(m_option_t * prop, int action, void *arg,
                             MPContext * mpctx)
{
    return m_property_flag_ro(prop, action, arg, mpctx->osd_function == OSD_PAUSE);
}


///@}

/// \defgroup AudioProperties Audio properties
/// \ingroup Properties
///@{

/// Volume (RW)
static int mp_property_volume(m_option_t * prop, int action, void *arg,
			      MPContext * mpctx)
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
static int mp_property_mute(m_option_t * prop, int action, void *arg,
			    MPContext * mpctx)
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
	    *(char **) arg = strdup(MSGTR_EnabledEdl);
	    return M_PROPERTY_OK;
	}
    default:
	return m_property_flag(prop, action, arg, &mpctx->mixer.muted);

    }
}

/// Audio delay (RW)
static int mp_property_audio_delay(m_option_t * prop, int action,
				   void *arg, MPContext * mpctx)
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
static int mp_property_audio_format(m_option_t * prop, int action,
				    void *arg, MPContext * mpctx)
{
    if (!mpctx->sh_audio)
	return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(prop, action, arg, mpctx->sh_audio->format);
}

/// Audio codec name (RO)
static int mp_property_audio_codec(m_option_t * prop, int action,
                                   void *arg, MPContext * mpctx)
{
    if (!mpctx->sh_audio || !mpctx->sh_audio->codec)
	return M_PROPERTY_UNAVAILABLE;
    return m_property_string_ro(prop, action, arg, mpctx->sh_audio->codec->name);
}

/// Audio bitrate (RO)
static int mp_property_audio_bitrate(m_option_t * prop, int action,
				     void *arg, MPContext * mpctx)
{
    if (!mpctx->sh_audio)
	return M_PROPERTY_UNAVAILABLE;
    return m_property_bitrate(prop, action, arg, mpctx->sh_audio->i_bps);
}

/// Samplerate (RO)
static int mp_property_samplerate(m_option_t * prop, int action, void *arg,
				  MPContext * mpctx)
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
static int mp_property_channels(m_option_t * prop, int action, void *arg,
				MPContext * mpctx)
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
static int mp_property_balance(m_option_t * prop, int action, void *arg,
			      MPContext * mpctx)
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
static int mp_property_audio(m_option_t * prop, int action, void *arg,
			     MPContext * mpctx)
{
    int current_id = -1, tmp;

    switch (action) {
    case M_PROPERTY_GET:
	if (!mpctx->sh_audio)
	    return M_PROPERTY_UNAVAILABLE;
	if (!arg)
	    return M_PROPERTY_ERROR;
	*(int *) arg = audio_id;
	return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
	if (!mpctx->sh_audio)
	    return M_PROPERTY_UNAVAILABLE;
	if (!arg)
	    return M_PROPERTY_ERROR;

	if (audio_id < 0)
	    *(char **) arg = strdup(MSGTR_Disabled);
	else {
	    char lang[40] = MSGTR_Unknown;
            sh_audio_t* sh = mpctx->sh_audio;
            if (sh && sh->lang)
                av_strlcpy(lang, sh->lang, 40);
#ifdef CONFIG_DVDREAD
	    else if (mpctx->stream->type == STREAMTYPE_DVD) {
		int code = dvd_lang_from_aid(mpctx->stream, audio_id);
		if (code) {
		    lang[0] = code >> 8;
		    lang[1] = code;
		    lang[2] = 0;
		}
	    }
#endif

#ifdef CONFIG_DVDNAV
	    else if (mpctx->stream->type == STREAMTYPE_DVDNAV)
		mp_dvdnav_lang_from_aid(mpctx->stream, audio_id, lang);
#endif
	    *(char **) arg = malloc(64);
	    snprintf(*(char **) arg, 64, "(%d) %s", audio_id, lang);
	}
	return M_PROPERTY_OK;

    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_SET:
	if (!mpctx->demuxer)
	    return M_PROPERTY_UNAVAILABLE;
	if (action == M_PROPERTY_SET && arg)
	    tmp = *((int *) arg);
	else
	    tmp = -1;
	current_id = mpctx->demuxer->audio->id;
	audio_id = demuxer_switch_audio(mpctx->demuxer, tmp);
	if (audio_id == -2
	    || (audio_id > -1
		&& mpctx->demuxer->audio->id != current_id && current_id != -2))
	    uninit_player(INITIALIZED_AO | INITIALIZED_ACODEC);
	if (audio_id > -1 && mpctx->demuxer->audio->id != current_id) {
	    sh_audio_t *sh2;
	    sh2 = mpctx->demuxer->a_streams[mpctx->demuxer->audio->id];
	    if (sh2) {
		sh2->ds = mpctx->demuxer->audio;
		mpctx->sh_audio = sh2;
		reinit_audio_chain();
	    }
	}
	mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AUDIO_TRACK=%d\n", audio_id);
	return M_PROPERTY_OK;
    default:
	return M_PROPERTY_NOT_IMPLEMENTED;
    }

}

/// Selected video id (RW)
static int mp_property_video(m_option_t * prop, int action, void *arg,
			     MPContext * mpctx)
{
    int current_id = -1, tmp;

    switch (action) {
    case M_PROPERTY_GET:
	if (!mpctx->sh_video)
	    return M_PROPERTY_UNAVAILABLE;
	if (!arg)
	    return M_PROPERTY_ERROR;
	*(int *) arg = video_id;
	return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
	if (!mpctx->sh_video)
	    return M_PROPERTY_UNAVAILABLE;
	if (!arg)
	    return M_PROPERTY_ERROR;

	if (video_id < 0)
	    *(char **) arg = strdup(MSGTR_Disabled);
	else {
	    char lang[40] = MSGTR_Unknown;
	    *(char **) arg = malloc(64);
	    snprintf(*(char **) arg, 64, "(%d) %s", video_id, lang);
	}
	return M_PROPERTY_OK;

    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_SET:
	current_id = mpctx->demuxer->video->id;
	if (action == M_PROPERTY_SET && arg)
	    tmp = *((int *) arg);
	else
	    tmp = -1;
	video_id = demuxer_switch_video(mpctx->demuxer, tmp);
	if (video_id == -2
	    || (video_id > -1 && mpctx->demuxer->video->id != current_id
		&& current_id != -2))
	    uninit_player(INITIALIZED_VCODEC |
			  (fixed_vo && video_id != -2 ? 0 : INITIALIZED_VO));
	if (video_id > -1 && mpctx->demuxer->video->id != current_id) {
	    sh_video_t *sh2;
	    sh2 = mpctx->demuxer->v_streams[mpctx->demuxer->video->id];
	    if (sh2) {
		sh2->ds = mpctx->demuxer->video;
		mpctx->sh_video = sh2;
		reinit_video_chain();
	    }
	}
	mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_TRACK=%d\n", video_id);
	return M_PROPERTY_OK;

    default:
	return M_PROPERTY_NOT_IMPLEMENTED;
    }
}

static int mp_property_program(m_option_t * prop, int action, void *arg,
			       MPContext * mpctx)
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
static int mp_property_fullscreen(m_option_t * prop, int action, void *arg,
				  MPContext * mpctx)
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
#ifdef CONFIG_GUI
	if (use_gui)
	    guiGetEvent(guiIEvent, (char *) MP_CMD_GUI_FULLSCREEN);
	else
#endif
	if (vo_config_count)
	    mpctx->video_out->control(VOCTRL_FULLSCREEN, 0);
	return M_PROPERTY_OK;
    default:
	return m_property_flag(prop, action, arg, &vo_fs);
    }
}

static int mp_property_deinterlace(m_option_t * prop, int action,
				   void *arg, MPContext * mpctx)
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
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Panscan (RW)
static int mp_property_panscan(m_option_t * prop, int action, void *arg,
			       MPContext * mpctx)
{

    if (!mpctx->video_out
	|| mpctx->video_out->control(VOCTRL_GET_PANSCAN, NULL) != VO_TRUE)
	return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET:
	if (!arg)
	    return M_PROPERTY_ERROR;
	M_PROPERTY_CLAMP(prop, *(float *) arg);
	vo_panscan = *(float *) arg;
	mpctx->video_out->control(VOCTRL_SET_PANSCAN, NULL);
	return M_PROPERTY_OK;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
	vo_panscan += (arg ? *(float *) arg : 0.1) *
	    (action == M_PROPERTY_STEP_DOWN ? -1 : 1);
	if (vo_panscan > 1)
	    vo_panscan = 1;
	else if (vo_panscan < 0)
	    vo_panscan = 0;
	mpctx->video_out->control(VOCTRL_SET_PANSCAN, NULL);
	return M_PROPERTY_OK;
    default:
	return m_property_float_range(prop, action, arg, &vo_panscan);
    }
}

/// Helper to set vo flags.
/** \ingroup PropertyImplHelper
 */
static int mp_property_vo_flag(m_option_t * prop, int action, void *arg,
			       int vo_ctrl, int *vo_var, MPContext * mpctx)
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
	if (vo_config_count)
	    mpctx->video_out->control(vo_ctrl, 0);
	return M_PROPERTY_OK;
    default:
	return m_property_flag(prop, action, arg, vo_var);
    }
}

/// Window always on top (RW)
static int mp_property_ontop(m_option_t * prop, int action, void *arg,
			     MPContext * mpctx)
{
    return mp_property_vo_flag(prop, action, arg, VOCTRL_ONTOP, &vo_ontop,
			       mpctx);
}

/// Display in the root window (RW)
static int mp_property_rootwin(m_option_t * prop, int action, void *arg,
			       MPContext * mpctx)
{
    return mp_property_vo_flag(prop, action, arg, VOCTRL_ROOTWIN,
			       &vo_rootwin, mpctx);
}

/// Show window borders (RW)
static int mp_property_border(m_option_t * prop, int action, void *arg,
			      MPContext * mpctx)
{
    return mp_property_vo_flag(prop, action, arg, VOCTRL_BORDER,
			       &vo_border, mpctx);
}

/// Framedropping state (RW)
static int mp_property_framedropping(m_option_t * prop, int action,
				     void *arg, MPContext * mpctx)
{

    if (!mpctx->sh_video)
	return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_PRINT:
	if (!arg)
	    return M_PROPERTY_ERROR;
	*(char **) arg = strdup(frame_dropping == 1 ? MSGTR_Enabled :
				(frame_dropping == 2 ? MSGTR_HardFrameDrop :
				 MSGTR_Disabled));
	return M_PROPERTY_OK;
    default:
	return m_property_choice(prop, action, arg, &frame_dropping);
    }
}

/// Color settings, try to use vf/vo then fall back on TV. (RW)
static int mp_property_gamma(m_option_t * prop, int action, void *arg,
			     MPContext * mpctx)
{
    int *gamma = prop->priv, r, val;

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
static int mp_property_vsync(m_option_t * prop, int action, void *arg,
			     MPContext * mpctx)
{
    return m_property_flag(prop, action, arg, &vo_vsync);
}

/// Video codec tag (RO)
static int mp_property_video_format(m_option_t * prop, int action,
				    void *arg, MPContext * mpctx)
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
static int mp_property_video_codec(m_option_t * prop, int action,
                                   void *arg, MPContext * mpctx)
{
    if (!mpctx->sh_video || !mpctx->sh_video->codec)
	return M_PROPERTY_UNAVAILABLE;
    return m_property_string_ro(prop, action, arg, mpctx->sh_video->codec->name);
}


/// Video bitrate (RO)
static int mp_property_video_bitrate(m_option_t * prop, int action,
				     void *arg, MPContext * mpctx)
{
    if (!mpctx->sh_video)
	return M_PROPERTY_UNAVAILABLE;
    return m_property_bitrate(prop, action, arg, mpctx->sh_video->i_bps);
}

/// Video display width (RO)
static int mp_property_width(m_option_t * prop, int action, void *arg,
			     MPContext * mpctx)
{
    if (!mpctx->sh_video)
	return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(prop, action, arg, mpctx->sh_video->disp_w);
}

/// Video display height (RO)
static int mp_property_height(m_option_t * prop, int action, void *arg,
			      MPContext * mpctx)
{
    if (!mpctx->sh_video)
	return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(prop, action, arg, mpctx->sh_video->disp_h);
}

/// Video fps (RO)
static int mp_property_fps(m_option_t * prop, int action, void *arg,
			   MPContext * mpctx)
{
    if (!mpctx->sh_video)
	return M_PROPERTY_UNAVAILABLE;
    return m_property_float_ro(prop, action, arg, mpctx->sh_video->fps);
}

/// Video aspect (RO)
static int mp_property_aspect(m_option_t * prop, int action, void *arg,
			      MPContext * mpctx)
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
static int mp_property_sub_pos(m_option_t * prop, int action, void *arg,
			       MPContext * mpctx)
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
    default:
	return m_property_int_range(prop, action, arg, &sub_pos);
    }
}

/// Selected subtitles (RW)
static int mp_property_sub(m_option_t * prop, int action, void *arg,
			   MPContext * mpctx)
{
    demux_stream_t *const d_sub = mpctx->d_sub;
    const int global_sub_size = mpctx->global_sub_size;
    int source = -1, reset_spu = 0;
    char *sub_name;

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
	if (subdata)
	    sub_name = subdata->filename;
#ifdef CONFIG_ASS
	if (ass_track && ass_track->name)
	    sub_name = ass_track->name;
#endif
	if (sub_name) {
	    char *tmp, *tmp2;
	    tmp = sub_name;
	    if ((tmp2 = strrchr(tmp, '/')))
		tmp = tmp2 + 1;

	    snprintf(*(char **) arg, 63, "(%d) %s%s",
		     mpctx->set_of_sub_pos + 1,
		     strlen(tmp) < 20 ? "" : "...",
		     strlen(tmp) < 20 ? tmp : tmp + strlen(tmp) - 19);
	    return M_PROPERTY_OK;
	}
#ifdef CONFIG_DVDNAV
	if (mpctx->stream->type == STREAMTYPE_DVDNAV) {
	    if (vo_spudec && dvdsub_id >= 0) {
		unsigned char lang[3];
		if (mp_dvdnav_lang_from_sid(mpctx->stream, dvdsub_id, lang)) {
		    snprintf(*(char **) arg, 63, "(%d) %s", dvdsub_id, lang);
		    return M_PROPERTY_OK;
		}
	    }
	}
#endif

	if ((mpctx->demuxer->type == DEMUXER_TYPE_MATROSKA
             || mpctx->demuxer->type == DEMUXER_TYPE_LAVF
             || mpctx->demuxer->type == DEMUXER_TYPE_LAVF_PREFERRED
             || mpctx->demuxer->type == DEMUXER_TYPE_OGG)
             && d_sub && d_sub->sh && dvdsub_id >= 0) {
            const char* lang = ((sh_sub_t*)d_sub->sh)->lang;
            if (!lang) lang = MSGTR_Unknown;
	    snprintf(*(char **) arg, 63, "(%d) %s", dvdsub_id, lang);
	    return M_PROPERTY_OK;
	}

	if (vo_vobsub && vobsub_id >= 0) {
	    const char *language = MSGTR_Unknown;
	    language = vobsub_get_id(vo_vobsub, (unsigned int) vobsub_id);
	    snprintf(*(char **) arg, 63, "(%d) %s",
		     vobsub_id, language ? language : MSGTR_Unknown);
	    return M_PROPERTY_OK;
	}
#ifdef CONFIG_DVDREAD
	if (vo_spudec && mpctx->stream->type == STREAMTYPE_DVD
	    && dvdsub_id >= 0) {
	    char lang[3];
	    int code = dvd_lang_from_sid(mpctx->stream, dvdsub_id);
	    lang[0] = code >> 8;
	    lang[1] = code;
	    lang[2] = 0;
	    snprintf(*(char **) arg, 63, "(%d) %s", dvdsub_id, lang);
	    return M_PROPERTY_OK;
	}
#endif
	if (dvdsub_id >= 0) {
	    snprintf(*(char **) arg, 63, "(%d) %s", dvdsub_id, MSGTR_Unknown);
	    return M_PROPERTY_OK;
	}
	snprintf(*(char **) arg, 63, MSGTR_Disabled);
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

    if (mpctx->global_sub_pos >= 0)
	source = sub_source(mpctx);

    mp_msg(MSGT_CPLAYER, MSGL_DBG3,
	   "subtitles: %d subs, (v@%d s@%d d@%d), @%d, source @%d\n",
	   global_sub_size,
	   mpctx->global_sub_indices[SUB_SOURCE_VOBSUB],
	   mpctx->global_sub_indices[SUB_SOURCE_SUBS],
	   mpctx->global_sub_indices[SUB_SOURCE_DEMUX],
	   mpctx->global_sub_pos, source);

    mpctx->set_of_sub_pos = -1;
    subdata = NULL;

    vobsub_id = -1;
    dvdsub_id = -1;
    if (d_sub) {
	if (d_sub->id > -2)
	    reset_spu = 1;
	d_sub->id = -2;
    }
#ifdef CONFIG_ASS
    ass_track = 0;
#endif

    if (source == SUB_SOURCE_VOBSUB) {
        vobsub_id = vobsub_get_id_by_index(vo_vobsub, mpctx->global_sub_pos - mpctx->global_sub_indices[SUB_SOURCE_VOBSUB]);
    } else if (source == SUB_SOURCE_SUBS) {
	mpctx->set_of_sub_pos =
	    mpctx->global_sub_pos - mpctx->global_sub_indices[SUB_SOURCE_SUBS];
#ifdef CONFIG_ASS
	if (ass_enabled && mpctx->set_of_ass_tracks[mpctx->set_of_sub_pos])
	    ass_track = mpctx->set_of_ass_tracks[mpctx->set_of_sub_pos];
	else
#endif
	{
	    subdata = mpctx->set_of_subtitles[mpctx->set_of_sub_pos];
	    vo_osd_changed(OSDTYPE_SUBTITLE);
	}
    } else if (source == SUB_SOURCE_DEMUX) {
	dvdsub_id =
	    mpctx->global_sub_pos - mpctx->global_sub_indices[SUB_SOURCE_DEMUX];
	if (d_sub && dvdsub_id < MAX_S_STREAMS) {
	    int i = 0;
	    // default: assume 1:1 mapping of sid and stream id
	    d_sub->id = dvdsub_id;
	    d_sub->sh = mpctx->demuxer->s_streams[d_sub->id];
	    ds_free_packs(d_sub);
	    for (i = 0; i < MAX_S_STREAMS; i++) {
		sh_sub_t *sh = mpctx->demuxer->s_streams[i];
		if (sh && sh->sid == dvdsub_id) {
		    d_sub->id = i;
		    d_sub->sh = sh;
		    break;
		}
	    }
	    if (d_sub->sh && d_sub->id >= 0) {
		sh_sub_t *sh = d_sub->sh;
		if (sh->type == 'v')
		    init_vo_spudec();
#ifdef CONFIG_ASS
		else if (ass_enabled)
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
	&& dvdsub_id < 0 && reset_spu) {
	dvdsub_id = -2;
	d_sub->id = dvdsub_id;
    }
#endif
    update_subtitles(mpctx->sh_video, d_sub, 1);

    return M_PROPERTY_OK;
}

/// Selected sub source (RW)
static int mp_property_sub_source(m_option_t * prop, int action, void *arg,
                                  MPContext * mpctx)
{
    int source;
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
            snprintf(*(char **) arg, 63, MSGTR_SubSourceFile);
            break;
        case SUB_SOURCE_VOBSUB:
            snprintf(*(char **) arg, 63, MSGTR_SubSourceVobsub);
            break;
        case SUB_SOURCE_DEMUX:
            snprintf(*(char **) arg, 63, MSGTR_SubSourceDemux);
            break;
        default:
            snprintf(*(char **) arg, 63, MSGTR_Disabled);
        }
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(int*)arg);
        if (*(int *) arg < 0)
            mpctx->global_sub_pos = -1;
        else if (*(int *) arg != sub_source(mpctx)) {
            if (*(int *) arg != sub_source_by_pos(mpctx, mpctx->global_sub_indices[*(int *) arg]))
                return M_PROPERTY_UNAVAILABLE;
            mpctx->global_sub_pos = mpctx->global_sub_indices[*(int *) arg];
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
                    source == sub_source_by_pos(mpctx, mpctx->global_sub_indices[source]))
                step_all -= step;
        }
        if (source == cur_source)
            return M_PROPERTY_OK;
        if (source == -1)
            mpctx->global_sub_pos = -1;
        else
            mpctx->global_sub_pos = mpctx->global_sub_indices[source];
        break;
    }
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }
    --mpctx->global_sub_pos;
    return mp_property_sub(prop, M_PROPERTY_STEP_UP, NULL, mpctx);
}

/// Selected subtitles from specific source (RW)
static int mp_property_sub_by_type(m_option_t * prop, int action, void *arg,
                                   MPContext * mpctx)
{
    int source, is_cur_source, offset;
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

    offset = mpctx->global_sub_indices[source];
    if (offset < 0 || source != sub_source_by_pos(mpctx, offset))
        return M_PROPERTY_UNAVAILABLE;

    is_cur_source = sub_source(mpctx) == source;
    switch (action) {
    case M_PROPERTY_GET:
        if (!arg)
            return M_PROPERTY_ERROR;
        if (is_cur_source) {
            *(int *) arg = mpctx->global_sub_pos - offset;
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
        snprintf(*(char **) arg, 63, MSGTR_Disabled);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        if (*(int *) arg >= 0) {
            int index = *(int *)arg;
            if (source == SUB_SOURCE_VOBSUB)
                index = vobsub_get_index_by_id(vo_vobsub, index);
            mpctx->global_sub_pos = offset + index;
            if (index < 0 || mpctx->global_sub_pos >= mpctx->global_sub_size
                    || sub_source(mpctx) != source) {
                mpctx->global_sub_pos = -1;
                *(int *) arg = -1;
            }
        }
        else
            mpctx->global_sub_pos = -1;
        break;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN: {
        int step_all = (arg && *(int*)arg != 0 ? *(int*)arg : 1)
                       * (action == M_PROPERTY_STEP_UP ? 1 : -1);
        int step = (step_all > 0) ? 1 : -1;
        int max_sub_pos_for_source = -1;
        if (!is_cur_source)
            mpctx->global_sub_pos = -1;
        while (step_all) {
            if (mpctx->global_sub_pos == -1) {
                if (step > 0)
                    mpctx->global_sub_pos = offset;
                else if (max_sub_pos_for_source == -1) {
                    // Find max pos for specific source
                    mpctx->global_sub_pos = mpctx->global_sub_size - 1;
                    while (mpctx->global_sub_pos >= 0
                            && sub_source(mpctx) != source)
                        --mpctx->global_sub_pos;
                }
                else
                    mpctx->global_sub_pos = max_sub_pos_for_source;
            }
            else {
                mpctx->global_sub_pos += step;
                if (mpctx->global_sub_pos < offset ||
                        mpctx->global_sub_pos >= mpctx->global_sub_size ||
                        sub_source(mpctx) != source)
                    mpctx->global_sub_pos = -1;
            }
            step_all -= step;
        }
        break;
    }
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }
    --mpctx->global_sub_pos;
    return mp_property_sub(prop, M_PROPERTY_STEP_UP, NULL, mpctx);
}

/// Subtitle delay (RW)
static int mp_property_sub_delay(m_option_t * prop, int action, void *arg,
				 MPContext * mpctx)
{
    if (!mpctx->sh_video)
	return M_PROPERTY_UNAVAILABLE;
    return m_property_delay(prop, action, arg, &sub_delay);
}

/// Alignment of text subtitles (RW)
static int mp_property_sub_alignment(m_option_t * prop, int action,
				     void *arg, MPContext * mpctx)
{
    char *name[] = { MSGTR_Top, MSGTR_Center, MSGTR_Bottom };

    if (!mpctx->sh_video || mpctx->global_sub_pos < 0
	|| sub_source(mpctx) != SUB_SOURCE_SUBS)
	return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_PRINT:
	if (!arg)
	    return M_PROPERTY_ERROR;
	M_PROPERTY_CLAMP(prop, sub_alignment);
	*(char **) arg = strdup(name[sub_alignment]);
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
static int mp_property_sub_visibility(m_option_t * prop, int action,
				      void *arg, MPContext * mpctx)
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
static int mp_property_ass_use_margins(m_option_t * prop, int action,
				      void *arg, MPContext * mpctx)
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
static int mp_property_sub_forced_only(m_option_t * prop, int action,
				       void *arg, MPContext * mpctx)
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
static int mp_property_sub_scale(m_option_t * prop, int action, void *arg,
			      MPContext * mpctx)
{

    switch (action) {
        case M_PROPERTY_SET:
            if (!arg)
                return M_PROPERTY_ERROR;
            M_PROPERTY_CLAMP(prop, *(float *) arg);
#ifdef CONFIG_ASS
            if (ass_enabled) {
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
            if (ass_enabled) {
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
            if (ass_enabled)
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
static int mp_property_tv_color(m_option_t * prop, int action, void *arg,
				MPContext * mpctx)
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
	return tv_set_color_options(tvh, (int) prop->priv, *(int *) arg);
    case M_PROPERTY_GET:
	return tv_get_color_options(tvh, (int) prop->priv, arg);
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
	if ((r = tv_get_color_options(tvh, (int) prop->priv, &val)) >= 0) {
	    if (!r)
		return M_PROPERTY_ERROR;
	    val += (arg ? *(int *) arg : 1) *
		(action == M_PROPERTY_STEP_DOWN ? -1 : 1);
	    M_PROPERTY_CLAMP(prop, val);
	    return tv_set_color_options(tvh, (int) prop->priv, val);
	}
	return M_PROPERTY_ERROR;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

#endif

#ifdef CONFIG_TV_TELETEXT
static int mp_property_teletext_common(m_option_t * prop, int action, void *arg,
                  MPContext * mpctx)
{
    int val,result;
    int base_ioctl=(int)prop->priv;
    /* 
      for teletext's GET,SET,STEP ioctls this is not 0
      SET is GET+1
      STEP is GET+2
    */
    tvi_handle_t *tvh = mpctx->demuxer->priv;
    if (mpctx->demuxer->type != DEMUXER_TYPE_TV || !tvh)
        return M_PROPERTY_UNAVAILABLE;
    if(!base_ioctl)
        return M_PROPERTY_ERROR;

    switch (action) {
    case M_PROPERTY_GET:
        if (!arg)
            return M_PROPERTY_ERROR;
        result=tvh->functions->control(tvh->priv, base_ioctl, arg);
        break;
    case M_PROPERTY_SET:
        if (!arg)
            return M_PROPERTY_ERROR;
        M_PROPERTY_CLAMP(prop, *(int *) arg);
        result=tvh->functions->control(tvh->priv, base_ioctl+1, arg);
        break;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        result=tvh->functions->control(tvh->priv, base_ioctl, &val);
        val += (arg ? *(int *) arg : 1) * (action == M_PROPERTY_STEP_DOWN ? -1 : 1);
        result=tvh->functions->control(tvh->priv, base_ioctl+1, &val);
        break;
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }

    return result == TVI_CONTROL_TRUE ? M_PROPERTY_OK : M_PROPERTY_ERROR;
}

static int mp_property_teletext_mode(m_option_t * prop, int action, void *arg,
                  MPContext * mpctx)
{
    tvi_handle_t *tvh = mpctx->demuxer->priv;
    int result;
    int val;

    //with tvh==NULL will fail too
    result=mp_property_teletext_common(prop,action,arg,mpctx);
    if(result!=M_PROPERTY_OK)
        return result;

    if(tvh->functions->control(tvh->priv, prop->priv, &val)==TVI_CONTROL_TRUE && val)
        mp_input_set_section("teletext");
    else
        mp_input_set_section("tv");
    return M_PROPERTY_OK;
}

static int mp_property_teletext_page(m_option_t * prop, int action, void *arg,
                  MPContext * mpctx)
{
    tvi_handle_t *tvh = mpctx->demuxer->priv;
    int result;
    int val;
    if (mpctx->demuxer->type != DEMUXER_TYPE_TV || !tvh)
        return M_PROPERTY_UNAVAILABLE;
    switch(action){
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        //This should be handled separately
        val = (arg ? *(int *) arg : 1) * (action == M_PROPERTY_STEP_DOWN ? -1 : 1);
        result=tvh->functions->control(tvh->priv, TV_VBI_CONTROL_STEP_PAGE, &val);
        break;
    default:
        result=mp_property_teletext_common(prop,action,arg,mpctx);
    }
    return result;
}


#endif /* CONFIG_TV_TELETEXT */

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
    { "length", mp_property_length, CONF_TYPE_TIME,
     M_OPT_MIN, 0, 0, NULL },
    { "percent_pos", mp_property_percent_pos, CONF_TYPE_INT,
     M_OPT_RANGE, 0, 100, NULL },
    { "time_pos", mp_property_time_pos, CONF_TYPE_TIME,
     M_OPT_MIN, 0, 0, NULL },
    { "chapter", mp_property_chapter, CONF_TYPE_INT,
     M_OPT_MIN, 1, 0, NULL },
    { "chapters", mp_property_chapters, CONF_TYPE_INT,
     0, 0, 0, NULL },
    { "angle", mp_property_angle, CONF_TYPE_INT,
     CONF_RANGE, -2, 10, NULL },
    { "metadata", mp_property_metadata, CONF_TYPE_STRING_LIST,
     0, 0, 0, NULL },
    { "pause", mp_property_pause, CONF_TYPE_FLAG,
     M_OPT_RANGE, 0, 1, NULL },

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
     CONF_RANGE, -2, MAX_A_STREAMS - 1, NULL },
    { "balance", mp_property_balance, CONF_TYPE_FLOAT,
     M_OPT_RANGE, -1, 1, NULL },

    // Video
    { "fullscreen", mp_property_fullscreen, CONF_TYPE_FLAG,
     M_OPT_RANGE, 0, 1, NULL },
    { "deinterlace", mp_property_deinterlace, CONF_TYPE_FLAG,
     M_OPT_RANGE, 0, 1, NULL },
    { "ontop", mp_property_ontop, CONF_TYPE_FLAG,
     M_OPT_RANGE, 0, 1, NULL },
    { "rootwin", mp_property_rootwin, CONF_TYPE_FLAG,
     M_OPT_RANGE, 0, 1, NULL },
    { "border", mp_property_border, CONF_TYPE_FLAG,
     M_OPT_RANGE, 0, 1, NULL },
    { "framedropping", mp_property_framedropping, CONF_TYPE_INT,
     M_OPT_RANGE, 0, 2, NULL },
    { "gamma", mp_property_gamma, CONF_TYPE_INT,
     M_OPT_RANGE, -100, 100, &vo_gamma_gamma },
    { "brightness", mp_property_gamma, CONF_TYPE_INT,
     M_OPT_RANGE, -100, 100, &vo_gamma_brightness },
    { "contrast", mp_property_gamma, CONF_TYPE_INT,
     M_OPT_RANGE, -100, 100, &vo_gamma_contrast },
    { "saturation", mp_property_gamma, CONF_TYPE_INT,
     M_OPT_RANGE, -100, 100, &vo_gamma_saturation },
    { "hue", mp_property_gamma, CONF_TYPE_INT,
     M_OPT_RANGE, -100, 100, &vo_gamma_hue },
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
     CONF_RANGE, -2, MAX_V_STREAMS - 1, NULL },
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
     M_OPT_RANGE, -100, 100, (void *) TV_COLOR_BRIGHTNESS },
    { "tv_contrast", mp_property_tv_color, CONF_TYPE_INT,
     M_OPT_RANGE, -100, 100, (void *) TV_COLOR_CONTRAST },
    { "tv_saturation", mp_property_tv_color, CONF_TYPE_INT,
     M_OPT_RANGE, -100, 100, (void *) TV_COLOR_SATURATION },
    { "tv_hue", mp_property_tv_color, CONF_TYPE_INT,
     M_OPT_RANGE, -100, 100, (void *) TV_COLOR_HUE },
#endif

#ifdef CONFIG_TV_TELETEXT
    { "teletext_page", mp_property_teletext_page, CONF_TYPE_INT,
     M_OPT_RANGE, 100, 899,  (void*)TV_VBI_CONTROL_GET_PAGE },
    { "teletext_subpage", mp_property_teletext_common, CONF_TYPE_INT,
     M_OPT_RANGE, 0, 64, (void*)TV_VBI_CONTROL_GET_SUBPAGE },
    { "teletext_mode", mp_property_teletext_mode, CONF_TYPE_FLAG,
     M_OPT_RANGE, 0, 1, (void*)TV_VBI_CONTROL_GET_MODE },
    { "teletext_format", mp_property_teletext_common, CONF_TYPE_INT,
     M_OPT_RANGE, 0, 3, (void*)TV_VBI_CONTROL_GET_FORMAT },
    { "teletext_half_page", mp_property_teletext_common, CONF_TYPE_INT,
     M_OPT_RANGE, 0, 2, (void*)TV_VBI_CONTROL_GET_HALF_PAGE },
#endif

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

char *property_expand_string(MPContext * mpctx, char *str)
{
    return m_properties_expand_string(mp_properties, str, mpctx);
}

void property_print_help(void)
{
    m_properties_print_help_list(mp_properties);
}


///@}
// Properties group


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
 * property to its next value. Otherwise it sets it to the given
 * value.
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
    /// progressbar type
    int osd_progbar;
    /// osd msg id if it must be shared
    int osd_id;
    /// osd msg template
    const char *osd_msg;
} set_prop_cmd[] = {
    // general
    { "loop", MP_CMD_LOOP, 0, 0, -1, MSGTR_LoopStatus },
    { "chapter", MP_CMD_SEEK_CHAPTER, 0, 0, -1, NULL },
    { "angle", MP_CMD_SWITCH_ANGLE, 0, 0, -1, NULL },
    // audio
    { "volume", MP_CMD_VOLUME, 0, OSD_VOLUME, -1, MSGTR_Volume },
    { "mute", MP_CMD_MUTE, 1, 0, -1, MSGTR_MuteStatus },
    { "audio_delay", MP_CMD_AUDIO_DELAY, 0, 0, -1, MSGTR_AVDelayStatus },
    { "switch_audio", MP_CMD_SWITCH_AUDIO, 1, 0, -1, MSGTR_OSDAudio },
    { "balance", MP_CMD_BALANCE, 0, OSD_BALANCE, -1, MSGTR_Balance },
    // video
    { "fullscreen", MP_CMD_VO_FULLSCREEN, 1, 0, -1, NULL },
    { "panscan", MP_CMD_PANSCAN, 0, OSD_PANSCAN, -1, MSGTR_Panscan },
    { "ontop", MP_CMD_VO_ONTOP, 1, 0, -1, MSGTR_OnTopStatus },
    { "rootwin", MP_CMD_VO_ROOTWIN, 1, 0, -1, MSGTR_RootwinStatus },
    { "border", MP_CMD_VO_BORDER, 1, 0, -1, MSGTR_BorderStatus },
    { "framedropping", MP_CMD_FRAMEDROPPING, 1, 0, -1, MSGTR_FramedroppingStatus },
    { "gamma", MP_CMD_GAMMA, 0, OSD_BRIGHTNESS, -1, MSGTR_Gamma },
    { "brightness", MP_CMD_BRIGHTNESS, 0, OSD_BRIGHTNESS, -1, MSGTR_Brightness },
    { "contrast", MP_CMD_CONTRAST, 0, OSD_CONTRAST, -1, MSGTR_Contrast },
    { "saturation", MP_CMD_SATURATION, 0, OSD_SATURATION, -1, MSGTR_Saturation },
    { "hue", MP_CMD_HUE, 0, OSD_HUE, -1, MSGTR_Hue },
    { "vsync", MP_CMD_SWITCH_VSYNC, 1, 0, -1, MSGTR_VSyncStatus },
	// subs
    { "sub", MP_CMD_SUB_SELECT, 1, 0, -1, MSGTR_SubSelectStatus },
    { "sub_source", MP_CMD_SUB_SOURCE, 1, 0, -1, MSGTR_SubSourceStatus },
    { "sub_vob", MP_CMD_SUB_VOB, 1, 0, -1, MSGTR_SubSelectStatus },
    { "sub_demux", MP_CMD_SUB_DEMUX, 1, 0, -1, MSGTR_SubSelectStatus },
    { "sub_file", MP_CMD_SUB_FILE, 1, 0, -1, MSGTR_SubSelectStatus },
    { "sub_pos", MP_CMD_SUB_POS, 0, 0, -1, MSGTR_SubPosStatus },
    { "sub_alignment", MP_CMD_SUB_ALIGNMENT, 1, 0, -1, MSGTR_SubAlignStatus },
    { "sub_delay", MP_CMD_SUB_DELAY, 0, 0, OSD_MSG_SUB_DELAY, MSGTR_SubDelayStatus },
    { "sub_visibility", MP_CMD_SUB_VISIBILITY, 1, 0, -1, MSGTR_SubVisibleStatus },
    { "sub_forced_only", MP_CMD_SUB_FORCED_ONLY, 1, 0, -1, MSGTR_SubForcedOnlyStatus },
#ifdef CONFIG_FREETYPE
    { "sub_scale", MP_CMD_SUB_SCALE, 0, 0, -1, MSGTR_SubScale},
#endif
#ifdef CONFIG_ASS
    { "ass_use_margins", MP_CMD_ASS_USE_MARGINS, 1, 0, -1, NULL },
#endif
#ifdef CONFIG_TV
    { "tv_brightness", MP_CMD_TV_SET_BRIGHTNESS, 0, OSD_BRIGHTNESS, -1, MSGTR_Brightness },
    { "tv_hue", MP_CMD_TV_SET_HUE, 0, OSD_HUE, -1, MSGTR_Hue },
    { "tv_saturation", MP_CMD_TV_SET_SATURATION, 0, OSD_SATURATION, -1, MSGTR_Saturation },
    { "tv_contrast", MP_CMD_TV_SET_CONTRAST, 0, OSD_CONTRAST, -1, MSGTR_Contrast },
#endif
    { NULL, 0, 0, 0, -1, NULL }
};


/// Handle commands that set a property.
static int set_property_command(MPContext * mpctx, mp_cmd_t * cmd)
{
    int i, r;
    m_option_t* prop;
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
	else
	    r = mp_property_do(pname, M_PROPERTY_STEP_UP, NULL, mpctx);
    } else if (cmd->args[1].v.i)	//set
	r = mp_property_do(pname, M_PROPERTY_SET, &cmd->args[0].v, mpctx);
    else			// adjust
	r = mp_property_do(pname, M_PROPERTY_STEP_UP, &cmd->args[0].v, mpctx);

    if (r <= 0)
	return 1;

    if (set_prop_cmd[i].osd_progbar) {
	if (prop->type == CONF_TYPE_INT) {
	    if (mp_property_do(pname, M_PROPERTY_GET, &r, mpctx) > 0)
		set_osd_bar(set_prop_cmd[i].osd_progbar,
			    set_prop_cmd[i].osd_msg, prop->min, prop->max, r);
	} else if (prop->type == CONF_TYPE_FLOAT) {
	    float f;
	    if (mp_property_do(pname, M_PROPERTY_GET, &f, mpctx) > 0)
		set_osd_bar(set_prop_cmd[i].osd_progbar,
			    set_prop_cmd[i].osd_msg, prop->min, prop->max, f);
	} else
	    mp_msg(MSGT_CPLAYER, MSGL_ERR,
		   "Property use an unsupported type.\n");
	return 1;
    }

    if (set_prop_cmd[i].osd_msg) {
	char *val = mp_property_print(pname, mpctx);
	if (val) {
	    set_osd_msg(set_prop_cmd[i].osd_id >=
			0 ? set_prop_cmd[i].osd_id : OSD_MSG_PROPERTY + i,
			1, osd_duration, set_prop_cmd[i].osd_msg, val);
	    free(val);
	}
    }
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

int run_command(MPContext * mpctx, mp_cmd_t * cmd)
{
    sh_audio_t * const sh_audio = mpctx->sh_audio;
    sh_video_t * const sh_video = mpctx->sh_video;
    int brk_cmd = 0;
    if (!set_property_command(mpctx, cmd))
	switch (cmd->id) {
	case MP_CMD_SEEK:{
		float v;
		int abs;
		if (sh_video)
		    mpctx->osd_show_percentage = sh_video->fps;
		v = cmd->args[0].v.f;
		abs = (cmd->nargs > 1) ? cmd->args[1].v.i : 0;
		if (abs == 2) {	/* Absolute seek to a specific timestamp in seconds */
		    abs_seek_pos = SEEK_ABSOLUTE;
		    if (sh_video)
			mpctx->osd_function =
			    (v > sh_video->pts) ? OSD_FFW : OSD_REW;
		    rel_seek_secs = v;
		} else if (abs) {	/* Absolute seek by percentage */
		    abs_seek_pos = SEEK_ABSOLUTE | SEEK_FACTOR;
		    if (sh_video)
			mpctx->osd_function = OSD_FFW;	// Direction isn't set correctly
		    rel_seek_secs = v / 100.0;
		} else {
		    rel_seek_secs += v;
		    mpctx->osd_function = (v > 0) ? OSD_FFW : OSD_REW;
		}
		brk_cmd = 1;
	    }
	    break;

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
	    }
	    break;

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
	    }
	    break;

	case MP_CMD_GET_PROPERTY:{
		char *tmp;
		if (mp_property_do(cmd->args[0].v.s, M_PROPERTY_TO_STRING,
				   &tmp, mpctx) <= 0) {
		    mp_msg(MSGT_CPLAYER, MSGL_WARN,
			   "Failed to get value of property '%s'.\n",
			   cmd->args[0].v.s);
		    break;
		}
		mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_%s=%s\n",
		       cmd->args[0].v.s, tmp);
		free(tmp);
	    }
	    break;

	case MP_CMD_EDL_MARK:
	    if (edl_fd) {
		float v = sh_video ? sh_video->pts :
		    playing_audio_pts(sh_audio, mpctx->d_audio,
				      mpctx->audio_out);

		if (mpctx->begin_skip == MP_NOPTS_VALUE) {
		    mpctx->begin_skip = v;
		    mp_msg(MSGT_CPLAYER, MSGL_INFO, MSGTR_EdloutStartSkip);
		} else {
		    if (mpctx->begin_skip > v)
			mp_msg(MSGT_CPLAYER, MSGL_WARN, MSGTR_EdloutBadStop);
		    else {
			fprintf(edl_fd, "%f %f %d\n", mpctx->begin_skip, v, 0);
			mp_msg(MSGT_CPLAYER, MSGL_INFO, MSGTR_EdloutEndSkip);
		    }
		    mpctx->begin_skip = MP_NOPTS_VALUE;
		}
	    }
	    break;

	case MP_CMD_SWITCH_RATIO:
            if (!sh_video)
                break;
	    if (cmd->nargs == 0 || cmd->args[0].v.f == -1)
		movie_aspect = (float) sh_video->disp_w / sh_video->disp_h;
	    else
		movie_aspect = cmd->args[0].v.f;
	    mpcodecs_config_vo(sh_video, sh_video->disp_w, sh_video->disp_h, 0);
	    break;

	case MP_CMD_SPEED_INCR:{
		float v = cmd->args[0].v.f;
		playback_speed += v;
		build_afilter_chain(sh_audio, &ao_data);
		set_osd_msg(OSD_MSG_SPEED, 1, osd_duration, MSGTR_OSDSpeed,
			    playback_speed);
	    } break;

	case MP_CMD_SPEED_MULT:{
		float v = cmd->args[0].v.f;
		playback_speed *= v;
		build_afilter_chain(sh_audio, &ao_data);
		set_osd_msg(OSD_MSG_SPEED, 1, osd_duration, MSGTR_OSDSpeed,
			    playback_speed);
	    } break;

	case MP_CMD_SPEED_SET:{
		float v = cmd->args[0].v.f;
		playback_speed = v;
		build_afilter_chain(sh_audio, &ao_data);
		set_osd_msg(OSD_MSG_SPEED, 1, osd_duration, MSGTR_OSDSpeed,
			    playback_speed);
	    } break;

	case MP_CMD_FRAME_STEP:
	case MP_CMD_PAUSE:
	    cmd->pausing = 1;
	    brk_cmd = 1;
	    break;

	case MP_CMD_FILE_FILTER:
	    file_filter = cmd->args[0].v.i;
	    break;

	case MP_CMD_QUIT:
	    exit_player_with_rc(EXIT_QUIT,
				(cmd->nargs > 0) ? cmd->args[0].v.i : 0);

	case MP_CMD_PLAY_TREE_STEP:{
		int n = cmd->args[0].v.i == 0 ? 1 : cmd->args[0].v.i;
		int force = cmd->args[1].v.i;

#ifdef CONFIG_GUI
		if (use_gui) {
		    int i = 0;
		    if (n > 0)
			for (i = 0; i < n; i++)
			    mplNext();
		    else
			for (i = 0; i < -1 * n; i++)
			    mplPrev();
		} else
#endif
		{
		    if (!force && mpctx->playtree_iter) {
			play_tree_iter_t *i =
			    play_tree_iter_new_copy(mpctx->playtree_iter);
			if (play_tree_iter_step(i, n, 0) ==
			    PLAY_TREE_ITER_ENTRY)
			    mpctx->eof =
				(n > 0) ? PT_NEXT_ENTRY : PT_PREV_ENTRY;
			play_tree_iter_free(i);
		    } else
			mpctx->eof = (n > 0) ? PT_NEXT_ENTRY : PT_PREV_ENTRY;
		    if (mpctx->eof)
			mpctx->play_tree_step = n;
		    brk_cmd = 1;
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
			mpctx->eof = (n > 0) ? PT_UP_NEXT : PT_UP_PREV;
		    play_tree_iter_free(i);
		} else
		    mpctx->eof = (n > 0) ? PT_UP_NEXT : PT_UP_PREV;
		brk_cmd = 1;
	    }
	    break;

	case MP_CMD_PLAY_ALT_SRC_STEP:
	    if (mpctx->playtree_iter && mpctx->playtree_iter->num_files > 1) {
		int v = cmd->args[0].v.i;
		if (v > 0
		    && mpctx->playtree_iter->file <
		    mpctx->playtree_iter->num_files)
		    mpctx->eof = PT_NEXT_SRC;
		else if (v < 0 && mpctx->playtree_iter->file > 1)
		    mpctx->eof = PT_PREV_SRC;
	    }
	    brk_cmd = 1;
	    break;

	case MP_CMD_SUB_STEP:
	    if (sh_video) {
		int movement = cmd->args[0].v.i;
		step_sub(subdata, sh_video->pts, movement);
#ifdef CONFIG_ASS
		if (ass_track)
		    sub_delay +=
			ass_step_sub(ass_track,
				     (sh_video->pts +
				      sub_delay) * 1000 + .5, movement) / 1000.;
#endif
		set_osd_msg(OSD_MSG_SUB_DELAY, 1, osd_duration,
			    MSGTR_OSDSubDelay, ROUND(sub_delay * 1000));
	    }
	    break;

	case MP_CMD_SUB_LOG:
	    log_sub();
	    break;

	case MP_CMD_OSD:{
		int v = cmd->args[0].v.i;
		int max = (term_osd
			   && !sh_video) ? MAX_TERM_OSD_LEVEL : MAX_OSD_LEVEL;
		if (osd_level > max)
		    osd_level = max;
		if (v < 0)
		    osd_level = (osd_level + 1) % (max + 1);
		else
		    osd_level = v > max ? max : v;
		/* Show OSD state when disabled, but not when an explicit
		   argument is given to the OSD command, i.e. in slave mode. */
		if (v == -1 && osd_level <= 1)
		    set_osd_msg(OSD_MSG_OSD_STATUS, 0, osd_duration,
				MSGTR_OSDosd,
				osd_level ? MSGTR_OSDenabled :
				MSGTR_OSDdisabled);
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

		if (cmd->args[1].v.i)	// append
		    play_tree_append_entry(mpctx->playtree->child, e);
		else {
		    // Go back to the starting point.
		    while (play_tree_iter_up_step
			   (mpctx->playtree_iter, 0, 1) != PLAY_TREE_ITER_END)
			/* NOP */ ;
		    play_tree_free_list(mpctx->playtree->child, 1);
		    play_tree_set_child(mpctx->playtree, e);
		    pt_iter_goto_head(mpctx->playtree_iter);
		    mpctx->eof = PT_NEXT_SRC;
		}
		brk_cmd = 1;
	    }
	    break;

	case MP_CMD_LOADLIST:{
		play_tree_t *e = parse_playlist_file(cmd->args[0].v.s);
		if (!e)
		    mp_msg(MSGT_CPLAYER, MSGL_ERR,
			   MSGTR_PlaylistLoadUnable, cmd->args[0].v.s);
		else {
		    if (cmd->args[1].v.i)	// append
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
			mpctx->eof = PT_NEXT_SRC;
		    }
		}
		brk_cmd = 1;
	    }
	    break;

	case MP_CMD_STOP:
	    // Go back to the starting point.
	    while (play_tree_iter_up_step
		   (mpctx->playtree_iter, 0, 1) != PLAY_TREE_ITER_END)
		/* NOP */ ;
	    mpctx->eof = PT_STOP;
	    brk_cmd = 1;
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
		    set_osd_msg(OSD_MSG_RADIO_CHANNEL, 1, osd_duration,
				MSGTR_OSDChannel,
				radio_get_channel_name(mpctx->demuxer->stream));
		}
	    }
	    break;

	case MP_CMD_RADIO_SET_CHANNEL:
	    if (mpctx->demuxer->stream->type == STREAMTYPE_RADIO) {
		radio_set_channel(mpctx->demuxer->stream, cmd->args[0].v.s);
		if (radio_get_channel_name(mpctx->demuxer->stream)) {
		    set_osd_msg(OSD_MSG_RADIO_CHANNEL, 1, osd_duration,
				MSGTR_OSDChannel,
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
			set_osd_msg(OSD_MSG_TV_CHANNEL, 1, osd_duration,
				    MSGTR_OSDChannel, tv_channel_current->name);
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


		    if (dvb_step_channel(mpctx->stream, dir))
			mpctx->eof = mpctx->dvbin_reopen = 1;
	    }
#endif /* CONFIG_DVBIN */
	    break;

	case MP_CMD_TV_SET_CHANNEL:
	    if (mpctx->file_format == DEMUXER_TYPE_TV) {
		tv_set_channel((tvi_handle_t *) (mpctx->demuxer->priv),
			       cmd->args[0].v.s);
		if (tv_channel_list) {
		    set_osd_msg(OSD_MSG_TV_CHANNEL, 1, osd_duration,
				MSGTR_OSDChannel, tv_channel_current->name);
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

		    if (dvb_set_channel
			(mpctx->stream, cmd->args[1].v.i, cmd->args[0].v.i))
			mpctx->eof = mpctx->dvbin_reopen = 1;
	    }
	    break;
#endif /* CONFIG_DVBIN */

	case MP_CMD_TV_LAST_CHANNEL:
	    if (mpctx->file_format == DEMUXER_TYPE_TV) {
		tv_last_channel((tvi_handle_t *) (mpctx->demuxer->priv));
		if (tv_channel_list) {
		    set_osd_msg(OSD_MSG_TV_CHANNEL, 1, osd_duration,
				MSGTR_OSDChannel, tv_channel_current->name);
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
#ifdef CONFIG_TV_TELETEXT
	case MP_CMD_TV_TELETEXT_ADD_DEC:
	{
	    tvi_handle_t* tvh=(tvi_handle_t *)(mpctx->demuxer->priv);
	    if (mpctx->file_format == DEMUXER_TYPE_TV)
		tvh->functions->control(tvh->priv,TV_VBI_CONTROL_ADD_DEC,&(cmd->args[0].v.s));
	    break;
	}
	case MP_CMD_TV_TELETEXT_GO_LINK:
	{
	    tvi_handle_t* tvh=(tvi_handle_t *)(mpctx->demuxer->priv);
	    if (mpctx->file_format == DEMUXER_TYPE_TV)
		tvh->functions->control(tvh->priv,TV_VBI_CONTROL_GO_LINK,&(cmd->args[0].v.i));
	    break;
	}
#endif /* CONFIG_TV_TELETEXT */
#endif /* CONFIG_TV */

	case MP_CMD_SUB_LOAD:
	    if (sh_video) {
		int n = mpctx->set_of_sub_size;
		add_subtitles(cmd->args[0].v.s, sh_video->fps, 0);
		if (n != mpctx->set_of_sub_size) {
		    if (mpctx->global_sub_indices[SUB_SOURCE_SUBS] < 0)
			mpctx->global_sub_indices[SUB_SOURCE_SUBS] =
			    mpctx->global_sub_size;
		    ++mpctx->global_sub_size;
		}
	    }
	    break;

	case MP_CMD_SUB_REMOVE:
	    if (sh_video) {
		int v = cmd->args[0].v.i;
		sub_data *subd;
		if (v < 0) {
		    for (v = 0; v < mpctx->set_of_sub_size; ++v) {
			subd = mpctx->set_of_subtitles[v];
			mp_msg(MSGT_CPLAYER, MSGL_STATUS,
			       MSGTR_RemovedSubtitleFile, v + 1,
			       filename_recode(subd->filename));
			sub_free(subd);
			mpctx->set_of_subtitles[v] = NULL;
		    }
		    mpctx->global_sub_indices[SUB_SOURCE_SUBS] = -1;
		    mpctx->global_sub_size -= mpctx->set_of_sub_size;
		    mpctx->set_of_sub_size = 0;
		    if (mpctx->set_of_sub_pos >= 0) {
			mpctx->global_sub_pos = -2;
			subdata = NULL;
			mp_input_queue_cmd(mp_input_parse_cmd("sub_select"));
		    }
		} else if (v < mpctx->set_of_sub_size) {
		    subd = mpctx->set_of_subtitles[v];
		    mp_msg(MSGT_CPLAYER, MSGL_STATUS,
			   MSGTR_RemovedSubtitleFile, v + 1,
			   filename_recode(subd->filename));
		    sub_free(subd);
		    if (mpctx->set_of_sub_pos == v) {
			mpctx->global_sub_pos = -2;
			subdata = NULL;
			mp_input_queue_cmd(mp_input_parse_cmd("sub_select"));
		    } else if (mpctx->set_of_sub_pos > v) {
			--mpctx->set_of_sub_pos;
			--mpctx->global_sub_pos;
		    }
		    while (++v < mpctx->set_of_sub_size)
			mpctx->set_of_subtitles[v - 1] =
			    mpctx->set_of_subtitles[v];
		    --mpctx->set_of_sub_size;
		    --mpctx->global_sub_size;
		    if (mpctx->set_of_sub_size <= 0)
			mpctx->global_sub_indices[SUB_SOURCE_SUBS] = -1;
		    mpctx->set_of_subtitles[mpctx->set_of_sub_size] = NULL;
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
	    if (vo_config_count) {
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
		mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_LENGTH=%.2lf\n",
		       demuxer_get_time_length(mpctx->demuxer));
	    }
	    break;

	case MP_CMD_GET_FILENAME:{
		mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_FILENAME='%s'\n",
		       get_metadata(META_NAME));
	    }
	    break;

	case MP_CMD_GET_VIDEO_CODEC:{
		char *inf = get_metadata(META_VIDEO_CODEC);
		if (!inf)
		    inf = strdup("");
		mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_VIDEO_CODEC='%s'\n", inf);
		free(inf);
	    }
	    break;

	case MP_CMD_GET_VIDEO_BITRATE:{
		char *inf = get_metadata(META_VIDEO_BITRATE);
		if (!inf)
		    inf = strdup("");
		mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_VIDEO_BITRATE='%s'\n", inf);
		free(inf);
	    }
	    break;

	case MP_CMD_GET_VIDEO_RESOLUTION:{
		char *inf = get_metadata(META_VIDEO_RESOLUTION);
		if (!inf)
		    inf = strdup("");
		mp_msg(MSGT_GLOBAL, MSGL_INFO,
		       "ANS_VIDEO_RESOLUTION='%s'\n", inf);
		free(inf);
	    }
	    break;

	case MP_CMD_GET_AUDIO_CODEC:{
		char *inf = get_metadata(META_AUDIO_CODEC);
		if (!inf)
		    inf = strdup("");
		mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_AUDIO_CODEC='%s'\n", inf);
		free(inf);
	    }
	    break;

	case MP_CMD_GET_AUDIO_BITRATE:{
		char *inf = get_metadata(META_AUDIO_BITRATE);
		if (!inf)
		    inf = strdup("");
		mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_AUDIO_BITRATE='%s'\n", inf);
		free(inf);
	    }
	    break;

	case MP_CMD_GET_AUDIO_SAMPLES:{
		char *inf = get_metadata(META_AUDIO_SAMPLES);
		if (!inf)
		    inf = strdup("");
		mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_AUDIO_SAMPLES='%s'\n", inf);
		free(inf);
	    }
	    break;

	case MP_CMD_GET_META_TITLE:{
		char *inf = get_metadata(META_INFO_TITLE);
		if (!inf)
		    inf = strdup("");
		mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_TITLE='%s'\n", inf);
		free(inf);
	    }
	    break;

	case MP_CMD_GET_META_ARTIST:{
		char *inf = get_metadata(META_INFO_ARTIST);
		if (!inf)
		    inf = strdup("");
		mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_ARTIST='%s'\n", inf);
		free(inf);
	    }
	    break;

	case MP_CMD_GET_META_ALBUM:{
		char *inf = get_metadata(META_INFO_ALBUM);
		if (!inf)
		    inf = strdup("");
		mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_ALBUM='%s'\n", inf);
		free(inf);
	    }
	    break;

	case MP_CMD_GET_META_YEAR:{
		char *inf = get_metadata(META_INFO_YEAR);
		if (!inf)
		    inf = strdup("");
		mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_YEAR='%s'\n", inf);
		free(inf);
	    }
	    break;

	case MP_CMD_GET_META_COMMENT:{
		char *inf = get_metadata(META_INFO_COMMENT);
		if (!inf)
		    inf = strdup("");
		mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_COMMENT='%s'\n", inf);
		free(inf);
	    }
	    break;

	case MP_CMD_GET_META_TRACK:{
		char *inf = get_metadata(META_INFO_TRACK);
		if (!inf)
		    inf = strdup("");
		mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_TRACK='%s'\n", inf);
		free(inf);
	    }
	    break;

	case MP_CMD_GET_META_GENRE:{
		char *inf = get_metadata(META_INFO_GENRE);
		if (!inf)
		    inf = strdup("");
		mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_META_GENRE='%s'\n", inf);
		free(inf);
	    }
	    break;

	case MP_CMD_GET_VO_FULLSCREEN:
	    if (mpctx->video_out && vo_config_count)
		mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_VO_FULLSCREEN=%d\n", vo_fs);
	    break;

	case MP_CMD_GET_PERCENT_POS:
	    mp_msg(MSGT_GLOBAL, MSGL_INFO, "ANS_PERCENT_POSITION=%d\n",
		   demuxer_get_percent_pos(mpctx->demuxer));
	    break;

	case MP_CMD_GET_TIME_POS:{
		float pos = 0;
		if (sh_video)
		    pos = sh_video->pts;
		else if (sh_audio && mpctx->audio_out)
		    pos =
			playing_audio_pts(sh_audio, mpctx->d_audio,
					  mpctx->audio_out);
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
	    mplayer_put_key(cmd->args[0].v.i);
	    break;

	case MP_CMD_SET_MOUSE_POS:{
		int pointer_x, pointer_y;
		double dx, dy;
		pointer_x = cmd->args[0].v.i;
		pointer_y = cmd->args[1].v.i;
		rescale_input_coordinates(pointer_x, pointer_y, &dx, &dy);
#ifdef CONFIG_DVDNAV
		if (mpctx->stream->type == STREAMTYPE_DVDNAV
		    && dx > 0.0 && dy > 0.0) {
		    int button = -1;
		    pointer_x = (int) (dx * (double) sh_video->disp_w);
		    pointer_y = (int) (dy * (double) sh_video->disp_h);
		    mp_dvdnav_update_mouse_pos(mpctx->stream,
					       pointer_x, pointer_y, &button);
		    if (osd_level > 1 && button > 0)
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
		if (osd_level > 1 && button > 0)
		    set_osd_msg(OSD_MSG_TEXT, 1, osd_duration,
				"Selected button number %d", button);
	    }
	    break;

	case MP_CMD_SWITCH_TITLE:
	    if (mpctx->stream->type == STREAMTYPE_DVDNAV)
	        mp_dvdnav_switch_title(mpctx->stream, cmd->args[0].v.i);
	    break;

#endif

	default:
#ifdef CONFIG_GUI
	    if ((use_gui) && (cmd->id > MP_CMD_GUI_EVENTS))
		guiGetEvent(guiIEvent, (char *) cmd->id);
	    else
#endif
		mp_msg(MSGT_CPLAYER, MSGL_V,
		       "Received unknown cmd %s\n", cmd->name);
	}

    switch (cmd->pausing) {
    case 1:	// "pausing"
	mpctx->osd_function = OSD_PAUSE;
	break;
    case 3:	// "pausing_toggle"
	mpctx->was_paused = !mpctx->was_paused;
	if (mpctx->was_paused)
	    mpctx->osd_function = OSD_PAUSE;
	else if (mpctx->osd_function == OSD_PAUSE)
	    mpctx->osd_function = OSD_PLAY;
	break;
    case 2:	// "pausing_keep"
	if (mpctx->was_paused)
	    mpctx->osd_function = OSD_PAUSE;
    }
    return brk_cmd;
}
