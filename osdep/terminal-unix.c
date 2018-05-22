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

#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <assert.h>

#include <termios.h>
#include <unistd.h>

#include "osdep/io.h"
#include "osdep/threads.h"
#include "osdep/polldev.h"

#include "common/common.h"
#include "misc/bstr.h"
#include "input/input.h"
#include "input/keycodes.h"
#include "misc/ctype.h"
#include "terminal.h"

static volatile struct termios tio_orig;
static volatile int tio_orig_set;

static int tty_in = -1, tty_out = -1;

struct key_entry {
    const char *seq;
    int mpkey;
    // If this is not NULL, then if seq is matched as unique prefix, the
    // existing sequence is replaced by the following string. Matching
    // continues normally, and mpkey is or-ed into the final result.
    const char *replace;
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

    {"\033[A", MP_KEY_UP},
    {"\033[B", MP_KEY_DOWN},
    {"\033[C", MP_KEY_RIGHT},
    {"\033[D", MP_KEY_LEFT},
    {"\033[E", MP_KEY_KP5},
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
    assert(count <= b->len);

    memmove(&b->b[0], &b->b[count], b->len - count);
    b->len -= count;
    b->mods = 0;
}

static struct termbuf buf;

static bool getch2(struct input_ctx *input_ctx)
{
    int retval = read(tty_in, &buf.b[buf.len], BUF_LEN - buf.len);
    /* Return false on EOF to stop running select() on the FD, as it'd
     * trigger all the time. Note that it's possible to get temporary
     * EOF on terminal if the user presses ctrl-d, but that shouldn't
     * happen if the terminal state change done in terminal_init()
     * works.
     */
    if (retval == 0)
        return false;
    if (retval == -1)
        return errno != EBADF && errno != EINVAL;
    buf.len += retval;

    while (buf.len) {
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
            if (buf.b[0] == '\033') {
                skip_buf(&buf, 1);
                if (buf.len > 0 && mp_isalnum(buf.b[0])) { // meta+normal key
                    mp_input_put_key(input_ctx, buf.b[0] | MP_KEY_MODIFIER_ALT);
                    skip_buf(&buf, 1);
                } else if (buf.len == 1 && buf.b[0] == '\033') {
                    mp_input_put_key(input_ctx, MP_KEY_ESC);
                    skip_buf(&buf, 1);
                } else {
                    // Throw it away. Typically, this will be a complete,
                    // unsupported sequence, and dropping this will skip it.
                    skip_buf(&buf, buf.len);
                }
            } else {
                mp_input_put_key(input_ctx, buf.b[0]);
                skip_buf(&buf, 1);
            }
            continue;
        }

        int seq_len = strlen(match->seq);
        if (seq_len > buf.len)
            goto read_more; /* partial match */

        if (match->replace) {
            int rep = strlen(match->replace);
            assert(rep <= seq_len);
            memcpy(buf.b, match->replace, rep);
            memmove(buf.b + rep, buf.b + seq_len, buf.len - seq_len);
            buf.len = rep + buf.len - seq_len;
            buf.mods |= match->mpkey;
            continue;
        }

        mp_input_put_key(input_ctx, buf.mods | match->mpkey);
        skip_buf(&buf, seq_len);
    }

read_more:  /* need more bytes */
    return true;
}

static volatile int getch2_active  = 0;
static volatile int getch2_enabled = 0;
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
    tcgetattr(tty_in,&tio_new);

    if (!tio_orig_set) {
        tio_orig = tio_new;
        tio_orig_set = 1;
    }

    tio_new.c_lflag &= ~(ICANON|ECHO); /* Clear ICANON and ECHO. */
    tio_new.c_cc[VMIN] = 1;
    tio_new.c_cc[VTIME] = 0;
    tcsetattr(tty_in,TCSANOW,&tio_new);

    getch2_active = 1;
}

static void do_deactivate_getch2(void)
{
    if (!getch2_active)
        return;

    enable_kx(false);

    if (tio_orig_set) {
        // once set, it will never be set again
        // so we can cast away volatile here
        tcsetattr(tty_in, TCSANOW, (const struct termios *) &tio_orig);
    }

    getch2_active = 0;
}

// sigaction wrapper
static int setsigaction(int signo, void (*handler) (int),
                        int flags, bool do_mask)
{
    struct sigaction sa;
    sa.sa_handler = handler;

    if(do_mask)
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

static void stop_sighandler(int signum)
{
    do_deactivate_getch2();

    // note: for this signal, we use SA_RESETHAND but do NOT mask signals
    // so this will invoke the default handler
    raise(SIGTSTP);
}

static void continue_sighandler(int signum)
{
    // SA_RESETHAND has reset SIGTSTP, so we need to restore it here
    setsigaction(SIGTSTP, stop_sighandler, SA_RESETHAND, false);

    getch2_poll();
}

static pthread_t input_thread;
static struct input_ctx *input_ctx;
static int death_pipe[2] = {-1, -1};

static void close_death_pipe(void)
{
    for (int n = 0; n < 2; n++) {
        if (death_pipe[n] >= 0)
            close(death_pipe[n]);
        death_pipe[n] = -1;
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
    do_deactivate_getch2();

    (void)write(death_pipe[1], &(char){1}, 1);
}

static void *terminal_thread(void *ptr)
{
    mpthread_set_name("terminal");
    bool stdin_ok = read_terminal; // if false, we still wait for SIGTERM
    while (1) {
        getch2_poll();
        struct pollfd fds[2] = {
            { .events = POLLIN, .fd = death_pipe[0] },
            { .events = POLLIN, .fd = tty_in }
        };
        polldev(fds, stdin_ok ? 2 : 1, -1);
        if (fds[0].revents)
            break;
        if (fds[1].revents) {
            if (!getch2(input_ctx))
                break;
        }
    }
    char c;
    bool quit = read(death_pipe[0], &c, 1) == 1 && c == 1;
    // Important if we received SIGTERM, rather than regular quit.
    if (quit) {
        struct mp_cmd *cmd = mp_input_parse_cmd(input_ctx, bstr0("quit 4"), "");
        if (cmd)
            mp_input_queue_cmd(input_ctx, cmd);
    }
    return NULL;
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

    if (pthread_create(&input_thread, NULL, terminal_thread, NULL)) {
        input_ctx = NULL;
        close_death_pipe();
        close_tty();
        return;
    }

    setsigaction(SIGINT,  quit_request_sighandler, SA_RESETHAND, false);
    setsigaction(SIGQUIT, quit_request_sighandler, SA_RESETHAND, false);
    setsigaction(SIGTERM, quit_request_sighandler, SA_RESETHAND, false);
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
        pthread_join(input_thread, NULL);
        close_death_pipe();
        input_ctx = NULL;
    }

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

void terminal_init(void)
{
    assert(!getch2_enabled);
    getch2_enabled = 1;

    tty_in = tty_out = open("/dev/tty", O_RDWR | O_CLOEXEC);
    if (tty_in < 0) {
        tty_in = STDIN_FILENO;
        tty_out = STDOUT_FILENO;
    }

    // handlers to fix terminal settings
    setsigaction(SIGCONT, continue_sighandler, 0, true);
    setsigaction(SIGTSTP, stop_sighandler, SA_RESETHAND, false);
    setsigaction(SIGTTIN, SIG_IGN, 0, true);
    setsigaction(SIGTTOU, SIG_IGN, 0, true);

    getch2_poll();
}
