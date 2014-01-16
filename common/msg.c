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
    struct mpv_global *global;
    // --- protected by mp_msg_lock
    char *msglevels;
    bool smode;         // slave mode compatibility glue
    bool module;
    bool termosd;       // use terminal control codes for status line
    bool header;        // indicate that message header should be printed
    int blank_lines;    // number of lines useable by status
    int status_lines;   // number of current status lines
    bool color;
    int verbose;
    bool force_stderr;
    // --- semi-atomic access
    bool mute;
    // --- must be accessed atomically
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

static const struct mp_log null_log = {0};
struct mp_log *const mp_null_log = (struct mp_log *)&null_log;

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
    log->level = MSGL_STATUS + log->root->verbose; // default log level
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
// Thread-safety: see mp_msg().
bool mp_msg_test(struct mp_log *log, int lev)
{
    mp_memory_barrier();
    if (!log->root || log->root->mute)
        return false;
    if (lev == MSGL_STATUS) {
        // skip status line output if stderr is a tty but in background
        if (terminal_in_background())
            return false;
    }
    if (log->reload_counter != log->root->reload_counter)
        update_loglevel(log);
    return lev <= log->level || (log->root->smode && lev == MSGL_SMODE);
}

// Reposition cursor and clear lines for outputting the status line. In certain
// cases, like term OSD and subtitle display, the status can consist of
// multiple lines.
static void prepare_status_line(struct mp_log_root *root, char *new_status)
{
    FILE *f = stderr;

    size_t new_lines = 1;
    char *tmp = new_status;
    while (1) {
        tmp = strchr(tmp, '\n');
        if (!tmp)
            break;
        new_lines++;
        tmp++;
    }

    size_t old_lines = root->status_lines;
    size_t clear_lines = MPMIN(MPMAX(new_lines, old_lines), root->blank_lines);

    // clear the status line itself
    fprintf(f, "\r%s", terminal_erase_to_end_of_line);
    // and clear all previous old lines
    for (size_t n = 1; n < clear_lines; n++)
        fprintf(f, "%s\r%s", terminal_cursor_up, terminal_erase_to_end_of_line);
    // skip "unused" blank lines, so that status is aligned to term bottom
    for (size_t n = new_lines; n < clear_lines; n++)
        fprintf(f, "\n");

    root->status_lines = new_lines;
    root->blank_lines = MPMAX(root->blank_lines, new_lines);
}

static void flush_status_line(struct mp_log_root *root)
{
    // If there was a status line, don't overwrite it, but skip it.
    if (root->status_lines)
        fprintf(stderr, "\n");
    root->status_lines = 0;
    root->blank_lines = 0;
}

void mp_msg_flush_status_line(struct mpv_global *global)
{
    pthread_mutex_lock(&mp_msg_lock);
    flush_status_line(global->log->root);
    pthread_mutex_unlock(&mp_msg_lock);
}

bool mp_msg_has_status_line(struct mpv_global *global)
{
    pthread_mutex_lock(&mp_msg_lock);
    bool r = global->log->root->status_lines > 0;
    pthread_mutex_unlock(&mp_msg_lock);
    return r;
}

static void set_msg_color(FILE* stream, int lev)
{
    static const int v_colors[] = {9, 1, 3, -1, -1, 2, 8, 8, -1};
    terminal_set_foreground_color(stream, v_colors[lev]);
}

void mp_msg_va(struct mp_log *log, int lev, const char *format, va_list va)
{
    if (!mp_msg_test(log, lev))
        return; // do not display

    pthread_mutex_lock(&mp_msg_lock);

    struct mp_log_root *root = log->root;
    FILE *stream = (root->force_stderr || lev == MSGL_STATUS) ? stderr : stdout;

    char tmp[MSGSIZE_MAX];
    if (vsnprintf(tmp, MSGSIZE_MAX, format, va) < 0)
        snprintf(tmp, MSGSIZE_MAX, "[fprintf error]\n");
    tmp[MSGSIZE_MAX - 2] = '\n';
    tmp[MSGSIZE_MAX - 1] = 0;
    char *text = tmp;

    bool header = root->header;
    const char *prefix = log->prefix;
    char *terminate = NULL;

    if ((lev >= MSGL_V && lev != MSGL_SMODE) || root->verbose || root->module)
        prefix = log->verbose_prefix;

    if (lev == MSGL_STATUS) {
        if (root->termosd) {
            prepare_status_line(root, text);
            terminate = "\r";
        } else {
            terminate = "\n";
        }
        root->header = true;
    } else {
        flush_status_line(root);
        size_t len = strlen(text);
        root->header = len && text[len - 1] == '\n';
    }

    if (root->color)
        set_msg_color(stream, lev);

    do {
        if (header && prefix)
            fprintf(stream, "[%s] ", prefix);

        char *next = strchr(text, '\n');
        int len = next ? next - text + 1 : strlen(text);
        fprintf(stream, "%.*s", len, text);
        text = text + len;

        header = true;
    } while (text[0]);

    if (terminate)
        fprintf(stream, "%s", terminate);

    if (root->color)
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
// Thread-safety: fully thread-safe, but keep in mind that talloc is not (so
//                talloc_ctx must be owned by the current thread).
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
    root->header = true;
    root->reload_counter = 1;

    struct mp_log dummy = { .root = root };
    struct mp_log *log = mp_log_new(root, &dummy, "");

    global->log = log;

    mp_msg_update_msglevels(global);
}

void mp_msg_update_msglevels(struct mpv_global *global)
{
    struct mp_log_root *root = global->log->root;
    struct MPOpts *opts = global->opts;

    if (!opts)
        return;

    pthread_mutex_lock(&mp_msg_lock);

    root->verbose = opts->verbose;
    root->module = opts->msg_module;
    root->smode = opts->msg_identify;
    root->color = opts->msg_color && isatty(fileno(stdout));
    root->termosd = !opts->slave_mode && isatty(fileno(stderr));

    talloc_free(root->msglevels);
    root->msglevels = talloc_strdup(root, global->opts->msglevels);

    mp_atomic_add_and_fetch(&root->reload_counter, 1);
    mp_memory_barrier();
    pthread_mutex_unlock(&mp_msg_lock);
}

void mp_msg_mute(struct mpv_global *global, bool mute)
{
    struct mp_log_root *root = global->log->root;

    root->mute = mute;
}

void mp_msg_force_stderr(struct mpv_global *global, bool force_stderr)
{
    struct mp_log_root *root = global->log->root;

    root->force_stderr = force_stderr;
}

void mp_msg_uninit(struct mpv_global *global)
{
    talloc_free(global->log->root);
    global->log = NULL;
}

// Thread-safety: fully thread-safe, but keep in mind that the lifetime of
//                log must be guaranteed during the call.
//                Never call this from signal handlers.
void mp_msg(struct mp_log *log, int lev, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    mp_msg_va(log, lev, format, va);
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
