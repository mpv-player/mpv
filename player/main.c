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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>

#include "config.h"
#include "talloc.h"

#include "misc/dispatch.h"
#include "osdep/io.h"
#include "osdep/terminal.h"
#include "osdep/timer.h"
#include "osdep/main-fn.h"

#include "common/av_log.h"
#include "common/codecs.h"
#include "common/encode.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "options/m_property.h"
#include "common/common.h"
#include "common/msg.h"
#include "common/msg_control.h"
#include "common/global.h"
#include "options/parse_configfile.h"
#include "options/parse_commandline.h"
#include "common/playlist.h"
#include "options/options.h"
#include "options/path.h"
#include "input/input.h"

#include "audio/decode/dec_audio.h"
#include "audio/out/ao.h"
#include "audio/mixer.h"
#include "demux/demux.h"
#include "stream/stream.h"
#include "sub/osd.h"
#include "video/decode/dec_video.h"
#include "video/out/vo.h"

#include "core.h"
#include "client.h"
#include "command.h"
#include "screenshot.h"

#ifdef _WIN32
#include <windows.h>
#endif

#if HAVE_COCOA
#include "osdep/macosx_events.h"
#endif

#ifndef FULLCONFIG
#define FULLCONFIG "(missing)\n"
#endif

enum exit_reason {
  EXIT_NONE,
  EXIT_NORMAL,
  EXIT_ERROR,
};

const char mp_help_text[] =
"Usage:   mpv [options] [url|path/]filename\n"
"\n"
"Basic options:\n"
" --start=<time>    seek to given (percent, seconds, or hh:mm:ss) position\n"
" --no-audio        do not play sound\n"
" --no-video        do not play video\n"
" --fs              fullscreen playback\n"
" --sub-file=<file> specify subtitle file to use\n"
" --playlist=<file> specify playlist file\n"
"\n"
" --list-options    list all mpv options\n"
"\n";

static const char def_config[] =
    "[pseudo-gui]\n"
    "terminal=no\n"
    "force-window=yes\n"
    "idle=once\n";

static pthread_mutex_t terminal_owner_lock = PTHREAD_MUTEX_INITIALIZER;
static struct MPContext *terminal_owner;

static bool cas_terminal_owner(struct MPContext *old, struct MPContext *new)
{
    pthread_mutex_lock(&terminal_owner_lock);
    bool r = terminal_owner == old;
    if (r)
        terminal_owner = new;
    pthread_mutex_unlock(&terminal_owner_lock);
    return r;
}

static void update_logging(struct MPContext *mpctx)
{
    mp_msg_update_msglevels(mpctx->global);
    if (mpctx->opts->use_terminal && cas_terminal_owner(NULL, mpctx))
        terminal_init();
}

void mp_print_version(struct mp_log *log, int always)
{
    int v = always ? MSGL_INFO : MSGL_V;
    mp_msg(log, v,
           "%s (C) 2000-2015 mpv/MPlayer/mplayer2 projects\n built on %s\n",
           mpv_version, mpv_builddate);
    print_libav_versions(log, v);
    mp_msg(log, v, "\n");
    // Only in verbose mode.
    if (!always) {
        mp_msg(log, MSGL_V, "Configuration: " CONFIGURATION "\n");
        mp_msg(log, MSGL_V, "List of enabled features: %s\n", FULLCONFIG);
    }
}

static void shutdown_clients(struct MPContext *mpctx)
{
    while (mpctx->clients && mp_clients_num(mpctx)) {
        mp_client_broadcast_event(mpctx, MPV_EVENT_SHUTDOWN, NULL);
        mp_dispatch_queue_process(mpctx->dispatch, 0);
        mp_wait_events(mpctx, 10000);
    }
}

