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
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <stdint.h>

#include "mpv_talloc.h"

#include "misc/bstr.h"
#include "osdep/atomic.h"
#include "common/common.h"
#include "common/global.h"
#include "misc/ring.h"
#include "misc/bstr.h"
#include "options/options.h"
#include "options/path.h"
#include "osdep/terminal.h"
#include "osdep/io.h"
#include "osdep/timer.h"

#include "libmpv/client.h"

#include "msg.h"
#include "msg_control.h"

struct mp_log_root {
    struct mpv_global *global;
    // --- protected by mp_msg_lock
    char **msg_levels;
    bool use_terminal;  // make accesses to stderr/stdout
    bool module;
    bool show_time;
    bool termosd;       // use terminal control codes for status line
    int blank_lines;    // number of lines usable by status
    int status_lines;   // number of current status lines
    bool color;
    int verbose;
    bool really_quiet;
    bool force_stderr;
    struct mp_log_buffer **buffers;
    int num_buffers;
    FILE *log_file;
    FILE *stats_file;
    char *log_path;
    char *stats_path;
    // --- must be accessed atomically
    /* This is incremented every time the msglevels must be reloaded.
     * (This is perhaps better than maintaining a globally accessible and
     * synchronized mp_log tree.) */
    atomic_ulong reload_counter;
    // --- protected by mp_msg_lock
    bstr buffer;
};

struct mp_log {
    struct mp_log_root *root;
    const char *prefix;
    const char *verbose_prefix;
    int level;                  // minimum log level for any outputs
    int terminal_level;         // minimum log level for terminal output
    atomic_ulong reload_counter;
    char *partial;
};

struct mp_log_buffer {
    struct mp_log_root *root;
    struct mp_ring *ring;
    int level;
    void (*wakeup_cb)(void *ctx);
    void *wakeup_cb_ctx;
};

// Protects some (not all) state in mp_log_root
static pthread_mutex_t mp_msg_lock = PTHREAD_MUTEX_INITIALIZER;

static const struct mp_log null_log = {0};
struct mp_log *const mp_null_log = (struct mp_log *)&null_log;

static bool match_mod(const char *name, const char *mod)
{
    if (!strcmp(mod, "all"))
        return true;
    // Path prefix matches
    bstr b = bstr0(name);
    return bstr_eatstart0(&b, mod) && (bstr_eatstart0(&b, "/") || !b.len);
}

