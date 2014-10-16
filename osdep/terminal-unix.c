/*
 * GyS-TermIO v2.0 (for GySmail v3)
 * a very small replacement of ncurses library
 *
 * copyright (C) 1999 A'rpi/ESP-team
 *
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

#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <assert.h>

#if HAVE_TERMIOS
#if HAVE_TERMIOS_H
#include <termios.h>
#endif
#if HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif
#endif

#include <unistd.h>
#include <poll.h>

#include "osdep/io.h"

#include "common/common.h"
#include "misc/bstr.h"
#include "input/input.h"
#include "input/keycodes.h"
#include "misc/ctype.h"
#include "terminal.h"

#if HAVE_TERMIOS
static volatile struct termios tio_orig;
static volatile int tio_orig_set;
#endif

#if !(HAVE_TERMINFO || HAVE_TERMCAP)

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
    int retval = read(0, &buf.b[buf.len], BUF_LEN - buf.len);
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

static void load_termcap(void)
{
}

static void enable_kx(bool enable)
{
    if (isatty(STDOUT_FILENO)) {
        char *cmd = enable ? "\033=" : "\033>";
        printf("%s", cmd);
        fflush(stdout);
    }
}

#else /* terminfo/termcap */

typedef struct {
    char *cap;
    int len;
    int code;
    char chars[8];
} keycode_st;

typedef struct {
    keycode_st *map;
    int len;
    int cap;
} keycode_map;

static keycode_map getch2_keys;

static char *term_rmkx = NULL;
static char *term_smkx = NULL;

#if HAVE_TERMINFO
#include <curses.h>
#endif
#include <term.h>

static keycode_st *keys_push(char *p, int code) {
    if (strlen(p) > 8)
        return NULL;

    if (getch2_keys.len == getch2_keys.cap) {
        getch2_keys.cap *= 2;
        if (getch2_keys.cap == 0)
            getch2_keys.cap = 32;

        getch2_keys.map = realloc(getch2_keys.map, sizeof(keycode_st) * getch2_keys.cap);
    }

    keycode_st *st = &getch2_keys.map[getch2_keys.len++];
    st->cap = NULL;
    st->len = strlen(p);
    st->code = code;
    strncpy(st->chars, p, 8);

    return st;
}

static int keys_count_matches(char *buf, int buflen) {
    int count = 0;
    if (buflen < 0)
        buflen = strlen(buf);

    for (int i = 0; i < getch2_keys.len; i++) {
        keycode_st *st = &getch2_keys.map[i];
        int len = MPMIN(buflen, st->len);

        if (memcmp(buf, st->chars, len) == 0)
            count++;
    }
    return count;
}

static keycode_st *keys_search(char *buf, int buflen) {
    if (buflen < 0)
        buflen = strlen(buf);

    for (int i = 0; i < getch2_keys.len; i++) {
        keycode_st *st = &getch2_keys.map[i];

        if (buflen >= st->len && memcmp(buf, st->chars, st->len) == 0)
            return st;
    }
    return NULL;
}

/* pushes only if there is no duplicate.
   important as we only consider keys if the matches are unique. */
static keycode_st* keys_push_once(char *p, int code) {
    keycode_st *st = keys_search(p, -1);
    if (!st)
        return keys_push(p, code);
    return st;
}

typedef struct {
    char *buf;
    char *pos;
    int cap;
} buf_st;
static buf_st termcap_buf;

static void ensure_cap(buf_st *buf, int cap) {
    if (buf->pos - buf->buf < cap) {
        ptrdiff_t diff = buf->pos - buf->buf;
        buf->cap += cap;
        buf->buf = realloc(buf->buf, buf->cap);
        buf->pos = buf->buf + diff;
    }
}

static char *termcap_get(char *id) {
    ensure_cap(&termcap_buf, 1024);
    return tgetstr(id, &termcap_buf.pos);
}

typedef struct {
    char *id;
    int code;
} cap_key_pair;

#if 0
#include <stdio.h>
#include <ctype.h>

