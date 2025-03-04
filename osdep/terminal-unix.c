/*
 * Based on GyS-TermIO v2.0 (for GySmail v3) (copyright (C) 1999 A'rpi/ESP-team)
 *
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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <assert.h>

#include <termios.h>
#include <unistd.h>

#include "osdep/io.h"
#include "osdep/threads.h"
#include "osdep/poll_wrapper.h"

#include "common/common.h"
#include "misc/bstr.h"
#include "input/input.h"
#include "input/keycodes.h"
#include "misc/ctype.h"
#include "terminal.h"

// Timeout in ms after which the (normally ambiguous) ESC key is detected.
#define ESC_TIMEOUT 100

// Timeout in ms after which the poll for input is aborted. The FG/BG state is
// tested before every wait, and a positive value allows reactivating input
// processing when mpv is brought to the foreground while it was running in the
// background. In such a situation, an infinite timeout (-1) will keep mpv
// waiting for input without realizing the terminal state changed - and thus
// buffer all keypresses until ENTER is pressed.
#define INPUT_TIMEOUT 1000

static struct termios tio_orig;

static int tty_in = -1, tty_out = -1;

enum entry_type {
    ENTRY_TYPE_KEY = 0,
    ENTRY_TYPE_MOUSE_BUTTON,
    ENTRY_TYPE_MOUSE_MOVE,
};

struct key_entry {
    const char *seq;
    int mpkey;
    // If this is not NULL, then if seq is matched as unique prefix, the
    // existing sequence is replaced by the following string. Matching
    // continues normally, and mpkey is or-ed into the final result.
    const char *replace;
    // Extend the match length by a certain length, so the contents
    // after the match can be processed with custom logic.
    int skip;
    enum entry_type type;
};

static const struct key_entry keys[] = {
    {"\010", MP_KEY_BS},
    {"\011", MP_KEY_TAB},
    {"\012", MP_KEY_ENTER},
    {"\177", MP_KEY_BS},

    {"\033[1~", MP_KEY_HOME},
    {"\033[2~", MP_KEY_INS},
    {"\033[3~", MP_KEY_DEL},
    {"\033[4~", MP_KEY_END},
    {"\033[5~", MP_KEY_PGUP},
    {"\033[6~", MP_KEY_PGDWN},
    {"\033[7~", MP_KEY_HOME},
    {"\033[8~", MP_KEY_END},

    {"\033[11~", MP_KEY_F+1},
    {"\033[12~", MP_KEY_F+2},
    {"\033[13~", MP_KEY_F+3},
    {"\033[14~", MP_KEY_F+4},
    {"\033[15~", MP_KEY_F+5},
    {"\033[17~", MP_KEY_F+6},
    {"\033[18~", MP_KEY_F+7},
    {"\033[19~", MP_KEY_F+8},
    {"\033[20~", MP_KEY_F+9},
    {"\033[21~", MP_KEY_F+10},
    {"\033[23~", MP_KEY_F+11},
    {"\033[24~", MP_KEY_F+12},

    {"\033OA", MP_KEY_UP},
    {"\033OB", MP_KEY_DOWN},
    {"\033OC", MP_KEY_RIGHT},
    {"\033OD", MP_KEY_LEFT},
    {"\033[A", MP_KEY_UP},
    {"\033[B", MP_KEY_DOWN},
    {"\033[C", MP_KEY_RIGHT},
    {"\033[D", MP_KEY_LEFT},
    {"\033[E", MP_KEY_KPBEGIN},
    {"\033[F", MP_KEY_END},
    {"\033[H", MP_KEY_HOME},

    {"\033[[A", MP_KEY_F+1},
    {"\033[[B", MP_KEY_F+2},
    {"\033[[C", MP_KEY_F+3},
    {"\033[[D", MP_KEY_F+4},
    {"\033[[E", MP_KEY_F+5},

    {"\033OE", MP_KEY_KP5}, // mintty?
    {"\033OM", MP_KEY_KPENTER},
    {"\033OP", MP_KEY_F+1},
    {"\033OQ", MP_KEY_F+2},
    {"\033OR", MP_KEY_F+3},
    {"\033OS", MP_KEY_F+4},

    {"\033Oa", MP_KEY_UP | MP_KEY_MODIFIER_CTRL}, // urxvt
    {"\033Ob", MP_KEY_DOWN | MP_KEY_MODIFIER_CTRL},
    {"\033Oc", MP_KEY_RIGHT | MP_KEY_MODIFIER_CTRL},
    {"\033Od", MP_KEY_LEFT | MP_KEY_MODIFIER_CTRL},
    {"\033Oj", '*'}, // also keypad, but we don't have separate codes for them
    {"\033Ok", '+'},
    {"\033Om", '-'},
    {"\033On", MP_KEY_KPDEC},
    {"\033Oo", '/'},
    {"\033Op", MP_KEY_KP0},
    {"\033Oq", MP_KEY_KP1},
    {"\033Or", MP_KEY_KP2},
    {"\033Os", MP_KEY_KP3},
    {"\033Ot", MP_KEY_KP4},
    {"\033Ou", MP_KEY_KP5},
    {"\033Ov", MP_KEY_KP6},
    {"\033Ow", MP_KEY_KP7},
    {"\033Ox", MP_KEY_KP8},
    {"\033Oy", MP_KEY_KP9},

    {"\033[a", MP_KEY_UP | MP_KEY_MODIFIER_SHIFT}, // urxvt
    {"\033[b", MP_KEY_DOWN | MP_KEY_MODIFIER_SHIFT},
    {"\033[c", MP_KEY_RIGHT | MP_KEY_MODIFIER_SHIFT},
    {"\033[d", MP_KEY_LEFT | MP_KEY_MODIFIER_SHIFT},
    {"\033[2^", MP_KEY_INS | MP_KEY_MODIFIER_CTRL},
    {"\033[3^", MP_KEY_DEL | MP_KEY_MODIFIER_CTRL},
    {"\033[5^", MP_KEY_PGUP | MP_KEY_MODIFIER_CTRL},
    {"\033[6^", MP_KEY_PGDWN | MP_KEY_MODIFIER_CTRL},
    {"\033[7^", MP_KEY_HOME | MP_KEY_MODIFIER_CTRL},
    {"\033[8^", MP_KEY_END | MP_KEY_MODIFIER_CTRL},

    {"\033[1;2", MP_KEY_MODIFIER_SHIFT, .replace = "\033["}, // xterm
    {"\033[1;3", MP_KEY_MODIFIER_ALT, .replace = "\033["},
    {"\033[1;5", MP_KEY_MODIFIER_CTRL, .replace = "\033["},
    {"\033[1;4", MP_KEY_MODIFIER_ALT | MP_KEY_MODIFIER_SHIFT, .replace = "\033["},
    {"\033[1;6", MP_KEY_MODIFIER_CTRL | MP_KEY_MODIFIER_SHIFT, .replace = "\033["},
    {"\033[1;7", MP_KEY_MODIFIER_CTRL | MP_KEY_MODIFIER_ALT, .replace = "\033["},
    {"\033[1;8",
     MP_KEY_MODIFIER_CTRL | MP_KEY_MODIFIER_ALT | MP_KEY_MODIFIER_SHIFT,
     .replace = "\033["},

    {"\033[29~", MP_KEY_MENU},
    {"\033[Z", MP_KEY_TAB | MP_KEY_MODIFIER_SHIFT},

    // Mouse button inputs. 2 bytes of position information requires special processing.
    {"\033[M ", MP_MBTN_LEFT | MP_KEY_STATE_DOWN, .skip = 2, .type = ENTRY_TYPE_MOUSE_BUTTON},
    {"\033[M!", MP_MBTN_MID | MP_KEY_STATE_DOWN, .skip = 2, .type = ENTRY_TYPE_MOUSE_BUTTON},
    {"\033[M\"", MP_MBTN_RIGHT | MP_KEY_STATE_DOWN, .skip = 2, .type = ENTRY_TYPE_MOUSE_BUTTON},
    {"\033[M#", MP_INPUT_RELEASE_ALL, .skip = 2, .type = ENTRY_TYPE_MOUSE_BUTTON},
    {"\033[M`", MP_WHEEL_UP, .skip = 2, .type = ENTRY_TYPE_MOUSE_BUTTON},
    {"\033[Ma", MP_WHEEL_DOWN, .skip = 2, .type = ENTRY_TYPE_MOUSE_BUTTON},
    // Mouse move inputs. No key events should be generated for them.
    {"\033[M@", MP_MBTN_LEFT | MP_KEY_STATE_DOWN, .skip = 2, .type = ENTRY_TYPE_MOUSE_MOVE},
    {"\033[MA", MP_MBTN_MID | MP_KEY_STATE_DOWN, .skip = 2, .type = ENTRY_TYPE_MOUSE_MOVE},
    {"\033[MB", MP_MBTN_RIGHT | MP_KEY_STATE_DOWN, .skip = 2, .type = ENTRY_TYPE_MOUSE_MOVE},
    {"\033[MC", MP_INPUT_RELEASE_ALL, .skip = 2, .type = ENTRY_TYPE_MOUSE_MOVE},
    {0}
};

#define BUF_LEN 256

struct termbuf {
    unsigned char b[BUF_LEN];
    int len;
    int mods;
};

static void skip_buf(struct termbuf *b, unsigned int count)
{
    mp_assert(count <= b->len);

    memmove(&b->b[0], &b->b[count], b->len - count);
    b->len -= count;
    b->mods = 0;
}

static struct termbuf buf;

static void process_input(struct input_ctx *input_ctx, bool timeout)
{
    while (buf.len) {
        // Lone ESC is ambiguous, so accept it only after a timeout.
        if (timeout &&
            ((buf.len == 1 && buf.b[0] == '\033') ||
             (buf.len > 1 && buf.b[0] == '\033' && buf.b[1] == '\033')))
        {
            mp_input_put_key(input_ctx, MP_KEY_ESC);
            skip_buf(&buf, 1);
        }

        int utf8_len = bstr_parse_utf8_code_length(buf.b[0]);
        if (utf8_len > 1) {
            if (buf.len < utf8_len)
                goto read_more;

            mp_input_put_key_utf8(input_ctx, buf.mods, (bstr){buf.b, utf8_len});
            skip_buf(&buf, utf8_len);
            continue;
        }

        const struct key_entry *match = NULL; // may be a partial match
        for (int n = 0; keys[n].seq; n++) {
            const struct key_entry *e = &keys[n];
            if (memcmp(e->seq, buf.b, MPMIN(buf.len, strlen(e->seq))) == 0) {
                if (match)
                    goto read_more; /* need more bytes to disambiguate */
                match = e;
            }
        }

        if (!match) { // normal or unknown key
            int mods = 0;
            if (buf.b[0] == '\033') {
                if (buf.len > 1 && buf.b[1] == '[') {
                    // Throw away unrecognized mouse CSI sequences.
                    // Cannot be handled by the loop below since the bytes
                    // afterwards can be out of that range.
                    if (buf.len > 2 && buf.b[2] == 'M') {
                        skip_buf(&buf, buf.len);
                        continue;
                    }
                    // unknown CSI sequence. wait till it completes
                    for (int i = 2; i < buf.len; i++) {
                        if (buf.b[i] >= 0x40 && buf.b[i] <= 0x7E)  {
                            skip_buf(&buf, i+1);
                            continue;  // complete - throw it away
                        }
                    }
                    goto read_more;  // not yet complete
                }
                // non-CSI esc sequence
                skip_buf(&buf, 1);
                if (buf.len > 0 && buf.b[0] > 0 && buf.b[0] < 127) {
                    // meta+normal key
                    mods |= MP_KEY_MODIFIER_ALT;
                } else {
                    // Throw it away. Typically, this will be a complete,
                    // unsupported sequence, and dropping this will skip it.
                    skip_buf(&buf, buf.len);
                    continue;
                }
            }
            unsigned char c = buf.b[0];
            skip_buf(&buf, 1);
            if (c < 32) {
                // 1..26 is ^A..^Z, and 27..31 is ^3..^7
                c = c <= 26 ? (c + 'a' - 1) : (c + '3' - 27);
                mods |= MP_KEY_MODIFIER_CTRL;
            }
            mp_input_put_key(input_ctx, c | mods);
            continue;
        }

        int seq_len = strlen(match->seq) + match->skip;
        if (seq_len > buf.len)
            goto read_more; /* partial match */

        if (match->replace) {
            int rep = strlen(match->replace);
            mp_assert(rep <= seq_len);
            memcpy(buf.b, match->replace, rep);
            memmove(buf.b + rep, buf.b + seq_len, buf.len - seq_len);
            buf.len = rep + buf.len - seq_len;
            buf.mods |= match->mpkey;
            continue;
        }

        // Parse the initially skipped mouse position information.
        // The positions are 1-based character cell positions plus 32.
        // Treat mouse position as the pixel values at the center of the cell.
        if ((match->type == ENTRY_TYPE_MOUSE_BUTTON ||
             match->type == ENTRY_TYPE_MOUSE_MOVE) && seq_len >= 6)
        {
            int num_rows        = 80;
            int num_cols        = 25;
            int total_px_width  = 0;
            int total_px_height = 0;
            terminal_get_size2(&num_rows, &num_cols, &total_px_width, &total_px_height);
            mp_input_set_mouse_pos(input_ctx,
                (buf.b[4] - 32.5) * (total_px_width / num_cols),
                (buf.b[5] - 32.5) * (total_px_height / num_rows), false);
        }
        if (match->type != ENTRY_TYPE_MOUSE_MOVE)
            mp_input_put_key(input_ctx, buf.mods | match->mpkey);
        skip_buf(&buf, seq_len);
    }

