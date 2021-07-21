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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <locale.h>

#include "config.h"
#include "mpv_talloc.h"

#include "misc/dispatch.h"
#include "misc/thread_pool.h"
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
#include "common/stats.h"
#include "common/global.h"
#include "filters/f_decoder_wrapper.h"
#include "options/parse_configfile.h"
#include "options/parse_commandline.h"
#include "common/playlist.h"
#include "options/options.h"
#include "options/path.h"
#include "input/input.h"

#include "audio/out/ao.h"
#include "misc/thread_tools.h"
#include "sub/osd.h"
#include "test/tests.h"
#include "video/out/vo.h"

#include "core.h"
#include "client.h"
#include "command.h"
#include "screenshot.h"

static const char def_config[] =
#include "generated/etc/builtin.conf.inc"
;

#if HAVE_COCOA
#include "osdep/macosx_events.h"
#endif

#ifndef FULLCONFIG
#define FULLCONFIG "(missing)\n"
#endif

#if !HAVE_STDATOMIC
pthread_mutex_t mp_atomic_mutex = PTHREAD_MUTEX_INITIALIZER;
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
" --h=<string>      print options which contain the given string in their name\n"
"\n";

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

void mp_update_logging(struct MPContext *mpctx, bool preinit)
{
    bool had_log_file = mp_msg_has_log_file(mpctx->global);

    mp_msg_update_msglevels(mpctx->global, mpctx->opts);

    bool enable = mpctx->opts->use_terminal;
    bool enabled = cas_terminal_owner(mpctx, mpctx);
    if (enable != enabled) {
        if (enable && cas_terminal_owner(NULL, mpctx)) {
            terminal_init();
            enabled = true;
        } else if (!enable) {
            terminal_uninit();
            cas_terminal_owner(mpctx, NULL);
        }
    }

    if (mp_msg_has_log_file(mpctx->global) && !had_log_file)
        mp_print_version(mpctx->log, false); // for log-file=... in config files

    if (enabled && !preinit && mpctx->opts->consolecontrols)
        terminal_setup_getch(mpctx->input);
}

void mp_print_version(struct mp_log *log, int always)
{
    int v = always ? MSGL_INFO : MSGL_V;
    mp_msg(log, v, "%s %s\n built on %s\n",
           mpv_version, mpv_copyright, mpv_builddate);
    check_library_versions(log, v);
    mp_msg(log, v, "\n");
    // Only in verbose mode.
    if (!always) {
        mp_msg(log, MSGL_V, "Configuration: " CONFIGURATION "\n");
        mp_msg(log, MSGL_V, "List of enabled features: %s\n", FULLCONFIG);
        #ifdef NDEBUG
            mp_msg(log, MSGL_V, "Built with NDEBUG.\n");
        #endif
    }
}

void mp_destroy(struct MPContext *mpctx)
{
    mp_shutdown_clients(mpctx);

    mp_uninit_ipc(mpctx->ipc_ctx);
    mpctx->ipc_ctx = NULL;

    uninit_audio_out(mpctx);
    uninit_video_out(mpctx);

    // If it's still set here, it's an error.
    encode_lavc_free(mpctx->encode_lavc_ctx);
    mpctx->encode_lavc_ctx = NULL;

    command_uninit(mpctx);

    mp_clients_destroy(mpctx);

    osd_free(mpctx->osd);

#if HAVE_COCOA
    cocoa_set_input_context(NULL);
#endif

    if (cas_terminal_owner(mpctx, mpctx)) {
        terminal_uninit();
        cas_terminal_owner(mpctx, NULL);
    }

    mp_input_uninit(mpctx->input);

    uninit_libav(mpctx->global);

    mp_msg_uninit(mpctx->global);
    assert(!mpctx->num_abort_list);
    talloc_free(mpctx->abort_list);
    pthread_mutex_destroy(&mpctx->abort_lock);
    talloc_free(mpctx->mconfig); // destroy before dispatch
    talloc_free(mpctx);
}

static bool handle_help_options(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct mp_log *log = mpctx->log;
    if (opts->ao_opts->audio_device &&
        strcmp(opts->ao_opts->audio_device, "help") == 0)
    {
        ao_print_devices(mpctx->global, log);
        return true;
    }
    if (opts->property_print_help) {
        property_print_help(mpctx);
        return true;
    }
    if (encode_lavc_showhelp(log, opts->encode_opts))
        return true;
    return false;
}

