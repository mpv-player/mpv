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
#include "misc/bstr.h"
#include "options/options.h"
#include "options/path.h"
#include "osdep/terminal.h"
#include "osdep/io.h"
#include "osdep/threads.h"
#include "osdep/timer.h"

#include "libmpv/client.h"

#include "msg.h"
#include "msg_control.h"

#define TERM_BUF 100

struct mp_log_root {
    struct mpv_global *global;
    pthread_mutex_t lock;
    pthread_mutex_t log_file_lock;
    pthread_cond_t log_file_wakeup;
    // --- protected by lock
    char **msg_levels;
    bool use_terminal;  // make accesses to stderr/stdout
    bool module;
    bool show_time;
    int blank_lines;    // number of lines usable by status
    int status_lines;   // number of current status lines
    bool color;
    int verbose;
    bool really_quiet;
    bool force_stderr;
    struct mp_log_buffer **buffers;
    int num_buffers;
    struct mp_log_buffer *early_buffer;
    FILE *stats_file;
    bstr buffer;
    // --- must be accessed atomically
    /* This is incremented every time the msglevels must be reloaded.
     * (This is perhaps better than maintaining a globally accessible and
     * synchronized mp_log tree.) */
    atomic_ulong reload_counter;
    // --- owner thread only (caller of mp_msg_init() etc.)
    char *log_path;
    char *stats_path;
    pthread_t log_file_thread;
    // --- owner thread only, but frozen while log_file_thread is running
    FILE *log_file;
    struct mp_log_buffer *log_file_buffer;
    // --- protected by log_file_lock
    bool log_file_thread_active; // also termination signal for the thread
};

struct mp_log {
    struct mp_log_root *root;
    const char *prefix;
    const char *verbose_prefix;
    int max_level;              // minimum log level for this instance
    int level;                  // minimum log level for any outputs
    int terminal_level;         // minimum log level for terminal output
    atomic_ulong reload_counter;
    char *partial;
};

struct mp_log_buffer {
    struct mp_log_root *root;
    pthread_mutex_t lock;
    // --- protected by lock
    struct mp_log_buffer_entry **entries;   // ringbuffer
    int capacity;                           // total space in entries[]
    int entry0;                             // first (oldest) entry index
    int num_entries;                        // number of valid entries after entry0
    uint64_t dropped;                       // number of skipped entries
    bool silent;
    // --- immutable
    void (*wakeup_cb)(void *ctx);
    void *wakeup_cb_ctx;
    int level;
};

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
    pthread_mutex_lock(&root->lock);
    log->level = MSGL_STATUS + root->verbose; // default log level
    if (root->really_quiet)
        log->level = -1;
    for (int n = 0; root->msg_levels && root->msg_levels[n * 2 + 0]; n++) {
        if (match_mod(log->verbose_prefix, root->msg_levels[n * 2 + 0]))
            log->level = mp_msg_find_level(root->msg_levels[n * 2 + 1]);
    }
    log->terminal_level = log->level;
    for (int n = 0; n < log->root->num_buffers; n++) {
        int buffer_level = log->root->buffers[n]->level;
        if (buffer_level == MP_LOG_BUFFER_MSGL_LOGFILE)
            buffer_level = MSGL_DEBUG;
        if (buffer_level != MP_LOG_BUFFER_MSGL_TERM)
            log->level = MPMAX(log->level, buffer_level);
    }
    if (log->root->log_file)
        log->level = MPMAX(log->level, MSGL_DEBUG);
    if (log->root->stats_file)
        log->level = MPMAX(log->level, MSGL_STATS);
    log->level = MPMIN(log->level, log->max_level);
    atomic_store(&log->reload_counter, atomic_load(&log->root->reload_counter));
    pthread_mutex_unlock(&root->lock);
}

