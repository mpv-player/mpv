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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>

#ifdef PTW32_STATIC_LIB
#include <pthread.h>
#endif

#include <libavutil/intreadwrite.h>
#include <libavutil/attributes.h>

#include <libavcodec/version.h>

#include "config.h"
#include "talloc.h"

#include "osdep/io.h"

#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <windows.h>
#endif
#define WAKEUP_PERIOD 0.5
#include <string.h>
#include <unistd.h>

// #include <sys/mman.h>
#include <sys/types.h>
#ifndef __MINGW32__
#include <sys/ioctl.h>
#include <sys/wait.h>
#endif

#include <sys/time.h>
#include <sys/stat.h>

#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <limits.h>

#include <errno.h>

#include "core/mp_msg.h"
#include "av_log.h"


#include "core/m_option.h"
#include "core/m_config.h"
#include "core/mplayer.h"
#include "core/m_property.h"

#include "sub/subreader.h"
#include "sub/find_subfiles.h"
#include "sub/dec_sub.h"

#include "core/mp_osd.h"
#include "video/out/vo.h"
#include "core/screenshot.h"

#include "sub/sub.h"
#include "core/cpudetect.h"

#ifdef CONFIG_X11
#include "video/out/x11_common.h"
#endif

#include "audio/out/ao.h"

#include "core/codecs.h"

#include "sub/spudec.h"

#include "osdep/getch2.h"
#include "osdep/timer.h"

#include "core/input/input.h"
#include "core/encode.h"

#include "osdep/priority.h"

#include "stream/tv.h"
#include "stream/stream_radio.h"
#ifdef CONFIG_DVBIN
#include "stream/dvbin.h"
#endif
#include "stream/cache2.h"

//**************************************************************************//
//             Playtree
//**************************************************************************//
#include "core/playlist.h"
#include "core/playlist_parser.h"

//**************************************************************************//
//             Config
//**************************************************************************//
#include "core/parser-cfg.h"
#include "core/parser-mpcmd.h"

//**************************************************************************//
//             Config file
//**************************************************************************//

#include "core/path.h"

//**************************************************************************//
//**************************************************************************//
//             Input media streaming & demultiplexer:
//**************************************************************************//

#include "stream/stream.h"
#include "demux/demux.h"
#include "demux/stheader.h"

#ifdef CONFIG_DVDREAD
#include "stream/stream_dvd.h"
#endif

#include "audio/decode/dec_audio.h"
#include "video/decode/dec_video.h"
#include "video/mp_image.h"
#include "video/filter/vf.h"
#include "video/decode/vd.h"

#include "audio/mixer.h"

#include "core/mp_core.h"
#include "core/options.h"
#include "core/defaultopts.h"

static const char help_text[] = _(
"Usage:   mpv [options] [url|path/]filename\n"
"\n"
"Basic options: (complete list in the man page)\n"
" --start=<time>    seek to given (percent, seconds, or hh:mm:ss) position\n"
" --no-audio        do not play sound\n"
" --no-video        do not play video\n"
" --fs              fullscreen playback\n"
" --sub=<file>      specify subtitle file to use\n"
" --playlist=<file> specify playlist file\n"
"\n");

static const char av_desync_help_text[] = _(
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
"     Try a different -vo driver (-vo help for a list) or try -framedrop!\n"
"- Playing a video file with --vo=opengl with higher FPS than the monitor.\n"
"     This is due to vsync limiting the framerate.\n"
"- Playing from a slow network source.\n"
"     Download the file instead.\n"
"- Try to find out whether audio or video is causing this by experimenting\n"
"  with --no-video and --no-audio.\n"
"If none of this helps you, file a bug report.\n\n");


//**************************************************************************//
//**************************************************************************//

#include "core/mp_fifo.h"
#include "sub/ass_mp.h"


// ---

#include "core/mp_common.h"
#include "core/command.h"

static void reset_subtitles(struct MPContext *mpctx);
static void reinit_subs(struct MPContext *mpctx);
static struct track *open_external_file(struct MPContext *mpctx, char *filename,
                                        char *demuxer_name, int stream_cache,
                                        enum stream_type filter);

static float get_relative_time(struct MPContext *mpctx)
{
    unsigned int new_time = GetTimer();
    unsigned int delta = new_time - mpctx->last_time;
    mpctx->last_time = new_time;
    return delta * 0.000001;
}

static double rel_time_to_abs(struct MPContext *mpctx, struct m_rel_time t,
                              double fallback_time)
{
    double length = get_time_length(mpctx);
    switch (t.type) {
    case REL_TIME_ABSOLUTE:
        return t.pos;
    case REL_TIME_NEGATIVE:
        if (length != 0)
            return FFMAX(length - t.pos, 0.0);
        break;
    case REL_TIME_PERCENT:
        if (length != 0)
            return length * (t.pos / 100.0);
        break;
    case REL_TIME_CHAPTER:
        if (chapter_start_time(mpctx, t.pos) >= 0)
            return chapter_start_time(mpctx, t.pos);
        break;
    }
    return fallback_time;
}

static double get_play_end_pts(struct MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    if (opts->play_end.type) {
        return rel_time_to_abs(mpctx, opts->play_end, MP_NOPTS_VALUE);
    } else if (opts->play_length.type) {
        double start = rel_time_to_abs(mpctx, opts->play_start, 0);
        double length = rel_time_to_abs(mpctx, opts->play_length, -1);
        if (start != -1 && length != -1)
            return start + length;
    }
    return MP_NOPTS_VALUE;
}

static void print_stream(struct MPContext *mpctx, struct track *t, int id)
{
    struct sh_stream *s = t->stream;
    const char *tname = "?";
    const char *selopt = "?";
    const char *langopt = "?";
    switch (t->type) {
    case STREAM_VIDEO:
        tname = "Video"; selopt = "vid"; langopt = NULL;
        break;
    case STREAM_AUDIO:
        tname = "Audio"; selopt = "aid"; langopt = "alang";
        break;
    case STREAM_SUB:
        tname = "Subs"; selopt = "sid"; langopt = "slang";
        break;
    }
    mp_msg(MSGT_CPLAYER, MSGL_INFO, "[stream] %-5s %3s",
           tname, mpctx->current_track[t->type] == t ? "(+)" : "");
    mp_msg(MSGT_CPLAYER, MSGL_INFO, " --%s=%d", selopt, t->user_tid);
    if (t->lang && langopt)
        mp_msg(MSGT_CPLAYER, MSGL_INFO, " --%s=%s", langopt, t->lang);
    if (t->default_track)
        mp_msg(MSGT_CPLAYER, MSGL_INFO, " (*)");
    if (t->attached_picture)
        mp_msg(MSGT_CPLAYER, MSGL_INFO, " [P]");
    if (t->title)
        mp_msg(MSGT_CPLAYER, MSGL_INFO, " '%s'", t->title);
    const char *codec = s ? s->codec : NULL;
    if (s && t->type == STREAM_SUB)
        codec = sh_sub_type2str(s->sub->type);
    if (t->sh_sub) // external subs hack
        codec = sh_sub_type2str(t->sh_sub->type);
    mp_msg(MSGT_CPLAYER, MSGL_INFO, " (%s)", codec ? codec : "<unknown>");
    if (t->is_external)
        mp_msg(MSGT_CPLAYER, MSGL_INFO, " (external)");
    mp_msg(MSGT_CPLAYER, MSGL_INFO, "\n");
}

static void print_file_properties(struct MPContext *mpctx, const char *filename)
{
    double start_pts = MP_NOPTS_VALUE;
    double video_start_pts = MP_NOPTS_VALUE;
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_FILENAME=%s\n",
           filename);
    if (mpctx->sh_video) {
        /* Assume FOURCC if all bytes >= 0x20 (' ') */
        if (mpctx->sh_video->format >= 0x20202020)
            mp_msg(MSGT_IDENTIFY, MSGL_INFO,
                   "ID_VIDEO_FORMAT=%.4s\n", (char *)&mpctx->sh_video->format);
        else
            mp_msg(MSGT_IDENTIFY, MSGL_INFO,
                   "ID_VIDEO_FORMAT=0x%08X\n", mpctx->sh_video->format);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO,
               "ID_VIDEO_BITRATE=%d\n", mpctx->sh_video->i_bps * 8);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO,
               "ID_VIDEO_WIDTH=%d\n", mpctx->sh_video->disp_w);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO,
               "ID_VIDEO_HEIGHT=%d\n", mpctx->sh_video->disp_h);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO,
               "ID_VIDEO_FPS=%5.3f\n", mpctx->sh_video->fps);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO,
               "ID_VIDEO_ASPECT=%1.4f\n", mpctx->sh_video->aspect);
        video_start_pts = ds_get_next_pts(mpctx->sh_video->ds);
    }
    if (mpctx->sh_audio) {
        /* Assume FOURCC if all bytes >= 0x20 (' ') */
        if (mpctx->sh_audio->format >= 0x20202020)
            mp_msg(MSGT_IDENTIFY, MSGL_INFO,
                   "ID_AUDIO_FORMAT=%.4s\n", (char *)&mpctx->sh_audio->format);
        else
            mp_msg(MSGT_IDENTIFY, MSGL_INFO,
                   "ID_AUDIO_FORMAT=%d\n", mpctx->sh_audio->format);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO,
               "ID_AUDIO_BITRATE=%d\n", mpctx->sh_audio->i_bps * 8);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO,
               "ID_AUDIO_RATE=%d\n", mpctx->sh_audio->samplerate);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO,
               "ID_AUDIO_NCH=%d\n", mpctx->sh_audio->channels);
        start_pts = ds_get_next_pts(mpctx->sh_audio->ds);
    }
    if (video_start_pts != MP_NOPTS_VALUE) {
        if (start_pts == MP_NOPTS_VALUE || !mpctx->sh_audio ||
            (mpctx->sh_video && video_start_pts < start_pts))
            start_pts = video_start_pts;
    }
    if (start_pts != MP_NOPTS_VALUE)
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_START_TIME=%.2f\n", start_pts);
    else
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_START_TIME=unknown\n");
    mp_msg(MSGT_IDENTIFY, MSGL_INFO,
           "ID_LENGTH=%.2f\n", get_time_length(mpctx));
    int chapter_count = get_chapter_count(mpctx);
    if (chapter_count >= 0) {
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CHAPTERS=%d\n", chapter_count);
        for (int i = 0; i < chapter_count; i++) {
            mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CHAPTER_ID=%d\n", i);
            // print in milliseconds
            double time = chapter_start_time(mpctx, i) * 1000.0;
            mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CHAPTER_%d_START=%"PRId64"\n",
                   i, (int64_t)(time < 0 ? -1 : time));
            char *name = chapter_name(mpctx, i);
            if (name) {
                mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CHAPTER_%d_NAME=%s\n", i,
                       name);
                talloc_free(name);
            }
        }
    }
    struct demuxer *demuxer = mpctx->master_demuxer;
    if (demuxer->num_editions > 1)
        mp_msg(MSGT_CPLAYER, MSGL_INFO,
               "Playing edition %d of %d (--edition=%d).\n",
               demuxer->edition + 1, demuxer->num_editions, demuxer->edition);
    for (int t = 0; t < STREAM_TYPE_COUNT; t++) {
        for (int n = 0; n < mpctx->num_tracks; n++)
            if (mpctx->tracks[n]->type == t)
                print_stream(mpctx, mpctx->tracks[n], n);
    }
}

// Time used to seek external tracks to.
static double get_main_demux_pts(struct MPContext *mpctx)
{
    double main_new_pos = MP_NOPTS_VALUE;
    if (mpctx->demuxer) {
        for (int type = 0; type < STREAM_TYPE_COUNT; type++) {
            struct demux_stream *ds = mpctx->demuxer->ds[type];
            if (ds->sh && main_new_pos == MP_NOPTS_VALUE) {
                demux_fill_buffer(mpctx->demuxer, ds);
                if (ds->first)
                    main_new_pos = ds->first->pts;
            }
        }
    }
    return main_new_pos;
}

static void set_demux_field(struct MPContext *mpctx, enum stream_type type,
                            struct sh_stream *s)
{
    mpctx->sh[type] = s;
    // redundant fields for convenience access
    switch(type) {
        case STREAM_VIDEO: mpctx->sh_video = s ? s->video : NULL; break;
        case STREAM_AUDIO: mpctx->sh_audio = s ? s->audio : NULL; break;
        case STREAM_SUB: mpctx->sh_sub = s ? s->sub : NULL; break;
    }
}

static void init_demux_stream(struct MPContext *mpctx, enum stream_type type)
{
    struct track *track = mpctx->current_track[type];
    set_demux_field(mpctx, type, track ? track->stream : NULL);
    struct sh_stream *stream = mpctx->sh[type];
    if (stream) {
        demuxer_switch_track(stream->demuxer, type, stream);
        if (track->is_external) {
            double pts = get_main_demux_pts(mpctx);
            demux_seek(stream->demuxer, pts, mpctx->audio_delay, SEEK_ABSOLUTE);
        }
    }
}

static void cleanup_demux_stream(struct MPContext *mpctx, enum stream_type type)
{
    struct sh_stream *stream = mpctx->sh[type];
    if (stream)
        demuxer_switch_track(stream->demuxer, type, NULL);
    set_demux_field(mpctx, type, NULL);
}

// Switch the demuxers to current track selection. This is possibly important
// for intialization: if something reads packets from the demuxer (like at least
// reinit_audio_chain does, or when seeking), packets from the other streams
// should be queued instead of discarded. So all streams should be enabled
// before the first initialization function is called.
static void preselect_demux_streams(struct MPContext *mpctx)
{
    // Disable all streams, just to be sure no unwanted streams are selected.
    for (int n = 0; n < mpctx->num_sources; n++) {
        for (int type = 0; type < STREAM_TYPE_COUNT; type++)
            demuxer_switch_track(mpctx->sources[n], type, NULL);
    }

    for (int type = 0; type < STREAM_TYPE_COUNT; type++) {
        struct track *track = mpctx->current_track[type];
        if (track && track->stream)
            demuxer_switch_track(track->stream->demuxer, type, track->stream);
    }
}

static void uninit_subs(struct demuxer *demuxer)
{
    for (int i = 0; i < MAX_S_STREAMS; i++) {
        struct sh_sub *sh = demuxer->s_streams[i];
        if (sh && sh->initialized)
            sub_uninit(sh);
    }
}

void uninit_player(struct MPContext *mpctx, unsigned int mask)
{
    struct MPOpts *opts = &mpctx->opts;

    mask &= mpctx->initialized_flags;

    mp_msg(MSGT_CPLAYER, MSGL_DBG2, "\n*** uninit(0x%X)\n", mask);

    if (mask & INITIALIZED_ACODEC) {
        mpctx->initialized_flags &= ~INITIALIZED_ACODEC;
        if (mpctx->sh_audio)
            uninit_audio(mpctx->sh_audio);
        cleanup_demux_stream(mpctx, STREAM_AUDIO);
        mpctx->mixer.afilter = NULL;
    }

    if (mask & INITIALIZED_SUB) {
        mpctx->initialized_flags &= ~INITIALIZED_SUB;
        struct track *track = mpctx->current_track[STREAM_SUB];
        // One of these was active; they can't be both active.
        assert(!(mpctx->sh_sub && track && track->sh_sub));
        if (mpctx->sh_sub)
            sub_switchoff(mpctx->sh_sub, mpctx->osd);
        if (track && track->sh_sub)
            sub_switchoff(track->sh_sub, mpctx->osd);
        cleanup_demux_stream(mpctx, STREAM_SUB);
        reset_subtitles(mpctx);
    }

    if (mask & INITIALIZED_VCODEC) {
        mpctx->initialized_flags &= ~INITIALIZED_VCODEC;
        if (mpctx->sh_video)
            uninit_video(mpctx->sh_video);
        cleanup_demux_stream(mpctx, STREAM_VIDEO);
    }

    if (mask & INITIALIZED_DEMUXER) {
        mpctx->initialized_flags &= ~INITIALIZED_DEMUXER;
        for (int i = 0; i < mpctx->num_tracks; i++) {
            talloc_free(mpctx->tracks[i]);
        }
        mpctx->num_tracks = 0;
        for (int t = 0; t < STREAM_TYPE_COUNT; t++)
            mpctx->current_track[t] = NULL;
        assert(!mpctx->sh_video && !mpctx->sh_audio && !mpctx->sh_sub);
        mpctx->master_demuxer = NULL;
        for (int i = 0; i < mpctx->num_sources; i++) {
            uninit_subs(mpctx->sources[i]);
            struct demuxer *demuxer = mpctx->sources[i];
            if (demuxer->stream != mpctx->stream)
                free_stream(demuxer->stream);
            free_demuxer(demuxer);
        }
        talloc_free(mpctx->sources);
        mpctx->sources = NULL;
        mpctx->demuxer = NULL;
        mpctx->num_sources = 0;
        talloc_free(mpctx->timeline);
        mpctx->timeline = NULL;
        mpctx->num_timeline_parts = 0;
        talloc_free(mpctx->chapters);
        mpctx->chapters = NULL;
        mpctx->num_chapters = 0;
        mpctx->video_offset = 0;
    }

    // kill the cache process:
    if (mask & INITIALIZED_STREAM) {
        mpctx->initialized_flags &= ~INITIALIZED_STREAM;
        if (mpctx->stream)
            free_stream(mpctx->stream);
        mpctx->stream = NULL;
    }

    if (mask & INITIALIZED_VO) {
        mpctx->initialized_flags &= ~INITIALIZED_VO;
        vo_destroy(mpctx->video_out);
        mpctx->video_out = NULL;
    }

    // Must be after libvo uninit, as few vo drivers (svgalib) have tty code.
    if (mask & INITIALIZED_GETCH2) {
        mpctx->initialized_flags &= ~INITIALIZED_GETCH2;
        mp_msg(MSGT_CPLAYER, MSGL_DBG2, "\n[[[uninit getch2]]]\n");
        // restore terminal:
        getch2_disable();
    }

    if (mask & INITIALIZED_SPUDEC) {
        mpctx->initialized_flags &= ~INITIALIZED_SPUDEC;
        spudec_free(vo_spudec);
        vo_spudec = NULL;
    }

    if (mask & INITIALIZED_AO) {
        mpctx->initialized_flags &= ~INITIALIZED_AO;
        if (mpctx->mixer.ao) {
            // Normally the mixer remembers volume, but do it even if the
            // volume is set explicitly with --volume=...
            if (opts->mixer_init_volume >= 0 && mpctx->mixer.user_set_volume)
                mixer_getbothvolume(&mpctx->mixer, &opts->mixer_init_volume);
            if (opts->mixer_init_mute >= 0 && mpctx->mixer.user_set_mute)
                opts->mixer_init_mute = mixer_getmute(&mpctx->mixer);
            mixer_uninit(&mpctx->mixer);
        }
        if (mpctx->ao)
            ao_uninit(mpctx->ao, mpctx->stop_play != AT_END_OF_FILE);
        mpctx->ao = NULL;
        mpctx->mixer.ao = NULL;
    }
}