static void debug_keycode(keycode_st *st) {
    if (!st)
        return;

    char buf[128]; /* worst case should be 70 bytes */
    unsigned char *b = &buf[0];
    unsigned char *p = &st->chars[0];

    if (st->cap)
        b += sprintf(b, "%s: ", st->cap);

    for(; *p; p++) {
        if (*p == 27)
            b += sprintf(b, "\\e");
        else if (*p < 27)
            b += sprintf(b, "^%c", '@' + *p);
        else if (!isgraph(*p))
            b += sprintf(b, "\\x%02x", (unsigned int)*p);
        else
            b += sprintf(b, "%c", *p);
    }
    fprintf(stderr, "%s\n", buf);
}
#endif

static void termcap_add(cap_key_pair pair) {
    char *p = termcap_get(pair.id);
    if (p) {
        keycode_st *st = keys_push_once(p, pair.code);
        if (st)
            st->cap = pair.id;
        /* debug_keycode(st); */
    }
}

static void termcap_add_extra_f_keys(void) {
    char capbuf[3];
    for (int i = 11; i < 0x20; i++) {
        unsigned char c;
        if (i < 20) { /* 1-9 */
            c = '0' + (i - 10);
        } else {      /* A-Z */
            c = 'A' + (i - 20);
        }

        sprintf(&capbuf[0], "F%c", c);

        char *p = termcap_get(capbuf);
        if (p)
            keys_push_once(p, MP_KEY_F+i);
        else
            break; /* unlikely that the database has further keys */
    }
}

static void load_termcap(void)
{
    char *termtype = NULL;

#if HAVE_TERMINFO
    use_env(TRUE);
    int ret;
    if (setupterm(termtype, 1, &ret) != OK) {
        /* try again, with with "ansi" terminal if it was unset before */
        if (!termtype)
            termtype = getenv("TERM");
        if (!termtype || *termtype == '\0')
            termtype = "ansi";

        if (setupterm(termtype, 1, &ret) != OK) {
            if (ret < 0) {
                printf("Could not access the 'terminfo' data base.\n");
                return;
            } else {
                printf("Couldn't use terminal `%s' for input.\n", termtype);
                return;
            }
        }
    }
#else
    static char term_buffer[2048];
    if (!termtype) termtype = getenv("TERM");
    if (!termtype) termtype = "ansi";
    int success = tgetent(term_buffer, termtype);
    if (success < 0) {
        printf("Could not access the 'termcap' data base.\n");
        return;
    } else if (success == 0) {
        printf("Terminal type `%s' is not defined.\n", termtype);
        return;
    }
#endif
    ensure_cap(&termcap_buf, 2048);

    static char term_buf[128];
    char *buf_ptr = &term_buf[0];

    // References for terminfo/termcap codes:
    //  http://linux.die.net/man/5/termcap
    //  http://unixhelp.ed.ac.uk/CGI/man-cgi?terminfo+5

    term_smkx = tgetstr("ks", &buf_ptr);
    term_rmkx = tgetstr("ke", &buf_ptr);

    cap_key_pair keys[] = {
        {"kP", MP_KEY_PGUP}, {"kN", MP_KEY_PGDWN}, {"kh", MP_KEY_HOME}, {"kH", MP_KEY_END},
        {"kI", MP_KEY_INS},  {"kD", MP_KEY_DEL},  /* on PC keyboards */ {"@7", MP_KEY_END},

        {"kl", MP_KEY_LEFT}, {"kd", MP_KEY_DOWN}, {"ku", MP_KEY_UP}, {"kr", MP_KEY_RIGHT},

        {"do", MP_KEY_ENTER},
        {"kb", MP_KEY_BS},

        {"k1", MP_KEY_F+1},  {"k2", MP_KEY_F+2},  {"k3", MP_KEY_F+3},
        {"k4", MP_KEY_F+4},  {"k5", MP_KEY_F+5},  {"k6", MP_KEY_F+6},
        {"k7", MP_KEY_F+7},  {"k8", MP_KEY_F+8},  {"k9", MP_KEY_F+9},
        {"k;", MP_KEY_F+10}, {"k0", MP_KEY_F+0},

        /* K2 is the keypad center */
        {"K2", MP_KEY_KP5},

        /* EOL */
        {NULL},
    };
    for (int i = 0; keys[i].id; i++) {
        termcap_add(keys[i]);
    }
    termcap_add_extra_f_keys();

    /* special cases (hardcoded, no need for HAVE_TERMCAP) */

    /* it's important to use keys_push_once as we can't have duplicates */

    /* many terminals, for emacs compatibility, use 0x7f instead of ^H
       when typing backspace, even when the 'kb' cap says otherwise. */
    keys_push_once("\177", MP_KEY_BS);

    /* mintty always sends these when using the numpad arrows,
       even in application mode, for telling them from regular arrows. */
    keys_push_once("\033[A", MP_KEY_UP);
    keys_push_once("\033[B", MP_KEY_DOWN);
    keys_push_once("\033[C", MP_KEY_RIGHT);
    keys_push_once("\033[D", MP_KEY_LEFT);

    /* mintty uses this instead of the "K2" cap for keypad center */
    keys_push_once("\033OE", MP_KEY_KP5);

    /* fallback if terminfo and termcap are not available */
    keys_push_once("\012", MP_KEY_ENTER);
}