static void update_loglevel(struct mp_log *log)
{
    struct mp_log_root *root = log->root;
    pthread_mutex_lock(&mp_msg_lock);
    log->level = MSGL_STATUS + root->verbose; // default log level
    if (root->really_quiet)
        log->level -= 10;
    for (int n = 0; root->msg_levels && root->msg_levels[n * 2 + 0]; n++) {
        if (match_mod(log->verbose_prefix, root->msg_levels[n * 2 + 0]))
            log->level = mp_msg_find_level(root->msg_levels[n * 2 + 1]);
    }
    log->terminal_level = log->level;
    for (int n = 0; n < log->root->num_buffers; n++)
        log->level = MPMAX(log->level, log->root->buffers[n]->level);
    if (log->root->log_file)
        log->level = MPMAX(log->level, MSGL_DEBUG);
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
    if (!root)
        return false;
    if (atomic_load_explicit(&log->reload_counter, memory_order_relaxed) !=
        atomic_load_explicit(&root->reload_counter, memory_order_relaxed))
    {
        update_loglevel(log);
    }
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
    if (!new_status[0] && old_lines == 0)
        return; // nothing to clear

    size_t clear_lines = MPMIN(MPMAX(new_lines, old_lines), root->blank_lines);

    // clear the status line itself
    fprintf(f, "\r\033[K");
    // and clear all previous old lines
    for (size_t n = 1; n < clear_lines; n++)
        fprintf(f, "\033[A\r\033[K");
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

void mp_msg_flush_status_line(struct mp_log *log)
{
    pthread_mutex_lock(&mp_msg_lock);
    if (log->root)
        flush_status_line(log->root);
    pthread_mutex_unlock(&mp_msg_lock);
}

bool mp_msg_has_status_line(struct mpv_global *global)
{
    pthread_mutex_lock(&mp_msg_lock);
    bool r = global->log->root->status_lines > 0;
    pthread_mutex_unlock(&mp_msg_lock);
    return r;
}

static void set_term_color(FILE *stream, int c)
{
    if (c == -1) {
        fprintf(stream, "\033[0m");
    } else {
        fprintf(stream, "\033[%d;3%dm", c >> 3, c & 7);
    }
}


static void set_msg_color(FILE* stream, int lev)
{
    static const int v_colors[] = {9, 1, 3, -1, -1, 2, 8, 8, 8, -1};
    set_term_color(stream, v_colors[lev]);
}

static void pretty_print_module(FILE* stream, const char *prefix, bool use_color, int lev)
{
    // Use random color based on the name of the module
    if (use_color) {
        size_t prefix_len = strlen(prefix);
        unsigned int mod = 0;
        for (int i = 0; i < prefix_len; ++i)
            mod = mod * 33 + prefix[i];
        set_term_color(stream, (mod + 1) % 15 + 1);
    }

    fprintf(stream, "%10s", prefix);
    if (use_color)
        set_term_color(stream, -1);
    fprintf(stream, ": ");
    if (use_color)
        set_msg_color(stream, lev);
}

static bool test_terminal_level(struct mp_log *log, int lev)
{
    return lev <= log->terminal_level && log->root->use_terminal &&
           !(lev == MSGL_STATUS && terminal_in_background());
}

static void print_terminal_line(struct mp_log *log, int lev,
                                char *text,  char *trail)
{
    if (!test_terminal_level(log, lev))
        return;

    struct mp_log_root *root = log->root;
    FILE *stream = (root->force_stderr || lev == MSGL_STATUS) ? stderr : stdout;

    if (lev != MSGL_STATUS)
        flush_status_line(root);

    if (root->color)
        set_msg_color(stream, lev);

    if (root->show_time)
        fprintf(stream, "[%" PRId64 "] ", mp_time_us());

    const char *prefix = log->prefix;
    if ((lev >= MSGL_V) || root->verbose || root->module)
        prefix = log->verbose_prefix;

    if (prefix) {
        if (root->module) {
            pretty_print_module(stream, prefix, root->color, lev);
        } else {
            fprintf(stream, "[%s] ", prefix);
        }
    }

    fprintf(stream, "%s%s", text, trail);

    if (root->color)
        set_term_color(stream, -1);
    fflush(stream);
}

static void write_log_file(struct mp_log *log, int lev, char *text)
{
    struct mp_log_root *root = log->root;

    if (!root->log_file || lev > MPMAX(MSGL_DEBUG, log->terminal_level))
        return;

    fprintf(root->log_file, "[%8.3f][%c][%s] %s",
            (mp_time_us() - MP_START_TIME) / 1e6,
            mp_log_levels[lev][0],
            log->verbose_prefix, text);
    fflush(root->log_file);
}

static void write_msg_to_buffers(struct mp_log *log, int lev, char *text)
{
    struct mp_log_root *root = log->root;
    for (int n = 0; n < root->num_buffers; n++) {
        struct mp_log_buffer *buffer = root->buffers[n];
        int buffer_level = buffer->level;
        if (buffer_level == MP_LOG_BUFFER_MSGL_TERM)
            buffer_level = log->terminal_level;
        if (lev <= buffer_level && lev != MSGL_STATUS) {
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
                    .text = "log message buffer overflow\n",
                };
            }
            mp_ring_write(buffer->ring, (unsigned char *)&entry, sizeof(entry));
            if (buffer->wakeup_cb)
                buffer->wakeup_cb(buffer->wakeup_cb_ctx);
        }
    }
}

static void dump_stats(struct mp_log *log, int lev, char *text)
{
    struct mp_log_root *root = log->root;
    if (lev == MSGL_STATS && root->stats_file)
        fprintf(root->stats_file, "%"PRId64" %s\n", mp_time_us(), text);
}