static MP_NORETURN void exit_player(struct MPContext *mpctx,
                                    enum exit_reason how, int rc)
{
    uninit_player(mpctx, INITIALIZED_ALL);

#ifdef CONFIG_ENCODING
    encode_lavc_finish(mpctx->encode_lavc_ctx);
    encode_lavc_free(mpctx->encode_lavc_ctx);
#endif

    mpctx->encode_lavc_ctx = NULL;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    timeEndPeriod(1);
#endif

    mp_input_uninit(mpctx->input);

    osd_free(mpctx->osd);

#ifdef CONFIG_ASS
    ass_library_done(mpctx->ass_library);
    mpctx->ass_library = NULL;
#endif

    talloc_free(mpctx->key_fifo);

    if (how != EXIT_NONE) {
        const char *reason;
        switch (how) {
        case EXIT_QUIT:  reason = "Quit"; break;
        case EXIT_EOF:   reason = "End of file"; break;
        case EXIT_ERROR: reason = "Fatal error"; break;
        default:         abort();
        }
        mp_tmsg(MSGT_CPLAYER, MSGL_INFO, "\nExiting... (%s)\n", reason);
    }

    // must be last since e.g. mp_msg uses option values
    // that will be freed by this.
    if (mpctx->mconfig)
        m_config_free(mpctx->mconfig);
    mpctx->mconfig = NULL;

    talloc_free(mpctx);

    exit(rc);
}

#include "cfg-mplayer.h"

static int cfg_include(struct m_config *conf, char *filename)
{
    return m_config_parse_config_file(conf, filename);
}

#define DEF_CONFIG "# Write your default config options here!\n\n\n"

static bool parse_cfgfiles(struct MPContext *mpctx, m_config_t *conf)
{
    struct MPOpts *opts = &mpctx->opts;
    char *conffile;
    int conffile_fd;
    if (!opts->load_config)
        return true;
    if (!m_config_parse_config_file(conf, MPLAYER_CONFDIR "/mpv.conf") < 0)
        return false;
    if ((conffile = mp_find_user_config_file("")) == NULL)
        mp_tmsg(MSGT_CPLAYER, MSGL_WARN, "Cannot find HOME directory.\n");
    else {
        mkdir(conffile, 0777);
        talloc_free(conffile);
        if ((conffile = mp_find_user_config_file("config")) == NULL)
            mp_tmsg(MSGT_CPLAYER, MSGL_ERR,
                    "mp_find_user_config_file(\"config\") problem\n");
        else {
            if ((conffile_fd = open(conffile, O_CREAT | O_EXCL | O_WRONLY,
                        0666)) != -1) {
                mp_tmsg(MSGT_CPLAYER, MSGL_INFO,
                        "Creating config file: %s\n", conffile);
                write(conffile_fd, DEF_CONFIG, sizeof(DEF_CONFIG) - 1);
                close(conffile_fd);
            }
            if (m_config_parse_config_file(conf, conffile) < 0)
                return false;
            talloc_free(conffile);
        }
    }
    return true;
}

#define PROFILE_CFG_PROTOCOL "protocol."

static void load_per_protocol_config(m_config_t *conf, const char * const file)
{
    char *str;
    char protocol[strlen(PROFILE_CFG_PROTOCOL) + strlen(file) + 1];
    m_profile_t *p;

    /* does filename actually uses a protocol ? */
    str = strstr(file, "://");
    if (!str)
        return;

    sprintf(protocol, "%s%s", PROFILE_CFG_PROTOCOL, file);
    protocol[strlen(PROFILE_CFG_PROTOCOL) + strlen(file) - strlen(str)] = '\0';
    p = m_config_get_profile(conf, protocol);
    if (p) {
        mp_tmsg(MSGT_CPLAYER, MSGL_INFO,
                "Loading protocol-related profile '%s'\n", protocol);
        m_config_set_profile(conf, p);
    }
}

#define PROFILE_CFG_EXTENSION "extension."

static void load_per_extension_config(m_config_t *conf, const char * const file)
{
    char *str;
    char extension[strlen(PROFILE_CFG_EXTENSION) + 8];
    m_profile_t *p;

    /* does filename actually have an extension ? */
    str = strrchr(file, '.');
    if (!str)
        return;

    sprintf(extension, PROFILE_CFG_EXTENSION);
    strncat(extension, ++str, 7);
    p = m_config_get_profile(conf, extension);
    if (p) {
        mp_tmsg(MSGT_CPLAYER, MSGL_INFO,
                "Loading extension-related profile '%s'\n", extension);
        m_config_set_profile(conf, p);
    }
}

#define PROFILE_CFG_VO "vo."
#define PROFILE_CFG_AO "ao."

static void load_per_output_config(m_config_t *conf, char *cfg, char *out)
{
    char profile[strlen(cfg) + strlen(out) + 1];
    m_profile_t *p;

    sprintf(profile, "%s%s", cfg, out);
    p = m_config_get_profile(conf, profile);
    if (p) {
        mp_tmsg(MSGT_CPLAYER, MSGL_INFO,
                "Loading extension-related profile '%s'\n", profile);
        m_config_set_profile(conf, p);
    }
}

/**
 * Tries to load a config file
 * @return 0 if file was not found, 1 otherwise
 */
static int try_load_config(m_config_t *conf, const char *file)
{
    if (!mp_path_exists(file))
        return 0;
    mp_tmsg(MSGT_CPLAYER, MSGL_INFO, "Loading config '%s'\n", file);
    m_config_parse_config_file(conf, file);
    return 1;
}

static void load_per_file_config(m_config_t *conf, const char * const file,
                                 bool search_file_dir)
{
    char *confpath;
    char cfg[MP_PATH_MAX];
    const char *name;

    if (strlen(file) > MP_PATH_MAX - 14) {
        mp_msg(MSGT_CPLAYER, MSGL_WARN, "Filename is too long, "
               "can not load file or directory specific config files\n");
        return;
    }
    sprintf(cfg, "%s.conf", file);

    name = mp_basename(cfg);
    if (search_file_dir) {
        char dircfg[MP_PATH_MAX];
        strcpy(dircfg, cfg);
        strcpy(dircfg + (name - cfg), "mpv.conf");
        try_load_config(conf, dircfg);

        if (try_load_config(conf, cfg))
            return;
    }

    if ((confpath = mp_find_user_config_file(name)) != NULL) {
        try_load_config(conf, confpath);

        talloc_free(confpath);
    }
}

static void load_per_file_options(m_config_t *conf,
                                  struct playlist_param *params,
                                  int params_count)
{
    for (int n = 0; n < params_count; n++)
        m_config_set_option(conf, params[n].name, params[n].value);
}

/* When demux performs a blocking operation (network connection or
 * cache filling) if the operation fails we use this function to check
 * if it was interrupted by the user.
 * The function returns whether it was interrupted. */
static bool demux_was_interrupted(struct MPContext *mpctx)
{
    for (;;) {
        if (mpctx->stop_play != KEEP_PLAYING
            && mpctx->stop_play != AT_END_OF_FILE)
            return true;
        mp_cmd_t *cmd = mp_input_get_cmd(mpctx->input, 0, 0);
        if (!cmd)
            break;
        if (mp_input_is_abort_cmd(cmd->id))
            run_command(mpctx, cmd);
        mp_cmd_free(cmd);
    }
    return false;
}

static int find_new_tid(struct MPContext *mpctx, enum stream_type t)
{
    int new_id = -1;
    for (int i = 0; i < mpctx->num_tracks; i++) {
        struct track *track = mpctx->tracks[i];
        if (track->type == t)
            new_id = FFMAX(new_id, track->user_tid);
    }
    return new_id + 1;
}

// Map stream number (as used by libdvdread) to MPEG IDs (as used by demuxer).
static int map_id_from_demuxer(struct demuxer *d, enum stream_type type, int id)
{
    if (d->stream->type == STREAMTYPE_DVD && type == STREAM_SUB)
        id = id & 0x1F;
    return id;
}
static int map_id_to_demuxer(struct demuxer *d, enum stream_type type, int id)
{
    if (d->stream->type == STREAMTYPE_DVD && type == STREAM_SUB)
        id = id | 0x20;
    return id;
}

static struct track *add_stream_track(struct MPContext *mpctx,
                                      struct sh_stream *stream,
                                      bool under_timeline)
{
    for (int i = 0; i < mpctx->num_tracks; i++) {
        struct track *track = mpctx->tracks[i];
        if (track->stream == stream)
            return track;
        // DVD subtitle track that was added later
        if (stream->type == STREAM_SUB && track->type == STREAM_SUB &&
            map_id_from_demuxer(stream->demuxer, stream->type,
                                stream->demuxer_id) == track->demuxer_id
            && !track->stream)
        {
            track->stream = stream;
            track->demuxer_id = stream->demuxer_id;
            // Initialize lazily selected track
            if (track == mpctx->current_track[STREAM_SUB])
                reinit_subs(mpctx);
            return track;
        }
    }

    struct track *track = talloc_ptrtype(NULL, track);
    *track = (struct track) {
        .type = stream->type,
        .user_tid = find_new_tid(mpctx, stream->type),
        .demuxer_id = stream->demuxer_id,
        .title = stream->title,
        .default_track = stream->default_track,
        .attached_picture = stream->attached_picture,
        .lang = stream->lang,
        .under_timeline = under_timeline,
        .demuxer = stream->demuxer,
        .stream = stream,
    };
    MP_TARRAY_APPEND(mpctx, mpctx->tracks, mpctx->num_tracks, track);

    // Needed for DVD and Blu-ray.
    if (!track->lang) {
        struct stream_lang_req req = {
            .type = track->type,
            .id = map_id_from_demuxer(track->demuxer, track->type,
                                      track->demuxer_id)
        };
        stream_control(track->demuxer->stream, STREAM_CTRL_GET_LANG, &req);
        if (req.name[0])
            track->lang = talloc_strdup(track, req.name);
    }

    return track;
}

static void add_demuxer_tracks(struct MPContext *mpctx, struct demuxer *demuxer)
{
    for (int n = 0; n < demuxer->num_streams; n++)
        add_stream_track(mpctx, demuxer->streams[n], !!mpctx->timeline);
}

static void add_dvd_tracks(struct MPContext *mpctx)
{
#ifdef CONFIG_DVDREAD
    struct demuxer *demuxer = mpctx->demuxer;
    struct stream *stream = demuxer->stream;
    if (stream->type == STREAMTYPE_DVD) {
        int n_subs = dvd_number_of_subs(stream);
        for (int n = 0; n < n_subs; n++) {
            struct track *track = talloc_ptrtype(NULL, track);
            *track = (struct track) {
                .type = STREAM_SUB,
                .user_tid = find_new_tid(mpctx, STREAM_SUB),
                .demuxer_id = n,
                .demuxer = mpctx->demuxer,
            };
            MP_TARRAY_APPEND(mpctx, mpctx->tracks, mpctx->num_tracks, track);

            struct stream_lang_req req = {.type = STREAM_SUB, .id = n};
            stream_control(stream, STREAM_CTRL_GET_LANG, &req);
            track->lang = talloc_strdup(track, req.name);
        }
    }
#endif
}

struct track *mp_add_subtitles(struct MPContext *mpctx, char *filename,
                               float fps, int noerr)
{
    struct MPOpts *opts = &mpctx->opts;
    sub_data *subd = NULL;
    struct sh_sub *sh = NULL;

    if (filename == NULL)
        return NULL;

    if (opts->ass_enabled) {
#ifdef CONFIG_ASS
        struct ass_track *asst = mp_ass_read_stream(mpctx->ass_library,
                                                    filename, sub_cp);
        bool is_native_ass = asst;
        if (!asst) {
            subd = sub_read_file(filename, fps, &mpctx->opts);
            if (subd) {
                asst = mp_ass_read_subdata(mpctx->ass_library, opts, subd, fps);
                talloc_free(subd);
                subd = NULL;
            }
        }
        if (asst)
            sh = sd_ass_create_from_track(asst, is_native_ass, opts);
#endif
    } else
        subd = sub_read_file(filename, fps, &mpctx->opts);


    if (!sh && !subd) {
        struct track *ext = open_external_file(mpctx, filename, NULL, 0,
                                               STREAM_SUB);
        if (ext)
            return ext;
        mp_tmsg(MSGT_CPLAYER, noerr ? MSGL_WARN : MSGL_ERR,
                "Cannot load subtitles: %s\n", filename);
        return NULL;
    }

    struct track *track = talloc_ptrtype(NULL, track);
    *track = (struct track) {
        .type = STREAM_SUB,
        .title = talloc_strdup(track, filename),
        .user_tid = find_new_tid(mpctx, STREAM_SUB),
        .demuxer_id = -1,
        .is_external = true,
        .sh_sub = talloc_steal(track, sh),
        .subdata = talloc_steal(track, subd),
        .external_filename = talloc_strdup(track, filename),
    };
    MP_TARRAY_APPEND(mpctx, mpctx->tracks, mpctx->num_tracks, track);
    return track;
}