static void enable_kx(bool enable)
{
    char *cmd = enable ? term_smkx : term_rmkx;
    if (cmd)
        tputs(cmd, 1, putchar);
}

#define BUF_LEN 256

static unsigned char getch2_buf[BUF_LEN];
static int getch2_len = 0;
static int getch2_pos = 0;
static enum {
    STATE_INITIAL,
    STATE_UTF8,
} state = STATE_INITIAL;
static int utf8_len = 0;

static void walk_buf(unsigned int count) {
    if (!(count < BUF_LEN && count <= getch2_len))
        abort();

    memmove(&getch2_buf[0], &getch2_buf[count], getch2_len - count);
    getch2_len -= count;
    getch2_pos -= count;
    if (getch2_pos < 0)
        getch2_pos = 0;
}

static bool getch2(struct input_ctx *input_ctx)
{
    int retval = read(0, &getch2_buf[getch2_pos], BUF_LEN - getch2_len - getch2_pos);
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
    getch2_len += retval;

    while (getch2_pos < getch2_len) {
        unsigned char c = getch2_buf[getch2_pos++];

        switch (state) {
            case STATE_INITIAL: {
                int match_count = keys_count_matches(&getch2_buf[0], getch2_len);
                if (match_count == 1) {
                    keycode_st *st = keys_search(&getch2_buf[0], getch2_len);

                    if (st) {
                        mp_input_put_key(input_ctx, st->code);
                        walk_buf(st->len);
                    } /* else this is still a partial (but unique) match */

                    continue;
                } else if (match_count > 1) {
                    continue; /* need more bytes to disambiguate */
                } else {
                    /* backtrack, send as UTF-8 */
                    getch2_pos = 1;
                    c = getch2_buf[0];
                }
                utf8_len = bstr_parse_utf8_code_length(c);

                if (utf8_len > 1) {
                    state = STATE_UTF8;
                } else if (utf8_len == 1) {
                    switch (c) {
                    case 0x1b: /* ESC that's not part of escape sequence */
                        /* only if ESC was typed twice, otherwise ignore it */
                        if (getch2_len > 1 && getch2_buf[1] == 0x1b) {
                            walk_buf(1); /* eat the second ESC */
                            mp_input_put_key(input_ctx, MP_KEY_ESC);
                        }
                        break;
                    default:
                        mp_input_put_key(input_ctx, c);
                    }
                    walk_buf(1);
                } else
                    walk_buf(getch2_pos);

                break;
            }
            case STATE_UTF8: {
                if (getch2_pos < utf8_len) /* need more bytes */
                    continue;

                struct bstr s = {getch2_buf, utf8_len};
                int unicode = bstr_decode_utf8(s, NULL);

                if (unicode > 0) {
                    mp_input_put_key(input_ctx, unicode);
                }
                walk_buf(utf8_len);
                state = STATE_INITIAL;
                continue;
            }
        }
    }

    return true;
}

