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
#include <stdint.h>

#include "talloc.h"

#include "bstr/bstr.h"
#include "compat/atomics.h"
#include "common/common.h"
#include "common/global.h"
#include "misc/ring.h"
#include "options/options.h"
#include "osdep/terminal.h"
#include "osdep/io.h"
#include "osdep/timer.h"

#include "msg.h"
#include "msg_control.h"

/* maximum message length of mp_msg */
#define MSGSIZE_MAX 6144

struct mp_log_root {
    struct mpv_global *global;
    // --- protected by mp_msg_lock
    char *msglevels;
    bool use_terminal;  // make accesses to stderr/stdout
    bool module;
    bool show_time;
    bool termosd;       // use terminal control codes for status line
    bool header;        // indicate that message header should be printed
    int blank_lines;    // number of lines useable by status
    int status_lines;   // number of current status lines
    bool color;
    int verbose;
    bool force_stderr;
    struct mp_log_buffer **buffers;
    int num_buffers;
    FILE *stats_file;
    // --- semi-atomic access
    bool mute;
    // --- must be accessed atomically
    /* This is incremented every time the msglevels must be reloaded.
     * (This is perhaps better than maintaining a globally accessible and
     * synchronized mp_log tree.) */
    atomic_ulong reload_counter;
};

struct mp_log {
    struct mp_log_root *root;
    const char *prefix;
    const char *verbose_prefix;
    int level;                  // minimum log level for any outputs
    int terminal_level;         // minimum log level for terminal output
    atomic_ulong reload_counter;
};

struct mp_log_buffer {
    struct mp_log_root *root;
    struct mp_ring *ring;
    int level;
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
    log->level = -1;
    log->terminal_level = -1;
    if (log->root->use_terminal) {
        log->level = MSGL_STATUS + log->root->verbose; // default log level
        bstr s = bstr0(log->root->msglevels);
        bstr mod;
        int level;
        while (mp_msg_split_msglevel(&s, &mod, &level) > 0) {
            if (match_mod(log->verbose_prefix, mod))
                log->level = level;
        }
        log->terminal_level = log->root->use_terminal ? log->level : -1;
    }
    for (int n = 0; n < log->root->num_buffers; n++)
        log->level = MPMAX(log->level, log->root->buffers[n]->level);
    if (log->root->stats_file)
        log->level = MPMAX(log->level, MSGL_STATS);
    atomic_store(&log->reload_counter, atomic_load(&log->root->reload_counter));
    pthread_mutex_unlock(&mp_msg_lock);
}

// Return whether the message at this verbosity level would be actually printed.
// Thread-safety: see mp_msg().
bool mp_msg_test(struct mp_log *log, int lev)
{
    struct mp_log_root *root = log->root;
    if (!root || root->mute)
        return false;
    if (atomic_load(&log->reload_counter) != atomic_load(&root->reload_counter))
        update_loglevel(log);
    return lev <= log->level;
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
    if (terminal_erase_to_end_of_line[0]) {
        fprintf(f, "\r%s", terminal_erase_to_end_of_line);
    } else {
        // This code is for MS windows (no ANSI control sequences)
        get_screen_size();
        fprintf(f, "\r%*s\r", screen_width - 1, "");
    }
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
    static const int v_colors[] = {9, 1, 3, -1, -1, 2, 8, 8, 8, -1};
    terminal_set_foreground_color(stream, v_colors[lev]);
}

static void pretty_print_module(FILE* stream, const char *prefix, bool use_color, int lev)
{
    // Use random color based on the name of the module
    if (use_color) {
        size_t prefix_len = strlen(prefix);
        unsigned int mod = 0;
        for (int i = 0; i < prefix_len; ++i)
            mod = mod * 33 + prefix[i];
        terminal_set_foreground_color(stream, (mod + 1) % 15 + 1);
    }

    fprintf(stream, "%10s", prefix);
    if (use_color)
        terminal_set_foreground_color(stream, -1);
    fprintf(stream, ": ");
    if (use_color)
        set_msg_color(stream, lev);
}