void init_vo_spudec(struct MPContext *mpctx)
{
    uninit_player(mpctx, INITIALIZED_SPUDEC);
    unsigned width, height;

    // we currently can't work without video stream
    if (!mpctx->sh_video)
        return;

    width  = mpctx->sh_video->disp_w;
    height = mpctx->sh_video->disp_h;

#ifdef CONFIG_DVDREAD
    if (vo_spudec == NULL && mpctx->stream->type == STREAMTYPE_DVD) {
        vo_spudec = spudec_new_scaled(((dvd_priv_t *)(mpctx->stream->priv))->
                cur_pgc->palette, width, height, NULL, 0);
    }
#endif

    if (vo_spudec == NULL && mpctx->sh_sub) {
        sh_sub_t *sh = mpctx->sh_sub;
        vo_spudec = spudec_new_scaled(NULL, width, height, sh->extradata,
                                      sh->extradata_len);
    }

    if (vo_spudec != NULL) {
        mpctx->initialized_flags |= INITIALIZED_SPUDEC;
        mp_property_do("sub-forced-only", M_PROPERTY_SET,
                       &mpctx->opts.forced_subs_only, mpctx);
    }
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

static bool mp_get_cache_idle(struct MPContext *mpctx)
{
    int idle = 0;
    if (mpctx->stream)
        stream_control(mpctx->stream, STREAM_CTRL_GET_CACHE_IDLE, &idle);
    return idle;
}

#define saddf(var, ...) (*(var) = talloc_asprintf_append((*var), __VA_ARGS__))

// append time in the hh:mm:ss format (plus fractions if wanted)
static void sadd_hhmmssff(char **buf, double time, bool fractions)
{
    char *s = mp_format_time(time, fractions);
    *buf = talloc_strdup_append(*buf, s);
    talloc_free(s);
}

static void sadd_percentage(char **buf, int percent) {
    if (percent >= 0)
        *buf = talloc_asprintf_append(*buf, " (%d%%)", percent);
}

static int get_term_width(void)
{
    get_screen_size();
    int width = screen_width > 0 ? screen_width : 80;
#if defined(__MINGW32__) || defined(__CYGWIN__)
    /* Windows command line is broken (MinGW's rxvt works, but we
     * should not depend on that). */
    width--;
#endif
    return width;
}

static void write_status_line(struct MPContext *mpctx, const char *line)
{
    if (erase_to_end_of_line) {
        mp_msg(MSGT_STATUSLINE, MSGL_STATUS,
               "%s%s\r", line, erase_to_end_of_line);
    } else {
        int pos = strlen(line);
        int width = get_term_width() - pos;
        mp_msg(MSGT_STATUSLINE, MSGL_STATUS, "%s%*s\r", line, width, "");
    }
}

static void print_status(struct MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    sh_video_t * const sh_video = mpctx->sh_video;

    if (opts->quiet)
        return;

    if (opts->status_msg) {
        char *r = mp_property_expand_string(mpctx, opts->status_msg);
        write_status_line(mpctx, r);
        talloc_free(r);
        return;
    }

    char *line = NULL;

    // Playback status
    if (mpctx->paused_for_cache) {
        saddf(&line, "(Buffering) ");
    } else if (mpctx->paused) {
        saddf(&line, "(Paused) ");
    }

    if (mpctx->sh_audio)
        saddf(&line, "A");
    if (mpctx->sh_video)
        saddf(&line, "V");
    saddf(&line, ": ");

    // Playback position
    double cur = get_current_time(mpctx);
    sadd_hhmmssff(&line, cur, mpctx->opts.osd_fractions);

    double len = get_time_length(mpctx);
    if (len >= 0) {
        saddf(&line, " / ");
        sadd_hhmmssff(&line, len, mpctx->opts.osd_fractions);
    }

    sadd_percentage(&line, get_percent_pos(mpctx));

    // other
    if (opts->playback_speed != 1)
        saddf(&line, " x%4.2f", opts->playback_speed);

    // A-V sync
    if (mpctx->sh_audio && sh_video) {
        if (mpctx->last_av_difference != MP_NOPTS_VALUE)
            saddf(&line, " A-V:%7.3f", mpctx->last_av_difference);
        else
            saddf(&line, " A-V: ???");
        if (fabs(mpctx->total_avsync_change) > 0.05)
            saddf(&line, " ct:%7.3f", mpctx->total_avsync_change);
    }

#ifdef CONFIG_ENCODING
    double startpos = rel_time_to_abs(mpctx, opts->play_start, 0);
    double endpos = rel_time_to_abs(mpctx, opts->play_end, -1);
    float position = (get_current_time(mpctx) - startpos) /
                     (get_time_length(mpctx) - startpos);
    if (endpos != -1)
        position = max(position, (get_current_time(mpctx) - startpos)
                               / (endpos - startpos));
    if (opts->play_frames > 0)
        position = max(position,
                       1.0 - mpctx->max_frames / (double) opts->play_frames);
    char lavcbuf[80];
    if (encode_lavc_getstatus(mpctx->encode_lavc_ctx, lavcbuf, sizeof(lavcbuf),
            position, get_current_time(mpctx) - startpos) >= 0)
    {
        // encoding stats
        saddf(&line, "%s ", lavcbuf);
    } else
#endif
    {
        // VO stats
        if (sh_video && mpctx->drop_frame_cnt)
            saddf(&line, " D: %d", mpctx->drop_frame_cnt);
    }

    int cache = mp_get_cache_percent(mpctx);
    if (cache >= 0)
        saddf(&line, " Cache: %d%%", cache);

    // end
    write_status_line(mpctx, line);
    talloc_free(line);
}

/**
 * \brief build a chain of audio filters that converts the input format
 * to the ao's format, taking into account the current playback_speed.
 * sh_audio describes the requested input format of the chain.
 * ao describes the requested output format of the chain.
 */
static int build_afilter_chain(struct MPContext *mpctx)
{
    struct sh_audio *sh_audio = mpctx->sh_audio;
    struct ao *ao = mpctx->ao;
    struct MPOpts *opts = &mpctx->opts;
    int new_srate;
    int result;
    if (!sh_audio) {
        mpctx->mixer.afilter = NULL;
        return 0;
    }
    if (af_control_any_rev(sh_audio->afilter,
                           AF_CONTROL_PLAYBACK_SPEED | AF_CONTROL_SET,
                           &opts->playback_speed))
        new_srate = sh_audio->samplerate;
    else {
        new_srate = sh_audio->samplerate * opts->playback_speed;
        if (new_srate != ao->samplerate) {
            // limits are taken from libaf/af_resample.c
            if (new_srate < 8000)
                new_srate = 8000;
            if (new_srate > 192000)
                new_srate = 192000;
            opts->playback_speed = (float)new_srate / sh_audio->samplerate;
        }
    }
    result =  init_audio_filters(sh_audio, new_srate,
                                 &ao->samplerate, &ao->channels, &ao->format);
    mpctx->mixer.afilter = sh_audio->afilter;
    return result;
}


typedef struct mp_osd_msg mp_osd_msg_t;
struct mp_osd_msg {
    /// Previous message on the stack.
    mp_osd_msg_t *prev;
    /// Message text.
    char *msg;
    int id, level, started;
    /// Display duration in ms.
    unsigned time;
    // Show full OSD for duration of message instead of msg
    // (osd_show_progression command)
    bool show_position;
};

static mp_osd_msg_t *add_osd_msg(struct MPContext *mpctx, int id, int level,
                                 int time)
{
    rm_osd_msg(mpctx, id);
    mp_osd_msg_t *msg = talloc_struct(mpctx, mp_osd_msg_t, {
        .prev = mpctx->osd_msg_stack,
        .msg = "",
        .id = id,
        .level = level,
        .time = time,
    });
    mpctx->osd_msg_stack = msg;
    return msg;
}

static void set_osd_msg_va(struct MPContext *mpctx, int id, int level, int time,
                           const char *fmt, va_list ap)
{
    if (level == OSD_LEVEL_INVISIBLE)
        return;
    mp_osd_msg_t *msg = add_osd_msg(mpctx, id, level, time);
    msg->msg = talloc_vasprintf(msg, fmt, ap);
}

void set_osd_msg(struct MPContext *mpctx, int id, int level, int time,
                 const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    set_osd_msg_va(mpctx, id, level, time, fmt, ap);
    va_end(ap);
}

void set_osd_tmsg(struct MPContext *mpctx, int id, int level, int time,
                  const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    set_osd_msg_va(mpctx, id, level, time, mp_gtext(fmt), ap);
    va_end(ap);
}

/**
 *  \brief Remove a message from the OSD stack
 *
 *  This function can be used to get rid of a message right away.
 *
 */

void rm_osd_msg(struct MPContext *mpctx, int id)
{
    mp_osd_msg_t *msg, *last = NULL;

    // Search for the msg
    for (msg = mpctx->osd_msg_stack; msg && msg->id != id;
         last = msg, msg = msg->prev) ;
    if (!msg)
        return;

    // Detach it from the stack and free it
    if (last)
        last->prev = msg->prev;
    else
        mpctx->osd_msg_stack = msg->prev;
    talloc_free(msg);
}

/**
 *  \brief Get the current message from the OSD stack.
 *
 *  This function decrements the message timer and destroys the old ones.
 *  The message that should be displayed is returned (if any).
 *
 */

static mp_osd_msg_t *get_osd_msg(struct MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    mp_osd_msg_t *msg, *prev, *last = NULL;
    static unsigned last_update = 0;
    unsigned now = GetTimerMS();
    unsigned diff;
    char hidden_dec_done = 0;

    if (mpctx->osd_visible) {
        // 36000000 means max timed visibility is 1 hour into the future, if
        // the difference is greater assume it's wrapped around from below 0
        if (mpctx->osd_visible - now > 36000000) {
            mpctx->osd_visible = 0;
            mpctx->osd->progbar_type = -1; // disable
            vo_osd_changed(OSDTYPE_PROGBAR);
        }
    }
    if (mpctx->osd_function_visible) {
        if (mpctx->osd_function_visible - now > 36000000) {
            mpctx->osd_function_visible = 0;
            mpctx->osd_function = 0;
        }
    }

    if (!last_update)
        last_update = now;
    diff = now >= last_update ? now - last_update : 0;

    last_update = now;

    // Look for the first message in the stack with high enough level.
    for (msg = mpctx->osd_msg_stack; msg; last = msg, msg = prev) {
        prev = msg->prev;
        if (msg->level > opts->osd_level && hidden_dec_done)
            continue;
        // The message has a high enough level or it is the first hidden one
        // in both cases we decrement the timer or kill it.
        if (!msg->started || msg->time > diff) {
            if (msg->started)
                msg->time -= diff;
            else
                msg->started = 1;
            // display it
            if (msg->level <= opts->osd_level)
                return msg;
            hidden_dec_done = 1;
            continue;
        }
        // kill the message
        talloc_free(msg);
        if (last) {
            last->prev = prev;
            msg = last;
        } else {
            mpctx->osd_msg_stack = prev;
            msg = NULL;
        }
    }
    // Nothing found
    return NULL;
}

// type: mp_osd_font_codepoints, ASCII, or OSD_BAR_*
// name: fallback for terminal OSD
void set_osd_bar(struct MPContext *mpctx, int type, const char *name,
                 double min, double max, double val)
{
    struct MPOpts *opts = &mpctx->opts;
    if (opts->osd_level < 1 || !opts->osd_bar_visible)
        return;

    if (mpctx->sh_video && opts->term_osd != 1) {
        mpctx->osd_visible = (GetTimerMS() + opts->osd_duration) | 1;
        mpctx->osd->progbar_type = type;
        mpctx->osd->progbar_value = (val - min) / (max - min);
        mpctx->osd->progbar_num_stops = 0;
        vo_osd_changed(OSDTYPE_PROGBAR);
        return;
    }

    set_osd_msg(mpctx, OSD_MSG_BAR, 1, opts->osd_duration, "%s: %d %%",
                name, ROUND(100 * (val - min) / (max - min)));
}

// Update a currently displayed bar of the same type, without resetting the
// timer.
static void update_osd_bar(struct MPContext *mpctx, int type,
                           double min, double max, double val)
{
    if (mpctx->osd->progbar_type == type) {
        float new_value = (val - min) / (max - min);
        if (new_value != mpctx->osd->progbar_value) {
            mpctx->osd->progbar_value = new_value;
            vo_osd_changed(OSDTYPE_PROGBAR);
        }
    }
}

static void set_osd_bar_chapters(struct MPContext *mpctx, int type)
{
    struct osd_state *osd = mpctx->osd;
    osd->progbar_num_stops = 0;
    if (osd->progbar_type == type) {
        double len = get_time_length(mpctx);
        if (len > 0) {
            int num = get_chapter_count(mpctx);
            for (int n = 0; n < num; n++) {
                double time = chapter_start_time(mpctx, n);
                if (time >= 0) {
                    float pos = time / len;
                    MP_TARRAY_APPEND(osd, osd->progbar_stops,
                                     osd->progbar_num_stops, pos);
                }
            }
        }
    }
}

void set_osd_function(struct MPContext *mpctx, int osd_function)
{
    struct MPOpts *opts = &mpctx->opts;

    mpctx->osd_function = osd_function;
    mpctx->osd_function_visible = (GetTimerMS() + opts->osd_duration) | 1;
}

/**
 * \brief Display text subtitles on the OSD
 */
void set_osd_subtitle(struct MPContext *mpctx, subtitle *subs)
{
    int i;
    vo_sub = subs;
    vo_osd_changed(OSDTYPE_SUBTITLE);
    if (!mpctx->sh_video) {
        // reverse order, since newest set_osd_msg is displayed first
        for (i = SUB_MAX_TEXT - 1; i >= 0; i--) {
            if (!subs || i >= subs->lines || !subs->text[i])
                rm_osd_msg(mpctx, OSD_MSG_SUB_BASE + i);
            else {
                // HACK: currently display time for each sub line
                // except the last is set to 2 seconds.
                int display_time = i == subs->lines - 1 ? 180000 : 2000;
                set_osd_msg(mpctx, OSD_MSG_SUB_BASE + i, 1, display_time,
                            "%s", subs->text[i]);
            }
        }
    }
}

// sym == mpctx->osd_function
static void saddf_osd_function_sym(char **buffer, int sym)
{
    char temp[10];
    osd_get_function_sym(temp, sizeof(temp), sym);
    saddf(buffer, "%s ", temp);
}

static void sadd_osd_status(char **buffer, struct MPContext *mpctx, bool full)
{
    bool fractions = mpctx->opts.osd_fractions;
    int sym = mpctx->osd_function;
    if (!sym) {
        if (mpctx->paused_for_cache) {
            sym = OSD_CLOCK;
        } else if (mpctx->paused || mpctx->step_frames) {
            sym = OSD_PAUSE;
        } else {
            sym = OSD_PLAY;
        }
    }
    saddf_osd_function_sym(buffer, sym);
    char *custom_msg = mpctx->opts.osd_status_msg;
    if (custom_msg && full) {
        char *text = mp_property_expand_string(mpctx, custom_msg);
        *buffer = talloc_strdup_append(*buffer, text);
        talloc_free(text);
    } else {
        sadd_hhmmssff(buffer, get_current_time(mpctx), fractions);
        if (full) {
            saddf(buffer, " / ");
            sadd_hhmmssff(buffer, get_time_length(mpctx), fractions);
            sadd_percentage(buffer, get_percent_pos(mpctx));
            int cache = mp_get_cache_percent(mpctx);
            if (cache >= 0)
                saddf(buffer, " Cache: %d%%", cache);
        }
    }
}

// OSD messages initated by seeking commands are added lazily with this
// function, because multiple successive seek commands can be coalesced.
static void add_seek_osd_messages(struct MPContext *mpctx)
{
    if (mpctx->add_osd_seek_info & OSD_SEEK_INFO_BAR) {
        set_osd_bar(mpctx, OSD_BAR_SEEK, "Position", 0, 1,
                    av_clipf(get_current_pos_ratio(mpctx), 0, 1));
        set_osd_bar_chapters(mpctx, OSD_BAR_SEEK);
    }
    if (mpctx->add_osd_seek_info & OSD_SEEK_INFO_TEXT) {
        mp_osd_msg_t *msg = add_osd_msg(mpctx, OSD_MSG_TEXT, 1,
                                        mpctx->opts.osd_duration);
        msg->show_position = true;
    }
    if (mpctx->add_osd_seek_info & OSD_SEEK_INFO_CHAPTER_TEXT) {
        char *chapter = chapter_display_name(mpctx, get_current_chapter(mpctx));
        set_osd_tmsg(mpctx, OSD_MSG_TEXT, 1, mpctx->opts.osd_duration,
                     "Chapter: %s", chapter);
        talloc_free(chapter);
    }
    if ((mpctx->add_osd_seek_info & OSD_SEEK_INFO_EDITION)
        && mpctx->master_demuxer)
    {
        set_osd_tmsg(mpctx, OSD_MSG_TEXT, 1, mpctx->opts.osd_duration,
                     "Playing edition %d of %d.",
                     mpctx->master_demuxer->edition + 1,
                     mpctx->master_demuxer->num_editions);
    }
    mpctx->add_osd_seek_info = 0;
}

/**
 * \brief Update the OSD message line.
 *
 * This function displays the current message on the vo OSD or on the term.
 * If the stack is empty and the OSD level is high enough the timer
 * is displayed (only on the vo OSD).
 *
 */

static void update_osd_msg(struct MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    struct osd_state *osd = mpctx->osd;

    add_seek_osd_messages(mpctx);
    update_osd_bar(mpctx, OSD_BAR_SEEK, 0, 1,
                   av_clipf(get_current_pos_ratio(mpctx), 0, 1));

    // Look if we have a msg
    mp_osd_msg_t *msg = get_osd_msg(mpctx);
    if (msg && !msg->show_position) {
        if (mpctx->sh_video && opts->term_osd != 1) {
            osd_set_text(osd, msg->msg);
        } else if (opts->term_osd) {
            if (strcmp(mpctx->terminal_osd_text, msg->msg)) {
                talloc_free(mpctx->terminal_osd_text);
                mpctx->terminal_osd_text = talloc_strdup(mpctx, msg->msg);
                // Multi-line message => clear what will be the second line
                write_status_line(mpctx, "");
                mp_msg(MSGT_CPLAYER, MSGL_STATUS, "%s%s\n", opts->term_osd_esc,
                       mpctx->terminal_osd_text);
                print_status(mpctx);
            }
        }
        return;
    }

    int osd_level = opts->osd_level;
    if (msg && msg->show_position)
        osd_level = 3;

    if (mpctx->sh_video && opts->term_osd != 1) {
        // fallback on the timer
        char *text = NULL;

        if (osd_level >= 2)
            sadd_osd_status(&text, mpctx, osd_level == 3);

        osd_set_text(osd, text);
        talloc_free(text);
        return;
    }

    // Clear the term osd line
    if (opts->term_osd && mpctx->terminal_osd_text[0]) {
        mpctx->terminal_osd_text[0] = '\0';
        mp_msg(MSGT_CPLAYER, MSGL_STATUS, "%s\n", opts->term_osd_esc);
    }
}

void reinit_audio_chain(struct MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    struct ao *ao;
    init_demux_stream(mpctx, STREAM_AUDIO);
    if (!mpctx->sh_audio) {
        uninit_player(mpctx, INITIALIZED_AO);
        goto no_audio;
    }
    if (!(mpctx->initialized_flags & INITIALIZED_ACODEC)) {
        if (!init_best_audio_codec(mpctx->sh_audio, opts->audio_decoders))
            goto init_error;
        mpctx->initialized_flags |= INITIALIZED_ACODEC;
    }

    if (!(mpctx->initialized_flags & INITIALIZED_AO)) {
        mpctx->initialized_flags |= INITIALIZED_AO;
        mpctx->ao = ao_create(opts, mpctx->input);
        mpctx->ao->samplerate = opts->force_srate;
        mpctx->ao->format = opts->audio_output_format;
        // Automatic downmix
        if (opts->audio_output_channels == 2 && mpctx->sh_audio->channels != 2)
            mp_chmap_from_channels(&mpctx->ao->channels, 2);
    }
    ao = mpctx->ao;

    // first init to detect best values
    if (!init_audio_filters(mpctx->sh_audio,  // preliminary init
                            // input:
                            mpctx->sh_audio->samplerate,
                            // output:
                            &ao->samplerate, &ao->channels, &ao->format)) {
        mp_tmsg(MSGT_CPLAYER, MSGL_ERR, "Error at audio filter chain "
                "pre-init!\n");
        goto init_error;
    }
    if (!ao->initialized) {
        ao->buffersize = opts->ao_buffersize;
        ao->encode_lavc_ctx = mpctx->encode_lavc_ctx;
        ao_init(ao, opts->audio_driver_list);
        if (!ao->initialized) {
            mp_tmsg(MSGT_CPLAYER, MSGL_ERR,
                    "Could not open/initialize audio device -> no sound.\n");
            goto init_error;
        }
        ao->buffer.start = talloc_new(ao);
        mp_msg(MSGT_CPLAYER, MSGL_INFO,
               "AO: [%s] %dHz %dch %s (%d bytes per sample)\n",
               ao->driver->info->short_name,
               ao->samplerate, ao->channels.num,
               af_fmt2str_short(ao->format),
               af_fmt2bits(ao->format) / 8);
        mp_msg(MSGT_CPLAYER, MSGL_V, "AO: Description: %s\nAO: Author: %s\n",
               ao->driver->info->name, ao->driver->info->author);
        if (strlen(ao->driver->info->comment) > 0)
            mp_msg(MSGT_CPLAYER, MSGL_V, "AO: Comment: %s\n",
                   ao->driver->info->comment);
    }

    // init audio filters:
    if (!build_afilter_chain(mpctx)) {
        mp_tmsg(MSGT_CPLAYER, MSGL_ERR,
                "Couldn't find matching filter/ao format!\n");
        goto init_error;
    }
    mpctx->mixer.volstep = opts->volstep;
    mpctx->mixer.softvol = opts->softvol;
    mpctx->mixer.softvol_max = opts->softvol_max;
    mixer_reinit(&mpctx->mixer, ao);
    mpctx->syncing_audio = true;
    return;

init_error:
    uninit_player(mpctx, INITIALIZED_ACODEC | INITIALIZED_AO);
    cleanup_demux_stream(mpctx, STREAM_AUDIO);
no_audio:
    mpctx->current_track[STREAM_AUDIO] = NULL;
    mp_tmsg(MSGT_CPLAYER, MSGL_INFO, "Audio: no audio\n");
}


// Return pts value corresponding to the end point of audio written to the
// ao so far.
static double written_audio_pts(struct MPContext *mpctx)
{
    sh_audio_t *sh_audio = mpctx->sh_audio;
    if (!sh_audio)
        return MP_NOPTS_VALUE;
    demux_stream_t *d_audio = mpctx->sh_audio->ds;
    // first calculate the end pts of audio that has been output by decoder
    double a_pts = sh_audio->pts;
    if (a_pts != MP_NOPTS_VALUE)
        // Good, decoder supports new way of calculating audio pts.
        // sh_audio->pts is the timestamp of the latest input packet with
        // known pts that the decoder has decoded. sh_audio->pts_bytes is
        // the amount of bytes the decoder has written after that timestamp.
        a_pts += sh_audio->pts_bytes / (double) sh_audio->o_bps;
    else {
        // Decoder doesn't support new way of calculating pts (or we're
        // being called before it has decoded anything with known timestamp).
        // Use the old method of audio pts calculation: take the timestamp
        // of last packet with known pts the decoder has read data from,
        // and add amount of bytes read after the beginning of that packet
        // divided by input bps. This will be inaccurate if the input/output
        // ratio is not constant for every audio packet or if it is constant
        // but not accurately known in sh_audio->i_bps.

        a_pts = d_audio->pts;
        if (a_pts == MP_NOPTS_VALUE)
            return a_pts;

        // ds_tell_pts returns bytes read after last timestamp from
        // demuxing layer, decoder might use sh_audio->a_in_buffer for bytes
        // it has read but not decoded
        if (sh_audio->i_bps)
            a_pts += (ds_tell_pts(d_audio) - sh_audio->a_in_buffer_len) /
                     (double)sh_audio->i_bps;
    }
    // Now a_pts hopefully holds the pts for end of audio from decoder.
    // Substract data in buffers between decoder and audio out.

    // Decoded but not filtered
    a_pts -= sh_audio->a_buffer_len / (double)sh_audio->o_bps;

    // Data buffered in audio filters, measured in bytes of "missing" output
    double buffered_output = af_calc_delay(sh_audio->afilter);

    // Data that was ready for ao but was buffered because ao didn't fully
    // accept everything to internal buffers yet
    buffered_output += mpctx->ao->buffer.len;

    // Filters divide audio length by playback_speed, so multiply by it
    // to get the length in original units without speedup or slowdown
    a_pts -= buffered_output * mpctx->opts.playback_speed / mpctx->ao->bps;

    return a_pts + mpctx->video_offset;
}

// Return pts value corresponding to currently playing audio.
double playing_audio_pts(struct MPContext *mpctx)
{
    double pts = written_audio_pts(mpctx);
    if (pts == MP_NOPTS_VALUE)
        return pts;
    return pts - mpctx->opts.playback_speed *ao_get_delay(mpctx->ao);
}

// When reading subtitles from a demuxer, and we don't read video or audio
// from the demuxer, we must explicitly read subtitle packets. (Normally,
// subs are interleaved with video and audio, so we get them automatically.)
static bool is_non_interleaved(struct MPContext *mpctx, struct track *track)
{
    if (track->is_external || !track->demuxer)
        return true;

    struct demuxer *demuxer = track->demuxer;
    for (int type = 0; type < STREAM_TYPE_COUNT; type++) {
        struct track *other = mpctx->current_track[type];
        if (other && other != track && other->demuxer && other->demuxer == demuxer)
            return false;
    }
    return true;
}

static void reset_subtitles(struct MPContext *mpctx)
{
    if (mpctx->sh_sub)
        sub_reset(mpctx->sh_sub, mpctx->osd);
    sub_clear_text(&mpctx->subs, MP_NOPTS_VALUE);
    if (vo_sub)
        set_osd_subtitle(mpctx, NULL);
    if (vo_spudec) {
        spudec_reset(vo_spudec);
        vo_osd_changed(OSDTYPE_SPU);
    }
}

static void update_subtitles(struct MPContext *mpctx, double refpts_tl)
{
    struct MPOpts *opts = &mpctx->opts;
    struct sh_video *sh_video = mpctx->sh_video;
    struct sh_sub *sh_sub = mpctx->sh_sub;
    struct demux_stream *d_sub = sh_sub ? sh_sub->ds : NULL;
    unsigned char *packet = NULL;
    int len;
    int type = sh_sub ? sh_sub->type : '\0';

    mpctx->osd->sub_offset = mpctx->video_offset;

    struct track *track = mpctx->current_track[STREAM_SUB];
    if (!track)
        return;

    if (!track->under_timeline)
        mpctx->osd->sub_offset = 0;

    double refpts_s = refpts_tl - mpctx->osd->sub_offset;
    double curpts_s = refpts_s + sub_delay;

    // find sub
    if (track->subdata) {
        if (sub_fps == 0)
            sub_fps = sh_video ? sh_video->fps : 25;
        find_sub(mpctx, track->subdata, curpts_s *
                 (track->subdata->sub_uses_time ? 100. : sub_fps));
    }

    // DVD sub:
    if (type == 'v' && !(sh_sub && sh_sub->active)) {
        int timestamp;
        // Get a sub packet from the demuxer (or the vobsub.c thing, which
        // should be a demuxer, but isn't).
        while (1) {
            // Vobsub
            len = 0;
            {
                // DVD sub
                assert(d_sub->sh == sh_sub);
                len = ds_get_packet_sub(d_sub, (unsigned char **)&packet);
                if (len > 0) {
                    // XXX This is wrong, sh_video->pts can be arbitrarily
                    // much behind demuxing position. Unfortunately using
                    // d_video->pts which would have been the simplest
                    // improvement doesn't work because mpeg specific hacks
                    // in video.c set d_video->pts to 0.
                    float x = d_sub->pts - refpts_s;
                    if (x > -20 && x < 20) // prevent missing subs on pts reset
                        timestamp = 90000 * d_sub->pts;
                    else
                        timestamp = 90000 * curpts_s;
                    mp_dbg(MSGT_CPLAYER, MSGL_V, "\rDVD sub: len=%d  "
                           "v_pts=%5.3f  s_pts=%5.3f  ts=%d \n", len,
                           refpts_s, d_sub->pts, timestamp);
                }
            }
            if (len <= 0 || !packet)
                break;
            // create it only here, since with some broken demuxers we might
            // type = v but no DVD sub and we currently do not change the
            // "original frame size" ever after init, leading to wrong-sized
            // PGS subtitles.
            if (!vo_spudec)
                vo_spudec = spudec_new(NULL);
            if (timestamp >= 0)
                spudec_assemble(vo_spudec, packet, len, timestamp);
        }
    } else if (d_sub && (is_text_sub(type) || (sh_sub && sh_sub->active))) {
        bool non_interleaved = is_non_interleaved(mpctx, track);
        if (non_interleaved)
            ds_get_next_pts(d_sub);

        while (d_sub->first) {
            double subpts_s = ds_get_next_pts(d_sub);
            if (subpts_s > curpts_s) {
                mp_dbg(MSGT_CPLAYER, MSGL_DBG2,
                       "Sub early: c_pts=%5.3f s_pts=%5.3f\n",
                       curpts_s, subpts_s);
                // Libass handled subs can be fed to it in advance
                if (!opts->ass_enabled || !is_text_sub(type))
                    break;
                // Try to avoid demuxing whole file at once
                if (non_interleaved && subpts_s > curpts_s + 1)
                    break;
            }
            double duration = d_sub->first->duration;
            len = ds_get_packet_sub(d_sub, &packet);
            mp_dbg(MSGT_CPLAYER, MSGL_V, "Sub: c_pts=%5.3f s_pts=%5.3f "
                   "duration=%5.3f len=%d\n", curpts_s, subpts_s, duration,
                   len);
            if (type == 'm') {
                if (len < 2)
                    continue;
                len = FFMIN(len - 2, AV_RB16(packet));
                packet += 2;
            }
            if (sh_sub && sh_sub->active) {
                sub_decode(sh_sub, mpctx->osd, packet, len, subpts_s, duration);
            } else if (subpts_s != MP_NOPTS_VALUE) {
                // text sub
                if (duration < 0)
                    sub_clear_text(&mpctx->subs, MP_NOPTS_VALUE);
                if (type == 'a') { // ssa/ass subs without libass => convert to plaintext
                    int i;
                    unsigned char *p = packet;
                    for (i = 0; i < 8 && *p != '\0'; p++)
                        if (*p == ',')
                            i++;
                    if (*p == '\0')  /* Broken line? */
                        continue;
                    len -= p - packet;
                    packet = p;
                }
                double endpts_s = MP_NOPTS_VALUE;
                if (subpts_s != MP_NOPTS_VALUE && duration >= 0)
                    endpts_s = subpts_s + duration;
                sub_add_text(&mpctx->subs, packet, len, endpts_s);
                set_osd_subtitle(mpctx, &mpctx->subs);
            }
            if (non_interleaved)
                ds_get_next_pts(d_sub);
        }
        if (!opts->ass_enabled)
            if (sub_clear_text(&mpctx->subs, curpts_s))
                set_osd_subtitle(mpctx, &mpctx->subs);
    }
    if (vo_spudec) {
        spudec_heartbeat(vo_spudec, 90000 * curpts_s);
        if (spudec_changed(vo_spudec))
            vo_osd_changed(OSDTYPE_SPU);
    }
}

static int check_framedrop(struct MPContext *mpctx, double frame_time)
{
    struct MPOpts *opts = &mpctx->opts;
    // check for frame-drop:
    if (mpctx->sh_audio && !mpctx->ao->untimed && !mpctx->sh_audio->ds->eof) {
        float delay = opts->playback_speed * ao_get_delay(mpctx->ao);
        float d = delay - mpctx->delay;
        // we should avoid dropping too many frames in sequence unless we
        // are too late. and we allow 100ms A-V delay here:
        if (d < -mpctx->dropped_frames * frame_time - 0.100 && !mpctx->paused
            && !mpctx->restart_playback) {
            mpctx->drop_frame_cnt++;
            mpctx->dropped_frames++;
            return mpctx->opts.frame_dropping;
        } else
            mpctx->dropped_frames = 0;
    }
    return 0;
}

static float timing_sleep(struct MPContext *mpctx, float time_frame)
{
    // assume kernel HZ=100 for softsleep, works with larger HZ but with
    // unnecessarily high CPU usage
    struct MPOpts *opts = &mpctx->opts;
    float margin = opts->softsleep ? 0.011 : 0;
    while (time_frame > margin) {
        usec_sleep(1000000 * (time_frame - margin));
        time_frame -= get_relative_time(mpctx);
    }
    if (opts->softsleep) {
        if (time_frame < 0)
            mp_tmsg(MSGT_AVSYNC, MSGL_WARN,
                    "Warning! Softsleep underflow!\n");
        while (time_frame > 0)
            time_frame -= get_relative_time(mpctx);  // burn the CPU
    }
    return time_frame;
}

static void set_dvdsub_fake_extradata(struct sh_sub *sh_sub, struct stream *st,
                                      struct sh_video *sh_video)
{
#ifdef CONFIG_DVDREAD
    if (st->type != STREAMTYPE_DVD || !sh_video)
        return;

    struct mp_csp_params csp = MP_CSP_PARAMS_DEFAULTS;
    csp.int_bits_in = 8;
    csp.int_bits_out = 8;
    float cmatrix[3][4];
    mp_get_yuv2rgb_coeffs(&csp, cmatrix);

    int width  = sh_video->disp_w;
    int height = sh_video->disp_h;
    int *palette = ((dvd_priv_t *)st->priv)->cur_pgc->palette;

    char *s = NULL;
    s = talloc_asprintf_append(s, "size: %dx%d\n", width, height);
    s = talloc_asprintf_append(s, "palette: ");
    for (int i = 0; i < 16; i++) {
        int color = palette[i];
        int c[3] = {(color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff};
        mp_map_int_color(cmatrix, 8, c);
        color = (c[2] << 16) | (c[1] << 8) | c[0];

        if (i != 0)
            talloc_asprintf_append(s, ", ");
        s = talloc_asprintf_append(s, "%06x", color);
    }
    s = talloc_asprintf_append(s, "\n");

    free(sh_sub->extradata);
    sh_sub->extradata = strdup(s);
    sh_sub->extradata_len = strlen(s);
    talloc_free(s);
#endif
}

static void reinit_subs(struct MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    struct track *track = mpctx->current_track[STREAM_SUB];

    assert(!(mpctx->initialized_flags & INITIALIZED_SUB));

    init_demux_stream(mpctx, STREAM_SUB);

    if (!track)
        return;

    if (track->demuxer && !track->stream) {
        // Lazily added DVD track - we must not miss the first subtitle packet,
        // which makes the demuxer create the sh_stream, and contains the first
        // subtitle event.

        // demux_mpg - maps IDs directly to the logical stream number
        track->demuxer->sub->id = track->demuxer_id;

        // demux_lavf - IDs are essentially random, have to use MPEG IDs
        int id = map_id_to_demuxer(track->demuxer, track->type,
                                   track->demuxer_id);
        demux_control(track->demuxer, DEMUXER_CTRL_AUTOSELECT_SUBTITLE, &id);

        return;
    }

    mpctx->initialized_flags |= INITIALIZED_SUB;

    if (track->subdata || track->sh_sub) {
#ifdef CONFIG_ASS
        if (opts->ass_enabled && track->sh_sub)
            sub_init(track->sh_sub, mpctx->osd);
#endif
        vo_osd_changed(OSDTYPE_SUBTITLE);
    } else if (track->stream) {
        struct stream *s = track->demuxer ? track->demuxer->stream : NULL;
        if (s && s->type == STREAMTYPE_DVD)
            set_dvdsub_fake_extradata(mpctx->sh_sub, s, mpctx->sh_video);
        // lavc dvdsubdec doesn't read color/resolution on Libav 9.1 and below
        // Don't use it for new ffmpeg; spudec can't handle ffmpeg .idx demuxing
        // (ffmpeg added .idx demuxing during lavc 54.79.100)
        bool broken_lavc = false;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 40, 0)
        broken_lavc = true;
#endif
        if (mpctx->sh_sub->type == 'v' && track->demuxer
            && (track->demuxer->type == DEMUXER_TYPE_MPEG_PS || broken_lavc))
            init_vo_spudec(mpctx);
        else
            sub_init(mpctx->sh_sub, mpctx->osd);
    }
}

static char *track_layout_hash(struct MPContext *mpctx)
{
    char *h = talloc_strdup(NULL, "");
    for (int type = 0; type < STREAM_TYPE_COUNT; type++) {
        for (int n = 0; n < mpctx->num_tracks; n++) {
            struct track *track = mpctx->tracks[n];
            if (track->type != type)
                continue;
            h = talloc_asprintf_append_buffer(h, "%d-%d-%d-%d-%s\n", type,
                    track->user_tid, track->default_track, track->is_external,
                    track->lang ? track->lang : "");
        }
    }
    return h;
}

void mp_switch_track(struct MPContext *mpctx, enum stream_type type,
                     struct track *track)
{
    assert(!track || track->type == type);

    struct track *current = mpctx->current_track[type];
    if (track == current)
        return;

    if (type == STREAM_VIDEO) {
        uninit_player(mpctx, INITIALIZED_VCODEC |
                        (mpctx->opts.fixed_vo && track ? 0 : INITIALIZED_VO));
    } else if (type == STREAM_AUDIO) {
        uninit_player(mpctx, INITIALIZED_AO | INITIALIZED_ACODEC);
    } else if (type == STREAM_SUB) {
        uninit_player(mpctx, INITIALIZED_SUB);
    }

    mpctx->current_track[type] = track;

    int user_tid = track ? track->user_tid : -2;
    if (type == STREAM_VIDEO) {
        mpctx->opts.video_id = user_tid;
        reinit_video_chain(mpctx);
    } else if (type == STREAM_AUDIO) {
        mpctx->opts.audio_id = user_tid;
        reinit_audio_chain(mpctx);
    } else if (type == STREAM_SUB) {
        mpctx->opts.sub_id = user_tid;
        reinit_subs(mpctx);
    }

    talloc_free(mpctx->track_layout_hash);
    mpctx->track_layout_hash = talloc_steal(mpctx, track_layout_hash(mpctx));
}

struct track *mp_track_by_tid(struct MPContext *mpctx, enum stream_type type,
                              int tid)
{
    if (tid == -1)
        return mpctx->current_track[type];
    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *track = mpctx->tracks[n];
        if (track->type == type && track->user_tid == tid)
            return track;
    }
    return NULL;
}