static int cfg_include(void *ctx, char *filename, int flags)
{
    struct MPContext *mpctx = ctx;
    char *fname = mp_get_user_path(NULL, mpctx->global, filename);
    int r = m_config_parse_config_file(mpctx->mconfig, fname, NULL, flags);
    talloc_free(fname);
    return r;
}

// We mostly care about LC_NUMERIC, and how "." vs. "," is treated,
// Other locale stuff might break too, but probably isn't too bad.
static bool check_locale(void)
{
    char *name = setlocale(LC_NUMERIC, NULL);
    return !name || strcmp(name, "C") == 0 || strcmp(name, "C.UTF-8") == 0;
}

struct MPContext *mp_create(void)
{
    if (!check_locale()) {
        // Normally, we never print anything (except if the "terminal" option
        // is enabled), so this is an exception.
        fprintf(stderr, "Non-C locale detected. This is not supported.\n"
                        "Call 'setlocale(LC_NUMERIC, \"C\");' in your code.\n");
        return NULL;
    }

    char *enable_talloc = getenv("MPV_LEAK_REPORT");
    if (!enable_talloc)
        enable_talloc = HAVE_TA_LEAK_REPORT ? "1" : "0";
    if (strcmp(enable_talloc, "1") == 0)
        talloc_enable_leak_report();

    mp_time_init();

    struct MPContext *mpctx = talloc(NULL, MPContext);
    *mpctx = (struct MPContext){
        .last_chapter = -2,
        .term_osd_contents = talloc_strdup(mpctx, ""),
        .osd_progbar = { .type = -1 },
        .playlist = talloc_zero(mpctx, struct playlist),
        .dispatch = mp_dispatch_create(mpctx),
        .playback_abort = mp_cancel_new(mpctx),
        .thread_pool = mp_thread_pool_create(mpctx, 0, 1, 30),
        .stop_play = PT_NEXT_ENTRY,
        .play_dir = 1,
    };

    pthread_mutex_init(&mpctx->abort_lock, NULL);

    mpctx->global = talloc_zero(mpctx, struct mpv_global);

    stats_global_init(mpctx->global);

    // Nothing must call mp_msg*() and related before this
    mp_msg_init(mpctx->global);
    mpctx->log = mp_log_new(mpctx, mpctx->global->log, "!cplayer");
    mpctx->statusline = mp_log_new(mpctx, mpctx->log, "!statusline");

    mpctx->stats = stats_ctx_create(mpctx, mpctx->global, "main");

    // Create the config context and register the options
    mpctx->mconfig = m_config_new(mpctx, mpctx->log, &mp_opt_root);
    mpctx->opts = mpctx->mconfig->optstruct;
    mpctx->global->config = mpctx->mconfig->shadow;
    mpctx->mconfig->includefunc = cfg_include;
    mpctx->mconfig->includefunc_ctx = mpctx;
    mpctx->mconfig->use_profiles = true;
    mpctx->mconfig->is_toplevel = true;
    mpctx->mconfig->global = mpctx->global;
    m_config_parse(mpctx->mconfig, "", bstr0(def_config), NULL, 0);

    mpctx->input = mp_input_init(mpctx->global, mp_wakeup_core_cb, mpctx);
    screenshot_init(mpctx);
    command_init(mpctx);
    init_libav(mpctx->global);
    mp_clients_init(mpctx);
    mpctx->osd = osd_create(mpctx->global);

#if HAVE_COCOA
    cocoa_set_input_context(mpctx->input);
#endif

    char *verbose_env = getenv("MPV_VERBOSE");
    if (verbose_env)
        mpctx->opts->verbose = atoi(verbose_env);

    mp_cancel_trigger(mpctx->playback_abort);

    return mpctx;
}