// Set (numerically) the maximum level that should still be output for this log
// instances. E.g. lev=MSGL_WARN => show only warnings and errors.
void mp_msg_set_max_level(struct mp_log *log, int lev)
{
    if (!log->root)
        return;
    pthread_mutex_lock(&log->root->lock);
    log->max_level = MPCLAMP(lev, -1, MSGL_MAX);
    pthread_mutex_unlock(&log->root->lock);
    update_loglevel(log);
}

// Get the current effective msg level.
// Thread-safety: see mp_msg().
int mp_msg_level(struct mp_log *log)
{
    struct mp_log_root *root = log->root;
    if (!root)
        return -1;
    if (atomic_load_explicit(&log->reload_counter, memory_order_relaxed) !=
        atomic_load_explicit(&root->reload_counter, memory_order_relaxed))
    {
        update_loglevel(log);
    }
    return log->level;
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
    if (log->root) {
        pthread_mutex_lock(&log->root->lock);
        flush_status_line(log->root);
        pthread_mutex_unlock(&log->root->lock);
    }
}

void mp_msg_set_term_title(struct mp_log *log, const char *title)
{
    if (log->root && title) {
        // Lock because printf to terminal is not necessarily atomic.
        pthread_mutex_lock(&log->root->lock);
        fprintf(stderr, "\e]0;%s\007", title);
        pthread_mutex_unlock(&log->root->lock);
    }
}

bool mp_msg_has_status_line(struct mpv_global *global)
{
    struct mp_log_root *root = global->log->root;
    pthread_mutex_lock(&root->lock);
    bool r = root->status_lines > 0;
    pthread_mutex_unlock(&root->lock);
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
        fprintf(stream, "[%10.6f] ", (mp_time_us() - MP_START_TIME) / 1e6);

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

static struct mp_log_buffer_entry *log_buffer_read(struct mp_log_buffer *buffer)
{
    assert(buffer->num_entries);
    struct mp_log_buffer_entry *res = buffer->entries[buffer->entry0];
    buffer->entry0 = (buffer->entry0 + 1) % buffer->capacity;
    buffer->num_entries -= 1;
    return res;
}

static void write_msg_to_buffers(struct mp_log *log, int lev, char *text)
{
    struct mp_log_root *root = log->root;
    for (int n = 0; n < root->num_buffers; n++) {
        struct mp_log_buffer *buffer = root->buffers[n];
        bool wakeup = false;
        pthread_mutex_lock(&buffer->lock);
        int buffer_level = buffer->level;
        if (buffer_level == MP_LOG_BUFFER_MSGL_TERM)
            buffer_level = log->terminal_level;
        if (buffer_level == MP_LOG_BUFFER_MSGL_LOGFILE)
            buffer_level = MPMAX(log->terminal_level, MSGL_DEBUG);
        if (lev <= buffer_level && lev != MSGL_STATUS) {
            if (buffer->level == MP_LOG_BUFFER_MSGL_LOGFILE) {
                // If the buffer is full, block until we can write again.
                bool dead = false;
                while (buffer->num_entries == buffer->capacity && !dead) {
                    // Temporary unlock is OK; buffer->level is immutable, and
                    // buffer can't go away because the global log lock is held.
                    pthread_mutex_unlock(&buffer->lock);
                    pthread_mutex_lock(&root->log_file_lock);
                    if (root->log_file_thread_active) {
                        pthread_cond_wait(&root->log_file_wakeup,
                                          &root->log_file_lock);
                    } else {
                        dead = true;
                    }
                    pthread_mutex_unlock(&root->log_file_lock);
                    pthread_mutex_lock(&buffer->lock);
                }
            }
            if (buffer->num_entries == buffer->capacity) {
                struct mp_log_buffer_entry *skip = log_buffer_read(buffer);
                talloc_free(skip);
                buffer->dropped += 1;
            }
            struct mp_log_buffer_entry *entry = talloc_ptrtype(NULL, entry);
            *entry = (struct mp_log_buffer_entry) {
                .prefix = talloc_strdup(entry, log->verbose_prefix),
                .level = lev,
                .text = talloc_strdup(entry, text),
            };
            int pos = (buffer->entry0 + buffer->num_entries) % buffer->capacity;
            buffer->entries[pos] = entry;
            buffer->num_entries += 1;
            if (buffer->wakeup_cb && !buffer->silent)
                wakeup = true;
        }
        pthread_mutex_unlock(&buffer->lock);
        if (wakeup)
            buffer->wakeup_cb(buffer->wakeup_cb_ctx);
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

    struct mp_log_root *root = log->root;

    pthread_mutex_lock(&root->lock);

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
        if (lev == MSGL_STATUS)
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
            write_msg_to_buffers(log, lev, text);
            next[0] = saved;
            text = next;
        }

        if (lev == MSGL_STATUS) {
            if (text[0])
                print_terminal_line(log, lev, text, "\r");
        } else if (text[0]) {
            int size = strlen(text) + 1;
            if (talloc_get_size(log->partial) < size)
                log->partial = talloc_realloc(NULL, log->partial, char, size);
            memcpy(log->partial, text, size);
        }
    }

    pthread_mutex_unlock(&root->lock);
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
    log->max_level = MSGL_MAX;
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

    pthread_mutex_init(&root->lock, NULL);
    pthread_mutex_init(&root->log_file_lock, NULL);
    pthread_cond_init(&root->log_file_wakeup, NULL);

    struct mp_log dummy = { .root = root };
    struct mp_log *log = mp_log_new(root, &dummy, "");

    global->log = log;
}