bool mp_remove_track(struct MPContext *mpctx, struct track *track)
{
    if (track->under_timeline)
        return false;
    if (!track->is_external)
        return false;

    if (mpctx->current_track[track->type] == track) {
        mp_switch_track(mpctx, track->type, NULL);
        if (mpctx->current_track[track->type] == track)
            return false;
    }

    int index = 0;
    while (index < mpctx->num_tracks && mpctx->tracks[index] != track)
        index++;
    assert(index < mpctx->num_tracks);
    while (index + 1 < mpctx->num_tracks) {
        mpctx->tracks[index] = mpctx->tracks[index + 1];
        index++;
    }
    mpctx->num_tracks--;
    talloc_free(track);
    return true;
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
    struct MPOpts *opts = &mpctx->opts;

    if (!mpctx->sh_audio || mpctx->syncing_audio)
        return;

    double a_pts = written_audio_pts(mpctx) - mpctx->delay;
    double v_pts = mpctx->sh_video->pts;
    double av_delay = a_pts - v_pts;
    // Try to sync vo_flip() so it will *finish* at given time
    av_delay += mpctx->last_vo_flip_duration;
    av_delay -= mpctx->audio_delay;   // This much pts difference is desired

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

static int write_to_ao(struct MPContext *mpctx, void *data, int len, int flags,
                       double pts)
{
    if (mpctx->paused)
        return 0;
    struct ao *ao = mpctx->ao;
    double bps = ao->bps / mpctx->opts.playback_speed;
    ao->pts = pts;
    int played = ao_play(mpctx->ao, data, len, flags);
    if (played > 0) {
        mpctx->delay += played / bps;
        // Keep correct pts for remaining data - could be used to flush
        // remaining buffer when closing ao.
        ao->pts += played / bps;
        return played;
    }
    return 0;
}

#define ASYNC_PLAY_DONE -3
static int audio_start_sync(struct MPContext *mpctx, int playsize)
{
    struct ao *ao = mpctx->ao;
    struct MPOpts *opts = &mpctx->opts;
    sh_audio_t * const sh_audio = mpctx->sh_audio;
    int res;

    // Timing info may not be set without
    res = decode_audio(sh_audio, &ao->buffer, 1);
    if (res < 0)
        return res;

    int bytes;
    bool did_retry = false;
    double written_pts;
    double bps = ao->bps / opts->playback_speed;
    bool hrseek = mpctx->hrseek_active;   // audio only hrseek
    mpctx->hrseek_active = false;
    while (1) {
        written_pts = written_audio_pts(mpctx);
        double ptsdiff;
        if (hrseek)
            ptsdiff = written_pts - mpctx->hrseek_pts;
        else
            ptsdiff = written_pts - mpctx->sh_video->pts - mpctx->delay
                      - mpctx->audio_delay;
        bytes = ptsdiff * bps;
        bytes -= bytes % (ao->channels.num * af_fmt2bits(ao->format) / 8);

        // ogg demuxers give packets without timing
        if (written_pts <= 1 && sh_audio->pts == MP_NOPTS_VALUE) {
            if (!did_retry) {
                // Try to read more data to see packets that have pts
                int res = decode_audio(sh_audio, &ao->buffer, ao->bps);
                if (res < 0)
                    return res;
                did_retry = true;
                continue;
            }
            bytes = 0;
        }

        if (fabs(ptsdiff) > 300 || isnan(ptsdiff))   // pts reset or just broken?
            bytes = 0;

        if (bytes > 0)
            break;

        mpctx->syncing_audio = false;
        int a = FFMIN(-bytes, FFMAX(playsize, 20000));
        int res = decode_audio(sh_audio, &ao->buffer, a);
        bytes += ao->buffer.len;
        if (bytes >= 0) {
            memmove(ao->buffer.start,
                    ao->buffer.start + ao->buffer.len - bytes, bytes);
            ao->buffer.len = bytes;
            if (res < 0)
                return res;
            return decode_audio(sh_audio, &ao->buffer, playsize);
        }
        ao->buffer.len = 0;
        if (res < 0)
            return res;
    }
    if (hrseek)
        // Don't add silence in audio-only case even if position is too late
        return 0;
    int fillbyte = 0;
    if ((ao->format & AF_FORMAT_SIGN_MASK) == AF_FORMAT_US)
        fillbyte = 0x80;
    if (bytes >= playsize) {
        /* This case could fall back to the one below with
         * bytes = playsize, but then silence would keep accumulating
         * in a_out_buffer if the AO accepts less data than it asks for
         * in playsize. */
        char *p = malloc(playsize);
        memset(p, fillbyte, playsize);
        write_to_ao(mpctx, p, playsize, 0, written_pts - bytes / bps);
        free(p);
        return ASYNC_PLAY_DONE;
    }
    mpctx->syncing_audio = false;
    decode_audio_prepend_bytes(&ao->buffer, bytes, fillbyte);
    return decode_audio(sh_audio, &ao->buffer, playsize);
}

static int fill_audio_out_buffers(struct MPContext *mpctx, double endpts)
{
    struct MPOpts *opts = &mpctx->opts;
    struct ao *ao = mpctx->ao;
    int playsize;
    int playflags = 0;
    bool audio_eof = false;
    bool partial_fill = false;
    sh_audio_t * const sh_audio = mpctx->sh_audio;
    bool modifiable_audio_format = !(ao->format & AF_FORMAT_SPECIAL_MASK);
    int unitsize = ao->channels.num * af_fmt2bits(ao->format) / 8;

    if (mpctx->paused)
        playsize = 1;   // just initialize things (audio pts at least)
    else
        playsize = ao_get_space(ao);

    // Coming here with hrseek_active still set means audio-only
    if (!mpctx->sh_video)
        mpctx->syncing_audio = false;
    if (!opts->initial_audio_sync || !modifiable_audio_format) {
        mpctx->syncing_audio = false;
        mpctx->hrseek_active = false;
    }

    int res;
    if (mpctx->syncing_audio || mpctx->hrseek_active)
        res = audio_start_sync(mpctx, playsize);
    else
        res = decode_audio(sh_audio, &ao->buffer, playsize);

    if (res < 0) {  // EOF, error or format change
        if (res == -2) {
            /* The format change isn't handled too gracefully. A more precise
             * implementation would require draining buffered old-format audio
             * while displaying video, then doing the output format switch.
             */
            if (!mpctx->opts.gapless_audio)
                uninit_player(mpctx, INITIALIZED_AO);
            reinit_audio_chain(mpctx);
            return -1;
        } else if (res == ASYNC_PLAY_DONE)
            return 0;
        else if (mpctx->sh_audio->ds->eof)
            audio_eof = true;
    }

    if (endpts != MP_NOPTS_VALUE && modifiable_audio_format) {
        double bytes = (endpts - written_audio_pts(mpctx) + mpctx->audio_delay)
                       * ao->bps / opts->playback_speed;
        if (playsize > bytes) {
            playsize = FFMAX(bytes, 0);
            playflags |= AOPLAY_FINAL_CHUNK;
            audio_eof = true;
            partial_fill = true;
        }
    }

    assert(ao->buffer.len % unitsize == 0);
    if (playsize > ao->buffer.len) {
        partial_fill = true;
        playsize = ao->buffer.len;
        if (audio_eof)
            playflags |= AOPLAY_FINAL_CHUNK;
    }
    playsize -= playsize % unitsize;
    if (!playsize)
        return partial_fill && audio_eof ? -2 : -partial_fill;

    // play audio:

    int played = write_to_ao(mpctx, ao->buffer.start, playsize, playflags,
                             written_audio_pts(mpctx));
    assert(played % unitsize == 0);
    ao->buffer_playable_size = playsize - played;

    if (played > 0) {
        ao->buffer.len -= played;
        memmove(ao->buffer.start, ao->buffer.start + played, ao->buffer.len);
    } else if (!mpctx->paused && audio_eof && ao_get_delay(ao) < .04) {
        // Sanity check to avoid hanging in case current ao doesn't output
        // partial chunks and doesn't check for AOPLAY_FINAL_CHUNK
        return -2;
    }

    return -partial_fill;
}

static void vo_update_window_title(struct MPContext *mpctx)
{
    if (!mpctx->video_out)
        return;
    char *title = mp_property_expand_string(mpctx, mpctx->opts.wintitle);
    talloc_free(mpctx->video_out->window_title);
    mpctx->video_out->window_title = talloc_steal(mpctx, title);
}

static void update_fps(struct MPContext *mpctx)
{
#ifdef CONFIG_ENCODING
    struct sh_video *sh_video = mpctx->sh_video;
    if (mpctx->encode_lavc_ctx && sh_video)
        encode_lavc_set_video_fps(mpctx->encode_lavc_ctx, sh_video->fps);
#endif
}

int reinit_video_chain(struct MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    assert(!(mpctx->initialized_flags & INITIALIZED_VCODEC));
    init_demux_stream(mpctx, STREAM_VIDEO);
    sh_video_t * const sh_video = mpctx->sh_video;
    if (!sh_video) {
        uninit_player(mpctx, INITIALIZED_VO);
        goto no_video;
    }

    if (!video_read_properties(mpctx->sh_video)) {
        mp_tmsg(MSGT_CPLAYER, MSGL_ERR, "Video: Cannot read properties.\n");
        goto err_out;
    } else {
        mp_tmsg(MSGT_CPLAYER, MSGL_V, "[V] filefmt:%d  fourcc:0x%X  "
                "size:%dx%d  fps:%5.3f  ftime:=%6.4f\n",
                mpctx->master_demuxer->file_format, mpctx->sh_video->format,
                mpctx->sh_video->disp_w, mpctx->sh_video->disp_h,
                mpctx->sh_video->fps, mpctx->sh_video->frametime);
        if (opts->force_fps) {
            mpctx->sh_video->fps = opts->force_fps;
            mpctx->sh_video->frametime = 1.0f / mpctx->sh_video->fps;
        }
        update_fps(mpctx);

        if (!mpctx->sh_video->fps && !opts->force_fps && !opts->correct_pts) {
            mp_tmsg(MSGT_CPLAYER, MSGL_ERR, "FPS not specified in the "
                    "header or invalid, use the -fps option.\n");
        }
    }

    double ar = -1.0;
    //================== Init VIDEO (codec & libvo) ==========================
    if (!opts->fixed_vo || !(mpctx->initialized_flags & INITIALIZED_VO)) {
        mpctx->video_out
            = init_best_video_out(&opts->vo, mpctx->key_fifo, mpctx->input,
                                  mpctx->encode_lavc_ctx);
        if (!mpctx->video_out) {
            mp_tmsg(MSGT_CPLAYER, MSGL_FATAL, "Error opening/initializing "
                    "the selected video_out (-vo) device.\n");
            goto err_out;
        }
        mpctx->initialized_flags |= INITIALIZED_VO;
    }

    vo_update_window_title(mpctx);

    if (stream_control(mpctx->sh_video->ds->demuxer->stream,
                       STREAM_CTRL_GET_ASPECT_RATIO, &ar) != STREAM_UNSUPPORTED)
        mpctx->sh_video->stream_aspect = ar;

    {
        char *vf_arg[] = {
            "_oldargs_", (char *)mpctx->video_out, NULL
        };
        sh_video->vfilter = vf_open_filter(opts, NULL, "vo", vf_arg);
    }

    sh_video->vfilter = append_filters(sh_video->vfilter, opts->vf_settings);

    struct vf_instance *vf = sh_video->vfilter;
    mpctx->osd->render_subs_in_filter
        = vf->control(vf, VFCTRL_INIT_OSD, NULL) == VO_TRUE;

    init_best_video_codec(sh_video, opts->video_decoders);

    if (!sh_video->initialized)
        goto err_out;

    mpctx->initialized_flags |= INITIALIZED_VCODEC;

    sh_video->last_pts = MP_NOPTS_VALUE;
    sh_video->num_buffered_pts = 0;
    sh_video->next_frame_time = 0;
    mpctx->restart_playback = true;
    mpctx->delay = 0;

    // ========== Init display (sh_video->disp_w*sh_video->disp_h/out_fmt) ============

    return 1;

err_out:
    if (!opts->fixed_vo)
        uninit_player(mpctx, INITIALIZED_VO);
    cleanup_demux_stream(mpctx, STREAM_VIDEO);
no_video:
    mpctx->current_track[STREAM_VIDEO] = NULL;
    mp_tmsg(MSGT_CPLAYER, MSGL_INFO, "Video: no video\n");
    return 0;
}

static bool filter_output_queued_frame(struct MPContext *mpctx)
{
    struct sh_video *sh_video = mpctx->sh_video;
    struct vo *video_out = mpctx->video_out;

    struct mp_image *img = vf_chain_output_queued_frame(sh_video->vfilter);
    if (img)
        vo_queue_image(video_out, img);
    talloc_free(img);

    return !!img;
}

static void filter_video(struct MPContext *mpctx, struct mp_image *frame)
{
    struct sh_video *sh_video = mpctx->sh_video;

    frame->pts = sh_video->pts;
    vf_filter_frame(sh_video->vfilter, frame);
    filter_output_queued_frame(mpctx);
}

static double update_video_nocorrect_pts(struct MPContext *mpctx)
{
    struct sh_video *sh_video = mpctx->sh_video;
    double frame_time = 0;
    struct vo *video_out = mpctx->video_out;
    while (1) {
        // In nocorrect-pts mode there is no way to properly time these frames
        if (vo_get_buffered_frame(video_out, 0) >= 0)
            break;
        if (filter_output_queued_frame(mpctx))
            break;
        unsigned char *packet = NULL;
        frame_time = sh_video->next_frame_time;
        if (mpctx->restart_playback)
            frame_time = 0;
        int in_size = 0;
        while (!in_size)
            in_size = video_read_frame(sh_video, &sh_video->next_frame_time,
                                       &packet, mpctx->opts.force_fps);
        if (in_size < 0)
            return -1;
        sh_video->timer += frame_time;
        if (mpctx->sh_audio)
            mpctx->delay -= frame_time;
        // video_read_frame can change fps (e.g. for ASF video)
        update_fps(mpctx);
        int framedrop_type = check_framedrop(mpctx, frame_time);

        struct demux_packet pkt = {0};
        if (sh_video->ds->current)
            pkt = *sh_video->ds->current;
        pkt.buffer = packet;
        pkt.len = in_size;
        void *decoded_frame = decode_video(sh_video, &pkt, framedrop_type,
                                           sh_video->pts);
        if (decoded_frame) {
            filter_video(mpctx, decoded_frame);
        }
        break;
    }
    return frame_time;
}

static void determine_frame_pts(struct MPContext *mpctx)
{
    struct sh_video *sh_video = mpctx->sh_video;
    struct MPOpts *opts = &mpctx->opts;

    if (opts->user_pts_assoc_mode)
        sh_video->pts_assoc_mode = opts->user_pts_assoc_mode;
    else if (sh_video->pts_assoc_mode == 0) {
        if (mpctx->sh_video->ds->demuxer->timestamp_type == TIMESTAMP_TYPE_PTS
            && sh_video->codec_reordered_pts != MP_NOPTS_VALUE)
            sh_video->pts_assoc_mode = 1;
        else
            sh_video->pts_assoc_mode = 2;
    } else {
        int probcount1 = sh_video->num_reordered_pts_problems;
        int probcount2 = sh_video->num_sorted_pts_problems;
        if (sh_video->pts_assoc_mode == 2) {
            int tmp = probcount1;
            probcount1 = probcount2;
            probcount2 = tmp;
        }
        if (probcount1 >= probcount2 * 1.5 + 2) {
            sh_video->pts_assoc_mode = 3 - sh_video->pts_assoc_mode;
            mp_msg(MSGT_CPLAYER, MSGL_V, "Switching to pts association mode "
                   "%d.\n", sh_video->pts_assoc_mode);
        }
    }
    sh_video->pts = sh_video->pts_assoc_mode == 1 ?
                    sh_video->codec_reordered_pts : sh_video->sorted_pts;
}

static double update_video(struct MPContext *mpctx)
{
    struct sh_video *sh_video = mpctx->sh_video;
    struct vo *video_out = mpctx->video_out;
    sh_video->vfilter->control(sh_video->vfilter, VFCTRL_SET_OSD_OBJ,
                               mpctx->osd); // for vf_sub
    if (!mpctx->opts.correct_pts)
        return update_video_nocorrect_pts(mpctx);

    double pts;

    while (1) {
        if (vo_get_buffered_frame(video_out, false) >= 0)
            break;
        if (filter_output_queued_frame(mpctx))
            break;
        pts = MP_NOPTS_VALUE;
        struct demux_packet *pkt;
        while (1) {
            pkt = ds_get_packet2(mpctx->sh_video->ds, false);
            if (!pkt || pkt->len)
                break;
            /* Packets with size 0 are assumed to not correspond to frames,
             * but to indicate the absence of a frame in formats like AVI
             * that must have packets at fixed timecode intervals. */
        }
        if (pkt)
            pts = pkt->pts;
        if (pts != MP_NOPTS_VALUE)
            pts += mpctx->video_offset;
        if (pts >= mpctx->hrseek_pts - .005)
            mpctx->hrseek_framedrop = false;
        int framedrop_type = mpctx->hrseek_framedrop ? 1 :
                             check_framedrop(mpctx, sh_video->frametime);
        struct mp_image *decoded_frame =
            decode_video(sh_video, pkt, framedrop_type, pts);
        if (decoded_frame) {
            determine_frame_pts(mpctx);
            filter_video(mpctx, decoded_frame);
        } else if (!pkt) {
            if (vo_get_buffered_frame(video_out, true) < 0)
                return -1;
        }
        break;
    }

    if (!video_out->frame_loaded)
        return 0;

    pts = video_out->next_pts;
    if (pts == MP_NOPTS_VALUE) {
        mp_msg(MSGT_CPLAYER, MSGL_ERR, "Video pts after filters MISSING\n");
        // Try to use decoder pts from before filters
        pts = sh_video->pts;
        if (pts == MP_NOPTS_VALUE)
            pts = sh_video->last_pts;
    }
    if (mpctx->hrseek_active && pts < mpctx->hrseek_pts - .005) {
        vo_skip_frame(video_out);
        return 0;
    }
    mpctx->hrseek_active = false;
    sh_video->pts = pts;
    if (sh_video->last_pts == MP_NOPTS_VALUE)
        sh_video->last_pts = sh_video->pts;
    else if (sh_video->last_pts > sh_video->pts) {
        mp_msg(MSGT_CPLAYER, MSGL_INFO, "Decreasing video pts: %f < %f\n",
               sh_video->pts, sh_video->last_pts);
        /* If the difference in pts is small treat it as jitter around the
         * right value (possibly caused by incorrect timestamp ordering) and
         * just show this frame immediately after the last one.
         * Treat bigger differences as timestamp resets and start counting
         * timing of later frames from the position of this one. */
        if (sh_video->last_pts - sh_video->pts > 0.5)
            sh_video->last_pts = sh_video->pts;
        else
            sh_video->pts = sh_video->last_pts;
    }
    double frame_time = sh_video->pts - sh_video->last_pts;
    sh_video->last_pts = sh_video->pts;
    sh_video->timer += frame_time;
    if (mpctx->sh_audio)
        mpctx->delay -= frame_time;
    return frame_time;
}

void pause_player(struct MPContext *mpctx)
{
    if (mpctx->paused)
        return;
    mpctx->paused = 1;
    mpctx->step_frames = 0;
    mpctx->time_frame -= get_relative_time(mpctx);
    mpctx->osd_function = 0;

    if (mpctx->video_out && mpctx->sh_video && mpctx->video_out->config_ok)
        vo_control(mpctx->video_out, VOCTRL_PAUSE, NULL);

    if (mpctx->ao && mpctx->sh_audio)
        ao_pause(mpctx->ao);    // pause audio, keep data if possible

    // Only print status if there's actually a file being played.
    if (mpctx->num_sources)
        print_status(mpctx);

    if (!mpctx->opts.quiet)
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_PAUSED\n");
}

void unpause_player(struct MPContext *mpctx)
{
    if (!mpctx->paused)
        return;
    mpctx->paused = 0;
    mpctx->osd_function = 0;
    mpctx->paused_for_cache = false;

    if (mpctx->ao && mpctx->sh_audio)
        ao_resume(mpctx->ao);
    if (mpctx->video_out && mpctx->sh_video && mpctx->video_out->config_ok
        && !mpctx->step_frames)
        vo_control(mpctx->video_out, VOCTRL_RESUME, NULL);      // resume video
    (void)get_relative_time(mpctx);     // ignore time that passed during pause
}

static void draw_osd(struct MPContext *mpctx)
{
    struct vo *vo = mpctx->video_out;

    mpctx->osd->vo_pts = mpctx->video_pts;
    vo_draw_osd(vo, mpctx->osd);
}

static bool redraw_osd(struct MPContext *mpctx)
{
    struct vo *vo = mpctx->video_out;
    if (vo_redraw_frame(vo) < 0)
        return false;

    draw_osd(mpctx);

    vo_flip_page(vo, 0, -1);
    return true;
}

void add_step_frame(struct MPContext *mpctx)
{
    mpctx->step_frames++;
    if (mpctx->video_out && mpctx->sh_video && mpctx->video_out->config_ok)
        vo_control(mpctx->video_out, VOCTRL_PAUSE, NULL);
    unpause_player(mpctx);
}

static void seek_reset(struct MPContext *mpctx, bool reset_ao, bool reset_ac)
{
    if (mpctx->sh_video) {
        resync_video_stream(mpctx->sh_video);
        mpctx->sh_video->timer = 0;
        vo_seek_reset(mpctx->video_out);
        if (mpctx->sh_video->vf_initialized == 1)
            vf_chain_seek_reset(mpctx->sh_video->vfilter);
        mpctx->sh_video->timer = 0;
        mpctx->sh_video->num_buffered_pts = 0;
        mpctx->sh_video->last_pts = MP_NOPTS_VALUE;
        mpctx->delay = 0;
        mpctx->time_frame = 0;
        // Not all demuxers set d_video->pts during seek, so this value
        // (which was used by at least vobsub code below) may be completely
        // wrong (probably 0).
        mpctx->sh_video->pts = mpctx->sh_video->ds->pts + mpctx->video_offset;
        mpctx->video_pts = mpctx->sh_video->pts;
    }

    if (mpctx->sh_audio && reset_ac) {
        resync_audio_stream(mpctx->sh_audio);
        if (reset_ao)
            ao_reset(mpctx->ao);
        mpctx->ao->buffer.len = mpctx->ao->buffer_playable_size;
        mpctx->sh_audio->a_buffer_len = 0;
    }

    reset_subtitles(mpctx);

    mpctx->restart_playback = true;
    mpctx->hrseek_active = false;
    mpctx->hrseek_framedrop = false;
    mpctx->total_avsync_change = 0;
    mpctx->drop_frame_cnt = 0;
    mpctx->dropped_frames = 0;
    mpctx->playback_pts = MP_NOPTS_VALUE;

#ifdef CONFIG_ENCODING
    encode_lavc_discontinuity(mpctx->encode_lavc_ctx);
#endif
}

static bool timeline_set_part(struct MPContext *mpctx, int i, bool force)
{
    struct timeline_part *p = mpctx->timeline + mpctx->timeline_part;
    struct timeline_part *n = mpctx->timeline + i;
    mpctx->timeline_part = i;
    mpctx->video_offset = n->start - n->source_start;
    if (n->source == p->source && !force)
        return false;
    enum stop_play_reason orig_stop_play = mpctx->stop_play;
    if (!mpctx->sh_video && mpctx->stop_play == KEEP_PLAYING)
        mpctx->stop_play = AT_END_OF_FILE;  // let audio uninit drain data
    uninit_player(mpctx, INITIALIZED_VCODEC | (mpctx->opts.fixed_vo ? 0 : INITIALIZED_VO) | (mpctx->opts.gapless_audio ? 0 : INITIALIZED_AO) | INITIALIZED_ACODEC | INITIALIZED_SUB);
    mpctx->stop_play = orig_stop_play;

    mpctx->demuxer = n->source;
    mpctx->stream = mpctx->demuxer->stream;

    // While another timeline was active, the selection of active tracks might
    // have been changed - possibly we need to update this source.
    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *track = mpctx->tracks[n];
        if (track->under_timeline) {
            track->demuxer = mpctx->demuxer;
            track->stream = demuxer_stream_by_demuxer_id(track->demuxer,
                                                         track->type,
                                                         track->demuxer_id);
        }
    }
    preselect_demux_streams(mpctx);

    return true;
}