#endif /* terminfo/termcap */

static volatile int getch2_active  = 0;
static volatile int getch2_enabled = 0;

static void do_activate_getch2(void)
{
    if (getch2_active || !isatty(STDOUT_FILENO))
        return;

    enable_kx(true);

#if HAVE_TERMIOS
    struct termios tio_new;
    tcgetattr(0,&tio_new);

    if (!tio_orig_set) {
        tio_orig = tio_new;
        tio_orig_set = 1;
    }

    tio_new.c_lflag &= ~(ICANON|ECHO); /* Clear ICANON and ECHO. */
    tio_new.c_cc[VMIN] = 1;
    tio_new.c_cc[VTIME] = 0;
    tcsetattr(0,TCSANOW,&tio_new);
#endif

    getch2_active = 1;
}

static void do_deactivate_getch2(void)
{
    if (!getch2_active)
        return;

    enable_kx(false);

#if HAVE_TERMIOS
    if (tio_orig_set) {
        // once set, it will never be set again
        // so we can cast away volatile here
        tcsetattr(0, TCSANOW, (const struct termios *) &tio_orig);
    }
#endif

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
    int newstatus = (tcgetpgrp(0) == getpgrp());

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
static int death_pipe[2];

static void quit_request_sighandler(int signum)
{
    do_deactivate_getch2();

    write(death_pipe[1], &(char){0}, 1);
}

static void *terminal_thread(void *ptr)
{
    bool stdin_ok = isatty(STDIN_FILENO); // if false, we still wait for SIGTERM
    while (1) {
        struct pollfd fds[2] = {
            {.events = POLLIN, .fd = death_pipe[0]},
            {.events = POLLIN, .fd = STDIN_FILENO},
        };
        // Wait with some timeout, so we can call getch2_poll() frequently.
        poll(fds, stdin_ok ? 2 : 1, 1000);
        if (fds[0].revents)
            break;
        if (fds[1].revents)
            stdin_ok = getch2(input_ctx);
        getch2_poll();
    }
    // Important if we received SIGTERM, rather than regular quit.
    struct mp_cmd *cmd = mp_input_parse_cmd(input_ctx, bstr0("quit"), "");
    if (cmd)
        mp_input_queue_cmd(input_ctx, cmd);
    return NULL;
}

void terminal_setup_getch(struct input_ctx *ictx)
{
    if (!getch2_enabled)
        return;

    assert(!input_ctx); // already setup

    if (mp_make_wakeup_pipe(death_pipe) < 0)
        return;

    input_ctx = ictx;

    if (pthread_create(&input_thread, NULL, terminal_thread, NULL)) {
        input_ctx = NULL;
        close(death_pipe[0]);
        close(death_pipe[1]);
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

    do_deactivate_getch2();

    if (input_ctx) {
        write(death_pipe[1], &(char){0}, 1);
        pthread_join(input_thread, NULL);
        close(death_pipe[0]);
        close(death_pipe[1]);
        input_ctx = NULL;
    }

    getch2_enabled = 0;
}

bool terminal_in_background(void)
{
    return isatty(STDERR_FILENO) && tcgetpgrp(STDERR_FILENO) != getpgrp();
}

void terminal_get_size(int *w, int *h)
{
    struct winsize ws;
    if (ioctl(0, TIOCGWINSZ, &ws) < 0 || !ws.ws_row || !ws.ws_col)
        return;

    *w = ws.ws_col;
    *h = ws.ws_row;
}

int terminal_init(void)
{
    if (isatty(STDOUT_FILENO))
        load_termcap();

    assert(!getch2_enabled);

    // handlers to fix terminal settings
    setsigaction(SIGCONT, continue_sighandler, 0, true);
    setsigaction(SIGTSTP, stop_sighandler, SA_RESETHAND, false);
    setsigaction(SIGTTIN, SIG_IGN, 0, true);
    setsigaction(SIGTTOU, SIG_IGN, 0, true);

    do_activate_getch2();

    getch2_enabled = 1;
    return 0;
}