read_more: ;  /* need more bytes */
}

static int getch2_active  = 0;
static int getch2_enabled = 0;
static bool read_terminal;

static void enable_kx(bool enable)
{
    // This check is actually always true, as enable_kx calls are all guarded
    // by read_terminal, which is true only if both stdin and stdout are a
    // tty. Note that stderr being redirected away has no influence over mpv's
    // I/O handling except for disabling the terminal OSD, and thus stderr
    // shouldn't be relied on here either.
    if (isatty(tty_out)) {
        char *cmd = enable ? "\033=" : "\033>";
        (void)write(tty_out, cmd, strlen(cmd));
    }
}

static void do_activate_getch2(void)
{
    if (getch2_active || !read_terminal)
        return;

    enable_kx(true);

    struct termios tio_new;
    tcgetattr(tty_in, &tio_new);

    tio_new.c_lflag &= ~(ICANON|ECHO); /* Clear ICANON and ECHO. */
    tio_new.c_cc[VMIN] = 1;
    tio_new.c_cc[VTIME] = 0;
    tcsetattr(tty_in, TCSANOW, &tio_new);

    getch2_active = 1;
}

static void do_deactivate_getch2(void)
{
    if (!getch2_active)
        return;

    enable_kx(false);
    tcsetattr(tty_in, TCSANOW, &tio_orig);

    getch2_active = 0;
}