// Given pts, switch playback to the corresponding part.
// Return offset within that part.
static double timeline_set_from_time(struct MPContext *mpctx, double pts,
                                     bool *need_reset)
{
    if (pts < 0)
        pts = 0;
    for (int i = 0; i < mpctx->num_timeline_parts; i++) {
        struct timeline_part *p = mpctx->timeline + i;
        if (pts < (p + 1)->start) {
            *need_reset = timeline_set_part(mpctx, i, false);
            return pts - p->start + p->source_start;
        }
    }
    return -1;
}


// return -1 if seek failed (non-seekable stream?), 0 otherwise
static int seek(MPContext *mpctx, struct seek_params seek,
                bool timeline_fallthrough)
{
    struct MPOpts *opts = &mpctx->opts;

    if (!mpctx->demuxer)
        return -1;

    if (mpctx->stop_play == AT_END_OF_FILE)
        mpctx->stop_play = KEEP_PLAYING;
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
            // Clear audio from current position
            if (mpctx->sh_audio && !timeline_fallthrough) {
                ao_reset(mpctx->ao);
                mpctx->sh_audio->a_buffer_len = 0;
            }
            return -1;
        }
    }
    if (need_reset) {
        reinit_video_chain(mpctx);
        reinit_subs(mpctx);
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
        demuxer_amount -= opts->hr_seek_demuxer_offset;
    int seekresult = demux_seek(mpctx->demuxer, demuxer_amount,
                                mpctx->audio_delay, demuxer_style);
    if (seekresult == 0) {
        if (need_reset) {
            reinit_audio_chain(mpctx);
            seek_reset(mpctx, !timeline_fallthrough, false);
        }
        return -1;
    }

    // If audio or demuxer subs come from different files, seek them too:
    bool have_external_tracks = false;
    for (int type = 0; type < STREAM_TYPE_COUNT; type++) {
        struct track *track = mpctx->current_track[type];
        have_external_tracks |= track && track->is_external && track->demuxer;
    }
    if (have_external_tracks) {
        double main_new_pos;
        if (seek.type == MPSEEK_ABSOLUTE) {
            main_new_pos = seek.amount - mpctx->video_offset;
        } else {
            main_new_pos = get_main_demux_pts(mpctx);
        }
        for (int type = 0; type < STREAM_TYPE_COUNT; type++) {
            struct track *track = mpctx->current_track[type];
            if (track && track->is_external && track->demuxer)
                demux_seek(track->demuxer, main_new_pos, mpctx->audio_delay,
                           SEEK_ABSOLUTE);
        }
    }

    if (need_reset)
        reinit_audio_chain(mpctx);
    /* If we just reinitialized audio it doesn't need to be reset,
     * and resetting could lose audio some decoders produce during init. */
    seek_reset(mpctx, !timeline_fallthrough, !need_reset);

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
        mpctx->hrseek_framedrop = true;
        mpctx->hrseek_pts = hr_seek ? seek.amount
                                 : mpctx->timeline[mpctx->timeline_part].start;
    }

    mpctx->start_timestamp = GetTimerMS();

    return 0;
}