static void print_msg_on_terminal(struct mp_log *log, int lev, char *text)
{
    struct mp_log_root *root = log->root;
    FILE *stream = (root->force_stderr || lev == MSGL_STATUS) ? stderr : stdout;

    if (!(lev <= log->terminal_level))
        return;

    bool header = root->header;
    const char *prefix = log->prefix;
    char *terminate = NULL;

    if ((lev >= MSGL_V) || root->verbose || root->module)
        prefix = log->verbose_prefix;

    if (lev == MSGL_STATUS) {
        // skip status line output if stderr is a tty but in background
        if (terminal_in_background())
            return;
        // don't clear if we don't have to
        if (!text[0] && !root->status_lines)
            return;
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
        if (lev == MSGL_STATS)
            terminate = "\n";
    }

    if (root->color)
        set_msg_color(stream, lev);

    do {
        if (header) {
            if (root->show_time)
                fprintf(stream, "[%" PRId64 "] ", mp_time_us());

            if (prefix) {
                if (root->module) {
                    pretty_print_module(stream, prefix, root->color, lev);
                } else {
                    fprintf(stream, "[%s] ", prefix);
                }
            }
        }

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
}

static void write_msg_to_buffers(struct mp_log *log, int lev, char *text)
{
    struct mp_log_root *root = log->root;
    for (int n = 0; n < root->num_buffers; n++) {
        struct mp_log_buffer *buffer = root->buffers[n];
        if (lev <= buffer->level) {
            // Assuming a single writer (serialized by msg lock)
            int avail = mp_ring_available(buffer->ring) / sizeof(void *);
            if (avail < 1)
                continue;
            struct mp_log_buffer_entry *entry = talloc_ptrtype(NULL, entry);
            if (avail > 1) {
                *entry = (struct mp_log_buffer_entry) {
                    .prefix = talloc_strdup(entry, log->verbose_prefix),
                    .level = lev,
                    .text = talloc_strdup(entry, text),
                };
            } else {
                // write overflow message to signal that messages might be lost
                *entry = (struct mp_log_buffer_entry) {
                    .prefix = "overflow",
                    .level = MSGL_FATAL,
                    .text = "",
                };
            }
            mp_ring_write(buffer->ring, (unsigned char *)&entry, sizeof(entry));
        }
    }
}

static void dump_stats(struct mp_log *log, int lev, char *text)
{
    struct mp_log_root *root = log->root;
    if (lev == MSGL_STATS && root->stats_file) {
        fprintf(root->stats_file, "%"PRId64" %s #%s\n", mp_time_us(), text,
                log->verbose_prefix);
    }
}

void mp_msg_va(struct mp_log *log, int lev, const char *format, va_list va)
{
    if (!mp_msg_test(log, lev))
        return; // do not display

    pthread_mutex_lock(&mp_msg_lock);

    char tmp[MSGSIZE_MAX];
    if (vsnprintf(tmp, MSGSIZE_MAX, format, va) < 0)
        snprintf(tmp, MSGSIZE_MAX, "[fprintf error]\n");
    tmp[MSGSIZE_MAX - 2] = '\n';
    tmp[MSGSIZE_MAX - 1] = 0;
    char *text = tmp;

    print_msg_on_terminal(log, lev, text);
    write_msg_to_buffers(log, lev, text);
    dump_stats(log, lev, text);

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
    *root = (struct mp_log_root){
        .global = global,
        .header = true,
        .reload_counter = ATOMIC_VAR_INIT(1),
    };

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
    root->use_terminal = opts->use_terminal;
    root->show_time = opts->msg_time;
    if (root->use_terminal) {
        root->color = opts->msg_color && isatty(fileno(stdout));
        root->termosd = !opts->slave_mode && isatty(fileno(stderr));
    }

    talloc_free(root->msglevels);
    root->msglevels = talloc_strdup(root, global->opts->msglevels);

    atomic_fetch_add(&root->reload_counter, 1);
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
    struct mp_log_root *root = global->log->root;
    if (root->stats_file)
        fclose(root->stats_file);
    talloc_free(root);
    global->log = NULL;
}

struct mp_log_buffer *mp_msg_log_buffer_new(struct mpv_global *global,
                                            int size, int level)
{
    struct mp_log_root *root = global->log->root;

    pthread_mutex_lock(&mp_msg_lock);

    struct mp_log_buffer *buffer = talloc_ptrtype(NULL, buffer);
    *buffer = (struct mp_log_buffer) {
        .root = root,
        .level = level,
        .ring = mp_ring_new(buffer, sizeof(void *) * size),
    };
    if (!buffer->ring)
        abort();

    MP_TARRAY_APPEND(root, root->buffers, root->num_buffers, buffer);

    atomic_fetch_add(&root->reload_counter, 1);
    pthread_mutex_unlock(&mp_msg_lock);

    return buffer;
}

void mp_msg_log_buffer_destroy(struct mp_log_buffer *buffer)
{
    if (!buffer)
        return;

    pthread_mutex_lock(&mp_msg_lock);

    struct mp_log_root *root = buffer->root;
    for (int n = 0; n < root->num_buffers; n++) {
        if (root->buffers[n] == buffer) {
            MP_TARRAY_REMOVE_AT(root->buffers, root->num_buffers, n);
            goto found;
        }
    }

    abort();

found:

    while (1) {
        struct mp_log_buffer_entry *e = mp_msg_log_buffer_read(buffer);
        if (!e)
            break;
        talloc_free(e);
    }
    talloc_free(buffer);

    atomic_fetch_add(&root->reload_counter, 1);
    pthread_mutex_unlock(&mp_msg_lock);
}

// Return a queued message, or if the buffer is empty, NULL.
// Thread-safety: one buffer can be read by a single thread only.
struct mp_log_buffer_entry *mp_msg_log_buffer_read(struct mp_log_buffer *buffer)
{
    void *ptr = NULL;
    int read = mp_ring_read(buffer->ring, (unsigned char *)&ptr, sizeof(ptr));
    if (read == 0)
        return NULL;
    if (read != sizeof(ptr))
        abort();
    return ptr;
}

int mp_msg_open_stats_file(struct mpv_global *global, const char *path)
{
    struct mp_log_root *root = global->log->root;
    int r;

    pthread_mutex_lock(&mp_msg_lock);

    if (root->stats_file)
        fclose(root->stats_file);
    root->stats_file = fopen(path, "wb");
    r = root->stats_file ? 0 : -1;

    pthread_mutex_unlock(&mp_msg_lock);

    mp_msg_update_msglevels(global);
    return r;
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

char *mp_log_levels[MSGL_MAX + 1] = {
    [MSGL_FATAL]        = "fatal",
    [MSGL_ERR]          = "error",
    [MSGL_WARN]         = "warn",
    [MSGL_INFO]         = "info",
    [MSGL_STATUS]       = "status",
    [MSGL_V]            = "v",
    [MSGL_DEBUG]        = "debug",
    [MSGL_TRACE]        = "trace",
    [MSGL_STATS]        = "stats",
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
    for (int n = 0; n < MP_ARRAY_SIZE(mp_log_levels); n++) {
        if (mp_log_levels[n] && bstr_equals0(level, mp_log_levels[n])) {
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
