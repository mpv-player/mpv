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

#include <assert.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpv_talloc.h"

#include "misc/bstr.h"
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

// log buffer size (lines) logfile level
#define FILE_BUF 100

// lines to accumulate before any client requests the terminal loglevel
#define EARLY_TERM_BUF 100

// logfile lines to accumulate during init before we know the log file name.
// thousands of logfile lines during init can happen (especially with many
// scripts, big config, etc), so we set 5000. If it cycles and messages are
// overwritten, then the first (virtual) log line indicates how many were lost.
#define EARLY_FILE_BUF 5000

struct mp_log_root {
    struct mpv_global *global;
    mp_mutex lock;
    mp_mutex log_file_lock;
    mp_cond log_file_wakeup;
    // --- protected by lock
    char **msg_levels;
    bool use_terminal;  // make accesses to stderr/stdout
    bool module;
    bool show_time;
    int blank_lines;    // number of lines usable by status
    int status_lines;   // number of current status lines
    bool color[STDERR_FILENO + 1];
    bool isatty[STDERR_FILENO + 1];
    int verbose;
    bool really_quiet;
    bool force_stderr;
    struct mp_log_buffer **buffers;
    int num_buffers;
    struct mp_log_buffer *early_buffer;
    struct mp_log_buffer *early_filebuffer;
    FILE *stats_file;
    bstr buffer;
    bstr term_msg;
    bstr term_msg_tmp;
    bstr status_line;
    struct mp_log *status_log;
    bstr term_status_msg;
    // --- must be accessed atomically
    /* This is incremented every time the msglevels must be reloaded.
     * (This is perhaps better than maintaining a globally accessible and
     * synchronized mp_log tree.) */
    atomic_ulong reload_counter;
    // --- owner thread only (caller of mp_msg_init() etc.)
    char *log_path;
    char *stats_path;
    mp_thread log_file_thread;
    // --- owner thread only, but frozen while log_file_thread is running
    FILE *log_file;
    struct mp_log_buffer *log_file_buffer;
    // --- protected by log_file_lock
    bool log_file_thread_active; // also termination signal for the thread
    int module_indent;
};

struct mp_log {
    struct mp_log_root *root;
    const char *prefix;
    const char *verbose_prefix;
    int max_level;              // minimum log level for this instance
    int level;                  // minimum log level for any outputs
    int terminal_level;         // minimum log level for terminal output
    atomic_ulong reload_counter;
    bstr partial[MSGL_MAX + 1];
};

struct mp_log_buffer {
    struct mp_log_root *root;
    mp_mutex lock;
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
    mp_mutex_lock(&root->lock);
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
    mp_mutex_unlock(&root->lock);
}