void mp_msg_va(struct mp_log *log, int lev, const char *format, va_list va)
{
    if (!mp_msg_test(log, lev))
        return; // do not display

    pthread_mutex_lock(&mp_msg_lock);

    struct mp_log_root *root = log->root;

    root->buffer.len = 0;

    if (log->partial[0])
        bstr_xappend_asprintf(root, &root->buffer, "%s", log->partial);
    log->partial[0] = '\0';

    bstr_xappend_vasprintf(root, &root->buffer, format, va);

    char *text = root->buffer.start;

    if (lev == MSGL_STATS) {
        dump_stats(log, lev, text);
    } else if (lev == MSGL_STATUS && !test_terminal_level(log, lev)) {
        /* discard */
    } else {
        if (lev == MSGL_STATUS && root->termosd)
            prepare_status_line(root, text);

        // Split away each line. Normally we require full lines; buffer partial
        // lines if they happen.
        while (1) {
            char *end = strchr(text, '\n');
            if (!end)
                break;
            char *next = &end[1];
            char saved = next[0];
            next[0] = '\0';
            print_terminal_line(log, lev, text, "");
            write_log_file(log, lev, text);
            write_msg_to_buffers(log, lev, text);
            next[0] = saved;
            text = next;
        }

        if (lev == MSGL_STATUS) {
            if (text[0])
                print_terminal_line(log, lev, text, root->termosd ? "\r" : "\n");
        } else if (text[0]) {
            int size = strlen(text) + 1;
            if (talloc_get_size(log->partial) < size)
                log->partial = talloc_realloc(NULL, log->partial, char, size);
            memcpy(log->partial, text, size);
        }
    }

    pthread_mutex_unlock(&mp_msg_lock);
}

static void destroy_log(void *ptr)
{
    struct mp_log *log = ptr;
    // This is not managed via talloc itself, because mp_msg calls must be
    // thread-safe, while talloc is not thread-safe.
    talloc_free(log->partial);
}

// Create a new log context, which uses talloc_ctx as talloc parent, and parent
// as logical parent.
// The name is the prefix put before the output. It's usually prefixed by the
// parent's name. If the name starts with "/", the parent's name is not
// prefixed (except in verbose mode), and if it starts with "!", the name is
// not printed at all (except in verbose mode).
// If name is NULL, the parent's name/prefix is used.
// Thread-safety: fully thread-safe, but keep in mind that talloc is not (so
//                talloc_ctx must be owned by the current thread).
struct mp_log *mp_log_new(void *talloc_ctx, struct mp_log *parent,
                          const char *name)
{
    assert(parent);
    struct mp_log *log = talloc_zero(talloc_ctx, struct mp_log);
    if (!parent->root)
        return log; // same as null_log
    talloc_set_destructor(log, destroy_log);
    log->root = parent->root;
    log->partial = talloc_strdup(NULL, "");
    if (name) {
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
    } else {
        log->prefix = talloc_strdup(log, parent->prefix);
        log->verbose_prefix = talloc_strdup(log, parent->verbose_prefix);
    }
    return log;
}

void mp_msg_init(struct mpv_global *global)
{
    assert(!global->log);

    struct mp_log_root *root = talloc_zero(NULL, struct mp_log_root);
    *root = (struct mp_log_root){
        .global = global,
        .reload_counter = ATOMIC_VAR_INIT(1),
    };

    struct mp_log dummy = { .root = root };
    struct mp_log *log = mp_log_new(root, &dummy, "");

    global->log = log;

    mp_msg_update_msglevels(global);
}

// If opt is different from *current_path, reopen *file and update *current_path.
// If there's an error, _append_ it to err_buf.
// *current_path and *file are, rather trickily, only accessible under the
// mp_msg_lock.
static void reopen_file(char *opt, char **current_path, FILE **file,
                        const char *type, struct mpv_global *global)
{
    void *tmp = talloc_new(NULL);
    bool fail = false;

    char *new_path = mp_get_user_path(tmp, global, opt);
    if (!new_path)
        new_path = "";

    pthread_mutex_lock(&mp_msg_lock); // for *current_path/*file

    char *old_path = *current_path ? *current_path : "";
    if (strcmp(old_path, new_path) != 0) {
        if (*file)
            fclose(*file);
        *file = NULL;
        talloc_free(*current_path);
        *current_path = talloc_strdup(NULL, new_path);
        if (new_path[0]) {
            *file = fopen(new_path, "wb");
            fail = !*file;
        }
    }

    pthread_mutex_unlock(&mp_msg_lock);

    if (fail)
        mp_err(global->log, "Failed to open %s file '%s'\n", type, new_path);

    talloc_free(tmp);
}