static void *log_file_thread(void *p)
{
    struct mp_log_root *root = p;

    mpthread_set_name("log-file");

    pthread_mutex_lock(&root->log_file_lock);

    while (root->log_file_thread_active) {
        struct mp_log_buffer_entry *e =
            mp_msg_log_buffer_read(root->log_file_buffer);
        if (e) {
            pthread_mutex_unlock(&root->log_file_lock);
            fprintf(root->log_file, "[%8.3f][%c][%s] %s",
                    (mp_time_us() - MP_START_TIME) / 1e6,
                    mp_log_levels[e->level][0], e->prefix, e->text);
            fflush(root->log_file);
            pthread_mutex_lock(&root->log_file_lock);
            talloc_free(e);
            // Multiple threads might be blocked if the log buffer was full.
            pthread_cond_broadcast(&root->log_file_wakeup);
        } else {
            pthread_cond_wait(&root->log_file_wakeup, &root->log_file_lock);
        }
    }

    pthread_mutex_unlock(&root->log_file_lock);

    return NULL;
}

static void wakeup_log_file(void *p)
{
    struct mp_log_root *root = p;

    pthread_mutex_lock(&root->log_file_lock);
    pthread_cond_broadcast(&root->log_file_wakeup);
    pthread_mutex_unlock(&root->log_file_lock);
}

// Only to be called from the main thread.
static void terminate_log_file_thread(struct mp_log_root *root)
{
    bool wait_terminate = false;

    pthread_mutex_lock(&root->log_file_lock);
    if (root->log_file_thread_active) {
        root->log_file_thread_active = false;
        pthread_cond_broadcast(&root->log_file_wakeup);
        wait_terminate = true;
    }
    pthread_mutex_unlock(&root->log_file_lock);

    if (wait_terminate)
        pthread_join(root->log_file_thread, NULL);

    mp_msg_log_buffer_destroy(root->log_file_buffer);
    root->log_file_buffer = NULL;

    if (root->log_file)
        fclose(root->log_file);
    root->log_file = NULL;
}

// If opt is different from *current_path, update *current_path and return true.
// No lock must be held; passed values must be accessible without.
static bool check_new_path(struct mpv_global *global, char *opt,
                           char **current_path)
{
    void *tmp = talloc_new(NULL);
    bool res = false;

    char *new_path = mp_get_user_path(tmp, global, opt);
    if (!new_path)
        new_path = "";

    char *old_path = *current_path ? *current_path : "";
    if (strcmp(old_path, new_path) != 0) {
        talloc_free(*current_path);
        *current_path = NULL;
        if (new_path && new_path[0])
            *current_path = talloc_strdup(NULL, new_path);
        res = true;
    }

    talloc_free(tmp);

    return res;
}