// Set (numerically) the maximum level that should still be output for this log
// instances. E.g. lev=MSGL_WARN => show only warnings and errors.
void mp_msg_set_max_level(struct mp_log *log, int lev)
{
    if (!log->root)
        return;
    mp_mutex_lock(&log->root->lock);
    log->max_level = MPCLAMP(lev, -1, MSGL_MAX);
    mp_mutex_unlock(&log->root->lock);
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

static inline int term_msg_fileno(struct mp_log_root *root, int lev)
{
    return root->force_stderr ? STDERR_FILENO : STDOUT_FILENO;
}

static inline FILE *term_msg_fp(struct mp_log_root *root, int lev)
{
    return term_msg_fileno(root, lev) == STDERR_FILENO ? stderr : stdout;
}

static inline bool is_status_output(struct mp_log_root *root, int lev)
{
    if (lev == MSGL_STATUS)
        return true;
    int msg_out = term_msg_fileno(root, lev);
    int status_out = term_msg_fileno(root, MSGL_STATUS);
    if (msg_out != status_out && root->isatty[msg_out] != root->isatty[status_out])
        return false;
    return true;
}

// Reposition cursor and clear lines for outputting the status line. In certain
// cases, like term OSD and subtitle display, the status can consist of
// multiple lines.
static void prepare_prefix(struct mp_log_root *root, bstr *out, int lev, int term_lines)
{
    int new_lines = lev == MSGL_STATUS ? term_lines : 0;
    out->len = 0;

    if (!is_status_output(root, lev))
        return;

    if (!root->isatty[term_msg_fileno(root, lev)]) {
        if (root->status_lines)
            bstr_xappend(root, out, bstr0("\n"));
        root->status_lines = new_lines;
        return;
    }

    // Set cursor state
    if (new_lines && !root->status_lines) {
        bstr_xappend(root, out, bstr0(TERM_ESC_HIDE_CURSOR));
    } else if (!new_lines && root->status_lines) {
        bstr_xappend(root, out, bstr0(TERM_ESC_RESTORE_CURSOR));
    }

    int line_skip = 0;
    if (root->status_lines) {
        // Clear previous status line
        bstr_xappend(root, out, bstr0("\033[1K\r"));
        bstr up_clear = bstr0("\033[A\033[K");
        for (int i = 1; i < root->status_lines; ++i)
            bstr_xappend(root, out, up_clear);
        assert(root->status_lines > 0 && root->blank_lines >= root->status_lines);
        line_skip = root->blank_lines - root->status_lines;
    }

    if (new_lines)
        line_skip -= MPMAX(0, root->blank_lines - new_lines);

    if (line_skip)
        bstr_xappend_asprintf(root, out, line_skip > 0 ? "\033[%dA" : "\033[%dB", abs(line_skip));

    root->blank_lines = MPMAX(0, root->blank_lines - term_lines);
    root->status_lines = new_lines;
    root->blank_lines += root->status_lines;
}

static void msg_flush_status_line(struct mp_log_root *root, bool clear)
{
    if (!root->status_lines)
        goto done;

    FILE *fp = term_msg_fp(root, MSGL_STATUS);
    if (!clear) {
        if (root->isatty[term_msg_fileno(root, MSGL_STATUS)])
            fprintf(fp, TERM_ESC_RESTORE_CURSOR);
        fprintf(fp, "\n");
        root->blank_lines = 0;
        root->status_lines = 0;
        goto done;
    }

    bstr term_msg = {0};
    prepare_prefix(root, &term_msg, MSGL_STATUS, 0);
    if (term_msg.len) {
        fprintf(fp, "%.*s", BSTR_P(term_msg));
        talloc_free(term_msg.start);
    }

done:
    root->status_line.len = 0;
}

void mp_msg_flush_status_line(struct mp_log *log, bool clear)
{
    if (!log->root)
        return;

    mp_mutex_lock(&log->root->lock);
    msg_flush_status_line(log->root, clear);
    mp_mutex_unlock(&log->root->lock);
}

void mp_msg_set_term_title(struct mp_log *log, const char *title)
{
    if (log->root && title) {
        // Lock because printf to terminal is not necessarily atomic.
        mp_mutex_lock(&log->root->lock);
        fprintf(term_msg_fp(log->root, MSGL_STATUS), "\033]0;%s\007", title);
        mp_mutex_unlock(&log->root->lock);
    }
}

bool mp_msg_has_status_line(struct mpv_global *global)
{
    struct mp_log_root *root = global->log->root;
    mp_mutex_lock(&root->lock);
    bool r = root->status_lines > 0;
    mp_mutex_unlock(&root->lock);
    return r;
}

static void set_term_color(void *talloc_ctx, bstr *text, int c)
{
    if (c == -1) {
        bstr_xappend(talloc_ctx, text, bstr0("\033[0m"));
        return;
    }

    bstr_xappend_asprintf(talloc_ctx, text, "\033[%d;3%dm", c >> 3, c & 7);
}

static void set_msg_color(void *talloc_ctx, bstr *text, int lev)
{
    static const int v_colors[] = {9, 1, 3, -1, -1, 2, 8, 8, 8, -1};
    set_term_color(talloc_ctx, text, v_colors[lev]);
}

static void pretty_print_module(struct mp_log_root *root, bstr *text,
                                const char *prefix, int lev)
{
    size_t prefix_len = strlen(prefix);
    root->module_indent = MPMAX(10, MPMAX(root->module_indent, prefix_len));
    bool color = root->color[term_msg_fileno(root, lev)];

    // Use random color based on the name of the module
    if (color) {
        unsigned int mod = 0;
        for (int i = 0; i < prefix_len; ++i)
            mod = mod * 33 + prefix[i];
        set_term_color(root, text, (mod + 1) % 15 + 1);
    }

    bstr_xappend_asprintf(root, text, "%*s", root->module_indent, prefix);
    if (color)
        set_term_color(root, text, -1);
    bstr_xappend(root, text, bstr0(": "));
    if (color)
        set_msg_color(root, text, lev);
}

static bool test_terminal_level(struct mp_log *log, int lev)
{
    return lev <= log->terminal_level && log->root->use_terminal &&
           !(lev == MSGL_STATUS && terminal_in_background());
}

// This is very basic way to infer needed width for a string.
static int term_disp_width(bstr str)
{
    int width = 0;

    while (str.len) {
        if (bstr_eatstart0(&str, "\033[")) {
            while (str.len && !((*str.start >= '@' && *str.start <= '~') || *str.start == 'm'))
                str = bstr_cut(str, 1);
            str = bstr_cut(str, 1);
            continue;
        }

        bstr code = bstr_split_utf8(str, &str);
        if (code.len == 0)
            return 0;

        if (code.len == 1 && *code.start == '\n')
            continue;

        // Only single-width characters are supported
        width++;

        // Assume that everything before \r should be discarded for simplicity
        if (code.len == 1 && *code.start == '\r')
            width = 0;
    }

    return width;
}

static void append_terminal_line(struct mp_log *log, int lev,
                                 bstr text, bstr *term_msg, int *line_w)
{
    struct mp_log_root *root = log->root;

    size_t start = term_msg->len;

    if (root->show_time)
        bstr_xappend_asprintf(root, term_msg, "[%10.6f] ", mp_time_sec());

    const char *log_prefix = (lev >= MSGL_V) || root->verbose || root->module
                                ? log->verbose_prefix : log->prefix;
    if (log_prefix) {
        if (root->module) {
            pretty_print_module(root, term_msg, log_prefix, lev);
        } else {
            bstr_xappend_asprintf(root, term_msg, "[%s] ", log_prefix);
        }
    }

    bstr_xappend(root, term_msg, text);
    *line_w = root->isatty[term_msg_fileno(root, lev)]
                ? term_disp_width(bstr_splice(*term_msg, start, term_msg->len)) : 0;
}

static struct mp_log_buffer_entry *log_buffer_read(struct mp_log_buffer *buffer)
{
    assert(buffer->num_entries);
    struct mp_log_buffer_entry *res = buffer->entries[buffer->entry0];
    buffer->entry0 = (buffer->entry0 + 1) % buffer->capacity;
    buffer->num_entries -= 1;
    return res;
}

static void write_msg_to_buffers(struct mp_log *log, int lev, bstr text)
{
    struct mp_log_root *root = log->root;
    for (int n = 0; n < root->num_buffers; n++) {
        struct mp_log_buffer *buffer = root->buffers[n];
        bool wakeup = false;
        mp_mutex_lock(&buffer->lock);
        int buffer_level = buffer->level;
        if (buffer_level == MP_LOG_BUFFER_MSGL_TERM)
            buffer_level = log->terminal_level;
        if (buffer_level == MP_LOG_BUFFER_MSGL_LOGFILE)
            buffer_level = MPMAX(log->terminal_level, MSGL_DEBUG);
        if (lev <= buffer_level && lev != MSGL_STATUS) {
            if (buffer->level == MP_LOG_BUFFER_MSGL_LOGFILE) {
                // If the buffer is full, block until we can write again,
                // unless there's no write thread (died, or early filebuffer)
                bool dead = false;
                while (buffer->num_entries == buffer->capacity && !dead) {
                    // Temporary unlock is OK; buffer->level is immutable, and
                    // buffer can't go away because the global log lock is held.
                    mp_mutex_unlock(&buffer->lock);
                    mp_mutex_lock(&root->log_file_lock);
                    if (root->log_file_thread_active) {
                        mp_cond_wait(&root->log_file_wakeup,
                                          &root->log_file_lock);
                    } else {
                        dead = true;
                    }
                    mp_mutex_unlock(&root->log_file_lock);
                    mp_mutex_lock(&buffer->lock);
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
                .text = bstrdup0(entry, text),
            };
            int pos = (buffer->entry0 + buffer->num_entries) % buffer->capacity;
            buffer->entries[pos] = entry;
            buffer->num_entries += 1;
            if (buffer->wakeup_cb && !buffer->silent)
                wakeup = true;
        }
        mp_mutex_unlock(&buffer->lock);
        if (wakeup)
            buffer->wakeup_cb(buffer->wakeup_cb_ctx);
    }
}

static void dump_stats(struct mp_log *log, int lev, bstr text)
{
    struct mp_log_root *root = log->root;
    if (lev == MSGL_STATS && root->stats_file)
        fprintf(root->stats_file, "%"PRId64" %.*s\n", mp_time_ns(), BSTR_P(text));
}

static void write_term_msg(struct mp_log *log, int lev, bstr text, bstr *out)
{
    struct mp_log_root *root = log->root;
    bool print_term = test_terminal_level(log, lev);
    int fileno = term_msg_fileno(root, lev);
    int term_w = 0, term_h = 0;
    if (print_term && root->isatty[fileno])
        terminal_get_size(&term_w, &term_h);

    out->len = 0;

    // Split away each line. Normally we require full lines; buffer partial
    // lines if they happen.
    root->term_msg_tmp.len = 0;
    int term_msg_lines = 0;

    bstr str = text;
    while (str.len) {
        bstr line = bstr_getline(str, &str);
        if (line.start[line.len - 1] != '\n') {
            assert(str.len == 0);
            str = line;
            break;
        }

        if (print_term) {
            int line_w;
            append_terminal_line(log, lev, line, &root->term_msg_tmp, &line_w);
            term_msg_lines += (!line_w || !term_w)
                                ? 1 : (line_w + term_w - 1) / term_w;
        }
        write_msg_to_buffers(log, lev, line);
    }

    if (lev == MSGL_STATUS) {
        int line_w = 0;
        if (str.len && print_term)
            append_terminal_line(log, lev, str, &root->term_msg_tmp, &line_w);
        term_msg_lines += !term_w ? (str.len ? 1 : 0)
                                  : (line_w + term_w - 1) / term_w;
    } else if (str.len) {
        bstr_xappend(NULL, &log->partial[lev], str);
    }

    if (print_term && (root->term_msg_tmp.len || lev == MSGL_STATUS)) {
        prepare_prefix(root, out, lev, term_msg_lines);
        if (root->color[fileno] && root->term_msg_tmp.len) {
            set_msg_color(root, out, lev);
            set_term_color(root, &root->term_msg_tmp, -1);
        }
        bstr_xappend(root, out, root->term_msg_tmp);
    }
}

void mp_msg_va(struct mp_log *log, int lev, const char *format, va_list va)
{
    if (!mp_msg_test(log, lev))
        return; // do not display

    struct mp_log_root *root = log->root;

    mp_mutex_lock(&root->lock);

    root->buffer.len = 0;

    if (log->partial[lev].len)
        bstr_xappend(root, &root->buffer, log->partial[lev]);
    log->partial[lev].len = 0;

    bstr_xappend_vasprintf(root, &root->buffer, format, va);

    // Remember last status message and restore it to ensure that it is
    // always displayed
    if (lev == MSGL_STATUS) {
        root->status_log = log;
        root->status_line.len = 0;
        // Use bstr_xappend instead bstrdup to reuse allocated memory
        if (root->buffer.len)
            bstr_xappend(root, &root->status_line, root->buffer);
    }

    if (lev == MSGL_STATS) {
        dump_stats(log, lev, root->buffer);
    } else if (lev == MSGL_STATUS && !test_terminal_level(log, lev)) {
        /* discard */
    } else {
        write_term_msg(log, lev, root->buffer, &root->term_msg);

        root->term_status_msg.len = 0;
        if (lev != MSGL_STATUS && root->status_line.len && root->status_log &&
            is_status_output(root, lev) && test_terminal_level(root->status_log, MSGL_STATUS))
        {
            write_term_msg(root->status_log, MSGL_STATUS, root->status_line,
                           &root->term_status_msg);
        }

        FILE *stream = term_msg_fp(root, lev);
        if (root->term_msg.len) {
            fwrite(root->term_msg.start, root->term_msg.len, 1, stream);
            if (root->term_status_msg.len)
                fwrite(root->term_status_msg.start, root->term_status_msg.len, 1, stream);
            fflush(stream);
        }
    }

    mp_mutex_unlock(&root->lock);
}

static void destroy_log(void *ptr)
{
    struct mp_log *log = ptr;
    // This is not managed via talloc itself, because mp_msg calls must be
    // thread-safe, while talloc is not thread-safe.
    for (int lvl = 0; lvl <= MSGL_MAX; ++lvl)
        talloc_free(log->partial[lvl].start);
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
        .reload_counter = 1,
    };

    mp_mutex_init(&root->lock);
    mp_mutex_init(&root->log_file_lock);
    mp_cond_init(&root->log_file_wakeup);

    struct mp_log dummy = { .root = root };
    struct mp_log *log = mp_log_new(root, &dummy, "");

    global->log = log;
}

static MP_THREAD_VOID log_file_thread(void *p)
{
    struct mp_log_root *root = p;

    mp_thread_set_name("log");

    mp_mutex_lock(&root->log_file_lock);

    while (root->log_file_thread_active) {
        struct mp_log_buffer_entry *e =
            mp_msg_log_buffer_read(root->log_file_buffer);
        if (e) {
            mp_mutex_unlock(&root->log_file_lock);
            fprintf(root->log_file, "[%8.3f][%c][%s] %s",
                    mp_time_sec(),
                    mp_log_levels[e->level][0], e->prefix, e->text);
            fflush(root->log_file);
            mp_mutex_lock(&root->log_file_lock);
            talloc_free(e);
            // Multiple threads might be blocked if the log buffer was full.
            mp_cond_broadcast(&root->log_file_wakeup);
        } else {
            mp_cond_wait(&root->log_file_wakeup, &root->log_file_lock);
        }
    }

    mp_mutex_unlock(&root->log_file_lock);

    MP_THREAD_RETURN();
}

static void wakeup_log_file(void *p)
{
    struct mp_log_root *root = p;

    mp_mutex_lock(&root->log_file_lock);
    mp_cond_broadcast(&root->log_file_wakeup);
    mp_mutex_unlock(&root->log_file_lock);
}

// Only to be called from the main thread.
static void terminate_log_file_thread(struct mp_log_root *root)
{
    bool wait_terminate = false;

    mp_mutex_lock(&root->log_file_lock);
    if (root->log_file_thread_active) {
        root->log_file_thread_active = false;
        mp_cond_broadcast(&root->log_file_wakeup);
        wait_terminate = true;
    }
    mp_mutex_unlock(&root->log_file_lock);

    if (wait_terminate)
        mp_thread_join(root->log_file_thread);

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
        if (new_path[0])
            *current_path = talloc_strdup(NULL, new_path);
        res = true;
    }

    talloc_free(tmp);

    return res;
}