// sigaction wrapper
static int setsigaction(int signo, void (*handler) (int),
                        int flags, bool do_mask)
{
    struct sigaction sa;
    sa.sa_handler = handler;

    if (do_mask)
        sigfillset(&sa.sa_mask);
    else
        sigemptyset(&sa.sa_mask);

    sa.sa_flags = flags | SA_RESTART;
    return sigaction(signo, &sa, NULL);
}

static void getch2_poll(void)
{
    if (!getch2_enabled)
        return;

    // check if stdin is in the foreground process group
    int newstatus = (tcgetpgrp(tty_in) == getpgrp());

    // and activate getch2 if it is, deactivate otherwise
    if (newstatus)
        do_activate_getch2();
    else
        do_deactivate_getch2();
}

static mp_thread input_thread;
static struct input_ctx *input_ctx;
static int death_pipe[2] = {-1, -1};
enum { PIPE_STOP, PIPE_CONT };
static int stop_cont_pipe[2] = {-1, -1};

static void stop_cont_sighandler(int signum)
{
    int saved_errno = errno;
    char sig = signum == SIGCONT ? PIPE_CONT : PIPE_STOP;
    (void)write(stop_cont_pipe[1], &sig, 1);
    errno = saved_errno;
}

static void safe_close(int *p)
{
    if (*p >= 0)
        close(*p);
    *p = -1;
}