void mp_msg_update_msglevels(struct mpv_global *global, struct MPOpts *opts)
{
    struct mp_log_root *root = global->log->root;

    pthread_mutex_lock(&root->lock);

    root->verbose = opts->verbose;
    root->really_quiet = opts->msg_really_quiet;
    root->module = opts->msg_module;
    root->use_terminal = opts->use_terminal;
    root->show_time = opts->msg_time;
    if (root->use_terminal)
        root->color = opts->msg_color && isatty(STDOUT_FILENO);

    m_option_type_msglevels.free(&root->msg_levels);
    m_option_type_msglevels.copy(NULL, &root->msg_levels, &opts->msg_levels);

    atomic_fetch_add(&root->reload_counter, 1);
    pthread_mutex_unlock(&root->lock);

    if (check_new_path(global, opts->log_file, &root->log_path)) {
        terminate_log_file_thread(root);
        if (root->log_path) {
            root->log_file = fopen(root->log_path, "wb");
            if (root->log_file) {
                root->log_file_buffer =
                    mp_msg_log_buffer_new(global, 100, MP_LOG_BUFFER_MSGL_LOGFILE,
                                          wakeup_log_file, root);
                root->log_file_thread_active = true;
                if (pthread_create(&root->log_file_thread, NULL, log_file_thread,
                                   root))
                {
                    root->log_file_thread_active = false;
                    terminate_log_file_thread(root);
                }
            } else {
                mp_err(global->log, "Failed to open log file '%s'\n",
                       root->log_path);
            }
        }
    }

    if (check_new_path(global, opts->dump_stats, &root->stats_path)) {
        bool open_error = false;

        pthread_mutex_lock(&root->lock);
        if (root->stats_file)
            fclose(root->stats_file);
        root->stats_file = NULL;
        if (root->stats_path) {
            root->stats_file = fopen(root->stats_path, "wb");
            open_error = !root->stats_file;
        }
        pthread_mutex_unlock(&root->lock);

        if (open_error) {
            mp_err(global->log, "Failed to open stats file '%s'\n",
                   root->stats_path);
        }
    }
}

void mp_msg_force_stderr(struct mpv_global *global, bool force_stderr)
{
    struct mp_log_root *root = global->log->root;

    pthread_mutex_lock(&root->lock);
    root->force_stderr = force_stderr;
    pthread_mutex_unlock(&root->lock);
}

// Only to be called from the main thread.
bool mp_msg_has_log_file(struct mpv_global *global)
{
    struct mp_log_root *root = global->log->root;

    return !!root->log_file;
}

void mp_msg_uninit(struct mpv_global *global)
{
    struct mp_log_root *root = global->log->root;
    terminate_log_file_thread(root);
    mp_msg_log_buffer_destroy(root->early_buffer);
    assert(root->num_buffers == 0);
    if (root->stats_file)
        fclose(root->stats_file);
    talloc_free(root->stats_path);
    talloc_free(root->log_path);
    m_option_type_msglevels.free(&root->msg_levels);
    pthread_mutex_destroy(&root->lock);
    pthread_mutex_destroy(&root->log_file_lock);
    pthread_cond_destroy(&root->log_file_wakeup);
    talloc_free(root);
    global->log = NULL;
}

void mp_msg_set_early_logging(struct mpv_global *global, bool enable)
{
    struct mp_log_root *root = global->log->root;
    pthread_mutex_lock(&root->lock);

    if (enable != !!root->early_buffer) {
        if (enable) {
            pthread_mutex_unlock(&root->lock);
            struct mp_log_buffer *buf =
                mp_msg_log_buffer_new(global, TERM_BUF, MP_LOG_BUFFER_MSGL_TERM,
                                      NULL, NULL);
            pthread_mutex_lock(&root->lock);
            assert(!root->early_buffer); // no concurrent calls to this function
            root->early_buffer = buf;
        } else {
            struct mp_log_buffer *buf = root->early_buffer;
            root->early_buffer = NULL;
            pthread_mutex_unlock(&root->lock);
            mp_msg_log_buffer_destroy(buf);
            return;
        }
    }

    pthread_mutex_unlock(&root->lock);
}