void mp_msg_update_msglevels(struct mpv_global *global, struct MPOpts *opts)
{
    struct mp_log_root *root = global->log->root;

    mp_mutex_lock(&root->lock);

    root->verbose = opts->verbose;
    root->really_quiet = opts->msg_really_quiet;
    root->module = opts->msg_module;
    root->use_terminal = opts->use_terminal;
    root->show_time = opts->msg_time;

    if (root->really_quiet)
        root->status_lines = 0;

    for (int i = STDOUT_FILENO; i <= STDERR_FILENO && root->use_terminal; ++i) {
        root->isatty[i] = isatty(i);
        root->color[i] = opts->msg_color && root->isatty[i];
    }

    m_option_type_msglevels.free(&root->msg_levels);
    m_option_type_msglevels.copy(NULL, &root->msg_levels, &opts->msg_levels);

    atomic_fetch_add(&root->reload_counter, 1);
    mp_mutex_unlock(&root->lock);

    if (check_new_path(global, opts->log_file, &root->log_path)) {
        terminate_log_file_thread(root);
        if (root->log_path) {
            root->log_file = fopen(root->log_path, "wb");
            if (root->log_file) {

                // if a logfile is created and the early filebuf still exists,
                // flush and destroy the early buffer
                mp_mutex_lock(&root->lock);
                struct mp_log_buffer *earlybuf = root->early_filebuffer;
                if (earlybuf)
                    root->early_filebuffer = NULL;  // but it still logs msgs
                mp_mutex_unlock(&root->lock);

                if (earlybuf) {
                    // flush, destroy before creating the normal logfile buf,
                    // as once the new one is created (specifically, its write
                    // thread), then MSGL_LOGFILE messages become blocking, but
                    // the early logfile buf is without dequeue - can deadlock.
                    // note: timestamp is unknown, we use 0.000 as indication.
                    // note: new messages while iterating are still flushed.
                    struct mp_log_buffer_entry *e;
                    while ((e = mp_msg_log_buffer_read(earlybuf))) {
                        fprintf(root->log_file, "[%8.3f][%c][%s] %s", 0.0,
                                mp_log_levels[e->level][0], e->prefix, e->text);
                        talloc_free(e);
                    }
                    mp_msg_log_buffer_destroy(earlybuf);  // + remove from root
                }

                root->log_file_buffer =
                    mp_msg_log_buffer_new(global, FILE_BUF, MP_LOG_BUFFER_MSGL_LOGFILE,
                                          wakeup_log_file, root);
                root->log_file_thread_active = true;
                if (mp_thread_create(&root->log_file_thread, log_file_thread,
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

        mp_mutex_lock(&root->lock);
        if (root->stats_file)
            fclose(root->stats_file);
        root->stats_file = NULL;
        if (root->stats_path) {
            root->stats_file = fopen(root->stats_path, "wb");
            open_error = !root->stats_file;
        }
        mp_mutex_unlock(&root->lock);

        if (open_error) {
            mp_err(global->log, "Failed to open stats file '%s'\n",
                   root->stats_path);
        }
    }
}

void mp_msg_force_stderr(struct mpv_global *global, bool force_stderr)
{
    struct mp_log_root *root = global->log->root;

    mp_mutex_lock(&root->lock);
    root->force_stderr = force_stderr;
    mp_mutex_unlock(&root->lock);
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
    mp_msg_flush_status_line(global->log, true);
    if (root->really_quiet && root->isatty[term_msg_fileno(root, MSGL_STATUS)])
        fprintf(term_msg_fp(root, MSGL_STATUS), TERM_ESC_RESTORE_CURSOR);
    terminate_log_file_thread(root);
    mp_msg_log_buffer_destroy(root->early_buffer);
    mp_msg_log_buffer_destroy(root->early_filebuffer);
    assert(root->num_buffers == 0);
    if (root->stats_file)
        fclose(root->stats_file);
    talloc_free(root->stats_path);
    talloc_free(root->log_path);
    m_option_type_msglevels.free(&root->msg_levels);
    mp_mutex_destroy(&root->lock);
    mp_mutex_destroy(&root->log_file_lock);
    mp_cond_destroy(&root->log_file_wakeup);
    talloc_free(root);
    global->log = NULL;
}

// early logging store log messages before they have a known destination.
// there are two early log buffers which are similar logically, and both cease
// function (if still exist, independently) once the log destination is known,
// or mpv init is complete (typically, after all clients/scripts init is done).
//
// - "normal" early_buffer, holds early terminal-level logs, and is handed over
//   to the first client which requests such log buffer, so that it sees older
//   messages too. further clients which request a log buffer get a new one
//   which accumulates messages starting at this point in time.
//
// - early_filebuffer - early log-file messages until a log file name is known.
//   main cases where meaningful messages are accumulated before the filename
//   is known are when log-file is set at mpv.conf, or from script/client init.
//   once a file name is known, the early buffer is flushed and destroyed.
//   unlike the "proper" log-file buffer, the early filebuffer is not backed by
//   a write thread, and hence non-blocking (can overwrite old messages).
//   it's also bigger than the actual file buffer (early: 5000, actual: 100).

static void mp_msg_set_early_logging_raw(struct mpv_global *global, bool enable,
                                         struct mp_log_buffer **root_logbuf,
                                         int size, int level)
{
    struct mp_log_root *root = global->log->root;
    mp_mutex_lock(&root->lock);

    if (enable != !!*root_logbuf) {
        if (enable) {
            mp_mutex_unlock(&root->lock);
            struct mp_log_buffer *buf =
                mp_msg_log_buffer_new(global, size, level, NULL, NULL);
            mp_mutex_lock(&root->lock);
            assert(!*root_logbuf); // no concurrent calls to this function
            *root_logbuf = buf;
        } else {
            struct mp_log_buffer *buf = *root_logbuf;
            *root_logbuf = NULL;
            mp_mutex_unlock(&root->lock);
            mp_msg_log_buffer_destroy(buf);
            return;
        }
    }

    mp_mutex_unlock(&root->lock);
}

void mp_msg_set_early_logging(struct mpv_global *global, bool enable)
{
    struct mp_log_root *root = global->log->root;

    mp_msg_set_early_logging_raw(global, enable, &root->early_buffer,
                                 EARLY_TERM_BUF, MP_LOG_BUFFER_MSGL_TERM);

    // normally MSGL_LOGFILE buffer gets a write thread, but not the early buf
    mp_msg_set_early_logging_raw(global, enable, &root->early_filebuffer,
                                 EARLY_FILE_BUF, MP_LOG_BUFFER_MSGL_LOGFILE);
}

struct mp_log_buffer *mp_msg_log_buffer_new(struct mpv_global *global,
                                            int size, int level,
                                            void (*wakeup_cb)(void *ctx),
                                            void *wakeup_cb_ctx)
{
    struct mp_log_root *root = global->log->root;

    mp_mutex_lock(&root->lock);

    if (level == MP_LOG_BUFFER_MSGL_TERM) {
        // The first thing which creates a terminal-level log buffer gets the
        // early log buffer, if it exists. This is supposed to enable a script
        // to grab log messages from before it was initialized. It's OK that
        // this works only for 1 script and only once.
        if (root->early_buffer) {
            struct mp_log_buffer *buffer = root->early_buffer;
            root->early_buffer = NULL;
            mp_msg_log_buffer_resize(buffer, size);
            buffer->wakeup_cb = wakeup_cb;
            buffer->wakeup_cb_ctx = wakeup_cb_ctx;
            mp_mutex_unlock(&root->lock);
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

    mp_mutex_init(&buffer->lock);

    MP_TARRAY_APPEND(root, root->buffers, root->num_buffers, buffer);

    atomic_fetch_add(&root->reload_counter, 1);
    mp_mutex_unlock(&root->lock);

    return buffer;
}

void mp_msg_log_buffer_resize(struct mp_log_buffer *buffer, int size)
{
    mp_mutex_lock(&buffer->lock);

    assert(size > 0);
    if (buffer->capacity < size &&
        buffer->entry0 + buffer->num_entries <= buffer->capacity) {
        // shortcut if buffer doesn't wrap
        buffer->entries = talloc_realloc(buffer, buffer->entries,
                                         struct mp_log_buffer_entry *, size);
    } else if (buffer->capacity != size) {
        struct mp_log_buffer_entry **entries =
            talloc_array(buffer, struct mp_log_buffer_entry *, size);
        int num_entries = 0;
        for (int i = buffer->num_entries - 1; i >= 0; i--) {
            int entry = (buffer->entry0 + i) % buffer->num_entries;
            struct mp_log_buffer_entry *res = buffer->entries[entry];
            if (num_entries < size) {
                entries[num_entries++] = res;
            } else {
                talloc_free(res);
                buffer->dropped += 1;
            }
        }
        talloc_free(buffer->entries);
        buffer->entries = entries;
        buffer->entry0 = 0;
        buffer->num_entries = num_entries;
    }
    buffer->capacity = size;

    mp_mutex_unlock(&buffer->lock);
}

void mp_msg_log_buffer_set_silent(struct mp_log_buffer *buffer, bool silent)
{
    mp_mutex_lock(&buffer->lock);
    buffer->silent = silent;
    mp_mutex_unlock(&buffer->lock);
}

void mp_msg_log_buffer_destroy(struct mp_log_buffer *buffer)
{
    if (!buffer)
        return;

    struct mp_log_root *root = buffer->root;

    mp_mutex_lock(&root->lock);

    for (int n = 0; n < root->num_buffers; n++) {
        if (root->buffers[n] == buffer) {
            MP_TARRAY_REMOVE_AT(root->buffers, root->num_buffers, n);
            goto found;
        }
    }

    MP_ASSERT_UNREACHABLE();

found:

    while (buffer->num_entries)
        talloc_free(log_buffer_read(buffer));

    mp_mutex_destroy(&buffer->lock);
    talloc_free(buffer);

    atomic_fetch_add(&root->reload_counter, 1);
    mp_mutex_unlock(&root->lock);
}

// Return a queued message, or if the buffer is empty, NULL.
// Thread-safety: one buffer can be read by a single thread only.
struct mp_log_buffer_entry *mp_msg_log_buffer_read(struct mp_log_buffer *buffer)
{
    struct mp_log_buffer_entry *res = NULL;

    mp_mutex_lock(&buffer->lock);

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

    mp_mutex_unlock(&buffer->lock);

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
        if (mp_log_levels[n] && !strcasecmp(s, mp_log_levels[n]))
            return n;
    }
    return -1;
}