void mp_msg_update_msglevels(struct mpv_global *global)
{
    struct mp_log_root *root = global->log->root;
    struct MPOpts *opts = global->opts;

    if (!opts)
        return;

    pthread_mutex_lock(&mp_msg_lock);

    root->verbose = opts->verbose;
    root->really_quiet = opts->msg_really_quiet;
    root->module = opts->msg_module;
    root->use_terminal = opts->use_terminal;
    root->show_time = opts->msg_time;
    if (root->use_terminal) {
        root->color = opts->msg_color && isatty(STDOUT_FILENO);
        root->termosd = isatty(STDERR_FILENO);
    }

    m_option_type_msglevels.free(&root->msg_levels);
    m_option_type_msglevels.copy(NULL, &root->msg_levels,
                                 &global->opts->msg_levels);

    atomic_fetch_add(&root->reload_counter, 1);
    pthread_mutex_unlock(&mp_msg_lock);

    reopen_file(opts->log_file, &root->log_path, &root->log_file,
                "log", global);

    reopen_file(opts->dump_stats, &root->stats_path, &root->stats_file,
                "stats", global);
}

void mp_msg_force_stderr(struct mpv_global *global, bool force_stderr)
{
    struct mp_log_root *root = global->log->root;

    pthread_mutex_lock(&mp_msg_lock);
    root->force_stderr = force_stderr;
    pthread_mutex_unlock(&mp_msg_lock);
}

bool mp_msg_has_log_file(struct mpv_global *global)
{
    struct mp_log_root *root = global->log->root;

    pthread_mutex_lock(&mp_msg_lock);
    bool res = !!root->log_file;
    pthread_mutex_unlock(&mp_msg_lock);

    return res;
}

void mp_msg_uninit(struct mpv_global *global)
{
    struct mp_log_root *root = global->log->root;
    if (root->stats_file)
        fclose(root->stats_file);
    talloc_free(root->stats_path);
    if (root->log_file)
        fclose(root->log_file);
    talloc_free(root->log_path);
    m_option_type_msglevels.free(&root->msg_levels);
    talloc_free(root);
    global->log = NULL;
}

struct mp_log_buffer *mp_msg_log_buffer_new(struct mpv_global *global,
                                            int size, int level,
                                            void (*wakeup_cb)(void *ctx),
                                            void *wakeup_cb_ctx)
{
    struct mp_log_root *root = global->log->root;

#if !HAVE_ATOMICS
    return NULL;
#endif

    pthread_mutex_lock(&mp_msg_lock);

    struct mp_log_buffer *buffer = talloc_ptrtype(NULL, buffer);
    *buffer = (struct mp_log_buffer) {
        .root = root,
        .level = level,
        .ring = mp_ring_new(buffer, sizeof(void *) * size),
        .wakeup_cb = wakeup_cb,
        .wakeup_cb_ctx = wakeup_cb_ctx,
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

const char *const mp_log_levels[MSGL_MAX + 1] = {
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

const int mp_mpv_log_levels[MSGL_MAX + 1] = {
    [MSGL_FATAL]        = MPV_LOG_LEVEL_FATAL,
    [MSGL_ERR]          = MPV_LOG_LEVEL_ERROR,
    [MSGL_WARN]         = MPV_LOG_LEVEL_WARN,
    [MSGL_INFO]         = MPV_LOG_LEVEL_INFO,
    [MSGL_STATUS]       = 0, // never used
    [MSGL_V]            = MPV_LOG_LEVEL_V,
    [MSGL_DEBUG]        = MPV_LOG_LEVEL_DEBUG,
    [MSGL_TRACE]        = MPV_LOG_LEVEL_TRACE,
    [MSGL_STATS]        = 0, // never used
};

int mp_msg_find_level(const char *s)
{
    for (int n = 0; n < MP_ARRAY_SIZE(mp_log_levels); n++) {
        if (mp_log_levels[n] && !strcmp(s, mp_log_levels[n]))
            return n;
    }
    return -1;
}