static void close_sig_pipes(void)
{
    for (int n = 0; n < 2; n++) {
        safe_close(&death_pipe[n]);
        safe_close(&stop_cont_pipe[n]);
    }
}

static void close_tty(void)
{
    if (tty_in >= 0 && tty_in != STDIN_FILENO)
        close(tty_in);

    tty_in = tty_out = -1;
}

static void quit_request_sighandler(int signum)
{
    int saved_errno = errno;
    (void)write(death_pipe[1], &(char){1}, 1);
    errno = saved_errno;
}

static MP_THREAD_VOID terminal_thread(void *ptr)
{
    mp_thread_set_name("terminal/input");
    bool stdin_ok = read_terminal; // if false, we still wait for SIGTERM
    while (1) {
        getch2_poll();
        struct pollfd fds[3] = {
            { .events = POLLIN, .fd = death_pipe[0] },
            { .events = POLLIN, .fd = stop_cont_pipe[0] },
            { .events = POLLIN, .fd = tty_in }
        };
        /*
         * if the process isn't in foreground process group, then on macos
         * polldev() doesn't rest and gets into 100% cpu usage (see issue #11795)
         * with read() returning EIO. but we shouldn't quit on EIO either since
         * the process might be foregrounded later.
         *
         * so just avoid poll-ing tty_in when we know the process is not in the
         * foreground. there's a small race window, but the timeout will take
         * care of it so it's fine.
         */
        bool is_fg = tcgetpgrp(tty_in) == getpgrp();
        int r = polldev(fds, stdin_ok && is_fg ? 3 : 2, buf.len ? ESC_TIMEOUT : INPUT_TIMEOUT);
        if (fds[0].revents) {
            do_deactivate_getch2();
            break;
        }
        if (fds[1].revents & POLLIN) {
            int8_t c = -1;
            (void)read(stop_cont_pipe[0], &c, 1);
            if (c == PIPE_STOP) {
                do_deactivate_getch2();
                if (isatty(STDERR_FILENO)) {
                    (void)write(STDERR_FILENO, TERM_ESC_RESTORE_CURSOR,
                                sizeof(TERM_ESC_RESTORE_CURSOR) - 1);
                }
                // trying to reset SIGTSTP handler to default and raise it will
                // result in a race and there's no other way to invoke the
                // default handler. so just invoke SIGSTOP since it's
                // effectively the same thing.
                raise(SIGSTOP);
            } else if (c == PIPE_CONT) {
                getch2_poll();
            }
        }
        if (fds[2].revents) {
            int retval = read(tty_in, &buf.b[buf.len], BUF_LEN - buf.len);
            if (!retval || (retval == -1 && errno != EINTR && errno != EAGAIN && errno != EIO))
                break; // EOF/closed
            if (retval > 0) {
                buf.len += retval;
                process_input(input_ctx, false);
            }
        }
        if (r == 0)
            process_input(input_ctx, true);
    }
    char c;
    bool quit = read(death_pipe[0], &c, 1) == 1 && c == 1;
    // Important if we received SIGTERM, rather than regular quit.
    if (quit) {
        struct mp_cmd *cmd = mp_input_parse_cmd(input_ctx, bstr0("quit 4"), "");
        if (cmd)
            mp_input_queue_cmd(input_ctx, cmd);
    }
    MP_THREAD_RETURN();
}