void queue_seek(struct MPContext *mpctx, enum seek_type type, double amount,
                int exact)
{
    struct seek_params *seek = &mpctx->seek;
    switch (type) {
    case MPSEEK_RELATIVE:
        if (seek->type == MPSEEK_FACTOR)
            return;  // Well... not common enough to bother doing better
        seek->amount += amount;
        seek->exact = FFMAX(seek->exact, exact);
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
        };
        return;
    case MPSEEK_NONE:
        *seek = (struct seek_params){ 0 };
        return;
    }
    abort();
}

double get_time_length(struct MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return 0;

    if (mpctx->timeline)
        return mpctx->timeline[mpctx->num_timeline_parts].start;

    double get_time_ans;
    // <= 0 means DEMUXER_CTRL_NOTIMPL or DEMUXER_CTRL_DONTKNOW
    if (demux_control(demuxer, DEMUXER_CTRL_GET_TIME_LENGTH,
                      (void *) &get_time_ans) > 0)
        return get_time_ans;

    struct sh_video *sh_video = mpctx->sh_video;
    struct sh_audio *sh_audio = mpctx->sh_audio;
    if (sh_video && sh_video->i_bps && sh_audio && sh_audio->i_bps)
        return (double) (demuxer->movi_end - demuxer->movi_start) /
               (sh_video->i_bps + sh_audio->i_bps);
    if (sh_video && sh_video->i_bps)
        return (double) (demuxer->movi_end - demuxer->movi_start) /
               sh_video->i_bps;
    if (sh_audio && sh_audio->i_bps)
        return (double) (demuxer->movi_end - demuxer->movi_start) /
               sh_audio->i_bps;
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

double get_start_time(struct MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return 0;
    double time = 0;
    demux_control(demuxer, DEMUXER_CTRL_GET_START_TIME, &time);
    return time;
}

// Return playback position in 0.0-1.0 ratio, or -1 if unknown.
double get_current_pos_ratio(struct MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return -1;
    double ans = -1;
    double start = get_start_time(mpctx);
    double len = get_time_length(mpctx);
    double pos = get_current_time(mpctx);
    if (len > 0 && !demuxer->ts_resets_possible) {
        ans = av_clipf((pos - start) / len, 0, 1);
    } else {
        int len = (demuxer->movi_end - demuxer->movi_start);
        int64_t pos = demuxer->filepos > 0 ?
                      demuxer->filepos : stream_tell(demuxer->stream);
        if (len > 0)
            ans = av_clipf((double)(pos - demuxer->movi_start) / len, 0, 1);
    }
    return ans;
}

int get_percent_pos(struct MPContext *mpctx)
{
    return av_clip(get_current_pos_ratio(mpctx) * 100, 0, 100);
}

// -2 is no chapters, -1 is before first chapter
int get_current_chapter(struct MPContext *mpctx)
{
    double current_pts = get_current_time(mpctx);
    if (mpctx->chapters) {
        int i;
        for (i = 1; i < mpctx->num_chapters; i++)
            if (current_pts < mpctx->chapters[i].start)
                break;
        return FFMAX(mpctx->last_chapter_seek, i - 1);
    }
    if (mpctx->master_demuxer)
        return FFMAX(mpctx->last_chapter_seek,
                demuxer_get_current_chapter(mpctx->master_demuxer, current_pts));
    return -2;
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
    if (mpctx->chapters) {
        if (chapter < 0 || chapter >= mpctx->num_chapters)
            return NULL;
        return talloc_strdup(NULL, mpctx->chapters[chapter].name);
    }
    if (mpctx->master_demuxer)
        return demuxer_chapter_name(mpctx->master_demuxer, chapter);
    return NULL;
}

// returns the start of the chapter in seconds (-1 if unavailable)
double chapter_start_time(struct MPContext *mpctx, int chapter)
{
    if (mpctx->chapters)
        return mpctx->chapters[chapter].start;
    if (mpctx->master_demuxer)
        return demuxer_chapter_time(mpctx->master_demuxer, chapter, NULL);
    return -1;
}

int get_chapter_count(struct MPContext *mpctx)
{
    if (mpctx->chapters)
        return mpctx->num_chapters;
    if (mpctx->master_demuxer)
        return demuxer_chapter_count(mpctx->master_demuxer);
    return 0;
}

int seek_chapter(struct MPContext *mpctx, int chapter, double *seek_pts)
{
    mpctx->last_chapter_seek = -2;
    if (mpctx->chapters) {
        if (chapter >= mpctx->num_chapters)
            return -1;
        if (chapter < 0)
            chapter = 0;
        *seek_pts = mpctx->chapters[chapter].start;
        mpctx->last_chapter_seek = chapter;
        mpctx->last_chapter_pts = *seek_pts;
        return chapter;
    }

    if (mpctx->master_demuxer) {
        int res = demuxer_seek_chapter(mpctx->master_demuxer, chapter, seek_pts);
        if (res >= 0) {
            if (*seek_pts == -1)
                seek_reset(mpctx, true, true);  // for DVD
            else {
                mpctx->last_chapter_seek = res;
                mpctx->last_chapter_pts = *seek_pts;
            }
        }
        return res;
    }
    return -1;
}

static void update_avsync(struct MPContext *mpctx)
{
    if (!mpctx->sh_audio || !mpctx->sh_video)
        return;

    double a_pos = playing_audio_pts(mpctx);

    mpctx->last_av_difference = a_pos - mpctx->video_pts - mpctx->audio_delay;
    if (mpctx->time_frame > 0)
        mpctx->last_av_difference +=
                mpctx->time_frame * mpctx->opts.playback_speed;
    if (a_pos == MP_NOPTS_VALUE || mpctx->video_pts == MP_NOPTS_VALUE)
        mpctx->last_av_difference = MP_NOPTS_VALUE;
    if (mpctx->last_av_difference > 0.5 && mpctx->drop_frame_cnt > 50
        && !mpctx->drop_message_shown) {
        mp_tmsg(MSGT_AVSYNC, MSGL_WARN, "%s", mp_gtext(av_desync_help_text));
        mpctx->drop_message_shown = true;
    }
}

static void handle_pause_on_low_cache(struct MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    int cache = mp_get_cache_percent(mpctx);
    bool idle = mp_get_cache_idle(mpctx);
    if (mpctx->paused && mpctx->paused_for_cache) {
        if (cache < 0 || cache >= opts->stream_cache_min_percent || idle)
            unpause_player(mpctx);
    } else if (!mpctx->paused) {
        if (cache >= 0 && cache <= opts->stream_cache_pause && !idle) {
            mpctx->paused_for_cache = true;
            pause_player(mpctx);
        }
    }
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

#ifndef HAVE_POSIX_SELECT
    // No proper file descriptor event handling; keep waking up to poll input
    sleeptime = FFMIN(sleeptime, 0.02);
#endif

    if (mpctx->video_out)
        if (mpctx->video_out->wakeup_period > 0)
            sleeptime = FFMIN(sleeptime, mpctx->video_out->wakeup_period);

    return sleeptime;
}

static void run_playloop(struct MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    bool full_audio_buffers = false;
    bool audio_left = false, video_left = false;
    double endpts = get_play_end_pts(mpctx);
    bool end_is_chapter = false;
    double sleeptime = get_wakeup_period(mpctx);
    bool was_restart = mpctx->restart_playback;
    bool new_frame_shown = false;

#ifdef CONFIG_ENCODING
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
        int cur_chapter = get_current_chapter(mpctx);
        if (cur_chapter != -1 && cur_chapter + 1 > opts->chapterrange[1])
            mpctx->stop_play = PT_NEXT_ENTRY;
    }

    if (mpctx->sh_audio && !mpctx->restart_playback && !mpctx->ao->untimed) {
        int status = fill_audio_out_buffers(mpctx, endpts);
        full_audio_buffers = status >= 0;
        // Not at audio stream EOF yet
        audio_left = status > -2;
    }

    double buffered_audio = -1;
    while (mpctx->sh_video) {   // never loops, for "break;" only
        struct vo *vo = mpctx->video_out;
        update_fps(mpctx);

        video_left = vo->hasframe || vo->frame_loaded;
        if (!vo->frame_loaded && (!mpctx->paused || mpctx->restart_playback)) {
            double frame_time = update_video(mpctx);
            mp_dbg(MSGT_AVSYNC, MSGL_DBG2, "*** ftime=%5.3f ***\n", frame_time);
            if (mpctx->sh_video->vf_initialized < 0) {
                mp_tmsg(MSGT_CPLAYER, MSGL_FATAL,
                        "\nFATAL: Could not initialize video filters (-vf) "
                        "or video output (-vo).\n");
                uninit_player(mpctx, INITIALIZED_VCODEC | INITIALIZED_VO);
                cleanup_demux_stream(mpctx, STREAM_VIDEO);
                mpctx->current_track[STREAM_VIDEO] = NULL;
                if (!mpctx->current_track[STREAM_AUDIO])
                    mpctx->stop_play = PT_NEXT_ENTRY;
                break;
            }
            video_left = frame_time >= 0;
            if (video_left && !mpctx->restart_playback) {
                mpctx->time_frame += frame_time / opts->playback_speed;
                adjust_sync(mpctx, frame_time);
            }
            if (!video_left) {
                mpctx->delay = 0;
                mpctx->last_av_difference = 0;
            }
        }

        if (endpts != MP_NOPTS_VALUE)
            video_left &= mpctx->sh_video->pts < endpts;

        // ================================================================
        vo_check_events(vo);

        if (opts->heartbeat_cmd) {
            unsigned now = GetTimerMS();
            if (now - mpctx->last_heartbeat >
                (unsigned)(opts->heartbeat_interval * 1000))
            {
                mpctx->last_heartbeat = now;
                system(opts->heartbeat_cmd);
            }
        }

        if (!video_left || (mpctx->paused && !mpctx->restart_playback))
            break;
        if (!vo->frame_loaded) {
            sleeptime = 0;
            break;
        }

        mpctx->time_frame -= get_relative_time(mpctx);
        if (full_audio_buffers && !mpctx->restart_playback) {
            buffered_audio = ao_get_delay(mpctx->ao);
            mp_dbg(MSGT_AVSYNC, MSGL_DBG2, "delay=%f\n", buffered_audio);

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
            sleeptime = FFMIN(sleeptime, vsleep - 0.040);
            break;
        }
        sleeptime = 0;

        //=================== FLIP PAGE (VIDEO BLT): ======================

        vo_new_frame_imminent(vo);
        struct sh_video *sh_video = mpctx->sh_video;
        mpctx->video_pts = sh_video->pts;
        mpctx->last_vo_pts = mpctx->video_pts;
        mpctx->playback_pts = mpctx->video_pts;
        update_subtitles(mpctx, sh_video->pts);
        update_osd_msg(mpctx);
        draw_osd(mpctx);

        mpctx->time_frame -= get_relative_time(mpctx);
        mpctx->time_frame -= vo->flip_queue_offset;
        if (mpctx->time_frame > 0.001)
            mpctx->time_frame = timing_sleep(mpctx, mpctx->time_frame);
        mpctx->time_frame += vo->flip_queue_offset;

        unsigned int t2 = GetTimer();
        /* Playing with playback speed it's possible to get pathological
         * cases with mpctx->time_frame negative enough to cause an
         * overflow in pts_us calculation, thus the FFMAX. */
        double time_frame = FFMAX(mpctx->time_frame, -1);
        unsigned int pts_us = mpctx->last_time + time_frame * 1e6;
        int duration = -1;
        double pts2 = vo->next_pts2;
        if (pts2 != MP_NOPTS_VALUE && opts->correct_pts &&
                !mpctx->restart_playback) {
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
        }
        vo_flip_page(vo, pts_us | 1, duration);

        mpctx->last_vo_flip_duration = (GetTimer() - t2) * 0.000001;
        if (vo->driver->flip_page_timed) {
            // No need to adjust sync based on flip speed
            mpctx->last_vo_flip_duration = 0;
            // For print_status - VO call finishing early is OK for sync
            mpctx->time_frame -= get_relative_time(mpctx);
        }
        if (mpctx->restart_playback) {
            mpctx->syncing_audio = true;
            if (mpctx->sh_audio)
                fill_audio_out_buffers(mpctx, endpts);
            mpctx->restart_playback = false;
            mpctx->time_frame = 0;
            get_relative_time(mpctx);
        }
        update_avsync(mpctx);
        print_status(mpctx);
        screenshot_flip(mpctx);
        new_frame_shown = true;

        break;
    } // video

    if (mpctx->sh_audio && (mpctx->restart_playback ? !video_left :
                            mpctx->ao->untimed && (mpctx->delay <= 0 ||
                                                   !video_left))) {
        int status = fill_audio_out_buffers(mpctx, endpts);
        full_audio_buffers = status >= 0 && !mpctx->ao->untimed;
        // Not at audio stream EOF yet
        audio_left = status > -2;
    }
    if (!video_left)
        mpctx->restart_playback = false;
    if (mpctx->sh_audio && buffered_audio == -1)
        buffered_audio = mpctx->paused ? 0 : ao_get_delay(mpctx->ao);

    update_osd_msg(mpctx);

    // The cache status is part of the status line. Possibly update it.
    if (mpctx->paused && mp_get_cache_percent(mpctx) >= 0)
        print_status(mpctx);

    if (!video_left && (!mpctx->paused || was_restart)) {
        double a_pos = 0;
        if (mpctx->sh_audio) {
            a_pos = (written_audio_pts(mpctx) -
                     mpctx->opts.playback_speed * buffered_audio);
        }
        mpctx->playback_pts = a_pos;
        print_status(mpctx);

        if (!mpctx->sh_video)
            update_subtitles(mpctx, a_pos);
    }

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
    if ((mpctx->sh_audio || mpctx->sh_video) && !audio_left && !video_left
        && (opts->gapless_audio || buffered_audio < 0.05)
        && (!mpctx->paused || was_restart)) {
        if (end_is_chapter) {
            seek(mpctx, (struct seek_params){
                        .type = MPSEEK_ABSOLUTE,
                        .amount = mpctx->timeline[mpctx->timeline_part+1].start
                        }, true);
        } else
            mpctx->stop_play = AT_END_OF_FILE;
        sleeptime = 0;
    }

    if (!mpctx->stop_play && !mpctx->restart_playback) {

        // If no more video is available, one frame means one playloop iteration.
        // Otherwise, one frame means one video frame.
        if (!video_left)
            new_frame_shown = true;

        if (opts->playing_msg && !mpctx->playing_msg_shown && new_frame_shown) {
            mpctx->playing_msg_shown = true;
            char *msg = mp_property_expand_string(mpctx, opts->playing_msg);
            mp_msg(MSGT_CPLAYER, MSGL_INFO, "%s\n", msg);
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
        double audio_sleep = 9;
        if (mpctx->sh_audio && !mpctx->paused) {
            if (mpctx->ao->untimed) {
                if (!video_left)
                    audio_sleep = 0;
            } else if (full_audio_buffers) {
                audio_sleep = buffered_audio - 0.050;
                // Keep extra safety margin if the buffers are large
                if (audio_sleep > 0.100)
                    audio_sleep = FFMAX(audio_sleep - 0.200, 0.100);
                else
                    audio_sleep = FFMAX(audio_sleep, 0.020);
            } else
                audio_sleep = 0.020;
        }
        sleeptime = FFMIN(sleeptime, audio_sleep);
        if (sleeptime > 0 && mpctx->sh_video) {
            bool want_redraw = vo_get_want_redraw(mpctx->video_out);
            if (mpctx->video_out->driver->draw_osd)
                want_redraw |= mpctx->osd->want_redraw;
            mpctx->osd->want_redraw = false;
            if (want_redraw) {
                if (redraw_osd(mpctx)) {
                    sleeptime = 0;
                } else if (mpctx->paused && video_left) {
                    // force redrawing OSD by framestepping
                    add_step_frame(mpctx);
                    sleeptime = 0;
                }
            }
        }
        if (sleeptime > 0) {
            int sleep_ms = sleeptime * 1000;
            if (mpctx->sh_video) {
                unsigned int vo_sleep = vo_get_sleep_time(mpctx->video_out);
                sleep_ms = FFMIN(sleep_ms, vo_sleep);
            }
            mp_input_get_cmd(mpctx->input, sleep_ms, true);
        }
    }

    //================= Keyboard events, SEEKing ====================

    handle_pause_on_low_cache(mpctx);

    mp_cmd_t *cmd;
    while ((cmd = mp_input_get_cmd(mpctx->input, 0, 1)) != NULL) {
        /* Allow running consecutive seek commands to combine them,
         * but execute the seek before running other commands.
         * If the user seeks continuously (keeps arrow key down)
         * try to finish showing a frame from one location before doing
         * another seek (which could lead to unchanging display). */
        if ((mpctx->seek.type && cmd->id != MP_CMD_SEEK) ||
            (mpctx->restart_playback && cmd->id == MP_CMD_SEEK &&
             GetTimerMS() - mpctx->start_timestamp < 300))
            break;
        cmd = mp_input_get_cmd(mpctx->input, 0, 0);
        run_command(mpctx, cmd);
        mp_cmd_free(cmd);
        if (mpctx->stop_play)
            break;
    }

    // handle -sstep
    if (opts->step_sec > 0 && !mpctx->stop_play && !mpctx->paused &&
        !mpctx->restart_playback)
    {
        set_osd_function(mpctx, OSD_FFW);
        queue_seek(mpctx, MPSEEK_RELATIVE, opts->step_sec, 0);
    }

    if (opts->keep_open && mpctx->stop_play == AT_END_OF_FILE) {
        mpctx->stop_play = KEEP_PLAYING;
        pause_player(mpctx);
        if (mpctx->video_out && !mpctx->video_out->hasframe) {
            // Force screen refresh to make OSD usable
            double seek_to = mpctx->last_vo_pts;
            if (seek_to == MP_NOPTS_VALUE)
                seek_to = 0; // arbitrary default
            queue_seek(mpctx, MPSEEK_ABSOLUTE, seek_to, 1);
        }
    }

    if (mpctx->seek.type) {
        seek(mpctx, mpctx->seek, false);
        mpctx->seek = (struct seek_params){ 0 };
    }
}


static int read_keys(void *ctx, int fd)
{
    if (getch2(ctx))
        return MP_INPUT_NOTHING;
    return MP_INPUT_DEAD;
}

static bool attachment_is_font(struct demux_attachment *att)
{
    if (!att->name || !att->type || !att->data || !att->data_size)
        return false;
    // match against MIME types
    if (strcmp(att->type, "application/x-truetype-font") == 0
        || strcmp(att->type, "application/x-font") == 0)
        return true;
    // fallback: match against file extension
    if (strlen(att->name) > 4) {
        char *ext = att->name + strlen(att->name) - 4;
        if (strcasecmp(ext, ".ttf") == 0 || strcasecmp(ext, ".ttc") == 0
            || strcasecmp(ext, ".otf") == 0)
            return true;
    }
    return false;
}

// Result numerically higher => better match. 0 == no match.
static int match_lang(char **langs, char *lang)
{
    for (int idx = 0; langs && langs[idx]; idx++) {
        if (lang && strcmp(langs[idx], lang) == 0)
            return INT_MAX - idx;
    }
    return 0;
}

/* Get the track wanted by the user.
 * tid is the track ID requested by the user (-2: deselect, -1: default)
 * lang is a string list, NULL is same as empty list
 * Sort tracks based on the following criteria, and pick the first:
 * 0) track matches tid (always wins)
 * 1) track is external
 * 2) earlier match in lang list
 * 3) track is marked default
 * 4) lower track number
 * If select_fallback is not set, 4) is only used to determine whether a
 * matching track is preferred over another track. Otherwise, always pick a
 * track (if nothing else matches, return the track with lowest ID).
 */
// Return whether t1 is preferred over t2
static bool compare_track(struct track *t1, struct track *t2, char **langs)
{
    if (t1->is_external != t2->is_external)
        return t1->is_external;
    int l1 = match_lang(langs, t1->lang), l2 = match_lang(langs, t2->lang);
    if (l1 != l2)
        return l1 > l2;
    if (t1->default_track != t2->default_track)
        return t1->default_track;
    if (t1->attached_picture != t2->attached_picture)
        return !t1->attached_picture;
    return t1->user_tid <= t2->user_tid;
}
static struct track *select_track(struct MPContext *mpctx,
                                  enum stream_type type, int tid, char **langs)
{
    if (tid == -2)
        return NULL;
    bool select_fallback = type == STREAM_VIDEO || type == STREAM_AUDIO;
    struct track *pick = NULL;
    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *track = mpctx->tracks[n];
        if (track->type != type)
            continue;
        if (track->user_tid == tid)
            return track;
        if (!pick || compare_track(track, pick, langs))
            pick = track;
    }
    if (pick && !select_fallback && !pick->is_external
        && !match_lang(langs, pick->lang) && !pick->default_track)
        pick = NULL;
    if (pick && pick->attached_picture && !mpctx->opts.audio_display)
        pick = NULL;
    return pick;
}