// Finish mpctx initialization. This must be done after setting up all options.
// Some of the initializations depend on the options, and can't be changed or
// undone later.
// If options is not NULL, apply them as command line player arguments.
// Returns: 0 on success, -1 on error, 1 if exiting normally (e.g. help).
int mp_initialize(struct MPContext *mpctx, char **options)
{
    struct MPOpts *opts = mpctx->opts;

    assert(!mpctx->initialized);

    // Preparse the command line, so we can init the terminal early.
    if (options) {
        m_config_preparse_command_line(mpctx->mconfig, mpctx->global,
                                       &opts->verbose, options);
    }

    mp_init_paths(mpctx->global, opts);
    mp_msg_set_early_logging(mpctx->global, true);
    mp_update_logging(mpctx, true);

    if (options) {
        MP_VERBOSE(mpctx, "Command line options:");
        for (int i = 0; options[i]; i++)
            MP_VERBOSE(mpctx, " '%s'", options[i]);
        MP_VERBOSE(mpctx, "\n");
    }

    mp_print_version(mpctx->log, false);

    mp_parse_cfgfiles(mpctx);

    if (options) {
        int r = m_config_parse_mp_command_line(mpctx->mconfig, mpctx->playlist,
                                               mpctx->global, options);
        if (r < 0)
            return r == M_OPT_EXIT ? 1 : -1;
    }

    if (opts->operation_mode == 1) {
        m_config_set_profile(mpctx->mconfig, "builtin-pseudo-gui",
                             M_SETOPT_NO_OVERWRITE);
        m_config_set_profile(mpctx->mconfig, "pseudo-gui", 0);
    }

    // Backup the default settings, which should not be stored in the resume
    // config files. This explicitly includes values set by config files and
    // the command line.
    m_config_backup_watch_later_opts(mpctx->mconfig);

    mp_input_load_config(mpctx->input);

    // From this point on, all mpctx members are initialized.
    mpctx->initialized = true;
    mpctx->mconfig->option_change_callback = mp_option_change_callback;
    mpctx->mconfig->option_change_callback_ctx = mpctx;
    m_config_set_update_dispatch_queue(mpctx->mconfig, mpctx->dispatch);
    // Run all update handlers.
    mp_option_change_callback(mpctx, NULL, UPDATE_OPTS_MASK, false);

    if (handle_help_options(mpctx))
        return 1; // help

    check_library_versions(mp_null_log, 0);

#if HAVE_TESTS
    if (opts->test_mode && opts->test_mode[0])
        return run_tests(mpctx) ? 1 : -1;
#endif

    if (!mpctx->playlist->num_entries && !opts->player_idle_mode) {
        // nothing to play
        mp_print_version(mpctx->log, true);
        MP_INFO(mpctx, "%s", mp_help_text);
        return 1;
    }

    MP_STATS(mpctx, "start init");

#if HAVE_COCOA
    mpv_handle *ctx = mp_new_client(mpctx->clients, "osx");
    cocoa_set_mpv_handle(ctx);
#endif

    if (opts->encode_opts->file && opts->encode_opts->file[0]) {
        mpctx->encode_lavc_ctx = encode_lavc_init(mpctx->global);
        if(!mpctx->encode_lavc_ctx) {
            MP_INFO(mpctx, "Encoding initialization failed.\n");
            return -1;
        }
        m_config_set_profile(mpctx->mconfig, "encoding", 0);
        mp_input_enable_section(mpctx->input, "encode", MP_INPUT_EXCLUSIVE);
    }

    mp_load_scripts(mpctx);

    if (opts->force_vo == 2 && handle_force_window(mpctx, false) < 0)
        return -1;

    // Needed to properly enter _initial_ idle mode if playlist empty.
    if (mpctx->opts->player_idle_mode && !mpctx->playlist->num_entries)
        mpctx->stop_play = PT_STOP;

    MP_STATS(mpctx, "end init");

    return 0;
}

int mpv_main(int argc, char *argv[])
{
    struct MPContext *mpctx = mp_create();
    if (!mpctx)
        return 1;

    mpctx->is_cli = true;

    char **options = argv && argv[0] ? argv + 1 : NULL; // skips program name
    int r = mp_initialize(mpctx, options);
    if (r == 0)
        mp_play_files(mpctx);

    int rc = 0;
    const char *reason = NULL;
    if (r < 0) {
        reason = "Fatal error";
        rc = 1;
    } else if (r > 0) {
        // nothing
    } else if (mpctx->stop_play == PT_QUIT) {
        reason = "Quit";
    } else if (mpctx->files_played) {
        if (mpctx->files_errored || mpctx->files_broken) {
            reason = "Some errors happened";
            rc = 3;
        } else {
            reason = "End of file";
        }
    } else if (mpctx->files_broken && !mpctx->files_errored) {
        reason = "Errors when loading file";
        rc = 2;
    } else if (mpctx->files_errored) {
        reason = "Interrupted by error";
        rc = 2;
    } else {
        reason = "No files played";
    }

    if (reason)
        MP_INFO(mpctx, "Exiting... (%s)\n", reason);
    if (mpctx->has_quit_custom_rc)
        rc = mpctx->quit_custom_rc;

    mp_destroy(mpctx);
    return rc;
}