void terminal_setup_getch(struct input_ctx *ictx)
{
    if (!getch2_enabled || input_ctx)
        return;

    if (mp_make_wakeup_pipe(death_pipe) < 0)
        return;

    // Disable reading from the terminal even if stdout is not a tty, to make
    //   mpv ... | less
    // do the right thing.
    read_terminal = isatty(tty_in) && isatty(STDOUT_FILENO);

    input_ctx = ictx;

    if (mp_thread_create(&input_thread, terminal_thread, NULL)) {
        input_ctx = NULL;
        close_sig_pipes();
        close_tty();
        return;
    }

    setsigaction(SIGINT,  quit_request_sighandler, SA_RESETHAND, false);
    setsigaction(SIGQUIT, quit_request_sighandler, 0, true);
    setsigaction(SIGTERM, quit_request_sighandler, 0, true);
}

void terminal_uninit(void)
{
    if (!getch2_enabled)
        return;

    // restore signals
    setsigaction(SIGCONT, SIG_DFL, 0, false);
    setsigaction(SIGTSTP, SIG_DFL, 0, false);
    setsigaction(SIGINT,  SIG_DFL, 0, false);
    setsigaction(SIGQUIT, SIG_DFL, 0, false);
    setsigaction(SIGTERM, SIG_DFL, 0, false);
    setsigaction(SIGTTIN, SIG_DFL, 0, false);
    setsigaction(SIGTTOU, SIG_DFL, 0, false);

    if (input_ctx) {
        (void)write(death_pipe[1], &(char){0}, 1);
        mp_thread_join(input_thread);
    }

    close_sig_pipes();
    input_ctx = NULL;

    do_deactivate_getch2();
    close_tty();

    getch2_enabled = 0;
    read_terminal = false;
}