void mp_destroy(struct MPContext *mpctx)
{
#if !defined(__MINGW32__)
    mp_uninit_ipc(mpctx->ipc_ctx);
    mpctx->ipc_ctx = NULL;
#endif

    shutdown_clients(mpctx);

    uninit_audio_out(mpctx);
    uninit_video_out(mpctx);

#if HAVE_ENCODING
    encode_lavc_finish(mpctx->encode_lavc_ctx);
    encode_lavc_free(mpctx->encode_lavc_ctx);
#endif

    mpctx->encode_lavc_ctx = NULL;

    command_uninit(mpctx);

    mp_clients_destroy(mpctx);

    talloc_free(mpctx->gl_cb_ctx);
    mpctx->gl_cb_ctx = NULL;

    osd_free(mpctx->osd);

#if HAVE_COCOA
    cocoa_set_input_context(NULL);
#endif

    if (cas_terminal_owner(mpctx, mpctx)) {
        terminal_uninit();
        cas_terminal_owner(mpctx, NULL);
    }

    mp_dispatch_set_wakeup_fn(mpctx->dispatch, NULL, NULL);
    mp_input_uninit(mpctx->input);

    uninit_libav(mpctx->global);

    if (mpctx->autodetach)
        pthread_detach(pthread_self());

    mp_msg_uninit(mpctx->global);
    talloc_free(mpctx);
}

static int prepare_exit_cplayer(struct MPContext *mpctx, enum exit_reason how)
{
    int rc = 0;
    const char *reason = NULL;

    if (how == EXIT_ERROR) {
        reason = "Fatal error";
        rc = 1;
    } else if (how == EXIT_NORMAL) {
        if (mpctx->stop_play == PT_QUIT) {
            reason = "Quit";
            rc = 0;
        } else if (mpctx->files_played) {
            if (mpctx->files_errored || mpctx->files_broken) {
                reason = "Some errors happened";
                rc = 3;
            } else {
                reason = "End of file";
                rc = 0;
            }
        } else if (mpctx->files_broken && !mpctx->files_errored) {
            reason = "Errors when loading file";
            rc = 2;
        } else if (mpctx->files_errored) {
            reason = "Interrupted by error";
            rc = 2;
        } else {
            reason = "No files played";
            rc = 0;
        }
    }

    if (reason)
        MP_INFO(mpctx, "\nExiting... (%s)\n", reason);

    if (mpctx->has_quit_custom_rc)
        rc = mpctx->quit_custom_rc;

    mp_destroy(mpctx);
    return rc;
}

static bool handle_help_options(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct mp_log *log = mpctx->log;
    int opt_exit = 0;
    if (opts->audio_decoders && strcmp(opts->audio_decoders, "help") == 0) {
        struct mp_decoder_list *list = audio_decoder_list();
        mp_print_decoders(log, MSGL_INFO, "Audio decoders:", list);
        talloc_free(list);
        opt_exit = 1;
    }
    if (opts->video_decoders && strcmp(opts->video_decoders, "help") == 0) {
        struct mp_decoder_list *list = video_decoder_list();
        mp_print_decoders(log, MSGL_INFO, "Video decoders:", list);
        talloc_free(list);
        opt_exit = 1;
    }
    if ((opts->demuxer_name && strcmp(opts->demuxer_name, "help") == 0) ||
        (opts->audio_demuxer_name && strcmp(opts->audio_demuxer_name, "help") == 0) ||
        (opts->sub_demuxer_name && strcmp(opts->sub_demuxer_name, "help") == 0)) {
        demuxer_help(log);
        MP_INFO(mpctx, "\n");
        opt_exit = 1;
    }
    if (opts->audio_device && strcmp(opts->audio_device, "help") == 0) {
        ao_print_devices(mpctx->global, log);
        opt_exit = 1;
    }
#if HAVE_ENCODING
    if (encode_lavc_showhelp(log, opts->encode_opts))
        opt_exit = 1;
#endif
    return opt_exit;
}

static void osdep_preinit(int argc, char **argv)
{
    char *enable_talloc = getenv("MPV_LEAK_REPORT");
    if (argc > 1 && (strcmp(argv[1], "-leak-report") == 0 ||
                     strcmp(argv[1], "--leak-report") == 0))
        enable_talloc = "1";
    if (enable_talloc && strcmp(enable_talloc, "1") == 0)
        talloc_enable_leak_report();
}

static int cfg_include(void *ctx, char *filename, int flags)
{
    struct MPContext *mpctx = ctx;
    char *fname = mp_get_user_path(NULL, mpctx->global, filename);
    int r = m_config_parse_config_file(mpctx->mconfig, fname, NULL, flags);
    talloc_free(fname);
    return r;
}