// Normally, video/audio/sub track selection is persistent across files. This
// code resets track selection if the new file has a different track layout.
static void check_previous_track_selection(struct MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;

    if (!mpctx->track_layout_hash)
        return;

    char *h = track_layout_hash(mpctx);
    if (strcmp(h, mpctx->track_layout_hash) != 0) {
        // Reset selection, but only if they're not "auto" or "off".
        if (opts->video_id >= 0)
            mpctx->opts.video_id = -1;
        if (opts->audio_id >= 0)
            mpctx->opts.audio_id = -1;
        if (opts->sub_id >= 0)
            mpctx->opts.sub_id = -1;
        talloc_free(mpctx->track_layout_hash);
        mpctx->track_layout_hash = NULL;
    }
    talloc_free(h);
}

static void init_input(struct MPContext *mpctx)
{
    mpctx->input = mp_input_init(&mpctx->opts.input, mpctx->opts.load_config);
    mpctx->key_fifo = mp_fifo_create(mpctx->input, &mpctx->opts);
    if (mpctx->opts.slave_mode)
        mp_input_add_cmd_fd(mpctx->input, 0, USE_FD0_CMD_SELECT, MP_INPUT_SLAVE_CMD_FUNC, NULL);
    else if (mpctx->opts.consolecontrols)
        mp_input_add_key_fd(mpctx->input, 0, 1, read_keys, NULL, mpctx->key_fifo);
    // Set the libstream interrupt callback
    stream_set_interrupt_callback(mp_input_check_interrupt, mpctx->input);
}

static void open_subtitles_from_options(struct MPContext *mpctx)
{
    // after reading video params we should load subtitles because
    // we know fps so now we can adjust subtitle time to ~6 seconds AST
    // check .sub
    double sub_fps = mpctx->sh_video ? mpctx->sh_video->fps : 25;
    if (mpctx->opts.sub_name) {
        for (int i = 0; mpctx->opts.sub_name[i] != NULL; ++i)
            mp_add_subtitles(mpctx, mpctx->opts.sub_name[i], sub_fps, 0);
    }
    if (mpctx->opts.sub_auto) { // auto load sub file ...
        char **tmp = find_text_subtitles(&mpctx->opts, mpctx->filename);
        int nsub = MP_TALLOC_ELEMS(tmp);
        for (int i = 0; i < nsub; i++)
            mp_add_subtitles(mpctx, tmp[i], sub_fps, 1);
        talloc_free(tmp);
    }
}

static struct track *open_external_file(struct MPContext *mpctx, char *filename,
                                        char *demuxer_name, int stream_cache,
                                        enum stream_type filter)
{
    struct MPOpts *opts = &mpctx->opts;
    if (!filename)
        return NULL;
    int format = 0;
    struct stream *stream = open_stream(filename, &mpctx->opts, &format);
    if (!stream)
        goto err_out;
    stream_enable_cache_percent(stream, stream_cache,
                                opts->stream_cache_min_percent,
                                opts->stream_cache_seek_min_percent);
    // deal with broken demuxers: preselect streams
    int vs = -2, as = -2, ss = -2;
    switch (filter) {
    case STREAM_VIDEO: vs = -1; break;
    case STREAM_AUDIO: as = -1; break;
    case STREAM_SUB: ss = -1; break;
    }
    vs = -1; // avi can't go without video
    struct demuxer *demuxer =
        demux_open_withparams(&mpctx->opts, stream, format, demuxer_name,
                              as, vs, ss, filename, NULL);
    if (!demuxer) {
        free_stream(stream);
        goto err_out;
    }
    struct track *first = NULL;
    for (int n = 0; n < demuxer->num_streams; n++) {
        struct sh_stream *stream = demuxer->streams[n];
        if (stream->type == filter) {
            struct track *t = add_stream_track(mpctx, stream, false);
            t->is_external = true;
            t->title = talloc_strdup(t, filename);
            t->external_filename = talloc_strdup(t, filename);
            first = t;
        }
    }
    if (!first) {
        free_demuxer(demuxer);
        mp_msg(MSGT_CPLAYER, MSGL_WARN, "No streams added from file %s.\n",
               filename);
        goto err_out;
    }
    MP_TARRAY_APPEND(NULL, mpctx->sources, mpctx->num_sources, demuxer);
    return first;

err_out:
    mp_msg(MSGT_CPLAYER, MSGL_ERR, "Can not open external file %s.\n",
           filename);
    return false;
}

static void open_audiofiles_from_options(struct MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    open_external_file(mpctx, opts->audio_stream, opts->audio_demuxer_name,
                       opts->audio_stream_cache, STREAM_AUDIO);
}

// Just for -subfile. open_subtitles_from_options handles -sub text sub files.
static void open_subfiles_from_options(struct MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    open_external_file(mpctx, opts->sub_stream, opts->sub_demuxer_name,
                       0, STREAM_SUB);
}

static void print_timeline(struct MPContext *mpctx)
{
    if (mpctx->timeline) {
        int part_count = mpctx->num_timeline_parts;
        mp_msg(MSGT_CPLAYER, MSGL_V, "Timeline contains %d parts from %d "
               "sources. Total length %.3f seconds.\n", part_count,
               mpctx->num_sources, mpctx->timeline[part_count].start);
        mp_msg(MSGT_CPLAYER, MSGL_V, "Source files:\n");
        for (int i = 0; i < mpctx->num_sources; i++)
            mp_msg(MSGT_CPLAYER, MSGL_V, "%d: %s\n", i,
                   mpctx->sources[i]->filename);
        mp_msg(MSGT_CPLAYER, MSGL_V, "Timeline parts: (number, start, "
               "source_start, source):\n");
        for (int i = 0; i < part_count; i++) {
            struct timeline_part *p = mpctx->timeline + i;
            mp_msg(MSGT_CPLAYER, MSGL_V, "%3d %9.3f %9.3f %p/%s\n", i, p->start,
                   p->source_start, p->source, p->source->filename);
        }
        mp_msg(MSGT_CPLAYER, MSGL_V, "END %9.3f\n",
               mpctx->timeline[part_count].start);
    }
}

static void add_subtitle_fonts_from_sources(struct MPContext *mpctx)
{
#ifdef CONFIG_ASS
    if (mpctx->opts.ass_enabled) {
        for (int j = 0; j < mpctx->num_sources; j++) {
            struct demuxer *d = mpctx->sources[j];
            for (int i = 0; i < d->num_attachments; i++) {
                struct demux_attachment *att = d->attachments + i;
                if (mpctx->opts.use_embedded_fonts && attachment_is_font(att))
                    ass_add_font(mpctx->ass_library, att->name, att->data,
                                 att->data_size);
            }
        }
    }

    // libass seems to misbehave if fonts are changed while a renderer
    // exists, so we (re)create the renderer after fonts are set.
    assert(!mpctx->osd->ass_renderer);
    mpctx->osd->ass_renderer = ass_renderer_init(mpctx->osd->ass_library);
    if (mpctx->osd->ass_renderer)
        mp_ass_configure_fonts(mpctx->osd->ass_renderer,
                               mpctx->opts.sub_text_style);
#endif
}

static struct mp_resolve_result *resolve_url(const char *filename,
                                             struct MPOpts *opts)
{
#ifdef CONFIG_LIBQUVI
    return mp_resolve_quvi(filename, opts);
#else
    return NULL;
#endif
}

// Waiting for the slave master to send us a new file to play.
static void idle_loop(struct MPContext *mpctx)
{
    // ================= idle loop (STOP state) =========================
    while (mpctx->opts.player_idle_mode && !mpctx->playlist->current
           && mpctx->stop_play != PT_QUIT)
    {
        uninit_player(mpctx, INITIALIZED_AO | INITIALIZED_VO);
        mp_cmd_t *cmd;
        while (!(cmd = mp_input_get_cmd(mpctx->input,
                                        get_wakeup_period(mpctx) * 1000,
                                        false)));
        run_command(mpctx, cmd);
        mp_cmd_free(cmd);
    }
}