bool terminal_in_background(void)
{
    return read_terminal && tcgetpgrp(STDERR_FILENO) != getpgrp();
}

void terminal_get_size(int *w, int *h)
{
    struct winsize ws;
    if (ioctl(tty_in, TIOCGWINSZ, &ws) < 0 || !ws.ws_row || !ws.ws_col)
        return;

    *w = ws.ws_col;
    *h = ws.ws_row;
}

void terminal_get_size2(int *rows, int *cols, int *px_width, int *px_height)
{
    struct winsize ws;
    if (ioctl(tty_in, TIOCGWINSZ, &ws) < 0 || !ws.ws_row || !ws.ws_col
                                           || !ws.ws_xpixel || !ws.ws_ypixel)
        return;

    *rows = ws.ws_row;
    *cols = ws.ws_col;
    *px_width = ws.ws_xpixel;
    *px_height = ws.ws_ypixel;
}

void terminal_set_mouse_input(bool enable)
{
    printf(enable ? TERM_ESC_ENABLE_MOUSE : TERM_ESC_DISABLE_MOUSE);
    fflush(stdout);
}

void terminal_init(void)
{
    mp_assert(!getch2_enabled);
    getch2_enabled = 1;

    if (mp_make_wakeup_pipe(stop_cont_pipe) < 0) {
        getch2_enabled = 0;
        return;
    }

    tty_in = tty_out = open("/dev/tty", O_RDWR | O_CLOEXEC);
    if (tty_in < 0) {
        tty_in = STDIN_FILENO;
        tty_out = STDOUT_FILENO;
    }

    tcgetattr(tty_in, &tio_orig);

    // handlers to fix terminal settings
    setsigaction(SIGCONT, stop_cont_sighandler, 0, true);
    setsigaction(SIGTSTP, stop_cont_sighandler, 0, true);
    setsigaction(SIGTTIN, SIG_IGN, 0, true);
    setsigaction(SIGTTOU, SIG_IGN, 0, true);

    getch2_poll();
}