struct MPContext *mp_create(void)
{
    mp_time_init();

    struct MPContext *mpctx = talloc(NULL, MPContext);
    *mpctx = (struct MPContext){
        .last_chapter = -2,
        .term_osd_contents = talloc_strdup(mpctx, ""),
        .osd_progbar = { .type = -1 },
        .playlist = talloc_struct(mpctx, struct playlist, {0}),
        .dispatch = mp_dispatch_create(mpctx),
        .playback_abort = mp_cancel_new(mpctx),
    };

    mpctx->global = talloc_zero(mpctx, struct mpv_global);

    // Nothing must call mp_msg*() and related before this
    mp_msg_init(mpctx->global);
    mpctx->log = mp_log_new(mpctx, mpctx->global->log, "!cplayer");
    mpctx->statusline = mp_log_new(mpctx, mpctx->log, "!statusline");

    struct MPOpts *def_opts = talloc_ptrtype(mpctx, def_opts);
    *def_opts = mp_default_opts;
    def_opts->network_useragent = (char *)mpv_version;

    // Create the config context and register the options
    mpctx->mconfig = m_config_new(mpctx, mpctx->log, sizeof(struct MPOpts),
                                  def_opts, mp_opts);
    mpctx->opts = mpctx->mconfig->optstruct;
    mpctx->mconfig->includefunc = cfg_include;
    mpctx->mconfig->includefunc_ctx = mpctx;
    mpctx->mconfig->use_profiles = true;
    mpctx->mconfig->is_toplevel = true;
    m_config_parse(mpctx->mconfig, "", bstr0(def_config), NULL, 0);

    mpctx->global->opts = mpctx->opts;

    mpctx->input = mp_input_init(mpctx->global);
    screenshot_init(mpctx);
    mpctx->mixer = mixer_init(mpctx, mpctx->global);
    command_init(mpctx);
    init_libav(mpctx->global);
    mp_clients_init(mpctx);

    return mpctx;
}

void wakeup_playloop(void *ctx)
{
    struct MPContext *mpctx = ctx;
    mp_input_wakeup(mpctx->input);
}

