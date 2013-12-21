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
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

#include "talloc.h"

#include "bstr/bstr.h"
#include "compat/atomics.h"
#include "common/common.h"
#include "common/global.h"
#include "options/options.h"
#include "osdep/terminal.h"
#include "osdep/io.h"

#include "common/msg.h"

/* maximum message length of mp_msg */
#define MSGSIZE_MAX 6144

struct mp_log_root {
    /* This should, at some point, contain all mp_msg related state, instead
     * of having global variables (at least as long as we don't want to
     * control the terminal, which is global anyway). But for now, there is
     * not much. */
    struct mpv_global *global;
    char *msglevels; // protected by mp_msg_lock
    /* This is incremented every time the msglevels must be reloaded.
     * (This is perhaps better than maintaining a globally accessible and
     * synchronized mp_log tree.) */
    int64_t reload_counter;
};

struct mp_log {
    struct mp_log_root *root;
    const char *prefix;
    const char *verbose_prefix;
    int level;
    int64_t reload_counter;
};

// Protects some (not all) state in mp_log_root
static pthread_mutex_t mp_msg_lock = PTHREAD_MUTEX_INITIALIZER;

bool mp_msg_stdout_in_use;
int verbose;
bool mp_msg_mute;
int mp_smode;
int mp_msg_color = 1;
int mp_msg_module;
int mp_msg_cancolor;

// indicate if last line printed ended with \n or \r
static int header = 1;
// indicates if last line printed was a status line
static int statusline;

static const struct mp_log null_log = {0};
struct mp_log *const mp_null_log = (struct mp_log *)&null_log;

static void mp_msg_do_init(void)
{
    char *env = getenv("MPV_VERBOSE");
    if (env)
        verbose = atoi(env);
    mp_msg_cancolor = isatty(fileno(stdout));
}

static bool match_mod(const char *name, bstr mod)
{
    if (bstr_equals0(mod, "all"))
        return true;
    // Path prefix matches
    bstr b = bstr0(name);
    return bstr_eatstart(&b, mod) && (bstr_eatstart0(&b, "/") || !b.len);
}

static void update_loglevel(struct mp_log *log)
{
    pthread_mutex_lock(&mp_msg_lock);
    log->level = MSGL_STATUS + verbose; // default log level
    // Stupid exception for the remains of -identify
    if (match_mod(log->verbose_prefix, bstr0("identify")))
        log->level = -1;
    bstr s = bstr0(log->root->msglevels);
    bstr mod;
    int level;
    while (mp_msg_split_msglevel(&s, &mod, &level) > 0) {
        if (match_mod(log->verbose_prefix, mod))
            log->level = level;
    }
    log->reload_counter = log->root->reload_counter;
    pthread_mutex_unlock(&mp_msg_lock);
}

// Return whether the message at this verbosity level would be actually printed.
bool mp_msg_test_log(struct mp_log *log, int lev)
{
    if (mp_msg_mute || !log->root)
        return false;
    if (lev == MSGL_STATUS) {
        // skip status line output if stderr is a tty but in background
        if (terminal_in_background())
            return false;
    }
    mp_memory_barrier();
    if (log->reload_counter != log->root->reload_counter)
        update_loglevel(log);
    return lev <= log->level || (mp_smode && lev == MSGL_SMODE);
}

static int mp_msg_docolor(void)
{
    return mp_msg_cancolor && mp_msg_color;
}

static void set_msg_color(FILE* stream, int lev)
{
    static const int v_colors[] = {9, 1, 3, -1, -1, 2, 8, 8, -1};
    if (mp_msg_docolor())
        terminal_set_foreground_color(stream, v_colors[lev]);
}

void mp_msg_log_va(struct mp_log *log, int lev, const char *format, va_list va)
{
    if (!mp_msg_test_log(log, lev))
        return; // do not display

    pthread_mutex_lock(&mp_msg_lock);

    FILE *stream = (mp_msg_stdout_in_use || lev == MSGL_STATUS) ? stderr : stdout;

    char tmp[MSGSIZE_MAX];
    if (vsnprintf(tmp, MSGSIZE_MAX, format, va) < 0)
        snprintf(tmp, MSGSIZE_MAX, "[fprintf error]\n");
    tmp[MSGSIZE_MAX - 2] = '\n';
    tmp[MSGSIZE_MAX - 1] = 0;

    /* A status line is normally intended to be overwritten by the next
     * status line, and does not end with a '\n'. If we're printing a normal
     * line instead after the status one print '\n' to change line. */
    if (statusline && lev != MSGL_STATUS)
        fprintf(stderr, "\n");
    statusline = lev == MSGL_STATUS;

    set_msg_color(stream, lev);
    if (header) {
        if (lev >= MSGL_V || verbose || mp_msg_module) {
            fprintf(stream, "[%s] ", log->verbose_prefix);
        } else if (log->prefix) {
            fprintf(stream, "[%s] ", log->prefix);
        }
    }

    size_t len = strlen(tmp);
    header = len && (tmp[len - 1] == '\n' || tmp[len - 1] == '\r');

    fprintf(stream, "%s", tmp);

    if (mp_msg_docolor())
        terminal_set_foreground_color(stream, -1);
    fflush(stream);

    pthread_mutex_unlock(&mp_msg_lock);
}