// Start playing the current playlist entry.
// Handle initialization and deinitialization.
static void play_current_file(struct MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;

    mpctx->stop_play = 0;
    mpctx->filename = NULL;

    if (mpctx->playlist->current)
        mpctx->filename = mpctx->playlist->current->filename;

    if (!mpctx->filename)
        goto terminate_playback;

#ifdef CONFIG_ENCODING
    encode_lavc_discontinuity(mpctx->encode_lavc_ctx);
#endif

    mpctx->add_osd_seek_info &= OSD_SEEK_INFO_EDITION;

    m_config_enter_file_local(mpctx->mconfig);

    load_per_protocol_config(mpctx->mconfig, mpctx->filename);
    load_per_extension_config(mpctx->mconfig, mpctx->filename);
    load_per_file_config(mpctx->mconfig, mpctx->filename, opts->use_filedir_conf);

    if (opts->vo.video_driver_list)
        load_per_output_config(mpctx->mconfig, PROFILE_CFG_VO,
                               opts->vo.video_driver_list[0]);
    if (opts->audio_driver_list)
        load_per_output_config(mpctx->mconfig, PROFILE_CFG_AO,
                               opts->audio_driver_list[0]);

    assert(mpctx->playlist->current);
    load_per_file_options(mpctx->mconfig, mpctx->playlist->current->params,
                          mpctx->playlist->current->num_params);

    if (opts->reset_options) {
        for (int n = 0; opts->reset_options[n]; n++) {
            const char *opt = opts->reset_options[n];
            if (strcmp(opt, "all") == 0) {
                m_config_mark_all_file_local(mpctx->mconfig);
            } else {
                m_config_mark_file_local(mpctx->mconfig, opt);
            }
        }
    }

    // We must enable getch2 here to be able to interrupt network connection
    // or cache filling
    if (opts->consolecontrols && !opts->slave_mode) {
        if (mpctx->initialized_flags & INITIALIZED_GETCH2)
            mp_tmsg(MSGT_CPLAYER, MSGL_WARN,
                    "WARNING: getch2_init called twice!\n");
        else
            getch2_enable();  // prepare stdin for hotkeys...
        mpctx->initialized_flags |= INITIALIZED_GETCH2;
        mp_msg(MSGT_CPLAYER, MSGL_DBG2, "\n[[[init getch2]]]\n");
    }

#ifdef CONFIG_ASS
    if (opts->ass_style_override)
        ass_set_style_overrides(mpctx->ass_library, opts->ass_force_style_list);
#endif
    if (mpctx->video_out && mpctx->video_out->config_ok)
        vo_control(mpctx->video_out, VOCTRL_RESUME, NULL);

    mp_tmsg(MSGT_CPLAYER, MSGL_INFO, "Playing %s.\n", mpctx->filename);

    //============ Open & Sync STREAM --- fork cache2 ====================

    assert(mpctx->stream == NULL);
    assert(mpctx->demuxer == NULL);
    assert(mpctx->sh_audio == NULL);
    assert(mpctx->sh_video == NULL);
    assert(mpctx->sh_sub == NULL);

    char *stream_filename = mpctx->filename;
    mpctx->resolve_result = resolve_url(stream_filename, opts);
    if (mpctx->resolve_result)
        stream_filename = mpctx->resolve_result->url;
    int file_format = DEMUXER_TYPE_UNKNOWN;
    mpctx->stream = open_stream(stream_filename, opts, &file_format);
    if (!mpctx->stream) { // error...
        demux_was_interrupted(mpctx);
        goto terminate_playback;
    }
    mpctx->initialized_flags |= INITIALIZED_STREAM;

    if (file_format == DEMUXER_TYPE_PLAYLIST) {
        mp_msg(MSGT_CPLAYER, MSGL_ERR, "\nThis looks like a playlist, but "
               "playlist support will not be used automatically.\n"
               "mpv's playlist code is unsafe and should only be used with "
               "trusted sources.\nPlayback will probably fail.\n\n");
#if 0
        // Handle playlist
        mp_msg(MSGT_CPLAYER, MSGL_WARN, "Parsing playlist %s...\n",
               mpctx->filename);
        bool empty = true;
        struct playlist *pl = playlist_parse(mpctx->stream);
        if (pl) {
            empty = pl->first == NULL;
            playlist_transfer_entries(mpctx->playlist, pl);
            talloc_free(pl);
        }
        if (empty)
            mp_msg(MSGT_CPLAYER, MSGL_ERR, "Playlist was invalid or empty!\n");
        mpctx->stop_play = PT_NEXT_ENTRY;
        goto terminate_playback;
#endif
    }
    mpctx->stream->start_pos += opts->seek_to_byte;

    // CACHE2: initial prefill: 20%  later: 5%  (should be set by -cacheopts)
#ifdef CONFIG_DVBIN
goto_enable_cache: ;
#endif
    int res = stream_enable_cache_percent(mpctx->stream,
                                          opts->stream_cache_size,
                                          opts->stream_cache_min_percent,
                                          opts->stream_cache_seek_min_percent);
    if (res == 0)
        if (demux_was_interrupted(mpctx))
            goto terminate_playback;

    //============ Open DEMUXERS --- DETECT file type =======================

    mpctx->audio_delay = opts->audio_delay;

    mpctx->demuxer = demux_open(opts, mpctx->stream, file_format,
                                opts->audio_id, opts->video_id, opts->sub_id,
                                mpctx->filename);
    mpctx->master_demuxer = mpctx->demuxer;

    if (!mpctx->demuxer) {
        mp_tmsg(MSGT_CPLAYER, MSGL_ERR, "Failed to recognize file format.\n");
        goto terminate_playback;
    }

    if (mpctx->demuxer->matroska_data.ordered_chapters)
        build_ordered_chapter_timeline(mpctx);

    if (mpctx->demuxer->type == DEMUXER_TYPE_EDL)
        build_edl_timeline(mpctx);

    if (mpctx->demuxer->type == DEMUXER_TYPE_CUE)
        build_cue_timeline(mpctx);

    print_timeline(mpctx);

    if (!mpctx->num_sources) {
        MP_TARRAY_APPEND(NULL, mpctx->sources, mpctx->num_sources,
                         mpctx->demuxer);
    }

    if (mpctx->timeline && !mpctx->demuxer->matroska_data.ordered_chapters) {
        // With Matroska, the "master" file dictates track layout etc.
        // On the contrary, the EDL and CUE demuxers are empty wrappers.
        mpctx->demuxer = mpctx->timeline[0].source;
    }
    add_dvd_tracks(mpctx);
    add_demuxer_tracks(mpctx, mpctx->demuxer);

    mpctx->timeline_part = 0;
    if (mpctx->timeline)
        timeline_set_part(mpctx, mpctx->timeline_part, true);

    mpctx->initialized_flags |= INITIALIZED_DEMUXER;

    add_subtitle_fonts_from_sources(mpctx);

    open_subtitles_from_options(mpctx);
    open_audiofiles_from_options(mpctx);
    open_subfiles_from_options(mpctx);

    check_previous_track_selection(mpctx);

    mpctx->current_track[STREAM_VIDEO] =
        select_track(mpctx, STREAM_VIDEO, mpctx->opts.video_id, NULL);
    mpctx->current_track[STREAM_AUDIO] =
        select_track(mpctx, STREAM_AUDIO, mpctx->opts.audio_id,
                     mpctx->opts.audio_lang);
    mpctx->current_track[STREAM_SUB] =
        select_track(mpctx, STREAM_SUB, mpctx->opts.sub_id,
                     mpctx->opts.sub_lang);

    demux_info_print(mpctx->master_demuxer);
    print_file_properties(mpctx, mpctx->filename);

    preselect_demux_streams(mpctx);

#ifdef CONFIG_ENCODING
    if (mpctx->encode_lavc_ctx && mpctx->current_track[STREAM_VIDEO])
        encode_lavc_expect_stream(mpctx->encode_lavc_ctx, AVMEDIA_TYPE_VIDEO);
    if (mpctx->encode_lavc_ctx && mpctx->current_track[STREAM_AUDIO])
        encode_lavc_expect_stream(mpctx->encode_lavc_ctx, AVMEDIA_TYPE_AUDIO);
#endif

    reinit_video_chain(mpctx);
    reinit_audio_chain(mpctx);
    reinit_subs(mpctx);

    //================ SETUP STREAMS ==========================

    if (mpctx->sh_video) {
        mpctx->sh_video->timer = 0;
        if (!opts->ignore_start)
            mpctx->audio_delay += mpctx->sh_video->stream_delay;
    }
    if (mpctx->sh_audio) {
        if (opts->mixer_init_volume >= 0)
            mixer_setvolume(&mpctx->mixer, opts->mixer_init_volume,
                            opts->mixer_init_volume);
        if (opts->mixer_init_mute >= 0)
            mixer_setmute(&mpctx->mixer, opts->mixer_init_mute);
        if (!opts->ignore_start)
            mpctx->audio_delay -= mpctx->sh_audio->stream_delay;
    }

    if (opts->force_fps && mpctx->sh_video) {
        mpctx->sh_video->fps = opts->force_fps;
        mpctx->sh_video->frametime = 1.0f / mpctx->sh_video->fps;
        mp_tmsg(MSGT_CPLAYER, MSGL_INFO,
                "FPS forced to be %5.3f  (ftime: %5.3f).\n",
                mpctx->sh_video->fps, mpctx->sh_video->frametime);
    }

    mp_input_set_section(mpctx->input, NULL, 0);
    //TODO: add desired (stream-based) sections here
    if (mpctx->master_demuxer->type == DEMUXER_TYPE_TV)
        mp_input_set_section(mpctx->input, "tv", 0);
    if (mpctx->encode_lavc_ctx)
        mp_input_set_section(mpctx->input, "encode", MP_INPUT_NO_DEFAULT_SECTION);

    //==================== START PLAYING =======================

    if (!mpctx->sh_video && !mpctx->sh_audio) {
        mp_tmsg(MSGT_CPLAYER, MSGL_FATAL,
                "No video or audio streams selected.\n");
#ifdef CONFIG_DVBIN
        if (mpctx->stream->type == STREAMTYPE_DVB) {
            int dir;
            int v = mpctx->last_dvb_step;
            if (v > 0)
                dir = DVB_CHANNEL_HIGHER;
            else
                dir = DVB_CHANNEL_LOWER;

            if (dvb_step_channel(mpctx->stream, dir)) {
                mpctx->stop_play = PT_NEXT_ENTRY;
                mpctx->dvbin_reopen = 1;
            }
        }
#endif
        goto terminate_playback;
    }

    mp_tmsg(MSGT_CPLAYER, MSGL_V, "Starting playback...\n");

    mpctx->drop_frame_cnt = 0;
    mpctx->dropped_frames = 0;
    mpctx->max_frames = opts->play_frames;

    if (mpctx->max_frames == 0) {
        mpctx->stop_play = PT_NEXT_ENTRY;
        goto terminate_playback;
    }

    mpctx->time_frame = 0;
    mpctx->drop_message_shown = 0;
    mpctx->restart_playback = true;
    mpctx->video_pts = 0;
    mpctx->last_vo_pts = MP_NOPTS_VALUE;
    mpctx->last_seek_pts = 0;
    mpctx->playback_pts = MP_NOPTS_VALUE;
    mpctx->hrseek_active = false;
    mpctx->hrseek_framedrop = false;
    mpctx->step_frames = 0;
    mpctx->total_avsync_change = 0;
    mpctx->last_chapter_seek = -2;
    mpctx->playing_msg_shown = false;

    // If there's a timeline force an absolute seek to initialize state
    double startpos = rel_time_to_abs(mpctx, opts->play_start, -1);
    if (startpos != -1 || mpctx->timeline) {
        queue_seek(mpctx, MPSEEK_ABSOLUTE, startpos, 0);
        seek(mpctx, mpctx->seek, false);
    }
    if (opts->chapterrange[0] > 0) {
        double pts;
        if (seek_chapter(mpctx, opts->chapterrange[0] - 1, &pts) >= 0
            && pts > -1.0) {
            queue_seek(mpctx, MPSEEK_ABSOLUTE, pts, 0);
            seek(mpctx, mpctx->seek, false);
        }
    }

    mpctx->seek = (struct seek_params){ 0 };
    get_relative_time(mpctx); // reset current delta
    // Make sure VO knows current pause state
    if (mpctx->sh_video)
        vo_control(mpctx->video_out,
                   mpctx->paused ? VOCTRL_PAUSE : VOCTRL_RESUME, NULL);

    if (mpctx->opts.start_paused)
        pause_player(mpctx);

    while (!mpctx->stop_play)
        run_playloop(mpctx);

    mp_msg(MSGT_GLOBAL, MSGL_V, "EOF code: %d  \n", mpctx->stop_play);

#ifdef CONFIG_DVBIN
    if (mpctx->dvbin_reopen) {
        mpctx->stop_play = 0;
        uninit_player(mpctx, INITIALIZED_ALL - (INITIALIZED_STREAM | INITIALIZED_GETCH2 | (opts->fixed_vo ? INITIALIZED_VO : 0)));
        cache_uninit(mpctx->stream);
        mpctx->dvbin_reopen = 0;
        goto goto_enable_cache;
    }
#endif

terminate_playback:  // don't jump here after ao/vo/getch initialization!

    if (mpctx->step_frames)
        mpctx->paused = 1;

    mp_msg(MSGT_CPLAYER, MSGL_INFO, "\n");

    // time to uninit all, except global stuff:
    int uninitialize_parts = INITIALIZED_ALL;
    if (opts->fixed_vo)
        uninitialize_parts -= INITIALIZED_VO;
    if ((opts->gapless_audio && mpctx->stop_play == AT_END_OF_FILE) ||
        mpctx->encode_lavc_ctx)
        uninitialize_parts -= INITIALIZED_AO;
    uninit_player(mpctx, uninitialize_parts);

    // xxx handle this as INITIALIZED_CONFIG?
    m_config_leave_file_local(mpctx->mconfig);

    mpctx->filename = NULL;
    talloc_free(mpctx->resolve_result);
    mpctx->resolve_result = NULL;

    vo_sub = NULL;
#ifdef CONFIG_ASS
    if (mpctx->osd->ass_renderer)
        ass_renderer_done(mpctx->osd->ass_renderer);
    mpctx->osd->ass_renderer = NULL;
    ass_clear_fonts(mpctx->ass_library);
#endif
}

// Determine the next file to play. Note that if this function returns non-NULL,
// it can have side-effects and mutate mpctx.
struct playlist_entry *mp_next_file(struct MPContext *mpctx, int direction)
{
    struct playlist_entry *next = playlist_get_next(mpctx->playlist, direction);
    if (!next && mpctx->opts.loop_times >= 0) {
        if (direction > 0) {
            next = mpctx->playlist->first;
            if (next && mpctx->opts.loop_times > 0) {
                mpctx->opts.loop_times--;
                if (mpctx->opts.loop_times == 0)
                    mpctx->opts.loop_times = -1;
            }
        } else {
            next = mpctx->playlist->last;
        }
    }
    return next;
}

// Play all entries on the playlist, starting from the current entry.
// Return if all done.
static void play_files(struct MPContext *mpctx)
{
    for (;;) {
        idle_loop(mpctx);
        if (mpctx->stop_play == PT_QUIT)
            break;

        play_current_file(mpctx);
        if (mpctx->stop_play == PT_QUIT)
            break;

        if (!mpctx->stop_play || mpctx->stop_play == AT_END_OF_FILE)
            mpctx->stop_play = PT_NEXT_ENTRY;

        struct playlist_entry *new_entry = NULL;

        if (mpctx->stop_play == PT_NEXT_ENTRY) {
            new_entry = mp_next_file(mpctx, +1);
        } else if (mpctx->stop_play == PT_CURRENT_ENTRY) {
            new_entry = mpctx->playlist->current;
        } else if (mpctx->stop_play == PT_RESTART) {
            // The same as PT_CURRENT_ENTRY, unless we decide that the current
            // playlist entry can be removed during playback.
            new_entry = mpctx->playlist->current;
        } else { // PT_STOP
            playlist_clear(mpctx->playlist);
        }

        mpctx->playlist->current = new_entry;
        mpctx->playlist->current_was_replaced = false;
        mpctx->stop_play = 0;

        if (!mpctx->playlist->current && !mpctx->opts.player_idle_mode)
            break;
    }
}

static void print_version(int always)
{
    mp_msg(MSGT_CPLAYER, always ? MSGL_INFO : MSGL_V,
           "%s (C) 2000-2013 mpv/MPlayer/mplayer2 projects\n built on %s\n", mplayer_version, mplayer_builddate);
}

static bool handle_help_options(struct MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;
    int opt_exit = 0;
    if (opts->vo.video_driver_list &&
            strcmp(opts->vo.video_driver_list[0], "help") == 0) {
        list_video_out();
        opt_exit = 1;
    }
    if (opts->audio_driver_list &&
            strcmp(opts->audio_driver_list[0], "help") == 0) {
        list_audio_out();
        opt_exit = 1;
    }
    if (opts->audio_decoders && strcmp(opts->audio_decoders, "help") == 0) {
        struct mp_decoder_list *list = mp_audio_decoder_list();
        mp_print_decoders(MSGT_CPLAYER, MSGL_INFO, "Audio decoders:", list);
        talloc_free(list);
        opt_exit = 1;
    }
    if (opts->video_decoders && strcmp(opts->video_decoders, "help") == 0) {
        struct mp_decoder_list *list = mp_video_decoder_list();
        mp_print_decoders(MSGT_CPLAYER, MSGL_INFO, "Video decoders:", list);
        talloc_free(list);
        opt_exit = 1;
    }
    if (af_cfg.list && strcmp(af_cfg.list[0], "help") == 0) {
        af_help();
        mp_msg(MSGT_CPLAYER, MSGL_INFO, "\n");
        opt_exit = 1;
    }
#ifdef CONFIG_X11
    if (opts->vo.fstype_list && strcmp(opts->vo.fstype_list[0], "help") == 0) {
        fstype_help();
        mp_msg(MSGT_FIXME, MSGL_FIXME, "\n");
        opt_exit = 1;
    }
#endif
    if ((opts->demuxer_name && strcmp(opts->demuxer_name, "help") == 0) ||
        (opts->audio_demuxer_name && strcmp(opts->audio_demuxer_name, "help") == 0) ||
        (opts->sub_demuxer_name && strcmp(opts->sub_demuxer_name, "help") == 0)) {
        demuxer_help();
        mp_msg(MSGT_CPLAYER, MSGL_INFO, "\n");
        opt_exit = 1;
    }
    if (opts->list_properties) {
        property_print_help();
        opt_exit = 1;
    }
#ifdef CONFIG_ENCODING
    if (encode_lavc_showhelp(&mpctx->opts))
        opt_exit = 1;
#endif
    return opt_exit;
}

#ifdef PTW32_STATIC_LIB
static void detach_ptw32(void)
{
    pthread_win32_thread_detach_np();
    pthread_win32_process_detach_np();
}
#endif

static void osdep_preinit(int *p_argc, char ***p_argv)
{
    char *enable_talloc = getenv("MPV_LEAK_REPORT");
    if (*p_argc > 1 && (strcmp((*p_argv)[1], "-leak-report") == 0 ||
                        strcmp((*p_argv)[1], "--leak-report") == 0))
        enable_talloc = "1";
    if (enable_talloc && strcmp(enable_talloc, "1") == 0)
        talloc_enable_leak_report();

    GetCpuCaps(&gCpuCaps);

#ifdef __MINGW32__
    mp_get_converted_argv(p_argc, p_argv);
#endif

#ifdef PTW32_STATIC_LIB
    pthread_win32_process_attach_np();
    pthread_win32_thread_attach_np();
    atexit(detach_ptw32);
#endif

    InitTimer();
    srand(GetTimerMS());

#if defined(__MINGW32__) || defined(__CYGWIN__)
    // stop Windows from showing all kinds of annoying error dialogs
    SetErrorMode(0x8003);
    // request 1ms timer resolution
    timeBeginPeriod(1);
#endif

#ifdef HAVE_TERMCAP
    load_termcap(NULL); // load key-codes
#endif
}

/* This preprocessor directive is a hack to generate a mplayer-nomain.o object
 * file for some tools to link against. */
#ifndef DISABLE_MAIN
int main(int argc, char *argv[])
{
    osdep_preinit(&argc, &argv);

    if (argc >= 1) {
        argc--;
        argv++;
    }

    struct MPContext *mpctx = talloc(NULL, MPContext);
    *mpctx = (struct MPContext){
        .last_dvb_step = 1,
        .terminal_osd_text = talloc_strdup(mpctx, ""),
        .playlist = talloc_struct(mpctx, struct playlist, {0}),
    };

    mp_msg_init();
    init_libav();
    screenshot_init(mpctx);

    struct MPOpts *opts = &mpctx->opts;
    set_default_mplayer_options(opts);
    // Create the config context and register the options
    mpctx->mconfig = m_config_new(opts, cfg_include);
    m_config_register_options(mpctx->mconfig, mplayer_opts);
    m_config_register_options(mpctx->mconfig, common_opts);
    mp_input_register_options(mpctx->mconfig);

    // Preparse the command line
    m_config_preparse_command_line(mpctx->mconfig, argc, argv);

    print_version(false);
    print_libav_versions();

    if (!parse_cfgfiles(mpctx, mpctx->mconfig))
        exit_player(mpctx, EXIT_NONE, 1);

    if (!m_config_parse_mp_command_line(mpctx->mconfig, mpctx->playlist,
                                        argc, argv))
    {
        exit_player(mpctx, EXIT_ERROR, 1);
    }

    if (handle_help_options(mpctx))
        exit_player(mpctx, EXIT_NONE, 1);

    mp_msg(MSGT_CPLAYER, MSGL_V, "Configuration: " CONFIGURATION "\n");
    mp_tmsg(MSGT_CPLAYER, MSGL_V, "Command line:");
    for (int i = 0; i < argc; i++)
        mp_msg(MSGT_CPLAYER, MSGL_V, " '%s'", argv[i]);
    mp_msg(MSGT_CPLAYER, MSGL_V, "\n");

    if (!mpctx->playlist->first && !opts->player_idle_mode) {
        print_version(true);
        mp_msg(MSGT_CPLAYER, MSGL_INFO, "%s", mp_gtext(help_text));
        exit_player(mpctx, EXIT_NONE, 0);
    }

#ifdef CONFIG_PRIORITY
    set_priority();
#endif

#ifdef CONFIG_ENCODING
    if (opts->encode_output.file) {
        mpctx->encode_lavc_ctx = encode_lavc_init(&opts->encode_output);
	if(!mpctx->encode_lavc_ctx) {
            mp_msg(MSGT_VO, MSGL_INFO, "Encoding initialization failed.");
            exit_player(mpctx, EXIT_ERROR, 1);
	}
        m_config_set_option0(mpctx->mconfig, "vo", "lavc");
        m_config_set_option0(mpctx->mconfig, "ao", "lavc");
        m_config_set_option0(mpctx->mconfig, "fixed-vo", "yes");
        m_config_set_option0(mpctx->mconfig, "gapless-audio", "yes");
    }
#endif

#ifdef CONFIG_ASS
    mpctx->ass_library = mp_ass_init(opts);
#endif

    mpctx->osd = osd_create(opts, mpctx->ass_library);

    init_input(mpctx);

    mpctx->playlist->current = mpctx->playlist->first;
    play_files(mpctx);

    exit_player(mpctx, mpctx->stop_play == PT_QUIT ? EXIT_QUIT : EXIT_EOF,
                mpctx->quit_player_rc);

    return 1;
}
#endif /* DISABLE_MAIN */