// Finish mpctx initialization. This must be done after setting up all options.
// Some of the initializations depend on the options, and can't be changed or
// undone later.
// If options is not NULL, apply them as command line player arguments.
// Returns: <0 on error, 0 on success.
int mp_initialize(struct MPContext *mpctx, char **options)
{
    struct MPOpts *opts = mpctx->opts;

    assert(!mpctx->initialized);

    if (options) {
        // Preparse the command line, so we can init the terminal early.
        m_config_preparse_command_line(mpctx->mconfig, mpctx->global, options);

        update_logging(mpctx);

        MP_VERBOSE(mpctx, "Command line options:");
        for (int i = 0; options[i]; i++)
            MP_VERBOSE(mpctx, " '%s'", options[i]);
        MP_VERBOSE(mpctx, "\n");
    }

    update_logging(mpctx);
    mp_print_version(mpctx->log, false);

    mp_parse_cfgfiles(mpctx);
    update_logging(mpctx);

    if (options) {
        int r = m_config_parse_mp_command_line(mpctx->mconfig, mpctx->playlist,
                                               mpctx->global, options);
        if (r < 0)
            return r <= M_OPT_EXIT ? -2 : -1;
        update_logging(mpctx);
    }

    if (handle_help_options(mpctx))
        return -2;

    if (opts->dump_stats && opts->dump_stats[0]) {
        if (mp_msg_open_stats_file(mpctx->global, opts->dump_stats) < 0)
            MP_ERR(mpctx, "Failed to open stats file '%s'\n", opts->dump_stats);
    }
    MP_STATS(mpctx, "start init");

    if (opts->slave_mode) {
        MP_WARN(mpctx, "--slave-broken is deprecated (see manpage).\n");
        opts->consolecontrols = 0;
        m_config_set_option0(mpctx->mconfig, "input-file", "/dev/stdin");
    }

    if (!mpctx->playlist->first && !opts->player_idle_mode)
        return -3;

    mp_input_load(mpctx->input);
    mp_input_set_cancel(mpctx->input, mpctx->playback_abort);

    mp_dispatch_set_wakeup_fn(mpctx->dispatch, wakeup_playloop, mpctx);

#if HAVE_ENCODING
    if (opts->encode_opts->file && opts->encode_opts->file[0]) {
        mpctx->encode_lavc_ctx = encode_lavc_init(opts->encode_opts,
                                                  mpctx->global);
        if(!mpctx->encode_lavc_ctx) {
            MP_INFO(mpctx, "Encoding initialization failed.");
            return -1;
        }
        m_config_set_option0(mpctx->mconfig, "vo", "lavc");
        m_config_set_option0(mpctx->mconfig, "ao", "lavc");
        m_config_set_option0(mpctx->mconfig, "keep-open", "no");
        m_config_set_option0(mpctx->mconfig, "force-window", "no");
        m_config_set_option0(mpctx->mconfig, "gapless-audio", "yes");
        m_config_set_option0(mpctx->mconfig, "resume-playback", "no");
        m_config_set_option0(mpctx->mconfig, "load-scripts", "no");
        m_config_set_option0(mpctx->mconfig, "osc", "no");
        m_config_set_option0(mpctx->mconfig, "framedrop", "no");
        // never use auto
        if (!opts->audio_output_channels.num) {
            m_config_set_option_ext(mpctx->mconfig, bstr0("audio-channels"),
                                    bstr0("stereo"), M_SETOPT_PRESERVE_CMDLINE);
        }
        mp_input_enable_section(mpctx->input, "encode", MP_INPUT_EXCLUSIVE);
    }
#endif

#if !HAVE_LIBASS
    MP_WARN(mpctx, "Compiled without libass.\n");
    MP_WARN(mpctx, "There will be no OSD and no text subtitles.\n");
#endif

    mpctx->osd = osd_create(mpctx->global);

    // From this point on, all mpctx members are initialized.
    mpctx->initialized = true;

    mp_get_resume_defaults(mpctx);

    if (opts->consolecontrols && cas_terminal_owner(mpctx, mpctx))
        terminal_setup_getch(mpctx->input);

#if HAVE_COCOA
    cocoa_set_input_context(mpctx->input);
#endif

    if (opts->force_vo) {
        struct vo_extra ex = {
            .input_ctx = mpctx->input,
            .osd = mpctx->osd,
            .encode_lavc_ctx = mpctx->encode_lavc_ctx,
        };
        mpctx->video_out = init_best_video_out(mpctx->global, &ex);
        if (!mpctx->video_out) {
            MP_FATAL(mpctx, "Error opening/initializing "
                    "the selected video_out (-vo) device.\n");
            return -1;
        }
        mpctx->mouse_cursor_visible = true;
    }

    // Lua user scripts (etc.) can call arbitrary functions. Load them at a point
    // where this is safe.
    mp_load_scripts(mpctx);

#if !defined(__MINGW32__)
    mpctx->ipc_ctx = mp_init_ipc(mpctx->clients, mpctx->global);
#endif

#ifdef _WIN32
    if (opts->w32_priority > 0)
        SetPriorityClass(GetCurrentProcess(), opts->w32_priority);
#endif

    prepare_playlist(mpctx, mpctx->playlist);

    MP_STATS(mpctx, "end init");

    return 0;
}

int mpv_main(int argc, char *argv[])
{
    osdep_preinit(argc, argv);

    struct MPContext *mpctx = mp_create();
    struct MPOpts *opts = mpctx->opts;

    char *verbose_env = getenv("MPV_VERBOSE");
    if (verbose_env)
        opts->verbose = atoi(verbose_env);

    char **options = argv && argv[0] ? argv + 1 : NULL; // skips program name
    int r = mp_initialize(mpctx, options);
    if (r == -2) // help
        return prepare_exit_cplayer(mpctx, EXIT_NONE);
    if (r == -3) { // nothing to play
        mp_print_version(mpctx->log, true);
        MP_INFO(mpctx, "%s", mp_help_text);
        return prepare_exit_cplayer(mpctx, EXIT_NONE);
    }
    if (r < 0) // another error
        return prepare_exit_cplayer(mpctx, EXIT_ERROR);

    mp_play_files(mpctx);
    return prepare_exit_cplayer(mpctx, EXIT_NORMAL);
}