// Create a new log context, which uses talloc_ctx as talloc parent, and parent
// as logical parent.
// The name is the prefix put before the output. It's usually prefixed by the
// parent's name. If the name starts with "/", the parent's name is not
// prefixed (except in verbose mode), and if it starts with "!", the name is
// not printed at all (except in verbose mode).
struct mp_log *mp_log_new(void *talloc_ctx, struct mp_log *parent,
                          const char *name)
{
    assert(parent);
    assert(name);
    struct mp_log *log = talloc_zero(talloc_ctx, struct mp_log);
    if (!parent->root)
        return log; // same as null_log
    log->root = parent->root;
    if (name[0] == '!') {
        name = &name[1];
    } else if (name[0] == '/') {
        name = &name[1];
        log->prefix = talloc_strdup(log, name);
    } else {
        log->prefix = parent->prefix
                ? talloc_asprintf(log, "%s/%s", parent->prefix, name)
                : talloc_strdup(log, name);
    }
    log->verbose_prefix = parent->prefix
            ? talloc_asprintf(log, "%s/%s", parent->prefix, name)
            : talloc_strdup(log, name);
    if (log->prefix && !log->prefix[0])
        log->prefix = NULL;
    if (!log->verbose_prefix[0])
        log->verbose_prefix = "global";
    return log;
}

void mp_msg_init(struct mpv_global *global)
{
    assert(!global->log);

    struct mp_log_root *root = talloc_zero(NULL, struct mp_log_root);
    root->global = global;
    root->reload_counter = 1;

    struct mp_log dummy = { .root = root };
    struct mp_log *log = mp_log_new(root, &dummy, "");

    mp_msg_do_init();

    global->log = log;
}

struct mpv_global *mp_log_get_global(struct mp_log *log)
{
    return log->root->global;
}

void mp_msg_update_msglevels(struct mpv_global *global)
{
    struct mp_log_root *root = global->log->root;
    pthread_mutex_lock(&mp_msg_lock);
    talloc_free(root->msglevels);
    root->msglevels = talloc_strdup(root, global->opts->msglevels);
    mp_atomic_add_and_fetch(&root->reload_counter, 1);
    mp_memory_barrier();
    pthread_mutex_unlock(&mp_msg_lock);
}

void mp_msg_uninit(struct mpv_global *global)
{
    talloc_free(global->log->root);
    global->log = NULL;
}

void mp_msg_log(struct mp_log *log, int lev, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    mp_msg_log_va(log, lev, format, va);
    va_end(va);
}

static const char *level_names[] = {
    [MSGL_FATAL]        = "fatal",
    [MSGL_ERR]          = "error",
    [MSGL_WARN]         = "warn",
    [MSGL_INFO]         = "info",
    [MSGL_STATUS]       = "status",
    [MSGL_V]            = "v",
    [MSGL_DEBUG]        = "debug",
    [MSGL_TRACE]        = "trace",
};

int mp_msg_split_msglevel(struct bstr *s, struct bstr *out_mod, int *out_level)
{
    if (s->len == 0)
        return 0;
    bstr elem, rest;
    bstr_split_tok(*s, ":", &elem, &rest);
    bstr mod, level;
    if (!bstr_split_tok(elem, "=", &mod, &level) || mod.len == 0)
        return -1;
    int ilevel = -1;
    for (int n = 0; n < MP_ARRAY_SIZE(level_names); n++) {
        if (level_names[n] && bstr_equals0(level, level_names[n])) {
            ilevel = n;
            break;
        }
    }
    if (ilevel < 0 && !bstr_equals0(level, "no"))
        return -1;
    *s = rest;
    *out_mod = mod;
    *out_level = ilevel;
    return 1;
}