struct mp_log_buffer *mp_msg_log_buffer_new(struct mpv_global *global,
                                            int size, int level,
                                            void (*wakeup_cb)(void *ctx),
                                            void *wakeup_cb_ctx)
{
    struct mp_log_root *root = global->log->root;

    pthread_mutex_lock(&root->lock);

    if (level == MP_LOG_BUFFER_MSGL_TERM) {
        size = TERM_BUF;

        // The first thing which creates a terminal-level log buffer gets the
        // early log buffer, if it exists. This is supposed to enable a script
        // to grab log messages from before it was initialized. It's OK that
        // this works only for 1 script and only once.
        if (root->early_buffer) {
            struct mp_log_buffer *buffer = root->early_buffer;
            root->early_buffer = NULL;
            buffer->wakeup_cb = wakeup_cb;
            buffer->wakeup_cb_ctx = wakeup_cb_ctx;
            pthread_mutex_unlock(&root->lock);
            return buffer;
        }
    }

    assert(size > 0);

    struct mp_log_buffer *buffer = talloc_ptrtype(NULL, buffer);
    *buffer = (struct mp_log_buffer) {
        .root = root,
        .level = level,
        .entries = talloc_array(buffer, struct mp_log_buffer_entry *, size),
        .capacity = size,
        .wakeup_cb = wakeup_cb,
        .wakeup_cb_ctx = wakeup_cb_ctx,
    };

    pthread_mutex_init(&buffer->lock, NULL);

    MP_TARRAY_APPEND(root, root->buffers, root->num_buffers, buffer);

    atomic_fetch_add(&root->reload_counter, 1);
    pthread_mutex_unlock(&root->lock);

    return buffer;
}

void mp_msg_log_buffer_set_silent(struct mp_log_buffer *buffer, bool silent)
{
    pthread_mutex_lock(&buffer->lock);
    buffer->silent = silent;
    pthread_mutex_unlock(&buffer->lock);
}

void mp_msg_log_buffer_destroy(struct mp_log_buffer *buffer)
{
    if (!buffer)
        return;

    struct mp_log_root *root = buffer->root;

    pthread_mutex_lock(&root->lock);

    for (int n = 0; n < root->num_buffers; n++) {
        if (root->buffers[n] == buffer) {
            MP_TARRAY_REMOVE_AT(root->buffers, root->num_buffers, n);
            goto found;
        }
    }

    abort();

found:

    while (buffer->num_entries)
        talloc_free(log_buffer_read(buffer));

    pthread_mutex_destroy(&buffer->lock);
    talloc_free(buffer);

    atomic_fetch_add(&root->reload_counter, 1);
    pthread_mutex_unlock(&root->lock);
}

// Return a queued message, or if the buffer is empty, NULL.
// Thread-safety: one buffer can be read by a single thread only.
struct mp_log_buffer_entry *mp_msg_log_buffer_read(struct mp_log_buffer *buffer)
{
    struct mp_log_buffer_entry *res = NULL;

    pthread_mutex_lock(&buffer->lock);

    if (!buffer->silent && buffer->num_entries) {
        if (buffer->dropped) {
            res = talloc_ptrtype(NULL, res);
            *res = (struct mp_log_buffer_entry) {
                .prefix = "overflow",
                .level = MSGL_FATAL,
                .text = talloc_asprintf(res,
                    "log message buffer overflow: %"PRId64" messages skipped\n",
                    buffer->dropped),
            };
            buffer->dropped = 0;
        } else {
            res = log_buffer_read(buffer);
        }
    }

    pthread_mutex_unlock(&buffer->lock);

    return res;
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
